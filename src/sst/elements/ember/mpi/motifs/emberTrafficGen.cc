// Copyright 2009-2023 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2023, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// of the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#include <sst_config.h>
#include <emberTrafficGen.h>

using namespace SST;
using namespace SST::Ember;

#define TAG 0xDEADBEEF

EmberTrafficGenGenerator::EmberTrafficGenGenerator(SST::ComponentId_t id,
                                                    Params& params) :
	EmberMessagePassingGenerator(id, params, "TrafficGen")
{
    m_pattern = params.find<std::string>("arg.pattern", "plusOne");

    m_messageSize = (uint32_t) params.find("arg.messageSize", 1024);
    m_maxMessageSize = (uint32_t) params.find("arg.maxMessageSize", pow(2,20));

    if (m_pattern == "plusOne") {
        m_sendBuf = memAlloc(m_messageSize);
        m_recvBuf = memAlloc(m_messageSize);
    }
    else {
        m_sendBuf = memAlloc(m_maxMessageSize);
        m_recvBuf = memAlloc(m_maxMessageSize);
    }

    if (m_pattern == "plusOne") {
        m_mean = params.find("arg.mean", 5000.0);
        m_stddev = params.find("arg.stddev", 300.0 );
        m_startDelay = params.find("arg.startDelay", .0 );
    }
    else {
        m_meanMessageSize = params.find("arg.messageSizeMean", 1024);
        m_stddevMessageSize = params.find("arg.messageSizeStdDev", 1024);
        m_meanComputeDelay = params.find("arg.computeDelayMean", 1024);
        m_stddevComputeDelay = params.find("arg.computeDelayStdDev", 1024);
    }

    configure();
}

void EmberTrafficGenGenerator::configure()
{
    if (m_pattern == "plusOne") {
        configure_plusOne();
        return;
    }

    m_rank = rank();

    m_distMessageSize = new SSTGaussianDistribution(
                m_mean, m_stddev, new RNG::MarsagliaRNG( rank(), getJobId() ) );
    m_distComputeDelay = new SSTGaussianDistribution(
                m_mean, m_stddev, new RNG::MarsagliaRNG( getJobId(), rank() ) );
    m_distPartner = new RNG::MarsagliaRNG( getJobId() + rank(), getJobId - rank() ) );
}

void EmberTrafficGenGenerator::configure_plusOne()
{
    assert( 2 == size() );

    m_random = new SSTGaussianDistribution( m_mean, m_stddev,
                        //new RNG::MarsagliaRNG( 11 + rank(), 79  ) );
                        new RNG::MarsagliaRNG( 11 + rank(), getJobId()  ) );

    if ( 0 == rank() ) {
        verbose(CALL_INFO, 1, 0, "startDelay %.3f ns\n",m_startDelay);
        verbose(CALL_INFO, 1, 0, "compute time: mean %.3f ns,"
        " stdDev %.3f ns\n", m_random->getMean(), m_random->getStandardDev());
        verbose(CALL_INFO, 1, 0, "messageSize %d\n", m_messageSize);
    }
}

bool EmberTrafficGenGenerator::generate( std::queue<EmberEvent*>& evQ)
{
    if (m_pattern == "plusOne") {
      return generate_plusOne(evQ);
    }

    else return generate_random(evQ);
}

bool EmberTrafficGenGenerator::generate_plusOne( std::queue<EmberEvent*>& evQ)
{
    double computeTime = m_random->getNextDouble();

    if ( computeTime < 0 ) {
        computeTime = 0.0;
    }
    verbose(CALL_INFO, 1, 0, "computeTime=%.3f ns\n", computeTime );
    enQ_compute( evQ, (computeTime + m_startDelay) * 1000 );
    m_startDelay = 0;

    int other = (rank() + 1) % 2;
    enQ_irecv( evQ, m_recvBuf, m_messageSize, CHAR, other, TAG,
                                                GroupWorld, &m_req );
    enQ_send( evQ, m_sendBuf, m_messageSize, CHAR, other, TAG, GroupWorld );
    enQ_wait( evQ, &m_req );

    return false;
}

bool EmberTrafficGenGenerator::generate_random( std::queue<EmberEvent*>& evQ)
{
    evQ_ = &evQ;
    if (m_generateLoopIndex == 0) {
        memSetBacked();
        memAlloc(&m_sizeSendMemaddr, sizeofDataType(UINT64_T));
        memAlloc(&m_sizeRecvMemaddr, sizeofDataType(UINT64_T));

        // post receive for data requests
        recv_datareq();

        // send our first data request
        send_datareq();

        m_needToWait = true;
        ++m_generateLoopIndex;
        return false;
    }

    if (m_needToWait) {
        wait_for_any();
        m_needToWait = false;
        ++m_generateLoopIndex;
        return false;
    }

    // setup request indexes
    int datareq_recv_index = -1;
    int data_recv_index = -1;
    if (m_dataReqRecvActive) datareq_recv_index = 0;
    if (m_dataRecvActive) data_recv_index = datareq_recv_index + 1;

    // If we have a data request then send data
    if (m_requestIndex == datareq_recv_index) {

        m_dataReqRecvActive = false;
        int requestor = m_anyResponse.src;
        uint64_t size = m_sizeRecvMemaddr.at<uint64_t>(0);
        if (m_debug > 1) std::cerr << "rank " << m_rank << " sending " << size << " to " << requestor << std::endl;
        MessageRequest *send_req = new MessageRequest();
        m_sendRequests.push_back(send_req);
        enQ_isend( evQ, nullptr, size, UINT64_T, requestor, DATA, GroupWorld, send_req );
        // prepare for next request
        recv_datareq();
    }

    else if (m_requestIndex == m_dataRecvActive) {
        if (m_debug > 1) std::cerr << "rank " << m_rank << " received data from " << m_anyResponse.src << std::endl;
        compute();
        send_datareq();
        // make send requests go away
        m_testSends = true;
        }
    }
}

void EmberTrafficGenGenerator::recv_datareq() {
  std::queue<EmberEvent*>& evQ = *evQ_;
  if (m_debug > 1) std::cerr << "rank " << m_rank << " start a datareq recv\n";
  enQ_irecv( evQ, &m_sizeRecvMemaddr, 1, UINT64_T, Hermes::MP::AnySrc, DATA_REQUEST, GroupWorld, &m_dataReqRecvRequest);
  m_dataReqRecvActive = true;
}

void EmberTrafficGenGenerator::send_datareq() {
  std::queue<EmberEvent*>& evQ = *evQ_;

  // send a data request

  // determine rank to request data from
  int partner = m_distPartner->generateNextUInt32() % size();

  // determine size of data
  int m_dataReqSize = int( abs( m_distMessageSize->getNextDouble() ) );
  if (m_dataReqSize < 1) m_dataReqSize = 1;
  if (m_dataReqSize > m_maxMessageSize) m_dataReqSize = m_maxMessageSize;
  uint64_t* send_buffer = (uint64_t*) m_sizeSendMemaddr.getBacking();
  *send_buffer = m_dataReqSize;

  // send the request
  enQ_send( evQ, m_sizeSendMemaddr, m_messageSize, UINT64_T, partner, DATA_REQUEST, GroupWorld);

  // fire off a recv for the data
  if (m_debug > 1) std::cerr << "rank " << m_rank << " start a datarecv\n";
  enQ_irecv( evQ, m_recvBuffer, 1, UINT64_T, partner, DATA, GroupWorld, m_dataRecvRequest );
  m_dataRecvActive = true;
}

void EmberTrafficGenGenerator::wait_for_any() {
  std::queue<EmberEvent*>& evQ = *evQ_;

  uint64_t size = m_dataReqRecvActive + m_dataRecvActive;
  if (m_debug > 1) std::cerr << "rank " << m_rank <<  " enqueing waitany with size " << size << std::endl;
  if (m_generateLoopIndex > 1) delete m_allRequests;
  m_allRequests = new MessageRequest[size];
  uint64_t index=0;
  if (m_dataReqRecvActive) {
    if (m_debug > 2) std::cerr << "copy dataReqRecvRequest " << m_dataReqRecvRequest << " to " << index << std::endl;
    m_allRequests[index] = m_dataReqRecvRequest;
    ++index;
  }
  if (m_dataRecvActive) {
    if (m_debug > 2) std::cerr << "copy dataReqRecvRequest " << m_dataRecvRequest << " to " << index << std::endl;
    m_allRequests[index] = m_dataRecvRequest;
  }
  enQ_waitany(evQ, size, m_allRequests, &m_requestIndex, &m_anyResponse);
}

void EmberTrafficGenGenerator::compute() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    int delay = int( abs( m_distComputeDelay->getNextDouble() ) );
    if (delay < 0) delay = 1;
    enQ_compute( evQ, delay);
    return;
}
