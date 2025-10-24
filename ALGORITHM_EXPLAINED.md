# Q-Routing Algorithm Visualization

## The Q-Learning Formula (Implemented)

```
Q_new(dest, gate) = Q_old(dest, gate) + α × [delay + Q_min(dest, next_hop) - Q_old(dest, gate)]
                    └─────────────────┘   └─┘  └────┬─────┘ └──────┬──────────┘  └─────────────────┘
                         Current              │       │              │                 Current
                         Q-value        Learning│  Observed     Min Q-value         Q-value
                                          Rate  │   delay      at next hop
                                                │
                                          Temporal Difference Error
```

## Step-by-Step Example

### Network Topology

```
    PC (addr=1) ──── Router ──── DNS (addr=2)
                        │
                        └──────── HTTP (addr=3)
```

### Scenario: PC sends packet to DNS (destination=2)

#### **Iteration 1: No Knowledge**

**Before forwarding:**

```
Router Q-table (all initialized to 1.0):
Q[2][0] = 1.0  (gate 0 → PC, wrong)
Q[2][1] = 1.0  (gate 1 → DNS, correct!)
Q[2][2] = 1.0  (gate 2 → HTTP, wrong)
```

**Epsilon-greedy decision:**

- All equal, random choice → let's say gate 1 is chosen (lucky!)
- Forward packet via gate 1

**Feedback from DNS:**

- DNS is at destination, so its Q_min = 0.0
- DNS sends back: Q-UPDATE(dest=2, qValue=0.0)

**Delay calculation:**

- Packet sent at t=0.500s, received at t=0.5002s
- Delay = 0.0002s (0.2ms)

**Q-value update:**

```
Q_new[2][1] = Q_old[2][1] + α × [delay + Q_min - Q_old[2][1]]
            = 1.0 + 0.5 × [0.0002 + 0.0 - 1.0]
            = 1.0 + 0.5 × [-0.9998]
            = 1.0 - 0.4999
            = 0.5001
```

**After iteration 1:**

```
Q[2][0] = 1.0     (unexplored)
Q[2][1] = 0.5001  (learned: this is good!)
Q[2][2] = 1.0     (unexplored)
```

---

#### **Iteration 2: Some Knowledge**

**Epsilon-greedy decision:**

- 90% exploitation: choose minimum Q → gate 1 (Q=0.5001) ✓
- 10% exploration: random
- Let's say: exploitation wins → gate 1 chosen

**Feedback:**

- Same path, delay = 0.0002s
- DNS Q_min = 0.0

**Q-value update:**

```
Q_new[2][1] = 0.5001 + 0.5 × [0.0002 + 0.0 - 0.5001]
            = 0.5001 + 0.5 × [-0.4999]
            = 0.5001 - 0.24995
            = 0.25015
```

**After iteration 2:**

```
Q[2][0] = 1.0
Q[2][1] = 0.25015  (getting better!)
Q[2][2] = 1.0
```

---

#### **Iteration 5: Exploration!**

**Epsilon-greedy decision:**

- Random number = 0.08 < 0.1 → EXPLORATION!
- Random choice → gate 2 (HTTP, wrong path)

**Feedback:**

- HTTP is not destination 2, so it floods or drops
- High delay or timeout
- Let's say delay = 1.0s (no response)
- Q_min from HTTP's perspective to DNS = 2.0 (unknown path)

**Q-value update:**

```
Q_new[2][2] = 1.0 + 0.5 × [1.0 + 2.0 - 1.0]
            = 1.0 + 0.5 × [2.0]
            = 2.0
```

**After iteration 5:**

```
Q[2][0] = 1.0
Q[2][1] = 0.125    (converging to ~0.0002)
Q[2][2] = 2.0      (learned: this is bad!)
```

---

#### **Iteration 10: Converged**

**After many iterations:**

```
Q[2][0] = 1.0      (never tried)
Q[2][1] = 0.0002   (correct path, converged!)
Q[2][2] = 2.0      (wrong path, avoided)
```

**Routing decision:**

- Always (90%) choose gate 1 (minimum Q = 0.0002)
- Sometimes (10%) explore gates 0 or 2 (but Q-values keep them high)

---

## Multi-Hop Example (Extended Scenario)

### Topology with Multiple Routers

```
PC ── R1 ── R2 ── DNS
      │     │
    HTTP  Other nodes
```

### R2 → DNS (single hop)

```
R2: Q[DNS][gate_to_DNS] = 0.0002 (direct link)
```

### R1 → DNS (two hops)

**First packet:**

1. R1 forwards to R2
2. R2 sends Q-UPDATE back: qValue = 0.0002 (its best)
3. R1 delay to R2 = 0.0002
4. R1 update:

```
Q[DNS][gate_to_R2] = 1.0 + 0.5 × [0.0002 + 0.0002 - 1.0]
                   = 0.5002
```

**After convergence:**

```
R1: Q[DNS][gate_to_R2] = 0.0004 (two hops worth of delay)
R2: Q[DNS][gate_to_DNS] = 0.0002 (one hop)
```

This shows how Q-values accumulate along paths!

---

## Key Insights

### 1. **Self-Correcting**

- Wrong paths get high Q-values → avoided
- Right paths get low Q-values → preferred

### 2. **Exploration Prevents Local Optima**

- 10% random choices discover new paths
- If network changes, exploration finds new routes

### 3. **Convergence Speed**

- Learning rate α=0.5 → 50% adjustment each time
- Higher α → faster but oscillates
- Lower α → slower but stable

### 4. **Realistic Delays**

- Channel delay = 0.2ms per hop
- Q-values converge to sum of all hop delays
- Router can compare multi-hop paths

---

## Implementation Highlights

### In `router.cc`:

```cpp
// Select best gate (epsilon-greedy)
if (uniform(0,1) < EPSILON) {
    return randomGate();  // Explore
} else {
    return argmin(Q[dest]);  // Exploit
}

// Update Q-value
double oldQ = Q[dest][gate];
double newQ = oldQ + LEARNING_RATE * (delay + minNextQ - oldQ);
Q[dest][gate] = newQ;
```

### Message Flow:

```
Data Flow:    PC → R → DNS
              ──────────→

Q-Update:     PC ← R ← DNS
              ←───Q───
```

Each node sends feedback about its best Q-value upstream.

---

## Comparison: Static vs Q-Routing

### Static Routing

```
routes = "2:1, 3:2"  // Hardcoded
- Fast (no learning overhead)
- Brittle (fails if topology changes)
- Manual configuration
```

### Q-Routing

```
Q[2][0]=1.0, Q[2][1]=0.0002, Q[2][2]=2.0  // Learned
- Adaptive (handles topology changes)
- Self-organizing (no manual config)
- Exploration overhead (10% suboptimal)
- Converges to optimal over time
```

---

## Performance Metrics to Track

1. **Q-value convergence time**: How long until Q[dest][best_gate] < 0.001?
2. **Routing accuracy**: % of packets using optimal gate after learning
3. **Exploration cost**: % of packets sent on suboptimal paths
4. **Adaptation speed**: Time to re-learn after topology change

These can all be extracted from the `.vec` and `.sca` result files!

---

This implementation gives you a working Q-learning router that demonstrates the core principles of reinforcement learning for network routing. 🎓🚀
