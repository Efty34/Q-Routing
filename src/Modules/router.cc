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
    // Q-table: Q[destination][neighbor_gate] = estimated time to reach destination
    map<long, map<int, double>> qTable;
    int numGates;
    
    // Track forwarded packets: key = messageId, value = (dest, gate, sendTime)
    struct PacketInfo {
        long dest;
        int gate;
        double sendTime;
    };
    map<long, PacketInfo> pendingPackets;
    long nextPacketId = 0;
    
    // Track which gates have been tried for each destination (for initial exploration)
    map<long, set<int>> exploredGates;
    
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
    void updateQ(long dest, int gate, double measuredDelay) {
        double oldQ = getQ(dest, gate);
        // Simplified Q-learning for single router: Q(dest,gate) = oldQ + α * [delay - oldQ]
        // Since we're at the last hop before destination, minNextQ = 0
        double newQ = oldQ + LEARNING_RATE * (measuredDelay - oldQ);
        qTable[dest][gate] = newQ;
        
        emit(qValueSignal, newQ);
        EV_INFO << "Router Q-update: dest=" << dest << " gate=" << gate 
                << " oldQ=" << oldQ << " newQ=" << newQ 
                << " (measured delay=" << measuredDelay << ")\n";
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
        
        // Ignore Q-UPDATE messages (not used in single-router topology)
        if (msg->getKind() == Q_UPDATE) {
            delete msg;
            return;
        }
        
        // Check if this is a RESPONSE coming back - use it to learn!
        if (msg->getKind() == DNS_RESPONSE || msg->getKind() == HTTP_RESPONSE) {
            // This is a response packet - check if we forwarded the original request
            long responseSrc = SRC(msg);  // Who sent this response (the destination we were routing to)
            
            // Look for any pending packet to this destination from this gate
            for (auto it = pendingPackets.begin(); it != pendingPackets.end(); ) {
                if (it->second.dest == responseSrc && it->second.gate == inGate) {
                    // Found it! Calculate round-trip time
                    double currentTime = simTime().dbl();
                    double rtt = currentTime - it->second.sendTime;
                    
                    // Update Q-value based on measured delay
                    updateQ(it->second.dest, it->second.gate, rtt);
                    
                    // Remove from pending
                    it = pendingPackets.erase(it);
                    break;  // Only update once per response
                } else {
                    ++it;
                }
            }
            
            // Forward the response to its destination
            int outGate = selectGate(dst, inGate);
            if (outGate >= 0 && outGate < numGates) {
                EV_INFO << "Router forwarding response to dest=" << dst << " via gate=" << outGate << "\n";
                send(msg, "pppg$o", outGate);
            } else {
                EV_WARN << "Router: no valid gate for response dest=" << dst << ", dropping\n";
                delete msg;
            }
            return;
        }
        
        // For data messages (requests), route using Q-table
        int outGate = selectGate(dst, inGate);
        
        if (outGate >= 0 && outGate < numGates) {
            // Track this packet so we can learn from the response
            PacketInfo info;
            info.dest = dst;
            info.gate = outGate;
            info.sendTime = simTime().dbl();
            pendingPackets[nextPacketId++] = info;
            
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

