# Q-Routing Implementation for OMNeT++

## Overview

This implementation adds AI-driven adaptive routing using Q-learning to your OMNeT++ network simulation. The router now learns optimal paths dynamically based on observed transmission delays rather than using static routing tables.

## What Was Changed

### 1. **helpers.h** - Core Q-Routing Infrastructure

- Added `Q_UPDATE` message type (kind=30) for Q-learning feedback
- Added `sendTime` parameter to all messages for delay calculation
- Defined Q-routing constants:
  - `INITIAL_Q_VALUE = 1.0` (initial estimate in seconds)
  - `LEARNING_RATE = 0.5` (α - how quickly to learn from new information)
  - `EPSILON = 0.1` (10% exploration rate for epsilon-greedy policy)

### 2. **router.cc** - Q-Learning Implementation

Completely redesigned the Router module with:

#### Q-Table Structure

- `map<long, map<int, double>> qTable`
- Stores Q-values: Q[destination][neighbor_gate] = estimated time to reach destination
- Lower Q-values indicate better (faster) routes

#### Core Q-Learning Algorithm

```
Q(s,a) = Q(s,a) + α × [observed_delay + min_Q(next_hop) - Q(s,a)]
```

Where:

- `s` = current state (destination we're routing to)
- `a` = action (which gate to forward through)
- `α` = learning rate (LEARNING_RATE = 0.5)
- `observed_delay` = actual transmission time measured
- `min_Q(next_hop)` = best Q-value at the next router

#### Epsilon-Greedy Selection

- 90% exploitation: choose the gate with lowest Q-value (best known route)
- 10% exploration: randomly select a gate to discover potentially better routes

#### Feedback Mechanism

- When a router receives a data packet, it:
  1. Selects the best output gate using epsilon-greedy
  2. Sends Q-UPDATE feedback to the previous hop with its best Q-value
  3. Forwards the data packet
- When receiving Q-UPDATE:
  - Calculates transmission delay
  - Updates Q-value using Q-learning formula

### 3. **SimpleNet.ned** - Statistics Collection

- Added `@signal[qValue]` for Q-value monitoring
- Added `@statistic[qValue]` to record Q-value changes over time
- Results saved in `.vec` and `.sca` files

### 4. **Module Updates** (dns.cc, http.cc, pc.cc)

- All endpoint modules now ignore Q-UPDATE messages
- Messages automatically include timestamps via `mk()` helper

## How Q-Routing Works

### Initial State

- All Q-values start at 1.0 seconds (optimistic estimate)
- Router has no knowledge of best paths

### Learning Process

1. **PC sends DNS query** → Router must decide which gate to use
2. **Epsilon-greedy decision**: Usually picks lowest Q-value, sometimes explores randomly
3. **Router forwards packet** and sends Q-UPDATE feedback to previous hop
4. **Feedback contains**: destination address + router's best Q-value to that destination
5. **Previous hop updates Q-table** using observed delay + received Q-value
6. **Gradual convergence**: Over many packets, Q-values converge to actual path delays

### Example Scenario

```
PC → Router → DNS (destination=2)

Iteration 1: Q[2][0]=1.0, Q[2][1]=1.0, Q[2][2]=1.0 (all equal, random choice)
→ Router picks gate 1 (DNS is actually on gate 1)
→ Delay = 0.2ms, DNS sends Q=0.0 (it's at destination)
→ Update: Q[2][1] = 1.0 + 0.5×(0.0002 + 0.0 - 1.0) ≈ 0.5

Iteration 2: Q[2][1]=0.5 is now best, exploitation picks gate 1
→ Reinforces that gate 1 is correct path
→ Q[2][1] gradually converges to actual delay

Wrong paths:
If gate 0 or 2 were tried, higher delays would increase their Q-values,
making them less likely to be chosen in future.
```

## Benefits of This Implementation

1. **Self-Learning**: No manual routing configuration needed
2. **Adaptive**: Automatically adjusts to network conditions
3. **Robust**: Explores alternative paths periodically
4. **Observable**: Q-values recorded for analysis

## Running the Simulation

```bash
# Build the project
cd src
make

# Run simulation
cd ../simulations
../src/test

# View results
# Check .vec files for Q-value evolution over time
# Check .sca files for statistics
```

## Tuning Parameters (in helpers.h)

- **LEARNING_RATE (α)**:

  - Higher (0.7-0.9) = faster learning but more oscillation
  - Lower (0.1-0.3) = slower learning but more stable
  - Default: 0.5 (balanced)

- **EPSILON (ε)**:

  - Higher (0.2-0.3) = more exploration, slower convergence
  - Lower (0.01-0.05) = less exploration, faster convergence but may miss better paths
  - Default: 0.1 (10% exploration)

- **INITIAL_Q_VALUE**:
  - Should be optimistic (higher than expected delays)
  - Encourages exploration of all paths initially
  - Default: 1.0 seconds

## Expected Behavior

1. **Early simulation**: High variance in routing decisions (exploration)
2. **Mid simulation**: Q-values start converging to optimal paths
3. **Late simulation**: Mostly uses optimal paths with occasional exploration
4. **Logs**: Watch for "Router Q-update" messages showing learning process

## Observing Learning

Check the log output for messages like:

```
Router Q-update: dest=2 gate=1 oldQ=1.0 newQ=0.5
Router forwarding to dest=2 via gate=1 (Q=0.5)
```

This shows the router learning and adapting its routing decisions!

## Limitations (Simplified Implementation)

- Single router topology (can be extended to multiple routers)
- No discount factor (γ=0) - only considers immediate next hop
- Fixed network topology (doesn't handle link failures)
- Symmetric delays assumed

This is a pedagogical implementation demonstrating Q-learning principles for routing, suitable for understanding the core concepts without full MANET complexity.
