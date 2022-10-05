/*
 * Copyright (C) 2004, 2005 Filip Pizlo. All rights reserved.
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

tsf_limits_t *tsf_limits_create() {
    tsf_limits_t *result=malloc(sizeof(tsf_limits_t));
    if (result==NULL) {
        tsf_set_errno("Could not malloc tsf_limits_t");
        return NULL;
    }
    
    result->max_types           = UINT32_MAX;
    result->max_type_size       = UINT32_MAX;
    result->max_total_type_size = UINT32_MAX;
    result->max_buf_size        = UINT32_MAX;
    result->depth_limit         = UINT32_MAX;
    memset(&(result->allow_type_kind),255,32);
    result->max_array_length    = UINT32_MAX;
    result->max_struct_size     = UINT32_MAX;
    result->max_choice_size     = UINT32_MAX;
    result->max_string_size     = UINT32_MAX;
    
    return result;
}

void tsf_limits_destroy(tsf_limits_t *limits) {
    if (limits==NULL) {
        return;
    }
    free(limits);
}

tsf_limits_t *tsf_limits_clone(tsf_limits_t *limits) {
    tsf_limits_t *result=malloc(sizeof(tsf_limits_t));
    if (result==NULL) {
        tsf_set_errno("Could not malloc tsf_limits_t");
        return NULL;
    }
    
    memcpy(result,limits,sizeof(tsf_limits_t));
    
    return result;
}




