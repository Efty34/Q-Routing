#include <omnetpp.h>
#include "helpers.h"
using namespace omnetpp;

// Simple endpoint that responds to any data packet
// Used to demonstrate Q-routing - router learns paths to this node
class HTTP : public cSimpleModule {
  private:
    int addr = 0;
    
  protected:
    void initialize() override {
        addr = par("address");
        EV_INFO << "Endpoint " << addr << " initialized\n";
    }

    void handleMessage(cMessage *msg) override {
        if (msg->getKind() == Q_UPDATE) {
            // Ignore Q-UPDATE messages at endpoint
            delete msg;
            return;
        }
        
        // For any data packet, send a simple response back
        long src = SRC(msg);
        EV_INFO << "Endpoint " << addr << " received packet from " << src << ", responding\n";
        
        auto *resp = mk("RESPONSE", HTTP_RESPONSE, addr, src);
        resp->addPar("payload").setLongValue(200); // Simple payload
        
        // Small service delay to simulate processing
        sendDelayed(resp, SimTime(par("serviceTime").doubleValue()), "ppp$o");
        
        delete msg;
    }
};
Define_Module(HTTP);
