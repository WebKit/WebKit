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

#ifndef PAS_SEGREGATED_DEALLOCATION_LOGGING_MODE_H
#define PAS_SEGREGATED_DEALLOCATION_LOGGING_MODE_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_segregated_deallocation_logging_mode {
    pas_segregated_deallocation_no_logging_mode,
    pas_segregated_deallocation_size_oblivious_logging_mode,
    pas_segregated_deallocation_size_aware_logging_mode,
    pas_segregated_deallocation_checked_size_oblivious_logging_mode,
    pas_segregated_deallocation_checked_size_aware_logging_mode
};

typedef enum pas_segregated_deallocation_logging_mode pas_segregated_deallocation_logging_mode;

static inline bool pas_segregated_deallocation_logging_mode_does_logging(
    pas_segregated_deallocation_logging_mode logging_mode)
{
    switch (logging_mode) {
    case pas_segregated_deallocation_no_logging_mode:
        return false;
    case pas_segregated_deallocation_size_oblivious_logging_mode:
    case pas_segregated_deallocation_size_aware_logging_mode:
    case pas_segregated_deallocation_checked_size_oblivious_logging_mode:
    case pas_segregated_deallocation_checked_size_aware_logging_mode:
        return true;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

static inline bool pas_segregated_deallocation_logging_mode_is_size_aware(
    pas_segregated_deallocation_logging_mode logging_mode)
{
    switch (logging_mode) {
    case pas_segregated_deallocation_no_logging_mode:
        PAS_ASSERT(!"Should not be reached");
        return false;
    case pas_segregated_deallocation_size_oblivious_logging_mode:
    case pas_segregated_deallocation_checked_size_oblivious_logging_mode:
        return false;
    case pas_segregated_deallocation_size_aware_logging_mode:
    case pas_segregated_deallocation_checked_size_aware_logging_mode:
        return true;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

static inline bool pas_segregated_deallocation_logging_mode_is_checked(
    pas_segregated_deallocation_logging_mode logging_mode)
{
    switch (logging_mode) {
    case pas_segregated_deallocation_no_logging_mode:
        PAS_ASSERT(!"Should not be reached");
        return false;
    case pas_segregated_deallocation_size_oblivious_logging_mode:
    case pas_segregated_deallocation_size_aware_logging_mode:
        return false;
    case pas_segregated_deallocation_checked_size_oblivious_logging_mode:
    case pas_segregated_deallocation_checked_size_aware_logging_mode:
        return true;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_DEALLOCATION_LOGGING_MODE_H */

