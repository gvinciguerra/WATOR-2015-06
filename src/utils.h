/** \file utils.h
    \author Giorgio Vinciguerra
    \date Maggio 2015
    \copyright Copyright (c) 2015 Giorgio Vinciguerra. All rights reserved.
    \note Si dichiara che il contenuto di questo file è in ogni sua parte opera originale dell' autore.
    \brief File contenente macro e prototipi di funzioni di utilità per il progetto.
*/

#ifndef __UTILS__H
#define __UTILS__H

#include <assert.h>

// INIZIO definizioni attive solo in fase di debug (compilazione con gcc -D DEBUG)
#ifdef DEBUG
  /** Macro per la stampa (colorata, se il terminale la supporta) di
      informazioni di debug su stderr
    */
  #define DEBUG_PRINTF(format, ...) \
    { \
      fprintf(stderr, "\x1b[37m[%s:%s()]\x1b[0m "format, __FILE__, __func__, __VA_ARGS__); \
      fflush(stderr); \
    }
  #define DEBUG_PRINT(str) \
    { \
      fprintf(stderr, "\x1b[37m[%s:%s()]\x1b[0m "str, __FILE__, __func__); \
      fflush(stderr); \
    }
  /** Macro per le asserzioni attive soltanto durante la fase di debug */
  #define DEBUG_ASSERT(expr) assert(expr)
#else
  #define DEBUG_PRINTF(format, ...)
  #define DEBUG_PRINT(str)
  #define DEBUG_ASSERT(expr)
#endif
// FINE definizioni attive solo in fase di debug


/** Macro per la conversione di una stringa in unsigned long. Se non ha successo
    termina il programma
 */
#define STRTOUL_OR_FAIL(str, destVar) \
  { \
    destVar = strtoul(str, NULL, 10); \
    if (errno) print_fatal_error("%s non è un numero intero valido.", str); \
  }

/** Macro per l'esecuzione di una chiamata di sistema o di libreria. Se il
    risultato (che verrà salvato in var) è -1, stampa str e termina il programma
 */
#define SC_OR_FAIL(syscall, var, str) \
  if ((var = syscall) == -1) { \
      perror(str); \
      exit(EXIT_FAILURE); \
  }

/** Macro che valuta una funzione call e salva il risultato in var. Se var è
    null termina il programma stampando str. Tipicamente viene usata per
    funzioni che allocano memoria e restituiscono puntatori.
*/
#define NOT_NULL_OR_FAIL(call, var, str) \
  if ((var = call) == NULL) { \
    print_fatal_error(str); \
  }


/** Macro per l'esecuzione di una chiamata di sistema o di libreria. Se il
    risultato (che verrà salvato in var) è -1, stampa str ma non termina il
    programma
 */
#define SC_OR_PERROR(syscall, var, str) \
  if ((var = syscall) == -1) { \
    perror(str); \
  }

/** Stampa una stringa su stderr e termina il programma con status EXIT_FAILURE
    \param La stringa formato da stampare
  */
void print_fatal_error(const char * format, ... );


#endif
