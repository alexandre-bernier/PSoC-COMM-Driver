/*******************************************************************************
*
* Custom message structure.
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
* The custom message structure used is as follow:
*    MSG_FIRST_BYTE
*    MSG_LENGTH (from first to last byte)
*    MSG
*    MSG_LAST_BYTE
*
* The MSG_LENGTH should be used to validate the integrity of the message.
*
*******************************************************************************/

#ifndef _COMM_DRIVER_MSG_H
#define _COMM_DRIVER_MSG_H

/*******************************************************************************
* MACROS
*******************************************************************************/
// Header
#define MSG_FIRST_BYTE ((unsigned char)0x01)
/* MSG_LENGTH */

// Message
/* MSG */

// Footer
#define MSG_LAST_BYTE ((unsigned char)'\n')

// Metadata
#define MSG_HEADER_LENGTH ((unsigned char)2)
#define MSG_FOOTER_LENGTH ((unsigned char)1)
#define MSG_STRUCTURE_LENGTH (MSG_HEADER_LENGTH + MSG_FOOTER_LENGTH)
#define MSG_LENGTH_OFFS_FROM_FIRST_BYTE ((unsigned char)1)

#endif // _COMM_DRIVER_H

/* [] END OF FILE */
