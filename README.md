# PSoC-USBUART-Driver
Turnkey code for PSoC USBUART component.

Includes 2 ring buffers for Rx and Tx so we can hold more than the 64 bytes allowed by USBUART.

# Compatibility
This project has been tested on the following hardware:
  * PSoC 5LP (CY8C5888LTI-LP097)

# References
This project uses a simple C ring buffer implementation: https://github.com/dhess/c-ringbuf.

The design of this project is based on the following code: https://github.com/noritan/Design307.

# Instructions


# Setup
## TopDesign
Add the following components:
* 1 x USBUART (named 'USBUART')
* 1 x Clock (set at 2kHz)
* 1 x ISR (named 'int_usbuart') connected to the 2kHz clock

## USBUART component configuration
Descriptor Root = "Manual (Static Allocation)"

## Clocks configurations (.cydwr)
* IMO = 24MHz
* ILO = 100kHz  
* USB = 48Mhz (IMOx2)
* PLL = 79.5MHz (or as high as you want the MASTER_CLK to be)

## Macros (see usbuart_driver.h)
* Size of the FIFO buffers (Rx and Tx): see <RX/TX>_BUFFER_SIZE

## System configurations (.cydwr)
* Heap Size (bytes) = `RX_BUFFER_SIZE` + `TX_BUFFER_SIZE` + 2 bytes
                      
    (+ 512 bytes if you use 'sprintf' in your project)
    
    (+ any more heap required for your application)

## Custom messages
If you want to send/receive messages with a custom structure, make sure this line is uncommented:

    #include "usbuart_driver_msg.h"
  
And fill in the information in the file usbuart_driver_msg.h.

Otherwise, you can comment the line mentionned previously and it will deactivate all the functions related to custom messages.
