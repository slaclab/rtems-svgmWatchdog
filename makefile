WRAPPER_PATH = /home/till/rtos/mps
CFLAGS = -O -I$(WRAPPER_PATH)
CROSS = vxppc-gcc
CROSS_CFLAGS = $(CFLAGS) -I$(WRAPPER_PATH) -DSYNERGYTARGET
TGTDIR = /remote/synergy/test

all: wd wd.target.o wdclnt

install: all
	install wd.target.o $(TGTDIR)/wd.o

# wd on the host
wd: wd.host.o
	$(CC) -o $@ $<

wd.host.o: wd.c wd.h
	$(CC) $(CFLAGS) -o $@ -c $<

# wd on target
wd.target.o: wd.c wd.h
	$(CROSS) $(CROSS_CFLAGS)  -DVXWORKS -o $@ -c $<

# wd client
wdclnt: wdclnt.c wd.h
	$(CC) $(CFLAGS) -o $@ $<

clean::
	$(RM) -f wdclnt wd wd.host.o wd.target.o wd.o
