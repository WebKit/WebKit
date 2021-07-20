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

#ifndef PAS_ALLOCATOR_SCAVENGE_ACTION_H
#define PAS_ALLOCATOR_SCAVENGE_ACTION_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_allocator_scavenge_action {
    pas_allocator_scavenge_no_action,
    
    /* Request that the allocators stop if they are not dirty. Stop them forcefully if we had
       already asked them to stop and they didn't. */
    pas_allocator_scavenge_request_stop_action,
    
    /* Stop allocators forcefully. */
    pas_allocator_scavenge_force_stop_action
};

typedef enum pas_allocator_scavenge_action pas_allocator_scavenge_action;

static inline const char*
pas_allocator_scavenge_action_get_string(pas_allocator_scavenge_action action)
{
    switch (action)
    {
    case pas_allocator_scavenge_no_action:
        return "no_action";
    case pas_allocator_scavenge_request_stop_action:
        return "request_stop_action";
    case pas_allocator_scavenge_force_stop_action:
        return "force_stop_action";
    }
    
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_ALLOCATOR_SCAVENGE_ACTION_H */

