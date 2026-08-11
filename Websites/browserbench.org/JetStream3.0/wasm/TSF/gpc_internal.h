/*
 * Copyright (C) 2003, 2004, 2005, 2006 Filip Pizlo. All rights reserved.
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

#ifndef FP_GPC_INTERNAL_H
#define FP_GPC_INTERNAL_H

#include "tsf_internal.h"
#include "gpc.h"

/* stupid stuff */
void gpc_unlink_free(char *name);
void gpc_rmdir_free(char *name);
void gpc_rmdashrf_free(char *name);

/* debugging stuff */
void gpc_dyncc_code_gen_debug(const char *type,
                              const char *src_filename);

/* figure out the stack height for each cell inside of an intable.
 * also gives you the maximum stack height.  returns a hashtable
 * that maps the instruction cell address to the stack height.
 * the max stack height is stored in *max_height if max_height!=NULL.
 * also note that the stack height at a cell is the height that the
 * stack would have had when you were entering into that instruction,
 * not exiting it.
 *
 * NOTE: we represent heights using intptr_t because:
 * 1) we need signed, since stack effects can be negative, and
 * 2) we need it to be pointer-size, since we use it as a value to the
 *    hashtable, and hashtable values are void*. */
tsf_st_table *gpc_intable_get_stack_heights(gpc_intable_t *prog,
                                            intptr_t *max_height);

/* code generation thingies */
#define gpc_read_bitvector_to_array(nbytes) \
    uint32_t len;\
    uint32_t size;\
    uint32_t i;\
    \
    copy_ntohc_incsrc_bc(&ui8_tmp,cur,end,bounds_error);\
    if (ui8_tmp==255) {\
        copy_ntohl_incsrc_bc((uint8_t*)&len,cur,\
                             end,bounds_error);\
    } else {\
        len = ui8_tmp;\
    }\
    \
    array->len=len;\
    size=((len+7)>>3);\
    \
    if (cur + size > end) {\
        goto bounds_error;\
    }\
    \
    array->data=tsf_region_alloc(region,len*nbytes);\
    if (array->data==NULL) {\
        tsf_set_errno("Could not tsf_region_alloc()");\
        INT_FAILURE();\
    }\
    \
    for (i=7;i<len;i+=8) {\
        uint8_t bits=*cur;\
        array->data[i-7]=((bits>>0)&1);\
        array->data[i-6]=((bits>>1)&1);\
        array->data[i-5]=((bits>>2)&1);\
        array->data[i-4]=((bits>>3)&1);\
        array->data[i-3]=((bits>>4)&1);\
        array->data[i-2]=((bits>>5)&1);\
        array->data[i-1]=((bits>>6)&1);\
        array->data[i-0]=((bits>>7)&1);\
        cur++;\
    }\
    i=len&~7;\
    if (i<len) {\
        uint8_t bits=*cur;\
        array->data[i++]=((bits>>0)&1);\
        if (i<len) {\
            array->data[i++]=((bits>>1)&1);\
            if (i<len) {\
                array->data[i++]=((bits>>2)&1);\
                if (i<len) {\
                    array->data[i++]=((bits>>3)&1);\
                    if (i<len) {\
                        array->data[i++]=((bits>>4)&1);\
                        if (i<len) {\
                            array->data[i++]=((bits>>5)&1);\
                            if (i<len) {\
                                array->data[i++]=((bits>>6)&1);\
                                if (i<len) {\
                                    array->data[i++]=((bits>>7)&1);\
                                }\
                            }\
                        }\
                    }\
                }\
            }\
        }\
        cur++;\
    }

#define gpc_read_array_to_bitvector(kcode,tmpname,nbitbytes) \
    uint32_t size;\
    uint32_t len;\
    uint8_t *bvcur;\
    uint32_t nbytes;\
    uint32_t overflow;\
    uint32_t i;\
    \
    copy_ntohc_incsrc_bc(&ui8_tmp,cur,end,bounds_error);\
    if (ui8_tmp==255) {\
        copy_ntohl_incsrc_bc((uint8_t*)&bitvector->num_bits,cur,\
                             end,bounds_error);\
    } else {\
        bitvector->num_bits = ui8_tmp;\
    }\
    \
    len=bitvector->num_bits;\
    size=((bitvector->num_bits + 7)>>3);\
    \
    if (cur + len*nbitbytes > end) {\
        goto bounds_error;\
    }\
    \
    bitvector->bits=tsf_region_alloc(region,size);\
    if (bitvector->bits==NULL) {\
        tsf_set_errno("Could not tsf_region_alloc()");\
        INT_FAILURE();\
    }\
    \
    bvcur=bitvector->bits;\
    nbytes=len>>3;\
    \
    while (nbytes-->0) {\
        uint8_t bits=0;\
        for (i=0;i<8;++i) {\
            copy_ntoh##kcode##_incsrc((uint8_t*)&tmpname,cur);\
            if (tmpname) bits|=(1<<i);\
        }\
        *bvcur++=bits;\
    }\
    overflow=len&7;\
    if (len>0) {\
        uint8_t bits=0;\
        for (i=0;i<len;++i) {\
            copy_ntoh##kcode##_incsrc((uint8_t*)&tmpname,cur);\
            if (tmpname) bits|=(1<<i);\
        }\
        *bvcur=bits;\
    }

#define gpc_read_integer_array_to_bitvector(integer_type) \
    uint32_t len, i;\
    \
    copy_ntohc_incsrc_bc(&ui8_tmp, cur, end, bounds_error);\
    if (ui8_tmp == 255) {\
        copy_ntohl_incsrc_bc((uint8_t*)&bitvector->num_bits, cur,\
                             end, bounds_error);\
    } else {\
        bitvector->num_bits = ui8_tmp;\
    }\
    \
    len = bitvector->num_bits;\
    \
    bitvector->bits = tsf_region_alloc(region, (len + 7) >> 3);\
    if (bitvector->bits == NULL) {\
        tsf_set_errno("Could not tsf_region_alloc()");\
        INT_FAILURE();\
    }\
    \
    for (i = 0; i < len; ++i) {\
        tsf_##integer_type##_t integer;\
        read_tsf_##integer_type##_incsrc(&integer, cur, end, bounds_error);\
        if (integer) {\
            bitvector->bits[i >> 3] |= (1 << (i & 7));\
        } else {\
            bitvector->bits[i >> 3] &= ~(1 << (i & 7));\
        }\
    }

#define gpc_copy_array_to_bitvector() \
    uint8_t *dest_cur;\
    uint32_t i,j;\
    uint8_t cur_bits;\
    \
    dest->num_bits=src->len;\
    \
    dest->bits=tsf_region_alloc(region,(dest->num_bits+7)>>3);\
    if (dest->bits==NULL) {\
        tsf_set_errno("Could not allocate bitvector from region");\
        INT_FAILURE();\
    }\
    \
    dest_cur=dest->bits;\
    src_cur=src->data;\
    for (i=dest->num_bits>>3;i-->0;) {\
        cur_bits=0;\
        for (j=0;j<8;++j) {\
            if (*src_cur++) {\
                cur_bits|=(1<<j);\
            }\
        }\
        *dest_cur++=cur_bits;\
    }\
    if ((dest->num_bits&~7)<dest->num_bits) {\
        cur_bits=0;\
        for (i=(dest->num_bits&~7),j=0;i<dest->num_bits;++i,++j) {\
            if (*src_cur++) {\
                cur_bits|=(1<<j);\
            }\
        }\
        *dest_cur=cur_bits;\
    }

#define gpc_copy_bitvector_to_array(nbitbytes) \
    \
    uint32_t i;\
    uint32_t n;\
    uint8_t bits;\
    \
    dest->len=src->num_bits;\
    dest->data=tsf_region_alloc(region,dest->len*nbitbytes);\
    if (dest->data==NULL) {\
        tsf_set_errno("Could not allocate array");\
        INT_FAILURE();\
    }\
    \
    dest_cur=dest->data;\
    \
    n=dest->len>>3;\
    for (i=0;i<n;++i) {\
        bits=src->bits[i];\
        *dest_cur++=((bits>>0)&1);\
        *dest_cur++=((bits>>1)&1);\
        *dest_cur++=((bits>>2)&1);\
        *dest_cur++=((bits>>3)&1);\
        *dest_cur++=((bits>>4)&1);\
        *dest_cur++=((bits>>5)&1);\
        *dest_cur++=((bits>>6)&1);\
        *dest_cur++=((bits>>7)&1);\
    }\
    if ((dest->len&~7)<dest->len) {\
        bits=src->bits[(dest->len-1)>>3];\
        for (i=(dest->len&~7);i<dest->len;++i) {\
            *dest_cur++=bits&1;\
            bits>>=1;\
        }\
    }

#endif

