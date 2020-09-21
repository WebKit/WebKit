/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "Algorithm.h"
#include "GigacageKind.h"
#include "StdLibExtras.h"
#include <inttypes.h>

#if BENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)

namespace WebConfig {

using Slot = uint64_t;
extern "C" Slot g_config[];

} // namespace WebConfig

#endif // BENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)

namespace Gigacage {

struct Config {
    void* basePtr(Kind kind) const
    {
        RELEASE_BASSERT(kind < NumberOfKinds);
        return basePtrs[kind];
    }

    void setBasePtr(Kind kind, void* ptr)
    {
        RELEASE_BASSERT(kind < NumberOfKinds);
        basePtrs[kind] = ptr;
    }

    // All the fields in this struct should be chosen such that their
    // initial value is 0 / null / falsy because Config is instantiated
    // as a global singleton.

    bool isPermanentlyFrozen; // Will be set by the client if the Config gets frozen.
    bool isEnabled;
    bool disablingPrimitiveGigacageIsForbidden;
    bool shouldBeEnabled;

    // We would like to just put the std::once_flag for these functions
    // here, but we can't because std::once_flag has a implicitly-deleted
    // default constructor. So, we use a boolean instead.
    bool shouldBeEnabledHasBeenCalled;
    bool ensureGigacageHasBeenCalled;

    void* start;
    size_t totalSize;
    void* basePtrs[NumberOfKinds];
};

#if BENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)

constexpr size_t startSlotOfGigacageConfig = 0;
constexpr size_t startOffsetOfGigacageConfig = startSlotOfGigacageConfig * sizeof(WebConfig::Slot);

constexpr size_t reservedSlotsForGigacageConfig = 6;
constexpr size_t reservedBytesForGigacageConfig = reservedSlotsForGigacageConfig * sizeof(WebConfig::Slot);

constexpr size_t alignmentOfGigacageConfig = std::alignment_of<Gigacage::Config>::value;

static_assert(sizeof(Gigacage::Config) <= reservedBytesForGigacageConfig);
static_assert(bmalloc::roundUpToMultipleOf<alignmentOfGigacageConfig>(startOffsetOfGigacageConfig) == startOffsetOfGigacageConfig);

#define g_gigacageConfig (*bmalloc::bitwise_cast<Gigacage::Config*>(&WebConfig::g_config[Gigacage::startSlotOfGigacageConfig]))

#else // not BENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)

extern "C" BEXPORT Config g_gigacageConfig;

#endif // BENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)

} // namespace Gigacage

#if !BENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)
using Gigacage::g_gigacageConfig;
#endif
