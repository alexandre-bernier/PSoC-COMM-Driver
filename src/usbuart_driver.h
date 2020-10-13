/*******************************************************************************
*
* PSoC USBUART driver using FIFO buffers.
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
*  1 x USBUART (named 'USBUART')
*  1 x Clock (set at 2kHz) connected to int_usbuart
*  1 x ISR (named 'int_usbuart') connected to the clock mentioned above
*
* Configuration of component USBUART (TopDesign):
*  Descriptor Root = "Manual (Static Allocation)"
*
* Clocks configurations (.cydwr):
*  IMO = 24MHz
*  ILO = 100kHz
*  USB = 48MHz (IMOx2)
*  PLL = 79.5MHz (or as high as you want the MASTER_CLK to be)
*
* Macros (see below):
*  Size of the FIFO buffers (Rx and Tx): see <RX/TX>_BUFFER_SIZE
*
* System configurations (.cydwr):
*  Heap Size (bytes) = RX_BUFFER_SIZE + TX_BUFFER_SIZE + 2 bytes
*                      (plus 512 bytes if you use 'sprintf')
*                      (plus any more heap required for your application)
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

#ifndef _USBUART_DRIVER_H
#define _USBUART_DRIVER_H
    
#include <project.h>
#include <sys/param.h>
#include <stdbool.h>
#include "usbuart_driver_msg.h"

/*******************************************************************************
* MACROS
*******************************************************************************/
// Size of the buffers
// Memory allocated will be larger by one byte.
// Make sure the Heap is large enough.
#define RX_BUFFER_SIZE (256u)  
#define TX_BUFFER_SIZE (256u)

// Index of the USBUART component
// Shouldn't be changed unless you have more than one USBFS component.
#define USBFS_DEVICE (0u)

// Terminator of a line of data (limited to a single character)
#define USBUART_LINE_TERMINATOR ((uint8)'\n')

/*******************************************************************************
* PUBLIC PROTOTYPES
*******************************************************************************/
// Init
void usbuart_init();

// Single character
uint8 usbuart_getch(uint8 *data);
void usbuart_putch(uint8 *data);

// Line
uint8 usbuart_getline(uint8 *data);
void usbuart_putline(uint8 *data, uint8 count);

// Custom messages
#ifdef _USBUART_DRIVER_MSG_H
uint8 usbuart_getmsg(uint8 *data);
void usbuart_putmsg(uint8 *data, uint8 count);
#endif // _USBUART_DRIVER_MSG_H

#endif // _USBUART_DRIVER_H
/* [] END OF FILE */
