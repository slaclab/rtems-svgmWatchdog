WRAPPER_PATH = wrap
CFLAGS = -O -I$(WRAPPER_PATH)
CROSS = ccppc
CROSS_CFLAGS = -I$(WRAPPER_PATH) -DSYNERGYTARGET
# leave undefined if not installing vxWorks version TGTDIR = .
HOSTDIR = /afs/slac/g/spear/hostApps/@sys/bin/
RPCLIBS = -lrpcsvc  -lnsl

ifdef RTEMS_MAKEFILE_PATH
make_rtems = $(MAKE) -f Makefile.rtems $@
doch = echo haveit
else
make_rtems = echo 'Warning: RTEMS_MAKEFILE_PATH not set; not making RTEMS target'
doch = echo dont haveit
endif

# only make the RTEMS target if this makefile
# is called from the top directory (making all rtems apps)

ifdef RTEMS_CUSTOM
all:
	$(make_rtems)
else
all: wdclnt wd wd-vxworks.o
	$(make_rtems)
endif

install: all
ifdef TGTDIR
	install wd-vxworks.o $(TGTDIR)/wd.o
endif
ifndef RTEMS_CUSTOM
	install wdclnt	$(HOSTDIR)
endif
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

distclean: clean

blah:
	echo $(RTEMS_MAKEFILE_PATH)
	$(doch)
