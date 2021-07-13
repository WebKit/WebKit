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

#ifndef PAS_HEAP_CONFIG_KIND_H
#define PAS_HEAP_CONFIG_KIND_H

#include "pas_bitvector.h"

PAS_BEGIN_EXTERN_C;

struct pas_heap_config;
typedef struct pas_heap_config pas_heap_config;

enum pas_heap_config_kind {
#define PAS_DEFINE_HEAP_CONFIG_KIND(name, value) \
    pas_heap_config_kind_ ## name,
#include "pas_heap_config_kind.def"
#undef PAS_DEFINE_HEAP_CONFIG_KIND
};

typedef enum pas_heap_config_kind pas_heap_config_kind;

static const unsigned pas_heap_config_kind_num_kinds =
    0
#define PAS_DEFINE_HEAP_CONFIG_KIND(name, value) \
    + 1
#include "pas_heap_config_kind.def"
#undef PAS_DEFINE_HEAP_CONFIG_KIND
    ;

static inline const char*
pas_heap_config_kind_get_string(pas_heap_config_kind kind)
{
    switch (kind) {
#define PAS_DEFINE_HEAP_CONFIG_KIND(name, value) \
    case pas_heap_config_kind_ ## name: \
        return #name;
#include "pas_heap_config_kind.def"
#undef PAS_DEFINE_HEAP_CONFIG_KIND
    }
    PAS_ASSERT(!"Invalid kind");
    return NULL;
}

typedef bool (*pas_heap_config_kind_callback)(pas_heap_config_kind kind,
                                              pas_heap_config* config,
                                              void* arg);

PAS_API bool pas_heap_config_kind_for_each(
    pas_heap_config_kind_callback callback,
    void *arg);

#define PAS_EACH_HEAP_CONFIG_KIND(kind) \
    kind = (pas_heap_config_kind)0; \
    kind < pas_heap_config_kind_num_kinds; \
    kind = (pas_heap_config_kind)((unsigned)kind + 1)

PAS_API extern pas_heap_config* pas_heap_config_kind_for_config_table[];

static inline pas_heap_config* pas_heap_config_kind_get_config(pas_heap_config_kind kind)
{
    PAS_TESTING_ASSERT(kind < pas_heap_config_kind_num_kinds);
    return pas_heap_config_kind_for_config_table[kind];
}

PAS_API extern unsigned pas_heap_config_kind_is_active_bitvector[];

static inline bool pas_heap_config_kind_is_active(pas_heap_config_kind kind)
{
    PAS_TESTING_ASSERT(kind < pas_heap_config_kind_num_kinds);
    return pas_bitvector_get(pas_heap_config_kind_is_active_bitvector, (size_t)kind);
}

/* Returns true if we did set the bit. Must be called with heap lock held. */
PAS_API bool pas_heap_config_kind_set_active(pas_heap_config_kind kind);

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_CONFIG_KIND_H */

