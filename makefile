WRAPPER_PATH = wrap
CFLAGS = -O -I$(WRAPPER_PATH)
CROSS = ccppc
CROSS_CFLAGS = -I$(WRAPPER_PATH) -DSYNERGYTARGET
# leave undefined if not installing vxWorks version TGTDIR = .
HOSTDIR = /afs/slac/g/spear/hostApps/@sys/bin/
RPCLIBS = -lrpcsvc  -lnsl

ifdef RTEMS_MAKEFILE_PATH
make_rtems = $(MAKE) -f Makefile.rtems $@
else
make_rtems = echo 'Warning: RTEMS_MAKEFILE_PATH not set; not making RTEMS target'
endif

all: wdclnt wd wd-vxworks.o
	$(make_rtems)

install: all
ifdef TGTDIR
	install wd-vxworks.o $(TGTDIR)/wd.o
endif
	install wdclnt	$(HOSTDIR)
	$(make_rtems)

# wd on the host
wd: wd.host.o
	$(CC) -o $@ $< $(RPCLIBS)

wd.host.o: wd.c wd.h
	$(CC) $(CFLAGS) -o $@ -c $<

# wd on target
wd-vxworks.o: wd.c wd.h
ifdef WIND_BASE
	$(CROSS) $(CROSS_CFLAGS) $(CFLAGS) -DCPU=PPC604 -D__vxworks -DVXWORKS -I$(WIND_BASE)/target/h -o $@ -c $<
else
	echo 'Warning: WIND_BASE not defined; not making vxWorks target'
endif

# wd client
wdclnt: wdclnt.c wd.h
	$(CC) $(CFLAGS) -o $@ $< $(RPCLIBS)

clean::
	$(RM) -f wdclnt wd *.o
	$(make_rtems)
