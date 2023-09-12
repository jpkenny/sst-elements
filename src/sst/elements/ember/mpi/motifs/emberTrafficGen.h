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


#ifndef _H_EMBER_TRAFFIC_GEN
#define _H_EMBER_TRAFFIC_GEN

#include <sst/core/rng/gaussian.h>
#include <sst/core/rng/marsaglia.h>

#include "mpi/embermpigen.h"

namespace SST {
namespace Ember {

class EmberTrafficGenGenerator : public EmberMessagePassingGenerator {

public:
    SST_ELI_REGISTER_SUBCOMPONENT(
        EmberTrafficGenGenerator,
        "ember",
        "TrafficGenMotif",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "Models network traffic",
        SST::Ember::EmberGenerator
    )

    SST_ELI_DOCUMENT_PARAMS(
        { "arg.messageSize",    "Sets the size of exchange",    "1"},
        { "arg.mean",   "Sets the mean time between exchange",  "1"},
        { "arg.stddev", "Sets the stddev of time between exchange",     "1"},
        { "arg.startDelay", "Sets the stddev of time between exchange",     "1"},
    )

    SST_ELI_DOCUMENT_STATISTICS(
        { "time-Init", "Time spent in Init event",          "ns",  0},
        { "time-Finalize", "Time spent in Finalize event",  "ns", 0},
        { "time-Rank", "Time spent in Rank event",          "ns", 0},
        { "time-Size", "Time spent in Size event",          "ns", 0},
        { "time-Send", "Time spent in Recv event",          "ns", 0},
        { "time-Recv", "Time spent in Recv event",          "ns", 0},
        { "time-Irecv", "Time spent in Irecv event",        "ns", 0},
        { "time-Isend", "Time spent in Isend event",        "ns", 0},
        { "time-Wait", "Time spent in Wait event",          "ns", 0},
        { "time-Waitall", "Time spent in Waitall event",    "ns", 0},
        { "time-Waitany", "Time spent in Waitany event",    "ns", 0},
        { "time-Compute", "Time spent in Compute event",    "ns", 0},
        { "time-Barrier", "Time spent in Barrier event",    "ns", 0},
        { "time-Alltoallv", "Time spent in Alltoallv event", "ns", 0},
        { "time-Alltoall", "Time spent in Alltoall event",  "ns", 0},
        { "time-Allreduce", "Time spent in Allreduce event", "ns", 0},
        { "time-Reduce", "Time spent in Reduce event",      "ns", 0},
        { "time-Bcast", "Time spent in Bcast event",        "ns", 0},
        { "time-Gettime", "Time spent in Gettime event",    "ns", 0},
        { "time-Commsplit", "Time spent in Commsplit event", "ns", 0},
        { "time-Commcreate", "Time spent in Commcreate event", "ns", 0},
    )


public:
	EmberTrafficGenGenerator(SST::ComponentId_t, Params& params);
    bool generate( std::queue<EmberEvent*>& evQ);
    bool generate_plusOne( std::queue<EmberEvent*>& evQ);
    bool primary( ) {
        if (m_pattern == "plusOne")
            return false;
        return true;
    }
    void configure();
    void configure_plusOne();

    // extended patterns
    bool generate_random( std::queue<EmberEvent*>& evQ);
    void recv_datareq();
    void recv_stop();
    void send_datareq();
    void wait_for_any();
    void compute();
    void send_stop();

private:
    std::string m_pattern;

    uint32_t m_messageSize;
    uint32_t m_maxMessageSize;
    void* m_sendBuf;
    void* m_recvBuf;
    MessageRequest m_req;

    // original "plusOne" pattern
    //    MessageResponse m_resp;
    double  m_startDelay;
    double  m_mean;
    double  m_stddev;
    SSTGaussianDistribution* m_random;

    // extended patterns
    enum {DATA_REQUEST, DATA, STOP};
    std::queue<EmberEvent*>* evQ_;
    bool m_dataReqRecvActive;
    bool m_dataRecvActive;
    bool m_needToWait;
    bool m_testSends;
    bool m_stopped;
    unsigned int m_generateLoopIndex;
    unsigned int m_iterations;
    Hermes::MemAddr m_sizeSendMemaddr;
    Hermes::MemAddr m_sizeRecvMemaddr;
    Hermes::MemAddr m_rankBytes;
    Hermes::MemAddr m_totalBytes;
    MessageRequest m_dataReqRecvRequest;
    MessageRequest m_dataReqSendRequest;
    MessageRequest m_dataRecvRequest;
    MessageRequest m_stopRequest;
    MessageRequest* m_allRequests;
    std::list<MessageRequest*> m_sendRequests;
    MessageResponse m_anyResponse;
    uint32_t m_rank;
    uint32_t m_hotSpots;
    uint32_t m_hotSpotsRatio;
    std::vector<uint32_t> m_hotRanks;
    std::set<uint32_t> m_hotRanks_set;
    uint32_t m_hotCounter;
    uint32_t m_hotCounterInitial;
    uint32_t m_debug;
    uint64_t m_dataSize;
    uint64_t m_computeDelay;
    uint64_t m_startTime;
    uint64_t m_currentTime;
    uint64_t m_stopTime;
    int m_requestIndex;
    int m_sendRequestFlag;
    double  m_meanMessageSize;
    double  m_stddevMessageSize;
    SSTGaussianDistribution* m_distMessageSize;
    double  m_meanComputeDelay;
    double  m_stddevComputeDelay;
    SSTGaussianDistribution* m_distComputeDelay;
    SST::RNG::MarsagliaRNG* m_distPartner;
};

}
}

#endif
