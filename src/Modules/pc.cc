#include <omnetpp.h>
#include "helpers.h"
#include <map>
#include <set>
using namespace omnetpp;

// PC module that can act as source (traffic generator) or destination (responder)
class PC : public cSimpleModule {
  private:
    int addr = 0;
    int targetAddr = 2;
    bool isSource = false;
    bool isDestination = false;
    cMessage *sendTimer = nullptr;
    int packetsSent = 0;
    int packetsReceived = 0;
    int responsesSent = 0;
    int numGates = 0;
    
    // Q-learning for source PC to learn best router
    std::map<int, double> qTable;  // gate -> Q-value (estimated delay)
    std::set<int> exploredGates;   // Gates tried at least once
    std::map<long, std::pair<int, double>> pendingPackets; // packetId -> (gate, sendTime)
    long nextPacketId = 0;
    
    // Statistics
    std::map<int, int> gateUsageCount;  // Track how many times each gate is used

  protected:
    void initialize() override {
        addr = par("address");
        targetAddr = par("destAddr");
        isSource = par("isSource");
        isDestination = par("isDestination");
        numGates = gateSize("ppp");
        
        // Initialize Q-table for source
        if (isSource) {
            for (int i = 0; i < numGates; i++) {
                qTable[i] = INITIAL_Q_VALUE;
                gateUsageCount[i] = 0;
            }
        }
        
        if (isSource) {
            sendTimer = new cMessage("sendTimer");
            
            // Schedule first packet
            double startTime = par("startTime").doubleValue();
            scheduleAt(simTime() + startTime, sendTimer);
            
            EV_INFO << "Source PC " << addr 
                    << " will send to destination " << targetAddr 
                    << " starting at " << startTime << "s (gates=" << numGates << ")\n";
        } else if (isDestination) {
            EV_INFO << "Destination PC " << addr << " ready to receive (gates=" << numGates << ")\n";
        }
    }
    
    // Select best gate using smart exploration + epsilon-greedy (same as router)
    int selectGate() {
        // PHASE 1: Try each gate at least once
        std::vector<int> unexploredGates;
        for (int i = 0; i < numGates; i++) {
            if (exploredGates.find(i) == exploredGates.end()) {
                unexploredGates.push_back(i);
            }
        }
        
        if (!unexploredGates.empty()) {
            int gate = unexploredGates[0];
            exploredGates.insert(gate);
            EV_INFO << "Source PC " << addr << ": Initial exploration - trying gate " << gate << "\n";
            return gate;
        }
        
        // PHASE 2: Epsilon-greedy
        if (uniform(0, 1) < EPSILON) {
            int gate = intuniform(0, numGates - 1);
            EV_INFO << "Source PC " << addr << ": Random exploration - gate " << gate << "\n";
            return gate;
        }
        
        // Exploitation: choose best gate (lowest Q-value)
        int bestGate = 0;
        double bestQ = qTable[0];
        for (int i = 1; i < numGates; i++) {
            if (qTable[i] < bestQ) {
                bestQ = qTable[i];
                bestGate = i;
            }
        }
        EV_INFO << "Source PC " << addr << ": Exploitation - best gate " << bestGate 
                << " (Q=" << bestQ << ")\n";
        return bestGate;
    }
    
    void updateQ(int gate, double measuredDelay) {
    double oldQ = qTable[gate];
    double newQ = oldQ + LEARNING_RATE * (measuredDelay - oldQ);
    double error = fabs(newQ - measuredDelay); 
    
    qTable[gate] = newQ;
    
    EV_INFO << "Source PC " << addr << " Q-update: gate=" << gate 
            << " oldQ=" << oldQ << " newQ=" << newQ 
            << " (RTT=" << measuredDelay << ", error=" << error;
    
    // Check if converged (error < 5ms = 0.005s)
    if (error < 0.005) {
        EV_INFO << " CONVERGED";
    }
    EV_INFO << ")\n";
}

    void handleMessage(cMessage *msg) override {
        if (msg->isSelfMessage()) {
            // Send a data packet to target (SOURCE only) - use Q-learning to select best gate
            int selectedGate = selectGate();
            
            auto *pkt = mk("DATA", DNS_QUERY, addr, targetAddr);
            pkt->addPar("seqNum").setLongValue(nextPacketId);
            pkt->addPar("sendTime").setDoubleValue(simTime().dbl());
            
            // Track this packet
            pendingPackets[nextPacketId] = std::make_pair(selectedGate, simTime().dbl());
            gateUsageCount[selectedGate]++;
            
            send(pkt, "ppp$o", selectedGate);
            nextPacketId++;
            packetsSent++;
            
            EV_INFO << "Source PC " << addr << " sent packet #" << packetsSent 
                    << " to destination " << targetAddr << " via gate " << selectedGate 
                    << " (Q=" << qTable[selectedGate] << ")\n";
            
            // Schedule next packet
            double interval = par("sendInterval").doubleValue();
            if (interval > 0) {
                scheduleAt(simTime() + interval, sendTimer);
            }
            return;
        }

        // Ignore Q-UPDATE messages at endpoint
        if (msg->getKind() == Q_UPDATE) {
            delete msg;
            return;
        }
        
        // Check if this packet is actually for us
        long dst = DST(msg);
        if (dst != addr) {
            // Not for us - this is a misrouted packet, drop it
            EV_WARN << "PC " << addr << " received packet destined for " << dst 
                    << " (not for us), dropping\n";
            delete msg;
            return;
        }
        
        // DESTINATION: Receive request and send response
        if (isDestination && msg->getKind() == DNS_QUERY) {
            packetsReceived++;
            long src = SRC(msg);
            int inGate = msg->getArrivalGate()->getIndex();
            long seqNum = msg->par("seqNum").longValue();
            
            EV_INFO << "Destination PC " << addr << " received request from " << src 
                    << " via gate " << inGate << " (total: " << packetsReceived << ")\n";
            
            // Send response back through the same gate (include seqNum for tracking)
            auto *response = mk("RESPONSE", DNS_RESPONSE, addr, src);
            response->addPar("seqNum").setLongValue(seqNum);
            send(response, "ppp$o", inGate);
            responsesSent++;
            
            EV_INFO << "Destination PC " << addr << " sent response to " << src 
                    << " via gate " << inGate << "\n";
            delete msg;
            return;
        }
        
        // SOURCE: Receive response and learn from it
        if (isSource && msg->getKind() == DNS_RESPONSE) {
            packetsReceived++;
            long src = SRC(msg);
            
            // Check if we have this packet in pending (for Q-learning)
            long seqNum = msg->par("seqNum").longValue();
            if (pendingPackets.find(seqNum) != pendingPackets.end()) {
                int gate = pendingPackets[seqNum].first;
                double sendTime = pendingPackets[seqNum].second;
                double rtt = simTime().dbl() - sendTime;
                
                // Update Q-value based on end-to-end RTT
                updateQ(gate, rtt);
                
                pendingPackets.erase(seqNum);
            }
            
            EV_INFO << "Source PC " << addr << " received response from " << src 
                    << " (total received: " << packetsReceived 
                    << ", success rate: " << (100.0 * packetsReceived / packetsSent) << "%)\n";
            delete msg;
            return;
        }
        
        // Unknown message type
        EV_WARN << "PC " << addr << " received unexpected message kind=" << msg->getKind() << "\n";
        delete msg;
    }

    void finish() override {
        if (sendTimer) {
            cancelAndDelete(sendTimer);
            sendTimer = nullptr;
        }
        
        if (isSource) {
            EV_INFO << "========================================\n";
            EV_INFO << "Source PC " << addr << " - Final Statistics:\n";
            EV_INFO << "  Packets Sent: " << packetsSent << "\n";
            EV_INFO << "  Responses Received: " << packetsReceived << "\n";
            EV_INFO << "  Success Rate: " << (100.0 * packetsReceived / packetsSent) << "%\n";
            EV_INFO << "----------------------------------------\n";
            EV_INFO << "Gate Usage Statistics:\n";
            for (int i = 0; i < numGates; i++) {
                double percentage = (100.0 * gateUsageCount[i]) / packetsSent;
                EV_INFO << "  Gate " << i << " (Router" << (11+i) << "): " 
                        << gateUsageCount[i] << " packets (" << percentage << "%) "
                        << "Final Q=" << qTable[i] << "\n";
            }
            EV_INFO << "========================================\n";
        } else if (isDestination) {
            EV_INFO << "Destination PC " << addr << " - Received: " << packetsReceived 
                    << ", Responses Sent: " << responsesSent << "\n";
        }
    }
};
Define_Module(PC);

