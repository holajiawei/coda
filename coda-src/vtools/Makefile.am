## Process this file with automake to produce Makefile.in

if BUILD_CLIENT
bin_PROGRAMS = codacon cfs cmon coda_replay hoard spy mkcodabf
dist_man_MANS = cfs.1 cmon.1 coda_replay.1 hoard.1 spy.1 mkcodabf.1
if HAVE_PYTHON
bin_SCRIPTS = gcodacon
endif
endif
sbin_PROGRAMS = codaconfedit

codacon_SOURCES	    = codacon.cc
cfs_SOURCES	    = cfs.cc
cmon_SOURCES	    = cmon.cc
coda_replay_SOURCES = coda_replay.cc coda_replay.h
hoard_SOURCES	    = hoard.cc
spy_SOURCES	    = spy.cc
EXTRA_DIST = logbandwidth.in logcmls.in logreintegration.in logprogress.in
CLEANFILES = $(bin_SCRIPTS)

AM_CPPFLAGS = $(RPC2_CFLAGS) \
	      -I$(top_srcdir)/lib-src/base \
	      -I$(top_srcdir)/coda-src/kerndep \
	      -I$(top_srcdir)/coda-src/util \
	      -I$(top_srcdir)/coda-src/vicedep \
	      -I$(top_builddir)/coda-src/vicedep \
	      -I$(top_srcdir)/coda-src/al \
	      -I$(top_srcdir)/coda-src/partition \
	      -I$(top_srcdir)/coda-src/vv \
	      -I$(top_srcdir)/coda-src/venus

codacon_LDADD = $(top_builddir)/lib-src/base/libbase.la \
		$(RPC2_LIBS)

cfs_LDADD = $(top_builddir)/coda-src/vv/libvv.la \
	    $(top_builddir)/coda-src/kerndep/libkerndep.la \
	    $(top_builddir)/lib-src/base/libbase.la

cmon_LDADD = $(top_builddir)/coda-src/vicedep/libvenusdep.la \
	     $(top_builddir)/lib-src/base/libbase.la \
	     $(LIBCURSES) $(RPC2_LIBS)

hoard_LDADD = $(top_builddir)/coda-src/util/libutil.la \
	      $(top_builddir)/coda-src/kerndep/libkerndep.la \
	      $(top_builddir)/lib-src/base/libbase.la

spy_LDADD = $(top_builddir)/lib-src/base/libbase.la \
	    $(RPC2_LIBS)

codaconfedit_LDADD = $(top_builddir)/lib-src/base/libbase.la

