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
*******************************************************************************/

#include "usbuart_driver.h"
#include "ringbuf.h"

#define USBUART_MAX_PACKET_SIZE (64u)
#define TX_MAX_REJECT (8u)

/*******************************************************************************
* PRIVATE VARIABLES
*******************************************************************************/
// Buffer to copy bytes from USBUART into FIFO buffers
uint8 _tempBuffer[USBUART_MAX_PACKET_SIZE];

// RX buffer
ringbuf_t _rxBuffer; // Circular buffer for RX operations

// TX buffer
ringbuf_t _txBuffer; // Circular buffer for TX operations
bool _txZlpRequired = false; // Flag to indicate the ZLP is required
uint8 _txReject = 0; // The count of trial rejected by the TX endpoint


/*******************************************************************************
* PRIVATE PROTOTYPES
*******************************************************************************/
void _init_cdc(bool first_init);
void _usbuart_rx_isr();
void _usbuart_tx_isr();


/*******************************************************************************
* INTERRUPTS
*******************************************************************************/
// Must be placed after the functions prototypes (or after their definition)
CY_ISR(int_usbuart_isr) {
    _usbuart_rx_isr();
    _usbuart_tx_isr();
}


/*******************************************************************************
* PUBLIC FUNCTIONS
*******************************************************************************/
/*******************************************************************************
* Function Name: usbuart_init
********************************************************************************
* Summary:
*  Start USBUART and configure it's CDC interface and the interrupts.
*  Should be called once before the infinite loop in your main.
*   
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
void usbuart_init()
{    
    // Allocate memory for the buffers
    _rxBuffer = ringbuf_new(RX_BUFFER_SIZE);
    _txBuffer = ringbuf_new(TX_BUFFER_SIZE);
    
    // Reset buffers
    ringbuf_reset(_rxBuffer);
    ringbuf_reset(_txBuffer);
    
    // Start USBFS component
    USBUART_Start(USBFS_DEVICE, USBUART_5V_OPERATION);
    
    // Wait for USBFS enumaration and configure CDC interface
    _init_cdc(true);
    
    // Start periodic interrupts
    int_usbuart_StartEx(int_usbuart_isr);
}

/*******************************************************************************
* Function Name: usbuart_getch
********************************************************************************
* Summary:
*  Read a byte from the rxBuffer.
*   
* Parameters:
*  data: Pointer to a uint8 where the byte read will be copied.
*
* Return:
*  uint8: The number of bytes copied.
*
*******************************************************************************/
uint8 usbuart_getch(uint8 *data)
{
    uint8 count = 1;
    
    // Exit if 'data' is NULL or if the buffer is empty
    if(!data || ringbuf_is_empty(_rxBuffer))
        return 0;
    
    // Prevent interrupts
    uint8 state = CyEnterCriticalSection();
    
    // Check if USBFS configuration has changed
    _init_cdc(false);
    
    // Extract a single byte from the FIFO buffer
    ringbuf_memcpy_from(data, _rxBuffer, count); 
    
    // Re-enable interrupts
    CyExitCriticalSection(state);
    
    return count;
}

/*******************************************************************************
* Function Name: usbuart_putch
********************************************************************************
* Summary:
*  Write a byte to the txBuffer.
*   
* Parameters:
*  data: Pointer to a uint8 that will be sent through USBUART.
*
* Return:
*  None.
*
*******************************************************************************/
void usbuart_putch(uint8 *data)
{
    uint8 count = 1;
    uint8 state;
    
    // Exit if 'data' is NULL
    if(!data)
        return;
    
    // Wait until there's enough room in the TX buffer
    while(1u) {
        // Prevent interrupts
        state = CyEnterCriticalSection();
        
        // Check if USBFS configuration has changed
        _init_cdc(false);
        
        // Check if there's enough space free in the TX buffer
        if(ringbuf_bytes_free(_txBuffer) >= count) break;
        
        // Re-enable interrupts
        CyExitCriticalSection(state);
    }
    
    // Copy a single byte into the FIFO buffer
    ringbuf_memcpy_into(_txBuffer, data, count); 
    
    // Re-enable interrupts
    CyExitCriticalSection(state);
}

/*******************************************************************************
* Function Name: usbuart_getline
********************************************************************************
* Summary:
*  Read a line from the rxBuffer. A line ends with USBUART_LINE_TERMINATOR
*  (see usbuart_driver.h).
*   
* Parameters:
*  data: Pointer to an array of uint8 where the bytes read will be copied.
*        The line terminator will not be copied.
*
* Return:
*  uint8: The number of bytes returned.
*
*******************************************************************************/
uint8 usbuart_getline(uint8 *data)
{
    // Exit if 'data' is NULL or if the buffer is empty
    if(!data || ringbuf_is_empty(_rxBuffer))
        return 0;
    
    // Look for a line terminator in the buffer, exit if not found
    uint16 line_term_offs = ringbuf_findchr(_rxBuffer, USBUART_LINE_TERMINATOR, 0);
    if(line_term_offs == ringbuf_bytes_used(_rxBuffer))
        return 0;
    
    // Prevent interrupts
    uint8 state = CyEnterCriticalSection();
    
    // Check if USBFS configuration has changed
    _init_cdc(false);
    
    // Extract a line from the FIFO buffer (without the line terminator)
    ringbuf_memcpy_from(data, _rxBuffer, line_term_offs);
    
    // Remove the line terminator from the FIFO buffer
    ringbuf_remove_from_tail(_rxBuffer, 1);
    
    // Re-enable interrupts
    CyExitCriticalSection(state);
    
    return line_term_offs;
}

/*******************************************************************************
* Function Name: usbuart_putline
********************************************************************************
* Summary:
*  Write a line to the txBuffer. The line terminator USBUART_LINE_TERMINATOR
*  will be appended automatically (see usbuart_driver.h).
*   
* Parameters:
*  data: Pointer to an array of uint8 containing the line to send.
*  count: The number of bytes in the array 'data'.
*
* Return:
*  None.
*
*******************************************************************************/
void usbuart_putline(uint8 *data, uint8 count)
{
    uint8 state;
    
    // Exit if 'data' is NULL
    if(!data || count <= 0)
        return;
    
    // Wait until there's enough room in the TX buffer
    while(1u) {
        // Prevent interrupts
        state = CyEnterCriticalSection();
        
        // Check if USBFS configuration has changed
        _init_cdc(false);
        
        // Check if there's enough space free in the TX buffer
        if(ringbuf_bytes_free(_txBuffer) >= count+1) break;
        
        // Re-enable interrupts
        CyExitCriticalSection(state);
    }
    
    // Copy the line into the FIFO buffer
    ringbuf_memcpy_into(_txBuffer, data, count);
    
    // Copy the line terminator into the FIFO buffer
    uint8 line_terminator = USBUART_LINE_TERMINATOR;
    ringbuf_memcpy_into(_txBuffer, &line_terminator, 1);
    
    // Re-enable interrupts
    CyExitCriticalSection(state);
}

#ifdef _USBUART_DRIVER_MSG_H
/*******************************************************************************
* Function Name: usbuart_getmsg
********************************************************************************
* Summary:
*  Read a message from the rxBuffer. A message ends with MSG_LAST_BYTE
*  (see usbuart_drivermsg.h). It may return '0' if a complete message couldn't
*  be found in the FIFO buffer.
*   
* Parameters:
*  data: Pointer to an array of uint8 where the bytes read will be copied.
*        The bytes used to verify the message's integrity will not be copied.
*
* Return:
*  uint8: The number of bytes returned.
*
*******************************************************************************/
uint8 usbuart_getmsg(uint8 *data)
{
    // Exit if 'data' is NULL or if the buffer is empty
    if(!data || ringbuf_is_empty(_rxBuffer))
        return 0;
    
    bool message_found = false;
    uint16 msg_first_byte_offs = 0;
    uint8 msg_length = 0;
    uint8 msg_last_byte = 0;
    
    // Find the first complete message in the FIFO buffer, exit if not found
    while(!message_found) {
        
        // Find the first occurence of MSG_FIRST_BYTE, exit if not found
        msg_first_byte_offs = ringbuf_findchr(_rxBuffer, MSG_FIRST_BYTE, 0);
        if(msg_first_byte_offs == ringbuf_bytes_used(_rxBuffer))
            return 0;
        
        // Remove all bytes until MSG_FIRST_BYTE if it's not at the begginning
        // of the FIFO buffer
        if(msg_first_byte_offs)
            ringbuf_remove_from_tail(_rxBuffer, msg_first_byte_offs);
            
        // Extract the MSG_LENGTH, exit if not found
        if(ringbuf_bytes_used(_rxBuffer) < MSG_HEADER_LENGTH)
            return 0;
        msg_length = ringbuf_peek(_rxBuffer, MSG_LENGTH_OFFS_FROM_FIRST_BYTE);
            
        // Check if MSG_LAST_BYTE is where expected, exit if not enough bytes
        // in FIFO buffer
        if(ringbuf_bytes_used(_rxBuffer) < msg_length)
            return 0;
        msg_last_byte = ringbuf_peek(_rxBuffer, msg_length-1);
        if(msg_last_byte == MSG_LAST_BYTE)
            message_found = true;
            
        // Remove first byte if message not found and try again
        if(!message_found)
            ringbuf_remove_from_tail(_rxBuffer, MSG_LENGTH_OFFS_FROM_FIRST_BYTE);
    }
    
    // Prevent interrupts
    uint8 state = CyEnterCriticalSection();
    
    // Check if USBFS configuration has changed
    _init_cdc(false);
    
    // Remove message header from the FIFO buffer
    ringbuf_remove_from_tail(_rxBuffer, MSG_HEADER_LENGTH);
    
    // Extract the message from the FIFO buffer (without the header/footer)
    uint8 count = msg_length - MSG_STRUCTURE_LENGTH;
    ringbuf_memcpy_from(data, _rxBuffer, count);
    
    // Remove the message footer from the FIFO buffer
    ringbuf_remove_from_tail(_rxBuffer, MSG_FOOTER_LENGTH);
    
    // Re-enable interrupts
    CyExitCriticalSection(state);
    
    return count;
}

/*******************************************************************************
* Function Name: usbuart_putmsg
********************************************************************************
* Summary:
*  Write a message to the txBuffer. The message will be padded with the
*  custom structure found in "usbuart_driver_msg.h".
*   
* Parameters:
*  data: Pointer to an array of uint8 containing the message to send.
*  count: The number of bytes in the array 'data'.
*
* Return:
*  None.
*
*******************************************************************************/
void usbuart_putmsg(uint8 *data, uint8 count)
{
    uint8 state;
    
    // Exit if 'data' is NULL
    if(!data || count <= 0)
        return;
    
    uint8 msg_length = count + MSG_STRUCTURE_LENGTH;
    
    // Wait until there's enough room in the TX buffer
    while(1u) {
        // Prevent interrupts
        state = CyEnterCriticalSection();
        
        // Check if USBFS configuration has changed
        _init_cdc(false);
        
        // Check if there's enough space free in the TX buffer
        if(ringbuf_bytes_free(_txBuffer) >= msg_length) break;
        
        // Re-enable interrupts
        CyExitCriticalSection(state);
    }
    
    // Write the message header into the FIFO buffer
    uint8 msg_header[MSG_HEADER_LENGTH] = {MSG_FIRST_BYTE, msg_length};
    ringbuf_memcpy_into(_txBuffer, msg_header, MSG_HEADER_LENGTH);
    
    // Copy the message into the FIFO buffer
    ringbuf_memcpy_into(_txBuffer, data, count);
    
    // Write the message footer into the FIFO buffer
    uint8 msg_footer[MSG_FOOTER_LENGTH] = {MSG_LAST_BYTE};
    ringbuf_memcpy_into(_txBuffer, msg_footer, MSG_FOOTER_LENGTH);
    
    // Re-enable interrupts
    CyExitCriticalSection(state);
}
#endif // _USBUART_DRIVER_MSG_H


/*******************************************************************************
* PRIVATE FUNCTIONS
*******************************************************************************/
/*******************************************************************************
* Function Name: _init_cdc
********************************************************************************
* Summary:
*  Wait for USBFS enumeration and configure the CDC interface.
*  This should be called periodically to check if the USBFS configuration
*  has changed.
*   
* Parameters:
*  first_init: Must only be 'TRUE' right after the call to USBUART_Start().
*
* Return:
*  None.
*
*******************************************************************************/
void _init_cdc(bool first_init)
{
    // To do only on first init or if configurations have changed
    if(first_init || USBUART_IsConfigurationChanged()) {
        
        // Wait for USBFS to enumerate
        while (!USBUART_GetConfiguration());
        
        // Ensure to clear the CHANGE flag
        USBUART_IsConfigurationChanged();
        
        // Initialize the CDC feature
        USBUART_CDC_Init();
    }
}

/*******************************************************************************
* Function Name: _usbuart_rx_isr
********************************************************************************
* Summary:
*  Copy all available bytes from USBUART into the RX FIFO buffer.
*   
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
void _usbuart_rx_isr()
{
    uint16 count = 0;
    
    // Prevent interrupts
    uint8 state = CyEnterCriticalSection();
    
    // Check if USBUART has data available
    if (USBUART_DataIsReady()) {
        
        // Check that the FIFO buffer has enough free space to receive 
        // all available bytes from USBUART
        if (USBUART_GetCount() <= ringbuf_bytes_free(_rxBuffer)) {
            
            // Copy available bytes into the FIFO buffer
            count = USBUART_GetAll(_tempBuffer);
            ringbuf_memcpy_into(_rxBuffer, _tempBuffer, count);
        }
    }
    
    // Re-enable interrupts
    CyExitCriticalSection(state);
}

/*******************************************************************************
* Function Name: _usbuart_tx_isr
********************************************************************************
* Summary:
*  Try to send everything in TX FIFO buffer into USBUART (or up to 64 bytes).
*   
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
void _usbuart_tx_isr()
{
    uint16 count = 0;
    
    // Prevent interrupts
    uint8 state = CyEnterCriticalSection();
    
    // Check if there's anything in the TX FIFO buffer or if a Zero Length
    // Packet is required
    if (!ringbuf_is_empty(_txBuffer) || _txZlpRequired) {
        
        // Check if USBUART is ready to send data
        if (USBUART_CDCIsReady()) {
            
            // Check the amount of bytes in the buffer
            // Can't send more than USBUART_MAX_PACKET_SIZE bytes
            count = MIN(ringbuf_bytes_used(_txBuffer), USBUART_MAX_PACKET_SIZE);
            
            // Send packet
            ringbuf_memcpy_from(_tempBuffer, _txBuffer, count);
            USBUART_PutData(_tempBuffer, count);
            
            // Clear the buffer
            _txZlpRequired = (count == USBUART_MAX_PACKET_SIZE);
            _txReject = 0;
        }
        
        // Discard the TX FIFO buffer content if USBUART rejects too many times
        else if (++_txReject > TX_MAX_REJECT) {
            ringbuf_reset(_txBuffer);
            _txReject = 0;
        }
        
        // Expect next time
        else {
        }
    }
    
    // Re-enable interrupts
    CyExitCriticalSection(state);
}

/* [] END OF FILE */
