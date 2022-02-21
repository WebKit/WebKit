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

#ifndef PAS_BITFIT_VIEW_H
#define PAS_BITFIT_VIEW_H

#include "pas_bitfit_page_config.h"
#include "pas_compact_bitfit_directory_ptr.h"
#include "pas_heap_summary.h"
#include "pas_lock.h"

PAS_BEGIN_EXTERN_C;

struct pas_bitfit_directory;
struct pas_bitfit_page;
struct pas_bitfit_view;
typedef struct pas_bitfit_directory pas_bitfit_directory;
typedef struct pas_bitfit_page pas_bitfit_page;
typedef struct pas_bitfit_view pas_bitfit_view;

struct pas_bitfit_view {
    void* page_boundary;
    pas_compact_bitfit_directory_ptr directory;
    bool is_owned;
    unsigned index;
    pas_lock ownership_lock;
    pas_lock commit_lock;
};

PAS_API pas_bitfit_view* pas_bitfit_view_create(pas_bitfit_directory* directory,
                                                unsigned index);

PAS_API void pas_bitfit_view_note_nonemptiness(pas_bitfit_view* view);
PAS_API void pas_bitfit_view_note_full_emptiness(pas_bitfit_view* view, pas_bitfit_page* page);
PAS_API void pas_bitfit_view_note_partial_emptiness(pas_bitfit_view* view, pas_bitfit_page* page);
PAS_API void pas_bitfit_view_note_max_free(pas_bitfit_view* view);

PAS_API pas_heap_summary pas_bitfit_view_compute_summary(pas_bitfit_view* view);

typedef bool (*pas_bitfit_view_for_each_live_object_callback)(
    pas_bitfit_view* view,
    uintptr_t begin,
    size_t size,
    void* arg);

PAS_API bool pas_bitfit_view_for_each_live_object(
    pas_bitfit_view* view,
    pas_bitfit_view_for_each_live_object_callback callback,
    void* arg);

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_VIEW_H */

