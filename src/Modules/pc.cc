#include <omnetpp.h>
#include "helpers.h"
using namespace omnetpp;

// Simple traffic generator that sends packets to specified destination
// Used to generate traffic for Q-routing learning
class PC : public cSimpleModule {
  private:
    int addr = 0;
    int targetAddr = 2;
    cMessage *sendTimer = nullptr;
    int packetsSent = 0;
    int packetsReceived = 0;

  protected:
    void initialize() override {
        addr = par("address");
        targetAddr = par("destAddr");
        
        sendTimer = new cMessage("sendTimer");
        
        // Schedule first packet
        double startTime = par("startTime").doubleValue();
        scheduleAt(simTime() + startTime, sendTimer);
        
        EV_INFO << "Traffic Generator " << addr 
                << " will send to destination " << targetAddr 
                << " starting at " << startTime << "s\n";
    }

    void handleMessage(cMessage *msg) override {
        if (msg->isSelfMessage()) {
            // Send a data packet to target
            auto *pkt = mk("DATA", DNS_QUERY, addr, targetAddr);
            pkt->addPar("seqNum").setLongValue(packetsSent++);
            send(pkt, "ppp$o");
            
            EV_INFO << "Node " << addr << " sent packet #" << packetsSent 
                    << " to destination " << targetAddr << "\n";
            
            // Schedule next packet
            double interval = par("sendInterval").doubleValue();
            if (interval > 0) {
                scheduleAt(simTime() + interval, sendTimer);
            }
            return;
        }

        // Handle responses
        if (msg->getKind() == Q_UPDATE) {
            // Ignore Q-UPDATE messages at endpoint
            delete msg;
            return;
        }
        
        // Received a response
        packetsReceived++;
        long src = SRC(msg);
        EV_INFO << "Node " << addr << " received response from " << src 
                << " (total received: " << packetsReceived << ")\n";
        
        delete msg;
    }

    void finish() override {
        cancelAndDelete(sendTimer);
        sendTimer = nullptr;
        EV_INFO << "Node " << addr << " - Sent: " << packetsSent 
                << ", Received: " << packetsReceived << "\n";
    }
};
Define_Module(PC);

