/*
 * Copyright (c) 2025-2026 Apple Inc. All rights reserved.
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

#ifndef PAS_MTE_CONFIG_H
#define PAS_MTE_CONFIG_H

#include "pas_platform.h"
#include "pas_runtime_config.h"
#include "pas_config.h"
#if defined(PAS_BMALLOC)
#include "BPlatform.h"
#endif

#if defined(__has_include)
#if __has_include(<WebKitAdditions/pas_mte_additions.h>)
// FIXME: Properly support using WKA in modules.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-modular-include-in-module"
#include <WebKitAdditions/pas_mte_additions.h>
#pragma clang diagnostic pop
#endif // __has_include(<WebKitAdditions/pas_mte_additions.h>)
#if __has_include(<libproc.h>)
#include <libproc.h>
#endif // __has_include(<libproc.h>)
#endif // defined(__has_include)

#include <stddef.h>
#include <stdint.h>

#ifdef __APPLE__
#include <Availability.h>
#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
#endif
#if PAS_OS(DARWIN)
#include <dispatch/dispatch.h>
#if PAS_USE_APPLE_INTERNAL_SDK
#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#include <mach/vm_page_size.h>
#include <mach/vm_statistics.h>
#endif // PAS_USE_APPLE_INTERNAL_SDK
#endif // PAS_OS(DARWIN)

#if defined(PAS_USE_OPENSOURCE_MTE) && PAS_USE_OPENSOURCE_MTE
#if PAS_ENABLE_MTE

#define PAS_USE_MTE (PAS_RUNTIME_CONFIG_PTR->enabled)
#ifndef PAS_USE_MTE_IN_WEBCONTENT
#define PAS_USE_MTE_IN_WEBCONTENT 1
#endif

#define PAS_MTE_MEDIUM_TAGGING_ENABLED (PAS_RUNTIME_CONFIG_PTR->medium_tagging_enabled)
#define PAS_MTE_IS_LOCKDOWN_MODE (PAS_RUNTIME_CONFIG_PTR->is_lockdown_mode)
#define PAS_MTE_IS_HARDENED (PAS_RUNTIME_CONFIG_PTR->is_hardened)
#define PAS_MTE_USE_LARGE_OBJECT_DELEGATION (PAS_USE_MTE && PAS_MTE_IS_HARDENED)

#define PAS_VM_MTE 0x2000
#define PAS_MTE_PROC_FLAG_SEC_ENABLED 0x4000000

#define PAS_MTE_SHOULD_STORE_TAG 1

/*
 * This setting would force all non-compact TZone allocations into a single bucket.
 * Normally this would be a security regression, as it effectively bypasses the
 * iso-heap mechanism that TZone relies on for its security guarantees.
 * Normally, this would be a security regression, as it effectively removes the
 * randomness at the heart of the TZone security feature by putting all classes
 * from the same TZone into a single iso-heap.

 * However, MTE provides the same security benefits as TZone, and as such it's
 * OK to bypass TZone for objects we know will be MTE-tagged.
 * Presently, the main reason for doing so would be performance, but as MTE
 * is currently (c. 2026) only enabled in non-performant processes, there's no
 * reason to have it on. If it is re-enabled it should be set to PAS_USE_MTE
 * so as to preserve the security properties of non-MTE processes.
 *
 * Astute observers may notice that in bmalloc we do the converse, i.e. allocating
 * always-compact objects from a single heap-ref. This is OK since in that heap,
 * we already expect all allocations to come out of the same singular intrinsic
 * heap.
 */
#define PAS_BYPASS_TZONE_FOR_NONCOMPACT_OBJECTS 0

#define PAS_MTE_FEATURE_RETAG_ON_SCAVENGE 0
#define PAS_MTE_FEATURE_LOG_ON_TAG 1
#define PAS_MTE_FEATURE_LOG_ON_PURIFY 2
#define PAS_MTE_FEATURE_LOG_PAGE_ALLOC 3
#define PAS_MTE_FEATURE_ZERO_TAG_ALL 4
// ATE and PTE are always enabled together
#define PAS_MTE_FEATURE_ADJACENT_TAG_EXCLUSION 5
#define PAS_MTE_FEATURE_PREVIOUS_TAG_EXCLUSION PAS_MTE_FEATURE_ADJACENT_TAG_EXCLUSION
#define PAS_MTE_FEATURE_ASSERT_ADJACENT_TAGS_ARE_DISJOINT 6
#define PAS_MTE_FEATURE_CHECK_TAG_ON_DEALLOC 7

// Helper to access feature bits by index (for dynamic feature checking)
#define PAS_MTE_FEATURE_BIT(feature) ( \
    (feature) == PAS_MTE_FEATURE_RETAG_ON_SCAVENGE ? PAS_RUNTIME_CONFIG_PTR->mode_bits.retag_on_scavenge : \
    (feature) == PAS_MTE_FEATURE_LOG_ON_TAG ? PAS_RUNTIME_CONFIG_PTR->mode_bits.log_on_tag : \
    (feature) == PAS_MTE_FEATURE_LOG_ON_PURIFY ? PAS_RUNTIME_CONFIG_PTR->mode_bits.log_on_purify : \
    (feature) == PAS_MTE_FEATURE_LOG_PAGE_ALLOC ? PAS_RUNTIME_CONFIG_PTR->mode_bits.log_page_alloc : \
    (feature) == PAS_MTE_FEATURE_ZERO_TAG_ALL ? PAS_RUNTIME_CONFIG_PTR->mode_bits.zero_tag_all : \
    (feature) == PAS_MTE_FEATURE_ADJACENT_TAG_EXCLUSION ? PAS_RUNTIME_CONFIG_PTR->mode_bits.adjacent_tag_exclusion : \
    (feature) == PAS_MTE_FEATURE_ASSERT_ADJACENT_TAGS_ARE_DISJOINT ? PAS_RUNTIME_CONFIG_PTR->mode_bits.assert_adjacent_tags_are_disjoint : \
    (feature) == PAS_MTE_FEATURE_CHECK_TAG_ON_DEALLOC ? PAS_RUNTIME_CONFIG_PTR->mode_bits.check_tag_on_dealloc : \
    0)

#define PAS_MTE_FEATURE_FORCED(feature) (0)
#define PAS_MTE_FEATURE_HARDENED_FORCED(feature) (feature == PAS_MTE_FEATURE_ADJACENT_TAG_EXCLUSION || feature == PAS_MTE_FEATURE_RETAG_ON_SCAVENGE)
#define PAS_MTE_FEATURE_DEBUG_FORCED(feature) (feature == PAS_MTE_FEATURE_ASSERT_ADJACENT_TAGS_ARE_DISJOINT || feature == PAS_MTE_FEATURE_RETAG_ON_SCAVENGE || feature == PAS_MTE_FEATURE_CHECK_TAG_ON_DEALLOC)

#define PAS_MTE_FEATURE_FORCED_IN_RELEASE_BUILD(feature) \
    (PAS_MTE_FEATURE_FORCED(feature) || \
     (PAS_MTE_FEATURE_HARDENED_FORCED(feature) && PAS_MTE_IS_HARDENED))

#define PAS_MTE_FEATURE_FORCED_IN_DEBUG_BUILD(feature) \
    (PAS_MTE_FEATURE_FORCED_IN_RELEASE_BUILD(feature) || \
     PAS_MTE_FEATURE_DEBUG_FORCED(feature) || \
     PAS_MTE_FEATURE_BIT(feature))

#ifndef NDEBUG
#define PAS_MTE_FEATURE_ENABLED(feature) (PAS_USE_MTE && PAS_MTE_FEATURE_FORCED_IN_DEBUG_BUILD(feature))
#else
#define PAS_MTE_FEATURE_ENABLED(feature) (PAS_USE_MTE && PAS_MTE_FEATURE_FORCED_IN_RELEASE_BUILD(feature))
#endif

// FIXME: rdar://171662605
#define PAS_WORKAROUND_RDAR_171662605_UNCONDITIONAL_TAG_ON_ALLOC (1)

/*
 * These are defined here rather than in pas_mte.h because they are needed by
 * pas_zero_memory.h, which is a transitive depencency of pas_mte.h
 */
#define PAS_MTE_CHECK_TAG_AND_SET_TCO(ptr) do { \
        /* We're only checking one tag-granule, so it's not perfect, \
         * but it does mean that a potential attacker would at least \
         * need to know the tag for some of their target range. */ \
        __asm__ volatile( \
            ".arch_extension memtag\n\t" \
            "ldr xzr, [%0]\n\t" \
            "msr tco, #1" \
            : \
            : "r"(ptr) \
            : "memory" \
        ); \
    } while (0)
#define PAS_MTE_SET_TCO_UNCHECKED do { \
        __asm__ volatile( \
            ".arch_extension memtag\n\t" \
            "msr tco, #1" \
            : \
            : \
            : "memory" \
        ); \
    } while (0)
#define PAS_MTE_CLEAR_TCO do { \
        __asm__ volatile( \
            ".arch_extension memtag\n\t" \
            "msr tco, #0" \
            : \
            : \
            : "memory" \
        ); \
    } while (0)

#else // !PAS_ENABLE_MTE
#define PAS_USE_MTE (0)
#define PAS_USE_MTE_IN_WEBCONTENT (0)
#define PAS_MTE_FEATURE_ENABLED(feature) (0)
#define PAS_MTE_USE_LARGE_OBJECT_DELEGATION (0)
#define PAS_MTE_CHECK_TAG_AND_SET_TCO(ptr) do { (void)ptr; } while (0)
#define PAS_MTE_SET_TCO_UNCHECKED do { } while (0)
#define PAS_MTE_CLEAR_TCO do { } while (0)
#define PAS_BYPASS_TZONE_FOR_NONCOMPACT_OBJECTS 0
#endif // PAS_ENABLE_MTE

#ifdef __cplusplus
extern "C" {
#endif
bool pas_mte_is_mte_enabled(void);
void pas_mte_ensure_initialized(void);
void pas_mte_force_nontaggable_user_allocations_into_large_heap(void);
void pas_bmalloc_force_allocations_into_bitfit_heaps_where_available(void);
#ifdef __cplusplus
}
#endif

#define PAS_MTE_INITIALIZE_IN_WTF_CONFIG \
    pas_mte_ensure_initialized()

#if defined(PAS_BMALLOC)
#if BENABLE(LIBPAS)
#if BENABLE_MTE != PAS_ENABLE_MTE
#error "cannot enable MTE in libpas without enabling it in bmalloc, or vice versa"
#endif // BENABLE(LIBPAS)
#endif // defined(PAS_BMALLOC)

#define BMALLOC_VM_MTE PAS_VM_MTE
#define BMALLOC_USE_MTE PAS_USE_MTE

#endif // defined(PAS_BMALLOC) && BENABLE(LIBPAS)
#endif // defined(PAS_USE_OPENSOURCE_MTE) && PAS_USE_OPENSOURCE_MTE
#endif // PAS_MTE_CONFIG_H
