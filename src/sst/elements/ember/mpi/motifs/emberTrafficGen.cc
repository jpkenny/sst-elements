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
#include "emberTrafficGen.h"
#include <cstdlib>

using namespace SST;
using namespace SST::Ember;

#define TAG 0xDEADBEEF

EmberTrafficGenGenerator::EmberTrafficGenGenerator(SST::ComponentId_t id,
                                                    Params& params) :
    EmberMessagePassingGenerator(id, params, "TrafficGen"), m_generateLoopIndex(0), m_needToWait(false), m_currentTime(0),
    m_rankBytes(0), m_totalBytes(0), m_stopped(false)
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
        m_debug = params.find<int>("arg.debugLevel",0);
        m_meanMessageSize = params.find("arg.messageSizeMean", 1024);
        m_stddevMessageSize = params.find("arg.messageSizeStdDev", 1024);
        m_computeDelay = params.find("arg.computeDelay", 100);
        m_iterations = params.find<unsigned int>("arg.iterations", 1000);
        m_stopTime = params.find<unsigned int>("arg.stopTimeUs", 100);
        m_hotSpots = params.find<unsigned int>("arg.hotSpots",0);
        m_hotSpotsRatio = params.find<unsigned int>("arg.hotSpotsRatio",99);
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
                m_meanMessageSize, m_stddevMessageSize, new RNG::MarsagliaRNG( 11 + rank(), RAND_MAX / (rank() + 1) ) );

    std::srand(1); // want same hotRanks on every node
    if (m_hotSpots) {
        while (m_hotRanks_set.size() < m_hotSpots) {
            uint32_t rank = std::rand() % size();
            m_hotRanks_set.insert( rank );
        }
        std::copy(m_hotRanks_set.begin(), m_hotRanks_set.end(), std::back_inserter(m_hotRanks));

        m_hotCounterInitial = m_hotSpotsRatio;
        m_hotCounter = m_hotCounterInitial;
    }

    memSetBacked();
    m_sizeSendMemaddr = memAlloc(sizeofDataType(UINT64_T));
    m_sizeRecvMemaddr = memAlloc(sizeofDataType(UINT64_T));
    m_rankBytes = memAlloc(sizeofDataType(UINT64_T));
    m_totalBytes = memAlloc(sizeofDataType(UINT64_T));
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
    if (m_pattern == "plusOne") return generate_plusOne(evQ);
    return generate_random(evQ);
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

    if(m_stopped) {
        if (m_rank == 0) {
            uint64_t bytes = m_totalBytes.at<uint64_t>(0);
            std::cerr << "Total system observed bandwidth: " << (double) bytes / (double) m_stopTime * (double) 1e-3  << " GB/s\n";
        }
        return true;
    }

    if (m_rank == 0 && m_debug > 0) std::cerr << "Simulation time: " << m_currentTime / 1000 << " us" << std::endl;
    if (m_debug > 2) std::cerr << "rank " << m_rank << " entering loop " << m_generateLoopIndex << std::endl;

    if (m_generateLoopIndex == 0) {

        // post receive for data requests
        recv_datareq();
        if (m_rank != 0) recv_stop();
        else enQ_getTime( evQ, &m_startTime );

        // send our first data request
        send_datareq();

        m_needToWait = true;
        ++m_generateLoopIndex;
        return false;
    }

    m_currentTime = getCurrentSimTimeMicro();
    if ((m_rank == 0) && (m_currentTime > m_stopTime)) {
        if (m_debug > 2) std::cerr << "rank " << m_rank << " sending stop messages at time=" << m_currentTime << "us\n";
        send_stop();
        ++m_generateLoopIndex;
        enQ_reduce( evQ, m_rankBytes, m_totalBytes, 1, UINT64_T, Hermes::MP::SUM, 0, GroupWorld );
        m_stopped = true;
        return false;
    }

    if (m_needToWait) {
        if (m_debug > 2) std::cerr << "rank " << m_rank << " waiting for any request\n";
        wait_for_any();
        m_needToWait = false;
        ++m_generateLoopIndex;
        return false;
    }

    if (m_debug > 2) std::cerr << "rank " << m_rank << " got request " << m_requestIndex << std::endl;

    // setup request indexes
    int datareq_recv_index = -1;
    int data_recv_index = -1;
    if (m_dataReqRecvActive) datareq_recv_index = 0;
    if (m_dataRecvActive) data_recv_index = datareq_recv_index + 1;
    int stop_index = data_recv_index + 1;

    // Time to stop?
    if (m_requestIndex == stop_index) {
        if (m_debug > 1) std::cerr << "rank " << m_rank << " stopping with bytes " << m_rankBytes.at<uint64_t>(0) << std::endl;
        enQ_reduce( evQ, m_rankBytes, m_totalBytes, 1, UINT64_T, Hermes::MP::SUM, 0, GroupWorld );
        m_stopped = true;
        return false;
    }

    // If we have a data request then send data
    if (m_requestIndex == datareq_recv_index) {
        if (m_debug > 2) std::cerr << "rank " << m_rank << " got a data request\n";
        m_dataReqRecvActive = false;
        int requestor = m_anyResponse.src;
        uint64_t size = m_sizeRecvMemaddr.at<uint64_t>(0);
        if (m_debug > 0) std::cerr << size << " from rank " << m_rank << " to rank " << requestor << std::endl;
        MessageRequest *send_req = new MessageRequest();
        m_sendRequests.push_back(send_req);
        enQ_isend( evQ, nullptr, size, UINT64_T, requestor, DATA, GroupWorld, send_req );
        // prepare for next request
        recv_datareq();
    }

    else if (m_requestIndex == data_recv_index) {
        if (m_debug > 0) std::cerr << "rank " << m_rank << " received data from " << m_anyResponse.src << std::endl;
        m_currentTime = getCurrentSimTimeMicro();
        if (m_currentTime < m_stopTime) {
            if (m_debug > 2) std::cerr << "rank " << m_rank << " accumulating " << m_dataSize << " bytes at time " << m_currentTime << std::endl;
            uint64_t* bytes = (uint64_t*) m_rankBytes.getBacking();
            *bytes += m_dataSize;
        }
        compute();
        enQ_getTime( evQ, &m_currentTime);
        send_datareq();
        // make send requests go away
        m_testSends = true;
    }

    m_needToWait = true;
    ++m_generateLoopIndex;
    return false;

}

void EmberTrafficGenGenerator::recv_datareq() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    if (m_debug > 2) std::cerr << "rank " << m_rank << " start a datareq recv\n";
    enQ_irecv( evQ, m_sizeRecvMemaddr, 1, UINT64_T, Hermes::MP::AnySrc, DATA_REQUEST, GroupWorld, &m_dataReqRecvRequest);
    m_dataReqRecvActive = true;
}

void EmberTrafficGenGenerator::recv_stop() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    enQ_irecv( evQ, nullptr, 1, UINT64_T, 0, STOP, GroupWorld, &m_stopRequest);
}

void EmberTrafficGenGenerator::send_datareq() {
    std::queue<EmberEvent*>& evQ = *evQ_;

    // send a data request

    // determine rank to request data from
    uint32_t partner = (uint32_t) m_rank;
    if (m_hotSpots && !m_hotCounter && !m_hotRanks_set.count(m_rank) ) {
        partner = m_hotRanks[std::rand() % m_hotSpots];
    }
    else {
        while (partner == m_rank)
            partner = std::rand() % size();
    }
    if (m_hotSpots) {
        if (m_hotCounter) --m_hotCounter;
        else m_hotCounter = m_hotCounterInitial;
    }

    // determine size of data
    m_dataSize = int( abs( m_distMessageSize->getNextDouble() ) );
    if (m_dataSize < 1) m_dataSize = 1;
    if (m_dataSize > m_maxMessageSize) m_dataSize = m_maxMessageSize;
    uint64_t* send_buffer = (uint64_t*) m_sizeSendMemaddr.getBacking();
    *send_buffer = m_dataSize;

    // fire off a recv for the data
    if (m_debug > 2) std::cerr << "rank " << m_rank << " start a datarecv\n";
    enQ_irecv( evQ, m_recvBuf, m_dataSize, UINT64_T, partner, DATA, GroupWorld, &m_dataRecvRequest );
    m_dataRecvActive = true;

    // send the request
    if (m_debug > 1) std::cerr << "rank " << m_rank << " sending data request (size=" << m_dataSize << ") to rank " << partner << std::endl;
    enQ_isend( evQ, m_sizeSendMemaddr, 1, UINT64_T, partner, DATA_REQUEST, GroupWorld, &m_dataReqSendRequest);
}

void EmberTrafficGenGenerator::wait_for_any() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    uint64_t size = m_dataReqRecvActive + m_dataRecvActive;
    if (m_rank != 0) ++size;
    if (m_debug > 2) std::cerr << "rank " << m_rank <<  " enqueing waitany with size " << size << std::endl;
    if (m_generateLoopIndex > 1) delete m_allRequests;
    m_allRequests = new MessageRequest[size];
    uint64_t index=0;
    if (m_dataReqRecvActive) {
        if (m_debug > 3) std::cerr << "copy dataReqRecvRequest " << m_dataReqRecvRequest << " to " << index << std::endl;
        m_allRequests[index] = m_dataReqRecvRequest;
        ++index;
    }
    if (m_dataRecvActive) {
        if (m_debug > 3) std::cerr << "copy dataRecvRequest " << m_dataRecvRequest << " to " << index << std::endl;
        m_allRequests[index] = m_dataRecvRequest;
        ++index;
    }
    if (m_rank != 0) m_allRequests[index] = m_stopRequest;
    enQ_waitany(evQ, size, m_allRequests, &m_requestIndex, &m_anyResponse);
}

void EmberTrafficGenGenerator::compute() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    uint64_t delay = m_dataSize * m_computeDelay;
    if (m_debug > 0) std::cerr << "rank " << m_rank <<  " computing for " << delay << std::endl;
    enQ_compute( evQ, delay);
    return;
}

void EmberTrafficGenGenerator::send_stop() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    for (int i=1; i<size(); ++i)
        enQ_send( evQ, nullptr, 1, UINT64_T, i, STOP, GroupWorld);
    return;
}
