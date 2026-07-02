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

#ifndef PAS_ZERO_MODE_H
#define PAS_ZERO_MODE_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_zero_mode {
    pas_zero_mode_may_have_non_zero,
    pas_zero_mode_is_all_zero
};

typedef enum pas_zero_mode pas_zero_mode;

static inline const char* pas_zero_mode_get_string(pas_zero_mode mode)
{
    switch (mode) {
    case pas_zero_mode_may_have_non_zero:
        return "may_have_non_zero";
    case pas_zero_mode_is_all_zero:
        return "is_all_zero";
    }
    PAS_ASSERT(!"Invalid mode");
    return NULL;
}

static inline void pas_zero_mode_validate(pas_zero_mode mode)
{
    switch (mode) {
    case pas_zero_mode_may_have_non_zero:
    case pas_zero_mode_is_all_zero:
        return;
    }
    PAS_ASSERT(!"Invalid mode");
}

static inline pas_zero_mode pas_zero_mode_merge(pas_zero_mode left, pas_zero_mode right)
{
    switch (left) {
    case pas_zero_mode_may_have_non_zero:
        return pas_zero_mode_may_have_non_zero;
    case pas_zero_mode_is_all_zero:
        switch (right) {
        case pas_zero_mode_may_have_non_zero:
            return pas_zero_mode_may_have_non_zero;
        case pas_zero_mode_is_all_zero:
            return pas_zero_mode_is_all_zero;
        }
        PAS_ASSERT(!"Invalid right mode");
        return pas_zero_mode_may_have_non_zero;
    }
    PAS_ASSERT(!"Invalid left mode");
    return pas_zero_mode_may_have_non_zero;
}

PAS_END_EXTERN_C;

#endif /* PAS_ZERO_MODE_H */


