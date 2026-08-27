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

#include <stdbool.h>
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

#ifdef __cplusplus
extern "C" {
#endif

#define PAS_VM_MTE 0x2000
#define PAS_MTE_PROC_FLAG_SEC_ENABLED 0x4000000

#define PAS_MTE_SHOULD_STORE_TAG 1

/*
 * The values double as bit positions within the mode value parsed by
 * pas_mte_config.c, so they must be explicit and must stay contiguous.
 */
typedef enum pas_mte_feature {
    pas_mte_feature_retag_on_free = 0,
    pas_mte_feature_log_on_tag,
    pas_mte_feature_log_on_purify,
    pas_mte_feature_log_page_alloc = 3,
    pas_mte_feature_zero_tag_all = 4,
    pas_mte_feature_adjacent_tag_exclusion = 5,
    pas_mte_feature_assert_adjacent_tags_are_disjoint = 6,
    pas_mte_feature_check_tag_on_dealloc = 7,
    pas_mte_feature_large_object_delegation = 8,
    pas_mte_feature_previous_tag_exclusion = 9
} pas_mte_feature;

/*
 * Regardless of this condition, features also enabled via a debug-only runtime
 * bit (see pas_runtime_config.h pas_runtime_config.mode_bits).
 */
typedef enum pas_mte_use_feature_condition {
    pas_mte_use_feature_never,
    pas_mte_use_feature_debug,
    pas_mte_use_feature_hardened,
    pas_mte_use_feature_always
} pas_mte_use_feature_condition;

/* Helper to access feature bits by index (for dynamic feature checking) */
#define PAS_MTE_FEATURE_BIT(feature) ( \
    (feature) == pas_mte_feature_retag_on_free ? PAS_RUNTIME_CONFIG_PTR->mode_bits.retag_on_free : \
    (feature) == pas_mte_feature_log_on_tag ? PAS_RUNTIME_CONFIG_PTR->mode_bits.log_on_tag : \
    (feature) == pas_mte_feature_log_on_purify ? PAS_RUNTIME_CONFIG_PTR->mode_bits.log_on_purify : \
    (feature) == pas_mte_feature_log_page_alloc ? PAS_RUNTIME_CONFIG_PTR->mode_bits.log_page_alloc : \
    (feature) == pas_mte_feature_zero_tag_all ? PAS_RUNTIME_CONFIG_PTR->mode_bits.zero_tag_all : \
    (feature) == pas_mte_feature_adjacent_tag_exclusion ? PAS_RUNTIME_CONFIG_PTR->mode_bits.adjacent_tag_exclusion : \
    (feature) == pas_mte_feature_previous_tag_exclusion ? PAS_RUNTIME_CONFIG_PTR->mode_bits.previous_tag_exclusion : \
    (feature) == pas_mte_feature_assert_adjacent_tags_are_disjoint ? PAS_RUNTIME_CONFIG_PTR->mode_bits.assert_adjacent_tags_are_disjoint : \
    (feature) == pas_mte_feature_check_tag_on_dealloc ? PAS_RUNTIME_CONFIG_PTR->mode_bits.check_tag_on_dealloc : \
    (feature) == pas_mte_feature_large_object_delegation ? PAS_RUNTIME_CONFIG_PTR->mode_bits.large_object_delegation : \
    0)

inline __attribute__((always_inline)) bool
pas_use_mte(void)
{
#if PAS_ENABLE_MTE
    return (PAS_RUNTIME_CONFIG_PTR->enabled);
#else
    return false;
#endif
}

inline __attribute__((always_inline)) bool
pas_mte_is_hardened(void)
{
    return pas_use_mte() && PAS_RUNTIME_CONFIG_PTR->is_hardened;
}

inline __attribute__((always_inline)) pas_mte_use_feature_condition
pas_mte_feature_use_condition(pas_mte_feature feature)
{
    switch (feature) {
    case pas_mte_feature_adjacent_tag_exclusion:
    case pas_mte_feature_previous_tag_exclusion:
    case pas_mte_feature_retag_on_free:
    case pas_mte_feature_large_object_delegation:
        return pas_mte_use_feature_hardened;
    case pas_mte_feature_assert_adjacent_tags_are_disjoint:
        return pas_mte_use_feature_debug;
    case pas_mte_feature_log_on_tag:
    case pas_mte_feature_log_on_purify:
    case pas_mte_feature_log_page_alloc:
    case pas_mte_feature_zero_tag_all:
    case pas_mte_feature_check_tag_on_dealloc:
        return pas_mte_use_feature_never;
    }
    return pas_mte_use_feature_never;
}

inline __attribute__((always_inline)) bool
pas_mte_use_feature(pas_mte_feature feature)
{
    if (!pas_use_mte())
        return false;

    pas_mte_use_feature_condition condition = pas_mte_feature_use_condition(feature);

    if (condition >= pas_mte_use_feature_always)
        return true;

#ifndef NDEBUG
    /* Debug builds honor the runtime mode bits in addition to the
     * static use-conditions. */
    if (PAS_MTE_FEATURE_BIT(feature) || condition == pas_mte_use_feature_debug)
        return true;
#endif

    return condition == pas_mte_use_feature_hardened && pas_mte_is_hardened();
}

// FIXME: rdar://171662605
#define PAS_WORKAROUND_RDAR_171662605_UNCONDITIONAL_TAG_ON_ALLOC (1)

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
 * reason to have it on. If it is re-enabled it should be set to pas_use_mte()
 * so as to preserve the security properties of non-MTE processes.
 *
 * Astute observers may notice that in bmalloc we do the converse, i.e. allocating
 * always-compact objects from a single heap-ref. This is OK since in that heap,
 * we already expect all allocations to come out of the same singular intrinsic
 * heap.
 */
#define PAS_BYPASS_TZONE_FOR_NONCOMPACT_OBJECTS 0

/*
 * These are defined here rather than in pas_mte.h because they are needed by
 * pas_zero_memory.h, which is a transitive depencency of pas_mte.h
 */

inline __attribute__((always_inline)) void
pas_mte_check_tag_and_set_tco(const void* ptr)
{
#if PAS_ENABLE_MTE
    /*
     * We're only checking one tag-granule, so it's not perfect, but it does
     * mean that a potential attacker would at least need to know the tag for
     * some of their target range.
     */
    __asm__ volatile(
        ".arch_extension memtag\n\t"
        "ldr xzr, [%0]\n\t"
        "msr tco, #1"
        :
        : "r"(ptr)
        : "memory"
    );
#else // !PAS_ENABLE_MTE
    (void)ptr;
#endif // PAS_ENABLE_MTE
}

inline __attribute__((always_inline)) void
pas_mte_set_tco_unchecked(void)
{
#if PAS_ENABLE_MTE
    __asm__ volatile(
        ".arch_extension memtag\n\t"
        "msr tco, #1"
        :
        :
        : "memory"
    );
#endif // PAS_ENABLE_MTE
}

inline __attribute__((always_inline)) void
pas_mte_clear_tco(void)
{
#if PAS_ENABLE_MTE
    __asm__ volatile(
        ".arch_extension memtag\n\t"
        "msr tco, #0"
        :
        :
        : "memory"
    );
#endif // PAS_ENABLE_MTE
}

bool pas_mte_is_mte_enabled(void);
void pas_mte_ensure_initialized(void);
void pas_mte_force_nontaggable_user_allocations_into_large_heap(void);
void pas_bmalloc_force_allocations_into_bitfit_heaps_where_available(void);

#ifdef __cplusplus
}
#endif

#if defined(PAS_BMALLOC)
#if BENABLE(LIBPAS)
#if BENABLE_MTE != PAS_ENABLE_MTE
#error "cannot enable MTE in libpas without enabling it in bmalloc, or vice versa"
#endif // BENABLE(LIBPAS)
#endif // defined(PAS_BMALLOC)

#define BMALLOC_VM_MTE PAS_VM_MTE

#endif // defined(PAS_BMALLOC) && BENABLE(LIBPAS)
#endif // PAS_MTE_CONFIG_H
