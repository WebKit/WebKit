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

#ifndef PAS_SEGREGATED_VIEW_H
#define PAS_SEGREGATED_VIEW_H

#include "pas_heap_summary.h"
#include "pas_lock.h"
#include "pas_log.h"
#include "pas_range.h"
#include "pas_segregated_page_config_kind.h"
#include "pas_segregated_view_kind.h"
#include "pas_tri_state.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* The segregated view is a polymorphic pointer. We use polymorphic pointers a lot, so this has
   a lot of things it can point to.
   
   It's called a view because what all of the things we point to using views have in common is that
   they are views of pages or some way of tracking page meta-data. */

struct pas_heap_config;
struct pas_magazine;
struct pas_segregated_directory;
struct pas_segregated_exclusive_view;
struct pas_segregated_page;
struct pas_segregated_page_config;
struct pas_segregated_partial_view;
struct pas_segregated_shared_handle;
struct pas_segregated_shared_page_directory;
struct pas_segregated_shared_view;
struct pas_segregated_size_directory;
struct pas_segregated_view_opaque;
typedef struct pas_heap_config pas_heap_config;
typedef struct pas_magazine pas_magazine;
typedef struct pas_segregated_directory pas_segregated_directory;
typedef struct pas_segregated_exclusive_view pas_segregated_exclusive_view;
typedef struct pas_segregated_page pas_segregated_page;
typedef struct pas_segregated_page_config pas_segregated_page_config;
typedef struct pas_segregated_partial_view pas_segregated_partial_view;
typedef struct pas_segregated_shared_handle pas_segregated_shared_handle;
typedef struct pas_segregated_shared_page_directory pas_segregated_shared_page_directory;
typedef struct pas_segregated_shared_view pas_segregated_shared_view;
typedef struct pas_segregated_size_directory pas_segregated_size_directory;
typedef struct pas_segregated_view_opaque* pas_segregated_view;

/* NOTE: it's an antipattern to use this function directly when you could have used the much more
   type-safe things like pas_segregated_exclusive_view_as_view. */
static inline pas_segregated_view pas_segregated_view_create(void* ptr,
                                                             pas_segregated_view_kind kind)
{
    if (!ptr)
        return NULL;
    return (pas_segregated_view)((uintptr_t)ptr | (uintptr_t)kind);
}

static inline pas_segregated_view pas_segregated_view_create_non_null(void* ptr,
                                                                      pas_segregated_view_kind kind)
{
    PAS_TESTING_ASSERT(ptr);
    return (pas_segregated_view)((uintptr_t)ptr | (uintptr_t)kind);
}

static inline void* pas_segregated_view_get_ptr(pas_segregated_view view)
{
    return (void*)((uintptr_t)view & ~PAS_SEGREGATED_VIEW_KIND_MASK);
}

static inline pas_segregated_view_kind pas_segregated_view_get_kind(pas_segregated_view view)
{
    return (pas_segregated_view_kind)((uintptr_t)view & PAS_SEGREGATED_VIEW_KIND_MASK);
}

static inline bool pas_segregated_view_is_null(pas_segregated_view view)
{
    return !view;
}

static inline bool pas_segregated_view_is_exclusive(pas_segregated_view view)
{
    return !pas_segregated_view_is_null(view)
        && pas_segregated_view_get_kind(view) == pas_segregated_exclusive_view_kind;
}

static inline bool pas_segregated_view_is_ineligible_exclusive(pas_segregated_view view)
{
    return pas_segregated_view_get_kind(view) == pas_segregated_ineligible_exclusive_view_kind;
}

static inline bool pas_segregated_view_is_some_exclusive(pas_segregated_view view)
{
    return pas_segregated_view_kind_is_some_exclusive(pas_segregated_view_get_kind(view));
}

static inline pas_segregated_view pas_segregated_view_as_ineligible(pas_segregated_view view)
{
    pas_segregated_view result;
    PAS_TESTING_ASSERT(pas_segregated_view_is_some_exclusive(view));
    result = (pas_segregated_view)(
        (uintptr_t)view | (uintptr_t)pas_segregated_ineligible_exclusive_view_kind);
    PAS_TESTING_ASSERT(pas_segregated_view_get_ptr(view) == pas_segregated_view_get_ptr(result));
    PAS_TESTING_ASSERT(pas_segregated_view_is_some_exclusive(view));
    return result;
}

static inline pas_segregated_view pas_segregated_view_as_eligible(pas_segregated_view view)
{
    pas_segregated_view result;
    PAS_TESTING_ASSERT(pas_segregated_view_is_some_exclusive(view));
    result = (pas_segregated_view)(
        (uintptr_t)view & ~(uintptr_t)pas_segregated_ineligible_exclusive_view_kind);
    PAS_TESTING_ASSERT(pas_segregated_view_get_ptr(view) == pas_segregated_view_get_ptr(result));
    PAS_TESTING_ASSERT(pas_segregated_view_is_some_exclusive(view));
    return result;
}

static inline pas_segregated_exclusive_view* pas_segregated_view_get_exclusive(pas_segregated_view view)
{
    PAS_TESTING_ASSERT(pas_segregated_view_is_some_exclusive(view));
    return (pas_segregated_exclusive_view*)pas_segregated_view_get_ptr(view);
}

static inline bool pas_segregated_view_is_shared(pas_segregated_view view)
{
    return pas_segregated_view_get_kind(view) == pas_segregated_shared_view_kind;
}

static inline pas_segregated_shared_view* pas_segregated_view_get_shared(pas_segregated_view view)
{
    PAS_ASSERT(pas_segregated_view_is_shared(view));
    return (pas_segregated_shared_view*)pas_segregated_view_get_ptr(view);
}

static inline bool pas_segregated_view_is_shared_handle(pas_segregated_view view)
{
    return pas_segregated_view_get_kind(view) == pas_segregated_shared_handle_kind;
}

static inline pas_segregated_shared_handle*
pas_segregated_view_get_shared_handle(pas_segregated_view view)
{
    PAS_ASSERT(pas_segregated_view_is_shared_handle(view));
    return (pas_segregated_shared_handle*)pas_segregated_view_get_ptr(view);
}

static inline bool pas_segregated_view_is_partial(pas_segregated_view view)
{
    return pas_segregated_view_get_kind(view) == pas_segregated_partial_view_kind;
}

static inline pas_segregated_partial_view* pas_segregated_view_get_partial(pas_segregated_view view)
{
    PAS_ASSERT(pas_segregated_view_is_partial(view));
    return (pas_segregated_partial_view*)pas_segregated_view_get_ptr(view);
}

static inline bool pas_segregated_view_is_size_directory(pas_segregated_view view)
{
    return pas_segregated_view_get_kind(view) == pas_segregated_size_directory_view_kind;
}

PAS_API pas_segregated_size_directory*
pas_segregated_view_get_size_directory_slow(pas_segregated_view view);

static inline pas_segregated_size_directory*
pas_segregated_view_get_size_directory(pas_segregated_view view)
{
    if (pas_segregated_view_is_size_directory(view))
        return (pas_segregated_size_directory*)pas_segregated_view_get_ptr(view);
    return pas_segregated_view_get_size_directory_slow(view);
}

PAS_API pas_segregated_page_config_kind
pas_segregated_view_get_page_config_kind(pas_segregated_view view);
PAS_API pas_segregated_page_config* pas_segregated_view_get_page_config(pas_segregated_view view);

static inline pas_segregated_page_role pas_segregated_view_get_page_role_for_owner(pas_segregated_view view)
{
    return pas_segregated_view_kind_get_role_for_owner(pas_segregated_view_get_kind(view));
}

static inline pas_segregated_page_role pas_segregated_view_get_page_role_for_allocator(pas_segregated_view view)
{
    return pas_segregated_view_kind_get_role_for_allocator(pas_segregated_view_get_kind(view));
}

PAS_API size_t pas_segregated_view_get_index(pas_segregated_view view);

PAS_API void* pas_segregated_view_get_page_boundary(pas_segregated_view view);
PAS_API pas_segregated_page* pas_segregated_view_get_page(pas_segregated_view view);

/* It's only OK to call this when you have ownership of the page, like if you made it ineligible or
   grabbed the ownership lock. Of course, if you grabbed the ownership lock then you cannot grab the
   commit lock, so hopefully you got here by making the page ineligible. */
PAS_API pas_lock* pas_segregated_view_get_commit_lock(pas_segregated_view view);

/* It's only OK to call this if you already hold the ownership lock or the view is ineligible. */
PAS_API pas_lock* pas_segregated_view_get_ownership_lock(pas_segregated_view view);

PAS_API bool pas_segregated_view_is_owned(pas_segregated_view view);

/* Locks the ownership lock. */
PAS_API void pas_segregated_view_lock_ownership_lock(pas_segregated_view view);
PAS_API void pas_segregated_view_lock_ownership_lock_conditionally(pas_segregated_view view,
                                                                   pas_lock_hold_mode lock_hold_mode);
/* Locks the ownership lock and tells you if the view is owned. If it isn't owned it also unlocks it. */
PAS_API bool pas_segregated_view_lock_ownership_lock_if_owned(pas_segregated_view view);
PAS_API bool pas_segregated_view_lock_ownership_lock_if_owned_conditionally(
    pas_segregated_view view,
    pas_lock_hold_mode lock_hold_mode);

PAS_API void pas_segregated_view_unlock_ownership_lock(pas_segregated_view view);
PAS_API void pas_segregated_view_unlock_ownership_lock_conditionally(pas_segregated_view view,
                                                                     pas_lock_hold_mode lock_hold_mode);

/* This is only valid to call on a partial view that was just made ineligible. */
PAS_API bool pas_segregated_view_is_primordial_partial(pas_segregated_view view);

PAS_API void pas_segregated_view_note_emptiness(
    pas_segregated_view view,
    pas_segregated_page* page);

typedef bool (*pas_segregated_view_for_each_live_object_callback)(
    pas_segregated_view view,
    pas_range range,
    void* arg);

/* It's best to call this without holding locks. This will grab the ownership lock of the view to
   prevent the view from being decommitted.
   
   It's sort of legal to call this with the page lock held and pretend to hold the ownership lock.
   That's fine if you have some other way of preventing the view from going away. */
PAS_API bool pas_segregated_view_for_each_live_object(
    pas_segregated_view view,
    pas_segregated_view_for_each_live_object_callback callback,
    void *arg,
    pas_lock_hold_mode ownership_lock_hold_mode);

/* Tells us if the eligible bit should be set. Note that for shared views/handles, this may return
   "maybe", since those are never in a state where they *cannot* be eligible. This can only be called
   during steady-state (no concurrent allocations or deallocations). */
PAS_API pas_tri_state pas_segregated_view_should_be_eligible(pas_segregated_view view,
                                                             pas_segregated_page_config* page_config);

PAS_API pas_segregated_view pas_segregated_view_for_object(
    uintptr_t begin,
    pas_heap_config* config);

PAS_API pas_heap_summary pas_segregated_view_compute_summary(
    pas_segregated_view view,
    pas_segregated_page_config* page_config);

PAS_API bool pas_segregated_view_is_eligible(pas_segregated_view view);
PAS_API bool pas_segregated_view_is_payload_empty(pas_segregated_view view);
PAS_API bool pas_segregated_view_is_empty(pas_segregated_view view);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_VIEW_H */

