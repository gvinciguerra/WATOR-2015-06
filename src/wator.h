/** \file wator.h
    \author lso15 & Giorgio Vinciguerra
    \date Aprile 2015
    \brief File contenente i prototipi delle funzioni della librearia Wator
*/
#ifndef __WATOR__H
#define __WATOR__H

#include <stdio.h>
#include <stdbool.h>

/** file di configurazione */
static const char CONFIGURATION_FILE[] = "wator.conf";

/** tipo delle celle del pianeta:
   SHARK squalo
   FISH pesce
   WATER solo acqua

*/
typedef enum cell { SHARK, FISH, WATER } cell_t;

/** tipo matrice acquatica che rappreseneta il pianeta */
typedef struct planet {
  /** righe */
  unsigned int nrow;
  /** colonne */
  unsigned int ncol;
  /** matrice pianeta */
  cell_t ** w;
  /** matrice contatori nascita (pesci e squali)*/
  int ** btime;
  /** matrice contatori morte (squali )*/
  int ** dtime;

} planet_t;

/** struttura che raccoglie le informazioni di simulazione */
typedef struct wator {
  /** sd numero chronon morte squali per digiuno */
  int sd;
  /** sb numero chronon riproduzione squali */
  int sb;
  /** fb numero chronon riproduzione pesci */
  int fb;
  /** nf numero pesci*/
  int nf;
  /** ns numero squali */
  int ns;
  /** numero worker */
  int nwork;
  /** durata simulazione */
  int chronon;
  /** pianeta acquatico */
  planet_t* plan;
} wator_t;

/** struttura che rappresenta una porzione della matrice di un pianeta */
typedef struct prectangle {
    /** riga di partenza */
    int fromRow;
    /** colonna di partenza */
    int fromCol;
    /** la larghezza del rettangolo */
    int cols;
    /** l'altezza del rettangolo */
    int rows;
} rect_t;

/** alloca e ritorna un rect_t delle dimensioni specificate. */
rect_t *make_rect(int fromRow, int fromCol, int width, int height);

/** trasforma una cella in un carattere
   \param a cella da trasformare

   \return 'W' se a contiene WATER
   \return 'S' se a contiene SHARK
   \return 'F' se a contiene FISH
   \return '?' in tutti gli altri casi
  */
char cell_to_char(cell_t a) ;

/** trasforma un carattere in una cella
   \param c carattere da trasformare

   \return WATER se c=='W'
   \return SHARK se c=='S'
   \return FISH se c=='F'
   \return -1 in tutti gli altri casi
  */
int char_to_cell(char c) ;

/** crea un nuovo pianeta vuoto (tutte le celle contengono WATER) utilizzando
    la rappresentazione con un vettore di puntatori a righe
    \param nrow numero righe
    \param numero colonne

    \return NULL se si sono verificati problemi nell'allocazione
    \return p puntatore alla matrice allocata altrimenti
 */
planet_t *new_planet(unsigned int nrow, unsigned int ncol);

/** dealloca un pianeta (e tutta la matrice ...)
    \param p pianeta da deallocare

 */
void free_planet(planet_t* p);

/** stampa il pianeta su file secondo il formato di fig 2 delle specifiche, es

3
5
W F S W W
F S W W S
W W W W W

dove 3 e' il numero di righe (seguito da newline \n)
5 e' il numero di colonne (seguito da newline \n)
e i caratteri W/F/S indicano il contenuto (WATER/FISH/SHARK) separati da un carattere blank (' '). Ogni riga terminata da newline \n

    \param f file su cui stampare il pianeta (viene sovrascritto se esiste)
    \param p puntatore al pianeta da stampare

    \return 0 se tutto e' andato bene
    \return -1 se si e' verificato un errore (in questo caso errno e' settata opportunamente)

 */
int print_planet(FILE *f, planet_t *p);


/** inizializza il pianeta leggendo da file la configurazione iniziale

    \param f file da dove caricare il pianeta (deve essere gia' stato aperto in lettura)

    \return p puntatore al nuovo pianeta (allocato dentro la funzione)
    \return NULL se si e' verificato un errore (setta errno)
            errno = ERANGE se il file e' mal formattato
 */
planet_t *load_planet(FILE *f);



/** crea una nuova istanza della simulazione in base ai contenuti
    del file di configurazione "wator.conf"

    \param fileplan nome del file da cui caricare il pianeta

    \return p puntatore alla nuova struttura che descrive
              i parametri di simulazione
    \return NULL se si e' verificato un errore (setta errno)
 */
wator_t *new_wator(char *fileplan);

/** libera la memoria della struttura wator (e di tutte le sottostrutture)
    \param pw puntatore struttura da deallocare

 */
void free_wator(wator_t *pw);

#define STOP 0
#define EAT  1
#define MOVE 2
#define ALIVE 3
#define DEAD 4
/** Regola 1: gli squali mangiano e si spostano
  \param pw puntatore alla struttura di simulazione
  \param (x,y) coordinate iniziali dello squalo
  \param (*k,*l) coordinate finali dello squalo (modificate in uscita)

  La funzione controlla i 4 vicini
              (x-1,y)
        (x,y-1) *** (x,y+1)
              (x+1,y)

  Se una di queste celle contiene un pesce, lo squalo mangia il pesce e
  si sposta nella cella precedentemente occupata dal pesce. Se nessuna
  delle celle adiacenti contiene un pesce, lo squalo si sposta
  in una delle celle adiacenti vuote. Se ci sono piu' celle vuote o piu' pesci
  la scelta e' casuale.
  Se tutte le celle adiacenti sono occupate da squali
  non possiamo ne mangiare ne spostarci lo squalo rimane fermo.

  NOTA: la situazione del pianeta viene
        modificata dalla funzione con il nuovo stato

  \return STOP se siamo rimasti fermi
  \return EAT se lo squalo ha mangiato il pesce
  \return MOVE se lo squalo si e' spostato solamente
  \return -1 se si e' verificato un errore (setta errno)
 */
int shark_rule1(wator_t *pw, int x, int y, int *k, int *l);

/** Regola 2: gli squali si riproducono e muoiono
  \param pw puntatore alla struttura wator
  \param (x,y) coordinate dello squalo
  \param (*k,*l) coordinate dell'eventuale squalo figlio (modificate in uscita)
         oppure (-1,-1) se non c'è stato un parto

  La funzione calcola nascite e morti in base agli indicatori
  btime(x,y) e dtime(x,y).

  == btime : nascite ===
  Se btime(x,y) e' minore di  pw->sb viene incrementato.
  Se btime(x,y) e' uguale a pw->sb si tenta di generare un nuovo squalo.
  Si considerano i 4 vicini
              (x-1,y)
        (x,y-1) *** (x,y+1)
              (x+1,y)

  Se una di queste celle e' vuota lo squalo figlio viene generato e la occupa, se le celle sono tutte occupate da pesci o squali la generazione non avviene.
  In entrambi i casi btime(x,y) viene azzerato.

  == dtime : morte dello squalo  ===
  Se dtime(x,y) e' minore di pw->sd viene incrementato.
  Se dtime(x,y) e' uguale a pw->sd lo squalo muore e la sua posizione viene
  occupata da acqua.

  NOTA: la situazione del pianeta viene
        modificata dalla funzione con il nuovo stato


  \return DEAD se lo squalo e' morto
  \return ALIVE se lo squalo e' vivo
  \return -1 se si e' verificato un errore (setta errno)
 */
int shark_rule2 (wator_t *pw, int x, int y, int *k, int *l);

/** Regola 3: i pesci si spostano

    \param pw puntatore alla struttura di simulazione
    \param (x,y) coordinate iniziali del pesce
    \param (*k,*l) coordinate finali del pesce

    La funzione controlla i 4 vicini
              (x-1,y)
        (x,y-1) *** (x,y+1)
              (x+1,y)

     un pesce si sposta casualmente in una delle celle adiacenti (libera).
     Se ci sono piu' celle vuote la scelta e' casuale.
     Se tutte le celle adiacenti sono occupate rimaniamo fermi.

     NOTA: la situazione del pianeta viene
        modificata dalla funzione con il nuovo stato

  \return STOP se siamo rimasti fermi
  \return MOVE se il pesce si e' spostato
  \return -1 se si e' verificato un errore (setta errno)
 */
int fish_rule3(wator_t *pw, int x, int y, int *k, int *l);

/** Regola 4: i pesci si riproducono
  \param pw puntatore alla struttura wator
  \param (x,y) coordinate del pesce
  \param (*k,*l) coordinate dell'eventuale pesce figlio (modificate in uscita)
         oppure (-1,-1) se non c'è stato un parto

  La funzione calcola nascite in base a btime(x,y)

  Se btime(x,y) e' minore di  pw->sb viene incrementato.
  Se btime(x,y) e' uguale a pw->sb si tenta di generare un nuovo pesce.
  Si considerano i 4 vicini
              (x-1,y)
        (x,y-1) *** (x,y+1)
              (x+1,y)

  Se una di queste celle e' vuota il pesce figlio viene generato e la occupa, se le celle sono tutte occupate da pesci o squali la generazione non avviene.
  In entrambi i casi btime(x,y) viene azzerato.

  NOTA: la situazione del pianeta viene
        modificata dalla funzione con il nuovo stato


  \return  0 se tutto e' andato bene
  \return -1 se si e' verificato un errore (setta errno)
 */
int fish_rule4(wator_t *pw, int x, int y, int *k, int *l);


/** restituisce il numero di pesci nel pianeta
    \param p puntatore al pianeta

    \return n (>=0) numero di pesci presenti
    \return -1 se si e' verificato un errore (setta errno )
 */
int fish_count(planet_t *p);

/** restituisce il numero di squali nel pianeta
    \param p puntatore al pianeta

    \return n (>=0) numero di squali presenti
    \return -1 se si e' verificato un errore (setta errno )
 */
int shark_count(planet_t *p);


/** calcola un chronon aggiornando tutti i valori della simulazione e il pianeta
   \param pw puntatore al pianeta
   \return 0 se tutto e' andato bene
   \return -1 se si e' verificato un errore (setta errno)
 */
int update_wator(wator_t *pw);

/** tipo di movimenti che uno squalo o un pesce può fare nella matrice del
    pianeta, a partire dalla posizione (x, y):
    UP verso su, ossia (x-1, y)
    DOWN verso giù, ossia (x+1, y)
    LEFT verso sinistra, ossia (x, y-1)
    RIGHT verso destra, ossia (x, y+1)
 */
typedef enum motion { UP, DOWN, LEFT, RIGHT } motion_t;

/** esamina una cella adiacente alla posizione (x, y) della matrice del pianeta
    p, in base al movimento m specificato e rispettando la forma sferica del
    pianeta.

    \param p puntatore al pianeta
    \param (x,y) le coordinate di partenza
    \param m il tipo di movimento che si vuole fare
    \param (*destX,*destY) le coordinate di arrivo (modificate in uscita)
    \return WATER se in (destX,destY) c'è 'W'
    \return SHARK se in (destX,destY) c'è 'S'
    \return FISH se in (destX,destY) c'è 'F'
    \return -1 se si e' verificato un errore
 */
int neighbor_cell(planet_t *p, int x, int y, motion_t m, int *destX, int *destY);

/** crea e azzera le due matrici btime e dtime di un pianeta.

    \param (nrows, cols) le dimensioni del pianeta
    \param (***btime) il puntatore alla matrice btime (modificate in uscita)
    \param (***dtime) il puntatore alla matrice dtime (modificate in uscita)
    \return 0 se le matrici sono state allocate (nel chiamante, *btime *dtime
            conterranno i puntatori a tali matrici)
    \return -1 se si e' verificato un errore
 */
int alloc_counters_matrices(unsigned int nrows, unsigned int ncols, int ***btime, int ***dtime);

/** sposta un pesce o uno squalo dalle coordinate (fromX,fromY) a (toX, toY).
    Muove anche i contatori btime (e dtime nel caso di uno squalo) dalla vecchia
    alla nuova posizione. La cella abbandonata diventa WATER con i contatori
    azzerati. Assume che alle coordinate di arrivo ci sia acqua.

    \param p puntatore al pianeta
    \param (fromX,fromY) le coordinate di partenza
    \param (toX,toY) le coordinate di arrivo
 */
void move_cell(planet_t *p, int fromX, int fromY, int toX, int toY);

/** stampa il pianeta sullo stdout con caratteri colorati.

    \param p puntatore al pianeta
 */
int print_planet_colored(planet_t *p);

/** aggiorna una porzione del pianeta. Salta le celle x,y per le quali
    cellsToSkipMatrix[x,y]=true.

    \param pw puntatore al pianeta
    \param rect il rettangolo da aggiornare. Deve essere all'interno del pianeta
    \param cellsToSkipMatrix una matrice di bool di dimensioni pari a pw->plan
    \return 0 se tutto e' andato bene
    \return -1 se si e' verificato un errore (setta errno)
 */
int update_wator_rect(wator_t *pw, rect_t *rect, bool **cellsToSkipMatrix);

#endif
