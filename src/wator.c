/** \file wator.c
    \author Giorgio Vinciguerra
    \date Aprile 2015
    \copyright Copyright (c) 2015 Giorgio Vinciguerra. All rights reserved.
    \note Si dichiara che il contenuto di questo file è in ogni sua parte opera originale dell' autore.
    \brief File contenente l'implementazione delle funzioni della librearia Wator
*/
/*
 [Frammento della documentazione finale del progetto]

 NOTE SULL'IMPLEMENTAZIONE – SCELTE PROGETTUALI
 In molte delle funzioni qui implementate vengono fatti dei controlli
 sull'allocazione della memoria e sui parametri.
 Questi controlli –al prezzo di un piccolo overhead– aumentano la robustezza
 della libreria: senza di essi si potrebbero verificare accessi in memoria non
 consentiti con la conseguente terminazione del programma.
 Tali situazioni "rompono" il contratto d'uso dell'interfaccia documentata in
 wator.h: la liberia NON può terminare i programmi che la usano; è l'utente
 della libreria che deve decidere se è il caso di terminare il programma o agire
 diversamente se le operazioni qui implementate non hanno successo.
 Per lo stesso motivo in wator.c non ci sono chiamate abort() o exit().
 */

#include "wator.h"
#include "utils.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

inline char cell_to_char(cell_t a)
{
    switch (a) {
        case SHARK: return 'S';
        case FISH:  return 'F';
        case WATER: return 'W';
        default:    return '?';
    }
}

inline int char_to_cell(char c)
{
    switch (c) {
        case 'S': return SHARK;
        case 'F': return FISH;
        case 'W': return WATER;
        default:  return -1;
    }
}

planet_t *new_planet(unsigned int nrows, unsigned int ncols)
{
    if (nrows == 0 || ncols == 0)
        return NULL;

    int **btimeMatrix;
    int **dtimeMatrix;
    if (0 != alloc_counters_matrices(nrows, ncols, &btimeMatrix, &dtimeMatrix))
        return NULL;

    planet_t *thePlanet = (planet_t *) malloc(sizeof(planet_t));
    cell_t **planetMatrix = (cell_t **) malloc(nrows * sizeof(cell_t *));
    if (thePlanet == NULL || planetMatrix == NULL) {
        free(thePlanet);
        free(planetMatrix);
        return NULL;
    }

    bool outOfMemory = false; // true se una malloc ha fallito
    unsigned int row = 0;     // È qui perché essere visibile al codice di pulizia
    for (; row < nrows; row++) {
        planetMatrix[row] = (cell_t *) malloc(ncols * sizeof(cell_t));
        if (planetMatrix[row] == NULL) {
            outOfMemory = true;
            DEBUG_PRINTF("Non è stato possibile allocare la riga %d.\n", row);
            break;
        }
        for (unsigned int col = 0; col < ncols; col++)
            planetMatrix[row][col] = WATER;
    }

    if (outOfMemory) { // Una malloc ha fallito, fa pulizia
        for (unsigned int i = 0; i < row; i++)
            free(planetMatrix[i]);
        free(planetMatrix);
        free(thePlanet);
        return NULL;
    }

    thePlanet->w     = planetMatrix;
    thePlanet->nrow  = nrows;
    thePlanet->ncol  = ncols;
    thePlanet->btime = btimeMatrix;
    thePlanet->dtime = dtimeMatrix;
    return thePlanet;
}

void free_planet(planet_t *p)
{
    if (p != NULL) {
        for (unsigned int row = 0; row < p->nrow; row++) {
            free(p->w[row]);
            free(p->btime[row]);
            free(p->dtime[row]);
        }
        free(p->w);
        free(p->btime);
        free(p->dtime);
        free(p);
    }
}

int print_planet(FILE *f, planet_t *p)
{
    if (p == NULL || f == NULL)
        return -1;

    if (fprintf(f, "%u\n%u\n", p->nrow, p->ncol) == -1)
        return -1;
    for (unsigned int row = 0; row < p->nrow; row++) {
        for (unsigned int col = 0; col < p->ncol; col++)
            if (fprintf(f, "%c ", cell_to_char(p->w[row][col])) == -1)
                return -1;

        // Sostituisce l'ultimo spazio con \n. Come mostrato nella specifica
        if (f != stdout && (fseek(f, -1, SEEK_CUR) == -1 || fputc('\n', f) == -1))
            return -1;
    }

    return ferror(f) != 0 ? -1 : 0;
}

int print_planet_colored(planet_t *p)
{
    if (p == NULL)
        return -1;

    for (unsigned int row = 0; row < p->nrow; row++) {
        for (unsigned int col = 0; col < p->ncol; col++) {
            char cell = cell_to_char(p->w[row][col]);
            switch (cell) {
                case 'W': printf("\x1b[36m%c\x1b[0m ", cell); break; // ciano
                case 'S': printf("\x1b[31m%c\x1b[0m ", cell); break; // rosso
                case 'F': printf("\x1b[33m%c\x1b[0m ", cell); break; // giallo
            }
        }
        printf("\n");
    }

    return 0;
}

planet_t *load_planet(FILE *f)
{
    fseek(f, 0, SEEK_SET);
    int nrows = 0, ncols = 0;
    char buffer[32];
    errno = ERANGE; // Finché la lettura non è completa, si assume che ci sarà un errore nel file

    if (f == NULL || fgets(buffer, sizeof(buffer), f) == NULL)
        return NULL;
    nrows = atoi(buffer);

    if (fgets(buffer, sizeof(buffer), f) == NULL)
        return NULL;
    ncols = atoi(buffer);

    if (ncols < 1 || nrows < 1)
        return NULL;

    // Nessun errore, prova ad allocare il pianeta e la matrice
    planet_t *thePlanet = (planet_t *) malloc(sizeof(planet_t));
    cell_t **planetMatrix = (cell_t **) malloc(nrows * sizeof(cell_t *));
    if (thePlanet == NULL || planetMatrix == NULL) {
        free(thePlanet);
        free(planetMatrix);
        return NULL;
    }

    // Lettura della matrice
    planetMatrix[0] = (cell_t *) malloc(ncols * sizeof(cell_t));
    bool merror = false; // Serve per la pulizia quando fallisce una malloc
    int row = 0, col = 0, cell = 0, c;
    while ((c = fgetc(f)) != EOF) {
        if (c == ' ' || c == '\n')
            continue; // skip degli spazi vuoti
        if ((cell = char_to_cell((char)c)) == -1) {
            merror = true;
            DEBUG_PRINTF("Il carattere %c non è valido.\n", c);
            break;
        }
        planetMatrix[row][col] = cell;

        if (col < ncols - 1)
            col++;
        else if (row < nrows -1) {
            row++;
            col = 0;
            planetMatrix[row] = (cell_t *) malloc(ncols * sizeof(cell_t));
            if (planetMatrix[row] == NULL) {
                merror = true;
                DEBUG_PRINTF("Non è stato possibile allocare la riga %d.\n", row);
                break;
            }
        }
        else // La matrice è piena
            break;
    }

    // Alloca le matrici ausiliarie del pianeta
    int **btimeMatrix = NULL;
    int **dtimeMatrix = NULL;
    bool merror2 = 0 != alloc_counters_matrices(nrows, ncols, &btimeMatrix, &dtimeMatrix);

    // Una malloc è fallita oppure il file è terminato prima che la matrice fosse piena
    if (merror || merror2 || row != nrows - 1 || col != ncols - 1) {
        errno = merror ? ENOMEM : ERANGE;
        free(thePlanet);
        for (int i = 0; i < row; i++)
            free(planetMatrix[i]);
        free(planetMatrix);
        if (!merror2) {
            for (int i = 0; i < row; i++) {
                free(btimeMatrix[i]);
                free(dtimeMatrix[i]);
            }
            free(btimeMatrix);
            free(dtimeMatrix);
        }
        return NULL;
    }

    errno = 0;
    thePlanet->w     = planetMatrix;
    thePlanet->nrow  = nrows;
    thePlanet->ncol  = ncols;
    thePlanet->btime = btimeMatrix;
    thePlanet->dtime = dtimeMatrix;
    return thePlanet;
}

wator_t *new_wator(char *fileplan)
{
    int sd, sb, fb;

    // Caricamento file configurazione
    FILE *file = fopen(CONFIGURATION_FILE, "r");
    if (file == NULL) {
        DEBUG_PRINTF("Errore nell'apertura di %s (%s).\n", CONFIGURATION_FILE, strerror(errno));
        return NULL; // errno settato da fopen
    }

    int argsAssigned = fscanf(file, "sd %d\nsb %d\nfb %d", &sd, &sb, &fb);
    fclose(file);
    if (argsAssigned != 3) {
        DEBUG_PRINTF("Formato del file di configurazione %s non riconosciuto.\n", fileplan);
        errno = ERANGE;
        return NULL;
    }

    FILE *planetFile = fopen(fileplan, "r");
    if (planetFile == NULL) {
        DEBUG_PRINTF("Errore nell'apertura di %s (%s).\n", fileplan, strerror(errno));
        return NULL; // errno settato da fopen
    }
    planet_t *thePlanet = load_planet(planetFile);
    fclose(planetFile);
    if (!thePlanet)
        return NULL; // errno settato da load_planet

    wator_t *aWator = (wator_t *) malloc(sizeof(wator_t));
    if (!aWator) {
        free_planet(thePlanet);
        return NULL; // errno settato da malloc
    }

    aWator->sd      = sd;
    aWator->sb      = sb;
    aWator->fb      = fb;
    aWator->nwork   = 0;
    aWator->chronon = 0;
    aWator->nf      = fish_count(thePlanet);
    aWator->ns      = shark_count(thePlanet);
    aWator->plan    = thePlanet;
    return aWator;
}

void free_wator(wator_t *pw)
{
    if (pw != NULL) {
        free_planet(pw->plan);
        free(pw);
    }
}

inline int shark_rule1(wator_t *pw, int x, int y, int *k, int *l)
{
    if (pw == NULL || pw->plan == NULL) {
        errno = EINVAL;
        return -1;
    }
    DEBUG_ASSERT(x >= 0 && y >= 0 && pw->plan->w[x][y] == SHARK);

    planet_t *p = pw->plan;
    motion_t motions[4] = {UP, RIGHT, DOWN, LEFT};
    cell_t cell;
    int waterCellsX[4];      // conterrà waterCellsCount ascisse di celle libere
    int waterCellsY[4];      // conterrà waterCellsCount ordinate di celle libere
    int waterCellsCount = 0; // il contatore di celle adiacenti libere (da 0 a 4)
    int destX, destY;

    for (int i = 0; i < 4; i++) { // Gli squali mangiano...
        cell = neighbor_cell(p, x, y, motions[i], &destX, &destY);
        if (cell == FISH) {
            p->w[destX][destY] = WATER;
            p->dtime[x][y] = 0;
            *k = destX;
            *l = destY;
            pw->nf--;
            move_cell(p, x, y, *k, *l);
            return EAT;
        }
        else if (cell == WATER) {
            // salva le celle dove lo squalo si potrà spostare se non ci sono pesci da mangiare
            waterCellsX[waterCellsCount] = destX;
            waterCellsY[waterCellsCount] = destY;
            waterCellsCount++;
        }
    }

    if (waterCellsCount > 0) { // ... si spostano ...
        int randomIndex = rand() % waterCellsCount;
        *k = waterCellsX[randomIndex];
        *l = waterCellsY[randomIndex];
        move_cell(p, x, y, *k, *l);
        return MOVE;
    }

    *k = x;
    *l = y;
    return STOP; // ... o rimangono fermi!
}

inline int shark_rule2(wator_t *pw, int x, int y, int *k, int *l)
{
    if (pw == NULL || pw->plan == NULL) {
        errno = EINVAL;
        return -1;
    }
    DEBUG_ASSERT(x >= 0 && y >= 0 && pw->plan->w[x][y] == SHARK);

    *k = -1; // nessun figlio... per adesso
    *l = -1;

    planet_t *p = pw->plan;
    if (p->btime[x][y] < pw->sb)
        p->btime[x][y] += 1;
    else { // prova a partorire
        p->btime[x][y] = 0;
        int cell;
        int destX, destY;
        motion_t motions[4] = {UP, RIGHT, DOWN, LEFT}; // le celle da ispezionare
        for (int i = 0; i < 4; i++) {
            cell = neighbor_cell(p, x, y, motions[i], &destX, &destY);
            if (cell == WATER) { // c'è spazio per il figlio
                *k = destX;
                *l = destY;
                pw->ns++;
                p->w[destX][destY] = SHARK;
                break;
            }
        }
    }

    if (p->dtime[x][y] < pw->sd) {
        p->dtime[x][y] += 1;
        return ALIVE;
    }
    else {
        p->w[x][y] = WATER;
        p->btime[x][y] = 0;
        p->dtime[x][y] = 0;
        pw->ns--;
        return DEAD;
    }
}

inline int fish_rule3(wator_t *pw, int x, int y, int *k, int *l)
{
    if (pw == NULL || pw->plan == NULL) {
        errno = EINVAL;
        return -1;
    }
    DEBUG_ASSERT(x >= 0 && y >= 0 && pw->plan->w[x][y] == FISH);

    planet_t *p = pw->plan;
    motion_t motions[4] = {UP, RIGHT, DOWN, LEFT};
    cell_t cell;
    int waterCellsX[4];      // conterrà waterCellsCount ascisse di celle libere
    int waterCellsY[4];      // conterrà waterCellsCount ordinate di celle libere
    int waterCellsCount = 0; // il contatore di celle adiacenti libere (da 0 a 4)
    int destX, destY;

    for (int i = 0; i < 4; i++) {
        cell = neighbor_cell(p, x, y, motions[i], &destX, &destY);
        if (cell == WATER) {
            waterCellsX[waterCellsCount] = destX;
            waterCellsY[waterCellsCount] = destY;
            waterCellsCount++;
        }
    }

    if (waterCellsCount > 0) { // sceglie una cella
        int randomIndex = rand() % waterCellsCount;
        *k = waterCellsX[randomIndex];
        *l = waterCellsY[randomIndex];
        move_cell(p, x, y, *k, *l);
        return MOVE;
    }

    *k = x; // non ci sono celle libere e il pesce rimane fermo
    *l = y;
    return STOP;
}

inline int fish_rule4(wator_t *pw, int x, int y, int *k, int *l)
{
    if (pw == NULL || pw->plan == NULL) {
        errno = EINVAL;
        return -1;
    }
    DEBUG_ASSERT(x >= 0 && y >= 0 && pw->plan->w[x][y] == FISH);

    *k = -1; // nessun figlio... per adesso
    *l = -1;

    planet_t *p = pw->plan;
    if (p->btime[x][y] < pw->fb)
        p->btime[x][y] += 1;
    else { // prova a partorire
        p->btime[x][y] = 0;
        int cell;
        int destX, destY;
        motion_t motions[4] = {UP, RIGHT, DOWN, LEFT}; // le celle da ispezionare
        for (int i = 0; i < 4; i++) {
            cell = neighbor_cell(p, x, y, motions[i], &destX, &destY);
            if (cell == WATER) {
                *k = destX;
                *l = destY;
                pw->nf++;
                p->w[destX][destY] = FISH;
                break; // è riuscito a partorire
            }
        }
    }
    return 0;
}

inline int neighbor_cell(planet_t *p, int x, int y, motion_t m, int *destX, int *destY)
{
    if (p == NULL)
        return -1;

    switch (m) {
        // Il cast evita che (x-1) venga trasformato in unsigned, con conseguente underflow quando x è 0
        case UP:
            *destX = (x - 1) % (int) p->nrow;
            *destY = y;
            break;
        case DOWN:
            *destX = (x + 1) % (int) p->nrow;
            *destY = y;
            break;
        case LEFT:
            *destX = x;
            *destY = (y - 1) % (int) p->ncol;
            break;
        case RIGHT:
            *destX = x;
            *destY = (y + 1) % (int) p->ncol;
            break;
        default:
            return -1;
    }

    // Trasforma l'operazione resto del C in operazione modulo
    if (*destX < 0)
        *destX += (int) p->nrow;
    if (*destY < 0)
        *destY += (int) p->ncol;

    // Il cast suggerisce al compilatore di non usare una versione della cella salvata nella cache
    return *(volatile cell_t*)&p->w[*destX][*destY];
}

int alloc_counters_matrices(unsigned int nrows, unsigned int ncols, int ***btime, int ***dtime)
{
    int **tempBtime = (int **) malloc(nrows * sizeof(int *));
    int **tempDtime = (int **) malloc(nrows * sizeof(int *));
    if (tempBtime == NULL || tempDtime == NULL) {
        free(tempBtime);
        free(tempDtime);
        return -1;
    }

    bool outOfMemory = false;
    unsigned int row = 0;     // È qui perché essere visibile al codice di pulizia
    for (; row < nrows; row++) {
        tempBtime[row] = (int *) calloc(ncols, sizeof(int));
        tempDtime[row] = (int *) calloc(ncols, sizeof(int));
        if (tempBtime[row] == NULL || tempDtime[row] == NULL) {
            outOfMemory = true;
            DEBUG_PRINTF("Non è stato possibile allocare la riga %d.\n", row);
            break;
        }
    }

    if (outOfMemory) { // Una malloc ha fallito, fa pulizia
        for (unsigned int i = 0; i < row; i++) {
            free(tempBtime[i]);
            free(tempDtime[i]);
        }
        free(tempBtime);
        free(tempDtime);
        return -1;
    }

    *btime = tempBtime;
    *dtime = tempDtime;
    return 0;
}

inline void move_cell(planet_t *p, int fromX, int fromY, int toX, int toY)
{
    if (p->w[toX][toY] != WATER)
        return;

    cell_t who = p->w[fromX][fromY];
    if (who == FISH) {
        p->w[toX][toY] = FISH;
        p->w[fromX][fromY] = WATER;
        p->btime[toX][toY] = p->btime[fromX][fromY];
        p->btime[fromX][fromY] = 0;
    }
    else if (who == SHARK) {
        p->w[toX][toY] = SHARK;
        p->w[fromX][fromY] = WATER;
        p->dtime[toX][toY] = p->dtime[fromX][fromY];
        p->dtime[fromX][fromY] = 0;
        p->btime[toX][toY] = p->btime[fromX][fromY];
        p->btime[fromX][fromY] = 0;
    }
}

int fish_count(planet_t *p)
{
    if (p == NULL) {
        errno = EINVAL;
        return -1;
    }

    int result = 0;
    for (unsigned int row = 0; row < p->nrow; row++)
        for (unsigned int col = 0; col < p->ncol; col++)
            if (p->w[row][col] == FISH)
                result++;
    return result;
}

int shark_count(planet_t *p)
{
    if (p == NULL) {
        errno = EINVAL;
        return -1;
    }

    int result = 0;
    for (unsigned int row = 0; row < p->nrow; row++)
        for (unsigned int col = 0; col < p->ncol; col++)
            if (p->w[row][col] == SHARK)
                result++;
    return result;
}

/*  INFO PER LA COMPRENSIONE DELL'ALGORITMO
    Per garantire che uno squalo o un pesce venga aggiornato esattamente una
    volta per ogni chronon, si fa uso di due array di bool, grandi ncol e
    chiamati cellsToSkipCurrRow, cellsToSkipNextRow. Ad ogni iterazione, se
    cellsToSkipCurrRow[c]=1 allora all'elemento in quella posizione non vanno
    applicate regole. Altrimenti le regole vanno applicate e, se lo squalo, il
    pesce, o un loro figlio:
    - si è spostato nella riga in basso (destR > r) allora imposta
      cellsToSkipNextRow[destC]=1 per ignorarlo in una delle prossime iterazioni
    - si è spostato nella riga a destra, allora imposta
      cellsToSkipCurrRow[destC]=1 per ignorarlo alla prossima iterazione.


    NOTA:
    - Non vengono controllati i valori restituiti dalle 4 regole perché
      non falliranno mai (le uniche condizioni in cui potrebbero tornare -1 sono
      state escluse con la prima guardia di update_wator).
    - Per semplificare il codice di update_wator è stata definita qui sotto una
      macro che applica la giusta regola in base al tipo di animale.
 */

#define APPLY_PROPER_RULE_NO(ruleNumber, toWho, x, y, k, l) { \
    switch (ruleNumber) { \
        case 1: \
            if (toWho == SHARK) shark_rule1(pw, x, y, k, l); \
            else                 fish_rule3(pw, x, y, k, l); \
            break; \
        case 2: \
            if (toWho == SHARK) shark_rule2(pw, x, y, k, l); \
            else                 fish_rule4(pw, x, y, k, l); \
            break; \
    } }

int update_wator(wator_t *pw)
{
    if (pw == NULL || pw->plan == NULL) {
        errno = EINVAL;
        return -1;
    }

    planet_t *p = pw->plan;
    const int rows = p->nrow;
    const int cols = p->ncol;
    bool *cellsToSkipCurrRow = (bool *) calloc(cols, sizeof(bool));
    bool *cellsToSkipNextRow = (bool *) malloc(cols * sizeof(bool));

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            cellsToSkipNextRow[c] = false;
            if (cellsToSkipCurrRow[c])
                continue;

            cell_t radar = p->w[r][c];
            if (radar != WATER) {
                int destR = -1, destC = -1;
                APPLY_PROPER_RULE_NO(1, radar, r, c, &destR, &destC);
                if (destC == 0 || destC > c) // Spostato a destra
                    cellsToSkipCurrRow[destC] = true;
                else if (destR == 0 || destR > r) // Spostato in basso
                    cellsToSkipNextRow[destC] = true;
                APPLY_PROPER_RULE_NO(2, radar, destR, destC, &destR, &destC);
                if (destC == 0 || destC > c) // C'è stato un parto nella cella a destra
                    cellsToSkipCurrRow[destC] = true;
                else if (destR == 0 || destR > r) // C'è stato un parto nella cella in basso
                    cellsToSkipNextRow[destC] = true;
            }
        }

        bool *tmp = cellsToSkipCurrRow;
        cellsToSkipCurrRow = cellsToSkipNextRow;
        cellsToSkipNextRow = tmp;
    }

    free(cellsToSkipCurrRow);
    free(cellsToSkipNextRow);
    pw->chronon++;

    return 0;
}

/*  INFO PER LA COMPRENSIONE DELL'ALGORITMO
    Similmente al metodo update_wator, dobbiamo tener traccia delle celle già
    aggiornate. Nella seguente funzione però non è possibile ottimizzare
    lo spazio usando solo un array di bool per la riga corrente e uno per la
    successiva.
    È necessario passare come argomento una matrice di bool, cosicché le future
    chiamate a update_wator_rect possano conoscere quali celle sono state già
    aggiornate

 */
inline int update_wator_rect(wator_t *pw, rect_t *rect, bool **cellsToSkipMatrix)
{
    if (pw == NULL || pw->plan == NULL || cellsToSkipMatrix == NULL
        || rect->fromRow < 0
        || rect->fromCol < 0
        || (unsigned int) rect->fromRow + rect->rows > pw->plan->nrow
        || (unsigned int) rect->fromCol + rect->cols > pw->plan->ncol) {
        DEBUG_ASSERT(false);
        errno = EINVAL;
        return -1;
    }

    planet_t *p = pw->plan;

    const int fromRow = rect->fromRow;
    const int fromCol = rect->fromCol;
    const int toRow   = rect->fromRow + rect->rows - 1;
    const int toCol   = rect->fromCol + rect->cols - 1;

    for (int r = fromRow; r <= toRow; r++) {
        for (int c = fromCol; c <= toCol; c++) {
            if (*(volatile bool*)&cellsToSkipMatrix[r][c])
                continue;

            cell_t radar = *(volatile cell_t*)&p->w[r][c];
            if (radar != WATER) {
                int destR = -1, destC = -1;
                APPLY_PROPER_RULE_NO(1, radar, r, c, &destR, &destC);
                cellsToSkipMatrix[destR][destC] = true;
                APPLY_PROPER_RULE_NO(2, radar, destR, destC, &destR, &destC);
                if (destR != -1) // => destC != -1 => c'è stato un parto
                    cellsToSkipMatrix[destR][destC] = true;
            }
        }
    }

    return 0;
}

inline rect_t *make_rect(int fromRow, int fromCol, int width, int height)
{
    DEBUG_ASSERT(fromRow >= 0 && fromCol >= 0 && width > 0 && height > 0);
    rect_t *r = malloc(sizeof(rect_t));
    if (!r)
        return NULL;
    r->fromRow = fromRow;
    r->fromCol = fromCol;
    r->cols = width;
    r->rows = height;
    return r;
}
