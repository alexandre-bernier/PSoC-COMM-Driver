/*******************************************************************************
*
* PSoC communication driver using FIFO buffers.
* Copyright (C) 2020, Alexandre Bernier
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
********************************************************************************
*
* Summary:
*  Handles communication through USBUART and implement circular buffers to
*  hold more than the 64 bytes allowed by USBUART.
* 
* Required files (see References):
*  ringbuf.h
*  ringbuf.c
*
* Required components in TopDesign:
*  1 x USBUART or UART (named 'COMM')
*
* Configuration of component USBUART (TopDesign):
*  Descriptor Root = "Manual (Static Allocation)"
*
* Clocks configurations for USBUART only (.cydwr):
*  IMO = 24MHz
*  ILO = 100kHz
*  USB = 48MHz (IMOx2)
*  PLL = 79.5MHz (or as high as you want the CPU clock to be)
*
* Macros to set (see below):
*  Select the type of communication block used in your TopDesign.
*  Set the frequency of the interrupt that will fill/empty RX/TX buffers.
*  Set the size of the FIFO buffers (Rx and Tx).
*
* System configurations (.cydwr):
*  Heap Size (bytes) = RX_BUFFER_SIZE + TX_BUFFER_SIZE + 2 bytes
*                      (plus 512 bytes if you use 'sprintf')
*                      (plus any more heap required for your application)
*
* Libraries:
*  You will need to add the 'math' library to the linker. Not doing so will not
*  show any errors during compilation or runtime, but the communication
*  may still not work without any indications of what's wrong. Here are the
*  steps:
*    1- Right-click on your project in the 'Workspace Explorer' and click on
*       'Build Settings...'.
*    2- In the left panel of the window, find the 'Linker' item. It should be
*       inside the compiler item (which can look like
*       'ARM GCC 5.4-2016-q2-update').
*    3- In the 'Additional Libraries' field, simply add the letter 'm' (lower
*       case).
*    4- Done! Click 'OK' to save your changes and close the window.
*
* Custom messages:
*  If you want to send/receive messages with a custom structure, make sure
*  this line is uncommented:
*    #include "usbuart_driver_msg.h"
*  And fill in the information in the file "usbuart_driver_msg.h".
*  Otherwise, you can comment the line mentionned previously and it will
*  deactivate all the functions related to custom messages.
*
* References:
*  https://github.com/noritan/Design307
*  https://github.com/dhess/c-ringbuf
*
********************************************************************************
*
* Revisions:
*  1.0: First.
*  1.1: Bug fix: First TX sent garbage.
*
*******************************************************************************/

#ifndef _COMM_DRIVER_H
#define _COMM_DRIVER_H
    
#include <project.h>
#include <sys/param.h>
#include <stdbool.h>
#include "comm_driver_msg.h"

/*******************************************************************************
* MACROS
*******************************************************************************/
// Select the type of communication block you use.
// Only one of the following macros should be set to '1'. 
// You can also disable the driver by putting all following macros to '0'.
#define USE_USBUART 1
#define USE_UART 0
    
// The desired frequency of the comm interupts.
// It will be converted to a number of ticks of the System Clock (SysClk).
// The frequency entered here cannot be higher than that of the SysClk.
// The number of ticks (SysClk / COMM_INTERRUPT_FREQ) must fit in a 24-bits register.
#define COMM_INTERRUPT_FREQ (2000u)

// Size of the buffers
// Memory allocated will be larger by one byte.
// Make sure the Heap is large enough.
#define RX_BUFFER_SIZE (128u)  
#define TX_BUFFER_SIZE (128u)

// Index of the USBUART component
// Shouldn't be changed unless you have more than one USBFS component.
#define USBFS_DEVICE (0u)

// Terminator of a line of data (limited to a single character)
#define COMM_LINE_TERMINATOR ((uint8)'\n')

/*******************************************************************************
* PUBLIC PROTOTYPES
*******************************************************************************/
// Init
void comm_init();

// Single character
uint8 comm_getch(uint8 *data);
void comm_putch(uint8 *data);

// Line
uint8 comm_getline(uint8 *data);
void comm_putline(uint8 *data, uint8 count);

// Custom messages
#ifdef _COMM_DRIVER_MSG_H
uint8 comm_getmsg(uint8 *data);
void comm_putmsg(uint8 *data, uint8 count);
#endif // _COMM_DRIVER_MSG_H

#endif // _COMM_DRIVER_H
/* [] END OF FILE */
