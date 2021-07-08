# PSoC-COMM-Driver
Self-contained implementation of PSoC USBUART/UART component.

Includes 2 FIFO ring buffers for Rx and Tx so we can hold more than the 64 bytes allowed by USBUART.

Data transmission (Rx and Tx) is handled automatically by interrupts at 2kHz (every 0.5ms).

# Compatibility
This project has been tested on the following hardware:
  * PSoC 4 (CY8C4244AZI-443)
  * PSoC 5LP (CY8C5888LTI-LP097)

# References
This project uses a simple C ring buffer implementation (provided in src/): https://github.com/dhess/c-ringbuf.

The design of this project is based on the following code: https://github.com/noritan/Design307.

# Example (main.c)
The following code shows how to do an echo with the 3 different types of messages available.

    #include <project.h>
    #include "comm_driver.h"

    int main(void)
    {
        uint8 count = 0;
        uint8 received_data[64];    // Arbitrary array size

        // Initializations
        CyGlobalIntEnable;  // Enable global interrupts
        comm_init();     // Start and setup the COMM driver
    
        // Application
        for(;;) {
            // Single character
            count = comm_getch(received_data);
            if(count)
                comm_putch(received_data);
            
            // Line of characters (a line ends with COMM_LINE_TERMINATOR)
            count = comm_getline(received_data);
            if(count)
                comm_putline(received_data, count);
            
            // Custom message (see comm_driver_msg.h)
            count = comm_getmsg(received_data);
            if(count)
                comm_putmsg(received_data, count);
        }
    }

# Setup
## TopDesign
Add the following components:
* 1 x USBUART or UART (named 'COMM')

## USBUART component configuration
Descriptor Root = "Manual (Static Allocation)"

## Clocks configurations for USBUART (.cydwr)
* IMO = 24MHz
* ILO = 100kHz  
* USB = 48Mhz (IMOx2)
* PLL = 79.5MHz (or as high as you want the MASTER_CLK to be)

## Clocks configurations for UART (.cydwr)
* SysClk > COMM_INTERRUPT_FREQ

## Macros (see comm_driver.h)
* Size of the ring buffers (Rx and Tx): see `<RX/TX>_BUFFER_SIZE`

## System configurations (.cydwr)
* Heap Size (bytes) = `RX_BUFFER_SIZE` + `TX_BUFFER_SIZE` + 2 bytes
                      
    (+ 512 bytes if you use 'sprintf' in your project)
    
    (+ any more heap required for your application)

## Custom messages
If you want to send/receive messages with a custom structure, make sure this line is uncommented:

    #include "comm_driver_msg.h"
  
And fill in the information in the file comm_driver_msg.h.

Otherwise, you can comment the line mentionned previously and it will deactivate all the functions related to custom messages.
