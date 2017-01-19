/*
 * Copyright (C) 2012-2015 Michael Tuexen
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef DATACHAN_H

#define DATA_CHANNEL_PPID_CONTROL   1
#define DATA_CHANNEL_PPID_DOMSTRING 2
#define DATA_CHANNEL_PPID_BINARY    3

struct rtcweb_datachannel_msg {
  uint8_t  msg_type;
  uint8_t  channel_type;  
  uint16_t flags;
  uint16_t reverse_stream;
  uint16_t reliability_params;
  /* msg_type_data follows */
} SCTP_PACKED;

/* msg_type values: */
#define DATA_CHANNEL_OPEN                     0
#define DATA_CHANNEL_OPEN_RESPONSE            1

/* channel_type values: */
#define DATA_CHANNEL_RELIABLE                 0
#define DATA_CHANNEL_RELIABLE_STREAM          1
#define DATA_CHANNEL_UNRELIABLE               2
#define DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT  3
#define DATA_CHANNEL_PARTIAL_RELIABLE_TIMED   4

/* flags values: */
#define DATA_CHANNEL_FLAG_OUT_OF_ORDER_ALLOWED 0x0001
/* all other bits reserved and should be set to 0 */

/* msg_type_data contains: */
/*
   for DATA_CHANNEL_OPEN:
      a DOMString label for the data channel
   for DATA_CHANNEL_OPEN_RESPONSE:
      a 16-bit value for errors or 0 for no error
*/

#define ERR_DATA_CHANNEL_ALREADY_OPEN   0
#define ERR_DATA_CHANNEL_NONE_AVAILABLE 1

#endif
