/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_H
#define PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_H

#include "pas_segregated_page_config_kind.h"
#include "pas_segregated_page_role.h"

PAS_BEGIN_EXTERN_C;

enum pas_segregated_page_config_kind_and_role {
#define PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND(name, value) \
    pas_segregated_page_config_kind_ ## name ## _and_shared_role, \
    pas_segregated_page_config_kind_ ## name ## _and_exclusive_role,
#include "pas_segregated_page_config_kind.def"
#undef PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND
};

typedef enum pas_segregated_page_config_kind_and_role pas_segregated_page_config_kind_and_role;

#define PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_NUM_BITS 6u
#define PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_MASK \
    ((1u << PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_NUM_BITS) - 1u)

#if PAS_COMPILER(CLANG)
#define PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND(name, value) \
    _Static_assert(pas_segregated_page_config_kind_ ## name ## _and_shared_role \
                   <= PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_MASK, \
                   "Kind-and-role doesn't fit in kind-and-role bits"); \
    _Static_assert(pas_segregated_page_config_kind_ ## name ## _and_exclusive_role \
                   <= PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_MASK, \
                   "Kind-and-role doesn't fit in kind-and-role bits");
#include "pas_segregated_page_config_kind.def"
#undef PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND
#endif /* PAS_COMPILER(CLANG) */

PAS_API const char*
pas_segregated_page_config_kind_and_role_get_string(pas_segregated_page_config_kind_and_role kind_and_role);

static inline pas_segregated_page_config_kind_and_role pas_segregated_page_config_kind_and_role_create(
    pas_segregated_page_config_kind kind,
    pas_segregated_page_role role)
{
    pas_segregated_page_config_kind_and_role result;
    result = (pas_segregated_page_config_kind_and_role)(((unsigned)kind << 1) | (unsigned)role);
    if (PAS_ENABLE_TESTING) {
        switch (kind) {
#define PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND(name, value) \
        case pas_segregated_page_config_kind_ ## name: \
            switch (role) { \
            case pas_segregated_page_shared_role: \
                PAS_ASSERT(result == pas_segregated_page_config_kind_ ## name ## _and_shared_role); \
                break; \
            case pas_segregated_page_exclusive_role: \
                PAS_ASSERT(result == pas_segregated_page_config_kind_ ## name ## _and_exclusive_role); \
                break; \
            } \
            break;
#include "pas_segregated_page_config_kind.def"
#undef PAS_DEFINE_SEGREGATED_PAGE_CONFIG_KIND
        }
    }
    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PAGE_CONFIG_KIND_AND_ROLE_H */

