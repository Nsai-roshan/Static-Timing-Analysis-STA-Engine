module basic_top (
    input clk,
    input a,
    input b,
    output y
);
  wire q0;
  wire n1;

  DFFQ   u_ff0 ( .CLK(clk), .D(a), .Q(q0) );
  AND2X1 u_and ( .A(q0), .B(b), .Y(n1) );
  BUF    u_buf ( .A(n1), .Y(y) );
endmodule
