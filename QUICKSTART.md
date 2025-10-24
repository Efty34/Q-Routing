# Quick Start Guide - Q-Routing Simulation

## Build and Run

### 1. Build the simulation

```bash
cd c:\Users\Asus\Downloads\omnetpp-6.2.0\samples\test\src
opp_makemake -f --deep
make clean
make
```

### 2. Run the simulation

```bash
cd ..\simulations
..\src\test.exe
```

Or for GUI mode:

```bash
..\src\test.exe -u Qtenv
```

## What to Look For

### In the Simulation Log

Look for these key messages showing Q-learning in action:

```
Router initialized with 3 gates (Q-routing enabled)
Router forwarding to dest=2 via gate=1 (Q=1.0)
Router Q-update: dest=2 gate=1 oldQ=1.0 newQ=0.5001
Router forwarding to dest=3 via gate=2 (Q=1.0)
Router Q-update: dest=3 gate=2 oldQ=1.0 newQ=0.5003
```

### Expected Flow

1. PC sends DNS_QUERY to DNS (address=2)
2. Router learns best path to DNS (gate 1)
3. DNS responds with HTTP address (address=3)
4. PC sends HTTP_GET to HTTP server
5. Router learns best path to HTTP (gate 2)
6. HTTP responds to PC

### Q-Value Evolution

- **Initial**: All Q-values = 1.0 (no knowledge)
- **After 1st packet**: Q-value drops to ~0.5 for correct path
- **After multiple packets**: Q-value converges to actual delay (~0.0002-0.0005)
- **Wrong paths**: If explored, will have higher Q-values

## Testing Scenarios

### Test 1: Basic Learning

Just run the default simulation - watch Q-values decrease as router learns

### Test 2: Multiple Requests

Modify `omnetpp.ini` to send more requests:

```ini
**.pc.startAt = exponential(1s)  # Multiple random requests
sim-time-limit = 10s
```

### Test 3: Change Exploration Rate

In `helpers.h`, try different epsilon values:

```cpp
const double EPSILON = 0.3;  // More exploration (30%)
```

### Test 4: Change Learning Rate

```cpp
const double LEARNING_RATE = 0.9;  // Faster learning
```

## Viewing Results

### Vector Files (.vec)

Shows Q-value changes over time:

```bash
# Use OMNeT++ IDE or scavetool to plot Q-value evolution
scavetool x General-#0.vec -o output.csv
```

### Scalar Files (.sca)

Summary statistics of Q-values:

```bash
scavetool scalar General-#0.sca
```

## Understanding the Q-Values

**Q[destination][gate] = estimated total time to reach destination via that gate**

Example after learning:

```
Q[2][0] = 5.0    (wrong path, never converges)
Q[2][1] = 0.0002 (correct path, very low - DNS is on gate 1)
Q[2][2] = 3.2    (wrong path, tried during exploration)

Q[3][0] = 4.1    (wrong path)
Q[3][1] = 2.5    (wrong path)
Q[3][2] = 0.0002 (correct path, HTTP is on gate 2)
```

Lower Q-values = Better routes (router prefers these)

## Troubleshooting

### No Q-updates in log?

- Check that messages are actually being sent (PC should send at 0.5s)
- Verify router is receiving messages

### Q-values not converging?

- LEARNING_RATE too low → increase to 0.7-0.9
- EPSILON too high → decrease to 0.05
- Not enough simulation time → increase sim-time-limit

### All Q-values stay at 1.0?

- Check that Q-UPDATE feedback is being sent
- Verify timestamp parameter exists on messages

## Success Indicators

✓ Router logs show "Q-update" messages with changing values  
✓ Q-values for correct paths decrease below 0.1  
✓ Q-values for wrong paths stay high (>1.0)  
✓ Router consistently chooses correct gate after learning  
✓ Occasional exploration (10% of time with default EPSILON)

## Next Steps

1. Add more PCs to generate more traffic
2. Extend to multiple routers (mesh topology)
3. Add link failures to test adaptability
4. Implement discount factor (γ) for multi-hop paths
5. Add congestion metrics to Q-value calculation

Happy Q-Learning! 🤖📊
