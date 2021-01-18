obj-m := jr3pci-driver.o


#CFLAGS_jr3pci-driver.o := -DDEBUG

all: app
	make -C /lib/modules/`uname -r`/build M=`pwd` modules

app.o:
	gcc -Wall -pedantic -c app.c

jr3API.o:
	gcc -Wall -pedantic -c jr3API.c

app: app.o jr3API.o
	gcc -Wall -pedantic app.o jr3API.o -o app

clean:
	make -C /lib/modules/`uname -r`/build M=`pwd` clean
	rm -f *.o app

