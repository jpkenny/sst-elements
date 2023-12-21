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
#include <limits>
#include <cstdlib>

using namespace SST;
using namespace SST::Ember;

#define TAG 0xDEADBEEF

EmberTrafficGenGenerator::EmberTrafficGenGenerator(SST::ComponentId_t id,
                                                    Params& params) :
    EmberMessagePassingGenerator(id, params, "TrafficGen"),
    m_generateLoopIndex(0), m_needToWait(false), m_currentTime(0), m_finished(false), m_finishing(false), m_finalRecvs(false), m_numStopped(0),
    m_rankBytes(0), m_totalBytes(0), m_requestIndex(-1), m_dataSendActive(false), m_dataRecvActive(false)
{
    m_pattern = params.find<std::string>("arg.pattern", "plusOne");
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
        m_messageSize = (uint32_t) params.find("arg.messageSize", 1024);
        m_mean = params.find("arg.mean", 5000.0);
        m_stddev = params.find("arg.stddev", 300.0 );
        m_startDelay = params.find("arg.startDelay", .0 );
    }
    else {
        m_debug = params.find<int>("arg.debugLevel",0);
        m_messageSize = params.find("arg.messageSize", std::numeric_limits<uint32_t>::max());
        m_meanMessageSize = params.find("arg.messageSizeMean", 1024);
        m_stddevMessageSize = params.find("arg.messageSizeStdDev", 1024);
        m_delay = params.find("arg.delayNano", 1000);
        m_hotSpots = params.find<unsigned int>("arg.hotSpots",0);
        m_hotSpotsRatio = params.find<unsigned int>("arg.hotSpotsRatio",99);
        m_iterations = params.find<unsigned int>("arg.stopIterations", std::numeric_limits<unsigned int>::max());
        m_stopTime = params.find<uint64_t>("arg.stopTimeUs", std::numeric_limits<uint64_t>::max());
        if (m_iterations != std::numeric_limits<unsigned int>::max() && m_stopTime != std::numeric_limits<uint64_t>::max()) {
            std::cerr << "Please set either stopIterations or stopTimeUs, not both\n";
            abort();
        }
        if (m_stopTime != std::numeric_limits<uint64_t>::max()) m_stopTime *= 1000;
        if (m_iterations == std::numeric_limits<unsigned int>::max() && m_stopTime == std::numeric_limits<uint64_t>::max()) {
            m_iterations = 1000;
        }
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
    m_commSize = size();

    m_distMessageSize = new SSTGaussianDistribution(
                m_meanMessageSize, m_stddevMessageSize, new RNG::MarsagliaRNG( 11 + rank(), RAND_MAX / (rank() + 1) ) );

    std::srand(1); // want same hotRanks on every node
    if (m_hotSpots) {
        while (m_hotRanks_set.size() < m_hotSpots) {
            uint32_t rank = std::rand() % m_commSize;
            m_hotRanks_set.insert( rank );
        }
        std::copy(m_hotRanks_set.begin(), m_hotRanks_set.end(), std::back_inserter(m_hotRanks));

        m_hotCounterInitial = m_hotSpotsRatio;
        m_hotCounter = m_hotCounterInitial;
    }

    memSetBacked();
    m_rankBytes = memAlloc(sizeofDataType(UINT64_T));
    m_totalBytes = memAlloc(sizeofDataType(UINT64_T));
    m_rankSends = memAlloc(m_commSize * sizeofDataType(UINT64_T));
    m_reducedSends = memAlloc(m_commSize * sizeofDataType(UINT64_T));
    for (int i=0; i < m_commSize; ++i) {
        m_rankSends.at<uint64_t>(i)=0;
        m_reducedSends.at<uint64_t>(i)=0;
    }
}

void EmberTrafficGenGenerator::configure_plusOne()
{
    assert( 2 == m_commSize);

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
    m_currentTime = getCurrentSimTimeNano();

    if (m_debug > 2) std::cerr << "rank " << m_rank << " entering loop " << m_generateLoopIndex
                               << " (t=" << m_currentTime / 1000.0 <<"us)" << std::endl;

    // Follow the termination path once ALLSTOPPED messages have gone out
    if (m_finishing || m_finalRecvs || m_finished)
        return check_termination();

    // Begin standard loop
    if (m_generateLoopIndex == 0) {
        enQ_getTime( evQ, &m_startTime );
        //post receives to handle termination
        if (m_rank != 0) recv_allstopped();
        else recv_stopping();
        // post receive for data notifies
        recv_data();
        send_data();
        m_needToWait = true;
        ++m_generateLoopIndex;
        return false;
    }

    if (m_needToWait) {
        if (m_debug > 2) std::cerr << "rank " << m_rank << " waiting for any request\n";
        wait_for_any();
        m_needToWait = false;
        ++m_generateLoopIndex;
        return false;
    }

    if (m_debug > 2) std::cerr << "rank " << m_rank << " got completed request index: " << m_requestIndex << std::endl;

    // Time to stop?
    // All nonzero ranks send STOPPING messages to rank zero when they meet their stopping criteria
    // Once zero has received all expected STOPPING messages and stopped itself, it sends ALLSTOPPED messages to all ranks
    // Upon ALLSTOPPED we reduce the total bytes sent/received, report results, and end the motif.
    if (m_requestIndex == STOP_REQUEST) {
        m_requestIndex = -1;
        m_needToWait = false;
        if (m_rank == 0) {
            if (m_numStopped < m_commSize - 1) ++m_numStopped;
            if (m_debug > 0) std::cerr << "rank " << m_rank << " received stop message " << m_numStopped
                                       << " from " << m_anyResponse.src << std::endl;
            if (!check_stop()) {
                recv_stopping();
                m_needToWait = true;
            }
            else {
                check_finish();
            }
        }
        else {
            if (m_debug > 1) std::cerr << "rank " << m_rank << " stopping with bytes " << m_rankBytes.at<uint64_t>(0) << std::endl;
            //enQ_reduce( evQ, m_rankBytes, m_totalBytes, 1, UINT64_T, Hermes::MP::SUM, 0, GroupWorld );
            check_finish();
        }
        ++m_generateLoopIndex;
        return false;
    }

    if (m_requestIndex == RECV_REQUEST) {
        m_requestIndex = -1;
        ++m_numRecv;
        m_dataRecvActive = false;

        if (m_currentTime < m_stopTime) {
            accumulate_data();
        }
        recv_data();
    }
    else if (m_requestIndex == SEND_REQUEST) {
        m_requestIndex = -1;
        m_dataSendActive = false;
        if (m_currentTime < m_stopTime && m_currentIteration < m_iterations) {
            send_data();
            delay();
        }
        else if (m_rank != 0) {
            if (m_debug > 0) std::cerr << "rank " << m_rank << " stopping\n";
            enQ_send(evQ, nullptr, 1, CHAR, 0, STOPPING, GroupWorld);
        }
        else {
            if (!check_stop()) {
                wait_for_any();
                ++m_generateLoopIndex;
                return false;
            }
        }
    }

    m_needToWait = true;
    ++m_generateLoopIndex;
    return false;
}

void EmberTrafficGenGenerator::recv_stopping() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    enQ_irecv( evQ, nullptr, 1, CHAR, Hermes::MP::AnySrc, STOPPING, GroupWorld, &m_requests[STOP_REQUEST]);
}

void EmberTrafficGenGenerator::recv_allstopped() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    enQ_irecv( evQ, nullptr, 1, CHAR, 0, ALLSTOPPED, GroupWorld, &m_requests[STOP_REQUEST]);
}

void EmberTrafficGenGenerator::recv_data() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    if (m_debug > 2) std::cerr << "rank " << m_rank << " start a datareq recv\n";
    enQ_irecv( evQ, m_recvBuf, m_maxMessageSize, UINT64_T, Hermes::MP::AnySrc, DATA, GroupWorld, &m_requests[RECV_REQUEST]);
    m_dataRecvActive = true;
}

void EmberTrafficGenGenerator::accumulate_data() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    if (m_debug > 0) std::cerr << "rank " << m_rank << " received " << m_anyResponse.count << " from " << m_anyResponse.src << std::endl;
    int count = m_anyResponse.count;
    if (m_debug > 2) std::cerr << "DEBUG RECEIVE: rank " << m_rank << " accumulating " << count << " t=" << double(m_currentTime) / double(1e9) << std::endl;
    uint64_t* bytes = (uint64_t*) m_rankBytes.getBacking();
    *bytes += count;
}

void EmberTrafficGenGenerator::delay() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    if (m_debug > 0) std::cerr << "rank " << m_rank <<  " delaying for " << m_delay << std::endl;
    enQ_compute( evQ, m_delay);
}

void EmberTrafficGenGenerator::send_data() {
    std::queue<EmberEvent*>& evQ = *evQ_;

    ++m_currentIteration;
    if (m_debug > 2)
            std::cerr << "DEBUG SEND: rank " << m_rank << ", iteration " << m_currentIteration << ",       t=" << double(m_currentTime) / double(1e9) << std::endl;

    // determine rank to send data to
    uint32_t partner = (uint32_t) m_rank;
    if (m_hotSpots && !m_hotCounter && !m_hotRanks_set.count(m_rank) ) {
        partner = m_hotRanks[std::rand() % m_hotSpots];
    }
    else {
        while (partner == m_rank)
            partner = std::rand() % m_commSize;
    }
    if (m_hotSpots) {
        if (m_hotCounter) --m_hotCounter;
        else m_hotCounter = m_hotCounterInitial;
    }

    // determine size of data
    if (m_messageSize == std::numeric_limits<uint32_t>::max()) {
      m_dataSize = int( abs( m_distMessageSize->getNextDouble() ) );
      if (m_dataSize < 1) m_dataSize = 1;
      if (m_dataSize > m_maxMessageSize) m_dataSize = m_maxMessageSize;
    }
    else m_dataSize = m_messageSize;
    if (m_debug > 1) std::cerr << "rank " << m_rank << " sending data (size=" << m_dataSize << ") to rank " << partner << std::endl;
    m_rankSends.at<uint64_t>(partner) += 1;
    if (m_debug > 1) std::cerr << "rank " << m_rank << " sending num " << m_rankSends.at<uint64_t>(partner) << " to " << partner << std::endl;
    enQ_isend( evQ, m_sendBuf, m_dataSize, UINT64_T, partner, DATA, GroupWorld, &m_requests[SEND_REQUEST]);
    m_dataSendActive = true;
}

void EmberTrafficGenGenerator::wait_for_any() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    int size = 2;
    if(m_dataSendActive) ++size;
    if (m_debug > 2)
        std::cerr << "DEBUG WAIT: rank " << m_rank <<  " waitany with size " << size << " t=" << double(m_currentTime) / double(1e9) << std::endl;
    enQ_waitany(evQ, size, m_requests, &m_requestIndex, &m_anyResponse);
}

void EmberTrafficGenGenerator::check_finish() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    if (m_debug > 2)
        std::cerr << "rank " << m_rank <<  " performing finishing allreduce " << std::endl;
    m_finishing = true;
    enQ_allreduce(evQ, m_rankSends, m_reducedSends, m_commSize, UINT64_T, Hermes::MP::SUM, GroupWorld);
}

bool EmberTrafficGenGenerator::start_final_waits() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    m_finishing = false;
    uint64_t numSendToMe = m_reducedSends.at<uint64_t>(m_rank);
    if (m_debug > 2)
        std::cerr << "rank " << m_rank << " received " << m_numRecv << " of " << numSendToMe << " expected messages\n";
    m_numFinalRecvs = numSendToMe - m_numRecv;
    if (m_numFinalRecvs) {
        m_finalRecvs = true;
        if (m_debug > 2) std::cerr << "rank " << m_rank << " enqueueing first final wait\n";
        enQ_wait(evQ, &m_requests[RECV_REQUEST], &m_anyResponse);
        return true;
    }
    if (m_debug > 2) std::cerr << "rank " << m_rank << " no final waits necessary\n";
    return false;
}

void EmberTrafficGenGenerator::get_total_bytes() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    enQ_reduce( evQ, m_rankBytes, m_totalBytes, 1, UINT64_T, Hermes::MP::SUM, 0, GroupWorld );
}

bool EmberTrafficGenGenerator::check_stop() {
    std::queue<EmberEvent*>& evQ = *evQ_;
    if (m_numStopped == m_commSize - 1 && (m_currentTime >= m_stopTime || m_currentIteration >= m_iterations)){
        if (m_debug > 1) std::cerr << "rank " << m_rank << " all ranks complete\n"
                                   << "rank " << m_rank << " stopping with bytes " << m_rankBytes.at<uint64_t>(0) << std::endl;
        m_needToWait = false;
        m_stopTimeActual = getCurrentSimTimeNano();
        for (int i=1; i < size(); ++i) {
            enQ_send(evQ, m_allStopped, 1, CHAR, i, ALLSTOPPED, GroupWorld);
        }
        //enQ_reduce( evQ, m_rankBytes, m_totalBytes, 1, UINT64_T, Hermes::MP::SUM, 0, GroupWorld );
        //return true;
        return true;
    }
    if (m_debug > 1) std::cerr << "rank " << m_rank << " not stopping yet\n";
    return false;
}

bool EmberTrafficGenGenerator::check_termination() {
    std::queue<EmberEvent*>& evQ = *evQ_;

    // Termination is much cleaner in the reference MPI code. Here we have three different termination states that we need to go through.
    // 1) determine outstanding sends to our rank and start
    if (m_finishing) {
      if (start_final_waits()) {
        return false;
      }
      else {
        enQ_cancel(evQ, m_requests[RECV_REQUEST]);
        m_finished = true;
        get_total_bytes();
      }
      return false;
    }
    // 2)
    if (m_finalRecvs == true) {
      if (m_debug > 2) std::cerr << "rank " << m_rank << " performing final waits\n";
      accumulate_data();
      --m_numFinalRecvs;
      if (m_debug > 2) std::cerr << "rank " << m_rank << " m_numFinalRecvs " << m_numFinalRecvs << std::endl;
      if(m_numFinalRecvs) {
        recv_data();
        enQ_wait(evQ, &m_requests[RECV_REQUEST], &m_anyResponse);
        return false;
      }
      else {
        m_finished = true;
        m_finalRecvs = false;
        get_total_bytes();
        return false;
      }
    }
    if(m_finished) {
      if (m_rank == 0) {
        m_stopTime = m_currentTime;
        uint64_t bytes = m_totalBytes.at<uint64_t>(0);
        std::cerr << "emberTrafficGen completed in " << (double) m_stopTime / (double) 1e9 << " seconds\n";
        std::cerr << "    with " << bytes << " total bytes sent\n";
        std::cerr << "Total observed bandwidth: " << (double) bytes / (double) m_stopTime  << " GB/s\n";
      }
      return true;
    }
    return true;
}
