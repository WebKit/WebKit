/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef PAS_TRI_STATE_H
#define PAS_TRI_STATE_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_tri_state {
    pas_tri_state_no,
    pas_tri_state_maybe,
    pas_tri_state_yes
};

typedef enum pas_tri_state pas_tri_state;

static inline const char* pas_tri_state_get_string(pas_tri_state tri_state)
{
    switch (tri_state) {
    case pas_tri_state_no:
        return "no";
    case pas_tri_state_maybe:
        return "maybe";
    case pas_tri_state_yes:
        return "yes";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static inline bool pas_tri_state_equals_boolean(pas_tri_state tri_state, bool boolean)
{
    switch (tri_state) {
    case pas_tri_state_no:
        return !boolean;
    case pas_tri_state_maybe:
        return true;
    case pas_tri_state_yes:
        return boolean;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

PAS_END_EXTERN_C;

#endif /* PAS_TRI_STATE_H */

