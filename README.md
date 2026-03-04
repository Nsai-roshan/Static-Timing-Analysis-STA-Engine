# Static Timing Analysis Prototype

A compact C++17 prototype for static timing analysis of small gate-level digital designs.

This project parses a simplified Liberty timing library, a gate-level Verilog netlist, and a basic SDC-style constraint file, then builds a timing graph and computes:

- arrival time
- required time
- setup slack
- hold slack
- path-level timing reports

## Features

- Parses simplified Liberty, gate-level Verilog, and SDC-style constraints
- Builds a directed acyclic timing graph from cell arcs and net connections
- Uses forward traversal for arrival time propagation
- Uses backward traversal for required time propagation
- Computes setup and hold slack
- Reconstructs reported timing paths
- Includes a small example design and regression test

## Tech Stack

- `C++17`
- `CMake`
- `g++`

## Inputs

The executable requires three input files:

1. Liberty `.lib`
   - cell timing model
2. Verilog `.v`
   - gate-level netlist
3. Constraint `.sdc`
   - clock and I/O timing constraints

Command format:

```text
sta --liberty <file.lib> --netlist <file.v> --constraints <file.sdc> [--report-paths N]
```

## Supported Input Subset

### Liberty

Supported items:

- `cell(...)`
- `pin(...)`
- `direction`
- `clock : true`
- `timing()`
- `related_pin`
- `timing_type`
- `cell_rise`
- `cell_fall`
- `rise_constraint`
- `fall_constraint`

### Verilog

Supported items:

- ANSI-style module headers
- `input`, `output`, and `wire` declarations
- named-pin gate instantiations
- simple `assign` statements

### Constraints

Supported commands:

- `create_clock`
- `set_clock_uncertainty`
- `set_input_delay`
- `set_output_delay`

## Output

The program prints a timing report to the terminal.

The report includes:

- design summary
- checked path count
- worst setup slack
- worst hold slack
- setup path details
- hold path details

Typical fields:

- `Design`
- `Instance Count`
- `Clock Count`
- `Checked Paths`
- `WNS (setup)`
- `WNS (hold)`
- `Path Type`
- `Startpoint`
- `Endpoint`
- `Arrival`
- `Required`
- `Slack`

## STA Flow

```text
Liberty (.lib) ----\
                    \
Verilog (.v) -------> Parse Inputs ---> Build Timing Graph ---> Forward Pass ---> Arrival Times
                    /                                            |
SDC (.sdc) -------/                                             v
                                                      Initialize Endpoint Requirements
                                                                    |
                                                                    v
                                                            Backward Pass
                                                                    |
                                                                    v
                                                         Required Times + Slack
                                                                    |
                                                                    v
                                                              Text Report
```

## Architecture

```text
                    +------------------+
                    |    src/main.cpp  |
                    | CLI entry point  |
                    +---------+--------+
                              |
                              v
                    +------------------------+
                    |   StaEngine interface  |
                    | include/sta/engine.hpp |
                    +-----------+------------+
                                |
                                v
  +-------------------------------------------------------------------+
  |                         src/engine.cpp                             |
  |-------------------------------------------------------------------|
  | Liberty parser                                                    |
  | Verilog parser                                                    |
  | Constraint parser                                                 |
  | Timing graph construction                                         |
  | Forward arrival propagation                                       |
  | Backward required propagation                                     |
  | Slack calculation and path reporting                              |
  +-------------------------------------------------------------------+
                                |
                                v
                    +-------------------------+
                    |   Timing graph model    |
                    | nodes, edges, states    |
                    +-------------------------+
```

## Example Design

The repository includes a working example under `examples/basic/`:

- `examples/basic/basic.lib`
- `examples/basic/basic.v`
- `examples/basic/basic.sdc`

## Build

### Direct g++

Build the main executable:

```powershell
g++ -std=gnu++17 -Iinclude src\engine.cpp src\main.cpp -o sta.exe
```

Build the test executable:

```powershell
g++ -std=gnu++17 -Iinclude src\engine.cpp tests\test_sta.cpp -o sta_tests.exe
```

### With CMake

```bash
cmake -S . -B build
cmake --build build
```

## Run

From the project root:

```powershell
.\sta.exe --liberty examples\basic\basic.lib `
          --netlist examples\basic\basic.v `
          --constraints examples\basic\basic.sdc `
          --report-paths 3
```

## Expected Output

For the bundled example, the output should look similar to:

```text
============================================================
STA Prototype Summary
============================================================
Design              : basic_top
Instance Count      : 3
Clock Count         : 1
Checked Paths       : 4
WNS (setup)         : 1.780
WNS (hold)          : 0.140
...
```

A successful run should show:

- a nonzero `Checked Paths` value
- setup and hold sections
- path-level timing details

## Testing

Build and run the regression test:

```powershell
g++ -std=gnu++17 -Iinclude src\engine.cpp tests\test_sta.cpp -o sta_tests.exe
.\sta_tests.exe
```

If the test passes, it exits without printing an error.

## Project Structure

```text
.
|-- CMakeLists.txt
|-- include/sta/engine.hpp
|-- src/engine.cpp
|-- src/main.cpp
|-- tests/test_sta.cpp
`-- examples/basic/
```

## Limitations

This project is intentionally limited to prototype scope.

It does not implement:

- full Liberty parsing
- advanced delay table interpolation
- wire RC modeling
- propagated clock tree analysis
- latch-based timing
- signoff-oriented timing features

## Summary

This project focuses on the core mechanics of static timing analysis: parsing timing inputs, building a timing graph, propagating timing information, and generating readable setup and hold reports for a small gate-level design.
