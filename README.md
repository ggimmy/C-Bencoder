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
- 💡 **Algoritmi di decodifica**: La logica di type_to_decode, decode_integer, decode_string, decode_list, decode_dict
- 💾 **Gestione della memoria**: Allocazione con malloc, struttura delle collezioni (liste concatenate, dizionari), **funzioni di deallocazione ricorsiva**
- 🔍 **Handling dei casi speciali**: Il `p_flag` locale in `decode_dict()` per differenziare stringhe normali da dati binari esadecimali in modo thread-safe
- ✅ **Validazione**: Controllo degli zeri iniziali negli interi, validazione delle lunghezze negative, **null-checking su tutti i puntatori in ingresso**, **null-checking su malloc**
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

### v1.2 - Febbraio 2026 *(commit recenti)*

#### ✅ Input Validation su puntatori e `atoi()` — implementata
Tutte le funzioni pubbliche ora validano i puntatori in ingresso prima di dereferenziarli. Se un argomento è `NULL`, la funzione stampa un messaggio diagnostico su `stderr` e termina con `exit(-1)`. Le funzioni interessate sono:

- `free_obj()`, `free_listNodes()`, `free_dictNodes()`
- `list_add()`, `dict_add()`
- `get_object_type()`, `get_list_node_type()`, `get_dict_value_type()`
- `print_hex()`, `print_list()`, `print_dict()`, `print_object()`
- `get_info_dict()`, `find_by_key()`

Per quanto riguarda `atoi()`, `decode_string()` controlla che il risultato non sia negativo (`bencoded_string_length < 0`) terminando con `exit(-1)` e messaggio su `stderr`.

---

#### ✅ Rimossa variabile globale `pieces` — meccanismo thread-safe — implementato
La variabile globale `int pieces` è stata **eliminata**. Al suo posto, `decode_dict()` utilizza ora una variabile locale `int p_flag` che viene impostata a `1` non appena viene incontrata la chiave `"pieces"` e azzerata subito dopo aver decodificato il valore binario corrispondente.

Questo rende il parsing **thread-safe**: più invocazioni di `decode_dict()` su thread distinti non condividono più stato globale, eliminando le race condition.

```c
// Prima (v1.1): variabile globale condivisa tra tutti i thread
extern int pieces;

// Dopo (v1.2): stato locale al singolo frame di decode_dict()
int p_flag = 0;
if (strcmp(key->object->int_str->decoded_element, "pieces") == 0) {
    p_flag = 1;
}
```

---

#### ✅ Rimosse funzioni inutilizzate — funzioni di test promosse a principali
Le versioni *lightweight* che ritornavano solo la lunghezza erano rimaste nel codebase senza essere utilizzate e sono state **rimosse**.

Le funzioni precedentemente denominate `test_decode_*` sono state **rinominate** e promosse a interfaccia principale del modulo:

| Nome precedente (v1.1)  | Nome attuale (v1.2) |
|-------------------------|---------------------|
| `test_decode_integer()` | `decode_integer()`  |
| `test_decode_string()`  | `decode_string()`   |
| `test_decode_list()`    | `decode_list()`     |
| `test_decode_dict()`    | `decode_dict()`     |

L'API pubblica in `bencode.h` riflette questo cambiamento.

---

### v1.1 - Febbraio 2026

#### ✅ Memory Management — implementato
Sono state aggiunte tre funzioni di deallocazione ricorsiva in `structs.c`:

- **`free_obj(b_obj *ptr)`** — libera un oggetto generico gestendo tutti i tipi tramite switch. Per collezioni (liste e dizionari), delega ricorsivamente alle funzioni dedicate.
- **`free_listNodes(b_list *ptr)`** — percorre la lista concatenata liberando ogni nodo e il relativo `b_obj`, poi libera `encoded_list` e la struttura `b_list`.
- **`free_dictNodes(b_dict *ptr)`** — percorre il dizionario liberando ogni nodo con chiave e valore, poi libera `encoded_dict` e la struttura `b_dict`.

#### ✅ Null-checking su malloc — implementato
`list_init()` e `dict_init()` controllano il valore di ritorno di `malloc`. Stessa robustezza applicata a `list_add()` e `dict_add()`.

#### ✅ Primitive protocollo BitTorrent — aggiunta `generate_peer_id`
Aggiunta `generate_peer_id()` che genera un Peer ID di 20 byte conforme alle specifiche BitTorrent, usando OpenSSL SHA1.

---

### v1.0 - Gennaio 2026
Versione iniziale con parser bencode funzionante (interi, stringhe, liste, dizionari), supporto al tipo binario `B_HEX` per il campo `pieces`, e decodificatori in doppia modalità (full/lightweight).

---

## Formato Bencode

Bencode è un schema di serializzazione minimalista che supporta quattro tipi di dati primitivi:

### 1. **Interi** (Integer)
**Formato**: `i<numero>e`

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

```
4:spam          → "spam"
11:hello world  → "hello world"
20:<20 byte>    → dati binari grezzi
```

**Caratteristiche**:
- La lunghezza è sempre un numero decimale seguito da `:`
- Seguono esattamente `<lunghezza>` byte di dati
- Può contenere qualsiasi byte, inclusi `0x00`
- I dati sono delimitati dalla lunghezza dichiarata, non da null-terminator

---

### 3. **Liste** (List)
**Formato**: `l<elementi>e`

```
le              → [] (lista vuota)
li1ei2ee        → [1, 2]
l4:spami42ee    → ["spam", 42]
lli1ei2eee      → [[1, 2]]
```

**Caratteristiche**:
- Inizia con `l` e termina con `e`
- Gli elementi possono essere di tipo diverso
- Supporta annidamento ricorsivo

---

### 4. **Dizionari** (Dictionary)
**Formato**: `d<coppie>e`

```
de                  → {} (dizionario vuoto)
d3:key5:valuee      → {"key": "value"}
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
             └──────────────┬──────────────────┬──────────────┐
                            │                  │              │
                ┌───────────▼─────┐   ┌────────▼─┐   ┌───────▼──────┐
                │ Elemento        │   │ Lista    │   │ Dizionario   │
                │ (b_element)     │   │(b_list)  │   │  (b_dict)    │
                │                 │   │          │   │              │
                │ - encoded       │   │- list -> │   │- dict ->     │
                │ - decoded       │   │          │   │              │
                │ - length        │   │list_node │   │ dict_node    │
                └─────────────────┘   └──────────┘   └──────────────┘
```

**Union `b_box`**: Contenitore polimorfo che può memorizzare uno tra:
- `pieces`: dati binari grezzi
- `int_str`: intero o stringa decodificati
- `list`: lista bencodificata
- `dict`: dizionario bencodificato

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

#### Strategia di Decodifica

Il modulo espone un'unica famiglia di funzioni che allocano strutture dati complete memorizzando sia la forma codificata che quella decodificata:

```c
b_obj* decode_integer(char *bencoded_int);
b_obj* decode_string(char *bencoded_string, int p_flag);
b_obj* decode_list(char *bencoded_list, int start);
b_obj* decode_dict(char *bencoded_dict, int start);
```

> **Nota v1.2**: Le precedenti versioni *lightweight* che ritornavano solo la lunghezza dell'elemento sono state rimosse in quanto inutilizzate. Le funzioni `test_decode_*` sono state promosse e rinominate nelle correnti `decode_*`.

#### Meccanismo Thread-Safe per il Campo `pieces`

Dalla v1.2, il rilevamento del campo binario `pieces` avviene tramite una variabile locale `p_flag` all'interno di `decode_dict()`, senza più dipendere da stato globale condiviso:

```
decode_dict()
  ├── Incontra chiave "pieces" → imposta p_flag = 1 (locale)
  ├── Decodifica il valore con decode_string(..., p_flag)
  │     └── p_flag=1 → ritorna B_HEX (dati binari)
  └── p_flag = 0  (reset locale, nessun effetto su altri thread)
```

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
/* Tipo enumerativo */
typedef enum {B_INT, B_STR, B_LIS, B_DICT, B_HEX, B_NULL} B_TYPE;

/* Union polimorfa */
union bencoded_union {
    struct pieces *pieces;
    struct bencoded_element *int_str;
    struct bencoded_list *list;
    struct bencoded_dict *dict;
};
typedef union bencoded_union b_box;

/* Wrapper generico */
struct bencoded_object {
    B_TYPE type;
    b_box *object;
};
typedef struct bencoded_object b_obj;
```

### Strutture per Elementi Scalari

```c
/* Dati binari grezzi (per campo "pieces" nei .torrent) */
struct pieces {
    unsigned char *decoded_pieces;
    ssize_t length;
};
typedef struct pieces b_pieces;

/* Intero o stringa decodificati */
struct bencoded_element {
    char *encoded_element;
    char *decoded_element;
    ssize_t length;
};
typedef struct bencoded_element b_element;
```

### Strutture per Collezioni

```c
/* Nodo di lista concatenata */
struct blist_node {
    b_obj *object;
    struct blist_node *next;
};
typedef struct blist_node list_node;

/* Lista completa */
struct bencoded_list {
    char *encoded_list;
    list_node *list;
    ssize_t length;
};
typedef struct bencoded_list b_list;

/* Nodo di dizionario concatenato */
struct bdict_node {
    b_obj *key;
    b_obj *value;
    struct bdict_node *next;
};
typedef struct bdict_node dict_node;

/* Dizionario completo */
struct bencoded_dict {
    char *encoded_dict;
    dict_node *dict;
    ssize_t length;
};
typedef struct bencoded_dict b_dict;
```

---

## API Reference

### Funzioni di Inizializzazione

#### `b_list* list_init(void)`
Inizializza una lista bencodificata vuota.

**Complessità**: O(1) | **Memory**: alloca `sizeof(b_list)`
**Note**: controlla il fallimento di `malloc`, stampa su `stderr` e termina con `exit(-1)`

---

#### `b_dict* dict_init(void)`
Inizializza un dizionario bencodificato vuoto.

**Complessità**: O(1) | **Memory**: alloca `sizeof(b_dict)`
**Note**: controlla il fallimento di `malloc`, stampa su `stderr` e termina con `exit(-1)`

---

### Funzioni di Aggiunta Elementi

#### `void list_add(b_list *lista, b_obj *elem)`
Aggiunge un elemento in coda a una lista.

**Complessità**: O(n) | **Memory**: alloca `sizeof(list_node)`
**Validation**: termina con `exit(-1)` se `lista` o `elem` sono `NULL`

---

#### `void dict_add(b_dict *dict, b_obj *key, b_obj *val)`
Aggiunge una coppia chiave-valore a un dizionario.

**Complessità**: O(n) | **Memory**: alloca `sizeof(dict_node)`
**Validation**: termina con `exit(-1)` se `dict`, `key` o `val` sono `NULL`
**Note**: NON ordina le chiavi lessicograficamente

---

### Funzioni di Deallocazione

#### `void free_obj(b_obj *ptr)`
Libera ricorsivamente un oggetto generico e tutta la sua memoria interna.

```c
b_obj *num = decode_integer("i42e");
free_obj(num);  // libera b_element, b_box, b_obj
```

Gestisce tutti i tipi tramite switch:
- `B_INT` / `B_STR`: libera `decoded_element`, `encoded_element`, `b_element`, `b_box`, `b_obj`
- `B_HEX`: libera `decoded_pieces`, `b_pieces`, `b_box`, `b_obj`
- `B_LIS`: delega a `free_listNodes()`, poi libera `b_box` e `b_obj`
- `B_DICT`: delega a `free_dictNodes()`, poi libera `b_box` e `b_obj`
- `B_NULL`: stampa errore su `stderr`

**Complessità**: O(n) totale | **Validation**: termina con `exit(-1)` se `ptr` è `NULL`

---

#### `void free_listNodes(b_list *ptr)`
Libera tutti i nodi di una lista e la struttura `b_list` stessa.

**Complessità**: O(n) | **Validation**: termina con `exit(-1)` se `ptr` è `NULL`

---

#### `void free_dictNodes(b_dict *ptr)`
Libera tutti i nodi di un dizionario e la struttura `b_dict` stessa.

**Complessità**: O(n) | **Validation**: termina con `exit(-1)` se `ptr` è `NULL`

---

### Funzioni di Query Tipo

#### `B_TYPE get_object_type(b_obj *obj)`
Ritorna il tipo di un oggetto bencodificato.
**Complessità**: O(1) | **Validation**: termina con `exit(-1)` se `obj` è `NULL`

---

#### `B_TYPE get_list_node_type(list_node *node)`
Ritorna il tipo dell'elemento in un nodo di lista.
**Complessità**: O(1) | **Validation**: termina con `exit(-1)` se `node` è `NULL`

---

#### `B_TYPE get_dict_value_type(dict_node *node)`
Ritorna il tipo del valore in un nodo di dizionario.
**Complessità**: O(1) | **Validation**: termina con `exit(-1)` se `node` è `NULL`

---

### Funzioni di Decodifica

#### `b_obj* decode_integer(char *bencoded_int)`
Decodifica un intero bencode e ritorna la struttura `b_obj`.

```c
b_obj *num = decode_integer("i42e");
printf("%s\n", num->object->int_str->decoded_element);  // Output: 42
free_obj(num);
```

**Input**: `"i<numero>e"` | **Output**: `b_obj` di tipo `B_INT`
**Validazione**: rifiuta zeri iniziali | **Error**: `exit(1)` se formato invalido
**Memory**: alloca `b_element`, `b_box`, `b_obj` — liberabile con `free_obj()`

---

#### `b_obj* decode_string(char *bencoded_string, int p_flag)`
Decodifica una bytestring bencode.

```c
b_obj *str = decode_string("4:spam", 0);
printf("%s\n", str->object->int_str->decoded_element);  // Output: spam
free_obj(str);
```

**Input**: `"<lunghezza>:<dati>"` | **Output**: `b_obj` di tipo `B_STR` (p_flag=0) o `B_HEX` (p_flag=1)
**Error**: `exit(-1)` se lunghezza < 0
**Memory**: alloca buffer, `b_element`/`b_pieces`, `b_box`, `b_obj` — liberabile con `free_obj()`

---

#### `b_obj* decode_list(char *bencoded_list, int start)`
Decodifica una lista bencode (ricorsiva).

```c
b_obj *lista = decode_list("li1ei2ee", 0);
free_obj(lista);
```

**Input**: `"l<elementi>e"` | **Output**: `b_obj` di tipo `B_LIS`
**Ricorsione**: supporta liste e dizionari nidificati
**Side-effects**: stampa "INIZIO LISTA" e il contenuto durante il parsing
**Error**: `exit(-1)` se tipo non riconosciuto
**Memory**: alloca `b_list`, nodi, `b_box`, `b_obj` — liberabile con `free_obj()`

---

#### `b_obj* decode_dict(char *bencoded_dict, int start)`
Decodifica un dizionario bencode (ricorsiva). Gestisce automaticamente il campo `pieces` tramite variabile locale `p_flag`, senza dipendere da stato globale.

```c
b_obj *dict = decode_dict("d3:key5:valuee", 0);
free_obj(dict);
```

**Input**: `"d<coppie>e"` | **Output**: `b_obj` di tipo `B_DICT`
**Ricorsione**: supporta liste e dizionari nidificati
**Thread-safety**: `p_flag` è locale — nessuna variabile globale condivisa
**Side-effects**: stampa "INIZIO DICT", chiavi, valori, "FINE DICT" durante il parsing
**Error**: `exit(-1)` se tipo valore non riconosciuto
**Memory**: alloca `b_dict`, nodi, `b_box`, `b_obj` — liberabile con `free_obj()`

---

### Funzioni di Stampa

#### `void print_hex(unsigned char *pieces, size_t length)`
Stampa un buffer di byte in formato esadecimale (`48 65 6C ...`).
**Validation**: termina con `exit(-1)` se `pieces` è `NULL`

---

#### `void print_list(b_list *lista)`
Stampa ricorsivamente il contenuto di una lista.
**Validation**: termina con `exit(-1)` se `lista` è `NULL`

---

#### `void print_dict(b_dict *dict)`
Stampa ricorsivamente il contenuto di un dizionario.
**Validation**: termina con `exit(-1)` se `dict` è `NULL`

---

#### `void print_object(b_obj *obj, size_t pieces_length)`
Stampa ricorsivamente un oggetto generico. Il parametro `pieces_length` è necessario per elementi di tipo `B_HEX`.
**Validation**: termina con `exit(-1)` se `obj` è `NULL`

---

### Funzioni di Ricerca

#### `b_dict* get_info_dict(b_dict *dict, char *key)`
Ricerca una chiave nel dizionario e ritorna il valore come `b_dict`.

```c
b_dict *info = get_info_dict(torrent->object->dict, "info");
```

**Output**: puntatore a `b_dict` se trovato, `NULL` altrimenti (stampa "NOT FOUND!")
**Validation**: termina con `exit(-1)` se `dict` o `key` sono `NULL`
**Complessità**: O(n)

---

#### `void find_by_key(b_dict *dict, char *key)`
Ricerca una chiave e stampa il valore associato.

```c
find_by_key(torrent->object->dict, "announce");
// Output: FOUND: http://tracker.example.com:6969
```

**Validation**: termina con `exit(-1)` se `dict` o `key` sono `NULL`
**Complessità**: O(n)

---

### Funzioni Utility BitTorrent

#### `void generate_peer_id(char *peer_key, unsigned char *peer_id)`
Genera un Peer ID di 20 byte per il protocollo BitTorrent.

```c
unsigned char peer_id[20];
generate_peer_id("my_client_v1.0", peer_id);
```

**Formato risultato**:
- Byte 0–7: `"-GS0001-"` (prefisso client ID)
- Byte 8–19: primi 12 byte di SHA1(peer_key)

**Requires**: OpenSSL (linkare con `-lssl -lcrypto`)
**Note**: `peer_id` NON è null-terminated — è un buffer binario puro

---

### Funzioni Helper

#### `B_TYPE type_to_decode(char start)`
Determina il tipo bencode dal primo carattere. **Complessità**: O(1)

---

#### `char* get_bencoded_int(char *bencoded_obj)`
Estrae un intero bencode completo dalla stringa. Ritorna stringa allocata — liberare con `free()`.

---

## Esempi di Utilizzo

### Esempio 1: Decodificare un Intero

```c
b_obj *num = decode_integer("i42e");
printf("Intero: %s\n", num->object->int_str->decoded_element);
printf("Codificato: %s\n", num->object->int_str->encoded_element);
free_obj(num);
```

---

### Esempio 2: Decodificare una Lista

```c
b_obj *lista = decode_list("li1ei2ei3ee", 0);
// Stampa automatica durante il parsing:
// INIZIO LISTA / 1 / 2 / 3
free_obj(lista);
```

---

### Esempio 3: Decodificare un Dizionario e Cercare una Chiave

```c
const char *s = "d8:announce32:http://tracker.example.com:69694:name8:test.txte";
b_obj *torrent = decode_dict(s, 0);

find_by_key(torrent->object->dict, "announce");
// Output: FOUND: http://tracker.example.com:6969

free_obj(torrent);
```

---

### Esempio 4: Parsing di un File .torrent

```c
int fd = open(argv[1], O_RDONLY);
char buffer[65536];
ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
buffer[len] = '\0';
close(fd);

b_obj *torrent = decode_dict(buffer, 0);

find_by_key(torrent->object->dict, "announce");

b_dict *info = get_info_dict(torrent->object->dict, "info");
if (info) print_dict(info);

free_obj(torrent);
```

---

### Esempio 5: Generare un Peer ID

```c
unsigned char peer_id[20];
generate_peer_id("my_client_v1.0_12345", peer_id);

for (int i = 0; i < 20; i++) printf("%02X ", peer_id[i]);
printf("\n");
// Output: 2D 47 53 30 30 30 31 2D ... (20 byte)
```

---

## Considerazioni sulla Memoria

### Pattern di Allocazione

**`decode_integer()`**:
```
b_obj → b_box → b_element → encoded_element (stringa)
                           → decoded_element (stringa)
```

**`decode_list()`**:
```
b_obj → b_box → b_list → encoded_list (stringa)
                        → list_node → list_node → ...
                              └── b_obj (per ogni elemento)
```

**`decode_dict()`**:
```
b_obj → b_box → b_dict → encoded_dict (stringa)
                        → dict_node → dict_node → ...
                              ├── key  (b_obj)
                              └── value (b_obj)
```

Tutte le strutture sono liberabili con una singola chiamata a `free_obj()`.

---

## Limitazioni Note

#### ~~Variabile Globale `pieces`~~ *(risolta in v1.2)*
✅ Sostituita con `p_flag` locale in `decode_dict()` — parsing ora thread-safe.

---

#### ~~No Input Validation~~ *(risolta in v1.2)*
✅ Tutte le funzioni pubbliche controllano i puntatori `NULL` in ingresso. `decode_string()` valida il risultato di `atoi()`.

---

#### ~~No Memory Management Helpers~~ *(risolta in v1.1)*
✅ Risolto con `free_obj()`, `free_listNodes()`, `free_dictNodes()`.

---

#### No Bounds Checking sul Buffer *(aperta)*
Non c'è controllo sulla lunghezza massima del buffer bencode in ingresso. Input malformati che eccedono i limiti possono causare comportamento indefinito.

---

## Riferimenti

- [BitTorrent Specification - Bencode](http://www.bittorrent.org/beps/bep_0003.html)
- [Wikipedia - Bencode](https://en.wikipedia.org/wiki/Bencode)
- [OpenSSL SHA1 Documentation](https://www.openssl.org/docs/man1.1.1/man3/SHA1.html)

---

**Ultima modifica**: Febbraio 2026 | **Versione**: 1.2 | **Autore**: Basato su codice originale, documentazione aggiunta con AI
