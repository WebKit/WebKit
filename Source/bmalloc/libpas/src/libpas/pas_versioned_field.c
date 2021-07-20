/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_versioned_field.h"

uintptr_t pas_versioned_field_minimize(pas_versioned_field* field,
                                       uintptr_t new_value)
{
    for (;;) {
        pas_versioned_field old_value;
        
        old_value = pas_versioned_field_read(field);
        
        if (pas_versioned_field_try_write(field,
                                          old_value,
                                          PAS_MIN(new_value, old_value.value)))
            return old_value.value;
    }
}

uintptr_t pas_versioned_field_maximize(pas_versioned_field* field,
                                       uintptr_t new_value)
{
    for (;;) {
        pas_versioned_field old_value;
        
        old_value = pas_versioned_field_read(field);
        
        if (pas_versioned_field_try_write(field,
                                          old_value,
                                          PAS_MAX(new_value, old_value.value)))
            return old_value.value;
    }
}

void pas_versioned_field_minimize_watched(pas_versioned_field* field,
                                          pas_versioned_field expected_value,
                                          uintptr_t new_value)
{
    if (new_value < expected_value.value)
        pas_versioned_field_try_write_watched(field, expected_value, new_value);
}

void pas_versioned_field_maximize_watched(pas_versioned_field* field,
                                          pas_versioned_field expected_value,
                                          uintptr_t new_value)
{
    if (new_value > expected_value.value)
        pas_versioned_field_try_write_watched(field, expected_value, new_value);
}

#endif /* LIBPAS_ENABLED */
