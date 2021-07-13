/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#include "pas_heap_config_kind.h"

#include "pas_all_heap_configs.h"
#include "pas_heap_lock.h"

bool pas_heap_config_kind_for_each(
    pas_heap_config_kind_callback callback,
    void *arg)
{
#define PAS_DEFINE_HEAP_CONFIG_KIND(name, value) \
    if (!callback(pas_heap_config_kind_ ## name, \
                  (value).config_ptr, \
                  arg)) \
        return false;
#include "pas_heap_config_kind.def"
#undef PAS_DEFINE_HEAP_CONFIG_KIND
    return true;    
}

pas_heap_config* pas_heap_config_kind_for_config_table[pas_heap_config_kind_num_kinds] = {
#define PAS_DEFINE_HEAP_CONFIG_KIND(name, value) \
    (value).config_ptr,
#include "pas_heap_config_kind.def"
#undef PAS_DEFINE_HEAP_CONFIG_KIND
};

unsigned pas_heap_config_kind_is_active_bitvector[
    PAS_BITVECTOR_NUM_WORDS(pas_heap_config_kind_num_kinds)];

bool pas_heap_config_kind_set_active(pas_heap_config_kind kind)
{
    PAS_TESTING_ASSERT(kind < pas_heap_config_kind_num_kinds);
    pas_heap_lock_assert_held();
    if (pas_bitvector_get(pas_heap_config_kind_is_active_bitvector, (size_t)kind))
        return false;
    pas_bitvector_set(pas_heap_config_kind_is_active_bitvector, (size_t)kind, true);
    return true;
}

#endif /* LIBPAS_ENABLED */
