# ESP32-FPGA Matrix Multiplication Accelerator

A cloud-to-edge hardware acceleration system for matrix multiplication.

## System Architecture
Phone (Telegram App)
↓  HTTPS
Telegram Bot API
↓  WiFi
ESP32 (Scheduler)
↓  SPI
FPGA (Hardware Accelerator)
## Hardware

- **FPGA**: Gowin Tang Nano 4K (GW1NSR-LV4C)
- **MCU**: ESP32 Dev Module
- **Communication**: SPI @ 100KHz

## How It Works

1. User sends matrix multiplication command via Telegram
2. ESP32 receives the command over WiFi
3. ESP32 tiles large matrices into 2×2 blocks
4. Each 2×2 block is sent to the FPGA via SPI
5. FPGA computes hardware matrix multiplication and asserts DONE signal
6. ESP32 reads result, accumulates, and reconstructs final matrix
7. Result is sent back to user via Telegram

## Supported Matrix Sizes

| Command | Size | FPGA Calls |
|---------|------|------------|
| /multiply2 | 2×2 | 1 |
| /multiply3 | 3×3 | 8 (zero-padded to 4×4) |
| /multiply4 | 4×4 | 8 |
| /multiply5 | 5×5 | 32 (zero-padded to 6×6) |
| /multiply6 | 6×6 | 32 |
| /multiply7 | 7×7 | 128 (zero-padded to 8×8) |
| /multiply8 | 8×8 | 128 |

## Usage
/multiplyN [N×N numbers for A] [N×N numbers for B]
Example (2×2): 
/multiply2 1 2 3 4 5 6 7 8
Result C = A*B:
| 19  22 |
| 43  50 |
## Pin Connections

| Signal | FPGA Pin | ESP32 GPIO |
|--------|----------|------------|
| MOSI   | 31       | 23         |
| MISO   | 32       | 19         |
| SCLK   | 40       | 18         |
| CS     | 39       | 5          |
| DONE   | 42       | 4          |
| CLK    | 45       | -          |

## Files

- `esp32_controller.ino` - ESP32 Arduino code (Telegram Bot + SPI master + matrix tiling)
- `fpga_matrix_accelerator.v` - Gowin FPGA Verilog (2×2 hardware matrix multiplier)
- `fpga_constraints.cst` - FPGA I/O pin constraints

## Key Concepts

- **Tiled Matrix Multiplication**: Large matrices are divided into 2×2 blocks, matching the FPGA accelerator size
- **Zero Padding**: Odd-sized matrices are padded to the next even size, results are trimmed
- **Hardware-Software Co-design**: FPGA handles computation, ESP32 handles scheduling and communication
- **Done Handshaking**: FPGA asserts DONE signal when result is ready, ESP32 polls before reading
