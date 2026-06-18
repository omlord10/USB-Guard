//==============================================================
// Tang Nano 9K — USBGuard v5
//
// UART bridge (Arduino <-> Tang) + LED control + HDMI terminal
//
// КНОПКИ -> UART TX:
//   btn[0] (P26) -> 'h'   (offset +100)
//   btn[1] (P27) -> 'k'   (offset +10 000)
//   btn[2] (P28) -> 'm'   (offset +1 000 000)
//   S1     (P4)  -> 's'   (start scan)
//   S2     (P3)  -> "res" (reset offset)
//
// UART RX -> LED + HDMI terminal:
//   Все принятые байты отображаются на HDMI-терминале.
//
//   Протокол управления LED:
//     Байт 0x01 (ESC) + следующий байт -> команда LED/статус.
//     Все остальные байты -> только HDMI-терминал (LED не затрагиваются).
//
//   РУЧНОЙ (после ESC):
//     '0'..'5' -> toggle LED P10..P16
//     'g'      -> toggle green   'r' -> toggle red
//
//   СТАТУСНЫЙ (после ESC):
//     'R' READY    -> зелёный медленно мигает
//     'N' NO_MEDIA -> зелёный одиночные вспышки
//     'W' WORKING  -> попеременно
//     'G' GOOD     -> зелёный горит
//     'B' BAD      -> красный горит
//     'E' ERROR    -> оба быстро мигают
//
// HDMI: 640x480 @ 60 Hz
// Скорость UART: 9600 8N1
//==============================================================
`default_nettype none

module top (
    input  wire       sys_clk,       // P52, 27 MHz
    input  wire       s1,            // P4  (S1 on board), active LOW
    input  wire       s2,            // P3  (S2 on board), active LOW
    input  wire [2:0] btn,           // P26, P27, P28 (active HIGH, ext pull-down)
    input  wire       uart_rx_pin,   // P34, от Arduino TX (через LLC)
    output wire       uart_tx_pin,   // P33, к Arduino RX (через LLC)
    output wire [5:0] led_ob,        // P10, P11, P13, P14, P15, P16 (active LOW)
    output wire       led_green,     // P29, active HIGH
    output wire       led_red,       // P30, active HIGH
    // HDMI
    output wire       tmds_clk_n,
    output wire       tmds_clk_p,
    output wire [2:0] tmds_d_n,
    output wire [2:0] tmds_d_p
);

    //==========================================================
    //  PLL: 27 MHz -> 126 MHz (clk_p5) -> /5 = 25.2 MHz (clk_p)
    //==========================================================
    wire clk_p5, clk_p, pll_lock;

    Gowin_rPLL u_pll (
        .clkin  (sys_clk),
        .clkout (clk_p5),
        .lock   (pll_lock)
    );

    Gowin_CLKDIV u_div_5 (
        .clkout (clk_p),
        .hclkin (clk_p5),
        .resetn (pll_lock)
    );

    //==========================================================
    //  Power-on reset  (~2.4 мс при 27 MHz)
    //==========================================================
    reg [15:0] por_cnt = 0;
    wire       por_done = &por_cnt;
    always @(posedge sys_clk)
        if (!por_done) por_cnt <= por_cnt + 1'b1;

    wire sys_resetn = por_done & pll_lock;

    // Reset synchronizer for pixel clock domain
    wire pixel_resetn;
    Reset_Sync u_rsync (
        .clk       (clk_p),
        .ext_reset (sys_resetn),
        .resetn    (pixel_resetn)
    );

    //==========================================================
    //  Двухтриггерная синхронизация входов
    //==========================================================
    reg [2:0] btn_s0 = 0, btn_s1 = 0;
    reg [1:0] s1_s = 2'b11;
    reg [1:0] s2_s = 2'b11;

    always @(posedge sys_clk) begin
        if (!sys_resetn) begin
            btn_s0 <= 3'b000;
            btn_s1 <= 3'b000;
            s1_s   <= 2'b11;
            s2_s   <= 2'b11;
        end else begin
            btn_s0 <= btn;
            btn_s1 <= btn_s0;
            s1_s   <= {s1_s[0], s1};
            s2_s   <= {s2_s[0], s2};
        end
    end

    //==========================================================
    //  Антидребезг ~39 мс  (2^20 / 27 MHz ≈ 38.8 мс)
    //==========================================================
    localparam DB_W = 20;
    reg [DB_W-1:0] db0=0, db1=0, db2=0, db_s1=0, db_s2=0;
    reg [2:0] btn_stable   = 3'b000;
    reg [2:0] btn_stable_d = 3'b000;
    reg       s1_stable    = 1'b1;
    reg       s1_stable_d  = 1'b1;
    reg       s2_stable    = 1'b1;
    reg       s2_stable_d  = 1'b1;

    always @(posedge sys_clk) begin
        if (!sys_resetn) begin
            db0<=0; db1<=0; db2<=0; db_s1<=0; db_s2<=0;
            btn_stable   <= 3'b000;
            btn_stable_d <= 3'b000;
            s1_stable    <= 1'b1;
            s1_stable_d  <= 1'b1;
            s2_stable    <= 1'b1;
            s2_stable_d  <= 1'b1;
        end else begin
            btn_stable_d <= btn_stable;
            s1_stable_d  <= s1_stable;
            s2_stable_d  <= s2_stable;

            // btn[0] = P26
            if (btn_s1[0] == btn_stable[0]) db0 <= 0;
            else begin db0 <= db0 + 1'b1; if (&db0) btn_stable[0] <= btn_s1[0]; end

            // btn[1] = P27
            if (btn_s1[1] == btn_stable[1]) db1 <= 0;
            else begin db1 <= db1 + 1'b1; if (&db1) btn_stable[1] <= btn_s1[1]; end

            // btn[2] = P28
            if (btn_s1[2] == btn_stable[2]) db2 <= 0;
            else begin db2 <= db2 + 1'b1; if (&db2) btn_stable[2] <= btn_s1[2]; end

            // S1 = P4
            if (s1_s[1] == s1_stable) db_s1 <= 0;
            else begin db_s1 <= db_s1 + 1'b1; if (&db_s1) s1_stable <= s1_s[1]; end

            // S2 = P3
            if (s2_s[1] == s2_stable) db_s2 <= 0;
            else begin db_s2 <= db_s2 + 1'b1; if (&db_s2) s2_stable <= s2_s[1]; end
        end
    end

    wire [2:0] btn_press = btn_stable & ~btn_stable_d;   // rising edge (active HIGH)
    wire       s1_press  = ~s1_stable & s1_stable_d;     // falling edge (active LOW)
    wire       s2_press  = ~s2_stable & s2_stable_d;     // falling edge (active LOW)

    //==========================================================
    //  UART RX
    //==========================================================
    wire [7:0] rx_data;
    wire       rx_valid;

    uart_rx u_rx (
        .clk   (sys_clk),
        .rx    (uart_rx_pin),
        .data  (rx_data),
        .valid (rx_valid)
    );

    //==========================================================
    //  Escape-протокол: 0x01 + байт = команда LED/статуса
    //  Все остальные байты -> только HDMI-терминал
    //==========================================================
    localparam ESC_BYTE = 8'h01;

    reg cmd_escape = 1'b0;

    // Классификация входящих байтов
    wire rx_is_esc  = rx_valid & ~cmd_escape & (rx_data == ESC_BYTE);
    wire rx_is_cmd  = rx_valid &  cmd_escape;
    wire rx_is_text = rx_valid & ~cmd_escape & (rx_data != ESC_BYTE);

    always @(posedge sys_clk) begin
        if (!sys_resetn)
            cmd_escape <= 1'b0;
        else if (rx_is_esc)
            cmd_escape <= 1'b1;
        else if (rx_is_cmd)
            cmd_escape <= 1'b0;
    end

    //==========================================================
    //  Онбордовые LED (ручной toggle) + статусные паттерны
    //  Реагируют ТОЛЬКО на rx_is_cmd (после ESC-префикса)
    //==========================================================
    reg [5:0] led_state   = 6'b000000;
    reg       green_man   = 1'b0;
    reg       red_man     = 1'b0;
    reg       mode_status = 1'b0;

    localparam ST_READY   = 3'd0;
    localparam ST_NOMEDIA = 3'd1;
    localparam ST_WORK    = 3'd2;
    localparam ST_GOOD    = 3'd3;
    localparam ST_BAD     = 3'd4;
    localparam ST_ERROR   = 3'd5;

    reg [2:0] status = ST_READY;

    always @(posedge sys_clk) begin
        if (!sys_resetn) begin
            led_state   <= 6'b000000;
            green_man   <= 1'b0;
            red_man     <= 1'b0;
            mode_status <= 1'b0;
            status      <= ST_READY;
        end else if (rx_is_cmd) begin
            case (rx_data)
                "0": begin led_state[0] <= ~led_state[0]; mode_status <= 1'b0; end
                "1": begin led_state[1] <= ~led_state[1]; mode_status <= 1'b0; end
                "2": begin led_state[2] <= ~led_state[2]; mode_status <= 1'b0; end
                "3": begin led_state[3] <= ~led_state[3]; mode_status <= 1'b0; end
                "4": begin led_state[4] <= ~led_state[4]; mode_status <= 1'b0; end
                "5": begin led_state[5] <= ~led_state[5]; mode_status <= 1'b0; end
                "g": begin green_man    <= ~green_man;    mode_status <= 1'b0; end
                "r": begin red_man      <= ~red_man;      mode_status <= 1'b0; end
                "R": begin status <= ST_READY;   mode_status <= 1'b1; end
                "N": begin status <= ST_NOMEDIA; mode_status <= 1'b1; end
                "W": begin status <= ST_WORK;    mode_status <= 1'b1; end
                "G": begin status <= ST_GOOD;    mode_status <= 1'b1; end
                "B": begin status <= ST_BAD;     mode_status <= 1'b1; end
                "E": begin status <= ST_ERROR;   mode_status <= 1'b1; end
                default: ;
            endcase
        end
    end

    assign led_ob = ~led_state;

    //==========================================================
    //  Генератор LED-паттернов
    //==========================================================
    reg [24:0] tick = 0;
    always @(posedge sys_clk) tick <= tick + 1'b1;

    wire slow_blink   = tick[24];
    wire med_blink    = tick[23];
    wire fast_blink   = tick[22];
    wire single_flash = (tick[24:21] == 4'b0000);

    reg green_auto, red_auto;

    always @(*) begin
        green_auto = 1'b0;
        red_auto   = 1'b0;
        case (status)
            ST_READY:   begin green_auto = slow_blink;   red_auto = 1'b0;       end
            ST_NOMEDIA: begin green_auto = single_flash; red_auto = 1'b0;       end
            ST_WORK:    begin green_auto = med_blink;    red_auto = ~med_blink;  end
            ST_GOOD:    begin green_auto = 1'b1;         red_auto = 1'b0;       end
            ST_BAD:     begin green_auto = 1'b0;         red_auto = 1'b1;       end
            ST_ERROR:   begin green_auto = fast_blink;   red_auto = fast_blink;  end
            default:    begin green_auto = 1'b0;         red_auto = 1'b0;       end
        endcase
    end

    assign led_green = mode_status ? green_auto : green_man;
    assign led_red   = mode_status ? red_auto   : red_man;

    //==========================================================
    //  UART TX — отправка по нажатию кнопок
    //  btn[0] -> "h\n"   btn[1] -> "k\n"   btn[2] -> "m\n"
    //  S1     -> "s\n"   S2     -> "res\n" (4 символа)
    //==========================================================
    reg  [7:0] tx_data = 0;
    reg        tx_send = 0;
    wire       tx_ready;

    uart_tx u_tx (
        .clk   (sys_clk),
        .data  (tx_data),
        .send  (tx_send),
        .tx    (uart_tx_pin),
        .ready (tx_ready)
    );

    // Буфер до 4 символов для отправки
    reg [7:0]  tx_buf [0:3];
    reg [2:0]  tx_len   = 0;    // сколько всего символов
    reg [2:0]  tx_idx   = 0;    // текущий индекс

    localparam TXS_IDLE  = 0;
    localparam TXS_SEND  = 1;
    localparam TXS_WAIT  = 2;

    reg [1:0] tx_state = TXS_IDLE;

    always @(posedge sys_clk) begin
        if (!sys_resetn) begin
            tx_state <= TXS_IDLE;
            tx_send  <= 1'b0;
            tx_len   <= 0;
            tx_idx   <= 0;
        end else begin
            tx_send <= 1'b0;

            case (tx_state)
                TXS_IDLE: begin
                    if (btn_press[0]) begin
                        tx_buf[0] <= "h";  tx_buf[1] <= 8'h0A;
                        tx_len <= 3'd2;  tx_idx <= 0;  tx_state <= TXS_SEND;
                    end
                    else if (btn_press[1]) begin
                        tx_buf[0] <= "k";  tx_buf[1] <= 8'h0A;
                        tx_len <= 3'd2;  tx_idx <= 0;  tx_state <= TXS_SEND;
                    end
                    else if (btn_press[2]) begin
                        tx_buf[0] <= "m";  tx_buf[1] <= 8'h0A;
                        tx_len <= 3'd2;  tx_idx <= 0;  tx_state <= TXS_SEND;
                    end
                    else if (s1_press) begin
                        tx_buf[0] <= "s";  tx_buf[1] <= 8'h0A;
                        tx_len <= 3'd2;  tx_idx <= 0;  tx_state <= TXS_SEND;
                    end
                    else if (s2_press) begin
                        tx_buf[0] <= "r";  tx_buf[1] <= "e";
                        tx_buf[2] <= "s";  tx_buf[3] <= 8'h0A;
                        tx_len <= 3'd4;  tx_idx <= 0;  tx_state <= TXS_SEND;
                    end
                end

                TXS_SEND: begin
                    if (tx_ready) begin
                        tx_data  <= tx_buf[tx_idx];
                        tx_send  <= 1'b1;
                        tx_state <= TXS_WAIT;
                    end
                end

                TXS_WAIT: begin
                    // Ждём один такт после send, затем проверяем есть ли ещё
                    if (tx_idx + 1 == tx_len)
                        tx_state <= TXS_IDLE;
                    else begin
                        tx_idx   <= tx_idx + 1'b1;
                        tx_state <= TXS_SEND;
                    end
                end
            endcase
        end
    end

    //==========================================================
    //  UART RX -> HDMI Terminal  (буфер 1 байт)
    //
    //  В терминал попадают ТОЛЬКО обычные текстовые байты
    //  (rx_is_text). ESC-префикс (0x01) и команды LED/статуса
    //  НЕ отображаются на экране.
    //==========================================================
    reg       term_valid = 1'b0;
    reg [7:0] term_data  = 8'd0;
    wire      term_ready;

    always @(posedge sys_clk) begin
        if (!sys_resetn) begin
            term_valid <= 1'b0;
        end else begin
            if (rx_is_text) begin
                term_valid <= 1'b1;
                term_data  <= rx_data;
            end else if (term_valid && term_ready) begin
                term_valid <= 1'b0;
            end
        end
    end

    //==========================================================
    //  HDMI — SVO pipeline (640×480 @ 60 Hz)
    //==========================================================
    svo_hdmi u_hdmi (
        .clk          (sys_clk),
        .resetn       (sys_resetn),

        .clk_pixel    (clk_p),
        .clk_5x_pixel (clk_p5),
        .locked       (pll_lock),

        // terminal: UART RX bytes → текст на экране
        .term_in_tvalid (term_valid),
        .term_in_tready (term_ready),
        .term_in_tdata  (term_data),

        .tmds_clk_n   (tmds_clk_n),
        .tmds_clk_p   (tmds_clk_p),
        .tmds_d_n     (tmds_d_n),
        .tmds_d_p     (tmds_d_p)
    );

endmodule


//==============================================================
//  Reset Sync (из референсного проекта)
//==============================================================
module Reset_Sync (
    input  wire clk,
    input  wire ext_reset,
    output wire resetn
);
    reg [3:0] reset_cnt = 0;

    always @(posedge clk or negedge ext_reset) begin
        if (~ext_reset)
            reset_cnt <= 4'b0;
        else
            reset_cnt <= reset_cnt + !resetn;
    end

    assign resetn = &reset_cnt;
endmodule

`default_nettype wire
