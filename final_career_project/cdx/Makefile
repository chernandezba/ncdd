CC=gcc
MODCFLAGS= -Wall -DMODULE -D__KERNEL__ -DLINUX
CFLAGS=-Wall

PROGRAMS=cdx.o cdx_client cdx_client.o cdx_client_fill.o \
					cdx_servidor cdx_servidor.o cdx_servidor_fill.o cdx_util.o \
					prova1 prova2 prova3 prova4 prova5 prova6

all: $(PROGRAMS)
	echo "Compilacio completada."

cdx.o: cdx.h cdx.c
	$(CC) $(MODCFLAGS) -c cdx.c

cdx_client.o: cdx.h cdx_client.c
	$(CC) $(CFLAGS) -c cdx_client.c

cdx_client_fill.o: cdx.h cdx_xarxa.h cdx_util.h cdx_client_fill.c
	$(CC) $(CFLAGS) -c cdx_client_fill.c

cdx_client: cdx_client.o cdx_client_fill.o cdx_util.o
	$(CC) $(CFLAGS) cdx_client.o cdx_client_fill.o cdx_util.o -o cdx_client

cdx_servidor.o: cdx.h cdx_xarxa.h cdx_servidor.c
	$(CC) $(CFLAGS) -c cdx_servidor.c

cdx_servidor_fill.o: cdx.h cdx_xarxa.h cdx_util.h cdx_servidor_fill.c
	$(CC) $(CFLAGS) -c cdx_servidor_fill.c

cdx_servidor: cdx_servidor.o cdx_servidor_fill.o cdx_util.o
	$(CC) $(CFLAGS) cdx_servidor.o cdx_servidor_fill.o cdx_util.o -o cdx_servidor

cdx_util.o: cdx_util.c
	$(CC) $(CFLAGS) -c cdx_util.c

prova1: prova1.c
	$(CC) $(CFLAGS) prova1.c -o prova1

prova2: prova2.c
	$(CC) $(CFLAGS) prova2.c -o prova2

prova3: prova3.c
	$(CC) $(CFLAGS) prova3.c -o prova3

prova4: prova4.c
	$(CC) $(CFLAGS) prova4.c -o prova4

prova5: prova5.c
	$(CC) $(CFLAGS) prova5.c -o prova5

prova6: cdx.h prova6.c
	$(CC) $(CFLAGS) prova6.c -o prova6


clean:
	rm -f $(PROGRAMS)
	rm -f core
