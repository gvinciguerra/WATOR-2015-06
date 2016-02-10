/** \file queue.h
    \author Giorgio Vinciguerra
    \date Maggio 2015
    \copyright Copyright (c) 2015 Giorgio Vinciguerra. All rights reserved.
    \note  Si dichiara che il contenuto di questo file è in ogni sua parte opera
           originale dell' autore.
    \brief File contenente i prototipi di funzioni per la creazione e la
           gestione di una semplice coda concorrente.
*/

#ifndef __QUEUE__H
#define __QUEUE__H

#include <pthread.h>
#include <stdbool.h>

/** Tipo di un generico elemento della coda. */
typedef struct queue_element {
    void *info;
    struct queue_element *next;
} queue_element_t;

/** Tipo di una coda */
typedef struct queue {
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    queue_element_t *front;
    queue_element_t *rear;
} queue_t;


/** Aggiunge un elemento in fondo alla coda. Se la coda è stata distrutta non
    fa nulla.
    \param q la coda
    \param info puntatore non nullo all'elemento da aggiungere
 */
void enqueue(volatile queue_t *q, void *info);

/** Estrae l'elemento in testa alla coda. Se la coda è vuota si mette in attesa.
    Se la coda è stata distrutta restituisce un puntatore nullo.
    \param q la coda
    \return il puntatore all'elemento estratto
    \return NULL se la coda è stata distrutta
 */
void *dequeue(volatile queue_t *q);

/** Crea una nuova coda vuota.
    \return Il puntatore alla nuova coda, oppure NULL se c'è stato un problema
            nell'allocazione.
 */
queue_t *create_queue();

/** Distrugge la coda, eliminando tutti gli item. Se qualche thread è in attesa
    su dequeue, questa restituirà NULL. Le chiamate successive a enqueue sulla
    stessa coda non avranno alcun effetto.
    \param q la coda
 */
void destroy_queue(volatile queue_t *q);

#endif
