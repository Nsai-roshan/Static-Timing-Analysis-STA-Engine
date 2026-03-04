# Static Timing Analysis Prototype

I built this project to learn the basics of static timing analysis and to practice building a parser-heavy graph algorithm project in C++.

This is a small educational STA prototype, not a production EDA tool. I kept the scope intentionally narrow so I could understand the core ideas well and explain them clearly.

## What I Built

I wrote a C++17 command-line tool that:

- parses a small subset of gate-level Verilog
- parses a simplified Liberty timing library
- parses basic SDC-style timing constraints
- builds a timing graph
- computes arrival time, required time, and slack
- reports a few timing paths

I also included:

- one basic example design
- one regression test

## Short Explanation

If I had to explain this project quickly in an interview, I would say:

> I built a small static timing analysis prototype in C++. It parses a simplified netlist, timing library, and constraint file, builds a DAG, propagates timing forward and backward, and reports setup and hold slack.

That is the level I would present it at for most new-grad software roles.

## Why I Think This Is A Good New-Grad Project

I think this project is strong because it shows:

- C++ implementation skill
- parser design
- graph modeling
- graph traversal algorithms
- clear handling of inputs, outputs, and reports

At the same time, I do not claim that it is a full STA engine. I present it as a focused prototype that covers the main concepts.

## What The Program Does

I simplified the core flow into a few steps:

1. I parse the input files.
2. I create graph nodes for ports and instance pins.
3. I add edges for timing arcs and net connections.
4. I seed timing startpoints.
5. I run a forward pass to compute arrival times.
6. I run a backward pass to compute required times.
7. I compute setup and hold slack.
8. I print path reports.

## Input And Output

### Input

The program needs three input files:

1. A Liberty file: cell timing model
2. A Verilog netlist: gate-level circuit
3. A constraint file: clock and I/O timing constraints

Command-line format:

```text
sta --liberty <file.lib> --netlist <file.v> --constraints <file.sdc> [--report-paths N]
```

### Output

The program prints a text timing report to the terminal.

The report contains:

- design summary
- number of checked paths
- worst setup slack
- worst hold slack
- setup path details
- hold path details

Typical fields in the output are:

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

## Example Inputs

I included one working example in `examples/basic/`.

The files are:

- `examples/basic/basic.lib`
- `examples/basic/basic.v`
- `examples/basic/basic.sdc`

## Supported Input Subset

I intentionally support a limited subset of each file format.

### Liberty

I support a small subset including:

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

I support:

- ANSI-style module headers
- `input`, `output`, and `wire` declarations
- named-pin gate instantiations
- simple `assign` statements

### Constraints

I support a basic SDC-style subset:

- `create_clock`
- `set_clock_uncertainty`
- `set_input_delay`
- `set_output_delay`

## Timing Concepts I Used

The main timing ideas in this project are:

- `Arrival time`
- `Required time`
- `Setup slack`
- `Hold slack`
- `Startpoint`
- `Endpoint`
- `Critical path`

I only implemented the basic versions of these ideas.

## What I Did Not Build

I think this section is important because it keeps the project honest.

I did not build:

- a full Liberty parser
- detailed delay table interpolation
- RC wire modeling
- propagated clock trees
- advanced signoff timing features
- a production-quality constraint engine

So if someone asks, I would describe this as:

> a learning-oriented STA prototype that demonstrates the main graph and timing ideas

not:

> a replacement for commercial STA tools

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

## How To Build

### Direct g++

Build the main program:

```powershell
g++ -std=gnu++17 -Iinclude src\engine.cpp src\main.cpp -o sta.exe
```

Build the test binary:

```powershell
g++ -std=gnu++17 -Iinclude src\engine.cpp tests\test_sta.cpp -o sta_tests.exe
```

### With CMake

```bash
cmake -S . -B build
cmake --build build
```

## How To Run

From the project root, run:

```powershell
.\sta.exe --liberty examples\basic\basic.lib `
          --netlist examples\basic\basic.v `
          --constraints examples\basic\basic.sdc `
          --report-paths 3
```

## What I Should See

When the program works, I should see:

- a summary banner called `STA Prototype Summary`
- a nonzero `Checked Paths` count
- setup and hold slack values
- at least a few path sections

Example of the kind of output I should see:

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

If `Checked Paths` is `0`, then something is wrong with parsing, connectivity, or the input files.

## Testing

I included a small regression test.

Build:

```powershell
g++ -std=gnu++17 -Iinclude src\engine.cpp tests\test_sta.cpp -o sta_tests.exe
```

Run:

```powershell
.\sta_tests.exe
```

If the test passes, it prints nothing and exits successfully.

The test checks:

- file parsing
- timing graph construction
- end-to-end report generation

## How I Would Explain The Design

If I had to explain the design in a simple way, I would say:

- I parse the timing library, netlist, and constraints into in-memory structures.
- I model the circuit as a directed acyclic graph.
- I use a topological forward pass to compute arrival times.
- I use a backward pass to compute required times.
- I compute setup and hold slack at valid endpoints.
- I reconstruct a few timing paths for the final report.

That is the core of the project. I would stay at that level unless the interviewer asks for more detail.

## Interview Prep Notes

If I want to use this repo later to prepare for interviews, these are the main questions I should be ready for.

### What problem does this project solve?

It analyzes timing in a small gate-level digital design. Given a netlist, a cell timing library, and constraints, it estimates when signals arrive, when they are required, and how much slack each path has.

### What data structure did I use?

I used a directed acyclic graph where:

- nodes represent ports or instance pins
- edges represent timing arcs or net connections

### Why did I use a DAG?

Because timing propagation is naturally a graph problem, and topological traversal makes forward and backward timing computation straightforward for combinational logic between sequential boundaries.

### What algorithms did I use?

The main algorithm is topological traversal:

- forward traversal for arrival times
- backward traversal for required times

### What are the most important outputs?

The most important outputs are:

- setup slack
- hold slack
- worst paths

### What limitations should I say clearly?

I should say clearly that:

- this is a prototype
- it supports a simplified subset of Liberty, Verilog, and SDC
- it does not implement advanced signoff features

### What is the best way to describe this for SWE roles?

I would describe it as:

> a C++ parser and graph algorithms project with domain-specific timing logic

### What is the best way to describe this for EDA roles?

I would describe it as:

> a small static timing analysis prototype built to learn timing graph construction and slack computation

## How I Would Present It On A Resume

I would keep the wording simple and honest.

Good version:

- Built a C++ static timing analysis prototype that parses simplified Verilog, Liberty, and SDC-style inputs.
- Modeled timing as a DAG and implemented forward/backward graph traversals to compute arrival time, required time, and setup/hold slack.
- Added timing path reporting for a small gate-level example design.

I would avoid wording that makes it sound like a signoff-quality industrial tool.

## Best Interview Framing

For general software engineering roles, I would frame this project as:

- a C++ parser and graph algorithms project

For EDA-related roles, I would frame it as:

- a small STA prototype built to learn timing analysis fundamentals

That way, I can match the explanation to the audience without overselling it.

## Final Take

I do think this topic can get deep very quickly, so I do not want to oversell it on GitHub.

The safer and better framing is:

- I built a small, clean, educational STA prototype
- I understand the main algorithmic ideas
- I can explain the design choices and limitations

I think that is a much better position for a new grad than trying to present this as a full EDA tool.
