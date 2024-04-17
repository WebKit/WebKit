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

#ifndef FP_TSF_ZIP_ABSTRACT_H
#define FP_TSF_ZIP_ABSTRACT_H

#include "tsf_types.h"

struct tsf_zip_abstract {
    uint8_t *next_in;
    uint32_t avail_in;
    
    uint8_t *next_out;
    uint32_t avail_out;
};

typedef struct tsf_zip_abstract tsf_zip_abstract_t;

enum tsf_zip_action {
    TSF_ZIP_ACT_RUN,
    TSF_ZIP_ACT_SYNC_FLUSH,
    TSF_ZIP_ACT_FULL_FLUSH,
    TSF_ZIP_ACT_FINISH
};

typedef enum tsf_zip_action tsf_zip_action_t;

enum tsf_zip_result {
    TSF_ZIP_RES_OK,
    TSF_ZIP_RES_END,
    TSF_ZIP_RES_ERROR   /* if this is returned, the error will be
                         * set. */
};

typedef enum tsf_zip_result tsf_zip_result_t;

tsf_zip_result_t tsf_zip_abstract_deflate(tsf_zip_mode_t mode,
                                          void *stream,
                                          tsf_zip_abstract_t *abstract,
                                          tsf_zip_action_t action);

tsf_zip_result_t tsf_zip_abstract_inflate(tsf_zip_mode_t mode,
                                          void *stream,
                                          tsf_zip_abstract_t *abstract,
                                          tsf_zip_action_t action);

#endif


