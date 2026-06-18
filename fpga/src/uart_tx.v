//==============================================================
// uart_tx.v - передатчик UART 8N1
//   send=1 на 1 такт когда ready=1 -> отправит data
//==============================================================
`default_nettype none

module uart_tx #(
    parameter DELAY = 2812      // 27_000_000 / 9600
)(
    input  wire       clk,
    input  wire [7:0] data,
    input  wire       send,
    output wire       tx,
    output reg        ready = 1'b1
);
    reg [12:0] cnt    = 0;
    reg [3:0]  bit_n  = 0;
    reg [9:0]  shift  = 10'b1111111111;   // в покое = 1

    assign tx = shift[0];

    always @(posedge clk) begin
        if (ready) begin
            if (send) begin
                // [9]=stop(1), [8:1]=data LSB first, [0]=start(0)
                shift <= {1'b1, data, 1'b0};
                cnt   <= 0;
                bit_n <= 0;
                ready <= 1'b0;
            end
        end else begin
            if (cnt + 1 == DELAY) begin
                cnt   <= 0;
                shift <= {1'b1, shift[9:1]};   // сдвиг вправо, в старший бит вкатываем 1
                if (bit_n == 4'd9) ready <= 1'b1;
                else               bit_n <= bit_n + 1'b1;
            end else cnt <= cnt + 1'b1;
        end
    end
endmodule
`default_nettype wire
