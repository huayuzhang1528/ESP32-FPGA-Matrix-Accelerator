module top(
    input  clk,
    input  sclk,
    input  cs,
    input  mosi,
    output miso,
    output done
);
    reg [2:0] sclk_r = 0;
    reg [2:0] cs_r   = 0;
    reg [1:0] mosi_r = 0;

    always @(posedge clk) sclk_r <= {sclk_r[1:0], sclk};
    always @(posedge clk) cs_r   <= {cs_r[1:0], cs};
    always @(posedge clk) mosi_r <= {mosi_r[0], mosi};

    wire sclk_rising  = (sclk_r[2:1] == 2'b01);
    wire sclk_falling = (sclk_r[2:1] == 2'b10);
    wire cs_active    = ~cs_r[1];
    wire cs_falling   = (cs_r[2:1] == 2'b10);
    wire mosi_data    = mosi_r[1];

    reg [2:0]  bit_cnt  = 0;
    reg [3:0]  byte_cnt = 0;
    reg [7:0]  rx_reg   = 0;
    reg [7:0]  a00=0, a01=0, a10=0, a11=0;
    reg [7:0]  b00=0, b01=0, b10=0;
    reg        done_reg = 0;
    reg [63:0] result   = 0;
    reg [63:0] tx_reg   = 0;
    reg        reading  = 0;

    wire [7:0] current_byte = {rx_reg[6:0], mosi_data};

    always @(posedge clk) begin
        if (cs_falling) begin
            if (done_reg) begin
                reading <= 1;
                tx_reg  <= result;
            end else begin
                bit_cnt  <= 0;
                byte_cnt <= 0;
                rx_reg   <= 0;
                reading  <= 0;
            end
        end else if (cs_active) begin
            if (!reading && sclk_rising) begin
                rx_reg  <= current_byte;
                bit_cnt <= bit_cnt + 1;
                if (bit_cnt == 3'd7) begin
                    bit_cnt <= 0;
                    case (byte_cnt)
                        4'd0: begin a00 <= current_byte; byte_cnt <= 4'd1; end
                        4'd1: begin a01 <= current_byte; byte_cnt <= 4'd2; end
                        4'd2: begin a10 <= current_byte; byte_cnt <= 4'd3; end
                        4'd3: begin a11 <= current_byte; byte_cnt <= 4'd4; end
                        4'd4: begin b00 <= current_byte; byte_cnt <= 4'd5; end
                        4'd5: begin b01 <= current_byte; byte_cnt <= 4'd6; end
                        4'd6: begin b10 <= current_byte; byte_cnt <= 4'd7; end
                        4'd7: begin
                            // current_byte是b11，直接用不经过寄存器
                            done_reg <= 1;
                            result   <= {
                                {8'b0,a00}*{8'b0,b00} + {8'b0,a01}*{8'b0,b10},
                                {8'b0,a00}*{8'b0,b01} + {8'b0,a01}*{8'b0,current_byte},
                                {8'b0,a10}*{8'b0,b00} + {8'b0,a11}*{8'b0,b10},
                                {8'b0,a10}*{8'b0,b01} + {8'b0,a11}*{8'b0,current_byte}
                            };
                            byte_cnt <= 4'd8;
                        end
                        default:;
                    endcase
                end
            end else if (reading && sclk_falling) begin
                tx_reg <= {tx_reg[62:0], 1'b0};
            end
        end else begin
            if (done_reg && reading) begin
                done_reg <= 0;
                reading  <= 0;
                bit_cnt  <= 0;
                byte_cnt <= 0;
            end
        end
    end

    assign miso = tx_reg[63];
    assign done = done_reg;
endmodule
