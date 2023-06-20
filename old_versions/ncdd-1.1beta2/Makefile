CC=gcc
MODCFLAGS= -Wall -DMODULE -D__KERNEL__ -DLINUX
CFLAGS=-Wall -ggdb
KERNDIR=/lib/modules/$(shell uname -r)
INSTALLDIR=$(KERNDIR)/misc
INITDIR=/etc/rc.d/init.d
MANPATH=/usr/share/man/es/man8

PROGRAMS=ncdd.o ncdd_client ncdd_client.o ncdd_client_child.o \
					ncdd_server ncdd_server.o ncdd_server_child.o ncdd_util.o \
					prova1 prova2 prova3 prova4 prova5 prova6 prova7 prova8 prova9 prova10

all: $(PROGRAMS)
	echo "Compilacio completada."

ncdd.o: ncdd.h ncdd.c
	$(CC) $(MODCFLAGS) -c ncdd.c

ncdd_client.o: ncdd.h ncdd_client.c
	$(CC) $(CFLAGS) -c ncdd_client.c

ncdd_client_child.o: ncdd.h ncdd_net.h ncdd_util.h ncdd_client_child.c
	$(CC) $(CFLAGS) -c ncdd_client_child.c

ncdd_client: ncdd_client.o ncdd_client_child.o ncdd_util.o
	$(CC) $(CFLAGS) ncdd_client.o ncdd_client_child.o ncdd_util.o -o ncdd_client

ncdd_server.o: ncdd.h ncdd_net.h ncdd_server.c
	$(CC) $(CFLAGS) -c ncdd_server.c

ncdd_server_child.o: ncdd.h ncdd_net.h ncdd_util.h ncdd_server_child.c
	$(CC) $(CFLAGS) -c ncdd_server_child.c

ncdd_server: ncdd_server.o ncdd_server_child.o ncdd_util.o
	$(CC) $(CFLAGS) ncdd_server.o ncdd_server_child.o ncdd_util.o -o ncdd_server

ncdd_util.o: ncdd_util.c
	$(CC) $(CFLAGS) -c ncdd_util.c

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

prova6: ncdd.h prova6.c
	$(CC) $(CFLAGS) prova6.c -o prova6

prova7: prova7.c
	$(CC) $(CFLAGS) prova7.c -o prova7

prova8: prova8.c
	$(CC) $(CFLAGS) prova8.c -o prova8

prova9: prova9.c
	$(CC) $(CFLAGS) prova9.c -o prova9

prova10: prova10.c
	$(CC) $(CFLAGS) prova10.c -o prova10


clean:
	rm -f $(PROGRAMS)
	rm -f core

install: ncdd.o ncdd_client ncdd_server
	cp ncdd_server /sbin
	chmod 700 /sbin/ncdd_server
	cp ncdd_client /sbin
	chmod 700 /sbin/ncdd_client
	cp ncdd.o $(INSTALLDIR)
	cp scripts/ncdd_client $(INITDIR)
	cp scripts/ncdd_server $(INITDIR)
	chkconfig --add ncdd_client
	chkconfig --add ncdd_server
	cp ncdd_client.8 $(MANPATH)
	cp ncdd_server.8 $(MANPATH)

dispositius:
	cd /dev
	mkdir ncdd
	cd ncdd

	mknod ncdd c 254 250

	mkdir localhost
	cd localhost

	mknod dsp c 254 0
	mknod lp0 c 254 1
	mknod fb0 c 254 2
	mknod zero c 254 3

	echo "Dispositius creats."
