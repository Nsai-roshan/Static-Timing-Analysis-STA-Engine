# Static Timing Analysis Engine

A C++17 static timing analysis engine for gate-level digital designs. The tool parses a gate-level Verilog netlist, a Liberty timing library, and SDC-style timing constraints, builds a timing DAG, propagates arrival and required times, computes setup and hold slack, and emits path-oriented timing reports.

This project is designed to demonstrate the core ideas behind industrial STA tools in a recruiter-friendly, inspectable codebase:

- timing graph construction from netlist and library data
- topological forward and backward traversal
- setup and hold analysis
- multi-clock path checking
- critical path reconstruction
- incremental timing updates after arc-delay changes

## What The Tool Does

Given:

- a Liberty `.lib` file with cell pin directions and timing arcs
- a gate-level Verilog `.v` netlist
- an SDC-style Tcl constraint file

the engine will:

1. Build timing graph nodes for ports and instance pins.
2. Add graph edges for combinational cell arcs and net connectivity.
3. Seed startpoints from input delays and register clock-to-Q arcs.
4. Propagate maximum arrival times for setup analysis.
5. Propagate minimum arrival times for hold analysis.
6. Back-propagate required times from outputs and register data pins.
7. Compute slack on all valid endpoints.
8. Reconstruct the worst setup and hold paths.
9. Re-run timing after delay overrides, with optional incremental update mode.

## Timing Concepts Used

- `Arrival time`: when data reaches a node.
- `Required time`: latest or earliest legal time for that node.
- `Setup slack`: `required_max - arrival_max`.
- `Hold slack`: `arrival_min - required_min`.
- `WNS`: worst negative slack.
- `TNS`: total negative slack.
- `Startpoint`: primary input or sequential output pin.
- `Endpoint`: primary output or sequential data input pin.
- `Critical path`: the path with the worst slack.

## Supported Input Subset

The parser is intentionally practical rather than full-language complete. It supports the subset needed for realistic demos and coursework-quality gate-level STA flows.

### Liberty

Supported:

- `cell(...)`
- `pin(...)`
- `direction`
- `clock : true`
- `timing()`
- `related_pin`
- `timing_type`
- `timing_sense`
- `cell_rise`
- `cell_fall`
- `rise_constraint`
- `fall_constraint`

Recognized timing types:

- `combinational`
- `rising_edge`
- `falling_edge`
- `setup_rising`
- `hold_rising`

### Verilog

Supported:

- ANSI-style module headers
- separate `input`, `output`, and `wire` declarations
- named-pin gate instantiations such as `.A(net)`
- simple `assign lhs = rhs;`

Assumptions:

- the combinational portion of the timing graph is acyclic
- sequential loops are cut by register boundaries
- the netlist is already gate-level

### Constraints

Supported SDC-style commands:

- `create_clock`
- `set_clock_uncertainty`
- `set_input_delay`
- `set_output_delay`
- `set_clock_groups -group {...}`

## Project Layout

```text
.
|-- CMakeLists.txt
|-- include/sta/engine.hpp
|-- src/engine.cpp
|-- src/main.cpp
|-- tests/test_sta.cpp
|-- examples/basic/
|-- examples/multiclock/
`-- scripts/sta_shell.tcl
```

## Build

### With CMake

```bash
cmake -S . -B build
cmake --build build
```

Binaries:

- `sta`: command-line STA engine
- `sta_tests`: regression test binary

### Direct g++

```bash
g++ -std=gnu++17 -Iinclude src/engine.cpp src/main.cpp -o sta
g++ -std=gnu++17 -Iinclude src/engine.cpp tests/test_sta.cpp -o sta_tests
```

## Run The Analyzer

```bash
./sta --liberty examples/basic/basic.lib \
      --netlist examples/basic/basic.v \
      --constraints examples/basic/basic.sdc \
      --report-paths 3
```

Windows PowerShell:

```powershell
.\sta.exe --liberty examples\basic\basic.lib `
          --netlist examples\basic\basic.v `
          --constraints examples\basic\basic.sdc `
          --report-paths 3
```

## Example Output

The report includes:

- summary metrics
- number of checked paths
- WNS and TNS
- setup and hold violations
- path-by-path breakdown with node sequence and arrival values

Typical sections look like:

- `Path Type`
- `Startpoint`
- `Endpoint`
- `Launch Clock`
- `Capture Clock`
- `Arrival`
- `Required`
- `Slack`
- detailed path points

## Incremental Timing Update

The engine supports delay overrides on existing cell arcs.

Override file format:

```text
<instance_name> <from_pin->to_pin> <max_delay> <min_delay>
```

Example:

```text
u_and A->Y 0.55 0.48
```

Run with incremental update:

```bash
./sta --liberty examples/basic/basic.lib \
      --netlist examples/basic/basic.v \
      --constraints examples/basic/basic.sdc \
      --override-file examples/basic/override.txt \
      --incremental
```

What happens:

- matching arc delays are updated
- affected forward cone timing is recomputed
- affected backward cone required times are recomputed
- reports are regenerated

## Multi-Clock Support

The engine tracks timing states per launch clock domain and evaluates endpoints against their capture clocks.

Supported behaviors:

- register-to-register timing across different clocks
- input-to-register timing
- register-to-output timing
- asynchronous clock grouping via `set_clock_groups`

The `examples/multiclock` design demonstrates cross-domain analysis.

## Testing

Run the unit-style regression binary:

```bash
./sta_tests
```

What the test checks:

- Liberty loading
- Verilog loading
- constraint parsing
- timing graph construction
- end-to-end setup and hold report generation

## Tcl Driver

The repository includes a small Tcl wrapper:

```bash
tclsh scripts/sta_shell.tcl <liberty> <netlist> <constraints> ?extra args...?
```

It forwards arguments to the compiled CLI so the project still has a Tcl entry point without needing a custom embedded Tcl shell.

## Implementation Notes

The timing engine models:

- nodes: ports and instance pins
- edges: combinational arcs and net edges
- startpoints: input delays and clock-to-Q arcs
- endpoints: primary outputs and sequential data pins

Core algorithm:

1. Parse design data.
2. Build timing DAG.
3. Topologically sort the DAG.
4. Forward-propagate max and min arrival times.
5. Initialize endpoint required times.
6. Reverse-propagate required times.
7. Compute setup and hold slack.
8. Reconstruct critical paths from predecessor edges.

## Current Limitations

This is a serious educational STA engine, but it is not a full commercial parser or signoff tool.

Current limitations:

- no full Liberty grammar support
- no slew or load interpolation tables
- no wire RC modeling
- no propagated clock tree analysis
- no latch transparency analysis
- no CPPR, OCV, AOCV, or POCV
- no false-path or multicycle-path coverage beyond basic asynchronous clock grouping

## Why This Project Is Valuable

This project demonstrates:

- graph modeling of hardware timing
- parser construction for hardware design formats
- static graph algorithms
- timing signoff concepts used in EDA
- performance-oriented incremental recomputation

For interviews, it gives you concrete material to discuss around:

- STA graph construction
- setup/hold equations
- path-based reporting
- multi-clock reasoning
- incremental analysis tradeoffs

## Validation In This Environment

Validated locally with:

- `g++ 6.3.0`
- direct compilation using `g++ -std=gnu++17`
- `sta_tests.exe`
- sample runs on `examples/basic` and `examples/multiclock`

`cmake` and `tclsh` were not installed in the current environment, so CMake/Tcl commands were prepared but not executed here.
