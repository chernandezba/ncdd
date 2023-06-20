obj-m += ncdd.o


all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
#	mkdir -f $(INSTALLDIR)
	cp ncdd.ko $(INSTALLDIR)

