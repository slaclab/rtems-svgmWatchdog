AUTOMAKE_OPTIONS=foreign

include $(top_srcdir)/rtems-pre.am

EXEEXT=$(OBJEXEEXT)

librtemsBspWatchdog_a_SOURCES       = wd.c wrap.h
librtemsBspWatchdog_a_SOURCES      += rtemsBspWatchdog.h

include_bsp_HEADERS                 = rtemsBspWatchdog.h

EXTRA_librtemsBspWatchdog_a_SOURCES = bsp_beatnik.c bsp_booke.c bsp_smic.c bsp_svgm.c bsp_uC5282.c

EXTRA_DIST                          = SvgmWatchdog.html

librtemsBspWatchdog_a_LIBADD        = $(WATCHDOG_BSP_SUPPORT).$(OBJEXT)
librtemsBspWatchdog_a_DEPENDENCIES  = $(WATCHDOG_BSP_SUPPORT).$(OBJEXT)

AM_CPPFLAGS                        += -I$(srcdir) -O2 -DTARGET

lib_LIBRARIES                       = librtemsBspWatchdog.a

if HAVE_CEXP
bin_PROGRAMS                    = watchdog
watchdog_SOURCES                = watchdog.modini.c
watchdog_LDADD                  = $(librtemsBspWatchdog_a_OBJECTS)
watchdog_DEPENDENCIES           = $(librtemsBspWatchdog_a_OBJECTS)

watchdog_LDADD                 += $(librtemsBspWatchdog_a_LIBADD)
watchdog_DEPENDENCIES          += $(librtemsBspWatchdog_a_DEPENDENCIES)

watchdog_LINK                   = $(OBJLINK)
endif


include $(top_srcdir)/rtems.am

HOSTPROG=wdclnt

EXTRA_PROGRAMS=$(HOSTPROG)
# ugly hack to avoid using $(EXEEXT)
# we MUST let 'configure' substitute something
# the HOSTPROG substitution evaluates to
# $(HOSTPROG)$(HOSTEXEEXT)
exechostbin_PROGRAMS = @HOSTPROGRAM@
wdclnt_SOURCES       = wdclnt.c rtemsBspWatchdog.h
wdclnt_LDADD_solaris = -lrpcsvc -lnsl
wdclnt_LDADD         = $(wdclnt_LDADD_$(build_os:solaris%=solaris))

wdclnt.$(OBJEXT): wdclnt.c rtemsBspWatchdog.h
	$(HOSTCC) -c -O2 -o $@ -I. -I.. -I$(top_builddir) -I$(srcdir)/wrap '-DPACKAGE_VERSION="$(PACKAGE_VERSION)"' $<

