`timescale 1ns/1ns
//
// top end ff for verilator
//

`define USE_VGA

module top(

   input clk_12 /*verilator public_flat*/,
   input reset/*verilator public_flat*/,

   output [7:0] VGA_R/*verilator public_flat*/,
   output [7:0] VGA_G/*verilator public_flat*/,
   output [7:0] VGA_B/*verilator public_flat*/,
   
   output VGA_HS,
   output VGA_VS,
   output VGA_HB,
   output VGA_VB,
   
   input        ioctl_download,
   input        ioctl_wr,
   input [24:0] ioctl_addr,
   input [7:0] ioctl_dout,
   input [7:0]  ioctl_index,
   output  reg     ioctl_wait=1'b0
);

   wire [2:0]  vgaBlue;
   wire [2:0]  vgaGreen;
   wire [2:0]  vgaRed;

   assign VGA_R = {vgaRed,vgaRed,vgaRed[2:1]};
   assign VGA_G = {vgaGreen,vgaGreen,vgaGreen[2:1]};
   assign VGA_B = {vgaBlue,vgaBlue,vgaBlue[2:1]};
   assign vgaBlue  = rgb[8:6];
   assign vgaGreen = rgb[5:3];
   assign vgaRed   = rgb[2:0];

   assign VGA_VS=vsync; 
   assign VGA_HS=hsync; 
   assign VGA_VB=vblank; 
   assign VGA_HB=hblank; 

   wire [8:0] rgb;
   wire       csync, hsync, vsync, hblank, vblank;
   wire [7:0] audio;
   wire [3:0] led/*verilator public_flat*/;
   reg [7:0]  trakball/*verilator public_flat*/;
   reg [7:0]  joystick/*verilator public_flat*/;
   reg [7:0]  sw1/*verilator public_flat*/;
   reg [7:0]  sw2/*verilator public_flat*/;
   reg [9:0]  playerinput/*verilator public_flat*/;

    centipede uut(
		 .clk_12mhz(clk_12),
 		 .reset(reset),
		 .playerinput_i(playerinput),
		 .trakball_i(trakball),
		 .joystick_i(joystick),
		 .sw1_i(sw1),
		 .sw2_i(sw2),
		 .led_o(led),
		 .rgb_o(rgb),
		 .sync_o(csync),
		 .hsync_o(hsync),
		 .vsync_o(vsync),
		 .hblank_o(hblank),
		 .vblank_o(vblank),
		 .audio_o(audio),
		 .clk_6mhz_o(),
       .flip_o(),
       .pause(1'b0),
       .dn_addr(16'b0),
       .dn_data(8'b0),
       .dn_wr(1'b0),
       .hs_address(7'b0),
		 .hs_data_in(8'b0),
		 .hs_data_out(),
		 .hs_write(1'b0),
		 .hs_access(1'b0)
       );
   
endmodule
