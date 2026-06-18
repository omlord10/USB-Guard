//==============================================================
// uart_rx.v - приёмник UART 8N1
//   DELAY = clk_freq / baud = 27_000_000 / 9600 = 2812
//   ширина счётчика 13 бит (2^13 = 8192 > 2812)
//==============================================================
`default_nettype none

module uart_rx #(
    parameter DELAY = 2812
)(
    input  wire       clk,
    input  wire       rx,
    output reg  [7:0] data  = 0,
    output reg        valid = 0    // 1 такт когда байт принят
);
    localparam HALF = DELAY / 2;

    localparam S_IDLE  = 0;
    localparam S_START = 1;
    localparam S_WAIT  = 2;
    localparam S_READ  = 3;
    localparam S_STOP  = 4;

    reg [2:0]  state   = S_IDLE;
    reg [12:0] cnt     = 0;
    reg [2:0]  bit_n   = 0;
    reg [7:0]  shift   = 0;

    always @(posedge clk) begin
        valid <= 1'b0;
        case (state)
            S_IDLE: if (rx == 1'b0) begin
                state <= S_START;
                cnt   <= 1;
                bit_n <= 0;
            end
            S_START: if (cnt == HALF) begin
                state <= S_WAIT;
                cnt   <= 1;
            end else cnt <= cnt + 1'b1;
            S_WAIT: if (cnt + 1 == DELAY) begin
                state <= S_READ;
            end else cnt <= cnt + 1'b1;
            S_READ: begin
                shift <= {rx, shift[7:1]};   // LSB first
                cnt   <= 1;
                if (bit_n == 3'd7) state <= S_STOP;
                else begin
                    state <= S_WAIT;
                    bit_n <= bit_n + 1'b1;
                end
            end
            S_STOP: if (cnt + 1 == DELAY) begin
                state <= S_IDLE;
                cnt   <= 0;
                data  <= shift;
                valid <= 1'b1;
            end else cnt <= cnt + 1'b1;
        endcase
    end
endmodule

`default_nettype wire
