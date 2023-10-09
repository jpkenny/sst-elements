# -*- Makefile -*-
#
#

SUBDIRS = libfabric

AM_CPPFLAGS += \
	$(MPI_CPPFLAGS) \
        -I$(top_srcdir)/src/sst/elements/iris \
        -I$(top_srcdir)/src/sst/elements/mercury \
        -I$(top_srcdir)/src/sst/elements

# unpleasant hack to make vintage automake (e.g. 1.13.4) work
AM_LIBTOOLFLAGS = --tag=CXX

compdir = $(pkglibdir)
comp_LTLIBRARIES = libsumi.la
libsumi_la_SOURCES = \
  sumi/message.cc \
  sumi/sim_transport.cc

library_includedir=$(includedir)/sst/hg

nobase_library_include_HEADERS = \
  sumi/message.h \
  sumi/message_fwd.h \
  sumi/null_buffer.h \
  sumi/sim_transport.h \
  sumi/transport.h

if !SST_ENABLE_PREVIEW_BUILD
libsumi_la_SOURCES += $(deprecated_libsumi_sources)
endif

EXTRA_DIST =
deprecated_EXTRA_DIST =

if !SST_ENABLE_PREVIEW_BUILD
EXTRA_DIST += $(deprecated_EXTRA_DIST)
endif

libsumi_la_LDFLAGS = -module -avoid-version

install-exec-hook:
	$(SST_REGISTER_TOOL) SST_ELEMENT_SOURCE     iris=$(abs_srcdir)

# This sed script converts 'od' output to a comma-separated list of byte-
# values, suitable for #include'ing into an array definition.
# This can be done much more simply with xxd or hexdump, but those tools
# are not installed by default on all supported platforms.
#
# od:   -v:             Print all data
#               -t x1:  Print as byte-values, in hex
# sed:  Script 1:  Remove base-address column from od output
#               Script 2:  Remove trailing blank line resulting from script 1
#               Script 3:  Add '0x' prefix, and ',' suffix to each value
%.inc: %.py
	od -v -t x1 < $< | sed -e 's/^[^ ]*[ ]*//g' -e '/^\s*$$/d' -e 's/\([0-9a-f]*\)[ $$]*/0x\1,/g' > $@

##########################################################################
##########################################################################
##########################################################################
