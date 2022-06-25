/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PAS_LARGE_SHARING_POOL_EPOCH_UPDATE_MODE_H
#define PAS_LARGE_SHARING_POOL_EPOCH_UPDATE_MODE_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_large_sharing_pool_epoch_update_mode {
    pas_large_sharing_pool_forward_min_epoch,
    pas_large_sharing_pool_combined_use_epoch
};

typedef enum pas_large_sharing_pool_epoch_update_mode pas_large_sharing_pool_epoch_update_mode;

static inline const char*
pas_large_sharing_pool_epoch_update_mode_get_string(
    pas_large_sharing_pool_epoch_update_mode mode)
{
    switch (mode) {
    case pas_large_sharing_pool_forward_min_epoch:
        return "forward_min_epoch";
    case pas_large_sharing_pool_combined_use_epoch:
        return "combined_use_epoch";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_SHARING_POOL_EPOCH_UPDATE_MODE_H */

