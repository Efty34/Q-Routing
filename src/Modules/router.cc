#include <omnetpp.h>
#include "helpers.h"
#include <map>
#include <sstream>
#include <vector>
#include <algorithm>
using namespace omnetpp;
using namespace std;

class Router : public cSimpleModule {
  private:
    // Q-table: Q[destination][neighbor_gate] = estimated time to reach destination
    map<long, map<int, double>> qTable;
    int numGates;
    
    // Statistics
    simsignal_t qValueSignal;
    
  protected:
    void initialize() override {
        numGates = gateSize("pppg");
        qValueSignal = registerSignal("qValue");
        
        // Initialize Q-table with default values for potential destinations
        // We'll dynamically add destinations as we see them
        EV_INFO << "Router initialized with " << numGates << " gates (Q-routing enabled)\n";
    }

    // Get Q-value for destination via specific neighbor gate
    double getQ(long dest, int gate) {
        if (qTable.find(dest) == qTable.end() || qTable[dest].find(gate) == qTable[dest].end()) {
            return INITIAL_Q_VALUE; // Default Q-value
        }
        return qTable[dest][gate];
    }
    
    // Update Q-value using Q-learning formula
    void updateQ(long dest, int gate, double observedDelay, double minNextQ) {
        double oldQ = getQ(dest, gate);
        // Q-learning update: Q(s,a) = Q(s,a) + α * [r + min_Q(s') - Q(s,a)]
        // Here: r = observedDelay, minNextQ is the best Q at next hop
        double newQ = oldQ + LEARNING_RATE * (observedDelay + minNextQ - oldQ);
        qTable[dest][gate] = newQ;
        
        emit(qValueSignal, newQ);
        EV_INFO << "Router Q-update: dest=" << dest << " gate=" << gate 
                << " oldQ=" << oldQ << " newQ=" << newQ << "\n";
    }
    
    // Select best gate based on Q-values (epsilon-greedy)
    int selectGate(long dest, int incomingGate) {
        vector<int> availableGates;
        for (int i = 0; i < numGates; i++) {
            if (i != incomingGate) { // Don't send back to incoming gate
                availableGates.push_back(i);
            }
        }
        
        if (availableGates.empty()) return -1;
        
        // Epsilon-greedy: explore vs exploit
        if (uniform(0, 1) < EPSILON) {
            // Exploration: random gate
            int idx = intuniform(0, availableGates.size() - 1);
            return availableGates[idx];
        }
        
        // Exploitation: choose gate with lowest Q-value (best estimated delay)
        int bestGate = availableGates[0];
        double bestQ = getQ(dest, bestGate);
        for (int g : availableGates) {
            double q = getQ(dest, g);
            if (q < bestQ) {
                bestQ = q;
                bestGate = g;
            }
        }
        return bestGate;
    }
    
    // Get minimum Q-value for destination (best option)
    double getMinQ(long dest) {
        double minQ = INITIAL_Q_VALUE;
        bool found = false;
        if (qTable.find(dest) != qTable.end()) {
            for (auto &entry : qTable[dest]) {
                if (!found || entry.second < minQ) {
                    minQ = entry.second;
                    found = true;
                }
            }
        }
        return minQ;
    }

    void handleMessage(cMessage *msg) override {
        long dst = DST(msg);
        int inGate = msg->getArrivalGate() ? msg->getArrivalGate()->getIndex() : -1;
        
        // Calculate transmission delay (time since message was sent)
        double sendTime = msg->par("sendTime").doubleValue();
        double currentTime = simTime().dbl();
        double transmissionDelay = currentTime - sendTime;
        
        // Handle Q-UPDATE feedback messages
        if (msg->getKind() == Q_UPDATE) {
            long qDest = msg->par("dest").longValue();
            double nextHopMinQ = msg->par("qValue").doubleValue();
            
            // Update Q-value based on feedback from next hop
            if (inGate >= 0) {
                updateQ(qDest, inGate, transmissionDelay, nextHopMinQ);
            }
            delete msg;
            return;
        }
        
        // For data messages, route using Q-table and send feedback
        int outGate = selectGate(dst, inGate);
        
        if (outGate >= 0 && outGate < numGates) {
            // Send Q-UPDATE feedback to previous hop
            if (inGate >= 0) {
                double myMinQ = getMinQ(dst);
                auto *feedback = new cMessage("Q_UPDATE", Q_UPDATE);
                feedback->addPar("src").setLongValue(0); // Router doesn't have address
                feedback->addPar("dst").setLongValue(0);
                feedback->addPar("sendTime").setDoubleValue(simTime().dbl());
                feedback->addPar("dest").setLongValue(dst);
                feedback->addPar("qValue").setDoubleValue(myMinQ);
                send(feedback, "pppg$o", inGate);
            }
            
            // Forward the message
            EV_INFO << "Router forwarding to dest=" << dst << " via gate=" << outGate 
                    << " (Q=" << getQ(dst, outGate) << ")\n";
            send(msg, "pppg$o", outGate);
        } else {
            // No valid gate found, drop message
            EV_WARN << "Router: no valid gate for dest=" << dst << ", dropping\n";
            delete msg;
        }
    }
};
Define_Module(Router);

