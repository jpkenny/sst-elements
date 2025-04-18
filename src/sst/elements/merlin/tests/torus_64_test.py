#!/usr/bin/env python
#
# Copyright 2009-2025 NTESS. Under the terms
# of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Copyright (c) 2009-2025, NTESS
# All rights reserved.
#
# This file is part of the SST software package. For license
# information, see the LICENSE file in the top level directory of the
# distribution.

import sst
from sst.merlin import *

if __name__ == "__main__":

    topo = topoTorus()
    endPoint = TestEndPoint()


    sst.merlin._params["torus.shape"] = "4x4x4"
    sst.merlin._params["torus.width"] = "1x1x1"
    sst.merlin._params["torus.local_ports"] = "1"
    sst.merlin._params["num_dims"] = "3"


    sst.merlin._params["link_bw"] = "4GB/s"
    sst.merlin._params["link_lat"] = "20ns"
    sst.merlin._params["flit_size"] = "8B"
    sst.merlin._params["xbar_bw"] = "4GB/s"
    sst.merlin._params["input_latency"] = "20ns"
    sst.merlin._params["output_latency"] = "20ns"
    sst.merlin._params["input_buf_size"] = "4kB"
    sst.merlin._params["output_buf_size"] = "4kB"

    #sst.merlin._params["checkerboard"] = "1"
    sst.merlin._params["xbar_arb"] = "merlin.xbar_arb_lru"

    topo.prepParams()
    endPoint.prepParams()
    topo.setEndPoint(endPoint)
    topo.build()

    #sst.setStatisticLoadLevel(9)

    #sst.setStatisticOutput("sst.statOutputCSV");
    #sst.setStatisticOutputOptions({
    #    "filepath" : "stats.csv",
    #    "separator" : ", "
    #})

    #endPoint.enableAllStatistics("0ns")

    #sst.enableAllStatisticsForComponentType("merlin.hr_router", {"type":"sst.AccumulatorStatistic","rate":"0ns"})
