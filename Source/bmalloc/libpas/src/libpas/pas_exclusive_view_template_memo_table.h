/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_EXCLUSIVE_VIEW_TEMPLATE_MEMO_TABLE_H
#define PAS_EXCLUSIVE_VIEW_TEMPLATE_MEMO_TABLE_H

#include "pas_config.h"

#include "pas_hashtable.h"
#include "pas_segregated_global_size_directory.h"

PAS_BEGIN_EXTERN_C;

struct pas_exclusive_view_template_memo_key;
typedef struct pas_exclusive_view_template_memo_key pas_exclusive_view_template_memo_key;

struct pas_exclusive_view_template_memo_key {
    unsigned object_size;
    pas_segregated_page_config_kind page_config_kind;
};

static inline pas_exclusive_view_template_memo_key
pas_exclusive_view_template_memo_key_create(
    unsigned object_size,
    pas_segregated_page_config_kind page_config_kind)
{
    pas_exclusive_view_template_memo_key result;
    result.object_size = object_size;
    result.page_config_kind = page_config_kind;
    return result;
}

typedef pas_segregated_global_size_directory* pas_exclusive_view_template_memo_entry;

static inline pas_exclusive_view_template_memo_entry
pas_exclusive_view_template_memo_entry_create_empty(void)
{
    return NULL;
}

static inline pas_exclusive_view_template_memo_entry
pas_exclusive_view_template_memo_entry_create_deleted(void)
{
    return (pas_exclusive_view_template_memo_entry)(uintptr_t)1;
}

static inline bool
pas_exclusive_view_template_memo_entry_is_empty_or_deleted(
    pas_exclusive_view_template_memo_entry entry)
{
    return (uintptr_t)entry <= (uintptr_t)1;
}

static inline bool
pas_exclusive_view_template_memo_entry_is_empty(
    pas_exclusive_view_template_memo_entry entry)
{
    return !entry;
}

static inline bool
pas_exclusive_view_template_memo_entry_is_deleted(
    pas_exclusive_view_template_memo_entry entry)
{
    return entry == pas_exclusive_view_template_memo_entry_create_deleted();
}

static inline pas_exclusive_view_template_memo_key
pas_exclusive_view_template_memo_entry_get_key(
    pas_exclusive_view_template_memo_entry entry)
{
    return pas_exclusive_view_template_memo_key_create(
        entry->object_size,
        entry->base.page_config_kind);
}

static inline unsigned
pas_exclusive_view_template_memo_key_get_hash(
    pas_exclusive_view_template_memo_key key)
{
    return (unsigned)((uintptr_t)key.page_config_kind ^ pas_hash32(key.object_size));
}

static inline bool
pas_exclusive_view_template_memo_key_is_equal(
    pas_exclusive_view_template_memo_key a,
    pas_exclusive_view_template_memo_key b)
{
    return a.object_size == b.object_size
        && a.page_config_kind == b.page_config_kind;
}

/* This uses the bootstrap heap because this can be used from utility. */
PAS_CREATE_HASHTABLE(pas_exclusive_view_template_memo_table,
                     pas_exclusive_view_template_memo_entry,
                     pas_exclusive_view_template_memo_key);

PAS_API extern pas_exclusive_view_template_memo_table pas_exclusive_view_template_memo_table_instance;

PAS_END_EXTERN_C;

#endif /* PAS_EXCLUSIVE_VIEW_TEMPLATE_MEMO_TABLE_H */

