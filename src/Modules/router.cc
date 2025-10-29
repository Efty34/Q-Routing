#include <omnetpp.h>
#include "helpers.h"
#include <map>
#include <set>
#include <sstream>
#include <vector>
#include <algorithm>
using namespace omnetpp;
using namespace std;

class Router : public cSimpleModule {
  private:
    // Router's own address
    int myAddress;
    
    // Q-table: Q[destination][neighbor_gate] = estimated time to reach destination
    map<long, map<int, double>> qTable;
    int numGates;
    
    // Track forwarded packets: key = unique_id, value = packet info
    struct PacketInfo {
        long dest;
        int gate;
        double sendTime;
        long srcAddr;  // Original source for tracking
    };
    map<long, PacketInfo> pendingPackets;
    long nextPacketId = 0;
    
    // Track which gates have been tried for each destination (for initial exploration)
    map<long, set<int>> exploredGates;
    
    // Statistics
    simsignal_t qValueSignal;
    
  protected:
    void initialize() override {
        myAddress = par("address");
        numGates = gateSize("pppg");
        qValueSignal = registerSignal("qValue");
        
        EV_INFO << "Router " << myAddress << " initialized with " << numGates 
                << " gates (Multi-hop Q-routing enabled)\n";
    }

    // Get Q-value for destination via specific neighbor gate
    double getQ(long dest, int gate) {
        if (qTable.find(dest) == qTable.end() || qTable[dest].find(gate) == qTable[dest].end()) {
            return INITIAL_Q_VALUE; // Default Q-value
        }
        return qTable[dest][gate];
    }
    
    // Get minimum Q-value for destination across all gates
    double getMinQ(long dest) {
        double minQ = INITIAL_Q_VALUE;
        if (qTable.find(dest) != qTable.end()) {
            for (const auto& [gate, qValue] : qTable[dest]) {
                if (qValue < minQ) {
                    minQ = qValue;
                }
            }
        }
        return minQ;
    }
    
    // Update Q-value using full Q-routing formula with next-hop Q-value
    void updateQ(long dest, int gate, double transmissionTime, double nextHopQ) {
        double oldQ = getQ(dest, gate);
        // Full Q-routing: Q(x,y,d) = Q(x,y,d) + α * [t(x,y) + min_Q(y,*,d) - Q(x,y,d)]
        double target = transmissionTime + nextHopQ;
        double newQ = oldQ + LEARNING_RATE * (target - oldQ);
        qTable[dest][gate] = newQ;
        
        emit(qValueSignal, newQ);
        EV_INFO << "Router" << myAddress << " Q-update: dest=" << dest << " gate=" << gate 
                << " oldQ=" << oldQ << " newQ=" << newQ 
                << " (t=" << transmissionTime << " + nextQ=" << nextHopQ << ")\n";
    }
    
    // Select best gate based on Q-values (smart exploration + epsilon-greedy)
    int selectGate(long dest, int incomingGate) {
        vector<int> availableGates;
        for (int i = 0; i < numGates; i++) {
            if (i != incomingGate) { // Don't send back to incoming gate
                availableGates.push_back(i);
            }
        }
        
        if (availableGates.empty()) return -1;
        
        // PHASE 1: Initial Exploration - try each gate at least once
        // Check if there are unexplored gates for this destination
        vector<int> unexploredGates;
        for (int g : availableGates) {
            if (exploredGates[dest].find(g) == exploredGates[dest].end()) {
                unexploredGates.push_back(g);
            }
        }
        
        if (!unexploredGates.empty()) {
            // Pick first unexplored gate (systematic exploration)
            int selectedGate = unexploredGates[0];
            exploredGates[dest].insert(selectedGate);
            EV_INFO << "Router: Initial exploration - trying gate " << selectedGate 
                    << " for dest=" << dest << " (first time)\n";
            return selectedGate;
        }
        
        // PHASE 2: Epsilon-greedy (after all gates explored once)
        if (uniform(0, 1) < EPSILON) {
            // Exploration: random gate
            int idx = intuniform(0, availableGates.size() - 1);
            EV_INFO << "Router: Random exploration - gate " << availableGates[idx] 
                    << " for dest=" << dest << "\n";
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
        EV_INFO << "Router: Exploitation - best gate " << bestGate 
                << " for dest=" << dest << " (Q=" << bestQ << ")\n";
        return bestGate;
    }

    void handleMessage(cMessage *msg) override {
        long dst = DST(msg);
        long src = SRC(msg);
        int inGate = msg->getArrivalGate() ? msg->getArrivalGate()->getIndex() : -1;
        
        // Ignore Q-UPDATE messages (not used in this direct-learning topology)
        if (msg->getKind() == Q_UPDATE) {
            delete msg;
            return;
        }
        
        // Check if this is a RESPONSE coming back - use it to learn!
        if (msg->getKind() == DNS_RESPONSE || msg->getKind() == HTTP_RESPONSE) {
            // This is a response packet - the source is the destination we were routing to
            long responseSrc = src;
            
            // Look for any pending packet to this destination from this gate
            for (auto it = pendingPackets.begin(); it != pendingPackets.end(); ) {
                if (it->second.dest == responseSrc && it->second.gate == inGate) {
                    // Found it! Calculate round-trip time
                    double currentTime = simTime().dbl();
                    double rtt = currentTime - it->second.sendTime;
                    
                    // Update Q-value: For direct connection to destination, nextHopQ = 0
                    // Q = Q + α * (t + 0 - Q) which simplifies to Q = Q + α * (t - Q)
                    updateQ(it->second.dest, it->second.gate, rtt, 0.0);
                    
                    // Remove from pending
                    it = pendingPackets.erase(it);
                    break;  // Only update once per response
                } else {
                    ++it;
                }
            }
            
            // Forward the response to its destination (source PC)
            int outGate = selectGate(dst, inGate);
            if (outGate >= 0 && outGate < numGates) {
                EV_INFO << "Router" << myAddress << " forwarding response to dest=" << dst 
                        << " via gate=" << outGate << "\n";
                send(msg, "pppg$o", outGate);
            } else {
                EV_WARN << "Router" << myAddress << ": no valid gate for response dest=" << dst << ", dropping\n";
                delete msg;
            }
            return;
        }
        
        // For DATA messages (requests), route using Q-table
        int outGate = selectGate(dst, inGate);
        
        if (outGate >= 0 && outGate < numGates) {
            // Track this packet so we can learn from the response
            PacketInfo info;
            info.dest = dst;
            info.gate = outGate;
            info.sendTime = simTime().dbl();
            info.srcAddr = src;
            pendingPackets[nextPacketId++] = info;
            
            // Forward the message
            EV_INFO << "Router" << myAddress << " forwarding DATA to dest=" << dst 
                    << " via gate=" << outGate << " (Q=" << getQ(dst, outGate) << ")\n";
            send(msg, "pppg$o", outGate);
        } else {
            EV_WARN << "Router" << myAddress << ": no valid gate for dest=" << dst << ", dropping\n";
            delete msg;
        }
    }
};
Define_Module(Router);

