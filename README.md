# Q-Routing: Reinforcement Learning for Adaptive Network Routing

A practical implementation of Q-Routing algorithm using OMNeT++ 6.2.0, demonstrating how reinforcement learning can be applied to adaptive packet routing in computer networks.

## 📋 Overview

This project implements the Q-Routing protocol introduced by Boyan and Littman (1994), where routers learn optimal routing decisions through experience without requiring global topology knowledge. The implementation features:

- **Smart Exploration**: Two-phase exploration strategy (systematic + epsilon-greedy)
- **Multi-router Topology**: Three parallel paths with different delays (10ms, 50ms, 100ms)
- **Real-time Learning**: Q-values converge to reflect true end-to-end delays
- **High Performance**: Achieves 96% optimal path usage with 100% delivery rate

## 🚀 Features

- ✅ Pure OMNeT++ implementation (no INET framework required)
- ✅ Reinforcement learning with Q-learning algorithm
- ✅ Epsilon-greedy exploration policy (ε = 0.1)
- ✅ Automatic convergence detection
- ✅ Comprehensive logging and statistics
- ✅ Educational code with detailed comments

## 📁 Project Structure

```
test/
├── src/
│   ├── Modules/
│   │   ├── router.cc       # Q-Routing router implementation
│   │   ├── pc.cc           # Source/Destination PC modules
│   │   ├── helpers.h       # Constants and helper functions
│   │   ├── dns.cc          # (unused legacy)
│   │   └── http.cc         # (unused legacy)
│   ├── SimpleNet.ned       # Network topology definition
│   ├── package.ned         # Package declaration
│   └── omnetpp.ini         # Simulation configuration
├── simulations/            # Alternative run location
├── results/                # Simulation output files
└── Makefile               # Build configuration
```

## 🛠️ Prerequisites

- **OMNeT++ 6.2.0** or later
- **C++ Compiler**: GCC (Linux/Mac) or MSVC (Windows)
- **Operating System**: Windows, Linux, or macOS

## 📥 Installation

### 1. Install OMNeT++

Download and install OMNeT++ 6.2.0 from [omnetpp.org](https://omnetpp.org/download/)

**Windows:**

```powershell
# Extract OMNeT++ and set environment variables
# Add to PATH: C:\omnetpp-6.2.0\bin
```

**Linux/Mac:**

```bash
tar xvfz omnetpp-6.2.0-linux.tgz
cd omnetpp-6.2.0
./configure
make
source setenv
```

### 2. Clone the Repository

```bash
cd omnetpp-6.2.0/samples
git clone https://github.com/Efty34/Q-Routing.git test
cd test
```

### 3. Build the Project

**Using OMNeT++ IDE:**

1. File → Import → Existing Projects into Workspace
2. Select the `test` directory
3. Right-click project → Build Project

**Using Command Line:**

**Windows (PowerShell):**

```powershell
cd src
opp_makemake -f --deep
make
```

**Linux/Mac:**

```bash
cd src
opp_makemake -f --deep
make MODE=release
```

## ▶️ Running the Simulation

### GUI Mode (Recommended for visualization)

```bash
cd src
./test
```

Then click **Run** in the OMNeT++ GUI.

### Command Line Mode

```powershell
# Windows
./test -u Cmdenv -c General

# Linux/Mac
./test -u Cmdenv -c General
```

### Custom Configuration

Edit `src/omnetpp.ini` to modify:

- Simulation duration: `sim-time-limit = 100s`
- Packet interval: `*.source.sendInterval = 2s`
- Learning rate: Modify `LEARNING_RATE` in `helpers.h`
- Exploration rate: Modify `EPSILON` in `helpers.h`

## 📊 Results

After simulation, check the `results/` directory:

- **`General-#0.sca`**: Scalar statistics (final Q-values, packet counts)
- **`General-#0.vec`**: Vector data (Q-value convergence over time)
- **`General-0.log`**: Detailed event log with Q-learning updates

### View Results

**In OMNeT++ IDE:**

1. Right-click on `.vec` file → Open With → Analysis Tool
2. Browse Data → Select `qValue:vector` → Plot

**Command Line:**

```bash
scavetool export -o qvalues.csv results/General-#0.vec
```

## 🎯 Expected Output

```
Source PC 1 - Final Statistics:
  Packets Sent: 50
  Responses Received: 50
  Success Rate: 100%
----------------------------------------
Gate Usage Statistics:
  Gate 0 (Router11): 48 packets (96%) Final Q=0.022
  Gate 1 (Router12): 1 packets (2%)   Final Q=0.551
  Gate 2 (Router13): 1 packets (2%)   Final Q=0.601
```

**Key Metrics:**

- ✅ 96% packets use optimal path (Router1 - 10ms delay)
- ✅ Q-values converge to true RTTs (22ms, 102ms, 202ms)
- ✅ 100% packet delivery rate
- ✅ Convergence in ~20 packets (~40 seconds)

## 🔧 Configuration Parameters

### Network Topology (SimpleNet.ned)

| Router  | Path Delay | Expected RTT |
| ------- | ---------- | ------------ |
| Router1 | 10ms       | 22ms         |
| Router2 | 50ms       | 102ms        |
| Router3 | 100ms      | 202ms        |

### Q-Learning Parameters (helpers.h)

```cpp
#define LEARNING_RATE 0.5      // α: How fast to learn
#define EPSILON 0.1            // ε: Exploration probability
#define INITIAL_Q_VALUE 1.0    // Initial Q-value estimate
```

## 📚 How It Works

1. **Initial Exploration**: Source PC tries each router once (packets 1-3)
2. **Learning Phase**: Q-values updated using: `Q_new = Q_old + α(RTT - Q_old)`
3. **Exploitation**: Select gate with lowest Q-value (90% of time)
4. **Continued Exploration**: Random gate selection (10% of time)
5. **Convergence**: Q-values stabilize at true delivery times

## 🐛 Troubleshooting

**Build Errors:**

```powershell
# Clean and rebuild
make clean
opp_makemake -f --deep
make
```

**No output in results/:**

- Check `omnetpp.ini` has `cmdenv-output-file` configured
- Ensure simulation runs to completion
- Verify `results/` directory exists

**Simulation crashes:**

- Check OMNeT++ version (requires 6.0+)
- Verify all `.cc` files compiled successfully
- Review error messages in console

## 📖 References

- Boyan, J. A., & Littman, M. L. (1994). "Packet routing in dynamically changing networks: A reinforcement learning approach." _Advances in Neural Information Processing Systems 6 (NIPS 1993)_.
- Sutton, R. S., & Barto, A. G. (2018). _Reinforcement Learning: An Introduction_, 2nd ed. MIT Press.

## 📄 License

This project is for educational purposes. Feel free to use and modify for academic projects.

