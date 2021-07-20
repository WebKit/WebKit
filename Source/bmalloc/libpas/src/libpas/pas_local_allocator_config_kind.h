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

#ifndef PAS_LOCAL_ALLOCATOR_CONFIG_KIND_H
#define PAS_LOCAL_ALLOCATOR_CONFIG_KIND_H

#include "pas_bitfit_page_config_kind.h"
#include "pas_segregated_page_config_kind.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_local_allocator_config_kind {
#define PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND(name, value) \
    pas_local_allocator_config_kind_normal_ ## name, \
    pas_local_allocator_config_kind_primordial_partial_ ## name,
#include "pas_segregated_page_config_kind.def"
#undef PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND
#define PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(name, value) \
    pas_local_allocator_config_kind_bitfit_ ## name,
#include "pas_bitfit_page_config_kind.def"
#undef PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND
};

typedef enum pas_local_allocator_config_kind pas_local_allocator_config_kind;

static PAS_ALWAYS_INLINE bool
pas_local_allocator_config_kind_is_primordial_partial(pas_local_allocator_config_kind kind)
{
    switch (kind) {
#define PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND(name, value) \
    case pas_local_allocator_config_kind_primordial_partial_ ## name: \
        return true;
#include "pas_segregated_page_config_kind.def"
#undef PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND
    default:
        return false;
    }
}

static PAS_ALWAYS_INLINE bool
pas_local_allocator_config_kind_is_bitfit(pas_local_allocator_config_kind kind)
{
    switch (kind) {
#define PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(name, value) \
    case pas_local_allocator_config_kind_bitfit_ ## name: \
        return true;
#include "pas_bitfit_page_config_kind.def"
#undef PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND
    default:
        return false;
    }
}

static PAS_ALWAYS_INLINE pas_local_allocator_config_kind
pas_local_allocator_config_kind_create_normal(pas_segregated_page_config_kind kind)
{
    switch (kind) {
#define PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND(name, value) \
    case pas_segregated_page_config_kind_ ## name: \
        return pas_local_allocator_config_kind_normal_ ## name;
#include "pas_segregated_page_config_kind.def"
#undef PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND
    }
    PAS_ASSERT(!"Should not be reached");
    return (pas_local_allocator_config_kind)0;
}

static PAS_ALWAYS_INLINE pas_local_allocator_config_kind
pas_local_allocator_config_kind_create_primordial_partial(pas_segregated_page_config_kind kind)
{
    switch (kind) {
#define PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND(name, value) \
    case pas_segregated_page_config_kind_ ## name: \
        return pas_local_allocator_config_kind_primordial_partial_ ## name;
#include "pas_segregated_page_config_kind.def"
#undef PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND
    }
    PAS_ASSERT(!"Should not be reached");
    return (pas_local_allocator_config_kind)0;
}

static PAS_ALWAYS_INLINE pas_local_allocator_config_kind
pas_local_allocator_config_kind_create_bitfit(pas_bitfit_page_config_kind kind)
{
    switch (kind) {
#define PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(name, value) \
    case pas_bitfit_page_config_kind_ ## name: \
        return pas_local_allocator_config_kind_bitfit_ ## name;
#include "pas_bitfit_page_config_kind.def"
#undef PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND
    }
    PAS_ASSERT(!"Should not be reached");
    return (pas_local_allocator_config_kind)0;
}

static PAS_ALWAYS_INLINE pas_segregated_page_config_kind
pas_local_allocator_config_kind_get_segregated_page_config_kind(pas_local_allocator_config_kind kind)
{
    switch (kind) {
#define PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND(name, value) \
    case pas_local_allocator_config_kind_normal_ ## name: \
    case pas_local_allocator_config_kind_primordial_partial_ ## name: \
        return pas_segregated_page_config_kind_ ## name;
#include "pas_segregated_page_config_kind.def"
#undef PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND
    default:
        PAS_ASSERT(!"Should not be reached");
        return (pas_segregated_page_config_kind)0;
    }
}

static PAS_ALWAYS_INLINE pas_bitfit_page_config_kind
pas_local_allocator_config_kind_get_bitfit_page_config_kind(pas_local_allocator_config_kind kind)
{
    switch (kind) {
#define PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(name, value) \
    case pas_local_allocator_config_kind_bitfit_ ## name: \
        return pas_bitfit_page_config_kind_ ## name;
#include "pas_bitfit_page_config_kind.def"
#undef PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND
    default:
        PAS_ASSERT(!"Should not be reached");
        return (pas_bitfit_page_config_kind)0;
    }
}

static inline const char*
pas_local_allocator_config_kind_get_string(pas_local_allocator_config_kind kind)
{
    switch (kind) {
#define PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND(name, value) \
    case pas_local_allocator_config_kind_normal_ ## name: \
        return "normal_" #name; \
    case pas_local_allocator_config_kind_primordial_partial_ ## name: \
        return "primordial_partial_" #name;
#include "pas_segregated_page_config_kind.def"
#undef PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND
#define PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(name, value) \
    case pas_local_allocator_config_kind_bitfit_ ## name: \
        return "bitfit_" #name;
#include "pas_bitfit_page_config_kind.def"
#undef PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_LOCAL_ALLOCATOR_CONFIG_KIND_H */

