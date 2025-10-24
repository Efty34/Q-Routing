package simple_dns_http;

// Fast channel - low delay
channel FastLink extends ned.DatarateChannel
{
    datarate = 100Mbps;
    delay = 0.1ms;
}

// Slow channel - higher delay
channel SlowLink extends ned.DatarateChannel
{
    datarate = 100Mbps;
    delay = 1.0ms;
}

// Q-Learning Router
simple Router
{
    parameters:
        string routes = default("");   // Not used in Q-routing (kept for compatibility)
        @display("i=device/router");
        @signal[qValue](type=double);
        @statistic[qValue](title="Q-values"; record=vector,stats; interpolationmode=none);
    gates:
        inout pppg[];                  // Variable-size gate array
}

// Traffic Generator / Endpoint
simple PC
{
    parameters:
        int address;                         // Node address
        int destAddr = default(2);           // Destination to send packets to
        double startTime @unit(s) = default(0.5s);     // When to start sending
        double sendInterval @unit(s) = default(1.0s);  // Interval between packets (0 = send once)
        @display("i=device/laptop");
    gates:
        inout ppp;
}

// Simple Endpoint (formerly DNS)
simple DNS
{
    parameters:
        int address;
        @display("i=device/server");
    gates:
        inout ppp;
}

// Simple Endpoint (formerly HTTP)
simple HTTP
{
    parameters:
        int address;
        double serviceTime @unit(s) = default(5ms);  // Response delay
        @display("i=device/server");
    gates:
        inout ppp;
}

//
// Network topology for Q-routing demonstration
// Multiple paths between nodes - router learns which is faster
//
//     node1 ----fast---\                /---fast--- node2  
//                       \              /
//                        --> Router -->
//                       /              \
//     node3 ----slow---/                \---slow--- node4
//
// Router has multiple options and must learn optimal paths through Q-learning
//
network SimpleNet
{
    submodules:
        // Central router with Q-learning
        router: Router {
            @display("p=400,250");
        }
        
        // Traffic generators (will send to each other)
        node1: PC {
            address = 1;
            destAddr = 2;           // Send to node2
            startTime = 0.5s;
            sendInterval = 2.0s;    // Send every 2 seconds
            @display("p=150,150");
        }
        
        node3: PC {
            address = 3;
            destAddr = 4;           // Send to node4
            startTime = 1.0s;
            sendInterval = 2.5s;    // Send every 2.5 seconds
            @display("p=150,350");
        }
        
        // Endpoints (respond to packets)
        node2: DNS {
            address = 2;
            @display("p=650,150");
        }
        
        node4: HTTP {
            address = 4;
            serviceTime = 5ms;
            @display("p=650,350");
        }
        
    connections:
        // node1 connected via FAST link (router will learn this is best path)
        node1.ppp <--> FastLink <--> router.pppg++;
        
        // node2 connected via FAST link
        node2.ppp <--> FastLink <--> router.pppg++;
        
        // node3 connected via SLOW link (router will learn this is slower)
        node3.ppp <--> SlowLink <--> router.pppg++;
        
        // node4 connected via SLOW link
        node4.ppp <--> SlowLink <--> router.pppg++;
}

