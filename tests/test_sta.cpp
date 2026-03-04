#include "sta/engine.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool write_file(const std::string& path, const std::string& content) {
    std::ofstream output(path.c_str());
    if (!output) {
        return false;
    }
    output << content;
    return true;
}

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Test failure: " << message << "\n";
        std::exit(1);
    }
}

}  // namespace

int main() {
    const std::string liberty =
        "library(simple) {\n"
        "  cell(BUF) {\n"
        "    pin(A) { direction : input; }\n"
        "    pin(Y) {\n"
        "      direction : output;\n"
        "      timing() {\n"
        "        related_pin : \"A\";\n"
        "        timing_type : combinational;\n"
        "        cell_rise(scalar) { values(\"0.20\"); }\n"
        "        cell_fall(scalar) { values(\"0.10\"); }\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "  cell(DFFQ) {\n"
        "    pin(CLK) { direction : input; clock : true; }\n"
        "    pin(D) {\n"
        "      direction : input;\n"
        "      timing() {\n"
        "        related_pin : \"CLK\";\n"
        "        timing_type : setup_rising;\n"
        "        rise_constraint(scalar) { values(\"0.30\"); }\n"
        "      }\n"
        "      timing() {\n"
        "        related_pin : \"CLK\";\n"
        "        timing_type : hold_rising;\n"
        "        rise_constraint(scalar) { values(\"0.10\"); }\n"
        "      }\n"
        "    }\n"
        "    pin(Q) {\n"
        "      direction : output;\n"
        "      timing() {\n"
        "        related_pin : \"CLK\";\n"
        "        timing_type : rising_edge;\n"
        "        cell_rise(scalar) { values(\"0.40\"); }\n"
        "        cell_fall(scalar) { values(\"0.20\"); }\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "}\n";

    const std::string netlist =
        "module top(input clk, input in1, output out1);\n"
        "  wire q0;\n"
        "  wire n1;\n"
        "  DFFQ u_ff0 ( .CLK(clk), .D(in1), .Q(q0) );\n"
        "  BUF  u_buf ( .A(q0), .Y(n1) );\n"
        "  DFFQ u_ff1 ( .CLK(clk), .D(n1), .Q(out1) );\n"
        "endmodule\n";

    const std::string sdc =
        "create_clock -name CLK -period 2.0 [get_ports clk]\n"
        "set_clock_uncertainty 0.05 [get_clocks CLK]\n"
        "set_input_delay 0.20 -clock CLK [get_ports in1]\n"
        "set_output_delay 0.30 -clock CLK [get_ports out1]\n";

    expect(write_file("test.lib", liberty), "write liberty");
    expect(write_file("test.v", netlist), "write netlist");
    expect(write_file("test.sdc", sdc), "write constraints");

    sta::StaEngine engine;
    expect(engine.load_liberty("test.lib"), "load liberty");
    expect(engine.load_netlist("test.v"), "load netlist");
    expect(engine.load_constraints("test.sdc"), "load constraints");
    expect(engine.build_timing_graph(), "build timing graph");
    expect(engine.analyze(), "analyze");

    expect(!engine.setup_reports().empty(), "setup report exists");
    expect(!engine.hold_reports().empty(), "hold report exists");
    expect(engine.summary().path_count >= 2U, "path count");
    expect(engine.build_summary_report().find("STA Prototype Summary") != std::string::npos, "summary text");

    return 0;
}
