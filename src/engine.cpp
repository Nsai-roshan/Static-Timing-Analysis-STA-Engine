#include "sta/engine.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <queue>
#include <sstream>

namespace sta {
namespace {

const double kInf = std::numeric_limits<double>::infinity();

std::string trim(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])) != 0) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0) {
        --last;
    }

    return value.substr(first, last - first);
}

std::string strip_comment(const std::string& value, const std::string& marker) {
    const std::size_t pos = value.find(marker);
    if (pos == std::string::npos) {
        return value;
    }
    return value.substr(0, pos);
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool starts_group(const std::string& value, const std::string& keyword) {
    return starts_with(value, keyword + "(") || starts_with(value, keyword + " (");
}

std::string unquote(const std::string& value) {
    if (value.size() >= 2U &&
        ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1U, value.size() - 2U);
    }
    return value;
}

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> result;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        item = trim(item);
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

std::string normalize_space(const std::string& input) {
    std::string output;
    bool in_space = false;
    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
            if (!in_space) {
                output.push_back(' ');
                in_space = true;
            }
        } else {
            output.push_back(c);
            in_space = false;
        }
    }
    return trim(output);
}

std::string extract_between(const std::string& line, char open_ch, char close_ch) {
    const std::size_t begin = line.find(open_ch);
    if (begin == std::string::npos) {
        return "";
    }
    const std::size_t end = line.find(close_ch, begin + 1U);
    if (end == std::string::npos || end <= begin + 1U) {
        return "";
    }
    return trim(line.substr(begin + 1U, end - begin - 1U));
}

double parse_number(const std::string& token) {
    std::stringstream stream(token);
    double value = 0.0;
    stream >> value;
    return value;
}

std::string to_string_int(int value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

std::string to_string_double(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

double parse_scalar_from_values(const std::string& line) {
    const std::size_t quote_begin = line.find('"');
    const std::size_t quote_end = line.rfind('"');
    if (quote_begin == std::string::npos || quote_end == std::string::npos || quote_end <= quote_begin) {
        return 0.0;
    }

    std::string numbers = line.substr(quote_begin + 1U, quote_end - quote_begin - 1U);
    std::replace(numbers.begin(), numbers.end(), ',', ' ');

    std::stringstream stream(numbers);
    double best = 0.0;
    double parsed = 0.0;
    bool found = false;
    while (stream >> parsed) {
        if (!found || parsed > best) {
            best = parsed;
        }
        found = true;
    }
    return found ? best : 0.0;
}

PinDirection parse_direction(const std::string& value) {
    if (value == "input") {
        return PinDirection::Input;
    }
    if (value == "output") {
        return PinDirection::Output;
    }
    if (value == "inout") {
        return PinDirection::Inout;
    }
    return PinDirection::Unknown;
}

std::string extract_name_after_keyword(const std::string& line, const std::string& keyword) {
    const std::size_t pos = line.find(keyword);
    if (pos == std::string::npos) {
        return "";
    }
    const std::size_t open_pos = line.find('(', pos + keyword.size());
    const std::size_t close_pos = line.find(')', open_pos + 1U);
    if (open_pos == std::string::npos || close_pos == std::string::npos || close_pos <= open_pos + 1U) {
        return "";
    }
    return trim(line.substr(open_pos + 1U, close_pos - open_pos - 1U));
}

std::string make_node_key(const std::string& instance_name, const std::string& pin_name) {
    return instance_name + "|" + pin_name;
}

std::vector<std::string> tokenize_sdc(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_brace = false;
    bool in_bracket = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '{') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            in_brace = true;
            continue;
        }
        if (c == '}') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            in_brace = false;
            continue;
        }
        if (c == '[') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            in_bracket = true;
            continue;
        }
        if (c == ']') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            in_bracket = false;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(c)) != 0 && !in_brace && !in_bracket) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(c);
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

std::string extract_sdc_object_name(const std::string& token, const std::string& command) {
    if (token == command) {
        return "";
    }
    if (starts_with(token, command + " ")) {
        return trim(token.substr(command.size() + 1U));
    }
    return "";
}

std::vector<std::string> expand_decl_names(const std::string& statement) {
    std::string line = statement;
    if (!line.empty() && line.back() == ';') {
        line.pop_back();
    }

    std::string names_section = trim(line.substr(line.find(' ') + 1U));
    bool has_range = false;
    int msb = 0;
    int lsb = 0;

    if (!names_section.empty() && names_section[0] == '[') {
        const std::size_t close_bracket = names_section.find(']');
        if (close_bracket != std::string::npos) {
            const std::string range = names_section.substr(1U, close_bracket - 1U);
            const std::size_t colon = range.find(':');
            if (colon != std::string::npos) {
                msb = static_cast<int>(parse_number(range.substr(0U, colon)));
                lsb = static_cast<int>(parse_number(range.substr(colon + 1U)));
                has_range = true;
            }
            names_section = trim(names_section.substr(close_bracket + 1U));
        }
    }

    std::vector<std::string> names = split(names_section, ',');
    std::vector<std::string> expanded;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (!has_range) {
            expanded.push_back(names[i]);
            continue;
        }
        if (msb >= lsb) {
            for (int bit = msb; bit >= lsb; --bit) {
                expanded.push_back(names[i] + "[" + to_string_int(bit) + "]");
            }
        } else {
            for (int bit = msb; bit <= lsb; ++bit) {
                expanded.push_back(names[i] + "[" + to_string_int(bit) + "]");
            }
        }
    }

    return expanded;
}

}  // namespace

bool LibertyCell::is_sequential() const {
    return !clock_to_q_arcs.empty() || !constraint_arcs.empty();
}

bool StaEngine::read_file(const std::string& path, std::string* out) {
    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input) {
        last_error_ = "Unable to open file: " + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    *out = buffer.str();
    return true;
}

bool StaEngine::load_liberty(const std::string& path) {
    std::string content;
    return read_file(path, &content) && parse_liberty(content);
}

bool StaEngine::load_netlist(const std::string& path) {
    std::string content;
    return read_file(path, &content) && parse_netlist(content);
}

bool StaEngine::load_constraints(const std::string& path) {
    std::string content;
    return read_file(path, &content) && parse_constraints(content);
}

bool StaEngine::parse_liberty(const std::string& content) {
    liberty_cells_.clear();

    std::string normalized;
    normalized.reserve(content.size() * 2U);
    for (std::size_t i = 0; i < content.size(); ++i) {
        const char c = content[i];
        normalized.push_back(c);
        if (c == '{' || c == '}' || c == ';') {
            normalized.push_back('\n');
        }
    }

    std::stringstream stream(normalized);
    std::string raw_line;
    LibertyCell* current_cell = NULL;
    LibertyPin current_pin;
    bool in_pin = false;
    bool in_timing = false;
    int cell_depth = 0;
    int pin_depth = 0;
    int timing_depth = 0;

    struct TimingTemp {
        std::string current_pin_name;
        std::string related_pin;
        std::string timing_type;
        std::string timing_sense;
        double cell_rise = 0.0;
        double cell_fall = 0.0;
        double rise_constraint = 0.0;
        double fall_constraint = 0.0;
    } timing_temp;

    std::map<std::pair<std::string, std::string>, LibertyConstraintArc> pending_constraints;

    while (std::getline(stream, raw_line)) {
        std::string line = trim(strip_comment(strip_comment(raw_line, "//"), "#"));
        if (line.empty()) {
            continue;
        }

        const int open_braces = static_cast<int>(std::count(line.begin(), line.end(), '{'));
        const int close_braces = static_cast<int>(std::count(line.begin(), line.end(), '}'));

        if (starts_group(line, "cell")) {
            const std::string cell_name = extract_name_after_keyword(line, "cell");
            liberty_cells_[cell_name] = LibertyCell();
            current_cell = &liberty_cells_[cell_name];
            current_cell->name = cell_name;
            pending_constraints.clear();
            cell_depth = 0;
        } else if (current_cell != NULL && starts_group(line, "pin")) {
            current_pin = LibertyPin();
            current_pin.name = extract_name_after_keyword(line, "pin");
            in_pin = true;
            pin_depth = 0;
        } else if (in_pin && starts_group(line, "timing")) {
            in_timing = true;
            timing_depth = 0;
            timing_temp = TimingTemp();
            timing_temp.current_pin_name = current_pin.name;
        } else if (in_pin && starts_with(line, "direction")) {
            const std::size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string value = trim(line.substr(colon + 1U));
                value = trim(value.substr(0U, value.find(';')));
                current_pin.direction = parse_direction(value);
            }
        } else if (in_pin && starts_with(line, "clock")) {
            current_pin.is_clock = line.find("true") != std::string::npos;
        } else if (in_timing && starts_with(line, "related_pin")) {
            const std::size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string value = trim(line.substr(colon + 1U));
                value = trim(value.substr(0U, value.find(';')));
                timing_temp.related_pin = unquote(value);
            }
        } else if (in_timing && starts_with(line, "timing_type")) {
            const std::size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string value = trim(line.substr(colon + 1U));
                value = trim(value.substr(0U, value.find(';')));
                timing_temp.timing_type = value;
            }
        } else if (in_timing && starts_with(line, "timing_sense")) {
            const std::size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string value = trim(line.substr(colon + 1U));
                value = trim(value.substr(0U, value.find(';')));
                timing_temp.timing_sense = value;
            }
        } else if (in_timing && line.find("values") != std::string::npos) {
            const double parsed = parse_scalar_from_values(line);
            if (timing_temp.timing_type.find("setup") != std::string::npos ||
                timing_temp.timing_type.find("hold") != std::string::npos) {
                timing_temp.rise_constraint = parsed;
                timing_temp.fall_constraint = parsed;
            } else {
                if (timing_temp.cell_rise == 0.0) {
                    timing_temp.cell_rise = parsed;
                } else {
                    timing_temp.cell_fall = parsed;
                }
            }
        }

        if (in_timing) {
            timing_depth += open_braces - close_braces;
            if (timing_depth <= 0) {
                const double max_delay = std::max(timing_temp.cell_rise, timing_temp.cell_fall);
                const double min_delay = timing_temp.cell_fall > 0.0
                    ? std::min(timing_temp.cell_rise, timing_temp.cell_fall)
                    : timing_temp.cell_rise;

                if (timing_temp.timing_type == "combinational" || timing_temp.timing_type.empty()) {
                    if (!timing_temp.related_pin.empty()) {
                        LibertyCombArc arc;
                        arc.from_pin = timing_temp.related_pin;
                        arc.to_pin = timing_temp.current_pin_name;
                        arc.max_delay = max_delay;
                        arc.min_delay = min_delay;
                        arc.timing_sense = timing_temp.timing_sense;
                        current_cell->comb_arcs.push_back(arc);
                    }
                } else if (timing_temp.timing_type == "rising_edge" || timing_temp.timing_type == "falling_edge") {
                    LibertyClockToQArc arc;
                    arc.clock_pin = timing_temp.related_pin;
                    arc.output_pin = timing_temp.current_pin_name;
                    arc.max_delay = max_delay;
                    arc.min_delay = min_delay;
                    current_cell->clock_to_q_arcs.push_back(arc);
                } else if (timing_temp.timing_type.find("setup") != std::string::npos ||
                           timing_temp.timing_type.find("hold") != std::string::npos) {
                    const std::pair<std::string, std::string> key(timing_temp.current_pin_name, timing_temp.related_pin);
                    LibertyConstraintArc& arc = pending_constraints[key];
                    arc.data_pin = timing_temp.current_pin_name;
                    arc.clock_pin = timing_temp.related_pin;
                    if (timing_temp.timing_type.find("setup") != std::string::npos) {
                        arc.setup = std::max(timing_temp.rise_constraint, timing_temp.fall_constraint);
                    } else {
                        arc.hold = std::max(timing_temp.rise_constraint, timing_temp.fall_constraint);
                    }
                }

                in_timing = false;
                timing_depth = 0;
            }
        } else if (in_pin) {
            pin_depth += open_braces - close_braces;
            if (pin_depth <= 0) {
                current_cell->pins[current_pin.name] = current_pin;
                in_pin = false;
                pin_depth = 0;
            }
        } else if (current_cell != NULL) {
            cell_depth += open_braces - close_braces;
            if (cell_depth <= 0) {
                for (std::map<std::pair<std::string, std::string>, LibertyConstraintArc>::const_iterator it = pending_constraints.begin();
                     it != pending_constraints.end(); ++it) {
                    current_cell->constraint_arcs.push_back(it->second);
                }
                current_cell = NULL;
                cell_depth = 0;
            }
        }
    }

    if (liberty_cells_.empty()) {
        last_error_ = "No Liberty cells were parsed.";
        return false;
    }
    return true;
}

bool StaEngine::parse_netlist(const std::string& content) {
    netlist_ = Netlist();

    std::stringstream stream(content);
    std::string raw_line;
    std::string statement;

    while (std::getline(stream, raw_line)) {
        std::string line = trim(strip_comment(strip_comment(raw_line, "//"), "#"));
        if (line.empty()) {
            continue;
        }

        statement += " " + line;
        if (line.find(';') == std::string::npos && line.find("endmodule") == std::string::npos) {
            continue;
        }

        statement = normalize_space(statement);

        if (starts_with(statement, "module ")) {
            const std::size_t space = statement.find(' ');
            const std::size_t paren = statement.find('(', space + 1U);
            const std::size_t close_paren = statement.rfind(')');
            if (space != std::string::npos && paren != std::string::npos) {
                netlist_.module_name = trim(statement.substr(space + 1U, paren - space - 1U));
            }
            if (paren != std::string::npos && close_paren != std::string::npos && close_paren > paren + 1U) {
                const std::vector<std::string> ports = split(statement.substr(paren + 1U, close_paren - paren - 1U), ',');
                for (std::size_t port_index = 0; port_index < ports.size(); ++port_index) {
                    const std::string port_decl = trim(ports[port_index]);
                    if (starts_with(port_decl, "input ")) {
                        const std::vector<std::string> names = expand_decl_names(port_decl + ";");
                        for (std::size_t i = 0; i < names.size(); ++i) {
                            netlist_.inputs.insert(names[i]);
                        }
                    } else if (starts_with(port_decl, "output ")) {
                        const std::vector<std::string> names = expand_decl_names(port_decl + ";");
                        for (std::size_t i = 0; i < names.size(); ++i) {
                            netlist_.outputs.insert(names[i]);
                        }
                    }
                }
            }
        } else if (starts_with(statement, "input ")) {
            const std::vector<std::string> names = expand_decl_names(statement);
            for (std::size_t i = 0; i < names.size(); ++i) {
                netlist_.inputs.insert(names[i]);
            }
        } else if (starts_with(statement, "output ")) {
            const std::vector<std::string> names = expand_decl_names(statement);
            for (std::size_t i = 0; i < names.size(); ++i) {
                netlist_.outputs.insert(names[i]);
            }
        } else if (starts_with(statement, "wire ")) {
            const std::vector<std::string> names = expand_decl_names(statement);
            for (std::size_t i = 0; i < names.size(); ++i) {
                netlist_.wires.insert(names[i]);
            }
        } else if (starts_with(statement, "assign ")) {
            std::string body = trim(statement.substr(7U));
            if (!body.empty() && body.back() == ';') {
                body.pop_back();
            }
            const std::size_t equal_pos = body.find('=');
            if (equal_pos != std::string::npos) {
                netlist_.assigns.push_back(std::make_pair(trim(body.substr(0U, equal_pos)),
                                                          trim(body.substr(equal_pos + 1U))));
            }
        } else if (statement.find('(') != std::string::npos && statement.find('.') != std::string::npos) {
            std::string body = statement;
            if (!body.empty() && body.back() == ';') {
                body.pop_back();
            }

            const std::size_t first_space = body.find(' ');
            const std::size_t paren = body.find('(', first_space + 1U);
            if (first_space != std::string::npos && paren != std::string::npos) {
                Instance inst;
                inst.type = trim(body.substr(0U, first_space));
                inst.name = trim(body.substr(first_space + 1U, paren - first_space - 1U));

                const std::size_t close_paren = body.rfind(')');
                const std::string pin_blob = body.substr(paren + 1U, close_paren - paren - 1U);
                const std::vector<std::string> pin_tokens = split(pin_blob, ',');
                for (std::size_t i = 0; i < pin_tokens.size(); ++i) {
                    const std::string token = trim(pin_tokens[i]);
                    if (token.empty() || token[0] != '.') {
                        continue;
                    }
                    const std::size_t pin_open = token.find('(');
                    const std::size_t pin_close = token.rfind(')');
                    if (pin_open == std::string::npos || pin_close == std::string::npos || pin_close <= pin_open + 1U) {
                        continue;
                    }
                    const std::string pin_name = trim(token.substr(1U, pin_open - 1U));
                    const std::string net_name = trim(token.substr(pin_open + 1U, pin_close - pin_open - 1U));
                    inst.pin_to_net[pin_name] = net_name;
                }

                netlist_.instances.push_back(inst);
            }
        }

        statement.clear();
    }

    if (netlist_.module_name.empty()) {
        last_error_ = "No Verilog module definition was parsed.";
        return false;
    }
    return true;
}

bool StaEngine::parse_constraints(const std::string& content) {
    constraints_ = ConstraintSet();

    std::stringstream stream(content);
    std::string raw_line;
    while (std::getline(stream, raw_line)) {
        std::string line = trim(strip_comment(strip_comment(raw_line, "#"), "//"));
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = tokenize_sdc(line);
        if (tokens.empty()) {
            continue;
        }

        if (tokens[0] == "create_clock") {
            Clock clock;
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                if (tokens[i] == "-name" && i + 1U < tokens.size()) {
                    clock.name = tokens[++i];
                } else if (tokens[i] == "-period" && i + 1U < tokens.size()) {
                    clock.period = parse_number(tokens[++i]);
                } else if (tokens[i] == "get_ports" && i + 1U < tokens.size()) {
                    clock.port_name = tokens[++i];
                } else {
                    const std::string port_name = extract_sdc_object_name(tokens[i], "get_ports");
                    if (!port_name.empty()) {
                        clock.port_name = port_name;
                    }
                }
            }

            if (clock.name.empty()) {
                clock.name = clock.port_name;
            }
            constraints_.clocks_by_name[clock.name] = clock;
            constraints_.port_to_clock[clock.port_name] = clock.name;
        } else if (tokens[0] == "set_clock_uncertainty" && tokens.size() >= 4U) {
            const double uncertainty = parse_number(tokens[1]);
            std::string clock_name;
            for (std::size_t i = 2; i < tokens.size(); ++i) {
                if (tokens[i] == "get_clocks" && i + 1U < tokens.size()) {
                    clock_name = tokens[++i];
                } else {
                    const std::string parsed_name = extract_sdc_object_name(tokens[i], "get_clocks");
                    if (!parsed_name.empty()) {
                        clock_name = parsed_name;
                    }
                }
            }
            if (!clock_name.empty() && constraints_.clocks_by_name.count(clock_name) > 0U) {
                constraints_.clocks_by_name[clock_name].uncertainty = uncertainty;
            }
        } else if (tokens[0] == "set_input_delay" || tokens[0] == "set_output_delay") {
            PortDelay delay;
            const bool is_input = tokens[0] == "set_input_delay";
            bool explicit_min = false;
            bool explicit_max = false;

            if (tokens.size() >= 2U) {
                const double value = parse_number(tokens[1]);
                delay.has_max = true;
                delay.has_min = true;
                delay.max_delay = value;
                delay.min_delay = value;
            }

            for (std::size_t i = 2; i < tokens.size(); ++i) {
                if (tokens[i] == "-clock" && i + 1U < tokens.size()) {
                    delay.clock_name = tokens[++i];
                } else if (tokens[i] == "-min") {
                    explicit_min = true;
                } else if (tokens[i] == "-max") {
                    explicit_max = true;
                } else if (tokens[i] == "get_ports" && i + 1U < tokens.size()) {
                    delay.port_name = tokens[++i];
                } else {
                    const std::string port_name = extract_sdc_object_name(tokens[i], "get_ports");
                    if (!port_name.empty()) {
                        delay.port_name = port_name;
                    }
                }
            }

            if (explicit_min && !explicit_max) {
                delay.has_max = false;
            } else if (explicit_max && !explicit_min) {
                delay.has_min = false;
            }

            if (is_input) {
                constraints_.input_delays[delay.port_name] = delay;
            } else {
                constraints_.output_delays[delay.port_name] = delay;
            }
        } else if (tokens[0] == "set_clock_groups") {
            std::vector<std::vector<std::string> > groups;
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                if (tokens[i] == "-group" && i + 1U < tokens.size()) {
                    groups.push_back(split(tokens[++i], ' '));
                }
            }

            for (std::size_t left = 0; left < groups.size(); ++left) {
                for (std::size_t right = left + 1U; right < groups.size(); ++right) {
                    for (std::size_t a = 0; a < groups[left].size(); ++a) {
                        for (std::size_t b = 0; b < groups[right].size(); ++b) {
                            constraints_.false_clock_pairs.insert(std::make_pair(groups[left][a], groups[right][b]));
                            constraints_.false_clock_pairs.insert(std::make_pair(groups[right][b], groups[left][a]));
                        }
                    }
                }
            }
        }
    }

    return true;
}

int StaEngine::ensure_node(const std::string& instance_name, const std::string& pin_name, bool is_port,
                           bool is_input_port, bool is_output_port) {
    const std::string key = make_node_key(instance_name, pin_name);
    std::unordered_map<std::string, int>::const_iterator it = node_lookup_.find(key);
    if (it != node_lookup_.end()) {
        return it->second;
    }

    Node node;
    node.id = static_cast<int>(nodes_.size());
    node.instance_name = instance_name;
    node.pin_name = pin_name;
    node.is_port = is_port;
    node.is_input_port = is_input_port;
    node.is_output_port = is_output_port;
    node.name = instance_name == "__port__" ? pin_name : instance_name + "/" + pin_name;

    nodes_.push_back(node);
    outgoing_edges_.push_back(std::vector<int>());
    incoming_edges_.push_back(std::vector<int>());
    timing_states_.push_back(std::unordered_map<std::string, DomainTiming>());
    node_lookup_[key] = node.id;
    return node.id;
}

int StaEngine::find_node(const std::string& instance_name, const std::string& pin_name) const {
    const std::string key = make_node_key(instance_name, pin_name);
    std::unordered_map<std::string, int>::const_iterator it = node_lookup_.find(key);
    return it == node_lookup_.end() ? -1 : it->second;
}

void StaEngine::add_edge(int from, int to, double max_delay, double min_delay, const std::string& name,
                         const std::string& kind, const std::string& instance_name,
                         const std::string& from_pin, const std::string& to_pin) {
    Edge edge;
    edge.id = static_cast<int>(edges_.size());
    edge.from = from;
    edge.to = to;
    edge.max_delay = max_delay;
    edge.min_delay = min_delay;
    edge.name = name;
    edge.kind = kind;
    edge.instance_name = instance_name;
    edge.from_pin = from_pin;
    edge.to_pin = to_pin;
    edges_.push_back(edge);
    outgoing_edges_[from].push_back(edge.id);
    incoming_edges_[to].push_back(edge.id);
}

bool StaEngine::build_topological_order() {
    topo_order_.clear();
    std::vector<int> indegree(nodes_.size(), 0);
    for (std::size_t i = 0; i < edges_.size(); ++i) {
        ++indegree[edges_[i].to];
    }

    std::queue<int> ready;
    for (std::size_t i = 0; i < indegree.size(); ++i) {
        if (indegree[i] == 0) {
            ready.push(static_cast<int>(i));
        }
    }

    while (!ready.empty()) {
        const int node = ready.front();
        ready.pop();
        topo_order_.push_back(node);
        for (std::size_t i = 0; i < outgoing_edges_[node].size(); ++i) {
            const Edge& edge = edges_[outgoing_edges_[node][i]];
            --indegree[edge.to];
            if (indegree[edge.to] == 0) {
                ready.push(edge.to);
            }
        }
    }

    if (topo_order_.size() != nodes_.size()) {
        last_error_ = "Timing graph contains a cycle.";
        return false;
    }
    return true;
}

bool StaEngine::build_timing_graph() {
    nodes_.clear();
    edges_.clear();
    outgoing_edges_.clear();
    incoming_edges_.clear();
    node_lookup_.clear();
    timing_states_.clear();
    topo_order_.clear();
    net_driver_.clear();
    net_loads_.clear();

    for (std::set<std::string>::const_iterator it = netlist_.inputs.begin(); it != netlist_.inputs.end(); ++it) {
        ensure_node("__port__", *it, true, true, false);
    }
    for (std::set<std::string>::const_iterator it = netlist_.outputs.begin(); it != netlist_.outputs.end(); ++it) {
        ensure_node("__port__", *it, true, false, true);
    }

    for (std::size_t i = 0; i < netlist_.instances.size(); ++i) {
        const Instance& inst = netlist_.instances[i];
        std::unordered_map<std::string, LibertyCell>::const_iterator lib_it = liberty_cells_.find(inst.type);
        if (lib_it == liberty_cells_.end()) {
            last_error_ = "Missing Liberty cell for instance type '" + inst.type + "'.";
            return false;
        }
        const LibertyCell& cell = lib_it->second;

        for (std::unordered_map<std::string, std::string>::const_iterator pin_it = inst.pin_to_net.begin();
             pin_it != inst.pin_to_net.end(); ++pin_it) {
            ensure_node(inst.name, pin_it->first, false, false, false);

            std::unordered_map<std::string, LibertyPin>::const_iterator cell_pin_it = cell.pins.find(pin_it->first);
            if (cell_pin_it == cell.pins.end()) {
                continue;
            }

            if (cell_pin_it->second.direction == PinDirection::Output) {
                net_driver_[pin_it->second] = make_node_key(inst.name, pin_it->first);
            } else {
                net_loads_[pin_it->second].push_back(make_node_key(inst.name, pin_it->first));
            }
        }

        for (std::size_t arc_index = 0; arc_index < cell.comb_arcs.size(); ++arc_index) {
            const LibertyCombArc& arc = cell.comb_arcs[arc_index];
            const int from = find_node(inst.name, arc.from_pin);
            const int to = find_node(inst.name, arc.to_pin);
            if (from >= 0 && to >= 0) {
                add_edge(from, to, arc.max_delay, arc.min_delay,
                         inst.name + ":" + arc.from_pin + "->" + arc.to_pin,
                         "cell_arc", inst.name, arc.from_pin, arc.to_pin);
            }
        }
    }

    for (std::set<std::string>::const_iterator it = netlist_.inputs.begin(); it != netlist_.inputs.end(); ++it) {
        net_driver_[*it] = make_node_key("__port__", *it);
    }
    for (std::set<std::string>::const_iterator it = netlist_.outputs.begin(); it != netlist_.outputs.end(); ++it) {
        net_loads_[*it].push_back(make_node_key("__port__", *it));
    }

    for (std::size_t i = 0; i < netlist_.assigns.size(); ++i) {
        const std::pair<std::string, std::string>& assignment = netlist_.assigns[i];
        if (net_driver_.count(assignment.second) > 0U) {
            net_driver_[assignment.first] = net_driver_[assignment.second];
        }
        if (net_loads_.count(assignment.first) > 0U) {
            std::vector<std::string>& loads = net_loads_[assignment.first];
            net_loads_[assignment.second].insert(net_loads_[assignment.second].end(), loads.begin(), loads.end());
        }
    }

    for (std::unordered_map<std::string, std::vector<std::string> >::const_iterator load_it = net_loads_.begin();
         load_it != net_loads_.end(); ++load_it) {
        std::unordered_map<std::string, std::string>::const_iterator driver_it = net_driver_.find(load_it->first);
        if (driver_it == net_driver_.end()) {
            continue;
        }

        const std::string driver_key = driver_it->second;
        const std::size_t driver_sep = driver_key.find('|');
        const int driver_node = find_node(driver_key.substr(0U, driver_sep), driver_key.substr(driver_sep + 1U));
        if (driver_node < 0) {
            continue;
        }

        for (std::size_t i = 0; i < load_it->second.size(); ++i) {
            const std::string load_key = load_it->second[i];
            const std::size_t load_sep = load_key.find('|');
            const int load_node = find_node(load_key.substr(0U, load_sep), load_key.substr(load_sep + 1U));
            if (load_node >= 0 && load_node != driver_node) {
                add_edge(driver_node, load_node, 0.0, 0.0, "net:" + load_it->first, "net", "", "", "");
            }
        }
    }

    return build_topological_order();
}

void StaEngine::reset_analysis_state() {
    timing_states_.assign(nodes_.size(), std::unordered_map<std::string, DomainTiming>());
    endpoint_checks_.clear();
    setup_reports_.clear();
    hold_reports_.clear();
    summary_ = AnalysisSummary();
    summary_.worst_setup_slack = kInf;
    summary_.worst_hold_slack = kInf;
}

bool StaEngine::analyze() {
    if (nodes_.empty() && !build_timing_graph()) {
        return false;
    }

    reset_analysis_state();
    seed_startpoints();
    forward_propagate_all();
    backward_propagate_all();
    collect_endpoint_checks();
    build_reports();
    return true;
}

void StaEngine::seed_startpoints() {
    for (std::set<std::string>::const_iterator it = netlist_.inputs.begin(); it != netlist_.inputs.end(); ++it) {
        const int node = find_node("__port__", *it);
        std::unordered_map<std::string, PortDelay>::const_iterator delay_it = constraints_.input_delays.find(*it);
        if (node < 0 || delay_it == constraints_.input_delays.end()) {
            continue;
        }

        DomainTiming& timing = timing_states_[node][delay_it->second.clock_name];
        timing.max_valid = delay_it->second.has_max;
        timing.min_valid = delay_it->second.has_min;
        timing.arrival_max = delay_it->second.max_delay;
        timing.arrival_min = delay_it->second.min_delay;
    }

    for (std::size_t i = 0; i < netlist_.instances.size(); ++i) {
        const Instance& inst = netlist_.instances[i];
        const LibertyCell& cell = liberty_cells_[inst.type];
        if (!cell.is_sequential()) {
            continue;
        }

        for (std::size_t arc_index = 0; arc_index < cell.clock_to_q_arcs.size(); ++arc_index) {
            const LibertyClockToQArc& arc = cell.clock_to_q_arcs[arc_index];
            std::unordered_map<std::string, std::string>::const_iterator clock_net_it = inst.pin_to_net.find(arc.clock_pin);
            if (clock_net_it == inst.pin_to_net.end()) {
                continue;
            }
            std::unordered_map<std::string, std::string>::const_iterator clock_name_it = constraints_.port_to_clock.find(clock_net_it->second);
            if (clock_name_it == constraints_.port_to_clock.end()) {
                continue;
            }

            const int q_node = find_node(inst.name, arc.output_pin);
            if (q_node < 0) {
                continue;
            }

            DomainTiming& timing = timing_states_[q_node][clock_name_it->second];
            timing.max_valid = true;
            timing.min_valid = true;
            timing.arrival_max = arc.max_delay;
            timing.arrival_min = arc.min_delay;
        }
    }
}

void StaEngine::forward_propagate_all() {
    for (std::size_t order_index = 0; order_index < topo_order_.size(); ++order_index) {
        const int node = topo_order_[order_index];
        const std::unordered_map<std::string, DomainTiming> current_states = timing_states_[node];
        for (std::unordered_map<std::string, DomainTiming>::const_iterator state_it = current_states.begin();
             state_it != current_states.end(); ++state_it) {
            const std::string& domain = state_it->first;
            const DomainTiming& current = state_it->second;

            for (std::size_t edge_index = 0; edge_index < outgoing_edges_[node].size(); ++edge_index) {
                const Edge& edge = edges_[outgoing_edges_[node][edge_index]];
                DomainTiming& next = timing_states_[edge.to][domain];

                if (current.max_valid) {
                    const double candidate = current.arrival_max + edge.max_delay;
                    if (!next.max_valid || candidate > next.arrival_max) {
                        next.max_valid = true;
                        next.arrival_max = candidate;
                        next.pred_max_edge = edge.id;
                    }
                }

                if (current.min_valid) {
                    const double candidate = current.arrival_min + edge.min_delay;
                    if (!next.min_valid || candidate < next.arrival_min) {
                        next.min_valid = true;
                        next.arrival_min = candidate;
                        next.pred_min_edge = edge.id;
                    }
                }
            }
        }
    }
}

bool StaEngine::clocks_are_false_path(const std::string& launch_clock, const std::string& capture_clock) const {
    return constraints_.false_clock_pairs.count(std::make_pair(launch_clock, capture_clock)) > 0U;
}

void StaEngine::initialize_endpoint_requirements_for_node(int node_id) {
    const Node& node = nodes_[node_id];

    if (node.is_output_port) {
        std::unordered_map<std::string, PortDelay>::const_iterator delay_it = constraints_.output_delays.find(node.pin_name);
        if (delay_it != constraints_.output_delays.end()) {
            for (std::unordered_map<std::string, DomainTiming>::iterator it = timing_states_[node_id].begin();
                 it != timing_states_[node_id].end(); ++it) {
                DomainTiming& timing = it->second;
                if (constraints_.clocks_by_name.count(delay_it->second.clock_name) > 0U) {
                    const Clock& clock = constraints_.clocks_by_name.find(delay_it->second.clock_name)->second;
                    if (delay_it->second.has_max) {
                        timing.req_max_valid = true;
                        timing.required_max = clock.period - delay_it->second.max_delay - clock.uncertainty;
                    }
                    if (delay_it->second.has_min) {
                        timing.req_min_valid = true;
                        timing.required_min = delay_it->second.min_delay;
                    }
                }
            }
        }
    }

    if (node.is_port || node.instance_name == "__port__") {
        return;
    }

    const Instance* instance = NULL;
    for (std::size_t i = 0; i < netlist_.instances.size(); ++i) {
        if (netlist_.instances[i].name == node.instance_name) {
            instance = &netlist_.instances[i];
            break;
        }
    }
    if (instance == NULL) {
        return;
    }

    const LibertyCell& cell = liberty_cells_[instance->type];
    for (std::size_t i = 0; i < cell.constraint_arcs.size(); ++i) {
        const LibertyConstraintArc& arc = cell.constraint_arcs[i];
        if (arc.data_pin != node.pin_name) {
            continue;
        }

        std::unordered_map<std::string, std::string>::const_iterator clock_net_it = instance->pin_to_net.find(arc.clock_pin);
        if (clock_net_it == instance->pin_to_net.end()) {
            continue;
        }
        std::unordered_map<std::string, std::string>::const_iterator capture_name_it = constraints_.port_to_clock.find(clock_net_it->second);
        if (capture_name_it == constraints_.port_to_clock.end()) {
            continue;
        }
        std::unordered_map<std::string, Clock>::const_iterator clock_it = constraints_.clocks_by_name.find(capture_name_it->second);
        if (clock_it == constraints_.clocks_by_name.end()) {
            continue;
        }

        const std::string capture_clock = capture_name_it->second;
        const Clock& clock = clock_it->second;
        for (std::unordered_map<std::string, DomainTiming>::iterator it = timing_states_[node_id].begin();
             it != timing_states_[node_id].end(); ++it) {
            if (clocks_are_false_path(it->first, capture_clock)) {
                continue;
            }
            DomainTiming& timing = it->second;
            timing.req_max_valid = true;
            timing.req_min_valid = true;
            timing.required_max = clock.period - arc.setup - clock.uncertainty;
            timing.required_min = arc.hold + clock.uncertainty;
        }
    }
}

void StaEngine::backward_propagate_all() {
    for (std::size_t node = 0; node < nodes_.size(); ++node) {
        initialize_endpoint_requirements_for_node(static_cast<int>(node));
    }

    for (std::size_t order_index = topo_order_.size(); order_index > 0; --order_index) {
        const int node = topo_order_[order_index - 1U];
        const std::unordered_map<std::string, DomainTiming> current_states = timing_states_[node];
        for (std::unordered_map<std::string, DomainTiming>::const_iterator state_it = current_states.begin();
             state_it != current_states.end(); ++state_it) {
            const std::string& domain = state_it->first;
            const DomainTiming& current = state_it->second;

            for (std::size_t edge_index = 0; edge_index < incoming_edges_[node].size(); ++edge_index) {
                const Edge& edge = edges_[incoming_edges_[node][edge_index]];
                DomainTiming& previous = timing_states_[edge.from][domain];

                if (current.req_max_valid) {
                    const double candidate = current.required_max - edge.max_delay;
                    if (!previous.req_max_valid || candidate < previous.required_max) {
                        previous.req_max_valid = true;
                        previous.required_max = candidate;
                    }
                }

                if (current.req_min_valid) {
                    const double candidate = current.required_min - edge.min_delay;
                    if (!previous.req_min_valid || candidate > previous.required_min) {
                        previous.req_min_valid = true;
                        previous.required_min = candidate;
                    }
                }
            }
        }
    }
}

void StaEngine::collect_endpoint_checks() {
    endpoint_checks_.clear();
    summary_.worst_setup_slack = kInf;
    summary_.worst_hold_slack = kInf;

    for (std::size_t node_index = 0; node_index < nodes_.size(); ++node_index) {
        const Node& node = nodes_[node_index];
        const bool output_endpoint = node.is_output_port && constraints_.output_delays.count(node.pin_name) > 0U;

        bool register_endpoint = false;
        std::string capture_clock;
        if (!node.is_port && node.instance_name != "__port__") {
            for (std::size_t i = 0; i < netlist_.instances.size(); ++i) {
                if (netlist_.instances[i].name != node.instance_name) {
                    continue;
                }
                const LibertyCell& cell = liberty_cells_[netlist_.instances[i].type];
                for (std::size_t arc_index = 0; arc_index < cell.constraint_arcs.size(); ++arc_index) {
                    const LibertyConstraintArc& arc = cell.constraint_arcs[arc_index];
                    if (arc.data_pin == node.pin_name) {
                        register_endpoint = true;
                        std::unordered_map<std::string, std::string>::const_iterator net_it =
                            netlist_.instances[i].pin_to_net.find(arc.clock_pin);
                        if (net_it != netlist_.instances[i].pin_to_net.end() &&
                            constraints_.port_to_clock.count(net_it->second) > 0U) {
                            capture_clock = constraints_.port_to_clock.find(net_it->second)->second;
                        }
                        break;
                    }
                }
                break;
            }
        }

        if (!output_endpoint && !register_endpoint) {
            continue;
        }

        for (std::unordered_map<std::string, DomainTiming>::const_iterator it = timing_states_[node_index].begin();
             it != timing_states_[node_index].end(); ++it) {
            const std::string capture = output_endpoint
                ? constraints_.output_delays.find(node.pin_name)->second.clock_name
                : capture_clock;

            if (it->second.max_valid && it->second.req_max_valid) {
                EndpointCheck check;
                check.analysis_type = "setup";
                check.node_id = static_cast<int>(node_index);
                check.launch_clock = it->first;
                check.capture_clock = capture;
                check.arrival_time = it->second.arrival_max;
                check.required_time = it->second.required_max;
                check.slack = it->second.required_max - it->second.arrival_max;
                endpoint_checks_.push_back(check);
                summary_.worst_setup_slack = std::min(summary_.worst_setup_slack, check.slack);
                if (check.slack < 0.0) {
                    ++summary_.setup_violations;
                    summary_.total_negative_setup_slack += check.slack;
                }
            }

            if (it->second.min_valid && it->second.req_min_valid) {
                EndpointCheck check;
                check.analysis_type = "hold";
                check.node_id = static_cast<int>(node_index);
                check.launch_clock = it->first;
                check.capture_clock = capture;
                check.arrival_time = it->second.arrival_min;
                check.required_time = it->second.required_min;
                check.slack = it->second.arrival_min - it->second.required_min;
                endpoint_checks_.push_back(check);
                summary_.worst_hold_slack = std::min(summary_.worst_hold_slack, check.slack);
                if (check.slack < 0.0) {
                    ++summary_.hold_violations;
                    summary_.total_negative_hold_slack += check.slack;
                }
            }
        }
    }

    if (summary_.worst_setup_slack == kInf) {
        summary_.worst_setup_slack = 0.0;
    }
    if (summary_.worst_hold_slack == kInf) {
        summary_.worst_hold_slack = 0.0;
    }
    summary_.path_count = endpoint_checks_.size();
}

std::vector<PathPoint> StaEngine::reconstruct_path(int endpoint_node, const std::string& launch_clock, bool use_max_path) const {
    std::vector<PathPoint> reversed;
    int node = endpoint_node;

    while (node >= 0) {
        std::unordered_map<std::string, DomainTiming>::const_iterator state_it = timing_states_[node].find(launch_clock);
        if (state_it == timing_states_[node].end()) {
            break;
        }

        PathPoint point;
        point.node_name = node_display_name(node);
        point.arrival_time = use_max_path ? state_it->second.arrival_max : state_it->second.arrival_min;
        reversed.push_back(point);

        const int pred_edge = use_max_path ? state_it->second.pred_max_edge : state_it->second.pred_min_edge;
        if (pred_edge < 0) {
            break;
        }

        reversed.back().edge_name = edges_[pred_edge].name;
        node = edges_[pred_edge].from;
    }

    std::reverse(reversed.begin(), reversed.end());
    return reversed;
}

std::string StaEngine::node_display_name(int node_id) const {
    if (node_id < 0 || static_cast<std::size_t>(node_id) >= nodes_.size()) {
        return "";
    }
    return nodes_[node_id].name;
}

void StaEngine::build_reports() {
    setup_reports_.clear();
    hold_reports_.clear();

    for (std::size_t i = 0; i < endpoint_checks_.size(); ++i) {
        const EndpointCheck& check = endpoint_checks_[i];
        PathReport report;
        report.analysis_type = check.analysis_type;
        report.endpoint = node_display_name(check.node_id);
        report.launch_clock = check.launch_clock;
        report.capture_clock = check.capture_clock;
        report.arrival_time = check.arrival_time;
        report.required_time = check.required_time;
        report.slack = check.slack;
        report.points = reconstruct_path(check.node_id, check.launch_clock, check.analysis_type == "setup");
        if (!report.points.empty()) {
            report.startpoint = report.points.front().node_name;
        }

        if (check.analysis_type == "setup") {
            setup_reports_.push_back(report);
        } else {
            hold_reports_.push_back(report);
        }
    }

    std::sort(setup_reports_.begin(), setup_reports_.end(),
              [](const PathReport& left, const PathReport& right) {
                  return left.slack < right.slack;
              });
    std::sort(hold_reports_.begin(), hold_reports_.end(),
              [](const PathReport& left, const PathReport& right) {
                  return left.slack < right.slack;
              });
}

std::string StaEngine::last_error() const {
    return last_error_;
}

const AnalysisSummary& StaEngine::summary() const {
    return summary_;
}

const std::vector<PathReport>& StaEngine::setup_reports() const {
    return setup_reports_;
}

const std::vector<PathReport>& StaEngine::hold_reports() const {
    return hold_reports_;
}

std::string StaEngine::build_summary_report() const {
    std::ostringstream report;
    report << "============================================================\n";
    report << "STA Prototype Summary\n";
    report << "============================================================\n";
    report << "Design              : " << netlist_.module_name << "\n";
    report << "Instance Count      : " << netlist_.instances.size() << "\n";
    report << "Clock Count         : " << constraints_.clocks_by_name.size() << "\n";
    report << "Checked Paths       : " << summary_.path_count << "\n";
    report << "WNS (setup)         : " << to_string_double(summary_.worst_setup_slack) << "\n";
    report << "WNS (hold)          : " << to_string_double(summary_.worst_hold_slack) << "\n";
    report << "TNS (setup)         : " << to_string_double(summary_.total_negative_setup_slack) << "\n";
    report << "TNS (hold)          : " << to_string_double(summary_.total_negative_hold_slack) << "\n";
    report << "Setup Violations    : " << summary_.setup_violations << "\n";
    report << "Hold Violations     : " << summary_.hold_violations << "\n";
    return report.str();
}

std::string StaEngine::build_path_report(std::size_t max_paths_per_type) const {
    std::ostringstream report;
    const std::vector<PathReport>* groups[2] = { &setup_reports_, &hold_reports_ };

    for (int group_index = 0; group_index < 2; ++group_index) {
        const std::vector<PathReport>& paths = *groups[group_index];
        const std::size_t limit = std::min(max_paths_per_type, paths.size());
        for (std::size_t i = 0; i < limit; ++i) {
            const PathReport& path = paths[i];
            report << "------------------------------------------------------------\n";
            report << "Path Type     : " << path.analysis_type << "\n";
            report << "Startpoint    : " << path.startpoint << "\n";
            report << "Endpoint      : " << path.endpoint << "\n";
            report << "Launch Clock  : " << path.launch_clock << "\n";
            report << "Capture Clock : " << path.capture_clock << "\n";
            report << "Arrival       : " << to_string_double(path.arrival_time) << "\n";
            report << "Required      : " << to_string_double(path.required_time) << "\n";
            report << "Slack         : " << to_string_double(path.slack) << "\n";
            report << "Points:\n";
            for (std::size_t point_index = 0; point_index < path.points.size(); ++point_index) {
                const PathPoint& point = path.points[point_index];
                report << "  " << std::setw(2) << point_index + 1U
                       << "  " << std::left << std::setw(24) << point.node_name
                       << " arrival=" << std::setw(8) << to_string_double(point.arrival_time);
                if (!point.edge_name.empty()) {
                    report << " via " << point.edge_name;
                }
                report << "\n";
            }
        }
    }

    return report.str();
}

}  // namespace sta
