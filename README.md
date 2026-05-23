
<p align="center">
  <img src="siemenslogo.png" alt="Siemens Logo" height="60" />
  &nbsp;&nbsp;&nbsp;&nbsp;
  <img src="autosarlogo.png" alt="AUTOSAR Logo" height="60" />
</p>

---

# Fault Injection Framework

> An automated fault injection harness designed to test the resilience of the **AUTOSAR Memory Stack** — built as a graduation project in collaboration with **Siemens EDA**.

---

## Overview

This framework intercepts memory operations at runtime using a **non-intrusive macro-hooking mechanism**, enabling configurable, time-based fault injection without modifying the target stack source code. Test scenarios are driven entirely by JSON configuration files, making the harness reusable across different AUTOSAR NvM configurations.

---

## Features

- **Non-intrusive hooking** — macro-level interception with zero changes to the AUTOSAR stack
- **Configurable fault types**
  - Multi-bit flips
  - Burst errors
  - Stuck-at faults
- **Time-based injection** — faults triggered at precise offsets via deterministic timing
- **JSON-driven test cases** — define, repeat, and version-control test scenarios
- **Python GUI** — visual interface for test configuration and result inspection
- **NvM simulation** — standalone NvM module for isolated testing

---

## Project Structure

```
├── Fault_Injection_Framework/   # Core harness logic
├── FrameWork_Header_Files/      # Public headers
├── FrameWork_src_Files/         # Framework source implementation
├── src/                         # Target stack source (AUTOSAR NvM)
├── Include/                     # AUTOSAR include layer
├── GUI/                         # Python-based test GUI
└── NvmSimulation                # Standalone NvM simulation binary
```

---

## Tech Stack

| Layer | Technology |
|---|---|
| Core Framework | C (84%) |
| GUI & Tooling | Python (16%) |
| Test Configuration | JSON |
| Target Stack | AUTOSAR NvM / Memory Stack |

---

## How It Works

1. **Hook insertion** — macros replace memory read/write calls with interceptors at compile time
2. **Timing context** — each hook receives a context struct carrying injection policy (delay, fault type, target address)
3. **Fault dispatch** — on trigger condition, the harness corrupts the operation according to the configured fault model
4. **Result logging** — outcomes are recorded and surfaced via the GUI for pass/fail analysis

---

## Getting Started

```bash
# Clone the repo
git clone https://github.com/GP-Fault-Injetion/Fault_Injection_Framework.git

# Build the framework (GCC / MinGW)
cd Fault_Injection_Framework
make

# Run a test case
./NvmSimulation --config test_cases/multibit_flip.json
```

> **Note:** Pre-built test binaries (`test.exe`, `test2.exe`) are included for quick Windows evaluation.

---

## Example Test Case (JSON)

```json
{
  "fault_type": "multi_bit_flip",
  "target_address": "0x20001000",
  "delay_ms": 25,
  "affected_bits": [3, 7],
  "repeat": 5
}
```

---

## Fault Types Reference

| Fault | Description |
|---|---|
| `multi_bit_flip` | Flips multiple bits simultaneously in a memory word |
| `burst_error` | Injects a sequence of errors over a short time window |
| `stuck_at` | Forces a bit to remain 0 or 1 regardless of write operations |

---

## Team

Developed as a graduation project by students of [your university].
Industry collaboration: **Siemens EDA**

---

## License

*Source code available upon request.*
