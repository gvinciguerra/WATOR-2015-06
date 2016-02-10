/**
    \file wator.c
    \author Giorgio Vinciguerra
    \date Maggio 2015
    \copyright Copyright (c) 2015 Giorgio Vinciguerra. All rights reserved.
    \note Si dichiara che il contenuto di questo file è in ogni sua parte opera originale dell' autore.
    \brief File contenente l'implementazione di funzioni di utilità per il progetto.
*/

#include "utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void print_fatal_error(const char *format, ... )
{
    va_list args;
    va_start (args, format);
    vfprintf(stderr, format, args);
    va_end (args);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}
