/** \file main.c
    \author Giorgio Vinciguerra
    \date Maggio 2015
    \copyright Copyright (c) 2015 Giorgio Vinciguerra. All rights reserved.
    \note  Si dichiara che il contenuto di questo file è in ogni sua parte opera
           originale dell' autore.
    \brief File che attiva un server, un thread dispatcher, un thread collector
           e n thread worker (definiti in farm.c) e lancia il processo
           visualizer.

    Questo file verrà compilato nell'eseguibile wator. Il processo wator si
    occupa della creazione della struttura a farm per la simulazione, e della
    creazione di un server per le comunicazioni con un visualizer.
 */

#include "farm.h"
#include "wator.h"
#include "utils.h"
#include "visualizer.h"
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <libgen.h>
#include <pthread.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

/** Numero di secondi tra un checkpoint automatico e l'altro */
#define SEC 150

/** Numero totale di worker di default quando l'opzione -n non è specificata */
#define NWORK_DEF 8

/** Intervallo di default, misurato in chronon, tra la visualizzazione/dump del
    pianeta quando l'opzione -v non è specificata */
#define CHRON_DEF 4

/** Intervallo di default, misurato in millisecondi, tra il calcolo un chronon e
    l'altro quando l'opzione -d non è specificata */
#define CHRON_DELAY 0

/** Numero massimo di connessioni contemporanee ammesse sul socket */
#define SOCKET_MAXCONN 1

// Dichiarazione delle variabili extern di farm.h
volatile wator_t *wator;
volatile bool mustTerminateFlag = false;
volatile int visualizerSocket = -1;
volatile int visualizerConnectionFd = -1;
volatile long chrInterval = CHRON_DEF;
volatile useconds_t chronDelay = CHRON_DELAY;
volatile queue_t *tasksQueue;
int totalWorkers = NWORK_DEF; // Può non essere volatile visto che non cambia durante la simulazione

/** Realizza la funzionalità di checkpointing: salva lo stato corrente della
    simulazione in un file wator.check nella stessa cartella dell'eseguibile e
    avvia un allarme impostato a SEC secondi per il prossimo checkpoint.
    Non termina la simulazione se si verificano errori di I/O.
 */
void checkpoint()
{
    FILE *f = fopen("wator.check", "w");
    if (f == NULL)
        perror("Impossibile salvare lo stato del pianeta in wator.check.");
    else {
        print_planet(f, wator->plan);
        fclose(f);
    }
    alarm(SEC);
}

int main(int argc, char *argv[])
{
    // Prova a rimuovere i file wator_worker_wid delle precedenti simulazioni
    int retval = system("rm -f wator_worker_*");

    /* =========================================================================
        CONTROLLO DEI PARAMETRI e delle condizioni per l'avvio del programma
     */
    char c, *planetFile, *dumpFile = NULL;

    if (argc < 2)
        print_fatal_error("Nessun file di input.");
    planetFile = argv[1];
    if (-1 == access(CONFIGURATION_FILE, R_OK))
        print_fatal_error("File di configurazione '%s' non trovato o permessi insufficienti.", CONFIGURATION_FILE);
    if (-1 == access(planetFile, R_OK))
        print_fatal_error("File del pianeta '%s' non trovato o permessi insufficienti.", planetFile);

    optind = 2;
    while ((c = getopt(argc, argv, ":n:v:f:d:")) != -1)
        switch (c) {
            case 'f': dumpFile = optarg; break;
            case 'n': STRTOUL_OR_FAIL(optarg, totalWorkers); break;
            case 'v': STRTOUL_OR_FAIL(optarg, chrInterval); break;
            case 'd': STRTOUL_OR_FAIL(optarg, chronDelay); chronDelay *= 1000.0; break;
            case ':': print_fatal_error("L'opzione -%c richiede un argomento.", optopt);
            case '?': print_fatal_error("Opzione -%c non riconosciuta.", optopt);
            default:  print_fatal_error("Mi aspettavo un'opzione ma ho ricevuto %c.", c);
        }
    if (optind < argc)
        print_fatal_error("Sono stati forniti troppi argomenti.");

    /* =========================================================================
                    CARICAMENTO SIMULAZIONE WATOR
     */

    wator = new_wator(planetFile);
    if (!wator)
        print_fatal_error("Impossibile caricare la simulazione.");
    if (wator->plan->nrow < 5 || wator->plan->ncol < 5)
        print_fatal_error("Il pianeta non ha un numero sufficiente di righe o colonne");

    /* =========================================================================
                CREAZIONE DEL SOCKET e AVVIO DEL VISUALIZER
     */

    int visualizerPid;
    char tmpPath[sizeof(SOCKET_PATH)]; // Necessario perché in alcune implementazioni dirname() modifica il parametro passato
    struct sockaddr_un sockaddr = {.sun_family = AF_UNIX};
    strncpy(tmpPath, SOCKET_PATH, sizeof(SOCKET_PATH));
    strncpy(sockaddr.sun_path, SOCKET_PATH, sizeof(sockaddr.sun_path));
    unlink(SOCKET_PATH);
    mkdir(dirname(tmpPath), 0666);

    SC_OR_FAIL(socket(AF_UNIX, SOCK_STREAM, 0), visualizerSocket, "Errore nella creazione del socket");
    SC_OR_FAIL(bind(visualizerSocket, (struct sockaddr *) &sockaddr, sizeof(sockaddr)), retval, "Errore nell'assegnamento di un nome al socket");
    SC_OR_FAIL(fork(), visualizerPid, "Errore nella creazione del visualizer");

    // Non riceve segnali SIGPIPE se un client/server chiude la connessione in anticipo
    if (SIG_ERR == signal(SIGPIPE, SIG_IGN))
        print_fatal_error("Impossibile gestire i segnali");

    if (visualizerPid == 0)
        SC_OR_FAIL(execlp("./visualizer", "visualizer", dumpFile ? dumpFile : NULL, NULL), retval, "L'eseguibile visualizer non può essere lanciato");
    SC_OR_FAIL(listen(visualizerSocket, SOCKET_MAXCONN), retval, "Errore in listen");

    /* =========================================================================
                    CREAZIONE DELLA STRUTTURA FARM
     */

    tasksQueue = create_queue();

    // Imposta una mask che i nuovi thread erediteranno (la mask del thread corrente verrà ripristinata)
    sigset_t mainThreadMask, otherThreadMask;
    sigemptyset(&otherThreadMask);
    sigaddset(&otherThreadMask, SIGINT);
    sigaddset(&otherThreadMask, SIGTERM);
    sigaddset(&otherThreadMask, SIGUSR1);
    sigaddset(&otherThreadMask, SIGALRM);
    SC_OR_FAIL(pthread_sigmask(SIG_SETMASK, &otherThreadMask, &mainThreadMask), retval, "Impossibile mascherare i segnali");

    // Tutti i controlli hanno avuto successo, generazione dei worker...
    pthread_t dispatcher, collector;
    pthread_t *workersArray = (pthread_t *) malloc(totalWorkers * sizeof(pthread_t));
    for (int i = 0; i < totalWorkers; i++) {
        int *args = malloc(sizeof(int)); *args = i;
        SC_OR_FAIL(pthread_create(&workersArray[i], NULL, worker_loop, args), retval, "Impossibile creare un thread worker");
    }
    // ... del dispatcher e del collector
    SC_OR_FAIL(pthread_create(&dispatcher, NULL, dispatcher_loop, NULL), retval, "Impossibile creare il thread dispatcher");
    SC_OR_FAIL(pthread_create(&collector, NULL, collector_loop, NULL), retval, "Impossibile creare il thread collector");

    /* =========================================================================
                    GESTIONE DEI SEGNALI del main thread
     */

    checkpoint(); // Effettua il 1° checkpoint e avvia l'allarme per il prossimo
    int sig;
    while (!mustTerminateFlag) {
        sigwait(&otherThreadMask, &sig);
        DEBUG_PRINTF("Segnale ricevuto %d\n", sig);
        switch (sig) {
            case SIGINT:
            case SIGTERM:
                mustTerminateFlag = true;
                break;

            case SIGALRM:
            case SIGUSR1:
                checkpoint();
                break;

            default:
                break;
        }
    }


    /* =========================================================================
                    FASE DI TERMINAZIONE
     */

    DEBUG_PRINT("Avvio terminazione gentile...\n");
    SC_OR_FAIL(pthread_join(collector, NULL), retval, "Errore nell'attesa della terminazione del collector");
    SC_OR_FAIL(pthread_join(dispatcher, NULL), retval, "Errore nell'attesa della terminazione del dispatcher");

    for (int i = 0; i < totalWorkers; i++)
        SC_OR_FAIL(pthread_join(workersArray[i], NULL), retval, "Errore nell'attesa della terminazione di un worker");
    free(workersArray);

    close(visualizerSocket);
    kill(visualizerPid, SIGUSR2);
    waitpid(visualizerPid, &retval, 0);
    unlink(SOCKET_PATH);
    DEBUG_PRINT("Simulazione terminata con successo\n");
    return EXIT_SUCCESS;
}
