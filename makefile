WRAPPER_PATH = /home/till/rtos/mps
CFLAGS = -O -I$(WRAPPER_PATH)
CROSS = ccppc
CROSS_CFLAGS = -I$(WRAPPER_PATH) -DSYNERGYTARGET
TGTDIR = /remote/synergy/test
RPCLIBS = -lrpcsvc  -lnsl

all: wd wd.target.o wdclnt

install: all
	install wd.target.o $(TGTDIR)/wd.o

# wd on the host
wd: wd.host.o
	$(CC) -o $@ $< $(RPCLIBS)

wd.host.o: wd.c wd.h
	$(CC) $(CFLAGS) -o $@ -c $<

# wd on target
wd.target.o: wd.c wd.h
	$(CROSS) $(CROSS_CFLAGS) $(CFLAGS) -DCPU=PPC604 -D__vxworks -DVXWORKS -I$(WIND_BASE)/target/h -o $@ -c $<

# wd client
wdclnt: wdclnt.c wd.h
	$(CC) $(CFLAGS) -o $@ $< $(RPCLIBS)

clean::
	$(RM) -f wdclnt wd wd.host.o wd.target.o wd.o
