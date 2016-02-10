/** \file visualizer.c
    \author Giorgio Vinciguerra
    \date Maggio 2015
    \copyright Copyright (c) 2015 Giorgio Vinciguerra. All rights reserved.
    \note  Si dichiara che il contenuto di questo file è in ogni sua parte
           opera originale dell' autore.
    \brief File che contiene l'implementazione delle funzioni del visualizer.

    Questo file verrà compilato in un eseguibile. Se lanciato con un parametro,
    esso indichera il file su cui salvare la matrice del pianeta, altrimenti
    verrà stampata su stdout.
    Il processo wator utilizza il segnale SIGUSR2 per informare il visualizer
    dell'avvio della procedura di terminazione.
    Le funzioni qui implementate non sono presenti nell'header file in quanto
    esse NON fanno parte dell'interfaccia pubblica del visualizer.
 */

#include "utils.h"
#include "wator.h"
#include "visualizer.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <sys/un.h>
#include <sys/socket.h>

static int visualizerSocket;         // Il socket wator <-> visualizer
static cell_t **planetMatrix;        // La matrice che va riempita con i messaggi di wator
static char *dumpFile;               // Indica il file su cui effettuare il dump
static volatile bool mustTerminate;  // Flag che diventa true all'arrivo di SIGUSR2

/** Alloca una nuova matrice del pianeta di nrow*ncol celle. Se non ha successo
    ritorna false, altrimenti modifica la variabile globale planetMatrix
    e ritorna true.
 */
bool new_planet_matrix(unsigned int nrow, unsigned int ncol)
{
    static unsigned savedNrow = 0, savedNcol = 0;
    if (savedNrow == nrow && savedNcol == ncol)
        return true; // Matrice di quelle dimensioni già allocata
    else {
        if (planetMatrix) {
            for (int i = 0; i < nrow; i++)
                free(planetMatrix[i]);
            free(planetMatrix);
            planetMatrix = NULL;
        }
    }

    planetMatrix = (cell_t **) malloc(nrow * sizeof(cell_t *));
    if (planetMatrix == NULL)
        return false;

    bool outOfMemory = false; // true se una malloc ha fallito
    unsigned int row = 0;     // È qui perché essere visibile al codice di pulizia
    for (; row < nrow; row++) {
        planetMatrix[row] = (cell_t *) malloc(ncol * sizeof(cell_t));
        if (planetMatrix[row] == NULL) {
            outOfMemory = true;
            break;
        }
    }

    if (outOfMemory) { // Una malloc ha fallito, fa pulizia
        for (unsigned int i = 0; i < row; i++)
            free(planetMatrix[i]);
        free(planetMatrix);
        return false;
    }

    return true;
}

/** Effettua al più MAX_CONNECTION_ATTEMPTS tentativi di connessione al socket
    con il processo wator. Se la connessione è stata stabilita e i messaggi
    iniziali (dimensioni della matrice di simulazione) sono stati scambiati con
    successo, modifica gli argomenti con la dimensione della matrice e
    ritorna true, altrimenti ritorna false.
 */
bool connect_to_socket(unsigned int *nrow, unsigned int *ncol)
{
    struct sockaddr_un sockaddr = {.sun_family = AF_UNIX};
    strncpy(sockaddr.sun_path, SOCKET_PATH, sizeof(sockaddr.sun_path));

    SC_OR_FAIL(socket(AF_UNIX, SOCK_STREAM, 0), visualizerSocket, "Errore nella creazione del socket");
    int attempts = 1;
    while (-1 == connect(visualizerSocket, (struct sockaddr*) &sockaddr, sizeof(sockaddr))) {
        perror("Errore");
        if (attempts <= MAX_CONNECTION_ATTEMPTS) {
            DEBUG_PRINTF("Non riesco a connettermi al server. Tentativo %d di %d\n", attempts, MAX_CONNECTION_ATTEMPTS);
            sleep(DELAY_BETWEEN_CONNECTIONS);
        }
        else
            return false;
        attempts++;
    }
    if (-1 == recv(visualizerSocket, nrow, MESSAGE_TYPE1_LENGTH, 0) ||
        -1 == recv(visualizerSocket, ncol, MESSAGE_TYPE1_LENGTH, 0) ||
        *nrow < 1 || *ncol < 1)
        return false;

    return true;
}

/** Macro che calcola la prossima posizione di un carattere all'interno della
    matrice del pianeta. */
#define NEXT(r, c) { if(c==ncol-1) {c=0;r++;} else c++; }

/** Legge i dati dal socket. Ogni byte ricevuto rappresenta una cella con cui
    verrà riempita planetMatrix.

    \return true se la matrice è stata riempita, altrimenti false.
 */
bool read_from_socket(unsigned int nrow, unsigned int ncol)
{
    ssize_t totalByteRead;

    char buffer[MESSAGE_TYPE2_LENGTH];
    int currRow = 0, currCol = 0;
    while ((totalByteRead = recv(visualizerSocket, buffer, MESSAGE_TYPE2_LENGTH, 0)) != -1 || errno == EINTR) {
        if (totalByteRead == 0)
            continue;

        for (size_t i = 0; i < totalByteRead; i++) {
            planetMatrix[currRow][currCol] = char_to_cell(buffer[i]);
            if (currRow == nrow - 1 && currCol == ncol - 1)
                return 1; // Matrice riempita
            else
                NEXT(currRow, currCol);
        }
    }
    return 0; // Matrice non riempita
}

/** Funzione che gestisce l'arrivo del segnale di terminazione SIGUSR2. */
void terminate(int sig)
{
    mustTerminate = true;
}

/** Esegue la stampa della matrice del pianeta su schermo o su file. */
void print_or_dump_planet_matrix(unsigned int nrow, unsigned int ncol)
{
    FILE *destStream = stdout;
    if (dumpFile != NULL && NULL == (destStream = fopen(dumpFile, "w"))) {
        perror("Non è stato possibile salvare lo stato della matrice");
        return;
    }

    planet_t tmpPlan = {.w = planetMatrix, .nrow = nrow, .ncol = ncol};
    if (destStream == stdout) {
        if (system("clear") != -1)
            print_planet_colored(&tmpPlan);
        fflush(stdout);
    }
    else {
        int retval = print_planet(destStream, &tmpPlan);
        fclose(destStream);

        if (retval == -1)
            remove(dumpFile);
    }
}

int main(int argc, char *argv[])
{
    if (argc > 1) // devo fare il dump in un file anziché a schermo
        dumpFile = argv[1];

    // Maschera i segnali
    int retval;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    SC_OR_FAIL(pthread_sigmask(SIG_SETMASK, &set, NULL), retval, "Impossibile gestire i segnali in visualizer");

    // Installa un handler per il segnale di terminazione
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = terminate;
    SC_OR_FAIL(sigaction(SIGUSR2, &sa, NULL), retval, "Impossibile gestire i segnali in visualizer");

    unsigned int nrow = UINT_MAX; // Numero di colonne della matrice
    unsigned int ncol = UINT_MAX; // Numero di righe della matrice
    while (!mustTerminate) {
        if (connect_to_socket(&nrow, &ncol) && new_planet_matrix(nrow, ncol) && read_from_socket(nrow, ncol))
            print_or_dump_planet_matrix(nrow, ncol);
        close(visualizerSocket);
    }

    DEBUG_PRINT("Visualizer sta per terminare...\n");
    return EXIT_SUCCESS;
}
