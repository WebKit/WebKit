/*
 * Copyright (C) 2003, 2004, 2005 Filip Pizlo. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/* This file holds constants required by the serial protocol. */

#ifndef FP_TSF_SERIAL_PROTOCOL_H
#define FP_TSF_SERIAL_PROTOCOL_H

#include "tsf_types.h"

/* the length of all magic codes */
#define TSF_SP_MAGIC_LEN            4

/* What follows are the serial protocol command codes. */

/* Indicates that the type map should be reset.  Every valid TSF stream
 * must begin with a TSF_SP_RESET_TYPES command.  A TSF_SP_RESET_TYPES
 * command must be followed by the TSF magic code, defined in the constants
 * TSF_SP_MAGIC_SUFFIX and TSF_SP_MAGIC_SUFFIX_LEN. */
#define TSF_SP_C_RESET_TYPES                ((uint8_t)0)

/* Indicates that a new type definition follows.  The new type is defined
 * to have whatever code comes next.  At least one of these
 * must precede the first TSF_SP_C_DATA command in a stream. */
#define TSF_SP_C_NEW_TYPE                   ((uint8_t)1)

/* Indicates that data follows.  The format of said data is determined
 * by the tsf_buffer object. */
#define TSF_SP_C_DATA                       ((uint8_t)2)

/* Indicates that we're going to change formats.  Note that the
 * serial_in_man doesn't catch this; if you want to allow for zip formats
 * to be changed on the fly, you have to catch this yourself.  The
 * tsf_adaptive_reader does this. */
#define TSF_SP_C_SWITCH_FORMAT              ((uint8_t)'T')

/* If this bit in the command code is set, it indicates that data follows,
 * with "compact framing" (CF). The low bits are then used as the type
 * code. */
#define TSF_SP_C_CF_DATA_BIT                ((uint8_t)128)

/* It is a requirement that any additional serial protocol command codes must
 * be immediately followed by a 32-bit length specifier.  It is then
 * acceptable for input manager implementations to skip over the number of
 * bytes specified by the length specifier. */
 
/* The code that signifies a standard TSF stream.  (This is just a
 * TSF_SP_C_RESET_TYPES followed by a TSF_SP_MAGIC_SUFFIX.)*/
#define TSF_SP_MAGIC_CODE                   "\000TSF"

/* The 'suffix' of the above code; this is the part that will follow a
 * TSF_SP_C_RESET_TYPES command. */
#define TSF_SP_MAGIC_SUFFIX                 "TSF"
#define TSF_SP_MAGIC_SUFFIX_LEN             3

/* Magic codes for ZLib and libbzip2 files.  Note that it is no accident that
 * the first character of these codes has a 'T' - see TSF_SP_C_SWITCH_FORMAT
 * above. */

#define TSF_SP_ZLIB_MAGIC_CODE              "TSFz"

#define TSF_SP_BZIP2_MAGIC_CODE             "TSFb"

#endif


