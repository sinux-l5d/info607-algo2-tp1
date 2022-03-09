SRC=union-find.c
CC=gcc
LD=gcc
# CFLAGS=-g -Wall -Werror -pedantic -Wno-deprecated-declarations -std=c11
CFLAGS=-g -Wall -Werror -pedantic -std=c11
LIBS=-lm
# Choisissez si vous préférez GTK2 ou GTK3
# gtk+-2.0 pour GTK2
# gtk+-3.0 pour GTK3 (choisi ici)
GTKCFLAGS:=-g $(shell pkg-config --cflags gtk+-3.0)
GTKLIBS:=$(shell pkg-config --libs gtk+-3.0)

all: union-find

union-find: union-find.o 
	$(LD) union-find.o $(GTKLIBS) $(LIBS) -lm -o union-find

union-find.o: $(SRC)
	$(CC) -c $(SRC) $(CFLAGS) $(GTKCFLAGS) -o union-find.o

clean:
	rm -f union-find union-find.o

fullclean: clean
	rm -f *~ *.fig.bak
