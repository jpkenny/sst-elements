/**
Copyright 2009-2023 National Technology and Engineering Solutions of Sandia,
LLC (NTESS).  Under the terms of Contract DE-NA-0003525, the U.S. Government
retains certain rights in this software.

Sandia National Laboratories is a multimission laboratory managed and operated
by National Technology and Engineering Solutions of Sandia, LLC., a wholly
owned subsidiary of Honeywell International, Inc., for the U.S. Department of
Energy's National Nuclear Security Administration under contract DE-NA0003525.

Copyright (c) 2009-2023, NTESS

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Questions? Contact sst-macro-help@sandia.gov
*/

#include <mercury/components/nic.h>
//#include <sstmac/hardware/interconnect/interconnect.h>
#include <mercury/hardware/network/network_message.h>
#include <mercury/components/node.h>
#include <mercury/components/operating_system.h>
//#include <sstmac/common/event_manager.h>
//#include <sstmac/common/event_callback.h>
//#include <sstmac/common/stats/stat_spyplot.h>
//#include <sstmac/hardware/memory/memory_model.h>
//#include <sstmac/hardware/topology/topology.h>
//#include <sprockit/statics.h>
#include <sst/core/params.h>
//#include <sprockit/keyword_registration.h>
#include <mercury/common/util.h>

//RegisterDebugSlot(nic);

//RegisterNamespaces("nic", "message_sizes", "traffic_matrix",
//                   "message_size_histogram", "injection", "bytes");

//RegisterKeywords(
//{ "nic_name", "DEPRECATED: the type of NIC to use on the node" },
//{ "network_spyplot", "DEPRECATED: the file root of all stats showing traffic matrix" },
//{ "post_latency", "the latency of the NIC posting messages" },
//);

#define DEFAULT_NEGLIGIBLE_SIZE 256

namespace SST {
namespace Hg {

//static sprockit::NeedDeletestatics<NIC> del_statics;

void
NicEvent::serialize_order(serializer &ser)
{
  Event::serialize_order(ser);
  ser & msg_;
}

NIC::NIC(uint32_t id, SST::Params& params, Node* parent) :
  ConnectableSubcomponent(id, "nic", parent), 
  parent_(parent), 
  my_addr_(parent->addr()),
//  logp_link_(nullptr),
//  spy_bytes_(nullptr),
//  xmit_flows_(nullptr),
//  queue_(parent->os()),
  os_(parent->os())
{
  negligibleSize_ = params.find<int>("negligible_size", DEFAULT_NEGLIGIBLE_SIZE);
  //top_ = Topology::staticTopology(params);

  std::string subname = SST::Hg::sprintf("NIC.%d", my_addr_);
//  auto* spy = registerMultiStatistic<int,uint64_t>(params, "spy_bytes", subname);
//  //this might be a null statistic, dynamic cast to check
//  //no calls are made to this statistic unless it is non-null
//  //nullness checks are deferred to other places
//  spy_bytes_ = dynamic_cast<StatSpyplot<int,uint64_t>*>(spy);

//  xmit_flows_ = registerStatistic<uint64_t>(params, "xmit_flows", subname);
}

//void
//NIC::configureLogPLinks()
//{
//  initInputLink(addr(), hw::NIC::LogP);
//  initOutputLink(hw::NIC::LogP, addr());
//}

void
NIC::configureLinks()
{
  //set up LogP management/shortcut network
//  configureLogPLinks();

//  std::vector<Topology::InjectionPort> ports;
//  top_->injectionPorts(addr(), ports);
//  for (Topology::InjectionPort& port : ports){
//    initOutputLink(port.ep_port, port.switch_port);
//    initInputLink(port.switch_port, port.ep_port);
//  }
}

NIC::~NIC()
{
}

EventHandler*
NIC::mtlHandler() const
{
  return newHandler(const_cast<NIC*>(this), &NIC::mtlHandle);
}

void
NIC::mtlHandle(Event *ev)
{
//  nic_debug("MTL handle");
  NicEvent* nev = static_cast<NicEvent*>(ev);
  NetworkMessage* msg = nev->msg();
  delete nev;
  recvMessage(msg);
}

void
NIC::deleteStatics()
{
}

std::function<void(NetworkMessage*)>
NIC::ctrlIoctl()
{
  auto f = [=](NetworkMessage* msg){
    //this->sendManagerMsg(msg);
    this->injectSend(msg);
  };
  return f;
}

std::function<void(NetworkMessage*)>
NIC::dataIoctl()
{
  return std::bind(&NIC::injectSend, this, std::placeholders::_1);
}

void
NIC::injectSend(NetworkMessage* netmsg)
{
  if (netmsg->toaddr() == my_addr_){
    intranodeSend(netmsg);
  } else {
    netmsg->putOnWire();
    internodeSend(netmsg);
  }
}

void
NIC::recvMessage(NetworkMessage* netmsg)
{
//  nic_debug("handling %s:%lu of type %s from node %d while running",
//    netmsg->toString().c_str(),
//    netmsg->flowId(),
//    NetworkMessage::tostr(netmsg->type()),
//    int(netmsg->fromaddr()));

  switch (netmsg->type()) {
    case NetworkMessage::rdma_get_request: {
      netmsg->nicReverse(NetworkMessage::rdma_get_payload);
      netmsg->putOnWire();
      internodeSend(netmsg);
      break;
    }
    case NetworkMessage::nvram_get_request: {
      sst_hg_abort_printf("case NetworkMessage::nvram_get_request unimplemented\n");
//      netmsg->nicReverse(NetworkMessage::nvram_get_payload);
//      //internodeSend(netmsg);
//      parent_->handle(netmsg);
      break;
    }
    case NetworkMessage::failure_notification:
    case NetworkMessage::rdma_get_sent_ack:
    case NetworkMessage::payload_sent_ack:
    case NetworkMessage::rdma_put_sent_ack: {
      sst_hg_abort_printf("case NetworkMessage::rdma_put_sent_ack unimplemented\n");
//      //node_link_->send(netmsg);
//      parent_->handle(netmsg);
      break;
    }
    case NetworkMessage::rdma_get_nack:
    case NetworkMessage::rdma_get_payload:
    case NetworkMessage::rdma_put_payload:
    case NetworkMessage::nvram_get_payload:
    case NetworkMessage::smsg_send:
    case NetworkMessage::posted_send: {
      sst_hg_abort_printf("case NetworkMessage::posted_send unimplemented\n");
//      netmsg->takeOffWire();
//      parent_->handle(netmsg);
//      //node_link_->send(netmsg);
      break;
    }
    default: {
      sst_hg_throw_printf(SST::Hg::ValueError,
        "nic::handle: invalid message type %s: %s",
        NetworkMessage::tostr(netmsg->type()), netmsg->toString().c_str());
    }
  }
}

void
NIC::ackSend(NetworkMessage* payload)
{
  if (payload->needsAck()){
    NetworkMessage* ack = payload->cloneInjectionAck();
//    nic_debug("acking payload %s", payload->toString().c_str());
    sendToNode(ack);
  }
}

void
NIC::intranodeSend(NetworkMessage* payload)
{
//  nic_debug("intranode send payload %s", payload->toString().c_str());

  switch(payload->type())
  {
  case NetworkMessage::nvram_get_request:
    payload->nicReverse(NetworkMessage::nvram_get_payload);
    break;
  case NetworkMessage::rdma_get_request:
    payload->nicReverse(NetworkMessage::rdma_get_payload);
    break;
  default:
    break; //nothing to do
  }

//  MemoryModel* mem = parent_->mem();
  //use 64 as a negligible number of compute bytes
//  uint64_t byte_length = payload->byteLength();
//  if (byte_length > 64){
//    mem->accessFlow(payload->byteLength(),
//                TimeDelta(), //assume NIC can issue mem requests without delay
//                newCallback(this, &NIC::finishMemcpy, payload));
//  } else {
//    finishMemcpy(payload);
//  }
}

void
NIC::finishMemcpy(NetworkMessage* payload)
{
  ackSend(payload);
  payload->intranodeMemmove();
  sendToNode(payload);
}

void
NIC::recordMessage(NetworkMessage* netmsg)
{
//  nic_debug("sending message %lu of size %ld of type %s to node %d: "
//      "netid=%lu for %s",
//      netmsg->flowId(),
//      netmsg->byteLength(),
//      NetworkMessage::tostr(netmsg->type()),
//      int(netmsg->toaddr()),
//      netmsg->flowId(), netmsg->toString().c_str());

  if (netmsg->type() == NetworkMessage::null_netmsg_type){
    //assume this is a simple payload
    netmsg->setType(NetworkMessage::smsg_send);
  }

//  if (spy_bytes_){
//    spy_bytes_->addData(netmsg->toaddr(), netmsg->byteLength());
//  }
//  xmit_flows_->addData(netmsg->byteLength());
}

void
NIC::internodeSend(NetworkMessage* netmsg)
{
//  if (netmsg->toaddr() >= top_->numNodes()){
//    sst_hg_abort_printf("Got bad destination %d on NIC %d for %s",
//                      int(netmsg->toaddr()), int(addr()), netmsg->toString().c_str());
//  }

  recordMessage(netmsg);
//  nic_debug("internode send payload %llu of size %d %s",
//    netmsg->flowId(), int(netmsg->byteLength()), netmsg->toString().c_str());
  //we might not have a logp overlay network
//  if (negligibleSize(netmsg->byteLength())){
//    sendManagerMsg(netmsg);
//  } else {
//    doSend(netmsg);
//  }
  doSend(netmsg);
}

//void
//NIC::sendManagerMsg(NetworkMessage* msg)
//{
//  if (msg->toaddr() == my_addr_){
//    intranodeSend(msg);
//  } else {
//#if SSTMAC_SANITY_CHECK
//    if (!logp_link_){
//      spkt_abort_printf("NIC %d does not have LogP link", addr());
//    }
//#endif
//    logp_link_->send(new NicEvent(msg));
//    ackSend(msg);
//  }
//}

void
NIC::sendToNode(NetworkMessage* payload)
{
  auto forward_ev = newCallback(parent_, &Node::handle, payload);
  parent_->sendExecutionEventNow(forward_ev);
}

} // end of namespace Hg
} // end of namespace SST
