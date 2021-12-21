INSTALL_PATH = $(HOME)/Descargas/SimGrid-3.25

CC = gcc

CPPFLAGS = -I$(INSTALL_PATH)/include -I/usr/local/include/


NO_PRAYER_FOR_THE_WICKED =	-w -O3 -g   

LDFLAGS = -L$(INSTALL_PATH)/lib/
LDLIBS = -lm -lsimgrid -rdynamic $(INSTALL_PATH)/lib/libsimgrid.so -Wl,-rpath,$(INSTALL_PATH)/lib


all: CFLAGS=$(NO_PRAYER_FOR_THE_WICKED)
all: aleatorio_fcfs aleatorio_sjf
.PHONY : all

aleatorio_fcfs: aleatorio_fcfs.o rand.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

aleatorio_sjf: aleatorio_sjf.o rand.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

clean:
	rm -f aleatorio_fcfs aleatorio_sjf *.o

.SUFFIXES:
.PHONY : clean
