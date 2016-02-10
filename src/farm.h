/** \file farm.h
    \author Giorgio Vinciguerra
    \date Giugno 2015
    \copyright Copyright (c) 2015 Giorgio Vinciguerra. All rights reserved.
    \note  Si dichiara che il contenuto di questo file è in ogni sua parte opera
           originale dell' autore.
    \brief File contenente i prototipi delle funzioni che implementano la
           struttura a farm e le dichiarazioni di variabili globali condivise
           tra farm.c e main.c
*/

#ifndef __FARM__H
#define __FARM__H

#include "wator.h"
#include "queue.h"
#include <unistd.h>
#include <stdbool.h>

/** Il ciclo eseguito da uno dei thread worker. */
void *worker_loop(void *arg);

/** Il ciclo eseguito da un thread collector. */
void *collector_loop(void *arg);

/** Il ciclo eseguito da un thread dispatcher. */
void *dispatcher_loop(void *arg);

/** Puntatore alla simulazione corrente */
extern volatile wator_t *wator;

/** Flag che informa i thread dell'esecuzione della "terminazione gentile" */
extern volatile bool mustTerminateFlag;

/** Il socket del server wator */
extern volatile int visualizerSocket;

/** Il delay in microsecondi tra un chronon e l'altro */
extern volatile useconds_t chronDelay;

/** La connessione con un client visualizer. -1 sta ad indicare che nessun visualizer è connesso */
extern volatile int visualizerConnectionFd;

/** Intervallo in chronon tra le comunicazioni col visualizer */
extern volatile long chrInterval;

/** La coda concorrente dei task */
extern volatile queue_t *tasksQueue;

/** Il numero totale di worker attivi nella simulazione */
extern int totalWorkers;

/** I possibili stati che può assumere la struttura a farm della simulazione */
typedef enum {DISPATCHING_BATCH_1, DISPATCHING_BATCH_2, DISPATCHING_BATCH_3, COLLECTING, TERMINATING} farm_status_t;

/** Lo stato corrente della simulazione */
extern volatile farm_status_t farmStatus;


#endif
