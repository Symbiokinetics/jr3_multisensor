CC=gcc
CFLAGS=-Wall -pedantic
OBJ=obj
LDFLAGS=-pthread
INC=inc
SRC=src

all: main.o jr3API.o threads.o misc.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ)/main.o \
		$(OBJ)/jr3API.o $(OBJ)/threads.o \
		$(OBJ)/misc.o -o collector

clean:
	rm -f collector obj/*

main.o:
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)/main.c -o $(OBJ)/main.o

jr3API.o:
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)/jr3API.c -o $(OBJ)/jr3API.o

threads.o:
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)/threads.c -o $(OBJ)/threads.o

misc.o:
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)/misc.c -o $(OBJ)/misc.o


