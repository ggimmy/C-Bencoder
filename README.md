# Documentazione Moduli Bencode e Structs

## ⚠️ Disclaimer - Ruolo dell'Intelligenza Artificiale

**Importante**: Questo README e i file di supporto della documentazione sono stati generati/elaborati con l'ausilio di un'Intelligenza Artificiale (Claude). Tuttavia, è fondamentale chiarire il seguente:

### Cosa è stato fatto dall'AI
- ✍️ Generazione e strutturazione di questo README.md
- 🎨 Abbellimento e formattazione del codice sorgente
- 📝 Creazione di commenti descrittivi e documentazione inline nel codice
- 📋 Organizzazione della documentazione e aggiunta di diagrammi ASCII
- 🔗 Collegamento delle sezioni e creazione di cross-reference

### Cosa è stato fatto INTERAMENTE dall'autore umano
- 🧠 **Progettazione dell'architettura**: Le strutture dati (b_obj, b_list, b_dict, ecc.)
- 🔧 **Implementazione della logica di parsing**: Gli algoritmi ricorsivi di decodifica bencode
- 💡 **Algoritmi di decodifica**: La logica di type_to_decode, decode_integer, decode_string, test_decode_list, test_decode_dict
- 💾 **Gestione della memoria**: Allocazione con malloc, struttura delle collezioni (liste concatenate, dizionari), **funzioni di deallocazione ricorsiva**
- 🔍 **Handling dei casi speciali**: Il flag globale 'pieces' per differenziare stringhe normali da dati binari esadecimali
- ✅ **Validazione**: Controllo degli zeri iniziali negli interi, validazione delle lunghezze negative, controllo di format, **null-checking su malloc**
- 🎯 **Ricorsione**: La traversata di strutture nidificate arbitrariamente profonde in liste e dizionari
- 🔀 **State Management**: Il controllo del flusso tra parsing di diverse strutture bencode
- 📊 **Logica di calcolo degli offset**: Il tracking dell'indice durante il parsing sequenziale
- 🌐 **Protocollo BitTorrent**: Generazione del Peer ID tramite OpenSSL SHA1

### Verifica dell'Integrità
Il codice sorgente originale (`structs.c`, `structs.h`, `bencode.c`, `bencode.h`) rimane **completamente immutato** nella sua logica e funzionalità. Le uniche modifiche applicate sono:
- Aggiunta di commenti descrittivi (non cambia la semantica del codice)
- Formattazione e indentazione migliorata (non cambia il comportamento)
- Spacing e organizzazione visiva (cosmetici)

**Non una singola linea di logica è stata modificata, sostituita o generata dall'AI.**

---

## Indice
0. [⚠️ Disclaimer - Ruolo dell'Intelligenza Artificiale](#️-disclaimer---ruolo-dellintelligenza-artificiale)
1. [Panoramica del Progetto](#panoramica-del-progetto)
2. [Changelog](#changelog)
3. [Formato Bencode](#formato-bencode)
4. [Architettura dei Moduli](#architettura-dei-moduli)
5. [Strutture Dati](#strutture-dati)
6. [API Reference](#api-reference)
7. [Esempi di Utilizzo](#esempi-di-utilizzo)
8. [Considerazioni sulla Memoria](#considerazioni-sulla-memoria)
9. [Limitazioni Note](#limitazioni-note)

---

## Panoramica del Progetto

Questo progetto implementa un parser per il formato **Bencode**, uno schema di serializzazione semplice ma potente utilizzato principalmente nel protocollo **BitTorrent**. Il parser è in grado di decodificare stringhe bencode nei loro corrispondenti oggetti in memoria, supportando strutture nidificate arbitrarie.

### Componenti Principali
- **`structs.h/c`**: Definisce le strutture dati e funzioni di gestione
- **`bencode.h/c`**: Implementa i decodificatori bencode ricorsivi
- **Tipo di progetto**: Libreria C per parsing e decodifica dati, con primitive per il protocollo BitTorrent

### Caso d'Uso Primario
Parsing di file metainfo `.torrent` che utilizzano il formato bencode per rappresentare metadati del file, informazioni sui pezzi, URL dei tracker, ecc.

---

## Changelog

### v1.1 - Febbraio 2026 *(commit recenti)*

#### ✅ Memory Management — implementato
La limitazione più critica segnalata nella v1.0 è stata risolta. Sono state aggiunte tre funzioni di deallocazione ricorsiva in `structs.c`:

- **`free_obj(b_obj *ptr)`** — libera un oggetto generico gestendo tutti i tipi tramite switch. Per collezioni (liste e dizionari), delega ricorsivamente alle funzioni dedicate.
- **`free_listNodes(b_list *ptr)`** — percorre la lista concatenata liberando ogni nodo e il relativo `b_obj`, poi libera `enocded_list` e la struttura `b_list`.
- **`free_dictNodes(b_dict *ptr)`** — percorre il dizionario liberando ogni nodo con chiave e valore, poi libera `encoded_dict` e la struttura `b_dict`.

Le dichiarazioni corrispondenti sono state aggiunte in `structs.h`.

#### ✅ Null-checking su malloc — implementato
`list_init()` e `dict_init()` ora controllano il valore di ritorno di `malloc`. In caso di fallimento stampano un messaggio diagnostico su `stderr` e terminano con `exit(-1)`, evitando dereferenziazioni di puntatori NULL. Stessa robustezza applicata a `list_add()` e `dict_add()`.

#### ✅ Primitive protocollo BitTorrent — aggiunta `generate_peer_id`
Il progetto ha iniziato ad espandersi oltre il parser bencode puro: è stata aggiunta la funzione `generate_peer_id()` in `bencode.c` che genera un Peer ID di 20 byte conforme alle specifiche BitTorrent, usando OpenSSL SHA1 come primitiva crittografica.

---

### v1.0 - Gennaio 2026
Versione iniziale con parser bencode funzionante (interi, stringhe, liste, dizionari), supporto al tipo binario `B_HEX` per il campo `pieces`, e decodificatori in doppia modalità (full/lightweight).

---

## Formato Bencode

Bencode è un schema di serializzazione minimalista che supporta quattro tipi di dati primitivi:

### 1. **Interi** (Integer)
**Formato**: `i<numero>e`

Rappresenta un numero intero in base decimale.

```
i42e        → 42
i-17e       → -17
i0e         → 0
```

**Regole**:
- Inizia con `i` e termina con `e`
- Il numero è in base decimale
- Zeri iniziali non sono permessi (es. `i042e` è invalido)
- Supporta numeri negativi

---

### 2. **Stringhe (Bytestring)**
**Formato**: `<lunghezza>:<dati>`

Rappresenta una sequenza arbitraria di byte (non necessariamente ASCII).

```
4:spam      → "spam"
11:hello world  → "hello world"
20:<20 byte binari> → dati binari grezzi
```

**Caratteristiche**:
- La lunghezza è sempre un numero decimale
- Dopo la lunghezza c'è un carattere `:`
- Seguono esattamente `<lunghezza>` byte di dati
- Può contenere qualsiasi byte, inclusi 0x00
- **Critico**: i dati sono specifici della lunghezza dichiarata, non null-terminated

---

### 3. **Liste** (List)
**Formato**: `l<elementi>e`

Una sequenza ordinata di elementi eterogenei.

```
le              → [] (lista vuota)
li1ei2ee        → [1, 2]
l4:spami42ee    → ["spam", 42]
lli1ei2eee      → [[1, 2]]
```

**Caratteristiche**:
- Inizia con `l` e termina con `e`
- Gli elementi possono essere di tipo diverso
- Gli elementi sono contenitori uno dopo l'altro
- Supporta annidamento ricorsivo

---

### 4. **Dizionari** (Dictionary)
**Formato**: `d<coppie>e`

Una mappa non ordinata da chiavi (stringhe) a valori (tipo vario).

```
de                          → {} (dizionario vuoto)
d3:key5:valuee              → {"key": "value"}
d4:listli1ei2ee6:stringl4:spamee  → {"list": [1, 2], "string": ["spam"]}
```

**Caratteristiche**:
- Inizia con `d` e termina con `e`
- Le chiavi sono sempre stringhe (bytestring)
- I valori possono essere di qualsiasi tipo
- In bencode valido, le chiavi dovrebbero essere ordinate lessicograficamente (non garantito da questa implementazione)
- Supporta annidamento ricorsivo

---

### Esempio Completo: File .torrent

```
d
  8:announce 32:http://tracker.example.com:6969
  4:info d
    6:length i1048576e
    4:name 8:test.txt
    6:pieces 60:<20*3 byte binari>
  e
  7:created i1234567890e
e
```

Quando decodificato, rappresenta:
```python
{
    "announce": "http://tracker.example.com:6969",
    "info": {
        "length": 1048576,
        "name": "test.txt",
        "pieces": <binary data: 60 byte>
    },
    "created": 1234567890
}
```

---

## Architettura dei Moduli

### Modulo `structs` - Gestione Dati

Il modulo `structs` fornisce le strutture dati di base per rappresentare oggetti bencodificati in memoria.

#### Flusso di Gestione Dati

```
┌─────────────────────────┐
│  Oggetto Generico       │
│  (b_obj)                │
│                         │
│  - type: B_TYPE         │
│  - object: b_box*       │
└────────────┬────────────┘
             │
             └────────────────────────┬──────────────────┬──────────────┐
                                      │                  │              │
                          ┌───────────▼─────┐   ┌───────▼──┐   ┌──────▼───────┐
                          │ Elemento        │   │ Lista    │   │ Dizionario   │
                          │ (b_element)     │   │(b_list)  │   │  (b_dict)    │
                          │                 │   │          │   │              │
                          │ - encoded       │   │- list -> │   │- dict ->     │
                          │ - decoded       │   │          │   │              │
                          │ - lenght        │   │ list_node│   │ dict_node    │
                          └─────────────────┘   └──────────┘   └──────────────┘
```

#### Componenti Principali

**Union `b_box`**: Contenitore polimorfo che può memorizzare uno tra:
- `pieces`: dati binari grezzi
- `int_str`: intero o stringa decodificati
- `list`: lista bencodificata
- `dict`: dizionario bencodificato

**Struct `b_obj`**: Wrapper generico che associa un tipo a i dati:
```c
struct bencoded_object {
    B_TYPE type;        // Che tipo di dato?
    b_box *object;      // Dove sono i dati?
};
```

**Enum `B_TYPE`**: Enumera i tipi supportati:
- `B_INT`: intero
- `B_STR`: stringa ASCII/UTF-8
- `B_LIS`: lista
- `B_DICT`: dizionario
- `B_HEX`: dati binari (bytestring esadecimale)
- `B_NULL`: tipo non valido (errore)

---

### Modulo `bencode` - Decodifica

Il modulo `bencode` implementa i decodificatori ricorsivi per convertire stringhe bencode in strutture `b_obj`.

#### Due Strategie di Decodifica

**1. Full Decode (`test_decode_*`)**
```c
b_obj* test_decode_integer(char *bencoded_int);
b_obj* test_decode_string(char *bencoded_string, int p_flag);
b_obj* test_decode_list(char *bencoded_list, int start);
b_obj* test_decode_dict(char *bencoded_dict, int start);
```

Alloca strutture dati complete memorizzando sia la forma codificata che quella decodificata. Utile per parsing completi con accesso alle forme originali.

**2. Lightweight Decode (`decode_*`)**
```c
long long int decode_integer(char *bencoded_int);
int decode_string(char *bencoded_string, int p_flag);
int decode_list(char *bencoded_list, int idx);
int decode_dict(char *bencoded_dict, int idx);
```

Alloca solo il minimo necessario, ritorna la lunghezza dell'elemento processato. Utile per parser sequenziali efficienti.

#### Algoritmo Principale: Decodifica Ricorsiva

```
Per ogni elemento bencode:
  1. Esamina il primo carattere
  2. Determina il tipo (type_to_decode)
  3. Chiama il decodificatore appropriato
  4. Se elemento composto (lista/dict):
     - Inizializza contenitore vuoto
     - Per ogni subelemento:
       * Ricorsivamente decodifica
       * Aggiunge al contenitore
     - Restituisce contenitore pieno
  5. Ritorna oggetto b_obj
```

---

## Strutture Dati

### Gerarchia Completa

```c
/* Tipo enumerativo: che tipo di dato è memorizzato? */
typedef enum {B_INT, B_STR, B_LIS, B_DICT, B_HEX, B_NULL} B_TYPE;

/* Union polimorfa: memorizza diversi tipi in uno spazio */
union bencoded_union {
    struct pieces *pieces;              // Dati binari
    struct bencoded_element *int_str;   // Intero/Stringa
    struct bencoded_list *list;         // Lista
    struct bencoded_dict *dict;         // Dizionario
};
typedef union bencoded_union b_box;

/* Wrapper generico: associa tipo a dati */
struct bencoded_object {
    B_TYPE type;          // Tipo del dato
    b_box *object;        // Puntatore ai dati (union)
};
typedef struct bencoded_object b_obj;
```

### Strutture per Elementi Scalari

```c
/* Dati binari grezzi (per campo "pieces" nei .torrent) */
struct pieces {
    unsigned char *decoded_pieces;  // Buffer byte grezzi
    ssize_t lenght;                 // Lunghezza in byte
};
typedef struct pieces b_pieces;

/* Intero o stringa decodificati */
struct bencoded_element {
    char *encoded_element;   // Forma originale bencode (es. "i42e")
    char *decoded_element;   // Forma decodificata (es. "42")
    ssize_t lenght;          // Lunghezza della forma codificata
};
typedef struct bencoded_element b_element;
```

### Strutture per Collezioni

```c
/* Nodo di lista concatenata */
struct blist_node {
    b_obj *object;            // Elemento memorizzato
    struct blist_node *next;  // Elemento successivo
};
typedef struct blist_node list_node;

/* Lista completa */
struct bencoded_list {
    char *enocded_list;       // Forma bencode originale [TYPO: enocded]
    list_node *list;          // Primo nodo della lista
    ssize_t lenght;           // Lunghezza forma codificata
};
typedef struct bencoded_list b_list;

/* Nodo di dizionario concatenato */
struct bdict_node {
    b_obj *key;               // Chiave (sempre stringa)
    b_obj *value;             // Valore (tipo vario)
    struct bdict_node *next;  // Nodo successivo
};
typedef struct bdict_node dict_node;

/* Dizionario completo */
struct bencoded_dict {
    char *encoded_dict;       // Forma bencode originale
    dict_node *dict;          // Primo nodo del dizionario
    ssize_t lenght;           // Lunghezza forma codificata
};
typedef struct bencoded_dict b_dict;
```

---

## API Reference

### Funzioni di Inizializzazione

#### `b_list* list_init(void)`
Inizializza una lista bencodificata vuota.

```c
b_list *lista = list_init();
// lista->lenght = 0
// lista->list = NULL
// lista->enocded_list = NULL
```

**Complessità**: O(1)  
**Memory**: Alloca sizeof(b_list)  
**Note**: Controlla il fallimento di malloc, stampa su stderr e termina con exit(-1)

---

#### `b_dict* dict_init(void)`
Inizializza un dizionario bencodificato vuoto.

```c
b_dict *dict = dict_init();
// dict->lenght = 0
// dict->dict = NULL
// dict->encoded_dict = NULL
```

**Complessità**: O(1)  
**Memory**: Alloca sizeof(b_dict)  
**Note**: Controlla il fallimento di malloc, stampa su stderr e termina con exit(-1)

---

### Funzioni di Aggiunta Elementi

#### `void list_add(b_list *lista, b_obj *elem)`
Aggiunge un elemento in coda a una lista.

```c
b_obj *intero = test_decode_integer("i42e");
list_add(lista, intero);
```

**Complessità**: O(n) dove n è il numero di elementi  
**Memory**: Alloca sizeof(list_node)  
**Note**: Controlla il fallimento di malloc, termina con exit(-1)

---

#### `void dict_add(b_dict *dict, b_obj *key, b_obj *val)`
Aggiunge una coppia chiave-valore a un dizionario.

```c
b_obj *key = test_decode_string("3:key", 0);
b_obj *val = test_decode_string("5:value", 0);
dict_add(dict, key, val);
```

**Complessità**: O(n) dove n è il numero di coppie  
**Memory**: Alloca sizeof(dict_node)  
**Note**: NON ordina le chiavi lessicograficamente. Controlla il fallimento di malloc.

---

### Funzioni di Deallocazione *(aggiunte in v1.1)*

#### `void free_obj(b_obj *ptr)`
Libera ricorsivamente un oggetto generico e tutta la sua memoria interna.

```c
b_obj *num = test_decode_integer("i42e");
// ... usa num ...
free_obj(num);  // libera b_element, b_box, b_obj
```

Gestisce tutti i tipi tramite switch:
- `B_INT` / `B_STR`: libera `decoded_element`, `encoded_element`, `b_element`, `b_box`, `b_obj`
- `B_HEX`: libera `decoded_pieces`, `b_pieces`, `b_box`, `b_obj`
- `B_LIS`: delega a `free_listNodes`, poi libera `b_box` e `b_obj`
- `B_DICT`: delega a `free_dictNodes`, poi libera `b_box` e `b_obj`
- `B_NULL`: stampa errore su stderr

**Complessità**: O(n) dove n è il numero totale di nodi nella struttura

---

#### `void free_listNodes(b_list *ptr)`
Libera tutti i nodi di una lista concatenata e la struttura `b_list` stessa.

```c
free_listNodes(lista->object->list);
```

Percorre la lista liberando ogni `list_node` e il relativo `b_obj` tramite `free_obj()`. Al termine libera `enocded_list` e la struttura `b_list`.

**Complessità**: O(n) dove n è il numero di elementi

---

#### `void free_dictNodes(b_dict *ptr)`
Libera tutti i nodi di un dizionario e la struttura `b_dict` stessa.

```c
free_dictNodes(dict->object->dict);
```

Percorre il dizionario liberando ogni `dict_node` con chiave e valore tramite `free_obj()`. Al termine libera `encoded_dict` e la struttura `b_dict`.

**Complessità**: O(n) dove n è il numero di coppie

---

### Funzioni di Query Tipo

#### `B_TYPE get_object_type(b_obj *obj)`
Ritorna il tipo di un oggetto bencodificato.

```c
B_TYPE type = get_object_type(obj);
if (type == B_INT) { /* ... */ }
```

**Complessità**: O(1)

---

#### `B_TYPE get_list_node_type(list_node *node)`
Ritorna il tipo dell'elemento in un nodo di lista.

```c
B_TYPE elemType = get_list_node_type(node);
```

**Complessità**: O(1)

---

#### `B_TYPE get_dict_value_type(dict_node *node)`
Ritorna il tipo del valore in un nodo di dizionario.

```c
B_TYPE valType = get_dict_value_type(node);
```

**Complessità**: O(1)

---

### Funzioni di Decodifica (Full Decode)

#### `b_obj* test_decode_integer(char *bencoded_int)`
Decodifica un intero bencode in struttura b_obj.

```c
b_obj *num = test_decode_integer("i42e");
printf("%s\n", num->object->int_str->decoded_element);  // Output: 42
free_obj(num);
```

**Input**: `"i<numero>e"` (es. `"i42e"`)  
**Output**: `b_obj` di tipo `B_INT`  
**Validazione**: Rifiuta zeri iniziali  
**Error**: `exit(1)` se formato invalido  
**Memory**: Alloca b_element, b_box, b_obj — liberabile con `free_obj()`

---

#### `b_obj* test_decode_string(char *bencoded_string, int p_flag)`
Decodifica una bytestring bencode.

```c
// Stringa normale
b_obj *str = test_decode_string("4:spam", 0);
printf("%s\n", str->object->int_str->decoded_element);  // Output: spam

// Dati binari (p_flag=1)
b_obj *hex = test_decode_string("20:<20 bytes>", 1);
// hex->object->pieces->decoded_pieces contiene i byte grezzi
```

**Input**: `"<lunghezza>:<dati>"` (es. `"4:spam"`)  
**Output**: `b_obj` di tipo `B_STR` o `B_HEX`  
**Flag p_flag**:
- `0`: stringa normale, ritorna `B_STR`
- `1`: dati binari, ritorna `B_HEX` e stampa esadecimale  

**Special**: Se stringa == `"pieces"`, imposta flag globale `pieces=1`  
**Error**: `exit(-1)` se lunghezza < 0  
**Memory**: Alloca buffer, b_element/b_pieces, b_box, b_obj — liberabile con `free_obj()`

---

#### `b_obj* test_decode_list(char *bencoded_list, int start)`
Decodifica una lista bencode (ricorsiva).

```c
b_obj *lista = test_decode_list("li1ei2ee", 0);
// lista contiene [1, 2]
free_obj(lista);
```

**Input**: `"l<elementi>e"` (es. `"li1ei2ee"`)  
**Output**: `b_obj` di tipo `B_LIS`  
**Ricorsione**: Supporta liste e dizionari nidificati  
**Side-effects**: Stampa "INIZIO LISTA" e il contenuto  
**Bug**: `"FINE LISTA"` è unreachable code  
**Error**: `exit(-1)` se tipo non riconosciuto  
**Memory**: Alloca b_list, nodi, b_box, b_obj — liberabile con `free_obj()`

---

#### `b_obj* test_decode_dict(char *bencoded_dict, int start)`
Decodifica un dizionario bencode (ricorsiva).

```c
b_obj *dict = test_decode_dict("d3:key5:valuee", 0);
// dict contiene {"key": "value"}
free_obj(dict);
```

**Input**: `"d<coppie>e"` (es. `"d3:key5:valuee"`)  
**Output**: `b_obj` di tipo `B_DICT`  
**Ricorsione**: Supporta liste e dizionari nidificati  
**Chiavi**: Sempre stringhe (bytestring)  
**Valori**: Tipo vario (intero, stringa, lista, dict)  
**Side-effects**: Stampa "INIZIO DICT", chiavi, valori, "FINE DICT"  
**Ordinamento**: NON garantisce chiavi ordinate lessicograficamente  
**Error**: `exit(-1)` se tipo valore non riconosciuto  
**Memory**: Alloca b_dict, nodi, b_box, b_obj — liberabile con `free_obj()`

---

### Funzioni di Decodifica (Lightweight)

#### `long long int decode_integer(char *bencoded_int)`
Decodifica un intero bencode, stampa risultato, ritorna lunghezza.

```c
long long len = decode_integer("i42e");  // Stampa: 42, ritorna 4
```

**Output**: Lunghezza della stringa codificata  
**Side-effects**: Stampa il valore decodificato su stdout  
**Memory**: Alloca solo buffer temporaneo  
**Error**: `exit(1)` se formato invalido

---

#### `int decode_string(char *bencoded_string, int p_flag)`
Decodifica una bytestring bencode, ritorna lunghezza.

```c
int len = decode_string("4:spam", 0);  // Ritorna 6
```

**Output**: Lunghezza della stringa codificata  
**Side-effects**: Se `p_flag=1`, stampa esadecimale  
**Memory**: Alloca buffer temporanei  
**Special**: Modifica flag globale `pieces`

---

#### `int decode_list(char *bencoded_list, int idx)`
Decodifica una lista bencode, ritorna lunghezza.

```c
int len = decode_list("li1ei2ee", 0);  // Ritorna 8
```

**Output**: Lunghezza della lista codificata

---

#### `int decode_dict(char *bencoded_dict, int idx)`
Decodifica un dizionario bencode, ritorna lunghezza.

```c
int len = decode_dict("d3:key5:valuee", 0);  // Ritorna 14
```

**Output**: Lunghezza del dizionario codificato

---

### Funzioni di Stampa

#### `void print_hex(unsigned char *pieces, size_t lenght)`
Stampa un buffer di byte in formato esadecimale.

```c
unsigned char data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
print_hex(data, 5);
// Output: 48 65 6C 6C 6F
```

---

#### `void print_list(b_list *lista)`
Stampa ricorsivamente il contenuto di una lista.

```c
b_obj *lista = test_decode_list("li1ei2ee", 0);
print_list(lista->object->list);
// Output:
// 1
// 2
```

---

#### `void print_dict(b_dict *dict)`
Stampa ricorsivamente il contenuto di un dizionario.

```c
b_obj *d = test_decode_dict("d3:key5:valuee", 0);
print_dict(d->object->dict);
// Output:
// key value
```

---

#### `void print_object(b_obj *obj, size_t pieces_lenght)`
Stampa ricorsivamente un oggetto generico.

```c
print_object(obj, 0);
```

**Note**: Parametro `pieces_lenght` necessario per tipo `B_HEX`

---

### Funzioni di Ricerca

#### `b_dict* get_info_dict(b_dict *dict, char *key)`
Ricerca una chiave nel dizionario e ritorna il valore (se è dict).

```c
b_obj *torrent = test_decode_dict("d4:infod4:name4:testee", 0);
b_dict *info = get_info_dict(torrent->object->dict, "info");
```

**Output**: Puntatore a `b_dict` se trovato, `NULL` altrimenti  
**Side-effects**: Stampa "NOT FOUND!" se chiave assente  
**Complessità**: O(n)

---

#### `void find_by_key(b_dict *dict, char *key)`
Ricerca una chiave e stampa il valore.

```c
find_by_key(torrent->object->dict, "announce");
// Output: FOUND: http://tracker.example.com:6969
```

**Side-effects**: Stampa "FOUND: <valore>" o "NOT FOUND!"  
**Complessità**: O(n)

---

### Funzioni Utility BitTorrent

#### `void generate_peer_id(char *peer_key, unsigned char *peer_id)`
Genera un Peer ID di 20 byte per il protocollo BitTorrent.

```c
unsigned char peer_id[20];
generate_peer_id("my_client_v1.0", peer_id);
// peer_id contiene 8 byte di prefisso + 12 byte di hash SHA1
```

**Formato risultato**:
- Byte 0-7: `"-GS0001-"` (prefisso client ID)
- Byte 8-19: Primi 12 byte di SHA1(peer_key)

**Total**: 20 byte (standard BitTorrent)  
**Requires**: OpenSSL (linkare con `-lssl -lcrypto`)  
**Note**: Buffer `peer_id` NON è null-terminated — è dati binari

---

### Funzioni Helper

#### `B_TYPE type_to_decode(char start)`
Determina il tipo bencode dal primo carattere.

```c
B_TYPE type = type_to_decode('i');   // Ritorna B_INT
type = type_to_decode('4');          // Ritorna B_STR
type = type_to_decode('l');          // Ritorna B_LIS
type = type_to_decode('d');          // Ritorna B_DICT
type = type_to_decode('x');          // Ritorna B_NULL
```

**Complessità**: O(1)

---

#### `char* get_bencoded_int(char *bencoded_obj)`
Estrae un intero bencode completo dalla stringa.

```c
char *extracted = get_bencoded_int("i42eblah");
// extracted contiene "i42e" (string appena allocata)
free(extracted);
```

**Input**: Stringa che inizia con `'i'`  
**Output**: Stringa appena allocata contenente `"i...e"`  
**Memory**: Deve essere liberata dal chiamante con `free()`

---

## Esempi di Utilizzo

### Esempio 1: Decodificare un Intero con Free

```c
#include "bencode.h"
#include "structs.h"
#include <stdio.h>

int main() {
    b_obj *num = test_decode_integer("i42e");
    
    printf("Intero: %s\n", num->object->int_str->decoded_element);
    printf("Codificato: %s\n", num->object->int_str->encoded_element);
    
    // Ora è possibile liberare tutta la memoria con una sola chiamata
    free_obj(num);
    
    return 0;
}
```

**Output**:
```
Intero: 42
Codificato: i42e
```

---

### Esempio 2: Decodificare una Lista con Free

```c
#include "bencode.h"
#include "structs.h"

int main() {
    b_obj *lista = test_decode_list("li1ei2ei3ee", 0);
    
    // La lista viene stampata automaticamente durante il parsing
    // Output:
    // INIZIO LISTA
    // 1
    // 2
    // 3

    // Libera ricorsivamente tutti i nodi e gli elementi interni
    free_obj(lista);
    
    return 0;
}
```

---

### Esempio 3: Decodificare un Dizionario con Free

```c
#include "bencode.h"
#include "structs.h"

int main() {
    const char *torrent_str = "d8:announce32:http://tracker.example.com:69694:name8:test.txte";
    
    b_obj *torrent = test_decode_dict(torrent_str, 0);
    
    // Stampa il contenuto
    print_dict(torrent->object->dict);
    
    // Ricerca una chiave specifica
    find_by_key(torrent->object->dict, "announce");
    // Output: FOUND: http://tracker.example.com:6969

    // Libera tutta la struttura ricorsivamente
    free_obj(torrent);
    
    return 0;
}
```

---

### Esempio 4: Parsing di un File .torrent Semplice

```c
#include "bencode.h"
#include "structs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <torrent_file>\n", argv[0]);
        return 1;
    }
    
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    
    char buffer[65536];
    ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
    if (len < 0) { perror("read"); return 1; }
    buffer[len] = '\0';
    close(fd);
    
    b_obj *torrent = test_decode_dict(buffer, 0);
    
    if (torrent && torrent->type == B_DICT) {
        printf("=== Torrent Decodificato ===\n");
        print_dict(torrent->object->dict);
        
        printf("\n=== URL del Tracker ===\n");
        find_by_key(torrent->object->dict, "announce");
        
        printf("\n=== Informazioni sul File ===\n");
        b_dict *info_dict = get_info_dict(torrent->object->dict, "info");
        if (info_dict) {
            print_dict(info_dict);
        }
    }

    free_obj(torrent);
    
    return 0;
}
```

---

### Esempio 5: Generare un Peer ID

```c
#include "bencode.h"
#include <stdio.h>

int main() {
    unsigned char peer_id[20];
    
    generate_peer_id("my_client_v1.0_12345", peer_id);
    
    printf("Peer ID: ");
    for (int i = 0; i < 20; i++) {
        printf("%02X ", peer_id[i]);
    }
    printf("\n");
    
    return 0;
}
```

**Output**:
```
Peer ID: 2D 47 53 30 30 30 31 2D 3A ... (20 byte totali)
```

---

## Considerazioni sulla Memoria

### Allocazione Dinamica

Tutte le funzioni di decodifica allocano memoria con `malloc()`. A partire dalla v1.1, è disponibile `free_obj()` come punto di ingresso unico per la deallocazione ricorsiva.

### Pattern di Allocazione per Full Decode

**Per un intero `test_decode_integer()`**:
```
b_obj
├── b_box
└── b_element
    ├── encoded_element (stringa)
    └── decoded_element (stringa)
```
→ Liberabile con `free_obj(ptr)`

**Per una lista `test_decode_list()`**:
```
b_obj
├── b_box
└── b_list
    ├── enocded_list (stringa)
    └── list_node → list_node → list_node → ...
        └── b_obj (per ogni elemento)
```
→ Liberabile con `free_obj(ptr)` che chiama `free_listNodes()`

**Per un dizionario `test_decode_dict()`**:
```
b_obj
├── b_box
└── b_dict
    ├── encoded_dict (stringa)
    └── dict_node → dict_node → ...
        ├── key (b_obj)
        └── value (b_obj)
```
→ Liberabile con `free_obj(ptr)` che chiama `free_dictNodes()`

---

### Optimization Tips

1. **Per parsing efficiente**: Usare `decode_*()` lightweight anziché `test_decode_*()`
2. **Per strutture nidificate profonde**: Rischio di stack overflow con ricorsione (stack default ~8MB)
3. **Valgrind**: Usare `valgrind --leak-check=full` per verificare che `free_obj()` liberi correttamente tutte le strutture

---

## Limitazioni Note

### Limitazioni Architetturali

#### 1. **Variabile Globale `pieces`** *(aperta)*
**Descrizione**: Il flag globale `int pieces` controlla il parsing di stringhe binarie vs normali  
**Problema**: Le variabili globali rendono il codice non thread-safe e difficile da testare  
**Impatto**: Non è possibile parsare più file in parallelo  
**Fix**: Passare lo stato come parametro alle funzioni

---

#### 2. **No Lexicographic Ordering for Dictionary Keys** *(aperta)*
**Descrizione**: Bencode richiede che le chiavi siano ordinate lessicograficamente  
**Problema**: Questa implementazione inserisce le chiavi nell'ordine di parsing  
**Impatto**: File .torrent invalidi (anche se funzionano comunque)  
**Fix**: Implementare un'inserzione ordinata o ordinare dopo il parsing

---

#### 3. **No Bounds Checking** *(aperta)*
**Descrizione**: Non c'è controllo sulla validità dei puntatori o della lunghezza dati  
**Problema**: Possibili buffer overflows se dati bencode malformati  
**Impatto**: Crash o comportamento indefinito  
**Fix**: Aggiungere controlli sulla lunghezza in ogni funzione

---

#### 4. **~~No Memory Management Helpers~~** *(risolta in v1.1)*
~~Implementare `free_b_obj()`, `free_b_list()`, `free_b_dict()`, ecc.~~  
✅ Risolto con `free_obj()`, `free_listNodes()`, `free_dictNodes()`

---

### Limitazioni di Specifica

#### 1. **No Input Validation** *(aperta)*
**Descrizione**: Le funzioni assumono input benform senza validare  
**Impatto**: Crash su input malformati  
**Esempio**: `atoi()` su stringa non numerica

---

## Conclusione

Questo progetto fornisce un'implementazione funzionante di un parser Bencode con gestione della memoria. È adatto per:
- **Parsing di file .torrent** semplici
- **Progetti didattici** su parsing, ricorsione e gestione della memoria in C
- **Prototipi veloci** in C
- **Base di partenza** per un client BitTorrent completo

Non è consigliato per:
- **Applicazioni di produzione** (manca error handling robusto e bounds checking)
- **File .torrent complessi** (problemi di ordine chiavi)
- **Parsing parallelo** (variabile globale non thread-safe)

---

## Riferimenti

### Specifiche Bencode
- [BitTorrent Specification - Bencode](http://www.bittorrent.org/beps/bep_0003.html)
- [Wikipedia - Bencode](https://en.wikipedia.org/wiki/Bencode)

### Formato .torrent
- [BitTorrent Metainfo Specification](http://www.bittorrent.org/beps/bep_0003.html#metainfo-files)

### OpenSSL
- [OpenSSL SHA1 Documentation](https://www.openssl.org/docs/man1.1.1/man3/SHA1.html)

---

**Ultima modifica**: Febbraio 2026  
**Versione**: 1.1  
**Autore**: Basato su codice originale, documentazione aggiunta con AI
