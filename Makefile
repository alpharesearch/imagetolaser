CFLAGS= -O -Wall -g
LDFLAGS= -lm

imagecarve: imagetolaser.o 


indent:
	indent -br -npcs *.c
	
clean:
	rm imagecarve.o

