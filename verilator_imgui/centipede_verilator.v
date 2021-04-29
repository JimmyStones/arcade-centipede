//
//
//

`timescale 1ns/1ns

`define SDL_DISPLAY

module top(VGA_R,VGA_B,VGA_G,VGA_HS,VGA_VS,reset,clk_sys,clk_vid,ioctl_upload,ioctl_download,ioctl_addr,ioctl_dout,ioctl_din,ioctl_index,ioctl_wait,ioctl_wr);

   input clk_sys/*verilator public_flat*/;
   input clk_vid/*verilator public_flat*/;
   input reset/*verilator public_flat*/;

   output [7:0] VGA_R/*verilator public_flat*/;
   output [7:0] VGA_G/*verilator public_flat*/;
   output [7:0] VGA_B/*verilator public_flat*/;

   output VGA_HS;
   output VGA_VS;

   input        ioctl_upload;
   input        ioctl_download;
   input        ioctl_wr;
   input [24:0] ioctl_addr;
   input [7:0] ioctl_dout;
   output [7:0] ioctl_din;
   input [7:0]  ioctl_index;
   output  reg     ioctl_wait=1'b0;

   wire clk = clk_sys;


   wire [8:0] rgb;
   wire       csync, hsync, vsync, hblank, vblank;
   wire [7:0] audio;
   wire [3:0] led/*verilator public_flat*/;

   reg [7:0]  trakball/*verilator public_flat*/;
   reg [7:0]  joystick/*verilator public_flat*/;
   reg [7:0]  sw1/*verilator public_flat*/;
   reg [7:0]  sw2/*verilator public_flat*/;
   
   reg [9:0]  playerinput/*verilator public_flat*/;
  
   assign sw1 = 8'b01000001;

wire [9:0]ram_address;
 
   centipede uut(
		 .clk_12mhz(clk),
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
		.clk_6mhz_o()
		 );

always @(posedge clk) begin
if (ioctl_upload)
    $display("ioctl_addr %x ram_address %x ioctl_din %x ", ioctl_addr, ram_address ,ioctl_din );
end

   wire [2:0]  vgaBlue;
   wire [2:0]  vgaGreen;
   wire [2:0]  vgaRed;

   assign VGA_R = {vgaRed,vgaRed,vgaRed[2:1]};
   assign VGA_G = {vgaGreen,vgaGreen,vgaGreen[2:1]};
   assign VGA_B = {vgaBlue,vgaBlue,vgaBlue[2:1]};
   assign vgaBlue  = rgb[8:6];
   assign vgaGreen = rgb[5:3];
   assign vgaRed   = rgb[2:0];

   assign VGA_VS=~vsync; 
   assign VGA_HS=~hsync; 
 
endmodule // ff_tb

