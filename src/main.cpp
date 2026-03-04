#include "sta/engine.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void print_usage() {
    std::cout
        << "Usage: sta --liberty <file.lib> --netlist <file.v> --constraints <file.sdc>\n"
        << "           [--report-paths <count>] [--override-file <file>] [--incremental]\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string liberty_path;
    std::string netlist_path;
    std::string constraint_path;
    std::string override_path;
    std::size_t report_paths = 3;
    bool incremental = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--liberty" && i + 1 < argc) {
            liberty_path = argv[++i];
        } else if (arg == "--netlist" && i + 1 < argc) {
            netlist_path = argv[++i];
        } else if (arg == "--constraints" && i + 1 < argc) {
            constraint_path = argv[++i];
        } else if (arg == "--override-file" && i + 1 < argc) {
            override_path = argv[++i];
        } else if (arg == "--report-paths" && i + 1 < argc) {
            report_paths = static_cast<std::size_t>(std::atoi(argv[++i]));
        } else if (arg == "--incremental") {
            incremental = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage();
            return 1;
        }
    }

    if (liberty_path.empty() || netlist_path.empty() || constraint_path.empty()) {
        print_usage();
        return 1;
    }

    sta::StaEngine engine;
    if (!engine.load_liberty(liberty_path)) {
        std::cerr << "Failed to load Liberty file: " << engine.last_error() << "\n";
        return 1;
    }
    if (!engine.load_netlist(netlist_path)) {
        std::cerr << "Failed to load netlist: " << engine.last_error() << "\n";
        return 1;
    }
    if (!engine.load_constraints(constraint_path)) {
        std::cerr << "Failed to load constraints: " << engine.last_error() << "\n";
        return 1;
    }
    if (!engine.build_timing_graph()) {
        std::cerr << "Failed to build timing graph: " << engine.last_error() << "\n";
        return 1;
    }
    if (!engine.analyze()) {
        std::cerr << "Analysis failed: " << engine.last_error() << "\n";
        return 1;
    }

    if (!override_path.empty()) {
        std::vector<sta::DelayOverride> overrides;
        std::string error;
        if (!sta::StaEngine::parse_override_file(override_path, &overrides, &error)) {
            std::cerr << error << "\n";
            return 1;
        }
        if (!engine.apply_delay_overrides(overrides, incremental)) {
            std::cerr << "Override application failed: " << engine.last_error() << "\n";
            return 1;
        }
    }

    std::cout << engine.build_summary_report();
    std::cout << engine.build_path_report(report_paths);
    return 0;
}
