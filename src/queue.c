/** \file queue.c
    \author Giorgio Vinciguerra
    \date Maggio 2015
    \copyright Copyright (c) 2015 Giorgio Vinciguerra. All rights reserved.
    \note  Si dichiara che il contenuto di questo file è in ogni sua parte opera
           originale dell' autore.
    \brief File contenente l'implementazione di funzioni per la creazione e la
           gestione di una semplice coda concorrente.
*/

#include "queue.h"
#include <limits.h>
#include <stdlib.h>

void enqueue(volatile queue_t *q, void *info)
{
    if (q == NULL || info == NULL || q->size == INT_MIN)
        return;
    pthread_mutex_lock((pthread_mutex_t*) &q->mutex);

    queue_element_t *newRear = malloc(sizeof(queue_element_t));
    if (newRear == NULL) {
        pthread_mutex_unlock((pthread_mutex_t*) &q->mutex);
        return;
    }
    newRear->info = info;
    newRear->next = NULL;
    if (q->rear != NULL)
        q->rear->next = newRear;
    q->rear = newRear;
    q->size++;
    if (q->front == NULL)
        q->front = newRear;

    pthread_cond_signal((pthread_cond_t*) &q->cond);
    pthread_mutex_unlock((pthread_mutex_t*) &q->mutex);
}

void *dequeue(volatile queue_t *q)
{
    if (q == NULL || q->size == INT_MIN)
        return NULL;
    pthread_mutex_lock((pthread_mutex_t*) &q->mutex);

    while (q->front == NULL && q->size != INT_MIN) // Se la coda non è vuota e non distrutta
        pthread_cond_wait((pthread_cond_t*) &q->cond, (pthread_mutex_t*) &q->mutex);
    if (q->size == INT_MIN) {
        pthread_mutex_unlock((pthread_mutex_t *) &q->mutex);
        return NULL;
    }

    void *dequeuedInfo = q->front->info;
    queue_element_t *tmp = q->front;
    if (q->front == q->rear) // Coda di un solo elemento
        q->rear = NULL;
    q->front = q->front->next;
    q->size--;

    free(tmp);
    pthread_mutex_unlock((pthread_mutex_t*) &q->mutex);
    return dequeuedInfo;
}

queue_t *create_queue()
{
    queue_t *q = calloc(1, sizeof(queue_t));
    if (q != NULL && (-1 == pthread_mutex_init(&q->mutex, NULL)
                  ||  -1 == pthread_cond_init(&q->cond, NULL))) {
        free(q);
        return NULL;
    }
    return q;
}

void destroy_queue(volatile queue_t *q)
{
    pthread_mutex_lock((pthread_mutex_t *) &q->mutex);
    
    while (q->front != NULL) {
        queue_element_t *tmp = q->front;
        q->front = q->front->next;
        free(tmp);
    }
    q->rear = NULL;
    q->size = INT_MIN;

    pthread_cond_broadcast((pthread_cond_t*) &q->cond);
    pthread_mutex_unlock((pthread_mutex_t*) &q->mutex);
}
