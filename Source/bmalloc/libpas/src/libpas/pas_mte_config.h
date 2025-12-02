/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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
#include "pas_config.h"
#if defined(PAS_BMALLOC)
#include "BPlatform.h"
#endif

#if defined(__has_include)
#if __has_include(<WebKitAdditions/pas_mte_additions.h>)
#include <WebKitAdditions/pas_mte_additions.h>
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

typedef uint64_t Slot;

#ifdef __cplusplus
extern "C" {
#endif
extern Slot g_config[];
#ifdef __cplusplus
}
#endif

#define PAS_MTE_ENABLE_FLAG 0
#define PAS_MTE_MODE_BITS 1
#define PAS_MTE_TAGGING_RATE 2
#define PAS_MTE_MEDIUM_TAGGING_ENABLE_FLAG 3
#define PAS_MTE_LOCKDOWN_MODE_FLAG 4
#define PAS_MTE_HARDENED_FLAG 5

// Must be kept in sync with the offsets in WTFConfig.h:ReservedConfigByteOffset
#define PAS_MTE_CONFIG_RESERVED_BYTE_OFFSET 2
#define PAS_MTE_CONFIG_BYTE(byte) (((uint8_t*)(g_config + PAS_MTE_CONFIG_RESERVED_BYTE_OFFSET))[byte])

#define PAS_USE_MTE (PAS_MTE_CONFIG_BYTE(PAS_MTE_ENABLE_FLAG))
#ifndef PAS_USE_MTE_IN_WEBCONTENT
#define PAS_USE_MTE_IN_WEBCONTENT 1
#endif

#define PAS_MTE_CONFIG_FIELD(byte, bit) (((PAS_MTE_CONFIG_BYTE(byte)) & (1UL << (bit))) ? 1 : 0)
#define PAS_MTE_MEDIUM_TAGGING_ENABLED (PAS_MTE_CONFIG_BYTE(PAS_MTE_MEDIUM_TAGGING_ENABLE_FLAG))
#define PAS_MTE_IS_LOCKDOWN_MODE (PAS_MTE_CONFIG_BYTE(PAS_MTE_LOCKDOWN_MODE_FLAG))
#define PAS_MTE_IS_HARDENED (PAS_MTE_CONFIG_BYTE(PAS_MTE_HARDENED_FLAG))

#define PAS_VM_MTE 0x2000
#define PAS_MTE_PROC_FLAG_SEC_ENABLED 0x4000000

#define PAS_MTE_SHOULD_STORE_TAG 1

#ifndef PAS_USE_COMPACT_ONLY_HEAP
/*
 * The reason we make TZone compact-only heaps reliant on runtime PAS_MTE
 * enablement, and not the general compact-only heap, is that lumping all
 * non-compact objects into the same heap is a security regression for TZone,
 * but not a security regression for the general bmalloc heap where we already
 * expect all allocations to come out of the same singular intrinsic heap.
 * By avoiding checking PAS_USE_MTE, we save an additional check in the malloc
 * fast path for ordinary allocations, while the corresponding check for TZone
 * heaps only occurs during heap selection - it's not as significant.
 */
#define PAS_USE_COMPACT_ONLY_HEAP 1
#define PAS_USE_COMPACT_ONLY_TZONE_HEAP PAS_USE_MTE
#endif

#define PAS_MTE_FEATURE_RETAG_ON_SCAVENGE 0
#define PAS_MTE_FEATURE_LOG_ON_TAG 1
#define PAS_MTE_FEATURE_LOG_ON_PURIFY 2
#define PAS_MTE_FEATURE_LOG_PAGE_ALLOC 3
#define PAS_MTE_FEATURE_ZERO_TAG_ALL 4
#define PAS_MTE_FEATURE_ADJACENT_TAG_EXCLUSION 5
#define PAS_MTE_FEATURE_ASSERT_ADJACENT_TAGS_ARE_DISJOINT 6

#define PAS_MTE_FEATURE_FORCED(feature) (0)
#define PAS_MTE_FEATURE_HARDENED_FORCED(feature) (feature == PAS_MTE_FEATURE_RETAG_ON_SCAVENGE || feature == PAS_MTE_FEATURE_ADJACENT_TAG_EXCLUSION)
#define PAS_MTE_FEATURE_DEBUG_FORCED(feature) (feature == PAS_MTE_FEATURE_ASSERT_ADJACENT_TAGS_ARE_DISJOINT)

#define PAS_MTE_FEATURE_FORCED_IN_RELEASE_BUILD(feature) \
    (PAS_MTE_FEATURE_FORCED(feature) || \
     (PAS_MTE_FEATURE_HARDENED_FORCED(feature) && PAS_MTE_IS_HARDENED))

#define PAS_MTE_FEATURE_FORCED_IN_DEBUG_BUILD(feature) \
    (PAS_MTE_FEATURE_FORCED_IN_RELEASE_BUILD(feature) || \
     PAS_MTE_FEATURE_DEBUG_FORCED(feature) || \
     PAS_MTE_CONFIG_FIELD(PAS_MTE_MODE_BITS, feature))

#ifndef NDEBUG
#define PAS_MTE_FEATURE_ENABLED(feature) (PAS_USE_MTE && PAS_MTE_FEATURE_FORCED_IN_DEBUG_BUILD(feature))
#else
#define PAS_MTE_FEATURE_ENABLED(feature) (PAS_USE_MTE && PAS_MTE_FEATURE_FORCED_IN_RELEASE_BUILD(feature))
#endif

/*
 * These are defined here rather than in pas_mte.h because they are needed by
 * pas_zero_memory.h, which is a transitive depencency of pas_mte.h
 */
#define PAS_MTE_CHECK_TAG_AND_SET_TCO(ptr) do { \
        /* We're only checking one tag-granule, so it's not perfect, \
         * but it does mean that a potential attacker would at least \
         * need to know the tag for some of their target range. */ \
        asm volatile( \
            ".arch_extension memtag\n\t" \
            "ldr xzr, [%0]\n\t" \
            "msr tco, #1" \
            : \
            : "r"(ptr) \
            : "memory" \
        ); \
    } while (0)
#define PAS_MTE_SET_TCO_UNCHECKED do { \
        asm volatile( \
            ".arch_extension memtag\n\t" \
            "msr tco, #1" \
            : \
            : \
            : "memory" \
        ); \
    } while (0)
#define PAS_MTE_CLEAR_TCO do { \
        asm volatile( \
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
#define PAS_MTE_CHECK_TAG_AND_SET_TCO(ptr) do { (void)ptr; } while (0)
#define PAS_MTE_SET_TCO_UNCHECKED do { } while (0)
#define PAS_MTE_CLEAR_TCO do { } while (0)
#endif // PAS_ENABLE_MTE

#ifdef __cplusplus
extern "C" {
#endif
void pas_mte_ensure_initialized(void);
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
