#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <openssl/sha.h>

#include "bencode.h"
#include "structs.h"

/* ============================================================================
 * DEBUG: Codici ANSI per output colorato nel terminale
 * ============================================================================
 */



/* ============================================================================
 * VARIABILE GLOBALE: stato del parsing per il campo "pieces"
 * ============================================================================
 *
 * Questa variabile globale controlla se la prossima stringa decodificata
 * deve essere trattata come dati binari esadecimali oppure come stringa ASCII.
 *
 * Flusso di parsing nei file .torrent:
 *   1. Si incontra la stringa "pieces" come chiave nel dizionario "info"
 *   2. La funzione test_decode_string() imposta pieces = 1
 *   3. Il valore successivo viene decodificato come B_HEX (dati binari)
 *   4. La funzione test_decode_string() resetta pieces = 0
 *   5. Le stringhe successive tornano a essere decodificate come B_STR
 *
 * Variabili globali sono generalmente una cattiva pratica, ma qui vengono
 * usate per semplificare il parsing sequenziale senza passare stato tra funzioni.
 */
extern int pieces;  /* Flag: 0 = stringa normale, 1 = prossima stringa è dati binari */


/* ============================================================================
 * FUNZIONI: Determinazione del tipo bencode
 * ============================================================================
 */

/**
 * @brief Determina il tipo bencode dal primo carattere dello stream
 *
 * Esamina il primo carattere di una stringa bencode e ritorna il tipo
 * corrispondente per indirizzare il parsing al decodificatore appropriato.
 *
 * Mappatura caratteri:
 *   'i' (0x69)      → B_INT   (intero bencode)
 *   '0'-'9'         → B_STR   (lunghezza stringa)
 *   'l' (0x6C)      → B_LIS   (lista bencode)
 *   'd' (0x64)      → B_DICT  (dizionario bencode)
 *   altro           → B_NULL  (formato non riconosciuto)
 *
 * @param start Primo carattere dello stream da esaminare
 *
 * @return Il tipo B_TYPE corrispondente
 *
 * @note Implementazione: semplice catena di if/else if
 * @note Veloce: O(1) - non dipende dalla lunghezza dei dati
 * @note Non valida il resto della stringa bencode
 */
B_TYPE type_to_decode(char start) {
    if (start == 'i') {
        return B_INT;
    } else if (start >= '0' && start <= '9') {
        return B_STR;
    } else if (start == 'l') {
        return B_LIS;
    } else if (start == 'd') {
        return B_DICT;
    } else {
        return B_NULL;
    }
}


/* ============================================================================
 * FORWARD DECLARATION: prototipo per ricorsione
 * ============================================================================
 *
 * Questo prototipo è necessario perché test_decode_dict() è definito dopo
 * test_decode_list(), ma quest'ultima può chiamare test_decode_dict()
 * quando incontra un dizionario nidificato.
 */
int decode_dict(char *bencoded_dict, int idx);


/* ============================================================================
 * FUNZIONI: Helper per il parsing
 * ============================================================================
 */

/**
 * @brief Estrae un intero bencode completo dalla stringa
 *
 * Funzione di supporto che "ritaglia" un intero bencode da uno stream.
 * Dato un puntatore che inizia con 'i', scansiona in avanti finché non
 * trova 'e', alloca una stringa separata e la ritorna.
 *
 * Formato in ingresso:
 *   Esempio: "i42eblah..." → estrae "i42e" (incluso 'i' e 'e')
 *
 * Algoritmo:
 *   1. Itera sul carattere (i=0) finché non trova 'e'
 *   2. Alloca memoria: sizeof(char) * i + 1 (per i+1 caratteri)
 *   3. Copia i caratteri estratti con strncpy
 *   4. Ritorna la stringa estratta
 *
 * Gestione della memoria:
 *   - Alloca memoria per la stringa estratta
 *   - Il chiamante è responsabile di liberarla con free()
 *
 * @param bencoded_obj Puntatore a una stringa che inizia con 'i'
 *                     Esempio: "i42eblah"
 *
 * @return Stringa appena allocata contenente l'intero bencode (es. "i42e")
 *         La memoria deve essere liberata dal chiamante
 *
 * @note Non null-termina la stringa (usa strncpy che non aggiunge '\0')
 * @note Assume che esista un carattere 'e' in avanti (no bounds checking)
 * @note Non valida il contenuto, solo estrae fino a 'e'
 *
 * Complessità: O(n) dove n è la distanza fino a 'e'
 *
 * @see test_decode_integer() che usa questa funzione
 */
char* get_bencoded_int(char *bencoded_obj) {
    /* Scansiona fino al carattere 'e' di terminazione */
    int i = 0;
    while (bencoded_obj[i] != 'e') {
        i++;
    }

    /* Alloca memoria per l'intero estratto (incluso 'i' e 'e') */
    char* bencoded_int = malloc(sizeof(char) * (i + 2));  /* +1 per 'e' incluso */
    strncpy(bencoded_int, &bencoded_obj[0], i + 1);
    bencoded_int[i + 1] = '\0';


    return bencoded_int;
}


/* ============================================================================
 * FUNZIONI: Decodifica interi
 * ============================================================================
 */

/**
 * @brief Decodifica un intero bencode e ritorna la struttura completa
 *
 * Converte una stringa bencode che rappresenta un intero (i<num>e)
 * nella struttura b_obj corrispondente. Memorizza sia la forma codificata
 * che quella decodificata per debugging/verifica.
 *
 * Validazione:
 *   - Rifiuta zeri iniziali (leading zeros): bencoded_int[1]=='0' && bencoded_int[2]!='e'
 *   - Se invalido, stampa su stderr e termina con exit(1)
 *
 * Allocazione memoria:
 *   1. b_element: memorizza forme codificata/decodificata
 *   2. b_box (union): contiene il puntatore a b_element
 *   3. b_obj: wrapper con tipo B_INT
 *
 * Esempio di flusso:
 *   Input:  "i42e"
 *   Output: b_obj con:
 *     - object->int_str->encoded_element = "i42e"
 *     - object->int_str->decoded_element = "42"
 *     - object->int_str->lenght = 4
 *
 * @param bencoded_int Stringa che rappresenta un intero bencode
 *                     Deve iniziare con 'i' e terminare con 'e'
 *                     Esempio: "i42e", "i-17e", "i0e"
 *
 * @return Puntatore a b_obj di tipo B_INT contenente:
 *         - type: B_INT
 *         - object->int_str: la struttura b_element decodificata
 *
 * @note Termina il programma con exit(1) se il formato è invalido
 * @note Alloca tre strutture separate (b_element, b_box, b_obj)
 * @note La memoria deve essere liberata dal chiamante
 * @note Non usa atoll() per convertire, solo per validare
 * @note Le stringhe potrebbero non essere null-terminate in modo affidabile
 *
 * @see decode_integer() per una versione lightweight
 * @see get_bencoded_int() che è usata per estrarre l'intero
 */
b_obj* test_decode_integer(char *bencoded_int) {
    /* Alloca la struttura elemento */
    b_element *decodedInt = malloc(sizeof(b_element));
    decodedInt->lenght = strlen(bencoded_int);

    /* Validazione: rifiuta zeri iniziali (es. i042e) */
    if (bencoded_int[1] == '0' && bencoded_int[2] != 'e') {
        fprintf(stderr, "Errore, formato intero sbagliato (leading zero)! \n");
        exit(1);
    }
    /* Calcolo lunghezza del numero senza i e */
    int num_len = decodedInt->lenght - 2;
    /* Alloca buffer per la forma decodificata (escludendo 'i' e 'e') */
    char* result = malloc(sizeof(char)* (num_len + 1));

    /* Copia il contenuto escludendo 'i' iniziale e 'e' finale */
    strncpy(result, bencoded_int + 1, num_len);

    /* Null-termination */
    result[num_len] = '\0';

    /* Popola la struttura elemento */
    decodedInt->decoded_element = result;
    decodedInt->encoded_element = bencoded_int;

    /* Crea il wrapper b_obj */
    b_box *intero = malloc(sizeof(b_box));
    b_obj* integer = malloc(sizeof(b_obj));
    intero->int_str = decodedInt;
    integer->type = B_INT;
    integer->object = intero;

    return integer;
}


/**
 * @brief Decodifica un intero bencode (versione lightweight)
 *
 * Funzione leggera che decodifica un intero bencode e ritorna solo
 * la lunghezza dell'elemento codificato. Non alloca strutture dati.
 * Stampa il valore decodificato su stdout.
 *
 * Validazione:
 *   - Rifiuta zeri iniziali: bencoded_int[1]=='0' && bencoded_int[2]!='e'
 *   - Termina con exit(1) se invalido
 *
 * Algoritmo:
 *   1. Valida il formato (no leading zero)
 *   2. Alloca buffer temporaneo per la conversione
 *   3. Copia il numero escludendo 'i' e 'e'
 *   4. Converte a long long int con atoll()
 *   5. Stampa il risultato
 *   6. Ritorna la lunghezza totale
 *
 * @param bencoded_int Stringa bencode che rappresenta un intero
 *                     Esempio: "i42e"
 *
 * @return Lunghezza della stringa bencode (numero di caratteri)
 *         Esempio: per "i42e" ritorna 4
 *
 * @note Stampa il valore decodificato su stdout con printf()
 * @note Non alloca strutture dati complete (solo buffer temporaneo)
 * @note Termina il programma se il formato è invalido
 * @note Non controlla overflow di long long int
 *
 * @see test_decode_integer() per una versione che alloca strutture complete
 */
long long int decode_integer(char *bencoded_int) {
    /* Validazione: rifiuta zeri iniziali (es. i042e) */
    if (bencoded_int[1] == '0' && bencoded_int[2] != 'e') {
        fprintf(stderr, "Errore, formato intero sbagliato (leading zero)! \n");
        exit(1);
    }

    size_t bencoded_int_len = strlen(bencoded_int);

    /* Alloca buffer per il numero decodificato (senza 'i' e 'e') */
    char* result = malloc(sizeof(char) * bencoded_int_len - 2);

    /* Copia il numero escludendo 'i' iniziale e 'e' finale */
    strncpy(result, bencoded_int + 1, bencoded_int_len - 1);

    /* Converte a intero e stampa */
    unsigned long long int decoded_int = atoll(result);
    printf("%lld\n", decoded_int);

    return bencoded_int_len;
}


/* ============================================================================
 * FUNZIONI: Decodifica stringhe/bytestring
 * ============================================================================
 */

/**
 * @brief Decodifica una bytestring bencode e ritorna la struttura completa
 *
 * Converte una stringa bencode che rappresenta una bytestring (<len>:<dati>)
 * nella struttura b_obj corrispondente. Gestisce due casi:
 *   1. Stringa normale (ASCII): memorizza come b_element
 *   2. Dati binari (flag p_flag=1): memorizza come b_pieces (dati esadecimali)
 *
 * Parsing di una bytestring:
 *   Formato: "<lunghezza>:<dati>"
 *   Esempio: "4:spam" → lunghezza=4, dati="spam"
 *
 * Comportamento del flag p_flag:
 *   - p_flag=0 (stringa normale):
 *     * Decodifica come stringa ASCII/UTF-8
 *     * Stampa normalmente
 *     * Ritorna oggetto B_STR
 *
 *   - p_flag=1 (dati binari esadecimali):
 *     * Tratta i dati come byte arbitrari
 *     * Stampa rappresentazione esadecimale su stdout
 *     * Memorizza byte grezzi in hex_buffer
 *     * Ritorna oggetto B_HEX
 *     * Resetta il flag globale pieces = 0
 *
 * Caso di uso tipico (file .torrent):
 *   Durante il parsing del dizionario "info":
 *   - Incontra chiave "pieces" → imposta pieces=1
 *   - La prossima stringa viene decodificata con p_flag=1
 *   - Gli hash SHA1 (20 byte) sono memorizzati come dati binari
 *   - Dopo, pieces viene resettato a 0
 *
 * Allocazione memoria:
 *   - result: buffer per i dati decodificati
 *   - encoded_string: copia della forma codificata
 *   - hex_buffer (se p_flag=1): buffer per dati binari
 *   - b_element o b_pieces: struttura dati
 *   - b_box: union wrapper
 *   - b_obj: oggetto wrapper con tipo
 *
 * @param bencoded_string Stringa bencode che rappresenta una bytestring
 *                        Esempio: "4:spam" oppure "20:<20 byte binari>"
 * @param p_flag          Flag che specifica il tipo:
 *                        0 = stringa normale (B_STR)
 *                        1 = dati binari esadecimali (B_HEX)
 *
 * @return Puntatore a b_obj contenente:
 *         - Se p_flag=0: tipo B_STR con:
 *           * object->int_str->decoded_element: stringa decodificata
 *           * object->int_str->encoded_element: forma bencode
 *         - Se p_flag=1: tipo B_HEX con:
 *           * object->pieces->decoded_pieces: buffer byte grezzi
 *           * object->pieces->lenght: lunghezza dei byte
 *
 * @note Termina il programma con exit(-1) se lunghezza < 0
 * @note Stampa rappresentazione esadecimale su stdout per p_flag=1
 * @note Modifica la variabile globale 'pieces' se decodifica "pieces"
 * @note La memoria allocata deve essere liberata dal chiamante
 * @note Nota: il calcolo di lenght per hex può essere errato (include start_idx)
 *
 * Complessità: O(n) dove n è la lunghezza della stringa
 *
 * @see decode_string() per una versione lightweight
 */
b_obj* test_decode_string(char *bencoded_string, int p_flag) {
    /* Estrae la lunghezza della stringa dai primi caratteri (prima di ':') */
    int bencoded_string_lenght = atoi(&bencoded_string[0]);
    if (bencoded_string_lenght < 0) {
        fprintf(stderr, "Errore! Lunghezza bytestring negativa!\n");
        exit(-1);
    }

    /* Alloca buffer per i dati decodificati */
    char* result = malloc((sizeof(char) * bencoded_string_lenght) + 1); //+1 valgrind debug

    /* Trova la posizione di ':' che separa lunghezza dai dati */
    int start_idx = 0;
    while (bencoded_string[start_idx] != ':') {
        start_idx++;
    }
    start_idx += 1;  /* Salta il ':' stesso */

    /* Alloca buffer per la forma codificata */
    char* encoded_string = malloc((sizeof(char) * bencoded_string_lenght + start_idx) + 1); //+1 valgrind debug
    strncpy(encoded_string, bencoded_string, bencoded_string_lenght + start_idx);

    /* ===== CASO 1: Dati binari esadecimali (p_flag=1) ===== */
    if (p_flag) {
        int i = start_idx;

        /* Alloca buffer per i dati binari grezzi */
        unsigned char* hex_buffer = malloc(sizeof(unsigned char) * bencoded_string_lenght + start_idx);

        /* Copia i byte grezzi nel buffer */
        memcpy(hex_buffer, &bencoded_string[start_idx], bencoded_string_lenght + start_idx);

        /* Stampa i dati in formato esadecimale per debugging
        while (i < bencoded_string_lenght + start_idx) {
            printf("%02X ", (unsigned char)bencoded_string[i]);
            i++;
        }
        printf("\n");*/

        /* Crea la struttura b_pieces per memorizzare dati binari */
        b_pieces* decoded_string = malloc(sizeof(b_element));
        decoded_string->decoded_pieces = hex_buffer;
        decoded_string->lenght = bencoded_string_lenght + start_idx;
        pieces = 0;  /* Resetta il flag dopo aver processato */

        /* Crea il wrapper b_obj di tipo B_HEX */
        b_box *pic = malloc(sizeof(b_box));
        b_obj *hex = malloc(sizeof(b_obj));
        pic->pieces = decoded_string;
        hex->type = B_HEX;
        hex->object = pic;

        return hex;
    }
    /* ===== CASO 2: Stringa normale (p_flag=0) ===== */
    else {
        /* Copia i dati dal buffer codificato al buffer decodificato */
        strncpy(result, &bencoded_string[start_idx], bencoded_string_lenght);
        result[bencoded_string_lenght] = '\0';
    }

    /* Verifica se questa stringa è la chiave speciale "pieces" */
    if (strcmp(result, "pieces") == 0) {
        pieces = 1;  /* Imposta flag per decodificare il prossimo valore come binario */
    }

    /* Crea la struttura b_element per memorizzare la stringa */
    b_element* decoded_string = malloc(sizeof(b_element));
    decoded_string->decoded_element = result;
    encoded_string[bencoded_string_lenght + start_idx] = '\0';
    decoded_string->encoded_element = encoded_string;
    decoded_string->lenght = bencoded_string_lenght + start_idx;

    /* Crea il wrapper b_obj di tipo B_STR */
    b_box *str = malloc(sizeof(b_box));
    b_obj* string = malloc(sizeof(b_obj));
    str->int_str = decoded_string;
    string->type = B_STR;
    string->object = str;

    return string;
}


/**
 * @brief Decodifica una bytestring bencode (versione lightweight)
 *
 * Funzione leggera che decodifica una bytestring bencode (<len>:<dati>)
 * e ritorna solo la lunghezza dell'elemento codificato.
 * Non alloca strutture dati complete.
 *
 * Parsing:
 *   1. Estrae la lunghezza dai caratteri iniziali
 *   2. Trova la posizione di ':'
 *   3. Se p_flag=1, stampa dati binari in formato esadecimale
 *   4. Se p_flag=0, decodifica come stringa ASCII
 *   5. Verifica se la stringa è "pieces" per impostare il flag globale
 *
 * Debug output:
 *   Se pieces globale è 1, stampa su stdout con colore verde (ANSI)
 *   Formato: "DEBUG PIECES LENGHT == <lenght>"
 *
 * @param bencoded_string Stringa bencode che rappresenta una bytestring
 *                        Esempio: "4:spam" oppure "20:<dati binari>"
 * @param p_flag          Flag che specifica il tipo:
 *                        0 = stringa normale
 *                        1 = dati binari esadecimali
 *
 * @return Lunghezza della forma codificata
 *         Formula: bencoded_string_lenght + start_idx
 *         Dove start_idx è la posizione dopo ':'
 *
 * @note Termina il programma con exit(-1) se lunghezza < 0
 * @note Stampa output di debug se il flag globale 'pieces' è 1
 * @note Stampa dati esadecimali su stdout se p_flag=1
 * @note Modifica il flag globale 'pieces'
 * @note Alloca solo buffer temporanei
 *
 * @see test_decode_string() per una versione che alloca strutture complete
 */
int decode_string(char *bencoded_string, int p_flag) {
    /* Estrae la lunghezza della stringa */
    int bencoded_string_lenght = atoi(&bencoded_string[0]);
    if (bencoded_string_lenght < 0) {
        fprintf(stderr, "Errore! Lunghezza bytestring negativa!\n");
        exit(-1);
    }

    /* Debug: stampa il messaggio se stiamo processando il campo "pieces" */
    if (pieces) {
        printf(ANSI_COLOR_GREEN "DEBUG PIECES LENGHT == %d\n" ANSI_COLOR_RESET, bencoded_string_lenght);
    }

    /* Alloca buffer temporaneo per la stringa decodificata */
    char* result = malloc(sizeof(char) * bencoded_string_lenght);

    /* Trova la posizione di ':' */
    int start_idx = 0;
    while (bencoded_string[start_idx] != ':') {
        start_idx++;
    }
    start_idx += 1;  /* Salta ':' */

    /* ===== CASO 1: Dati binari esadecimali ===== */
    if (p_flag) {
        int i = start_idx;

        /* Alloca buffer per dati binari grezzi */
        unsigned char* hex_buffer = malloc(sizeof(unsigned char) * bencoded_string_lenght + start_idx);

        /* Copia i byte grezzi */
        memcpy(hex_buffer, &bencoded_string[start_idx], bencoded_string_lenght + start_idx);

        /* Stampa i dati in formato esadecimale */
        while (i < bencoded_string_lenght + start_idx) {
            printf("%02X ", (unsigned char)bencoded_string[i]);
            i++;
        }
        printf("\n");

        pieces = 0;  /* Resetta il flag dopo il processamento */
    }
    /* ===== CASO 2: Stringa normale ===== */
    else {
        /* Copia i dati dalla forma codificata */
        strncpy(result, &bencoded_string[start_idx], bencoded_string_lenght);
        result[bencoded_string_lenght] = '\0';
    }

    /* Verifica se questa è la chiave speciale "pieces" */
    if (strcmp(result, "pieces") == 0) {
        pieces = 1;  /* Imposta flag per il prossimo valore */
    }

    /* Ritorna la lunghezza totale dell'elemento codificato */
    return bencoded_string_lenght + start_idx;
}


/* ============================================================================
 * FUNZIONI: Decodifica liste (ricorsiva)
 * ============================================================================
 */

/**
 * @brief Decodifica una lista bencode con allocazione di memoria (ricorsiva)
 *
 * Converte una stringa bencode che rappresenta una lista (l<elementi>e)
 * in una struttura b_list con una lista concatenata di elementi.
 * Supporta annidamento ricorsivo di liste, dizionari, interi e stringhe.
 *
 * Formato di input:
 *   - Inizia con 'l'
 *   - Contiene zero o più elementi bencodificati
 *   - Termina con 'e'
 *   - Esempi: "le" (vuota), "li1ei2ee" ([1, 2]), "l4:spamee" (["spam"])
 *
 * Algoritmo:
 *   1. Stampa "INIZIO LISTA" per debug
 *   2. Inizializza una lista vuota con list_init()
 *   3. Itera da idx=1 finché non trova 'e'
 *   4. Per ogni elemento:
 *      - Determina il tipo con type_to_decode()
 *      - Chiama il decodificatore appropriato
 *      - Aggiunge l'elemento con list_add()
 *      - Avanza l'indice di idx
 *   5. Copia la forma codificata
 *   6. Stampa il contenuto con print_list()
 *   7. Ritorna l'oggetto wrapper b_obj
 *
 * Ricorsione:
 *   - Se incontra 'l' (sottolista): chiama ricorsivamente test_decode_list()
 *   - Se incontra 'd' (dizionario): chiama test_decode_dict()
 *   - I risultati sono aggiunti alla lista principale
 *
 * Allocazione memoria:
 *   1. b_list: struttura della lista
 *   2. Per ogni elemento: b_obj allocato dai decodificatori
 *   3. b_box: union wrapper
 *   4. b_obj: oggetto wrapper finale
 *
 * Bug noto:
 *   - La riga "printf("\t\tFINE LISTA\n");" dopo il return
 *     NON è mai eseguita (unreachable code)
 *
 * @param bencoded_list Stringa bencode che rappresenta una lista
 *                      Esempio: "li1ei2ee" (incluso 'l' e 'e')
 * @param start         Indice di inizio nel buffer (parametro ignorato)
 *                      Generalmente 0 per la prima chiamata
 *
 * @return Puntatore a b_obj di tipo B_LIS contenente:
 *         - type: B_LIS
 *         - object->list: la struttura b_list con la lista concatenata
 *         - object->list->lenght: lunghezza della forma codificata
 *         - object->list->enocded_list: copia della forma codificata
 *
 * @note Stampa "INIZIO LISTA" all'inizio e il contenuto con print_list()
 * @note Stampa "FINE LISTA" non è raggiungibile (bug)
 * @note Termina il programma con exit(-1) se incontra tipo B_NULL
 * @note La memoria deve essere liberata dal chiamante
 * @note Il parametro start non è usato nella implementazione attuale
 * @note Nota: il typo nel nome del campo enocded_list
 *
 * Complessità: O(n) dove n è il numero di elementi nella lista
 *
 * Esempio di uso:
 *   b_obj *list = test_decode_list("li1ei2ee", 0);
 *   // Stampa: INIZIO LISTA \n 1 \n 2 \n FINE LISTA
 *
 * @see test_decode_dict() per dizionari
 */
b_obj* test_decode_list(char *bencoded_list, int start) {
    printf("\n\t\tINIZIO LISTA\n");

    /* Inizializza una nuova lista vuota */
    b_list *lista = list_init();

    /* Itera attraverso gli elementi della lista (da idx=1 fino a 'e') */
    int idx = 1;
    while (bencoded_list[idx] != 'e') {
        /* Determina il tipo dell'elemento corrente */
        switch (type_to_decode(bencoded_list[idx])) {
            /* ===== ELEMENTO INTERO ===== */
            case B_INT: {
                char* bencoded_int = get_bencoded_int(&bencoded_list[idx]);
                b_obj* decodedInt = test_decode_integer(bencoded_int);
                list_add(lista, decodedInt);
                idx += decodedInt->object->int_str->lenght;
                break;
            }

            /* ===== ELEMENTO STRINGA ===== */
            case B_STR:
                if (pieces) {
                    /* Prossima stringa è dati binari (campo "pieces") */
                    b_obj* decodedPieces = test_decode_string(&bencoded_list[idx], pieces);
                    list_add(lista, decodedPieces);
                    idx += decodedPieces->object->pieces->lenght;
                } else {
                    /* Stringa normale */
                    b_obj *decodedString = test_decode_string(&bencoded_list[idx], pieces);
                    list_add(lista, decodedString);
                    idx += decodedString->object->int_str->lenght;
                }
                break;

            /* ===== SOTTOLISTA (ricorsione) ===== */
            case B_LIS: {
                b_obj *decodedList = test_decode_list(&bencoded_list[idx], idx);
                list_add(lista, decodedList);
                idx += decodedList->object->list->lenght;
                break;
            }

            /* ===== SOTTODIZIONARIO (ricorsione) ===== */
            case B_DICT: {
                b_obj *decodedDict = test_decode_dict(&bencoded_list[idx], idx);
                list_add(lista, decodedDict);
                idx += decodedDict->object->dict->lenght;
                break;
            }

            /* ===== TIPO NON RICONOSCIUTO ===== */
            case B_NULL:
                fprintf(stderr, "Formato non riconosciuto in decode_list (B_NULL), carattere incriminato: '%c'\n",
                        bencoded_list[idx]);
                exit(-1);
        }
    }

    /* Imposta la lunghezza totale della lista codificata */
    lista->lenght = idx + 1;

    /* Alloca e copia la forma codificata */
    b_box* list = malloc(sizeof(b_box));
    b_obj* return_list = malloc(sizeof(b_obj)); //era sizeof(b_box) prima, cambiato per valgrind debug

    char* encoded = malloc(sizeof(char) * idx + 2);
    strncpy(encoded, bencoded_list, idx + 1);

    /* Popola il wrapper */
    list->list = lista;
    lista->enocded_list = encoded;
    return_list->type = B_LIS;
    return_list->object = list; //invalid write of 8 caused from malloc in 665

    /* Stampa il contenuto della lista per debugging */
    print_list(lista);

    return return_list;

    /* BUG: Questa riga non è mai raggiunta (dead code) */
    printf("\t\tFINE LISTA\n");
}


/* ============================================================================
 * FUNZIONI: Decodifica dizionari (ricorsiva)
 * ============================================================================
 */

/**
 * @brief Decodifica un dizionario bencode con allocazione di memoria (ricorsiva)
 *
 * Converte una stringa bencode che rappresenta un dizionario (d<coppie>e)
 * in una struttura b_dict con una lista concatenata di coppie chiave-valore.
 * Supporta annidamento ricorsivo e valori eterogenei (chiavi sempre stringhe).
 *
 * Formato di input:
 *   - Inizia con 'd'
 *   - Contiene zero o più coppie (chiave, valore)
 *   - Chiave: sempre una bytestring
 *   - Valore: tipo vario (intero, stringa, lista, dizionario)
 *   - Termina con 'e'
 *   - Esempi: "de" (vuoto), "d3:key5:valuee" ({"key": "value"})
 *
 * Ordinamento:
 *   - In bencode valido, le chiavi dovrebbero essere ordinate lessicograficamente
 *   - Questa implementazione NON garantisce l'ordinamento
 *   - Gli elementi vengono inseriti nell'ordine di parsing
 *
 * Algoritmo:
 *   1. Stampa "INIZIO DICT" per debug
 *   2. Inizializza un dizionario vuoto con dict_init()
 *   3. Itera da idx=1 finché non trova 'e'
 *   4. Per ogni coppia chiave-valore:
 *      - Stampa "KEY = "
 *      - Decodifica la chiave (sempre stringa)
 *      - Stampa "VALUE = "
 *      - Determina il tipo del valore con type_to_decode()
 *      - Chiama il decodificatore appropriato per il valore
 *      - Aggiunge la coppia con dict_add()
 *   5. Stampa il contenuto con print_dict()
 *   6. Alloca e copia la forma codificata
 *   7. Ritorna l'oggetto wrapper b_obj
 *
 * Ricorsione:
 *   - Se il valore è 'l' (lista): chiama ricorsivamente test_decode_list()
 *   - Se il valore è 'd' (dizionario): chiama ricorsivamente test_decode_dict()
 *   - Permette strutture arbitrariamente nidificate (es. metafile .torrent)
 *
 * Allocazione memoria:
 *   1. b_dict: struttura del dizionario
 *   2. Per ogni coppia: chiave e valore b_obj dai decodificatori
 *   3. b_box: union wrapper
 *   4. b_obj: oggetto wrapper finale
 *
 * Caso di uso tipico (file .torrent):
 *   Struttura metafile .torrent:
 *   {
 *     "announce": "http://tracker.example.com:6969/announce",
 *     "info": {
 *       "name": "example.txt",
 *       "length": 1024,
 *       "pieces": <binary data>
 *     }
 *   }
 *
 * @param bencoded_dict Stringa bencode che rappresenta un dizionario
 *                      Esempio: "d3:key5:valuee" (incluso 'd' e 'e')
 * @param start         Indice di inizio nel buffer (parametro ignorato)
 *                      Generalmente 0 per la prima chiamata
 *
 * @return Puntatore a b_obj di tipo B_DICT contenente:
 *         - type: B_DICT
 *         - object->dict: la struttura b_dict con le coppie chiave-valore
 *         - object->dict->lenght: lunghezza della forma codificata
 *         - object->dict->encoded_dict: copia della forma codificata
 *
 * @note Stampa "INIZIO DICT", "KEY = ", "VALUE = ", "FINE DICT" per debug
 * @note Stampa il contenuto con print_dict()
 * @note Termina il programma con exit(-1) se incontra tipo B_NULL nel valore
 * @note La memoria deve essere liberata dal chiamante
 * @note Il parametro start non è usato nella implementazione attuale
 * @note Non garantisce che le chiavi rimangono ordinate lessicograficamente
 * @note Nota: il typo nel nome del campo encoded_dict (corretto rispetto a liste)
 * @note Nota: commentario su lenght contiene "//?" (incertezza dei programmatori)
 *
 * Complessità: O(n) dove n è il numero di coppie nel dizionario
 *
 * Esempio di uso:
 *   b_obj *torrent = test_decode_dict("d8:announce<...>4:infod<...>ee", 0);
 *   // Stampa la struttura dell'intero file torrent
 *
 * @see test_decode_list() per liste
 */
b_obj* test_decode_dict(char *bencoded_dict, int start) {
    printf("\n\t\tINIZIO DICT\n");

    /* Inizializza un nuovo dizionario vuoto */
    b_dict* dizio = dict_init();

    /* Variabile temporanea per la chiave */
    b_obj* key = malloc(sizeof(b_obj));

    /* Itera attraverso le coppie chiave-valore (da idx=1 fino a 'e') */
    int idx = 1;
    while (bencoded_dict[idx] != 'e') {
        /* ===== DECODIFICA DELLA CHIAVE (sempre stringa) ===== */
        printf("\nKEY = ");
        key = test_decode_string(&bencoded_dict[idx], pieces);
        idx += key->object->int_str->lenght;

        /* ===== DECODIFICA DEL VALORE (tipo vario) ===== */
        printf("VALUE = ");

        switch (type_to_decode(bencoded_dict[idx])) {
            /* ===== VALORE INTERO ===== */
            case B_INT: {
                char* bencoded_int = get_bencoded_int(&bencoded_dict[idx]);
                b_obj *decodedInt = test_decode_integer(bencoded_int);
                dict_add(dizio, key, decodedInt);
                idx += decodedInt->object->int_str->lenght;
                break;
            }

            /* ===== VALORE STRINGA ===== */
            case B_STR:
                if (pieces) {
                    /* Prossima stringa è dati binari (campo "pieces") */
                    b_obj *decodedPieces = test_decode_string(&bencoded_dict[idx], pieces);
                    dict_add(dizio, key, decodedPieces);
                    idx += decodedPieces->object->pieces->lenght;
                } else {
                    /* Stringa normale */
                    b_obj *decodedString = test_decode_string(&bencoded_dict[idx], pieces);
                    dict_add(dizio, key, decodedString);
                    idx += decodedString->object->int_str->lenght;
                }
                break;

            /* ===== VALORE LISTA (ricorsione) ===== */
            case B_LIS: {
                b_obj *decodedList = test_decode_list(&bencoded_dict[idx], idx);
                dict_add(dizio, key, decodedList);
                idx += decodedList->object->list->lenght;
                break;
            }

            /* ===== VALORE DIZIONARIO (ricorsione) ===== */
            case B_DICT: {
                b_obj *decodedDict = test_decode_dict(&bencoded_dict[idx], idx);
                dict_add(dizio, key, decodedDict);
                idx += decodedDict->object->dict->lenght;
                break;
            }

            /* ===== TIPO NON RICONOSCIUTO ===== */
            case B_NULL:
                fprintf(stderr, "Formato non riconosciuto in decode_list (B_NULL), carattere incriminato: '%c'\n",
                        bencoded_dict[idx]);
                exit(-1);
        }
    }

    /* Stampa il contenuto del dizionario per debugging */
    print_dict(dizio);
    printf("\t\tFINE DICT\n");

    /* Alloca il wrapper b_box e b_obj */
    b_box* dict = malloc(sizeof(b_box));
    b_obj *return_dict = malloc(sizeof(b_obj));

    /* Alloca e copia la forma codificata */
    char* encoded = malloc(sizeof(char) * idx + 2);
    memcpy(encoded, bencoded_dict, idx + 1);

    /* Popola il wrapper */
    dict->dict = dizio;
    dizio->encoded_dict = encoded;

    return_dict->type = B_DICT;
    return_dict->object = dict;
    return_dict->object->dict->lenght = idx + 1;  /* //? - Incertezza del programmatore */

    return return_dict;
}


/* ============================================================================
 * FUNZIONI: Utilità BitTorrent
 * ============================================================================
 */

/**
 * @brief Genera un ID peer (Peer ID) per il protocollo BitTorrent
 *
 * Crea un identificativo peer di 20 byte secondo le specifiche BitTorrent.
 * Il formato è:
 *   - 8 byte: prefisso identificativo "-GS0001-" (client ID e versione)
 *   - 12 byte: primi 12 byte di SHA1(peer_key)
 *
 * Specifiche BitTorrent:
 *   - Peer ID deve essere esattamente 20 byte
 *   - Viene scambiato nel protocollo per identificare il client
 *   - Comunemente: "-<client><version>-<random>"
 *   - Esempio reale: "-TR2710-" + 12 byte casuali (Transmission)
 *
 * Algoritmo:
 *   1. Definisce il prefisso di 8 byte "-GS0001-"
 *   2. Calcola la lunghezza della stringa peer_key
 *   3. Calcola SHA1(peer_key) usando OpenSSL
 *   4. Copia i primi 8 byte del prefisso nel peer_id
 *   5. Copia i 12 byte della firma SHA1 nel peer_id
 *   6. Risultato: 20 byte totali
 *
 * Requisiti di memoria:
 *   - peer_id deve essere allocato con almeno 20 byte
 *   - SHA_DIGEST_LENGTH (da OpenSSL) è generalmente 20 byte
 *   - Copia solo 12 byte dell'hash, non l'intero
 *
 * @param peer_key Stringa usata come seed per l'hash SHA1
 *                 Esempi: "hostname_timestamp_random", "client_v1.0"
 *                 Deve essere null-terminated
 * @param peer_id  Puntatore al buffer dove memorizzare il Peer ID
 *                 DEVE essere allocato con almeno 20 byte
 *                 DEVE essere allocato dal chiamante
 *
 * @return void (memorizza il risultato nel buffer peer_id)
 *
 * @note Il buffer peer_id NON è null-terminated (è binario)
 * @note Richiede OpenSSL: incluire <openssl/sha.h> e linkare -lssl -lcrypto
 * @note Il parametro peer_key deve essere una stringa valida
 * @note SHA1 produce 20 byte, ma solo i 12 ultimi sono usati
 *       (il prefisso occupa 8 byte, totale 20)
 * @note La funzione non valida i parametri (potrebbe crash se NULL)
 *
 * Caso di uso tipico:
 *   unsigned char peer_id[20];
 *   generate_peer_id("my_client_v1.0", peer_id);
 *   // peer_id contiene 20 byte univoci
 *
 * Sicurezza:
 *   - SHA1 è cryptograficamente debole ma sufficiente per questo uso
 *   - peer_key dovrebbe contenere casualità (timestamp, random)
 *   - Due invocazioni con lo stesso peer_key produrranno lo stesso peer_id
 *
 * Complessità: O(n) dove n è la lunghezza di peer_key (SHA1)
 */
void generate_peer_id(char *peer_key, unsigned char *peer_id) {
    /* Prefisso identificativo: "-GS0001-" (8 byte) */
    const char* prefix = "-GS0001-";

    /* Calcola la lunghezza della stringa peer_key */
    int n = strlen(peer_key);

    /* Buffer per memorizzare l'hash SHA1 (20 byte) */
    unsigned char hash[SHA_DIGEST_LENGTH];

    /* Calcola SHA1(peer_key) */
    SHA1((unsigned char*)peer_key, n, hash);

    /* Copia il prefisso (8 byte) nei primi 8 byte del peer_id */
    memcpy(peer_id, prefix, 8);

    /* Copia i 12 byte della firma SHA1 dopo il prefisso */
    memcpy(peer_id + 8, hash, 12);

    /* Risultato: 20 byte totali (8 + 12) */
}
