# Loop Back Connection Management 

This repository contains code for managing loop back connections between UART and SPI interfaces on a Xilinx board. The loop back connections allow data to be transmitted from one interface and received back on the same interface for testing and debugging purposes.

## Setup and Usage

1. Load the application onto the Xilinx board.
2. Connect to the board's console.
3. Use the following menu commands:

### Menu Commands

#### 1. Enable loop back for UART manager task

- To enable UART Manager task loop back, enter: `<ENTER><1><ENTER>` or `<ENTER><%><ENTER>`
- To disable UART Manager task loop back, enter: `<ENTER><1><ENTER>`

#### 2. Enable loop back for SPI1-SPI0 connection

- To enable SPI1-SPI0 loop back connection, enter: `<ENTER><2><ENTER>` or `<ENTER><%><ENTER>`
- To disable SPI1-SPI0 loop back connection, enter: `<ENTER><2><ENTER>`

### Loop Back Behavior

- **UART Manager Task Loop Back:**
  - When `task1_uart_loopback_en=0`, UART Manager Task loop back is disabled.
  - When `task1_uart_loopback_en=1`, UART Manager Task loop back is enabled.

- **SPI1-SPI0 Loop Back Connection:**
  - When `spi_master_loopback_en=1`, there is no SPI connection in effect. Data will be echoed back using FIFO2.
  - When `spi_master_loopback_en=0`, SPI connection is in effect. Data entered by the user will loop back from SPI1 to SPI0 and again SPI1. The data will then be displayed on the console using FIFO2.

## Initial Setup

- Upon running the application, the console will display the MENU.
- Typing anything from the console initially will result in no output.
- To enable UART loopback inside task1, type `<ENTER><1><ENTER>`.
- Now, any text typed from the console will be echoed back.

## Dependencies

This code utilizes the `xspips.h` and `xspipshw_h` driver functions provided by Xilinx, which are included in the `initialization.h` header file.

## License

This project is licensed under the CC0 1.0 Universal (Provide credits to the author).


