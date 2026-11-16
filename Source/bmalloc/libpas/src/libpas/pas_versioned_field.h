/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_VERSIONED_FIELD_H
#define PAS_VERSIONED_FIELD_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_versioned_field;
typedef struct pas_versioned_field pas_versioned_field;

struct PAS_ALIGNED(sizeof(uintptr_t) * 2) pas_versioned_field {
    uintptr_t value;
    uintptr_t version;
};

#define PAS_VERSIONED_FIELD_INITIALIZER ((pas_versioned_field){.value = 0, .version = 0})

#define PAS_VERSIONED_FIELD_IS_WATCHED_BIT  1
#define PAS_VERSIONED_FIELD_VERSION_STEP    2

#define PAS_VERSIONED_FIELD_INVALID_VERSION UINTPTR_MAX

static inline void pas_versioned_field_construct(pas_versioned_field* field,
                                                 uintptr_t value)
{
    field->value = value;
    field->version = 0;
}

static inline pas_versioned_field pas_versioned_field_create(uintptr_t value,
                                                             uintptr_t version)
{
    pas_versioned_field field;
    field.value = value;
    field.version = version;
    return field;
}

/* Versioned fields with invalid versions will always fail as expected values for try_write. */
static inline pas_versioned_field pas_versioned_field_create_with_invalid_version(uintptr_t value)
{
    return pas_versioned_field_create(value, PAS_VERSIONED_FIELD_INVALID_VERSION);
}

static inline pas_versioned_field pas_versioned_field_create_empty(void)
{
    return pas_versioned_field_create(0, 0);
}

static inline pas_versioned_field pas_versioned_field_with_version(pas_versioned_field field,
                                                                   uintptr_t version)
{
    return pas_versioned_field_create(field.value, version);
}

static inline pas_versioned_field pas_versioned_field_with_value(pas_versioned_field field,
                                                                 uintptr_t value)
{
    return pas_versioned_field_create(value, field.version);
}

static inline pas_pair pas_versioned_field_as_pair(pas_versioned_field field)
{
    union {
        pas_pair pair;
        pas_versioned_field field;
    } u;
    
    u.field = field;
    return u.pair;
}

static inline pas_versioned_field pas_versioned_field_from_pair(pas_pair pair)
{
    union {
        pas_pair pair;
        pas_versioned_field field;
    } u;
    
    u.pair = pair;
    return u.field;
}

static inline bool pas_versioned_field_equals(pas_versioned_field a,
                                              pas_versioned_field b)
{
    return a.value == b.value
        && a.version == b.version;
}

static inline pas_versioned_field pas_versioned_field_read_torn(pas_versioned_field* field)
{
    return *field;
}

/* This could use the instruction on arm. But for now it's just written in a way that ensures adequate
   atomicity without effects from the CPU's viewpoint. (It's hella effectful on the compiler, of course,
   since we don't want the compiler to think it can get creative with this code.)*/
static inline pas_versioned_field pas_versioned_field_read(pas_versioned_field* field)
{
    for (;;) {
        pas_versioned_field result;
        uintptr_t depend;

        result.version = field->version;
        depend = pas_depend(result.version);

        result.value = field[depend].value;
        depend = pas_depend(result.value);

        if (field[depend].version == result.version)
            return result;
    }
}

static inline bool pas_versioned_field_weak_cas(pas_versioned_field* field,
                                                pas_versioned_field expected_value,
                                                pas_versioned_field new_value)
{
    return pas_compare_and_swap_pair_weak(
        field,
        pas_versioned_field_as_pair(expected_value),
        pas_versioned_field_as_pair(new_value));
}

static inline pas_versioned_field pas_versioned_field_strong_cas(pas_versioned_field* field,
                                                                 pas_versioned_field expected_value,
                                                                 pas_versioned_field new_value)
{
    return pas_versioned_field_from_pair(
        pas_compare_and_swap_pair_strong(
            field,
            pas_versioned_field_as_pair(expected_value),
            pas_versioned_field_as_pair(new_value)));
}

static inline pas_versioned_field pas_versioned_field_read_to_watch(pas_versioned_field* field)
{
    pas_versioned_field result;
    
    for (;;) {
        pas_versioned_field old_value;
        pas_versioned_field new_value;

        old_value = pas_versioned_field_read(field);

        if (old_value.version & PAS_VERSIONED_FIELD_IS_WATCHED_BIT) {
            result = old_value;
            break;
        }

        new_value = pas_versioned_field_with_version(old_value, old_value.version + 1);

        if (pas_versioned_field_weak_cas(field, old_value, new_value)) {
            result = new_value;
            break;
        }
    }

    PAS_ASSERT(result.version & PAS_VERSIONED_FIELD_IS_WATCHED_BIT);
    return result;
}

/* If the field is being watched, this does a double-CAS to bump the version of the field (making it
   not watched) while changing the value.
   
   If the field is not being watched, then:
   
     -> If the new value equals the expected value, this does nothing and returns true.

     -> If the new value is different, this does a double-CAS to change the value while keeping the
        version the same.

   Note how the optimization for the equals case produces the same outcome as if we had done the
   double-CAS, except for fencing. */
static inline bool pas_versioned_field_try_write(pas_versioned_field* field,
                                                 pas_versioned_field expected_value,
                                                 uintptr_t new_value)
{
    pas_versioned_field new_versioned_value;

    if (expected_value.version == PAS_VERSIONED_FIELD_INVALID_VERSION)
        return false;

    new_versioned_value = pas_versioned_field_with_value(expected_value, new_value);

    if (!(expected_value.version & PAS_VERSIONED_FIELD_IS_WATCHED_BIT)) {
        if (new_value == expected_value.value) {
            /* If we're not changing the value then we rely on the fact that the read was atomic. */
            return true;
        }
    } else
        new_versioned_value.version++;

    return pas_versioned_field_weak_cas(field, expected_value, new_versioned_value);
}

/* This is a variant of pas_versioned_field_try_write that:
   
   - Only works for expected_value produced by read_to_watch.
   
   - Changes the version but keeps the field watched, so the next call to read_to_watch won't have
     to CAS.
   
   This is meant to be used by the allocator since we expect runs of allocations. */
static inline bool pas_versioned_field_try_write_watched(pas_versioned_field* field,
                                                         pas_versioned_field expected_value,
                                                         uintptr_t new_value)
{
    pas_versioned_field new_versioned_value;

    if (expected_value.version == PAS_VERSIONED_FIELD_INVALID_VERSION)
        return false;

    PAS_ASSERT(expected_value.version & PAS_VERSIONED_FIELD_IS_WATCHED_BIT);

    new_versioned_value = pas_versioned_field_with_version(
        pas_versioned_field_with_value(expected_value, new_value),
        expected_value.version + PAS_VERSIONED_FIELD_VERSION_STEP);

    return pas_versioned_field_equals(
        pas_versioned_field_strong_cas(field, expected_value, new_versioned_value),
        expected_value);
}

static inline void pas_versioned_field_write(pas_versioned_field* field,
                                             uintptr_t new_value)
{
    for (;;) {
        pas_versioned_field snapshot;
        
        snapshot = *field;
        
        if (pas_versioned_field_try_write(field, snapshot, new_value))
            return;
    }
}

PAS_API uintptr_t pas_versioned_field_minimize(pas_versioned_field* field,
                                               uintptr_t new_value);

PAS_API uintptr_t pas_versioned_field_maximize(pas_versioned_field* field,
                                               uintptr_t new_value);

PAS_API void pas_versioned_field_minimize_watched(pas_versioned_field* field,
                                                  pas_versioned_field expected_value,
                                                  uintptr_t new_value);

PAS_API void pas_versioned_field_maximize_watched(pas_versioned_field* field,
                                                  pas_versioned_field expected_value,
                                                  uintptr_t new_value);

PAS_END_EXTERN_C;

#endif /* PAS_VERSIONED_FIELD_H */

