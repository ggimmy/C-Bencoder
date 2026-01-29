#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "structs.h"

/* ============================================================================
 * FUNZIONI: Inizializzazione liste e dizionari
 * ============================================================================
 */

/**
 * @brief Inizializza una lista bencodificata vuota
 *
 * Alloca memoria per una nuova struttura b_list e inizializza tutti i campi:
 * - lenght impostato a 0 (lista vuota)
 * - enocded_list impostato a NULL
 * - list impostato a NULL (nessun nodo)
 *
 * @return Puntatore alla lista appena allocata
 *
 * @note La memoria deve essere liberata dal chiamante quando non più necessaria
 * @note Non controlla il fallimento di malloc (potrebbe causare crash se malloc fallisce)
 */
b_list* list_init(void) {
    b_list *newList = malloc(sizeof(b_list));
    newList->lenght = 0;
    newList->enocded_list = NULL;
    newList->list = NULL;

    return newList;
}


/**
 * @brief Inizializza un dizionario bencodificato vuoto
 *
 * Alloca memoria per una nuova struttura b_dict e inizializza tutti i campi:
 * - lenght impostato a 0 (dizionario vuoto)
 * - encoded_dict impostato a NULL
 * - dict impostato a NULL (nessun nodo)
 *
 * @return Puntatore al dizionario appena allocato
 *
 * @note La memoria deve essere liberata dal chiamante quando non più necessaria
 * @note Non controlla il fallimento di malloc (potrebbe causare crash se malloc fallisce)
 */
b_dict* dict_init(void) {
    b_dict *newDict = malloc(sizeof(b_dict));
    newDict->lenght = 0;
    newDict->encoded_dict = NULL;
    newDict->dict = NULL;

    return newDict;
}


/* ============================================================================
 * FUNZIONI: Aggiunta elementi a liste e dizionari
 * ============================================================================
 */

/**
 * @brief Aggiunge un elemento in coda a una lista bencodificata
 *
 * Crea un nuovo nodo list_node e lo inserisce alla fine della lista.
 * Se la lista è vuota, il nuovo nodo diventa la testa della lista.
 * Se la lista contiene elementi, il nuovo nodo viene appeso al final.
 *
 * Algoritmo:
 * 1. Alloca un nuovo nodo
 * 2. Imposta il puntatore all'elemento e next a NULL
 * 3. Se la lista è vuota, assegna il nodo come testa
 * 4. Altrimenti, attraversa fino al fine e appende il nodo
 *
 * @param lista Puntatore alla lista bencodificata dove aggiungere l'elemento
 * @param elem  Puntatore all'elemento (b_obj) da aggiungere
 *
 * @return void
 *
 * @note In caso di fallimento di malloc, stampa un errore su stderr e termina
 *       il programma con exit(-1)
 * @note La complessità è O(n) dove n è il numero di elementi già presenti
 * @note Per liste grandi, considerare di usare una coda per ottimizzare gli inserimenti
 */
void list_add(b_list *lista, b_obj *elem) {
    /* Alloca un nuovo nodo */
    list_node *newNode = malloc(sizeof(list_node));
    if (newNode) {
        newNode->object = elem;
        newNode->next = NULL;
    } else {
        fprintf(stderr, "Malloc failed in function list_add!\n");
        exit(-1);
    }

    /* Inserimento in lista vuota: il nuovo nodo diventa la testa */
    if (lista->list == NULL) {
        lista->list = newNode;
    }
    /* Inserimento in lista non vuota: appendi il nodo al fine */
    else {
        list_node *tmp = lista->list;
        while (tmp->next != NULL) {
            tmp = tmp->next;
        }
        tmp->next = newNode;
    }
}


/**
 * @brief Aggiunge una coppia chiave-valore a un dizionario bencodificato
 *
 * Crea un nuovo nodo dict_node contenente la coppia chiave-valore e lo inserisce
 * al fine del dizionario. Se il dizionario è vuoto, il nuovo nodo diventa il primo.
 *
 * Algoritmo:
 * 1. Alloca un nuovo nodo
 * 2. Imposta i puntatori a chiave e valore, next a NULL
 * 3. Se il dizionario è vuoto, assegna il nodo come primo
 * 4. Altrimenti, attraversa fino al fine e appende il nodo
 *
 * @param dict Puntatore al dizionario dove aggiungere la coppia
 * @param key  Puntatore all'elemento (b_obj) che rappresenta la chiave
 * @param val  Puntatore all'elemento (b_obj) che rappresenta il valore
 *
 * @return void
 *
 * @note In caso di fallimento di malloc, stampa un errore su stderr e termina
 *       il programma con exit(-1)
 * @note La complessità è O(n) dove n è il numero di coppie già presenti
 * @note In bencode, le chiavi dovrebbero essere ordinate lessicograficamente,
 *       ma questa implementazione non lo garantisce
 * @note Per dizionari grandi, considerare di usare una struttura dati più efficiente
 */
void dict_add(b_dict *dict, b_obj *key, b_obj *val) {
    /* Alloca un nuovo nodo */
    dict_node *newNode = malloc(sizeof(dict_node));
    if (newNode) {
        newNode->key = key;
        newNode->value = val;
        newNode->next = NULL;
    } else {
        fprintf(stderr, "Malloc failed in function dict_add!\n");
        exit(-1);
    }

    /* Inserimento in dizionario vuoto: il nuovo nodo diventa il primo */
    if (dict->dict == NULL) {
        dict->dict = newNode;
    }
    /* Inserimento in dizionario non vuoto: appendi il nodo al fine */
    else {
        dict_node *tmp = dict->dict;
        while (tmp->next != NULL) {
            tmp = tmp->next;
        }
        tmp->next = newNode;
    }
}


/* ============================================================================
 * FUNZIONI: Query sul tipo di dato
 * ============================================================================
 */

/**
 * @brief Restituisce il tipo di un oggetto bencodificato
 *
 * @param obj Puntatore all'oggetto (b_obj)
 * @return    Il tipo B_TYPE dell'oggetto (B_INT, B_STR, B_LIS, B_DICT, B_HEX, B_NULL)
 *
 * @note Questa è una semplice funzione getter
 */
B_TYPE get_object_type(b_obj *obj) {
    return obj->type;
}


/**
 * @brief Restituisce il tipo dell'elemento memorizzato in un nodo di lista
 *
 * Estrae il tipo dell'oggetto (b_obj) contenuto nel nodo della lista.
 *
 * @param node Puntatore al nodo della lista (list_node)
 * @return     Il tipo B_TYPE dell'elemento (B_INT, B_STR, B_LIS, B_DICT, B_HEX, B_NULL)
 *
 * @note Questa è una semplice funzione getter con un livello di indirezione in più
 */
B_TYPE get_list_node_type(list_node *node) {
    return node->object->type;
}


/**
 * @brief Restituisce il tipo del valore in un nodo di dizionario
 *
 * Estrae il tipo dell'oggetto (b_obj) che rappresenta il valore in una coppia
 * chiave-valore del dizionario.
 *
 * @param node Puntatore al nodo del dizionario (dict_node)
 * @return     Il tipo B_TYPE del valore (B_INT, B_STR, B_LIS, B_DICT, B_HEX, B_NULL)
 *
 * @note Questa è una semplice funzione getter
 * @note Non considera il tipo della chiave, solo del valore
 */
B_TYPE get_dict_value_type(dict_node *node) {
    return node->value->type;
}


/* ============================================================================
 * FUNZIONI: Stampa e output
 * ============================================================================
 */

/**
 * @brief Stampa un buffer di byte in formato esadecimale
 *
 * Itera attraverso il buffer e stampa ogni byte in formato esadecimale a 2 cifre,
 * separati da spazi. Utile per visualizzare dati binari come hash SHA1 o piece data.
 *
 * Esempio di output:
 *   48 65 6C 6C 6F 20 57 6F 72 6C 64
 *   (che corrisponde a "Hello World")
 *
 * @param pieces Puntatore al buffer di byte da stampare
 * @param lenght Numero di byte da stampare
 *
 * @return void
 *
 * @note Stampa su stdout (usare redirect per salvare su file)
 * @note Non stampa separatori di linea all'inizio, ma stampa un newline al fine
 * @note Non esegue alcun controllo su validità del puntatore o della lunghezza
 */
void print_hex(unsigned char *pieces, size_t lenght) {
    for (int i = 0; i < lenght; i++) {
        printf("%02X ", pieces[i]);
    }
    printf("\n");
}


/**
 * @brief Stampa ricorsivamente il contenuto di una lista bencodificata
 *
 * Itera attraverso i nodi della lista e stampa ogni elemento in base al suo tipo:
 * - B_INT: stampa il valore intero decodificato
 * - B_STR: stampa la stringa decodificata
 * - B_LIS: ricorsivamente stampa la sottolista
 * - B_DICT: ricorsivamente stampa il sottodizionario
 * - B_HEX: stampa i dati binari in formato esadecimale
 * - B_NULL: stampa un errore e termina il programma
 *
 * @param lista Puntatore alla lista (b_list) da stampare
 *
 * @return void
 *
 * @note Stampa su stdout
 * @note Termina il programma con exit(-1) se incontra un elemento di tipo B_HEX
 *       (comportamento che potrebbe essere un bug)
 * @note Termina il programma con exit(-1) se incontra un elemento di tipo B_NULL
 * @note La ricorsione permette di stampare liste nidificate
 */
void print_list(b_list *lista) {
    list_node *tmp = lista->list;

    while (tmp != NULL) {
        switch (get_list_node_type(tmp)) {
            case B_INT:
                printf("%s\n", tmp->object->object->int_str->decoded_element);
                break;

            case B_STR:
                printf("%s\n", tmp->object->object->int_str->decoded_element);
                break;

            case B_LIS:
                print_list(tmp->object->object->list);
                break;

            case B_DICT:
                print_dict(tmp->object->object->dict);
                break;

            case B_HEX:
                fprintf(stderr, "ERROR HEX BYTESTRING IN LIST!\n");
                exit(-1);

            case B_NULL:
                fprintf(stderr, "Error in function print_list NULL OBJECT TYPE!\n");
                exit(-1);
        }

        tmp = tmp->next;
    }
}


/**
 * @brief Stampa ricorsivamente il contenuto di un dizionario bencodificato
 *
 * Itera attraverso i nodi del dizionario e stampa ogni coppia chiave-valore.
 * La chiave viene sempre stampata come stringa, mentre il valore viene stampato
 * in base al suo tipo:
 * - B_INT: stampa la chiave e il valore intero
 * - B_STR: stampa la chiave e il valore stringa
 * - B_LIS: stampa la chiave e ricorsivamente la lista
 * - B_DICT: stampa la chiave e ricorsivamente il sottodizionario
 * - B_HEX: stampa la chiave e i dati binari in esadecimale
 * - B_NULL: stampa un errore e termina il programma
 *
 * Formato di output:
 *   chiave valore
 *   chiave valore
 *   ...
 *
 * @param dict Puntatore al dizionario (b_dict) da stampare
 *
 * @return void
 *
 * @note Stampa su stdout
 * @note Assume che la chiave sia sempre una stringa decodificata
 * @note Termina il programma con exit(-1) se incontra un valore di tipo B_NULL
 * @note La ricorsione permette di stampare dizionari nidificati
 * @note Nel caso B_HEX, chiama print_hex con lenght=0 (comportamento discutibile)
 */
void print_dict(b_dict *dict) {
    dict_node *tmp = dict->dict;

    while (tmp != NULL) {
        /* Stampa la chiave */
        printf("%s ", tmp->key->object->int_str->decoded_element);

        /* Stampa il valore in base al suo tipo */
        switch (get_dict_value_type(tmp)) {
            case B_INT:
                printf(" %s\n", tmp->value->object->int_str->decoded_element);
                break;

            case B_STR:
                printf(" %s\n", tmp->value->object->int_str->decoded_element);
                break;

            case B_LIS:
                print_list(tmp->value->object->list);
                break;

            case B_DICT:
                print_dict(tmp->value->object->dict);
                break;

            case B_HEX:
                print_hex(tmp->value->object->pieces->decoded_pieces, 0);
                break;

            case B_NULL:
                fprintf(stderr, "Error in function print_dict NULL OBJECT TYPE!\n");
                exit(-1);
        }

        tmp = tmp->next;
    }
}


/**
 * @brief Stampa ricorsivamente il contenuto di un oggetto generico bencodificato
 *
 * Funzione di alto livello che stampa un oggetto (b_obj) basandosi sul suo tipo.
 * È il punto di ingresso principale per la stampa di oggetti bencodificati.
 *
 * Comportamento per tipo:
 * - B_INT: stampa il valore intero decodificato
 * - B_STR: stampa la forma codificata (NOTA: stampa encoded, non decoded!)
 * - B_LIS: ricorsivamente stampa la lista
 * - B_DICT: ricorsivamente stampa il dizionario
 * - B_HEX: stampa i dati binari in formato esadecimale
 * - B_NULL: stampa un errore e termina il programma
 *
 * @param obj            Puntatore all'oggetto (b_obj) da stampare
 * @param pieces_lenght  Lunghezza dei dati per elementi di tipo B_HEX
 *                       (necessaria perché non è memorizzata nell'oggetto)
 *
 * @return void
 *
 * @note Stampa su stdout
 * @note Il parametro pieces_lenght è necessario perché la struttura b_pieces
 *       contiene la lunghezza ma viene passata errata (vedi print_dict)
 * @note Nota un bug potenziale: nel caso B_STR stampa l'elemento codificato
 *       mentre in altre funzioni stampa l'elemento decodificato
 * @note Manca un break dopo B_DICT (potrebbe causare fall-through al caso B_HEX)
 * @note Termina il programma con exit(-1) se incontra un oggetto di tipo B_NULL
 */
void print_object(b_obj *obj, size_t pieces_lenght) {
    switch (get_object_type(obj)) {
        case B_INT:
            printf("%s\n", obj->object->int_str->decoded_element);
            break;

        case B_STR:
            printf("%s\n", obj->object->int_str->encoded_element);
            break;

        case B_LIS:
            print_list(obj->object->list);
            break;

        case B_DICT:
            print_dict(obj->object->dict);
            break;

        case B_HEX:
            print_hex(obj->object->pieces->decoded_pieces, pieces_lenght);
            break;

        case B_NULL:
            fprintf(stderr, "Error in retriving object type in function 'print_object'!\n");
            exit(-1);
    }
}


/* ============================================================================
 * FUNZIONI: Ricerca e query su dizionari
 * ============================================================================
 */

/**
 * @brief Ricerca un valore in un dizionario per chiave e restituisce un sottodizionario
 *
 * Traversa il dizionario cercando una voce con la chiave specificata.
 * Se trovata, restituisce il valore associato (che deve essere un dizionario).
 * Se non trovata, stampa "NOT FOUND!" e restituisce NULL.
 *
 * Algoritmo:
 * 1. Itera attraverso i nodi del dizionario
 * 2. Confronta ogni chiave con quella cercata usando strcmp
 * 3. Se match, restituisce il valore (come b_dict)
 * 4. Se non trovato, stampa un messaggio e restituisce NULL
 *
 * Caso di uso tipico:
 *   Per navigare file .torrent:
 *   b_dict* root = parse_torrent_file(...);
 *   b_dict* info = get_info_dict(root, "info");
 *   if (info) {  accedi a sottocampi  }
 *
 * @param dict Puntatore al dizionario dove cercare
 * @param key  Stringa null-terminated che rappresenta la chiave da ricercare
 *
 * @return Puntatore al valore trovato se la chiave esiste e il valore è un dizionario,
 *         NULL altrimenti
 *
 * @note Stampa "NOT FOUND!" su stdout se la chiave non esiste
 * @note La complessità è O(n) dove n è il numero di coppie nel dizionario
 * @note Non verifica che il valore sia effettivamente un dizionario
 *       (potrebbe restituire un puntatore a un tipo diverso castato a b_dict)
 * @note Utilizza strcmp quindi il confronto è case-sensitive e dipende dalla locale
 */
b_dict* get_info_dict(b_dict *dict, char *key) {
    dict_node *tmp = dict->dict;

    while (tmp != NULL) {
        if (strcmp(key, tmp->key->object->int_str->decoded_element) == 0) {
            return tmp->value->object->dict;
        } else {
            tmp = tmp->next;
        }
    }

    printf("NOT FOUND!\n");
    return NULL;
}


/**
 * @brief Ricerca una chiave in un dizionario e stampa il valore associato
 *
 * Traversa il dizionario cercando una voce con la chiave specificata.
 * Se trovata, stampa "FOUND: " seguito dal valore e ritorna.
 * Se non trovata, stampa "NOT FOUND!" e ritorna.
 *
 * Algoritmo:
 * 1. Itera attraverso i nodi del dizionario
 * 2. Confronta ogni chiave con quella cercata usando strcmp
 * 3. Se match, stampa "FOUND: " e chiama print_object per il valore
 * 4. Se non trovato dopo aver traversato tutto, stampa "NOT FOUND!"
 *
 * Caso di uso tipico:
 *   find_by_key(torrent_dict, "announce");
 *   // Stampa: "FOUND: http://tracker.example.com:6969/announce"
 *
 * @param dict Puntatore al dizionario dove cercare
 * @param key  Stringa null-terminated che rappresenta la chiave da ricercare
 *
 * @return void
 *
 * @note Stampa su stdout
 * @note La complessità è O(n) dove n è il numero di coppie nel dizionario
 * @note Utilizza strcmp quindi il confronto è case-sensitive e dipende dalla locale
 * @note Chiama print_object con pieces_lenght=0 (comportamento discutibile
 *       per dati binari)
 * @note Non fornisce informazioni sul tipo del valore trovato
 */
void find_by_key(b_dict *dict, char *key) {
    dict_node *tmp = dict->dict;

    while (tmp != NULL) {
        if (strcmp(key, tmp->key->object->int_str->decoded_element) == 0) {
            printf("FOUND: ");
            print_object(tmp->value, 0);
            return;
        } else {
            tmp = tmp->next;
        }
    }
    printf("NOT FOUND!\n");
}
