#######################################################################
##
## Makefile.am for proc_police
##
#######################################################################

ACLOCAL_AMFLAGS = -I m4 -I src

AM_CXXFLAGS = \
	$(LCMAPS_CFLAGS) \
	-Wno-write-strings \
	$(CXX0X_CFLAGS)

EXTRA_DIST = bootstrap lcmaps-plugins-condor-update.spec

plugindir = $(MODULEDIR)
plugin_LTLIBRARIES = \
        liblcmaps_condor_update.la

liblcmaps_condor_update_la_SOURCES = \
	src/lcmaps_condor_update.c \
	src/condor_discovery.cxx \
	src/condor_discovery.h

liblcmaps_condor_update_la_LDFLAGS = -avoid-version

install-data-hook:
	( \
	cd $(DESTDIR)$(plugindir); \
	for mod in condor_update; do \
	    rm -f lcmaps_$$mod.mod; \
	    ln -s liblcmaps_$$mod.so.0.0.0 lcmaps_$$mod.mod; \
	done \
	)

