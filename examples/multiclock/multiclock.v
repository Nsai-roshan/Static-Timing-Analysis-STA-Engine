module multiclock_top (
    input clk_a,
    input clk_b,
    input din,
    output dout
);
  wire qa;
  wire n1;

  DFFQ u_src ( .CLK(clk_a), .D(din), .Q(qa) );
  BUF  u_buf ( .A(qa), .Y(n1) );
  DFFQ u_dst ( .CLK(clk_b), .D(n1), .Q(dout) );
endmodule
