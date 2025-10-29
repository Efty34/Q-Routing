package simple_dns_http;

// Fast path - lowest delay
channel FastPath extends ned.DatarateChannel
{
    datarate = 100Mbps;
    delay = 10ms;  // Fast router path
}

// Medium path - moderate delay
channel MediumPath extends ned.DatarateChannel
{
    datarate = 100Mbps;
    delay = 50ms;  // Medium router path
}

// Slow path - highest delay
channel SlowPath extends ned.DatarateChannel
{
    datarate = 100Mbps;
    delay = 100ms;  // Slow router path
}

// Connection links (minimal delay)
channel Link extends ned.DatarateChannel
{
    datarate = 100Mbps;
    delay = 1ms;
}

// Q-Learning Router
simple Router
{
    parameters:
        int address = default(0);          // Router's own address
        string routes = default("");       // Not used in Q-routing (kept for compatibility)
        @display("i=device/router");
        @signal[qValue](type=double);
        @statistic[qValue](title="Q-values"; record=vector,stats; interpolationmode=none);
    gates:
        inout pppg[];                      // Variable-size gate array
}

// Traffic Generator / Endpoint
simple PC
{
    parameters:
        int address;                         // Node address
        int destAddr = default(-1);          // Destination to send packets to (-1 = no sending)
        double startTime @unit(s) = default(0.5s);     // When to start sending
        double sendInterval @unit(s) = default(1.0s);  // Interval between packets (0 = send once)
        bool isSource = default(false);      // Is this the source node?
        bool isDestination = default(false); // Is this the destination node?
        @display("i=device/laptop");
    gates:
        inout ppp[];  // Gate array to support multiple connections
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
// Multi-Router Q-Routing Network
// 
// Topology: Source → 3 Routers (3 different paths) → Destination
//
//                    ┌──► R1 (10ms - FAST) ────┐
//                    │                          │
//   Source (addr=1) ─┼──► R2 (50ms - MEDIUM) ──┼─→ Destination (addr=2)
//                    │                          │
//                    └──► R3 (100ms - SLOW) ────┘
//
// Each router learns Q-values and discovers that R1 provides the fastest path
//
network SimpleNet
{
    submodules:
        // Source node - generates traffic
        source: PC {
            address = 1;
            destAddr = 2;           // Send to destination
            startTime = 1s;         // Start after initialization
            sendInterval = 2s;      // Send packet every 2 seconds
            isSource = true;
            @display("p=100,200");
        }
        
        // Destination node - receives traffic
        destination: PC {
            address = 2;
            destAddr = -1;          // Doesn't send
            isDestination = true;
            @display("p=500,200");
        }
        
        // Three routers with different processing delays
        router1: Router {
            address = 11;           // Router 1 address
            @display("p=300,100");
        }
        
        router2: Router {
            address = 12;           // Router 2 address
            @display("p=300,200");
        }
        
        router3: Router {
            address = 13;           // Router 3 address
            @display("p=300,300");
        }
        
    connections:
        // Source connects to all 3 routers
        source.ppp++ <--> Link <--> router1.pppg++;
        source.ppp++ <--> Link <--> router2.pppg++;
        source.ppp++ <--> Link <--> router3.pppg++;
        
        // Each router has different delay to destination
        router1.pppg++ <--> FastPath <--> destination.ppp++;     // 10ms - Best path
        router2.pppg++ <--> MediumPath <--> destination.ppp++;   // 50ms - Medium path
        router3.pppg++ <--> SlowPath <--> destination.ppp++;     // 100ms - Worst path
}

