/*
 * Copyright (C) 2003, 2004, 2005, 2014, 2015 Filip Pizlo. All rights reserved.
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

#ifndef FP_GPC_H
#define FP_GPC_H

#include "tsf.h"
#include <stdio.h>

/* this enumeration must contain the same instructions in the same order
 * as gpc_spec.in. */
enum gpc_cell {
    
    /* figure this might as well be here for completeness */
    GPC_I_NOP,
    
    /* label instruction.  this is translated into a nop.  but
     * it contains a label 'number' as an operand.  these numbers
     * do not have to be unique.  whenever a label is used, it will be
     * translated into a jump to the nearest label instruction that has
     * that label as its operand, by searching either in the forward or
     * backward direction, depending on whether or not it is a forward
     * or backward jump. */
    GPC_I_LABEL,
    
    /* we wrap up the useful subset of tsf_internal.h's converting
     * copy macros and then add conversion to them.  the operands are
     * as follows:
     * COPY_xTOyz_INCabc <offset>
     * The effect is that the pointer at the top of the stack has
     * offset added to it; in the case of INCDST, this serves as the
     * source, while for INCSRC it serves as the destination. */
    GPC_I_COPY_HTONC_INCDST,
    GPC_I_COPY_HTONS_INCDST,
    GPC_I_COPY_HTONL_INCDST,
    GPC_I_COPY_HTONLL_INCDST,
    GPC_I_COPY_HTONF_INCDST,
    GPC_I_COPY_HTOND_INCDST,
    
    GPC_I_COPY_HTONCHOICE_TO_UI8_INCDST,

    /* the rules:
     * -> if we do not change size or reduce size, we can ignore the
     *    size.
     * -> if we expand, then the signedness of the original determines
     *    whether or not we do sign expansion.
     * -> if expanding floats to ints, the signedness of the target
     *    matters (though I'm not sure why - this is only a conservative
     *    estimate based on looking at the things that gcc does for
     *    float to int conversion).
     */
    GPC_I_COPY_NTOHC_TO_BIT_INCSRC,
    GPC_I_COPY_NTOHC_INCSRC,
    GPC_I_COPY_NTOHC_TO_S_Z_INCSRC,
    GPC_I_COPY_NTOHC_TO_S_E_INCSRC,
    GPC_I_COPY_NTOHC_TO_L_Z_INCSRC,
    GPC_I_COPY_NTOHC_TO_L_E_INCSRC,
    GPC_I_COPY_NTOHC_TO_LL_Z_INCSRC,
    GPC_I_COPY_NTOHC_TO_LL_E_INCSRC,
    GPC_I_COPY_NTOHC_TO_F_Z_INCSRC,
    GPC_I_COPY_NTOHC_TO_F_E_INCSRC,
    GPC_I_COPY_NTOHC_TO_D_Z_INCSRC,
    GPC_I_COPY_NTOHC_TO_D_E_INCSRC,

    GPC_I_COPY_NTOHS_TO_BIT_INCSRC,
    GPC_I_COPY_NTOHS_TO_C_INCSRC,
    GPC_I_COPY_NTOHS_INCSRC,
    GPC_I_COPY_NTOHS_TO_L_Z_INCSRC,
    GPC_I_COPY_NTOHS_TO_L_E_INCSRC,
    GPC_I_COPY_NTOHS_TO_LL_Z_INCSRC,
    GPC_I_COPY_NTOHS_TO_LL_E_INCSRC,
    GPC_I_COPY_NTOHS_TO_F_Z_INCSRC,
    GPC_I_COPY_NTOHS_TO_F_E_INCSRC,
    GPC_I_COPY_NTOHS_TO_D_Z_INCSRC,
    GPC_I_COPY_NTOHS_TO_D_E_INCSRC,
    
    GPC_I_COPY_NTOHL_TO_BIT_INCSRC,
    GPC_I_COPY_NTOHL_TO_C_INCSRC,
    GPC_I_COPY_NTOHL_TO_S_INCSRC,
    GPC_I_COPY_NTOHL_INCSRC,
    GPC_I_COPY_NTOHL_TO_LL_Z_INCSRC,
    GPC_I_COPY_NTOHL_TO_LL_E_INCSRC,
    GPC_I_COPY_NTOHL_TO_F_Z_INCSRC,
    GPC_I_COPY_NTOHL_TO_F_E_INCSRC,
    GPC_I_COPY_NTOHL_TO_D_Z_INCSRC,
    GPC_I_COPY_NTOHL_TO_D_E_INCSRC,
    
    GPC_I_COPY_NTOHLL_TO_BIT_INCSRC,
    GPC_I_COPY_NTOHLL_TO_C_INCSRC,
    GPC_I_COPY_NTOHLL_TO_S_INCSRC,
    GPC_I_COPY_NTOHLL_TO_L_INCSRC,
    GPC_I_COPY_NTOHLL_INCSRC,
    GPC_I_COPY_NTOHLL_TO_F_Z_INCSRC,
    GPC_I_COPY_NTOHLL_TO_F_E_INCSRC,
    GPC_I_COPY_NTOHLL_TO_D_Z_INCSRC,
    GPC_I_COPY_NTOHLL_TO_D_E_INCSRC,
    
    GPC_I_COPY_NTOHF_TO_BIT_INCSRC,
    GPC_I_COPY_NTOHF_TO_C_Z_INCSRC,
    GPC_I_COPY_NTOHF_TO_C_E_INCSRC,
    GPC_I_COPY_NTOHF_TO_S_Z_INCSRC,
    GPC_I_COPY_NTOHF_TO_S_E_INCSRC,
    GPC_I_COPY_NTOHF_TO_L_Z_INCSRC,
    GPC_I_COPY_NTOHF_TO_L_E_INCSRC,
    GPC_I_COPY_NTOHF_TO_LL_Z_INCSRC,
    GPC_I_COPY_NTOHF_TO_LL_E_INCSRC,
    GPC_I_COPY_NTOHF_INCSRC,
    GPC_I_COPY_NTOHF_TO_D_INCSRC,
    
    GPC_I_COPY_NTOHD_TO_BIT_INCSRC,
    GPC_I_COPY_NTOHD_TO_C_Z_INCSRC,
    GPC_I_COPY_NTOHD_TO_C_E_INCSRC,
    GPC_I_COPY_NTOHD_TO_S_Z_INCSRC,
    GPC_I_COPY_NTOHD_TO_S_E_INCSRC,
    GPC_I_COPY_NTOHD_TO_L_Z_INCSRC,
    GPC_I_COPY_NTOHD_TO_L_E_INCSRC,
    GPC_I_COPY_NTOHD_TO_LL_Z_INCSRC,
    GPC_I_COPY_NTOHD_TO_LL_E_INCSRC,
    GPC_I_COPY_NTOHD_TO_F_INCSRC,
    GPC_I_COPY_NTOHD_INCSRC,
    
    /* below are convert and copy routines between two pointers and
     * offsets; these are designed for primitives.  the data is copied
     * from the given offset from the topmost pointer to the given offset
     * from the second-to-topmost pointer.  call these guys like so:
     * GPC_I_COPY_x_TO_y <dest offset> <source offset> */
    GPC_I_COPY_BIT, /* makes non-zero values one - good for converting to and
		       from bits */
    GPC_I_COPY_BIT_TO_S,
    GPC_I_COPY_BIT_TO_L,
    GPC_I_COPY_BIT_TO_LL,
    GPC_I_COPY_BIT_TO_F,
    GPC_I_COPY_BIT_TO_D,
    
    GPC_I_COPY_C,
    GPC_I_COPY_C_TO_S_Z,
    GPC_I_COPY_C_TO_S_E,
    GPC_I_COPY_C_TO_L_Z,
    GPC_I_COPY_C_TO_L_E,
    GPC_I_COPY_C_TO_LL_Z,
    GPC_I_COPY_C_TO_LL_E,
    GPC_I_COPY_C_TO_F_Z,
    GPC_I_COPY_C_TO_F_E,
    GPC_I_COPY_C_TO_D_Z,
    GPC_I_COPY_C_TO_D_E,

    GPC_I_COPY_S_TO_BIT,
    GPC_I_COPY_S_TO_C,
    GPC_I_COPY_S,
    GPC_I_COPY_S_TO_L_Z,
    GPC_I_COPY_S_TO_L_E,
    GPC_I_COPY_S_TO_LL_Z,
    GPC_I_COPY_S_TO_LL_E,
    GPC_I_COPY_S_TO_F_Z,
    GPC_I_COPY_S_TO_F_E,
    GPC_I_COPY_S_TO_D_Z,
    GPC_I_COPY_S_TO_D_E,
    
    GPC_I_COPY_L_TO_BIT,
    GPC_I_COPY_L_TO_C,
    GPC_I_COPY_L_TO_S,
    GPC_I_COPY_L,
    GPC_I_COPY_L_TO_LL_Z,
    GPC_I_COPY_L_TO_LL_E,
    GPC_I_COPY_L_TO_F_Z,
    GPC_I_COPY_L_TO_F_E,
    GPC_I_COPY_L_TO_D_Z,
    GPC_I_COPY_L_TO_D_E,
    
    GPC_I_COPY_LL_TO_BIT,
    GPC_I_COPY_LL_TO_C,
    GPC_I_COPY_LL_TO_S,
    GPC_I_COPY_LL_TO_L,
    GPC_I_COPY_LL,
    GPC_I_COPY_LL_TO_F_Z,
    GPC_I_COPY_LL_TO_F_E,
    GPC_I_COPY_LL_TO_D_Z,
    GPC_I_COPY_LL_TO_D_E,
    
    GPC_I_COPY_F_TO_BIT,
    GPC_I_COPY_F_TO_C_Z,
    GPC_I_COPY_F_TO_C_E,
    GPC_I_COPY_F_TO_S_Z,
    GPC_I_COPY_F_TO_S_E,
    GPC_I_COPY_F_TO_L_Z,
    GPC_I_COPY_F_TO_L_E,
    GPC_I_COPY_F_TO_LL_Z,
    GPC_I_COPY_F_TO_LL_E,
    GPC_I_COPY_F,
    GPC_I_COPY_F_TO_D,
    
    GPC_I_COPY_D_TO_BIT,
    GPC_I_COPY_D_TO_C_Z,
    GPC_I_COPY_D_TO_C_E,
    GPC_I_COPY_D_TO_S_Z,
    GPC_I_COPY_D_TO_S_E,
    GPC_I_COPY_D_TO_L_Z,
    GPC_I_COPY_D_TO_L_E,
    GPC_I_COPY_D_TO_LL_Z,
    GPC_I_COPY_D_TO_LL_E,
    GPC_I_COPY_D_TO_F,
    GPC_I_COPY_D,
    
    /* note that there are no 64-bit set operations.  this is the
     * result of cells possibly being 32-bit instead of 64-bit. */

    /* set operations.  these take a constant off the stream and
     * shove it into the requested offset from the ptr
     * on top of the stack.  use it like so:
     * GPC_I_SET_XX <dest offset> <constant> */
    GPC_I_SET_C,
    GPC_I_SET_S,
    GPC_I_SET_L,
    
    /* set operations.  these take a constant off the stream and
     * shove it into the requested offset from the ptr that is
     * second from top of the stack.  use it like so:
     * GPC_I_SET_2ND_XX <dest offset> <constant> */
    GPC_I_SET_2ND_C,
    GPC_I_SET_2ND_S,
    GPC_I_SET_2ND_L,
    
    /* note that the read instructions below all increment the
     * current buffer pointer register. */
    
    /* do a bitvector read.  the tsf_native_bitvector_t structure is
     * at the given offset from the topmost pointer.  call this like so:
     * GPC_I_BITVECTOR_READ <offset> */
    GPC_I_BITVECTOR_READ,
    
    /* bitvector read that converts the bitvector to something other than
     * a bitvector */
    GPC_I_BITVECTOR_READ_TO_C,
    GPC_I_BITVECTOR_READ_TO_S,
    GPC_I_BITVECTOR_READ_TO_L,
    GPC_I_BITVECTOR_READ_TO_LL,
    GPC_I_BITVECTOR_READ_TO_F,
    GPC_I_BITVECTOR_READ_TO_D,
    
    /* bitvector read that converts something other than a bitvector into
     * a bitvector */
    GPC_I_BITVECTOR_READ_FROM_C,
    GPC_I_BITVECTOR_READ_FROM_S,
    GPC_I_BITVECTOR_READ_FROM_L,
    GPC_I_BITVECTOR_READ_FROM_LL,
    GPC_I_BITVECTOR_READ_FROM_F,
    GPC_I_BITVECTOR_READ_FROM_D,
    
    GPC_I_BITVECTOR_READ_FROM_TSF_INTEGER_ARRAY,
    GPC_I_BITVECTOR_READ_FROM_TSF_LONG_ARRAY,

    /* do a bitvector write.  the tsf_native_bitvector_t structure is at
     * the given offset from the topmost pointer.  call this like so:
     * GPC_I_BITVECTOR_WRITE <offset> */
    GPC_I_BITVECTOR_WRITE,
    
    /* do a byte array read.  the tsf_native_array_t structure is at the
     * given offset from the topmost pointer.  the length is multiplied
     * by the given multiplier to obtain the size of the array in bytes.
     * call this like so:
     * GPC_I_BYTE_ARRAY_READ <offset> <multiplier> */
    GPC_I_BYTE_ARRAY_READ,
    
    /* do a byte array write.  the tsf_native_array_t structure is at the
     * given offset from the topmost pointer.  the length is multiplied
     * by the given multiplier to obtain the size of the array in bytes.
     * call this like so:
     * GPC_I_BYTE_ARRAY_WRITE <offset> <multiplier> */
    GPC_I_BYTE_ARRAY_WRITE,
    
    /* What follows are instructions for supporting TSF integers. See the
     * comment above the declaration of TSF_TK_INTEGER in tsf.h. */
    
    /* skip a TSF integer. */
    GPC_I_TSF_INTEGER_SKIP,
    GPC_I_TSF_UNSIGNED_SKIP,
    GPC_I_TSF_LONG_SKIP,
    
    /* read a TSF integer and convert to a variety of things. */
    GPC_I_TSF_INTEGER_READ_TO_BIT,
    GPC_I_TSF_INTEGER_READ_TO_C,
    GPC_I_TSF_INTEGER_READ_TO_S,
    GPC_I_TSF_INTEGER_READ,
    GPC_I_TSF_INTEGER_READ_TO_LL,
    GPC_I_TSF_INTEGER_READ_TO_F,
    GPC_I_TSF_INTEGER_READ_TO_D,
    GPC_I_TSF_UNSIGNED_READ_SUB1, /* reads onto the stack, not into memory. */
    GPC_I_TSF_LONG_READ_TO_BIT,
    GPC_I_TSF_LONG_READ_TO_C,
    GPC_I_TSF_LONG_READ_TO_S,
    GPC_I_TSF_LONG_READ_TO_L,
    GPC_I_TSF_LONG_READ,
    GPC_I_TSF_LONG_READ_TO_F,
    GPC_I_TSF_LONG_READ_TO_D,
    
    /* increment the size register for a TSF integer. */
    GPC_I_TSF_INTEGER_SIZE,
    GPC_I_TSF_UNSIGNED_PLUS1_SIZE,
    GPC_I_TSF_LONG_SIZE,
    
    /* write a TSF integer. */
    GPC_I_TSF_INTEGER_WRITE,
    GPC_I_TSF_UNSIGNED_PLUS1_WRITE,
    GPC_I_TSF_LONG_WRITE,
    
    /* set the string to the empty string.  the char* is at the given
     * offset from the topmost pointer. */
    GPC_I_STRING_SETEMPTY,
    
    /* do a string read.  the char* is at the given offset from the
     * topmost pointer. */
    GPC_I_STRING_READ,
    
    /* do a string skip. */
    GPC_I_STRING_SKIP,
    
    /* calculate the size for a string.  the char* is at the given offset
     * from the topmost pointer. */
    GPC_I_STRING_SIZE,
    
    /* do a string write.  the char* is at the given offset from the
     * topmost pointer. */
    GPC_I_STRING_WRITE,
    
    /* do a string copy.  the second to topmost pointer plus the 
     * destination offset points to the destination char*, while the
     * topmost pointer plus source offset points to the source char*.
     * call this like so:
     * GPC_I_STRING_COPY <dest offset> <src offset> */
    GPC_I_STRING_COPY,

    /* set the buffer to a buffer just containing void.  the tsf_buffer_t*
     * is at the given offset frm the topmost pointer. */
    GPC_I_ANY_SETEMPTY,
    
    /* do an any read.  the tsf_buffer_t* is at the given offset from the
     * topmost pointer. */
    GPC_I_ANY_READ,
    
    /* do an any skip. */
    GPC_I_ANY_SKIP,
    
    /* calculate the size of an any.  the tsf_buffer_t* is at the given
     * offset frm the topmost pointer. */
    GPC_I_ANY_SIZE,
    
    /* do an any write.  the tsf_buffer_t* is at the given offset from the
     * topmost pointer. */
    GPC_I_ANY_WRITE,
    
    /* do an any copy.  the second to topmost pointer plus the destination
     * offset points to the destination tsf_buffer_t*, while the topmost
     * pointer plus source offset points to the source tsf_buffer_t*.  call
     * this like so:
     * GPC_I_ANY_COPY <dest offset> <src offset> */
    GPC_I_ANY_COPY,
    
    /* do a memcpy from one data structure to another.  the
     * destination is second to topmost, while the source is topmost.
     * the operands are: offset from destination, offset from source, and
     * the size. */
    GPC_I_MEMCPY,
    
    /* copy an array.  the second to topmost pointer plus the
     * destination offset points to the destination tsf_native_array_t,
     * while the topmost pointer plus source offset points to the source
     * tsf_native_array_t.  the other operand is the multiplier that
     * specifies the number of bytes for each array element. call this
     * like so:
     * GPC_I_COPY_ARRAY <dest offset> <src offset> <multiplier> */
    GPC_I_COPY_ARRAY,
    
    /* copy a bitvector.  the second to topmost pointer plus the
     * destination offset points to the destination tsf_native_bitvector_t,
     * while the topmost pointer plus source offset points to the source
     * tsf_native_bitvector_t. call this like so:
     * GPC_I_COPY_ARRAY <dest offset> <src offset> */
    GPC_I_COPY_BITVECTOR,
    
    /* copy between bitvectors and other types of arrays */
    GPC_I_COPY_BITVECTOR_FROM_C,
    GPC_I_COPY_BITVECTOR_FROM_S,
    GPC_I_COPY_BITVECTOR_FROM_L,
    GPC_I_COPY_BITVECTOR_FROM_LL,
    GPC_I_COPY_BITVECTOR_FROM_F,
    GPC_I_COPY_BITVECTOR_FROM_D,
    GPC_I_COPY_BITVECTOR_TO_C,
    GPC_I_COPY_BITVECTOR_TO_S,
    GPC_I_COPY_BITVECTOR_TO_L,
    GPC_I_COPY_BITVECTOR_TO_LL,
    GPC_I_COPY_BITVECTOR_TO_F,
    GPC_I_COPY_BITVECTOR_TO_D,
    
    /* read a 8-bit value and place it on the stack.  if the value was
     * 255, it is instead converted to UINT32_MAX. */
    GPC_I_READC_TO_CHOICE,

    /* read a 32-bit value and place it at the top of the stack. */
    GPC_I_READL,
    
    /* do an array length write.  operand is the offset from the
     * topmost pointer to the 32-bit array length.  relevant
     * bounds checks are done. */
    GPC_I_ARRAY_LEN_WRITE_FIELD,
    
    /* do an array length read.  operand is the offset from the
     * topmost pointer to the 32-bit array length.  relevant
     * bounds checks are done. */
    GPC_I_ARRAY_LEN_READ_FIELD,
    
    /* do an array length read.  the array length is pushed onto
     * the stack.  relevant bounds checks are done. */
    GPC_I_ARRAY_LEN_READ_LOCAL,
    
    /* do an array length read.  discard the array length. */
    GPC_I_ARRAY_LEN_SKIP,
    
    /* if the 8-bit value pointed to by the specified offset from
     * the topmost pointer is non-zero, set set specified bit in
     * the current buffer location.  otherwise reset it.  the
     * buffer pointer is not advanced.   you call this thing as
     * follows:
     * GPC_I_BIT_MASK_WRITE <bit index> <offset> */
    GPC_I_BIT_MASK_WRITE,
    
    /* mask out the requested bit from the current buffer location
     * without advancing the pointer.  if set, then set the 8-bit
     * value pointed to by the specified offset from the topmost
     * pointer to 1; otherwise, set it to 0.  this instruction is
     * used as follows:
     * GPC_I_BIT_MASK_READ <bit index> <offset> */
    GPC_I_BIT_MASK_READ,
    
    /* same but store it in something other than a byte. */
    GPC_I_BIT_MASK_READ_TO_S,
    GPC_I_BIT_MASK_READ_TO_L,
    GPC_I_BIT_MASK_READ_TO_LL,
    GPC_I_BIT_MASK_READ_TO_F,
    GPC_I_BIT_MASK_READ_TO_D,
    
    /* write a bit stored in a byte.  the idea is that the value
     * is 'normalized', or converted to either a 1 or 0 prior to
     * being placed into the buffer.  the only argument is the
     * offset. */
    GPC_I_BIT_WRITE,
    
    /* read a bit store it in a byte.  the idea is that the value is
     * 'normalized', or converted to either a 1 or 0 prior to being
     * placed into the user's data structure.  the only argument is
     * the offset. */
    GPC_I_BIT_READ,
    
    /* read a bit store it in something other than a byte. */
    GPC_I_BIT_READ_TO_S,
    GPC_I_BIT_READ_TO_L,
    GPC_I_BIT_READ_TO_LL,
    GPC_I_BIT_READ_TO_F,
    GPC_I_BIT_READ_TO_D,
    
    /* skip some number of bytes. operand is the number of bytes. */
    GPC_I_SKIP,
    
    /* skip an array; the operands are the size of each element, and
     * an offset from the topmost pointer.  the array length is found
     * at that offset.  call this like so:
     * GPC_I_ARRAY_BC_AND_SKIP_FIELD <offset> <multiplier> */
    GPC_I_ARRAY_BC_AND_SKIP_FIELD,
    
    /* skip an array; this instruction reads the array length, does
     * relevant bounds checks, and skips the array.  all it needs to
     * know is the number of bytes required by each array element.
     * call this like so:
     * GPC_I_ARRAY_SKIP <constant> */
    GPC_I_ARRAY_SKIP,
    
    /* skip a bitvector; the operand is the offset from the topmost
     * pointer where the number of bits can be found. */
    GPC_I_BITVECTOR_BC_AND_SKIP_FIELD,
    
    /* skip a bitvector; this instruction reads the array length, does
     * relevant bounds checks, and skips the bitvector.  this
     * instruction takes no operands. */
    GPC_I_BITVECTOR_SKIP,
    
    /* increment the size register by the specified number of bytes. */
    GPC_I_INC_SIZE,
    
    /* examine the array length at the given offset from the topmost
     * pointer, and increment the size register by the number of
     * bytes that this array length would occupy in the stream. */
    GPC_I_INC_SIZE_ARRAY_LEN,
    
    /* increment the size register by the size of the array, where the
     * second operand is the size in bytes of each element and the
     * first operand is an offset to a tsf_native_array_t structure. */
    GPC_I_INC_SIZE_ARRAY,
    
    /* increment the size register to accomodate the number of bits
     * as specified by the 32-bit value at the given offset from the
     * topmost pointer. */
    GPC_I_INC_SIZE_BITVECTOR,
    
    /* bounds check.  bounds check failure leads to termination of
     * the interpreter with an error.  operand is the number of
     * bytes we expect to be able to see. */
    GPC_I_BC,
    
    /* array bounds check.  bounds check failure leads to termination
     * of the interpreter with an error.  operands are a size
     * constant and the offset from the topmost pointer to the
     * length of the array (32-bit).  if constant*array_len bytes
     * are not available, the bounds check fails.  call this like so:
     * GPC_I_ARRAY_BC_FIELD <offset> <multiplier> */
    GPC_I_ARRAY_BC_FIELD,
    
    /* array bounds check.  bounds check failure leads to termination
     * of the interpreter with an error.  operand is a size
     * constant.  the array length is the topmost stack element.  if
     * constant*array_len bytes are not available, the bounds check
     * fails. */
    GPC_I_ARRAY_BC_LOCAL,
    
    /* bitvector bounds check.  bounds check failure leads to termination
     * of the interpreter with an error.  the operand is the offset
     * from the topmost pointer to a 32-bit number of bits.  if
     * (num_bits+7)>>3 bytes are not available, the bounds check fails.
     */
    GPC_I_BITVECTOR_BC_FIELD,
    
    /* bitvector bounds check.  bounds check failure leads to termination
     * of the interpreter with an error.  the number of bits is the
     * topmost element on the stack.  if (num_bits+7)>>3 bytes are not
     * available, the bounds check fails.
     */
    GPC_I_BITVECTOR_BC_LOCAL,
    
    /* malloc a buffer.  will be automatically deallocated upon
     * interpreter failure or if it is not returned.  if allocation
     * fails, the interpreter fails.  the allocation size is determined
     * by the size register.  performing this allocation automatically sets
     * the end register so that bounds checks can be performed. */
    GPC_I_MALLOC_BUF,
    
    /* pop the buffer off the stack.  the current pointer, the buffer
     * pointer, the end pointer, the keep bit, and the types pointer are
     * all set.  however, size is not set (so you cannot do a MALLOC_BUF
     * after this call). */
    GPC_I_POP_BUF,
    
    /* destroy a buffer. */
    GPC_I_DESTROY_BUFFER,
    
    /* create an output map. */
    GPC_I_MAKE_OUT_MAP,
    
    /* examine the second-to-top-most pointer.  if it is NULL, then perform a
     * region alloc and replace the second-to-top-most pointer with the result.
     * if the region pointer was NULL, then it is replaced with the result as
     * well, and the region is put into keep mode.  the only operand is the
     * size. */
    GPC_I_ALLOC_ROOT_MAYBE_2ND,
    
    /* allocate and push.  this is a region alloc.  failure here
     * leads to termination of the interpreter.  the operand is
     * the size. */
    GPC_I_ALLOC,
    
    /* allocate and push array.  this is a region alloc.  failure here
     * leads to termination of the interpreter.  the operands are
     * the element size and the offset from the topmost pointer to
     * the tsf_native_array_t.  tsf_native_array_t::len should be set
     * already, and is used to determine how many bytes to alloc.
     * tsf_native_array_t::data will be set to the same thing that
     * gets pushed -- that is, the newly allocated array.  call this like
     * so: 
     * GPC_I_ALLOC_ARRAY <offset> <multiplier> */
    GPC_I_ALLOC_ARRAY,
    
    /* allocate and push array.  this is a region alloc.  failure here
     * leads to termination of the interpreter.  the operands are
     * the element size and the offset from the second to topmost pointer to
     * the tsf_native_array_t.  tsf_native_array_t::len should be set
     * already, and is used to determine how many bytes to alloc.
     * tsf_native_array_t::data will be set to the same thing that
     * gets pushed -- that is, the newly allocated array.  call this like
     * so: 
     * GPC_I_ALLOC_ARRAY <offset> <multiplier> */
    GPC_I_ALLOC_ARRAY_2ND,

    /* call a function of the form void(*)(void*), passing it the given
     * offset from the topmost pointer.  call it like so:
     * GPC_I_CALL <offset> <function pointer> */
    GPC_I_CALL,

    /* add callback to current region.  callback must be of the form
     * void(*)(void*).  the argument is computed as an offset from the
     * topmost pointer.  call it like so:
     * GPC_I_ADD_CBACK <offset> <function pointer> */
    GPC_I_ADD_CBACK,
    
    /* call free with the top of the stack. */
    GPC_I_FREE_IMMEDIATE,
    
    /* call free with the the pointer loaded from stack top + offset. */
    GPC_I_FREE,
    
    /* store the topmost pointer at the given offset from the second-to-
     * topmost pointer. */
    GPC_I_STORE_PTR,
    
    /* store the topmost pointer at the given offset from the third-to-
     * topmost pointer. */
    GPC_I_STORE_PTR_2ND,
    
    /* store the topmost value at the given offset from the second-to-
     * topmost pointer. */
    GPC_I_STORE_VAL,
    
    /* push a pointer onto the stack.  operand is an
     * offset from current top-most pointer where this pointer
     * can be found. */
    GPC_I_PUSH_PTR,
    
    /* push a pointer onto the stack.  operand is an
     * offset from current second-to-top-most pointer where this pointer
     * can be found. */
    GPC_I_PUSH_PTR_2ND,
    
    /* push a pointer onto the stack.  operand is an
     * offset from current third-to-top-most pointer where this pointer
     * can be found. */
    GPC_I_PUSH_PTR_3RD,
    
    /* push a 32-bit value onto the stack.  operand is an
     * offset from current top-most pointer where this
     * value can be found. */
    GPC_I_PUSH_VAL,

    /* dup the second-to-topmost pointer */
    GPC_I_DUP_PTR_2ND,
    
    /* pop a pointer or value from the stack. */
    GPC_I_POP,
    
    /* pop two things off the stack. */
    GPC_I_TWO_POP,
    
    /* pop three things off the stack. */
    GPC_I_THREE_POP,
    
    /* first, save the topmost pointer in a register.  then, add the
     * 32-bit value stored at an offset from the second to
     * topmost pointer multiplied by the supplied constant to the topmost
     * pointer.  finally, push the saved pointer onto the stack.
     * this weird instruction looks like this:
     * GPC_I_MULADD_PTR <offset> <constant>
     * What the heck does this instruction really do?  It sets you up
     * for a pointer loop! */
    GPC_I_REPUSH_MULADD_PTR,
    
    /* first, save the topmost pointer in a register.  then, add the
     * 32-bit value stored at an offset from the third to
     * topmost pointer multiplied by the supplied constant to the topmost
     * pointer.  finally, push the saved pointer onto the stack.
     * this weird instruction looks like this:
     * GPC_I_MULADD_PTR <offset> <constant> */
    GPC_I_REPUSH_MULADD_PTR_2ND,
    
    /* compare (less-than) the topmost pointer to the next-to-topmost
     * pointer.  if the comparison fails, jump to the given label.
     * this is a forward jump. */
    GPC_I_COMPFAILJUMP,
    
    /* compare (less-than) the second to topmost pointer to the third
     * to topmost pointer.  if the comparison fails, jump to the given
     * label.  this is a forward jump. */
    GPC_I_COMPFAILJUMP_2ND,
    
    /* add the given constant to the topmost pointer, compare it
     * (less-than) to the next-to-topmost pointer, and if the
     * comparison is true, jump to the given label.  note that
     * this is a backward branch, so the label is looked for
     * backwards, not forwards.  here's how to call it:
     * GPC_I_ADDCOMPJUMP <constant> <label> */
    GPC_I_ADDCOMPJUMP,
    
    /* add the destination constant to the second-to-topmost pointer,
     * add the source constant to the topmost pointer, compare the
     * second-to-topmost pointer to the third-to-topmost pointer, and
     * if the comparison is true, jump to the given label.  note that
     * this is a backward branch, so the label is looked for backwards,
     * not forwards.  here's how to call it:
     * GPC_I_TWO_ADDCOMPJUMP <dest const> <src const> <label> */
    GPC_I_TWO_ADDCOMPJUMP,
    
    /* if the topmost value on the stack is zero, jump forward to
     * the given label. */
    GPC_I_ZEROJUMP,
    
    /* decrement the value at the top of the stack.  if it is non-zero,
     * jump backward to the given label. */
    GPC_I_DECCOMPJUMP,
    
    /* compares the topmost element on the stack to the supplied constant.
     * if the topmost element is UINT32_MAX, nothing happens.  if the
     * topmost element is less than the supplied constant, nothing happens.
     * otherwise, it is an error, and the interpreter fails out. */
    GPC_I_CHECKCHOICE,
    
    /* pop the 32-bit value at the top of the stack
     * and use it as an index into the given table, and then place
     * the looked up value into the location pointed to by the topmost
     * pointer plus the offset.  here's the way you call this beast:
     * GPC_I_TABLESET_LOCAL_TO_FIELD <offset> <num table entries>
     *                               <entry 0> <entry 1> ...
     * If the value at the top of the stack is beyond the bounds of
     * this table, then it is an error and the interpreter is
     * terminated.  If the value is UINT32_MAX, then the location is
     * set to UINT32_MAX. */
    GPC_I_TABLESET_LOCAL_TO_FIELD,
    
    /* read the 32-bit value at the given source offset from the
     * topmost pointer and use it as an index into the given table, and
     * then place the looked up value into the location pointed to by
     * the destination offset plus the second to topmost pointer.
     * call this like so:
     * GPC_I_TABLESET_FIELD_TO_FIELD <dest offset> <source offset>
     *                               <num table entries> <entry 0> ...
     * If the value at the given offset from the topmost pointer is
     * beyond the bounds of this table, then it is an error and the
     * interpreter is terminated.  If the value is UINT32_MAX, then the
     * location is set to UINT32_MAX. */
    GPC_I_TABLESET_FIELD_TO_FIELD,
    
    /* retrieve the 32-bit value from the offset off of the topmost
     * pointer and use it as an index into the given table, and then jump
     * to the location labeled by the looked up value.  You call this
     * beast like so:
     * GPC_I_TABLEJUMP_FIELD <offset> <num table entries> <entry 0> ...
     * If the value at the given offset from the topmost pointer is
     * beyond the bounds of this table, then it is an error and the
     * interpreter is terminated.  If the value is UINT32_MAX, then no
     * jump occurs. */
    GPC_I_TABLEJUMP_FIELD,
    
    /* pop the 32-bit value at the top of the stack and
     * use it as an index into the given table, and then jump to the
     * location labeled by the looked up value.  You call this
     * beast like so:
     * GPC_I_TABLEJUMP <num table entries> <entry 0> ...
     * If the value at the topmost pointer is beyond the bounds of
     * this table, then it is an error and the interpreter is
     * terminated.  If the value is UINT32_MAX, then no jump occurs. */
    GPC_I_TABLEJUMP_LOCAL,
    
    /* jump to the given label unconditionally.  this is a forward jump */
    GPC_I_JUMP,
    
    /* returns one. */
    GPC_I_RETURN_ONE,
    
    /* returns the top pointer without clearing the region. */
    GPC_I_RETURN_TOP_WITH_REGION,
    
    /* assumes that the buffer that the client sees is in the stack slot just
     * below the top one. initializes it with the resulting data buffer and sets
     * the types to the type_in_man empty singleton. */
    GPC_I_RETURN_ONE_INIT_BUF_WITH_EMPTY_TYPES,
    
    /* assumes that the buffer that the client sees is in the stack slot just
     * below the top one. initializes it with the resulting data buffer and the
     * inverse of the current running out_map. */
    GPC_I_RETURN_ONE_INIT_BUF_WITH_TYPES_FROM_OUT_MAP,
    
    /* this is not a real instruction.  don't use it.  but make sure that
     * it is always at the end of this enumeration. */
    GPC_I_LAST
};

typedef uintptr_t gpc_cell_t;

/* a list of cell pointers */
struct gpc_cell_list {
    gpc_cell_t *cell;
    struct gpc_cell_list *next;
};

typedef struct gpc_cell_list gpc_cell_list_t;

/* contains GPC_I_LABEL instructions. */
struct gpc_proto {
    uint32_t num_args;
    gpc_cell_t *stream;
    uint32_t size;  /* number of cells allocated for stream.  this is also
                     * the number of cells that have code in them. */
};

/* does not contain GPC_I_LABEL instructions. */
struct gpc_intable {
    uint32_t num_args;
    gpc_cell_t *stream;
    uint32_t size;  /* number of cells allocated for stream.  this is also
                     * the number of cells that have code in them. */

    tsf_st_table *targets;

    tsf_st_table *stack_heights;
    intptr_t max_height;
};

typedef struct gpc_intable gpc_intable_t;

/* threaded code */
struct gpc_threaded {
    uint32_t num_args;
    gpc_cell_t *stream;
    uint32_t size;  /* number of cells allocated for stream.  this is also
                     * the number of cells that have code in them. */
    intptr_t max_height;
};

typedef struct gpc_threaded gpc_threaded_t;

/* the type for a function that implements running a gpc_program. */
typedef uintptr_t (*gpc_program_runner_t)(void *payload,
                                          void *region,
                                          gpc_cell_t *args);

/* the type of a function that desrtroys the payload of a gpc_program. */
typedef void (*gpc_program_destroyer_t)(void *payload);

/* this is what you use to run a GPC program.  delegates to one
 * of the execution engines. */
struct gpc_program {
    void *payload;
    gpc_program_runner_t runner;
    gpc_program_destroyer_t destroyer;
};

/* returns the size, in the number of slots, that this instruction
 * occupies.  note that some instructions are variable length.  for
 * these instructions, this macro returns UINT32_MAX.
 * FIXME: this macro is wrong.  the nh copy opcodes take one
 * operand, not two. */
uint32_t gpc_instruction_static_size(gpc_cell_t inst);

/* returns the effect on the stack from executing this instruction.
 * if 0 is returned, that means that the stack height is unaffected.  if
 * a positive number is returned, then the stack will grow by that amount.
 * if a negative number is returned, then the stack will shrink by that
 * amount. */
int32_t gpc_instruction_stack_effects(gpc_cell_t inst);

/* return a string with the name of the instruction. */
const char *gpc_instruction_to_string(gpc_cell_t inst);

/* returns the size, in the number of slots, that this instruction
 * occupies.  note that some instructions are variable length.  for
 * this reason, this macro requires that you pass in a pointer to
 * an instruction in the actual stream, so that the instruction
 * stream can be properly queried. */
uint32_t gpc_instruction_size(gpc_cell_t *instptr);

/* function to execute for jump targets within instructions */
typedef tsf_bool_t (*gpc_target_cback_t)(gpc_cell_t *cell,void *arg);

/* executes the given jump target callback for all back branches in
 * the given instruction. */
tsf_bool_t gpc_instruction_for_all_back_branches(gpc_cell_t *inst,
                                                 gpc_target_cback_t cback,
                                                 void *arg);

/* executes the given jump target callback for all forward branches in
 * the given instruction. */
tsf_bool_t gpc_instruction_for_all_forward_branches(gpc_cell_t *inst,
                                                    gpc_target_cback_t cback,
                                                    void *arg);

/* executes the given jump target callback for all branches in
 * the given instruction. */
tsf_bool_t gpc_instruction_for_all_branches(gpc_cell_t *inst,
                                            gpc_target_cback_t cback,
                                            void *arg);

/* returns tsf_true if the given instruction causes the program to
 * return. */
tsf_bool_t gpc_instruction_is_return(gpc_cell_t inst);

/* returns tsf_true if the given instruction may allow control flow
 * to continue to the instruction directly after it.  returns tsf_false
 * for instructions that do unconditional jumps and that terminate the
 * program, or for anything else that will never lead to control flow
 * continuing to the next instruction. */
tsf_bool_t gpc_instruction_is_continuous(gpc_cell_t inst);

/* create a prototype.  this is just the beginning of the creation
 * process.  it is then your responsibility to drop in the proper
 * opcodes and operands. */
gpc_proto_t *gpc_proto_create(uint32_t num_args);

/* destroy a prototype. */
void gpc_proto_destroy(gpc_proto_t *proto);

/* get the number of arguments */
#define gpc_proto_get_num_args(proto) \
            ((proto)->num_args)

/* reserve the specified number of additional slots. */
tsf_bool_t gpc_proto_reserve(gpc_proto_t *proto,
                             uint32_t num_slots);

/* append an instruction.  you are required to supply the correct
 * number of operands. */
tsf_bool_t gpc_proto_append(gpc_proto_t *proto,
                            gpc_cell_t inst,
                            ...);

/* append a tableset_local_to_field instruction */
tsf_bool_t gpc_proto_append_tableset_local_to_field(gpc_proto_t *proto,
                                                    gpc_cell_t offset,
                                                    gpc_cell_t num,
                                                    const gpc_cell_t *args);

/* append a tableset_field_to_field instruction */
tsf_bool_t gpc_proto_append_tableset_field_to_field(gpc_proto_t *proto,
                                                    gpc_cell_t dest_offset,
                                                    gpc_cell_t src_offset,
                                                    gpc_cell_t num,
                                                    const gpc_cell_t *args);

/* append a tablejump_local instruction */
tsf_bool_t gpc_proto_append_tablejump_local(gpc_proto_t *proto,
                                            gpc_cell_t num,
                                            const gpc_cell_t *args);

/* append a tablejump_local instruction */
tsf_bool_t gpc_proto_append_tablejump_field(gpc_proto_t *proto,
                                            gpc_cell_t offset,
                                            gpc_cell_t num,
                                            const gpc_cell_t *args);

/* print the prototype to the given FILE stream. */
void gpc_proto_print(gpc_proto_t *proto,
                     FILE *out);

/* debugging stuff */
void gpc_code_gen_debug(const char *type,
                        gpc_proto_t *proto);

/* create an interpretable from a prototype. */
gpc_intable_t *gpc_intable_from_proto(gpc_proto_t *proto);

/* clone an interpretable */
gpc_intable_t *gpc_intable_clone(gpc_intable_t *intable);

/* destroy an interpretable. */
void gpc_intable_destroy(gpc_intable_t *intable);

/* get the number of arguments */
#define gpc_intable_get_num_args(intable) \
            ((intable)->num_args)

/* get the beginning of the intable stream */
#define gpc_intable_get_stream(intable) \
            ((intable)->stream)

/* get the end (STL-style) of the intable stream */
#define gpc_intable_get_stream_end(intable) \
            ((intable)->stream+(intable)->size)

/* returns tsf_true if the given cell address is a jump target */
#define gpc_intable_is_target(intable,address) \
            (tsf_st_lookup((intable)->targets,(char*)address,NULL))

/* returns tsf_true if the given cell address is live code */
#define gpc_intable_is_live(intable,address) \
            (tsf_st_lookup((intable)->stack_heights,(char*)address,NULL))

/* returns the stack height at the given piece of code.  will return
   -1 if the code is not live.  note that the error code is not set in
   either case. */
intptr_t gpc_intable_get_height(gpc_intable_t *prog,
				gpc_cell_t *address);

/* returns tsf_true if gpc_intable_run is supported. */
tsf_bool_t gpc_intable_run_supported();

/* interpret the intable. */
uintptr_t gpc_intable_run(gpc_intable_t *prog,
                          void *region,
                          gpc_cell_t *args);

/* write the program using the given writer. */
tsf_bool_t gpc_intable_write(gpc_intable_t *prog,
			     tsf_writer_t writer,
			     void *arg);

/* NOTE: the all_returns, predecessors, and min_remaining analyses are
 * currently unused and so hence are excluded from the Makefile and from
 * the distribution.  They are, however, included in CVS, lest they ever
 * become useful. */

/* analysis to find all returns.  returns a NULL-terminated array of return
 * points in the given program.
 *
 * FIXME: this really should not be a separate analysis.  it should be just
 * an extra return from gpc_intable_get_predecessors(). */
gpc_cell_t **gpc_intable_find_all_returns(gpc_intable_t *prog);

/* find predecessors.  returns a hashtable mapping each instruction cell
 * to a linked list of its predecessors.  the linked list is typed as a
 * gpc_cell_list_t. */
tsf_st_table *gpc_intable_get_predecessors(gpc_intable_t *prog);

/* you should use this function to free up the list returned by the
 * gpc_intable_get_predecessors() function. */
void gpc_intable_destroy_predecessors(tsf_st_table *preds);

/* gives you a table that maps each instruction to the minimum number of
 * bytes that you are guaranteed to have left in the buffer, assuming
 * that the program does not overrun the buffer in the end. */
tsf_st_table *gpc_intable_get_min_remaining(gpc_intable_t *prog);

/* return true if the given intable ever does integer to float
 * conversion. */
tsf_bool_t gpc_intable_can_do_int_to_float_conversion(gpc_intable_t *prog);

/* returns tsf_true if the given intable ever calls functions that require
 * a pointer to the buffer pointer register. */
tsf_bool_t gpc_intable_can_call_buffer_mutator_functions(gpc_intable_t *prog);

/* returns tsf_true if the given intable ever uses the in_map (also
   known as types).  definitions of in_map don't con't - so this will
   return tsf_false if the intable defines but does not use the
   in_map. */
tsf_bool_t gpc_intable_can_use_in_map(gpc_intable_t *prog);

/* returns tsf_true if the given intable ever uses the out_map.
   definitions of out_map don't con't - so this will return tsf_false
   if the intable defines but does not use the out_map. */
tsf_bool_t gpc_intable_can_use_out_map(gpc_intable_t *prog);

/* returns tsf_true if gpc_threaded_t is supported. */
tsf_bool_t gpc_threaded_supported();

/* create a threaded program from an intable.  this may fail if GPC
 * was compiled with a compiler that does not support the address-of-
 * label extension. */
gpc_threaded_t *gpc_threaded_from_intable(gpc_intable_t *intable);

/* destroy a threaded */
void gpc_threaded_destroy(gpc_threaded_t *threaded);

/* get the number of arguments */
#define gpc_threaded_get_num_args(threaded) \
            ((threaded)->num_args)

/* run a threaded */
uintptr_t gpc_threaded_run(gpc_threaded_t *prog,
                           void *region,
                           gpc_cell_t *args);

/* create a program using the engine of your choice.  if you pass in
 * TSF_EN_DEFAULT, then the default engine is used (based on the
 * TSF_ENGINE_CONFIG environment variable; if it is not set, then
 * the default is GPINT).  if you pass in TSF_EN_NOT_SUPPORTED, then
 * NULL is returned without the error being set (this allows for error
 * propagation). */
gpc_program_t *gpc_program_from_proto(gpc_proto_t *proto);

/* same as above but create from an intable. */
gpc_program_t *gpc_program_from_intable(gpc_intable_t *intable);

/* destroy a program */
void gpc_program_destroy(gpc_program_t *prog);

/* run a program */
uintptr_t gpc_program_run(gpc_program_t *prog,
                          void *region,
                          gpc_cell_t *args);

#endif

