CC = gcc
CFLAGS = -Wall -g
# Link alle librerie OpenSSL (necessarie per SHA1 in bencode.c)
LDFLAGS = -lssl -lcrypto

# Nome dell'eseguibile finale
TARGET = bencode

# Oggetti da compilare
# Nota: bencode.o non è qui perché il codice di bencode.c è incluso direttamente in main.c
OBJS = main.o structs.o

# Regola di default
all: $(TARGET)

# Regola per creare l'eseguibile
$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Regola per main.o
# Dipende anche da bencode.c perché viene incluso tramite #include "bencode.c"
main.o: main.c bencode.c bencode.h structs.h
	$(CC) $(CFLAGS) -c main.c

# Regola per structs.o
structs.o: structs.c structs.h
	$(CC) $(CFLAGS) -c structs.c

# Regola per pulire i file compilati
clean:
	rm -f $(OBJS) $(TARGET)