/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_LARGE_MAP_H
#define PAS_LARGE_MAP_H

#include "pas_bootstrap_free_heap.h"
#include "pas_config.h"
#include "pas_first_level_tiny_large_map_entry.h"
#include "pas_hashtable.h"
#include "pas_large_map_entry.h"
#include "pas_small_large_map_entry.h"
#include "pas_tiny_large_map_entry.h"
#include "pas_utility_heap.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_large_heap;
typedef struct pas_large_heap pas_large_heap;

PAS_CREATE_HASHTABLE(pas_large_map_hashtable,
                     pas_large_map_entry,
                     pas_large_map_key);

PAS_API extern pas_large_map_hashtable pas_large_map_hashtable_instance;
PAS_API extern pas_large_map_hashtable_in_flux_stash pas_large_map_hashtable_instance_in_flux_stash;

PAS_CREATE_HASHTABLE(pas_small_large_map_hashtable,
                     pas_small_large_map_entry,
                     pas_large_map_key);

PAS_API extern pas_small_large_map_hashtable pas_small_large_map_hashtable_instance;
PAS_API extern pas_small_large_map_hashtable_in_flux_stash pas_small_large_map_hashtable_instance_in_flux_stash;

PAS_CREATE_HASHTABLE(pas_tiny_large_map_hashtable,
                     pas_first_level_tiny_large_map_entry,
                     pas_first_level_tiny_large_map_key);

PAS_API extern pas_tiny_large_map_hashtable pas_tiny_large_map_hashtable_instance;
PAS_API extern pas_tiny_large_map_hashtable_in_flux_stash pas_tiny_large_map_hashtable_instance_in_flux_stash;
PAS_API extern pas_tiny_large_map_second_level_hashtable_in_flux_stash pas_tiny_large_map_second_level_hashtable_in_flux_stash_instance;

PAS_API pas_large_map_entry pas_large_map_find(uintptr_t begin);

PAS_API void pas_large_map_add(pas_large_map_entry entry);
PAS_API pas_large_map_entry pas_large_map_take(uintptr_t begin);

typedef bool (*pas_large_map_for_each_entry_callback)(pas_large_map_entry entry,
                                                      void* arg);

PAS_API bool pas_large_map_for_each_entry(pas_large_map_for_each_entry_callback callback,
                                          void *arg);

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_MAP_H */

