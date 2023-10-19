#!/usr/bin/env python
#
# Copyright 2009-2023 NTESS. Under the terms
# of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Copyright (c) 2009-2023, NTESS
# All rights reserved.
#
# This file is part of the SST software package. For license
# information, see the LICENSE file in the top level directory of the
# distribution.

import sst
from sst.merlin.base import *
from sst.merlin.endpoint import *
from sst.merlin.interface import *
from sst.merlin.topology import *
from sst.hg import *

if __name__ == "__main__":

    PlatformDefinition.loadPlatformFile("platform_file_hg_mpi_test")
    PlatformDefinition.setCurrentPlatform("platform_hg_mpi_test")

    topo = topoSingle()
    topo.link_latency = "20ns"
    topo.num_ports = 32

    # Set up the routers
    #router = hr_router()
    #router.link_bw = "12GB/s"
    #router.flit_size = "8B"
    #router.xbar_bw = "50GB/s"
    #router.input_latency = "40ns"
    #router.output_latency = "40ns"
    #router.input_buf_size = "16kB"
    #router.output_buf_size = "16kB"
    #router.num_vns = 1
    #router.xbar_arb = "merlin.xbar_arb_lru"

    ep = HgJob(0,2)

    system = System()
    system.setTopology(topo)
    system.allocateNodes(ep,"linear")

    system.build()
