# -*- Makefile -*-
#
#

AM_CPPFLAGS += \
  -I$(top_srcdir)/src/sst/elements \
  -I$(top_srcdir)/src/sst/elements/mercury \
  -I$(top_srcdir)/src/sst/elements/iris \
  -I$(top_srcdir)/src/sst/elements/iris/libfabric/prov \
  -I$(top_srcdir)/src/sst/elements/iris/libfabric/prov/include \
  -I$(top_srcdir)/src/sst/elements/iris/libfabric/prov/sstmac/include \
  -I$(top_srcdir)/src/sst/elements/iris/libfabric/prov/hook/include/ \
  -I$(top_srcdir)/src/sst/elements/iris/libfabric/include \
  -DSSTMAC_INTEGRATED_SST_CORE=1

sstmac_sources = \
  prov/sprockit/debug.cc \
  prov/sprockit/keyword_registration.cc \
  prov/sprockit/output.cc \
  prov/sprockit/statics.cc \
  prov/sstmac/common/event_scheduler.cc \
  prov/sstmac/common/thread_lock.cc \
  prov/sstmac/common/timestamp.cc \
  prov/sstmac/common/stats/ftq_tag.cc \
  prov/sstmac/hardware/memory/memory_model.cc \
  prov/sstmac/hardware/network/network_message.cc \
  prov/sstmac/hardware/nic/nic.cc \
  prov/sstmac/hardware/node/node.cc \
  prov/sstmac/software/api/api.cc \
  prov/sstmac/software/launch/app_launcher.cc \
  prov/sstmac/software/launch/job_launcher.cc \
  prov/sstmac/software/libraries/library.cc \
  prov/sstmac/software/process/operating_system.cc \
  prov/sstmac/software/threading/stack_alloc.cc \
  prov/sstmac/src/sstmac_av.cc \
  prov/sstmac/src/sstmac_cm.cc \
  prov/sstmac/src/sstmac_cntr.cc \
  prov/sstmac/src/sstmac_cq.cc \
  prov/sstmac/src/sstmac_dom.cc \
  prov/sstmac/src/sstmac_ep.cc \
  prov/sstmac/src/sstmac_eq.cc \
  prov/sstmac/src/sstmac_fabric.cc \
  prov/sstmac/src/sstmac_mr.cc \
  prov/sstmac/src/sstmac_poll.cc \
  prov/sstmac/src/sstmac_sep.cc \
  prov/sstmac/src/sstmac_wait.cc \
  prov/sstmac/src/sstmac_sumi.cpp
        
common_sources = \
  src/log.c \
  src/common.c \
  src/rbtree.c \
  src/indexer.c \
  src/perf.c \
  src/mem.c \
  src/iov.c \
  src/shared/ofi_str.c \
  src/abi_1_0.c \
  src/unix/osd.c \
  src/enosys.c \
  src/fabric.c \
  src/fasthash.c \
  src/var.c \
  src/fi_tostr.c \
  src/tree.c \
  prov/util/src/util_atomic.c \
  prov/util/src/util_attr.c \
  prov/util/src/util_av.c \
  prov/util/src/util_cq.c \
  prov/util/src/util_cntr.c \
  prov/util/src/util_domain.c \
  prov/util/src/util_ep.c \
  prov/util/src/util_pep.c \
  prov/util/src/util_eq.c \
  prov/util/src/util_fabric.c \
  prov/util/src/util_main.c \
  prov/util/src/util_poll.c \
  prov/util/src/util_wait.c \
  prov/util/src/util_buf.c \
  prov/util/src/util_mr_map.c \
  prov/util/src/util_ns.c \
  prov/util/src/util_shm.c \
  prov/util/src/util_mem_monitor.c \
  prov/util/src/util_mem_hooks.c \
  prov/util/src/util_mr_cache.c \
  prov/util/src/util_coll.c

libfabric_sources = \
  prov/hook/src/hook_cm.c \
  prov/hook/src/hook_cq.c \
  prov/hook/src/hook_xfer.c \
  prov/hook/src/hook_ep.c \
  prov/hook/src/hook_cntr.c \
  prov/hook/src/hook_av.c \
  prov/hook/src/hook.c \
  prov/hook/src/hook_wait.c \
  prov/hook/src/hook_domain.c \
  prov/hook/src/hook_eq.c

compdir = $(pkglibdir)
comp_LTLIBRARIES = libfabric.la
libfabric_la_SOURCES = \
  $(sstmac_sources) \
  $(common_sources) \
  $(libfabric_sources)

libfabric_la_LIBADD = \
  -L$(pkglibdir) \
  -lhg \
  -lsumi
