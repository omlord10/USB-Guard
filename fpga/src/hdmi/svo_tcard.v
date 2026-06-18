/*
 *  SVO - Simple Video Out FPGA Core
 *
 *  Copyright (C) 2014  Clifford Wolf <clifford@clifford.at>
 *  
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Modified for USBGuard v5: чёрный фон вместо цветной тест-карты.
 */

`timescale 1ns / 1ps
`include "svo_defines.vh"

module svo_tcard #( `SVO_DEFAULT_PARAMS ) (
	input clk, resetn,

	// output stream
	//   tuser[0] ... start of frame
	output reg out_axis_tvalid,
	input out_axis_tready,
	output reg [SVO_BITS_PER_PIXEL-1:0] out_axis_tdata,
	output reg [0:0] out_axis_tuser
);
	`SVO_DECLS

	reg [`SVO_XYBITS-1:0] hcursor;
	reg [`SVO_XYBITS-1:0] vcursor;

	always @(posedge clk) begin
		if (!resetn) begin
			hcursor <= 0;
			vcursor <= 0;
			out_axis_tvalid <= 0;
			out_axis_tdata <= 0;
			out_axis_tuser <= 0;
		end else
		if (!out_axis_tvalid || out_axis_tready) begin
			out_axis_tvalid <= 1;
			out_axis_tdata  <= {SVO_BITS_PER_PIXEL{1'b0}};   // чёрный фон
			out_axis_tuser[0] <= !hcursor && !vcursor;

			if (hcursor == SVO_HOR_PIXELS-1) begin
				hcursor <= 0;
				if (vcursor == SVO_VER_PIXELS-1)
					vcursor <= 0;
				else
					vcursor <= vcursor + 1;
			end else begin
				hcursor <= hcursor + 1;
			end
		end
	end
endmodule
