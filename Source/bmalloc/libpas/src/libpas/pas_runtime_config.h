/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#ifndef PAS_RUNTIME_CONFIG_H
#define PAS_RUNTIME_CONFIG_H

#include "pas_config.h"
#include "pas_platform.h"
#if defined(PAS_BMALLOC)
#include "BPlatform.h"
// FIXME: Find a way to declare bmalloc's symbol visibility without having to
// import a bmalloc header.
#include "BExport.h"
#endif


#include <stddef.h>
#include <stdint.h>

typedef uint64_t Slot;

// If libpas is linked as part of bmalloc (i.e. as part of WebKit), then
// it uses the reserved slots in WTF's WebConfig::g_config to store its
// configuration data. In this case, libpas' configuration storage will
// begin at byte 0 of g_config.
// In all other build configurations, it allocates its own storage.
#ifdef __cplusplus
extern "C" {
#endif
#if LIBPAS_ENABLED
#if defined(PAS_BMALLOC)
BEXPORT extern Slot g_config[];
#else // !defined(PAS_BMALLOC)
extern Slot g_config[];
#endif // defined(PAS_BMALLOC)
#endif // LIBPAS_ENABLED
#ifdef __cplusplus
}
#endif

// Must be kept in sync with WTFConfig.h:reservedSlotsForLibpasConfiguration
#define PAS_RUNTIME_CONFIG_RESERVED_SLOTS 2
#define PAS_RUNTIME_CONFIG_RESERVED_BYTES (PAS_RUNTIME_CONFIG_RESERVED_SLOTS * sizeof(Slot))

typedef struct {
    uint8_t enabled;

    struct {
        uint8_t retag_on_scavenge : 1;
        uint8_t log_on_tag : 1;
        uint8_t log_on_purify : 1;
        uint8_t log_page_alloc : 1;
        uint8_t zero_tag_all : 1;
        uint8_t adjacent_tag_exclusion : 1;
        uint8_t assert_adjacent_tags_are_disjoint : 1;
    } mode_bits;

    uint8_t tagging_rate;
    bool medium_tagging_enabled;
    bool is_lockdown_mode;
    bool is_hardened;
} pas_runtime_config;

#if PAS_COMPILER(CLANG)
_Static_assert(sizeof(pas_runtime_config) <= PAS_RUNTIME_CONFIG_RESERVED_BYTES, "Must not exceed storage reserved by WTF");
#endif


#define PAS_RUNTIME_CONFIG_PTR ((pas_runtime_config*)((uint8_t*)(g_config)))

#endif // PAS_RUNTIME_CONFIG_H
