#!/usr/bin/env python
#
# Copyright 2009-2023 NTESS. Under the terms
# of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Copyright (c) 2009-2023, NTESS
# All rights reserved.
#
# Portions are copyright of other developers:
# See the file CONTRIBUTORS.TXT in the top level directory
# of the distribution for more information.
#
# This file is part of the SST software package. For license
# information, see the LICENSE file in the top level directory of the
# distribution.

import sys
import sst
from sst.merlin.base import *
from sst.merlin import *

class HgJob(Job):
    def __init__(self, job_id, num_nodes, numCores = 1, nicsPerNode = 1):
        Job.__init__(self,job_id,num_nodes * nicsPerNode)
        self._declareClassVariables(["_numCores","_nicsPerNode","node","nic","os","_params","_numNodes"])

        self._numCores = numCores
        self._nicsPerNode = nicsPerNode
        self._numNodes = num_nodes

        self.node = HgNodeConfiguration()
        self.nic = HgNicConfiguration()

    def getName(self):
        return "HgJob"


    def build(self, nodeID, extraKeys):
        print("building nodeID=%d" % nodeID)
#        if self._check_first_build():
#            sst.addGlobalParams("loopback_params_%s"%self._instance_name,
#                            { "numCores" : self._numCores,
#                              "nicsPerNode" : self._nicsPerNode })
#
#           sst.addGlobalParam("params_%s"%self._instance_name, 'jobId', self.job_id)

        node = self.node.build(nodeID)

        os = node.setSubComponent("os_slot", "hg.operating_system")
        nic = node.setSubComponent("nic_slot", "hg.nic")
        #nic = self.nic.build(node,"nic_slot",0)

        # Build NetworkInterface
        #logical_id = self._nid_map[nodeID]
        logical_id = 0
        print("logical_id %s\n" % logical_id)
        networkif, port_name = self.network_interface.build(node,"link_control_slot",0,self.job_id,self.size,logical_id,False)

        print("got network interface with port_name %s\n" % port_name)

        return (networkif, port_name)

class HgNodeConfiguration(TemplateBase):

    def __init__(self):
        TemplateBase.__init__(self)

    def build(self,nID):
        node = sst.Component("node" + str(nID), "hg.simplenode")
        return node

class HgNicConfiguration(TemplateBase):

    def __init__(self):
        TemplateBase.__init__(self)
        self._subscribeToPlatformParamSet("network_interface")

#        # Set up all the parameters:
#        self._declareParams("main", [
#            "verboseLevel", "verboseMask",
#            "nic2host_lat",
#            "numVNs", "getHdrVN", "getRespLargeVN",
#            "getRespSmallVN", "getRespSize",
#            "rxMatchDelay_ns", "txDelay_ns",
#            "hostReadDelay_ns",
#            "tracedPkt", "tracedNode",
#            "maxSendMachineQsize", "maxRecvMachineQSize",
#            "numSendMachines", "numRecvNicUnits",
#            "messageSendAlignment", "nicAllocationPolicy",
#            "packetOverhead", "packetSize",
#            "maxActiveRecvStreams", "maxPendingRecvPkts",
#            "dmaBW_GBs", "dmaContentionMult",
#            ])

#        self._declareParams("main",["useSimpleMemoryModel"])
#        self.useSimpleMemoryModel = 0
#        self._lockVariable("useSimpleMemoryModel")
#        # Set up default parameters
#        self._subscribeToPlatformParamSet("nic")


#    def build(self,nID,num_vNics=1):
#        if self._check_first_build():
#            sst.addGlobalParams("params_%s"%self._instance_name,self._getGroupParams("main"))
#            sst.addGlobalParam("params_%s"%self._instance_name,"num_vNics",num_vNics)

#        nic = sst.Component("nic" + str(nID), "hg.nic")
#        self._applyStatisticsSettings(nic)
#        nic.addGlobalParamSet("params_%s"%self._instance_name)
#        nic.addParam("nid",nID)
#        #nic.addParam("num_vNics",num_vNics)
#        return nic, "rtrLink"

    def build(self,comp,slot,slot_num):
#        if self._check_first_build():
#            set_name = "params_%s"%self._instance_name
#            sst.addGlobalParams(set_name, self._getGroupParams("params"))
#            sst.addGlobalParam(set_name,"job_id",job_id)
#            sst.addGlobalParam(set_name,"job_size",job_size)
#            sst.addGlobalParam(set_name,"use_nid_remap",use_nid_remap)


        print("setSubComponent hg.nic on slot %s" % slot)
        sub = comp.setSubComponent(slot,"hg.nic",slot_num)
        #self._applyStatisticsSettings(sub)
        #sub.addGlobalParamSet("params_%s"%self._instance_name)
        #sub.addParam("logical_nid",logical_nid)
        return sub

#    def getVirtNicPortName(self, index):
#        if not self._nic:
#            print("Connecting Virtual Nic to firefly.nic before it has been built")
#            sys.exit(1)
#        return 'core'+str(x)
