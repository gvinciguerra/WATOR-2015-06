/** \file visualizer.h
    \author Giorgio Vinciguerra
    \date Maggio 2015
    \copyright Copyright (c) 2015 Giorgio Vinciguerra. All rights reserved.
    \note  Si dichiara che il contenuto di questo file è in ogni sua parte opera
           originale dell' autore.
    \brief File contenente le macro e altre definizioni per la comunicazione via
           socket tra visualizer e wator.
*/

#ifndef __VISUALIZER__H
#define __VISUALIZER__H

/** Macro che indica la dimensione di un messaggio che contiene il numero di
    righe (o di colonne) della matrice. Questo tipo di messaggio è scambiato
    2 volte (una per ogni dimensione della matrice) appena i due processi si
    connettono. I successivi messaggi hanno dimensione MESSAGE_TYPE2_LENGTH.
 */
#define MESSAGE_TYPE1_LENGTH sizeof(unsigned int)

/** Macro che indica la dimensione di un messaggio che contiene le celle di una
    matrice.
 */
#define MESSAGE_TYPE2_LENGTH (size_t)512

/** Stringa che rappresenta il path del socket che connette i due processi. */
#define SOCKET_PATH "/tmp/visual.sck"

/** Macro che indica il numero massimo di tentativi di connessione al socket
    che il visualizer esegue prima di terminare.
 */
#define MAX_CONNECTION_ATTEMPTS 10

/** Macro che indica la distanza, in secondi, tra un tentativo di connessione
    fallito e un altro.
 */
#define DELAY_BETWEEN_CONNECTIONS 1

#endif
