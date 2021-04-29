module dpram #(
    parameter addr_width_g = 10,
    parameter data_width_g = 8
) (
    // Port A
    input   wire                clock_a,
    input   wire                wren_a,
    input   wire    [addr_width_g-1:0]  address_a,
    input   wire    [data_width_g-1:0]  data_a,
    output  reg     [data_width_g-1:0]  q_a,
     
    // Port B
    input   wire                clock_b,
    input   wire                wren_b,
    input   wire    [addr_width_g-1:0]  address_b,
    input   wire    [data_width_g-1:0]  data_b,
    output  reg     [data_width_g-1:0]  q_b,

    input wire byteena_a,
    input wire byteena_b
);
 
// Shared memory
reg [data_width_g-1:0] mem [(2**addr_width_g)-1:0];
 
// Port A
always @(posedge clock_a) begin
    q_a      <= mem[address_a];
    if(wren_a) begin
        q_a      <= data_a;
        mem[address_a] <= data_a;
    end
end
 
// Port B
always @(posedge clock_b) begin
    q_b      <= mem[address_b];
    if(wren_b) begin
        q_b      <= data_b;
        mem[address_b] <= data_b;
    end
end
 
endmodule
