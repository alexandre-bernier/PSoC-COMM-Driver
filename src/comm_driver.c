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

#include "comm_driver.h"
#include "ringbuf.h"

// Verification
#if USE_USBUART || USE_UART
    #if CYDEV_HEAP_SIZE < (RX_BUFFER_SIZE + TX_BUFFER_SIZE + 2)
        #error Invalid HEAP size! You need at least (RX_BUFFER_SIZE + TX_BUFFER_SIZE + 2) bytes for comm_driver
    #endif
#endif
#if !USE_USBUART && !USE_UART
    #warning Both USE_USBUART and USE_UART are set to '0'
#endif
    
// TX specific macros
#if USE_USBUART
    #define COMM_TX_MAX_PACKET_SIZE (64u)
#elif USE_UART
    #define COMM_TX_MAX_PACKET_SIZE (COMM_UART_TX_BUFFER_SIZE)
#else
    #define COMM_TX_MAX_PACKET_SIZE (0u)
#endif
#define TX_MAX_REJECT (8u)

// Interrupt macros
#if CY_PSOC5LP
    #define COMM_INT_NB_TICKS (BCLK__BUS_CLK__HZ / COMM_INTERRUPT_FREQ)
    #define SYSTICK_INT_NUM (CY_INT_SYSTICK_IRQN)
#elif CY_PSOC4
    #define COMM_INT_NB_TICKS (CYDEV_BCLK__SYSCLK__HZ / COMM_INTERRUPT_FREQ)
    #define SYSTICK_INT_NUM (SysTick_IRQn + 16)
#endif

/*******************************************************************************
* PRIVATE VARIABLES
*******************************************************************************/
// Buffer to copy bytes from communication block into FIFO buffers
uint8 _tempBuffer[COMM_TX_MAX_PACKET_SIZE];

// RX buffer
ringbuf_t _rxBuffer; // Circular buffer for RX operations

// TX buffer
ringbuf_t _txBuffer; // Circular buffer for TX operations
#if USE_USBUART
bool _txZlpRequired = false; // Flag to indicate the ZLP is required
uint8 _txReject = 0; // The count of trial rejected by the TX endpoint
#endif


/*******************************************************************************
* PRIVATE PROTOTYPES
*******************************************************************************/
#if USE_USBUART
void _init_cdc(bool first_init);
#endif
void _comm_rx_isr();
void _comm_tx_isr();


/*******************************************************************************
* INTERRUPTS
*******************************************************************************/
// Must be placed after the functions prototypes (or after their definition)
CY_ISR(int_comm_isr) {
    _comm_rx_isr();
    _comm_tx_isr();
}


/*******************************************************************************
* PUBLIC FUNCTIONS
*******************************************************************************/
/*******************************************************************************
* Function Name: comm_init
********************************************************************************
* Summary:
*  Start communication block and configure it and the interrupts.
*  Should be called once before the infinite loop in your main.
*   
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
void comm_init()
{    
    // Allocate memory for the buffers
    _rxBuffer = ringbuf_new(RX_BUFFER_SIZE);
    _txBuffer = ringbuf_new(TX_BUFFER_SIZE);
    
    // Reset buffers
    ringbuf_reset(_rxBuffer);
    ringbuf_reset(_txBuffer);
    
#if USE_USBUART
    // Start USBFS component
    COMM_Start(USBFS_DEVICE, COMM_5V_OPERATION);
    
    // Wait for USBFS enumaration and configure CDC interface
    _init_cdc(true);
#elif USE_UART
    // Start UART component
    COMM_Start();
    
    // Clear COMM buffers
    COMM_SpiUartClearRxBuffer();
    COMM_SpiUartClearTxBuffer();
#endif
    
    // Setup interrupt
    CyIntSetSysVector(SYSTICK_INT_NUM, int_comm_isr);
    SysTick_Config(COMM_INT_NB_TICKS);
    NVIC_EnableIRQ(SYSTICK_INT_NUM);
    CyGlobalIntEnable;  // In case it wasn't done if the main.
}

/*******************************************************************************
* Function Name: comm_getch
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
uint8 comm_getch(uint8 *data)
{
    uint8 count = 1;
    
    // Exit if 'data' is NULL or if the buffer is empty
    if(!data || ringbuf_is_empty(_rxBuffer))
        return 0;
    
    // Prevent interrupts
    uint8 state = CyEnterCriticalSection();
    
    // Extract a single byte from the FIFO buffer
    ringbuf_memcpy_from(data, _rxBuffer, count); 
    
    // Re-enable interrupts
    CyExitCriticalSection(state);
    
    return count;
}

/*******************************************************************************
* Function Name: comm_putch
********************************************************************************
* Summary:
*  Write a byte to the txBuffer.
*   
* Parameters:
*  data: Pointer to a uint8 that will be sent through the COMM block.
*
* Return:
*  None.
*
*******************************************************************************/
void comm_putch(uint8 *data)
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
* Function Name: comm_getline
********************************************************************************
* Summary:
*  Read a line from the rxBuffer. A line ends with COMM_LINE_TERMINATOR
*  (see comm_driver.h).
*   
* Parameters:
*  data: Pointer to an array of uint8 where the bytes read will be copied.
*        The line terminator will not be copied.
*
* Return:
*  uint8: The number of bytes returned.
*
*******************************************************************************/
uint8 comm_getline(uint8 *data)
{
    // Exit if 'data' is NULL or if the buffer is empty
    if(!data || ringbuf_is_empty(_rxBuffer))
        return 0;
    
    // Look for a line terminator in the buffer, exit if not found
    uint16 line_term_offs = ringbuf_findchr(_rxBuffer, COMM_LINE_TERMINATOR, 0);
    if(line_term_offs == ringbuf_bytes_used(_rxBuffer))
        return 0;
    
    // Prevent interrupts
    uint8 state = CyEnterCriticalSection();
    
    // Extract a line from the FIFO buffer (without the line terminator)
    ringbuf_memcpy_from(data, _rxBuffer, line_term_offs);
    
    // Remove the line terminator from the FIFO buffer
    ringbuf_remove_from_tail(_rxBuffer, 1);
    
    // Re-enable interrupts
    CyExitCriticalSection(state);
    
    return line_term_offs;
}

/*******************************************************************************
* Function Name: comm_putline
********************************************************************************
* Summary:
*  Write a line to the txBuffer. The line terminator COMM_LINE_TERMINATOR
*  will be appended automatically (see comm_driver.h).
*   
* Parameters:
*  data: Pointer to an array of uint8 containing the line to send.
*  count: The number of bytes in the array 'data'.
*
* Return:
*  None.
*
*******************************************************************************/
void comm_putline(uint8 *data, uint8 count)
{
    uint8 state;
    
    // Exit if 'data' is NULL
    if(!data || count <= 0)
        return;
    
    // Wait until there's enough room in the TX buffer
    while(1u) {
        // Prevent interrupts
        state = CyEnterCriticalSection();
        
        // Check if there's enough space free in the TX buffer
        if(ringbuf_bytes_free(_txBuffer) >= count+1) break;
        
        // Re-enable interrupts
        CyExitCriticalSection(state);
    }
    
    // Copy the line into the FIFO buffer
    ringbuf_memcpy_into(_txBuffer, data, count);
    
    // Copy the line terminator into the FIFO buffer
    uint8 line_terminator = COMM_LINE_TERMINATOR;
    ringbuf_memcpy_into(_txBuffer, &line_terminator, 1);
    
    // Re-enable interrupts
    CyExitCriticalSection(state);
}

#ifdef _COMM_DRIVER_MSG_H
/*******************************************************************************
* Function Name: comm_getmsg
********************************************************************************
* Summary:
*  Read a message from the rxBuffer. A message ends with MSG_LAST_BYTE
*  (see comm_driver_msg.h). It may return '0' if a complete message couldn't
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
uint8 comm_getmsg(uint8 *data)
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
            
        // Check if message length is valid (smaller than buffer size)
        if(msg_length >= 100) {
            ringbuf_remove_from_tail(_rxBuffer, 1);
            return 0;
        }
        
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
* Function Name: comm_putmsg
********************************************************************************
* Summary:
*  Write a message to the txBuffer. The message will be padded with the
*  custom structure found in "comm_driver_msg.h".
*   
* Parameters:
*  data: Pointer to an array of uint8 containing the message to send.
*  count: The number of bytes in the array 'data'.
*
* Return:
*  None.
*
*******************************************************************************/
void comm_putmsg(uint8 *data, uint8 count)
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
#endif // _COMM_DRIVER_MSG_H


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
*  first_init: Must only be 'TRUE' right after the call to COMM_Start().
*
* Return:
*  None.
*
*******************************************************************************/
#if USE_USBUART
void _init_cdc(bool first_init)
{
    // To do only on first init or if configurations have changed
    if(first_init || COMM_IsConfigurationChanged()) {
        
        // Wait for USBFS to enumerate
        while (!COMM_GetConfiguration());
        
        // Ensure to clear the CHANGE flag
        COMM_IsConfigurationChanged();
        
        // Initialize the CDC feature
        COMM_CDC_Init();
    }
}
#endif

/*******************************************************************************
* Function Name: _comm_rx_isr
********************************************************************************
* Summary:
*  Copy all available bytes from COMM block into the RX FIFO buffer.
*   
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
void _comm_rx_isr()
{
    // Prevent interrupts
    uint8 state = CyEnterCriticalSection();
    
#if USE_USBUART
    uint16 count = 0;
    
    // Check if USBFS configuration has changed
    _init_cdc(false);
    
    // Check if USBUART has data available
    if (COMM_DataIsReady()) {
        
        // Check that the FIFO buffer has enough free space to receive 
        // all available bytes from COMM block
        if (COMM_GetCount() <= ringbuf_bytes_free(_rxBuffer)) {
            
            // Copy available bytes into the FIFO buffer
            count = COMM_GetAll(_tempBuffer);
            ringbuf_memcpy_into(_rxBuffer, _tempBuffer, count);
        }
    }
#elif USE_UART
    uint32 available_bytes = COMM_SpiUartGetRxBufferSize();
    uint32 byte_read_32 = 0;
    uint8 byte_read_8;
    
    // Check that the FIFO buffer has enough free space to receive 
    // all available bytes from COMM
    if (available_bytes <= ringbuf_bytes_free(_rxBuffer)) {
        
        // Copy available bytes into the FIFO buffer
        for(uint32 i=0; i < available_bytes; i++) {
            byte_read_32 = COMM_SpiUartReadRxData();
            if(byte_read_32 == 0)
                continue;
            byte_read_8 = (uint8)(byte_read_32 & 0xFF);
            ringbuf_memcpy_into(_rxBuffer, &byte_read_8, 1);
        }
    }
#endif
    
    // Re-enable interrupts
    CyExitCriticalSection(state);
}

/*******************************************************************************
* Function Name: _comm_tx_isr
********************************************************************************
* Summary:
*  Try to send everything in TX FIFO buffer into the COMM block
*  (or up to the max available bytes in the COMM block).
*   
* Parameters:
*  None.
*
* Return:
*  None.
*
*******************************************************************************/
void _comm_tx_isr()
{
    uint16 count = 0;
    
    // Prevent interrupts
    uint8 state = CyEnterCriticalSection();
    
#if USE_USBUART
    // Check if there's anything in the TX FIFO buffer or if a Zero Length
    // Packet is required
    if (!ringbuf_is_empty(_txBuffer) || _txZlpRequired) {
        
        // Check if USBFS configuration has changed
        _init_cdc(false);
        
        // Check if USBUART is ready to send data
        if (COMM_CDCIsReady()) {
            
            // Check the amount of bytes in the buffer
            // Can't send more than COMM_TX_MAX_PACKET_SIZE bytes
            count = MIN(ringbuf_bytes_used(_txBuffer), COMM_TX_MAX_PACKET_SIZE);
            
            // Send packet
            ringbuf_memcpy_from(_tempBuffer, _txBuffer, count);
            COMM_PutData(_tempBuffer, count);
            
            // Clear the buffer
            _txZlpRequired = (count == COMM_TX_MAX_PACKET_SIZE);
            _txReject = 0;
        }
        
        // Discard the TX FIFO buffer content if COMM rejects too many times
        else if (++_txReject > TX_MAX_REJECT) {
            ringbuf_reset(_txBuffer);
            _txReject = 0;
        }
        
        // Expect next time
        else {
        }
    }
        
#elif USE_UART
    // Check if there's anything in the TX FIFO buffer
    if (!ringbuf_is_empty(_txBuffer)) {
        
        uint32 uart_bytes_used = COMM_SpiUartGetTxBufferSize();
        
        // Check if COMM has room in its TX buffer
        if (uart_bytes_used == 0) {
            
            // Check the amount of bytes in the buffer
            // Can't send more than COMM_TX_MAX_PACKET_SIZE bytes
            count = MIN(ringbuf_bytes_used(_txBuffer), COMM_TX_MAX_PACKET_SIZE);
            
            // Send packet
            ringbuf_memcpy_from(_tempBuffer, _txBuffer, count);
            COMM_SpiUartPutArray(_tempBuffer, count);
        }
        
        // Expect next time
        else {
        }
    }
#endif

    // Re-enable interrupts
    CyExitCriticalSection(state);
}

/* [] END OF FILE */
