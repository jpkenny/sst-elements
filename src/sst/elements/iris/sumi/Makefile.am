# -*- Makefile -*-
#
#

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
  allgather.cc \
  allgatherv.cc  \
  allreduce.cc \
  alltoall.cc \
  alltoallv.cc \
  bcast.cc \
  collective.cc \
  collective_actor.cc \
  collective_message.cc \
  communicator.cc \
  dense_rank_map.cc \
  gather.cc \
  gatherv.cc \
  message.cc \
  monitor.cc \
  reduce.cc \
  reduce_scatter.cc \
  scan.cc \
  scatter.cc \
  scatterv.cc  \
  sim_transport.cc \
  sumi.cc \
  sumi_thread.cc

library_includedir=$(includedir)/sst/elements/iris

nobase_library_include_HEADERS = \
  allgather.h \
  allgatherv.h \
  allreduce.h \
  alltoall.h \
  alltoallv.h \
  bcast.h \
  collective.h \
  collective_actor.h \
  collective_actor_fwd.h \
  collective_message.h \
  collective_message_fwd.h \
  comm_functions.h \
  communicator.h \
  communicator_fwd.h \
  dense_rank_map.h \
  gather.h \
  gatherv.h \
  message.h \
  message_fwd.h \
  monitor.h \
  network_id.h \
  options.h \
  reduce.h \
  reduce_scatter.h \
  scan.h \
  scatter.h \
  scatterv.h \
  sim_transport.h \
  sumi.h \
  sumi_thread.h \
  timeout.h \
  transport.h \
  transport_fwd.h

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
