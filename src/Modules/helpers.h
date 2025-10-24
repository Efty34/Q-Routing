#ifndef MODULES_HELPERS_H_
#define MODULES_HELPERS_H_

#include <omnetpp.h>
#include <map>
#include <sstream>
using namespace omnetpp;
using namespace std;

/*
Message kinds:
  10 = DATA_PACKET (generic data packet)
  11 = DATA_RESPONSE (generic response)
  30 = Q_UPDATE (Q-routing feedback between routers)

For all messages we set:
  par("src") : long  - logical sender address
  par("dst") : long  - logical destination address
  par("sendTime") : double - timestamp when sent (for delay calculation)

Q_UPDATE specific parameters:
  par("dest") : long - destination the Q-value refers to
  par("qValue") : double - best Q-value to reach destination (estimated delay)

Data packet optional parameters:
  par("seqNum") : long - sequence number
  par("payload") : long - payload size
*/

enum {
    DNS_QUERY=10, DNS_RESPONSE=11,      // Reusing for compatibility (now generic DATA/RESPONSE)
    HTTP_GET=20, HTTP_RESPONSE=21,      // Kept for compatibility
    Q_UPDATE=30                          // Q-routing feedback
};

// Q-routing constants
const double INITIAL_Q_VALUE = 1.0;      // Initial Q-value estimate (seconds)
const double LEARNING_RATE = 0.5;        // Alpha: how fast to learn (0-1)
const double EPSILON = 0.1;              // Exploration rate (10% random)

static cMessage* mk(const char* name, int kind, long src, long dst) {
    auto *m = new cMessage(name, kind);
    m->addPar("src").setLongValue(src);
    m->addPar("dst").setLongValue(dst);
    m->addPar("sendTime").setDoubleValue(simTime().dbl());
    return m;
}
static inline long SRC(cMessage* m){ return m->par("src").longValue(); }
static inline long DST(cMessage* m){ return m->par("dst").longValue(); }


#endif /* MODULES_HELPERS_H_ */
