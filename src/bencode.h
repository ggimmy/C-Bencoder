#ifndef BENCODE_H
#define BENCODE_H

#include "structs.h"

/* ============================================================================
 * PANORAMICA: Modulo di decodifica Bencode
 * ============================================================================
 *
 * Questo modulo implementa le funzioni per decodificare il formato Bencode,
 * uno schema di serializzazione semplice usato principalmente in applicazioni
 * peer-to-peer come BitTorrent.
 *
 * Formato Bencode:
 * ================
 * - Intero:       i<numero>e              Esempio: i42e → 42
 * - Stringa:      <lunghezza>:<dati>     Esempio: 4:spam → "spam"
 * - Lista:        l<elementi>e            Esempio: li1ei2ee → [1, 2]
 * - Dizionario:   d<coppie>e             Esempio: d3:key5:valuee → {"key": "value"}
 *
 * Le stringhe in bencode sono "bytestring" (array di byte) e possono contenere
 * dati binari arbitrari. Questo è importante per campi come "pieces" nei file .torrent.
 *
 * ============================================================================
 */


/* ============================================================================
 * FUNZIONI: Determinazione del tipo (type detection)
 * ============================================================================
 */

/**
 * @brief Determina il tipo bencode da decodificare basandosi sul primo carattere
 *
 * Esamina il primo carattere di una stringa bencode e restituisce il tipo
 * corrispondente. Questa funzione è usata come dispatcher per indirizzare
 * il parsing al decodificatore appropriato.
 *
 * Mappatura:
 *   - 'i' → B_INT    (intero: i<num>e)
 *   - '0'-'9' → B_STR (stringa: <len>:<dati>)
 *   - 'l' → B_LIS    (lista: l<elementi>e)
 *   - 'd' → B_DICT   (dizionario: d<coppie>e)
 *   - altro → B_NULL (tipo sconosciuto/invalido)
 *
 * @param start Primo carattere dello stream bencode
 *
 * @return Il tipo B_TYPE corrispondente al carattere
 *         - B_INT, B_STR, B_LIS, B_DICT per formati validi
 *         - B_NULL per caratteri non riconosciuti
 *
 * @note Non valida il resto della stringa, solo il primo carattere
 * @note È una funzione di lookup semplice e veloce
 *
 * Esempio di uso:
 *   B_TYPE type = type_to_decode('i');  // Ritorna B_INT
 *   type = type_to_decode('4');         // Ritorna B_STR
 *   type = type_to_decode('x');         // Ritorna B_NULL
 */
B_TYPE type_to_decode(char start);


/* ============================================================================
 * FUNZIONI: Helper e utility per parsing
 * ============================================================================
 */

/**
 * @brief Estrae un intero bencode completo dalla stringa
 *
 * Funzione di supporto che "ritaglia" un intero bencode da uno stream.
 * Dato un puntatore a una stringa che inizia con 'i', estrae l'intero completo
 * fino al 'e' di terminazione e lo restituisce come stringa separata.
 *
 * Algoritmo:
 *   1. Scansiona in avanti finché non trova 'e'
 *   2. Alloca memoria per la stringa estratta (i + e inclusi)
 *   3. Copia i caratteri estratti
 *   4. Ritorna la stringa allocata
 *
 * Esempio:
 *   Input:  "i42eblah" (puntatore all'inizio)
 *   Output: "i42e" (stringa appena allocata)
 *
 * @param bencoded_obj Puntatore a una stringa che inizia con 'i'
 *
 * @return Stringa appena allocata contenente l'intero bencode completo (i...e)
 *         La memoria deve essere liberata dal chiamante
 *
 * @note Alloca memoria: la memoria ritornata deve essere freed dal chiamante
 * @note Assume che il carattere 'e' esista (non controlla bounds)
 * @note Non valida il contenuto, solo lo estrae fino a 'e'
 *
 * @see test_decode_integer() che usa questa funzione
 */
char* get_bencoded_int(char *bencoded_obj);


/* ============================================================================
 * FUNZIONI: Decodifica con allocazione di memoria (test_decode_*)
 * ============================================================================
 *
 * Queste funzioni decodificano elementi singoli e allocano strutture dati
 * complete per memorizzarli. Il prefisso "test_" indica che mantengono sia la
 * forma codificata che quella decodificata (per debugging/verifica).
 *
 */

/**
 * @brief Decodifica un intero in formato bencode
 *
 * Converte una stringa bencode che rappresenta un intero (formato i<num>e)
 * nella struttura b_obj corrispondente. Alloca memoria per memorizzare
 * sia la forma codificata che quella decodificata.
 *
 * Formato di input:
 *   - Deve iniziare con 'i'
 *   - Deve terminare con 'e'
 *   - Contiene cifre decimali (opzionalmente con '-' per negativi)
 *   - Esempi: "i42e", "i-17e", "i0e"
 *
 * Validazione:
 *   - Rifiuta zeri iniziali (leading zeros): i042e è un errore
 *   - Se la validazione fallisce, stampa errore e termina con exit(1)
 *
 * Allocazione memoria:
 *   - Alloca b_element per memorizzare le forme codificata/decodificata
 *   - Alloca b_box (union) per contenere il puntatore
 *   - Alloca b_obj per il wrapper finale
 *   - La memoria deve essere liberata dal chiamante
 *
 * @param bencoded_int Stringa bencode che rappresenta un intero
 *                     Esempio: "i42e" (incluso il 'i' iniziale e 'e' finale)
 *
 * @return Puntatore a b_obj contenente l'intero decodificato con tipo B_INT
 *         I campi sono:
 *         - object->int_str->encoded_element: forma bencode ("i42e")
 *         - object->int_str->decoded_element: forma leggibile ("42")
 *         - object->int_str->length: lunghezza della forma codificata
 *
 * @note Termina il programma con exit(1) se il formato è invalido (es. leading zero)
 * @note Non controlla se il numero rientra nel range di long long int
 * @note La memoria allocata per le stringhe potrebbe non essere null-terminated
 *
 * Esempio di uso:
 *   b_obj *num = test_decode_integer("i42e");
 *   printf("%s\n", num->object->int_str->decoded_element); // Stampa: 42
 *
 * @see decode_integer() per una versione lightweight che non memorizza la forma
 */
b_obj* test_decode_integer(char *bencoded_int);


/**
 * @brief Decodifica una bytestring (stringa) in formato bencode
 *
 * Converte una stringa bencode che rappresenta una bytestring (formato <len>:<dati>)
 * nella struttura b_obj corrispondente. Alloca memoria e opzionalmente gestisce
 * il caso speciale di dati binari esadecimali (come il campo "pieces" nei .torrent).
 *
 * Formato di input:
 *   - Inizia con la lunghezza in decimale
 *   - Seguita da ':'
 *   - Seguita da esattamente <lunghezza> byte di dati
 *   - Esempi: "4:spam", "20:<20 byte di dati binari>"
 *
 * Comportamento del flag p_flag:
 *   - Se p_flag == 0 (stringa normale):
 *     * Decodifica come stringa ASCII/UTF-8 leggibile
 *     * Memorizza forma codificata e decodificata
 *     * Ritorna oggetto di tipo B_STR
 *
 *   - Se p_flag == 1 (dati binari esadecimali):
 *     * Interpreteaz i dati come bytes binari
 *     * Stampa una rappresentazione esadecimale su stdout
 *     * Memorizza i byte grezzi in un buffer
 *     * Ritorna oggetto di tipo B_HEX
 *     * Resetta il flag pieces globale a 0
 *
 * Allocazione memoria:
 *   - Alloca buffer per i dati decodificati
 *   - Alloca b_element (per B_STR) o b_pieces (per B_HEX)
 *   - Alloca b_box e b_obj wrapper
 *   - La memoria deve essere liberata dal chiamante
 *
 * @param bencoded_string Stringa bencode che rappresenta una bytestring
 *                        Esempio: "4:spam" (incluso la lunghezza e ':')
 * @param p_flag          Flag che indica il tipo di dati:
 *                        0 = stringa normale (B_STR)
 *                        1 = dati binari esadecimali (B_HEX)
 *
 * @return Per p_flag == 0: b_obj di tipo B_STR contenente:
 *         - object->int_str->decoded_element: stringa decodificata ("spam")
 *         - object->int_str->encoded_element: forma bencode ("4:spam")
 *
 *         Per p_flag == 1: b_obj di tipo B_HEX contenente:
 *         - object->pieces->decoded_pieces: buffer dei byte grezzi
 *         - object->pieces->length: lunghezza dei dati
 *
 * @note Stampa "Errore! Lunghezza bytestring negativa!" e termina se lunghezza < 0
 * @note Stampa rappresentazione esadecimale su stdout per p_flag == 1
 * @note La memoria allocata può non essere null-terminated per B_HEX
 * @note Modifica la variabile globale 'pieces' se la stringa contiene "pieces"
 *
 * Caso di uso tipico (file .torrent):
 *   1. Durante il parsing, quando si incontra la chiave "pieces"
 *   2. Si imposta il flag globale pieces = 1
 *   3. La prossima stringa viene decodificata come B_HEX
 *   4. Gli hash SHA1 sono memorizzati come byte binari
 *
 * @see decode_string() per una versione che solo ritorna la lunghezza
 */
b_obj* test_decode_string(char *bencoded_string, int p_flag);


/**
 * @brief Decodifica una lista in formato bencode con allocazione di memoria
 *
 * Converte una stringa bencode che rappresenta una lista (formato l<elementi>e)
 * in una struttura b_list contenente una lista concatenata di elementi.
 * Supporta annidamento ricorsivo di liste, dizionari, interi e stringhe.
 *
 * Formato di input:
 *   - Deve iniziare con 'l'
 *   - Contiene zero o più elementi bencodificati
 *   - Deve terminare con 'e'
 *   - Esempi: "le" (lista vuota), "li1ei2ee" ([1, 2]), "l4:spamee" (["spam"])
 *
 * Allocazione memoria:
 *   - Inizializza una nuova lista con list_init()
 *   - Per ogni elemento, chiama il decodificatore appropriato
 *   - Aggiunge l'elemento decodificato alla lista con list_add()
 *   - Alloca e copia la forma codificata
 *   - Stampa il contenuto della lista
 *   - Alloca b_box e b_obj wrapper
 *
 * Ricorsione:
 *   - Se incontra una sottolista, chiama ricorsivamente test_decode_list
 *   - Se incontra un sottodizionario, chiama ricorsivamente test_decode_dict
 *   - Permette strutture arbitrariamente nidificate
 *
 * @param bencoded_list Stringa bencode che rappresenta una lista
 *                      Esempio: "li1ei2ee" (incluso 'l' iniziale e 'e' finale)
 * @param start         Indice di inizio nel buffer (per ricorsione)
 *                      Solitamente 0 per la prima chiamata
 *
 * @return Puntatore a b_obj di tipo B_LIS contenente:
 *         - object->list->list: puntatore al primo nodo della lista concatenata
 *         - object->list->length: lunghezza della forma codificata
 *         - object->list->encoded_list: copia della forma codificata
 *
 * @note Stampa su stdout: "INIZIO LISTA" al start e "FINE LISTA" al termine
 * @note Stampa il contenuto della lista con print_list() prima di ritornare
 * @note Termina il programma con exit(-1) se incontra un tipo B_NULL
 * @note Il parametro start è ignorato nella implementazione attuale
 *
 * Esempio di uso:
 *   b_obj *list = test_decode_list("li1ei2ee", 0);
 *   // Stampa: INIZIO LISTA \n 1 \n 2 \n FINE LISTA
 *
 * @see test_decode_dict() per la decodifica di dizionari
 */
b_obj* test_decode_list(char *bencoded_list, int start);


/**
 * @brief Decodifica un dizionario in formato bencode con allocazione di memoria
 *
 * Converte una stringa bencode che rappresenta un dizionario (formato d<coppie>e)
 * in una struttura b_dict contenente una lista concatenata di coppie chiave-valore.
 * Supporta annidamento ricorsivo e coppie eterogenee (chiavi sempre stringhe).
 *
 * Formato di input:
 *   - Deve iniziare con 'd'
 *   - Contiene zero o più coppie alternando chiave (sempre stringa) e valore
 *   - Deve terminare con 'e'
 *   - Esempi: "de" (dict vuoto), "d3:key5:valuee" ({"key": "value"})
 *
 * Ordinamento:
 *   - In bencode valido, le chiavi dovrebbero essere ordinate lessicograficamente
 *   - Questa implementazione NON garantisce l'ordinamento
 *
 * Allocazione memoria:
 *   - Inizializza un nuovo dizionario con dict_init()
 *   - Per ogni coppia chiave-valore:
 *     * Decodifica la chiave (sempre stringa)
 *     * Decodifica il valore (tipo vario)
 *     * Aggiunge la coppia con dict_add()
 *   - Alloca e copia la forma codificata
 *   - Stampa il contenuto del dizionario
 *   - Alloca b_box e b_obj wrapper
 *
 * Ricorsione:
 *   - Se il valore è una sottolista, chiama ricorsivamente test_decode_list
 *   - Se il valore è un sottodizionario, chiama ricorsivamente test_decode_dict
 *   - Permette strutture arbitrariamente nidificate
 *
 * @param bencoded_dict Stringa bencode che rappresenta un dizionario
 *                      Esempio: "d3:key5:valuee" (incluso 'd' iniziale e 'e' finale)
 * @param start         Indice di inizio nel buffer (per ricorsione)
 *                      Solitamente 0 per la prima chiamata
 *
 * @return Puntatore a b_obj di tipo B_DICT contenente:
 *         - object->dict->dict: puntatore al primo nodo del dizionario
 *         - object->dict->length: lunghezza della forma codificata
 *         - object->dict->encoded_dict: copia della forma codificata
 *
 * @note Stampa su stdout: "INIZIO DICT" al start, "KEY = " prima della chiave,
 *       "VALUE = " prima del valore, e "FINE DICT" al termine
 * @note Stampa il contenuto del dizionario con print_dict() prima di ritornare
 * @note Termina il programma con exit(-1) se incontra un tipo B_NULL
 * @note Il parametro start è ignorato nella implementazione attuale
 *
 * Caso di uso tipico (file .torrent):
 *   b_obj *torrent = test_decode_dict("d8:announce<...>4:infod<...>ee", 0);
 *   // Ritorna la struttura che rappresenta l'intero file torrent
 *
 * @see test_decode_list() per la decodifica di liste
 */
b_obj* test_decode_dict(char *bencoded_dict, int start);


/* ============================================================================
 * FUNZIONI: Decodifica lightweight (decode_* senza allocazione di memoria)
 * ============================================================================
 *
 * Queste funzioni decodificano elementi e ritornano solo la lunghezza
 * dell'elemento codificato, senza allocare strutture complesse.
 * Utili per il parsing efficiente quando solo la forma decodificata è importante.
 *
 */

/**
 * @brief Decodifica un intero bencode e ritorna la lunghezza (senza alloc)
 *
 * Funzione leggera che decodifica un intero bencode (formato i<num>e) e
 * ritorna solo la lunghezza totale dell'elemento codificato.
 * Non alloca strutture dati, solo stampa il risultato su stdout.
 *
 * Validazione:
 *   - Rifiuta zeri iniziali (leading zeros): i042e è un errore
 *   - Se la validazione fallisce, stampa errore e termina con exit(1)
 *
 * @param bencoded_int Stringa bencode che rappresenta un intero
 *                     Esempio: "i42e"
 *
 * @return Lunghezza della stringa bencode (numero di caratteri da i a e inclusi)
 *
 * @note Stampa il valore decodificato su stdout (con printf)
 * @note Non alloca memoria per le stringhe
 * @note Termina il programma con exit(1) se il formato è invalido
 *
 * @see test_decode_integer() per una versione che alloca strutture complete
 */
long long int decode_integer(char *bencoded_int);


/**
 * @brief Decodifica una bytestring bencode e ritorna la lunghezza (senza alloc)
 *
 * Funzione leggera che decodifica una bytestring bencode (formato <len>:<dati>)
 * e ritorna solo la lunghezza totale dell'elemento codificato.
 * Non alloca strutture dati complete, gestisce il flag per dati binari.
 *
 * Comportamento del flag p_flag:
 *   - Se p_flag == 0 (stringa normale):
 *     * Decodifica la stringa
 *     * Non alloca memoria
 *
 *   - Se p_flag == 1 (dati binari):
 *     * Alloca buffer per dati binari
 *     * Stampa rappresentazione esadecimale su stdout
 *     * Resetta il flag pieces a 0
 *
 * @param bencoded_string Stringa bencode che rappresenta una bytestring
 *                        Esempio: "4:spam"
 * @param p_flag          Flag che indica il tipo:
 *                        0 = stringa normale, 1 = dati binari
 *
 * @return Lunghezza della forma codificata (<lunghezza> + ':' + <dati>)
 *
 * @note Stampa messaggi di debug su stdout se p_flag == 1
 * @note Termina il programma con exit(-1) se lunghezza < 0
 * @note Modifica la variabile globale 'pieces'
 *
 * @see test_decode_string() per una versione che alloca strutture complete
 */
int decode_string(char *bencoded_string, int p_flag);


/**
 * @brief Decodifica una lista bencode e ritorna la lunghezza (senza alloc)
 *
 * Funzione leggera che decodifica una lista bencode (formato l<elementi>e)
 * e ritorna solo la lunghezza totale dell'elemento codificato.
 * Supporta ricorsione per liste nidificate.
 *
 * @param bencoded_list Stringa bencode che rappresenta una lista
 *                      Esempio: "li1ei2ee"
 * @param idx           Indice di inizio nel buffer (per ricorsione)
 *
 * @return Lunghezza della forma codificata (numero di caratteri da 'l' a 'e')
 *
 * @note Solitamente usata internamente per determinare gli offset durante parsing
 * @note Non alloca strutture dati
 *
 * @see test_decode_list() per una versione che alloca strutture complete
 */
int decode_list(char *bencoded_list, int idx);


/**
 * @brief Decodifica un dizionario bencode e ritorna la lunghezza (senza alloc)
 *
 * Funzione leggera che decodifica un dizionario bencode (formato d<coppie>e)
 * e ritorna solo la lunghezza totale dell'elemento codificato.
 * Supporta ricorsione per dizionari nidificati.
 *
 * @param bencoded_dict Stringa bencode che rappresenta un dizionario
 *                      Esempio: "d3:key5:valuee"
 * @param idx           Indice di inizio nel buffer (per ricorsione)
 *
 * @return Lunghezza della forma codificata (numero di caratteri da 'd' a 'e')
 *
 * @note Solitamente usata internamente per determinare gli offset durante parsing
 * @note Non alloca strutture dati
 *
 * @see test_decode_dict() per una versione che alloca strutture complete
 */
int decode_dict(char *bencoded_dict, int idx);


/* ============================================================================
 * FUNZIONI: Utilità per BitTorrent
 * ============================================================================
 */

/**
 * @brief Genera un ID peer casuale per il protocollo BitTorrent
 *
 * Crea un identificativo peer (Peer ID) di 20 byte secondo le convenzioni
 * BitTorrent. Il formato è:
 *   - 8 byte: prefisso identificativo del client (es. "-GS0001-")
 *   - 12 byte: ultimi 12 byte di SHA1(peer_key)
 *
 * Questo ID è usato nel protocollo BitTorrent per identificare il client
 * quando si comunica con tracker e altri peer.
 *
 * Algoritmo:
 *   1. Usa il prefisso "-GS0001-" (8 byte)
 *   2. Calcola SHA1 della stringa peer_key
 *   3. Copia i primi 12 byte dell'hash nel peer_id
 *   4. Risultato: 20 byte totali (come richiesto da BitTorrent)
 *
 * @param peer_key Stringa usata come seed per l'hash SHA1
 *                 Esempio: "hostname_timestamp_random_value"
 * @param peer_id  Buffer dove memorizzare il Peer ID (deve essere >= 20 byte)
 *
 * @return void (memorizza il risultato in peer_id)
 *
 * @note Richiede OpenSSL (SHA1 da openssl/sha.h)
 * @note Il parametro peer_key deve essere una stringa valida null-terminated
 * @note Il buffer peer_id deve essere allocato dal chiamante (almeno 20 byte)
 * @note Non null-termina peer_id (è un buffer binario)
 *
 * Esempio di uso:
 *   unsigned char peer_id[20];
 *   generate_peer_id("my_client_v1.0", peer_id);
 *   // peer_id ora contiene 20 byte di ID peer univoco
 *
 * Specifiche BitTorrent:
 *   - Peer ID deve essere esattamente 20 byte
 *   - Spesso usato come: "-<client><version>-<random>"
 *   - Esempio reale: "-TR2710-" + 12 byte casuali (Transmission)
 */
void generate_peer_id(char *peer_key, unsigned char *peer_id);


#endif  /* BENCODE_H */
