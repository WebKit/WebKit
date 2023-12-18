/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#include "BExport.h"
#include <atomic>

#define BFOR_EACH_ALLOCATION_KIND(macro) \
    macro(JS_CELL)      /* Allocation of any JSCell */ \
    macro(NON_JS_CELL)  /* Allocation of any non-JSCell */ \
    macro(GIGACAGE)     /* Allocation within a gigacage, takes heap kind as first parameter */ \
    macro(COMPACTIBLE)  /* Pointers to this allocation are allowed to be stored compact. */

#define BPROFILE_ALLOCATION(kind, ...) \
    BPROFILE_ALLOCATION_ ## kind(__VA_ARGS__)

#define BPROFILE_TRY_ALLOCATION(kind, ...) \
    BPROFILE_TRY_ALLOCATION_ ## kind(__VA_ARGS__)

// Definitions of specializations of the above macro (i.e. BPROFILE_ALLOCATION_JS_CELL(ptr, size))
// may be provided in an AllocationCountsAdditions.h header to add custom profiling code
// at an allocation site, taking both the pointer variable (which may be modified) and the
// size of the allocation in bytes.

#if __has_include(<WebKitAdditions/AllocationCountsAdditions.h>)
#include <WebKitAdditions/AllocationCountsAdditions.h>
#elif __has_include(<AllocationCountsAdditions.h>)
#include <AllocationCountsAdditions.h>
#endif

// If allocation profiling macros weren't defined above, we define them below as no-ops.
// Additionally, BENABLE(PROFILE_<type>_ALLOCATION) can be queried to see if we are profiling
// allocations of a specific kind.

#ifdef BPROFILE_ALLOCATION_JS_CELL
#define BENABLE_PROFILE_JS_CELL_ALLOCATION 1
#else
#define BENABLE_PROFILE_JS_CELL_ALLOCATION 0
#define BPROFILE_ALLOCATION_JS_CELL(ptr, size) do { } while (false)
#endif

#ifdef BPROFILE_TRY_ALLOCATION_JS_CELL
#define BENABLE_PROFILE_JS_CELL_TRY_ALLOCATION 1
#else
#define BENABLE_PROFILE_JS_CELL_TRY_ALLOCATION 0
#define BPROFILE_TRY_ALLOCATION_JS_CELL(ptr, size) do { } while (false)
#endif

#ifdef BPROFILE_ALLOCATION_NON_JS_CELL
#define BENABLE_PROFILE_NON_JS_CELL_ALLOCATION 1
#else
#define BENABLE_PROFILE_NON_JS_CELL_ALLOCATION 0
#define BPROFILE_ALLOCATION_NON_JS_CELL(ptr, size) do { } while (false)
#endif

#ifdef BPROFILE_TRY_ALLOCATION_NON_JS_CELL
#define BENABLE_PROFILE_NON_JS_CELL_TRY_ALLOCATION 1
#else
#define BENABLE_PROFILE_NON_JS_CELL_TRY_ALLOCATION 0
#define BPROFILE_TRY_ALLOCATION_NON_JS_CELL(ptr, size) do { } while (false)
#endif

#ifdef BPROFILE_ALLOCATION_GIGACAGE
#define BENABLE_PROFILE_GIGACAGE_ALLOCATION 1
#else
#define BENABLE_PROFILE_GIGACAGE_ALLOCATION 0
#define BPROFILE_ALLOCATION_GIGACAGE(kind, ptr, size) do { } while (false)
#endif

#ifdef BPROFILE_TRY_ALLOCATION_GIGACAGE
#define BENABLE_PROFILE_GIGACAGE_TRY_ALLOCATION 1
#else
#define BENABLE_PROFILE_GIGACAGE_TRY_ALLOCATION 0
#define BPROFILE_TRY_ALLOCATION_GIGACAGE(kind, ptr, size) do { } while (false)
#endif

#ifdef BPROFILE_ALLOCATION_COMPACTIBLE
#define BENABLE_PROFILE_COMPACTIBLE_ALLOCATION 1
#else
#define BENABLE_PROFILE_COMPACTIBLE_ALLOCATION 0
#define BPROFILE_ALLOCATION_COMPACTIBLE(ptr, size) do { } while (false)
#endif

#ifdef BPROFILE_TRY_ALLOCATION_COMPACTIBLE
#define BENABLE_PROFILE_COMPACTIBLE_TRY_ALLOCATION 1
#else
#define BENABLE_PROFILE_COMPACTIBLE_TRY_ALLOCATION 0
#define BPROFILE_TRY_ALLOCATION_COMPACTIBLE(ptr, size) do { } while (false)
#endif
