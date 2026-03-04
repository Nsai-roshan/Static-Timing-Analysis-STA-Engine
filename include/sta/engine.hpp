#ifndef STA_ENGINE_HPP
#define STA_ENGINE_HPP

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sta {

enum class PinDirection {
    Input,
    Output,
    Inout,
    Unknown
};

struct LibertyPin {
    std::string name;
    PinDirection direction = PinDirection::Unknown;
    bool is_clock = false;
};

struct LibertyCombArc {
    std::string from_pin;
    std::string to_pin;
    double max_delay = 0.0;
    double min_delay = 0.0;
    std::string timing_sense;
};

struct LibertyConstraintArc {
    std::string data_pin;
    std::string clock_pin;
    double setup = 0.0;
    double hold = 0.0;
};

struct LibertyClockToQArc {
    std::string clock_pin;
    std::string output_pin;
    double max_delay = 0.0;
    double min_delay = 0.0;
};

struct LibertyCell {
    std::string name;
    std::unordered_map<std::string, LibertyPin> pins;
    std::vector<LibertyCombArc> comb_arcs;
    std::vector<LibertyConstraintArc> constraint_arcs;
    std::vector<LibertyClockToQArc> clock_to_q_arcs;

    bool is_sequential() const;
};

struct Instance {
    std::string type;
    std::string name;
    std::unordered_map<std::string, std::string> pin_to_net;
};

struct Netlist {
    std::string module_name;
    std::set<std::string> inputs;
    std::set<std::string> outputs;
    std::set<std::string> wires;
    std::vector<Instance> instances;
    std::vector<std::pair<std::string, std::string> > assigns;
};

struct Clock {
    std::string name;
    std::string port_name;
    double period = 0.0;
    double uncertainty = 0.0;
};

struct PortDelay {
    std::string port_name;
    std::string clock_name;
    bool has_max = false;
    bool has_min = false;
    double max_delay = 0.0;
    double min_delay = 0.0;
};

struct ConstraintSet {
    std::unordered_map<std::string, Clock> clocks_by_name;
    std::unordered_map<std::string, std::string> port_to_clock;
    std::unordered_map<std::string, PortDelay> input_delays;
    std::unordered_map<std::string, PortDelay> output_delays;
    std::set<std::pair<std::string, std::string> > false_clock_pairs;
};

struct PathPoint {
    std::string node_name;
    std::string edge_name;
    double arrival_time = 0.0;
};

struct PathReport {
    std::string analysis_type;
    std::string startpoint;
    std::string endpoint;
    std::string launch_clock;
    std::string capture_clock;
    double arrival_time = 0.0;
    double required_time = 0.0;
    double slack = 0.0;
    std::vector<PathPoint> points;
};

struct AnalysisSummary {
    double worst_setup_slack = 0.0;
    double worst_hold_slack = 0.0;
    double total_negative_setup_slack = 0.0;
    double total_negative_hold_slack = 0.0;
    std::size_t setup_violations = 0;
    std::size_t hold_violations = 0;
    std::size_t path_count = 0;
};

class StaEngine {
public:
    bool load_liberty(const std::string& path);
    bool load_netlist(const std::string& path);
    bool load_constraints(const std::string& path);

    bool build_timing_graph();
    bool analyze();

    std::string last_error() const;
    const AnalysisSummary& summary() const;
    const std::vector<PathReport>& setup_reports() const;
    const std::vector<PathReport>& hold_reports() const;

    std::string build_summary_report() const;
    std::string build_path_report(std::size_t max_paths_per_type) const;

private:
    struct Edge {
        int id = -1;
        int from = -1;
        int to = -1;
        double max_delay = 0.0;
        double min_delay = 0.0;
        std::string name;
        std::string kind;
        std::string instance_name;
        std::string from_pin;
        std::string to_pin;
    };

    struct Node {
        int id = -1;
        std::string name;
        std::string instance_name;
        std::string pin_name;
        bool is_port = false;
        bool is_input_port = false;
        bool is_output_port = false;
    };

    struct DomainTiming {
        bool max_valid = false;
        bool min_valid = false;
        bool req_max_valid = false;
        bool req_min_valid = false;
        double arrival_max = 0.0;
        double arrival_min = 0.0;
        double required_max = 0.0;
        double required_min = 0.0;
        int pred_max_edge = -1;
        int pred_min_edge = -1;
    };

    struct EndpointCheck {
        std::string analysis_type;
        int node_id = -1;
        std::string launch_clock;
        std::string capture_clock;
        double arrival_time = 0.0;
        double required_time = 0.0;
        double slack = 0.0;
    };

    bool parse_liberty(const std::string& content);
    bool parse_netlist(const std::string& content);
    bool parse_constraints(const std::string& content);

    int ensure_node(const std::string& instance_name, const std::string& pin_name, bool is_port, bool is_input_port, bool is_output_port);
    int find_node(const std::string& instance_name, const std::string& pin_name) const;
    void add_edge(int from, int to, double max_delay, double min_delay, const std::string& name, const std::string& kind,
                  const std::string& instance_name, const std::string& from_pin, const std::string& to_pin);
    bool build_topological_order();

    void seed_startpoints();
    void forward_propagate_all();
    void backward_propagate_all();
    void collect_endpoint_checks();
    void build_reports();

    void reset_analysis_state();
    void initialize_endpoint_requirements_for_node(int node_id);
    bool clocks_are_false_path(const std::string& launch_clock, const std::string& capture_clock) const;

    std::vector<PathPoint> reconstruct_path(int endpoint_node, const std::string& launch_clock, bool use_max_path) const;
    std::string node_display_name(int node_id) const;
    bool read_file(const std::string& path, std::string* out);

    std::unordered_map<std::string, LibertyCell> liberty_cells_;
    Netlist netlist_;
    ConstraintSet constraints_;

    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
    std::vector<std::vector<int> > outgoing_edges_;
    std::vector<std::vector<int> > incoming_edges_;
    std::unordered_map<std::string, int> node_lookup_;
    std::vector<int> topo_order_;
    std::vector<std::unordered_map<std::string, DomainTiming> > timing_states_;

    std::unordered_map<std::string, std::string> net_driver_;
    std::unordered_map<std::string, std::vector<std::string> > net_loads_;
    std::vector<EndpointCheck> endpoint_checks_;
    std::vector<PathReport> setup_reports_;
    std::vector<PathReport> hold_reports_;
    AnalysisSummary summary_;
    std::string last_error_;
};

}  // namespace sta

#endif
