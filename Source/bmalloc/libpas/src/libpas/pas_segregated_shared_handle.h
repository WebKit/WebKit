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

#ifndef PAS_SEGREGATED_SHARED_HANDLE_H
#define PAS_SEGREGATED_SHARED_HANDLE_H

#include "pas_compact_atomic_segregated_partial_view_ptr.h"
#include "pas_compact_segregated_shared_view_ptr.h"
#include "pas_utils.h"
#include "pas_segregated_page_config.h"
#include "pas_segregated_view.h"

PAS_BEGIN_EXTERN_C;

struct pas_segregated_page;
struct pas_segregated_partial_view;
struct pas_segregated_shared_handle;
struct pas_segregated_shared_view;
typedef struct pas_segregated_page pas_segregated_page;
typedef struct pas_segregated_partial_view pas_segregated_partial_view;
typedef struct pas_segregated_shared_handle pas_segregated_shared_handle;
typedef struct pas_segregated_shared_view pas_segregated_shared_view;

struct pas_segregated_shared_handle {
    void* page_boundary;
    pas_segregated_shared_page_directory* directory;
    pas_compact_segregated_shared_view_ptr shared_view;

    /* We maintain the invariant that each bit index in the alloc bits bitvector can be resolved to
       a sharing granule by downshifting by page_config->sharing_shift, such that each sharing granule
       has at most one partial view.
       
       We guarantee this by saying that once a partial view plants an alloc bit, it keeps claiming
       object until the bump pointer gets to another sharing granule. For example, if the sharing_shift
       is 1 and we allocate a 3-byte object, then we will claim the first two sharing granules (0 and
       1) and place the bump pointer at 3, which is still inside a sharing granule we have already
       claimed (1). So, we will bump the pointer again to 6, and claim another sharing granule (2),
       giving that partial view an extra object.
    
       Note that this has a goofy locking protocol:
    
       - We hold both the commit and ownership locks when adding a partial view to its *first*
         entry in this array. That first entry will be first both chronologically and it will have
         the smallest index.
    
       - We hold only the ownership lock when adding a partial view to additional entries in the
         list.
    
       - We never remove things from this list and we hold the ownership lock when destroying the
         handle. Handle destruction requires the page to be empty, so for example if you know you
         are freeing an object but haven't signaled emptiness yet, then you know that this cannot
         get deleted. Also, to delete it, the ownership lock is flashed, so if you hold the
         ownership lock and establish that the shared_view->is_owned, then you know that this list
         cannot change.
    
       You can scan this list to find all of the partial views, but that gets weird. If you hold
       the commit lock, you can use partial_view->noted_in_scan to track which views you've already
       seen. But that requires holding the commit lock. If you only hold the ownership lock then
       you can still loop over this, and you can even rely on it not changing, but you have to
       come up with your own way to ensure you don't see the same thing more than once. */
    pas_compact_atomic_segregated_partial_view_ptr partial_views[1];
};

#define PAS_SEGREGATED_SHARED_HANDLE_BASE_SIZE \
    PAS_OFFSETOF(pas_segregated_shared_handle, partial_views)

#define PAS_SEGREGATED_SHARED_HANDLE_NUM_VIEWS(num_alloc_bits, sharing_shift) \
    ((num_alloc_bits) >> (sharing_shift))

#define PAS_SEGREGATED_SHARED_HANDLE_SIZE(num_alloc_bits, sharing_shift) \
    (PAS_SEGREGATED_SHARED_HANDLE_BASE_SIZE + \
     PAS_ROUND_UP_TO_POWER_OF_2( \
         PAS_SEGREGATED_SHARED_HANDLE_NUM_VIEWS((num_alloc_bits), (sharing_shift)) * \
         sizeof(pas_compact_atomic_segregated_partial_view_ptr), \
         sizeof(uint64_t)))

static PAS_ALWAYS_INLINE size_t
pas_segregated_shared_handle_num_views(pas_segregated_page_config page_config)
{
    return PAS_SEGREGATED_SHARED_HANDLE_NUM_VIEWS(page_config.num_alloc_bits, page_config.sharing_shift);
}

static PAS_ALWAYS_INLINE size_t
pas_segregated_shared_handle_size(pas_segregated_page_config page_config)
{
    return PAS_SEGREGATED_SHARED_HANDLE_SIZE(page_config.num_alloc_bits, page_config.sharing_shift);
}

static inline pas_segregated_view
pas_segregated_shared_handle_as_view(pas_segregated_shared_handle* handle)
{
    return pas_segregated_view_create(handle, pas_segregated_shared_handle_kind);
}

static inline pas_segregated_view
pas_segregated_shared_handle_as_view_non_null(pas_segregated_shared_handle* handle)
{
    return pas_segregated_view_create_non_null(handle, pas_segregated_shared_handle_kind);
}

/* This installs the handle in the shared_view. Only valid to call if we're holding both the view's
   commit lock and the heap lock. */
PAS_API pas_segregated_shared_handle* pas_segregated_shared_handle_create(
    pas_segregated_shared_view* view,
    pas_segregated_shared_page_directory* directory,
    const pas_segregated_page_config* page_config);

/* This uninstalls the handle from the shared_view. */
PAS_API void pas_segregated_shared_handle_destroy(pas_segregated_shared_handle* handle);

PAS_API void pas_segregated_shared_handle_note_emptiness(
    pas_segregated_shared_handle* handle);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_SHARED_HANDLE_H */

