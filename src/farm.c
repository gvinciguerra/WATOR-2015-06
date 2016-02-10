/** \file farm.c
    \author Giorgio Vinciguerra
    \date Giugno 2015
    \copyright Copyright (c) 2015 Giorgio Vinciguerra. All rights reserved.
    \note  Si dichiara che il contenuto di questo file è in ogni sua parte opera
           originale dell' autore.
    \brief File contenente l'implementazione della struttura a farm.
*/

#include "farm.h"
#include "utils.h"
#include "visualizer.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

volatile farm_status_t farmStatus = DISPATCHING_BATCH_1;
static volatile int completedTasks; // n° di task completati
static int tasksInBatch1;           // n° di task nel gruppo 1 (rettang orizzontali)
static int tasksInBatch2;           // n° di task nel gruppo 2 (rettang orizz alti 2 celle)
static int tasksInBatch3;           // n° di task nel gruppo 3 (un rettang vertic largo 2 celle)
static bool volatile **cellsToSkip; // Celle a cui sono già state applicate le regole nel chron corrente

/* Mutex sulla variabile farmStatus della struttura a farm. Permette la
   mutua esclusione tra dispatcher-workers e collector */
static pthread_mutex_t farmStatusMutex = PTHREAD_MUTEX_INITIALIZER;

/* CV usata per avvisare il dispatcher di un cambio di stato */
static pthread_cond_t farmStatusCondDisp = PTHREAD_COND_INITIALIZER;

/* CV usata per avvisare il collector di un cambio di stato */
static pthread_cond_t farmStatusCondColl = PTHREAD_COND_INITIALIZER;

void *dispatcher_loop(void *arg)
{
    // Calcola una volta per tutte la suddivisione della matrice
    int nrow = wator->plan->nrow;
    int ncol = wator->plan->ncol;
    int planetSlices = totalWorkers + 1; // Prova a suddividere in base al n° worker
    int stdRectHeight;                   // L'altezza di base di un rettangolo
    int stdRectWidth = ncol - 2;         // La larghezza di un rettangolo

    do {
        planetSlices--;
        stdRectHeight = (nrow - 2.0 * planetSlices) / planetSlices;
    } while(stdRectHeight < 2);
    DEBUG_PRINTF("slices=%d, height=%d, width=%d\n", planetSlices, stdRectHeight, stdRectWidth);

    tasksInBatch1 = planetSlices;
    tasksInBatch2 = planetSlices;
    tasksInBatch3 = 1;
    int i = 0;
    int numberOfTasks = tasksInBatch1 + tasksInBatch2 + tasksInBatch3;
    bool isLastRectBigger = (nrow - planetSlices) % planetSlices != 0;
    rect_t *planetRectangles[numberOfTasks];

    // Primo batch
    for (; i < (isLastRectBigger ? tasksInBatch1 - 1 : tasksInBatch1); ++i) {
        int fromRow = i * (stdRectHeight + 2);
        rect_t *horizRect = make_rect(fromRow, 0,  stdRectWidth, stdRectHeight);
        planetRectangles[i] = horizRect;
    }
    if (isLastRectBigger) { // c'è un avanzo di righe da assegnare nell'ultimo task
        int fromRow = i * (stdRectHeight + 2);
        int height  = nrow - 2 - fromRow;
        rect_t *largeHorizRect = make_rect(fromRow, 0, stdRectWidth, height);
        planetRectangles[i++] = largeHorizRect;
    }
    // Secondo batch
    for (; i < (isLastRectBigger ? tasksInBatch1 - 1 : tasksInBatch1) + tasksInBatch2; ++i) {
        int fromRow = (i - tasksInBatch1) * (stdRectHeight + 2) + stdRectHeight;
        rect_t *twoSpacesHorizRect = make_rect(fromRow, 0,  ncol, 2);
        planetRectangles[i] = twoSpacesHorizRect;
    }
    if (isLastRectBigger) {
        rect_t *largeHorizRect = planetRectangles[tasksInBatch1-1];
        int fromRow = largeHorizRect->fromRow + largeHorizRect->rows;
        rect_t *twoSpacesHorizRect = make_rect(fromRow, 0, ncol, 2);
        planetRectangles[i++] = twoSpacesHorizRect;
    }
    // Terzo e ultimo batch
    rect_t *twoSpacesVertRect = make_rect(0, ncol - 2,  2, nrow);
    planetRectangles[i] = twoSpacesVertRect;

    for (int i = 0; i < numberOfTasks; i++)
        if (planetRectangles[i] == NULL)
            print_fatal_error("La creazione di un rettangolo è fallita");

    // Alloca la matrice delle celle da saltare
    cellsToSkip = (bool volatile **) malloc(nrow * sizeof(bool *));
    if (cellsToSkip == NULL)
        print_fatal_error("Errore nel setup del dispatcher");
    bool outOfMemory = false;
    unsigned int row = 0;     // È qui perché essere visibile al codice di pulizia
    for (; row < nrow; row++) {
        cellsToSkip[row] = (bool *) malloc(ncol * sizeof(bool));
        if (cellsToSkip[row] == NULL) {
            outOfMemory = true;
            break;
        }
    }
    if (outOfMemory) { // Una malloc ha fallito, fa pulizia
        for (unsigned int i = 0; i < row; i++)
            free((void *) cellsToSkip[i]);
        free(cellsToSkip);
        print_fatal_error("Errore nel setup del dispatcher");
    }

    /* ======================== DISPATCHER-LOOP ============================= */
    while (true) {
        pthread_mutex_lock(&farmStatusMutex);

        for (int r = 0; r < nrow; r++)
            memset((void*) cellsToSkip[r], 0, ncol * sizeof(bool));

        while (farmStatus != DISPATCHING_BATCH_1 && farmStatus != TERMINATING)
            pthread_cond_wait(&farmStatusCondDisp, &farmStatusMutex);
        if (farmStatus == TERMINATING) {
            pthread_mutex_unlock(&farmStatusMutex);
            break;
        }

        /* ================== PRIMO BATCH di task =========================== */
        int i;
        for (i = completedTasks = 0; i < tasksInBatch1; ++i)
            enqueue(tasksQueue, planetRectangles[i]);

        while (farmStatus != DISPATCHING_BATCH_2)
            pthread_cond_wait(&farmStatusCondDisp, &farmStatusMutex);

        DEBUG_ASSERT(completedTasks == tasksInBatch1);

        /* ================== SECONDO BATCH di task ========================= */
        for (; i < tasksInBatch1 + tasksInBatch2; ++i)
            enqueue(tasksQueue, planetRectangles[i]);

        while (farmStatus != DISPATCHING_BATCH_3)
            pthread_cond_wait(&farmStatusCondDisp, &farmStatusMutex);

        DEBUG_ASSERT(completedTasks == tasksInBatch1 + tasksInBatch2);

        /* ================= TERZO BATCH di task =========================== */
        enqueue(tasksQueue, planetRectangles[i]);

        pthread_mutex_unlock(&farmStatusMutex);
    }

    for (int i = 0; i < numberOfTasks; ++i)
        free(planetRectangles[i]);

    return NULL;
}

/** Macro per l'esecuzione di una chiamata di sistema o di libreria. Se il
    risultato (che verrà salvato in var) è -1, stampa str, rilascia la lock e
    salta alla prossima iterazione della simulazione.
 */
#define SC_OR_CONTINUE(syscall, var, str) \
  if ((var = syscall) == -1) { \
      perror(str); \
      farmStatus = DISPATCHING_BATCH_1; \
      pthread_cond_signal(&farmStatusCondDisp); \
      pthread_mutex_unlock(&farmStatusMutex); \
      continue; \
  }

void *collector_loop(void *arg)
{
    planet_t *p = wator->plan;
    unsigned int nrow = p->nrow;
    unsigned int ncol = p->ncol;

    while (true) {
        pthread_mutex_lock(&farmStatusMutex);

        while (farmStatus != COLLECTING && farmStatus != TERMINATING)
            pthread_cond_wait(&farmStatusCondColl, &farmStatusMutex);

        usleep(chronDelay);
        wator->chronon++;
        DEBUG_ASSERT(completedTasks == tasksInBatch1 + tasksInBatch2 + tasksInBatch3);

        // Invio matrice a un processo visualizer
        if (wator->chronon % chrInterval == 0) {
            ssize_t retval;
            close(visualizerConnectionFd);
            SC_OR_CONTINUE(accept(visualizerSocket, NULL, NULL), visualizerConnectionFd, "Errore in accept");
            SC_OR_CONTINUE(send(visualizerConnectionFd, &nrow, MESSAGE_TYPE1_LENGTH, 0), retval, "Errore nella comunicazione con visualizer");
            SC_OR_CONTINUE(send(visualizerConnectionFd, &ncol, MESSAGE_TYPE1_LENGTH, 0), retval, "Errore nella comunicazione con visualizer");

            char buffer[MESSAGE_TYPE2_LENGTH];
            int buffIndex = 0;
            for (int r = 0; r < nrow; r++)
                for (int c = 0; c < ncol; c++) {
                    buffer[buffIndex++] = cell_to_char(p->w[r][c]);
                    if (buffIndex == MESSAGE_TYPE2_LENGTH) { // Buffer full
                        SC_OR_CONTINUE(send(visualizerConnectionFd, buffer, MESSAGE_TYPE2_LENGTH, 0), retval, "Errore nella comunicazione con visualizer");
                        buffIndex = 0;
                    }
                }
            if (buffIndex > 0) { // C'è ancora un ultimo messaggio (non pieno) da inviare
                memset((void*)&buffer[buffIndex+1], 0, MESSAGE_TYPE2_LENGTH - buffIndex - 1);
                SC_OR_CONTINUE(send(visualizerConnectionFd, buffer, MESSAGE_TYPE2_LENGTH, 0), retval, "Errore nella comunicazione con visualizer");
            }
            DEBUG_PRINTF("Invio matrice (chronon=%d) completato\n", wator->chronon);
        }

        if (mustTerminateFlag) {
            destroy_queue(tasksQueue);
            farmStatus = TERMINATING;
            pthread_cond_signal(&farmStatusCondDisp); // Avvisa il dispatcher di non continuare il suo lavoro
            pthread_mutex_unlock(&farmStatusMutex);
            return NULL;
        }

        farmStatus = DISPATCHING_BATCH_1;
        pthread_cond_signal(&farmStatusCondDisp);
        pthread_mutex_unlock(&farmStatusMutex);
    }

    return NULL;
}

/* Funzione che increamenta atomicamente il contatore completedTasks e causa la
   transizione di stato della struttura a farm. È usata dai worker al termine
   della lavorazione di un rettangolo. */
static inline void increment_completedTasks()
{
    pthread_mutex_lock(&farmStatusMutex);
    completedTasks++;
    if (completedTasks == tasksInBatch1) {
        farmStatus = DISPATCHING_BATCH_2;
        pthread_cond_signal(&farmStatusCondDisp);
    }
    if (completedTasks == tasksInBatch1 + tasksInBatch2) {
        farmStatus = DISPATCHING_BATCH_3;
        pthread_cond_signal(&farmStatusCondDisp);
    }
    if (completedTasks == tasksInBatch1 + tasksInBatch2 + tasksInBatch3) {
        farmStatus = COLLECTING;
        pthread_cond_signal(&farmStatusCondColl);
    }
    pthread_mutex_unlock(&farmStatusMutex);
}

void *worker_loop(void *arg)
{
    int workerNumber = *(int *)arg;
    char filename[18];
    sprintf(filename, "wator_worker_%d", workerNumber);
    FILE *fp = fopen(filename, "w");
    if (!fp)
        print_fatal_error("Impossibile creare un file wator_worker_wid");
    fclose(fp);
    free(arg);

    while (true) {
        rect_t *rect = dequeue(tasksQueue); // Si mette in attesa se la coda è vuota
        if (rect == NULL) // La coda è stata distrutta, cioé il programma sta per terminare
            break;

        update_wator_rect((wator_t*) wator, rect, (bool**) cellsToSkip);
        increment_completedTasks();
    }

    return NULL;
}
