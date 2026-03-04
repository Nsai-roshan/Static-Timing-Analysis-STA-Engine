create_clock -name CLKA -period 4.00 [get_ports clk_a]
create_clock -name CLKB -period 6.00 [get_ports clk_b]
set_input_delay 0.50 -clock CLKA [get_ports din]
set_output_delay 0.60 -clock CLKB [get_ports dout]
