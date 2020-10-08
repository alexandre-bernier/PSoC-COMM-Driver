# PSoC-USBUART-Driver
Turnkey code for PSoC USBUART component.

Includes 2 FIFO ring buffers for Rx and Tx so we can hold more than the 64 bytes allowed by USBUART.

Data transmission (Rx and Tx) is handled automatically by interrupts at 2kHz (every 0.5ms).

# Compatibility
This project has been tested on the following hardware:
  * PSoC 5LP (CY8C5888LTI-LP097)

# References
This project uses a simple C ring buffer implementation: https://github.com/dhess/c-ringbuf.

The design of this project is based on the following code: https://github.com/noritan/Design307.

# Example (main.c)
The following code shows how to do an echo with the 3 different types of messages available.

    #include <project.h>
    #include "usbuart_driver.h"

    int main(void)
    {
        uint8 count = 0;
        uint8 received_data[64];    // Arbitrary array size

        // Initializations
        CyGlobalIntEnable;  // Enable global interrupts
        usbuart_init();     // Start and setup the USBUART driver
    
        // Application
        for(;;) {
            // Single character
            count = usbuart_getch(received_data);
            if(count)
                usbuart_putch(received_data);
            
            // Line of characters (a line ends with USBUART_LINE_TERMINATOR)
            count = usbuart_getline(received_data);
            if(count)
                usbuart_putline(received_data, count);
            
            // Custom message (see usbuart_driver_msg.h)
            count = usbuart_getmsg(received_data);
            if(count)
                usbuart_putmsg(received_data, count);
        }
    }

# Setup
## TopDesign
Add the following components:
* 1 x USBUART (named 'USBUART')
* 1 x Clock (set at 2kHz, or whichever frequency you wish the data transmission to happen)
* 1 x ISR (named 'int_usbuart') connected to the 2kHz clock

## USBUART component configuration
Descriptor Root = "Manual (Static Allocation)"

## Clocks configurations (.cydwr)
* IMO = 24MHz
* ILO = 100kHz  
* USB = 48Mhz (IMOx2)
* PLL = 79.5MHz (or as high as you want the MASTER_CLK to be)

## Macros (see usbuart_driver.h)
* Size of the ring buffers (Rx and Tx): see `<RX/TX>_BUFFER_SIZE`

## System configurations (.cydwr)
* Heap Size (bytes) = `RX_BUFFER_SIZE` + `TX_BUFFER_SIZE` + 2 bytes
                      
    (+ 512 bytes if you use 'sprintf' in your project)
    
    (+ any more heap required for your application)

## Custom messages
If you want to send/receive messages with a custom structure, make sure this line is uncommented:

    #include "usbuart_driver_msg.h"
  
And fill in the information in the file usbuart_driver_msg.h.

Otherwise, you can comment the line mentionned previously and it will deactivate all the functions related to custom messages.
