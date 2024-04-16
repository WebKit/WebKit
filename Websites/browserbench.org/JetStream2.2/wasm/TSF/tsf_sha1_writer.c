/*
 * Copyright (C) 2011, 2015 Filip Pizlo. All rights reserved.
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

#include "tsf_internal.h"

tsf_sha1_wtr_t *tsf_sha1_writer_create(void) {
    tsf_sha1_wtr_t *result=malloc(sizeof(tsf_sha1_wtr_t));
    if (result==NULL) {
        tsf_set_errno("Could not malloc tsf_sha1_wtr_t");
        return NULL;
    }
    
    tsf_SHA1Reset(&result->ctx);
    
    return result;
}

void tsf_sha1_writer_destroy(tsf_sha1_wtr_t *sha1) {
    free(sha1);
}

tsf_bool_t tsf_sha1_writer_write(void *arg,
                                 const void *buf,
                                 uint32_t len) {
    tsf_sha1_wtr_t *sha1=(tsf_sha1_wtr_t*)arg;
    
    if (sha1->ctx.Computed) {
        tsf_set_error(TSF_E_BAD_STATE,
                      "Cannot write using a SHA1 writer after "
                      "tsf_sha1_writer_finish() has been called");
        return tsf_false;
    }
    
    tsf_SHA1Input(&sha1->ctx, (const uint8_t*)buf, len);
    
    if (sha1->ctx.Corrupted) {
        tsf_set_error(TSF_E_INTERNAL,
                      "SHA1 writer has been corrupted.");
        return tsf_false;
    }
    
    return tsf_true;
}

uint32_t *tsf_sha1_writer_finish(tsf_sha1_wtr_t *sha1) {
    int32_t result=tsf_SHA1Result(&sha1->ctx);
    if (!result) {
        tsf_set_error(TSF_E_INTERNAL,
                      "SHA1 writer has been corrupted and the result cannot "
                      "be computed.");
        return NULL;
    }
    
    return tsf_sha1_writer_result(sha1);
}

uint32_t *tsf_sha1_writer_result(tsf_sha1_wtr_t *sha1) {
    return sha1->ctx.Message_Digest;
}

char *tsf_sha1_sum_to_str(uint32_t *sum) {
    const char *hex="0123456789abcdef";
    unsigned i;
    char *result;
    char *cur;
    
    if (sum==NULL) {
        /* propagate the error. */
        return NULL;
    }
    
    result=malloc(41);
    if (result==NULL) {
        tsf_set_errno("Could not malloc result in tsf_sha1_sum_to_str()");
        return NULL;
    }
    
    cur=result;
    
    for (i=0;i<5;++i) {
        uint32_t word;
        unsigned j;
        
        word=sum[i];

        for (j=0;j<4;++j) {
            *cur++ = hex[(word>>28)&0xF];
            *cur++ = hex[(word>>24)&0xF];
            
            word<<=8;
        }
    }
    
    *cur=0;
    
    return result;
}

