#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdio.h>  /* ssize_t */

/* ============================================================================
 * DEBUG: Codici ANSI per output colorato nel terminale
 * ============================================================================
 */

#define ANSI_COLOR_GREEN   "\x1b[32m"  /* Colore verde per messaggi di debug */
#define ANSI_COLOR_RED     "\x1b[31m"  /* Colore rosso (non usato attualmente) */
#define ANSI_COLOR_RESET   "\x1b[0m"   /* Reset al colore di default */


/* ============================================================================
 * TIPO ENUMERATIVO
 * ============================================================================
 */

/**
 * @enum B_TYPE
 * @brief Enumera i tipi di dati supportati da bencode
 *
 * Rappresenta i diversi tipi di elementi che possono essere codificati/decodificati
 * nel formato bencode (utilizzato da torrent):
 * - B_INT:  intero (es. i42e)
 * - B_STR:  bytestring (es. 4:spam)
 * - B_LIS:  lista (es. li1ei2ee)
 * - B_DICT: dizionario (es. d3:key5:valuee)
 * - B_HEX:  bytestring esadecimale per payload binari (es. pieces)
 * - B_NULL: tipo non inizializzato (indica errore)
 */
typedef enum {
    B_INT,   /* Intero */
    B_STR,   /* Bytestring (stringa) */
    B_LIS,   /* Lista */
    B_DICT,  /* Dizionario */
    B_HEX,   /* Bytestring esadecimale (binario) */
    B_NULL   /* Tipo non valido/non inizializzato */
} B_TYPE;


/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================
 */

union bencoded_union;
struct pieces;
struct bencoded_element;
struct bencoded_list;
struct bencoded_dict;
struct blist_node;
struct bdict_node;


/* ============================================================================
 * UNION: contenitore polimorfo per i dati bencodificati
 * ============================================================================
 */

/**
 * @union bencoded_union
 * @brief Union polimorfa per memorizzare diversi tipi di dati
 *
 * Consente di memorizzare un solo tipo di dato alla volta, risparmiando memoria.
 * Viene utilizzata all'interno di b_obj per memorizzare il contenuto effettivo.
 *
 * Contiene:
 * - pieces:   puntatore a dati binari (per B_HEX)
 * - int_str:  puntatore a elemento intero/stringa (per B_INT, B_STR)
 * - list:     puntatore a lista bencodificata (per B_LIS)
 * - dict:     puntatore a dizionario bencodificato (per B_DICT)
 */
union bencoded_union {
    struct pieces *pieces;           /* Dati binari esadecimali */
    struct bencoded_element *int_str; /* Intero o stringa decodificati */
    struct bencoded_list *list;      /* Lista bencodificata */
    struct bencoded_dict *dict;      /* Dizionario bencodificato */
};
typedef union bencoded_union b_box;


/* ============================================================================
 * STRUCT: oggetto bencoded
 * ============================================================================
 */

/**
 * @struct bencoded_object
 * @brief Wrapper che associa un tipo (B_TYPE) ai suoi dati
 *
 * Rappresenta un elemento generico bencodificato con informazioni sul tipo.
 * La struttura contiene:
 * - type:   enumera quale tipo di dato è memorizzato
 * - object: puntatore all'union che contiene i dati effettivi
 *
 * Consente di lavorare con dati eterogenei in modo type-safe.
 */
struct bencoded_object {
    B_TYPE type;      /* Tipo di dato memorizzato */
    b_box *object;    /* Puntatore ai dati effettivi */
};
typedef struct bencoded_object b_obj;


/* ============================================================================
 * STRUCT: dati binari/esadecimali (per campo "pieces" nei metafile .torrent)
 * ============================================================================
 */

/**
 * @struct pieces
 * @brief Contenitore per dati binari esadecimali (hash SHA1 dei pezzi)
 *
 * Memorizza il payload binario decodificato per bytestring esadecimali,
 * tipicamente il campo "pieces" in un metafile .torrent che contiene
 * gli hash SHA1 di ogni pezzo del file.
 *
 * Campi:
 * - decoded_pieces: buffer che contiene i dati binari decodificati
 * - lenght:         lunghezza in byte dei dati
 */
struct pieces {
    unsigned char *decoded_pieces; /* Buffer con i dati binari */
    ssize_t lenght;                /* Lunghezza del buffer */
};
typedef struct pieces b_pieces;


/* ============================================================================
 * STRUCT: elemento intero o stringa
 * ============================================================================
 */

/**
 * @struct bencoded_element
 * @brief Contenitore per un intero o una stringa bencodificati
 *
 * Memorizza sia la forma codificata che quella decodificata di un elemento
 * scalare (intero o bytestring).
 *
 * Campi:
 * - encoded_element:   forma originale bencodificata (es. "i42e" o "4:spam")
 * - decoded_element:   forma decodificata leggibile (es. "42" o "spam")
 * - lenght:            lunghezza totale dell'elemento codificato
 */
struct bencoded_element {
    char *encoded_element;  /* Forma bencodificata originale */
    char *decoded_element;  /* Forma decodificata leggibile */
    ssize_t lenght;         /* Lunghezza della forma codificata */
};
typedef struct bencoded_element b_element;


/* ============================================================================
 * STRUCT: nodo di una lista bencodificata
 * ============================================================================
 */

/**
 * @struct blist_node
 * @brief Nodo di una lista concatenata per memorizzare elementi bencodificati
 *
 * Implementa un nodo di una lista singolarmente concatenata.
 * Ogni nodo contiene un elemento generico (b_obj) e un puntatore al nodo successivo.
 *
 * Campi:
 * - object: puntatore all'elemento bencodificato
 * - next:   puntatore al nodo successivo (NULL se ultimo)
 */
struct blist_node {
    b_obj *object;           /* Elemento memorizzato */
    struct blist_node *next; /* Puntatore al prossimo nodo */
};
typedef struct blist_node list_node;


/* ============================================================================
 * STRUCT: nodo di un dizionario bencodificato
 * ============================================================================
 */

/**
 * @struct bdict_node
 * @brief Nodo di una lista concatenata per memorizzare coppie chiave-valore
 *
 * Implementa un nodo di una lista singolarmente concatenata per dizionari.
 * Ogni nodo contiene una coppia chiave-valore (entrambi b_obj).
 *
 * Campi:
 * - key:   puntatore all'elemento che rappresenta la chiave
 * - value: puntatore all'elemento che rappresenta il valore
 * - next:  puntatore al nodo successivo (NULL se ultimo)
 *
 * Nota: In bencode, le chiavi sono sempre stringhe e devono essere ordinate
 *       lessicograficamente (non è garantito da questa implementazione).
 */
struct bdict_node {
    b_obj *key;              /* Chiave della coppia (generalmente una stringa) */
    b_obj *value;            /* Valore associato alla chiave */
    struct bdict_node *next; /* Puntatore al prossimo nodo */
};
typedef struct bdict_node dict_node;


/* ============================================================================
 * STRUCT: lista bencodificata
 * ============================================================================
 */

/**
 * @struct bencoded_list
 * @brief Contenitore per una lista bencodificata
 *
 * Rappresenta una lista completa in formato bencode (es. li1e4:spamee).
 * Memorizza sia la forma codificata che una rappresentazione in memoria.
 *
 * Campi:
 * - enocded_list: forma bencodificata originale (NOTA: typo nel nome "enocded")
 * - list:         puntatore al primo nodo della lista concatenata
 * - lenght:       lunghezza totale della forma codificata
 */
struct bencoded_list {
    char *enocded_list;   /* Forma bencodificata originale [NOTA: typo nel nome] */
    list_node *list;      /* Puntatore al primo nodo della lista */
    ssize_t lenght;       /* Lunghezza della forma codificata */
};
typedef struct bencoded_list b_list;


/* ============================================================================
 * STRUCT: dizionario bencodificato
 * ============================================================================
 */

/**
 * @struct bencoded_dict
 * @brief Contenitore per un dizionario bencodificato
 *
 * Rappresenta un dizionario completo in formato bencode (es. d3:key5:valuee).
 * Memorizza sia la forma codificata che una rappresentazione in memoria.
 *
 * Campi:
 * - encoded_dict: forma bencodificata originale
 * - dict:         puntatore al primo nodo della lista concatenata (chiave-valore)
 * - lenght:       lunghezza totale della forma codificata
 */
struct bencoded_dict {
    char *encoded_dict; /* Forma bencodificata originale */
    dict_node *dict;    /* Puntatore al primo nodo del dizionario */
    ssize_t lenght;     /* Lunghezza della forma codificata */
};
typedef struct bencoded_dict b_dict;


/* ============================================================================
 * FUNZIONI: creazione e gestione liste
 * ============================================================================
 */

/**
 * @brief Inizializza una lista bencodificata vuota
 *
 * @return Puntatore a una nuova lista vuota allocata con malloc
 *         Il chiamante è responsabile di liberare la memoria con free()
 */
b_list* list_init(void);

/**
 * @brief Aggiunge un elemento in coda a una lista
 *
 * @param lista Puntatore alla lista dove aggiungere l'elemento
 * @param elem  Puntatore all'elemento (b_obj) da aggiungere
 *
 * @note La funzione alloca un nuovo nodo con malloc.
 *       In caso di errore di allocazione, il programma termina con exit(-1).
 */
void list_add(b_list *lista, b_obj *elem);


/* ============================================================================
 * FUNZIONI: creazione e gestione dizionari
 * ============================================================================
 */

/**
 * @brief Inizializza un dizionario bencodificato vuoto
 *
 * @return Puntatore a un nuovo dizionario vuoto allocato con malloc
 *         Il chiamante è responsabile di liberare la memoria con free()
 */
b_dict* dict_init(void);

/**
 * @brief Aggiunge una coppia chiave-valore a un dizionario
 *
 * @param dict Puntatore al dizionario dove aggiungere la coppia
 * @param key  Puntatore all'elemento (b_obj) che rappresenta la chiave
 * @param val  Puntatore all'elemento (b_obj) che rappresenta il valore
 *
 * @note La funzione alloca un nuovo nodo con malloc.
 *       In caso di errore di allocazione, il programma termina con exit(-1).
 *       Non garantisce che le chiavi rimangono ordinate lessicograficamente.
 */
void dict_add(b_dict *dict, b_obj *key, b_obj *val);

/*  ============================================================================
 *  FUNZIONI: deallocazione memoria
 *  ============================================================================
 */


 /**
  * @brief Libera ricorsivamente un oggetto bencodificato generico (b_obj)
  *
  * Dealloca tutta la memoria associata a un b_obj in base al suo tipo.
  * Per oggetti composti (B_LIS, B_DICT) la deallocazione è ricorsiva.
  *
  * @param ptr Puntatore all'oggetto da liberare. Non deve essere NULL.
  *
  * @note Dopo la chiamata ptr è invalidato; impostarlo a NULL per sicurezza.
  */
 void free_obj(b_obj *ptr);

 /**
  * @brief Libera tutti i nodi e la struttura di una lista bencodificata (b_list)
  *
  * Itera la lista concatenata liberando ricorsivamente ogni elemento
  * tramite free_obj(), poi libera la stringa codificata e la b_list stessa.
  *
  * @param ptr Puntatore alla lista da liberare. Non deve essere NULL.
  *
  * @note Libera anche la struttura b_list radice (ptr stesso).
  */
 void free_listNodes(b_list *ptr);

 /**
  * @brief Libera tutti i nodi e la struttura di un dizionario bencodificato (b_dict)
  *
  * Itera la lista di coppie chiave-valore liberando ricorsivamente chiave
  * e valore di ogni nodo tramite free_obj(), poi libera la stringa codificata
  * e la b_dict stessa.
  *
  * @param ptr Puntatore al dizionario da liberare. Non deve essere NULL.
  *
  * @note Libera anche la struttura b_dict radice (ptr stesso).
  */
 void free_dictNodes(b_dict *ptr);


/* ============================================================================
 * FUNZIONI: query sul tipo di dato
 * ============================================================================
 */

/**
 * @brief Restituisce il tipo di un oggetto bencodificato
 *
 * @param obj Puntatore all'oggetto (b_obj)
 * @return    Il tipo B_TYPE dell'oggetto
 */
B_TYPE get_object_type(b_obj *obj);

/**
 * @brief Restituisce il tipo dell'elemento memorizzato in un nodo di lista
 *
 * @param node Puntatore al nodo della lista
 * @return     Il tipo B_TYPE dell'elemento
 */
B_TYPE get_list_node_type(list_node *node);

/**
 * @brief Restituisce il tipo del valore in un nodo di dizionario
 *
 * @param node Puntatore al nodo del dizionario
 * @return     Il tipo B_TYPE del valore
 */
B_TYPE get_dict_value_type(dict_node *node);


/* ============================================================================
 * FUNZIONI: stampa e output
 * ============================================================================
 */

/**
 * @brief Stampa un buffer di byte in formato esadecimale
 *
 * @param pieces Puntatore al buffer di byte da stampare
 * @param lenght Numero di byte da stampare
 *
 * @note Stampa i byte in formato "XX XX XX ..." (es. "48 65 6C 6C 6F")
 */
void print_hex(unsigned char *pieces, size_t lenght);

/**
 * @brief Stampa il contenuto di una lista bencodificata
 *
 * @param lista Puntatore alla lista da stampare
 *
 * @note Stampa ricorsivamente gli elementi della lista.
 *       Termina il programma con exit(-1) se incontra un tipo non valido.
 */
void print_list(b_list *lista);

/**
 * @brief Stampa il contenuto di un dizionario bencodificato
 *
 * @param dict Puntatore al dizionario da stampare
 *
 * @note Stampa ricorsivamente le coppie chiave-valore del dizionario.
 *       Termina il programma con exit(-1) se incontra un tipo non valido.
 */
void print_dict(b_dict *dict);

/**
 * @brief Stampa il contenuto di un oggetto generico
 *
 * @param obj            Puntatore all'oggetto da stampare
 * @param pieces_lenght  Lunghezza dei dati esadecimali (per tipo B_HEX)
 *
 * @note Stampa ricorsivamente l'oggetto in base al suo tipo.
 *       Termina il programma con exit(-1) se incontra un tipo non valido.
 */
void print_object(b_obj *obj, size_t pieces_lenght);


/* ============================================================================
 * FUNZIONI: ricerca e query su dizionari
 * ============================================================================
 */

/**
 * @brief Ricerca un valore in un dizionario per chiave e restituisce un sottodizionario
 *
 * @param dict Puntatore al dizionario dove cercare
 * @param key  Stringa che rappresenta la chiave da ricercare
 *
 * @return Puntatore al valore trovato se la chiave esiste e il valore è un dizionario,
 *         NULL altrimenti (stampa "NOT FOUND!" su stdout)
 *
 * @note Utile per navigare strutture nidificate (es. metafile .torrent).
 */
b_dict* get_info_dict(b_dict *dict, char *key);

/**
 * @brief Ricerca una chiave in un dizionario e stampa il valore associato
 *
 * @param dict Puntatore al dizionario dove cercare
 * @param key  Stringa che rappresenta la chiave da ricercare
 *
 * @note Stampa "FOUND: " seguita dal valore se trovato,
 *       oppure "NOT FOUND!" se la chiave non esiste.
 */
void find_by_key(b_dict *dict, char *key);


#endif  /* STRUCTS_H */
