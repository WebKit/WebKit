/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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

#include <wtf/Compiler.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <bmalloc/GigacageConfig.h>
#include <cstddef>
#include <wtf/Assertions.h>
#include <wtf/Atomics.h>
#include <wtf/ExportMacros.h>
#include <wtf/PageBlock.h>
#include <wtf/PtrTag.h>
#include <wtf/StdLibExtras.h>
#include <wtf/threads/Signals.h>

namespace WebConfig {

using Slot = uint64_t;
// Used primarily as storage for WTFConfig.
// However, other components also use it to stow their own configurations:
// - the storage for allocator configs is located at the start of the
//   region, and is laid out a la ReservedGlobalConfigSlots below.
// - JSC::Config is located after the body of the WTFConfig,
//   at the address `spaceForExtensions`
extern "C" WTF_EXPORT_PRIVATE Slot g_config[];

constexpr size_t reservedSlotsForLibpasConfiguration = 2;
constexpr size_t reservedSlotsForExecutableAllocator = 2;
struct ReservedGlobalConfigSlots {
    // Unlike with the other two allocator configuration regions,
    // Libpas does NOT reference these definitions to see how much space it has /
    // what offset it starts at, as it doesn't depend on WTF and can't include
    // cpp headers.
    // Instead, it assumes
    //   A) it gets at least two slots of storage,
    //   B) that that storage starts at byte 0 of g_config (i.e. no offset)
    Slot reservationForLibpas[reservedSlotsForLibpasConfiguration];
    Slot reservationForExecutableAllocator[reservedSlotsForExecutableAllocator];
    Slot reservationForGigacage[Gigacage::reservedSlotsForGigacageConfig];
    // Storage for the WTF config proper begins here
};

constexpr ptrdiff_t startOffsetOfExecutableAllocatorConfig =
        offsetof(WebConfig::ReservedGlobalConfigSlots, reservationForExecutableAllocator);

#if !USE(SYSTEM_MALLOC)
static_assert(Gigacage::startOffsetOfGigacageConfig == offsetof(ReservedGlobalConfigSlots, reservationForGigacage));
#endif

} // namespace WebConfig

namespace WTF {

constexpr size_t ConfigAlignment = CeilingOnPageSize;
constexpr size_t ConfigSizeToProtect = std::max(CeilingOnPageSize, 16 * KB);

struct Config {
    WTF_EXPORT_PRIVATE static void permanentlyFreeze();
    WTF_EXPORT_PRIVATE static void initialize();
    WTF_EXPORT_PRIVATE static void finalize();
    WTF_EXPORT_PRIVATE static void NODELETE disableFreezingForTesting();

    struct AssertNotFrozenScope {
        AssertNotFrozenScope();
        ~AssertNotFrozenScope();
    };

    // All the fields in this struct should be chosen such that their
    // initial value is 0 / null / falsy because Config is instantiated
    // as a global singleton.

    uintptr_t lowestAccessibleAddress;
    uintptr_t highestAccessibleAddress;

    bool isPermanentlyFrozen;
    bool disabledFreezingForTesting;
    bool useSpecialAbortForExtraSecurityImplications;
#if PLATFORM(COCOA) || OS(ANDROID)
    bool disableForwardingVPrintfStdErrToOSLog;
#endif

#if USE(PTHREADS)
    bool isUserSpecifiedThreadSuspendResumeSignalConfigured;
    bool isThreadSuspendResumeSignalConfigured;
    int sigThreadSuspendResume;
#endif
    SignalHandlers signalHandlers;
    PtrTagLookup* ptrTagLookupHead;

    uint64_t spaceForExtensions[1];
};

constexpr ptrdiff_t startOffsetOfWTFConfig = sizeof(WebConfig::ReservedGlobalConfigSlots);
constexpr size_t startSlotOfWTFConfig = startOffsetOfWTFConfig / sizeof(WebConfig::Slot);

constexpr ptrdiff_t offsetOfWTFConfigExtension = startOffsetOfWTFConfig + offsetof(WTF::Config, spaceForExtensions);

constexpr size_t alignmentOfWTFConfig = std::alignment_of<WTF::Config>::value;

static_assert(roundUpToMultipleOf<alignmentOfWTFConfig>(startOffsetOfWTFConfig) == startOffsetOfWTFConfig);
static_assert(startOffsetOfWTFConfig + sizeof(WTF::Config) <= ConfigSizeToProtect);

WTF_EXPORT_PRIVATE void setPermissionsOfConfigPage();

WTF_EXPORT_PRIVATE void makePagesFreezable(void* base, size_t); // Works together with permanentlyFreezePages().

enum class FreezePagePermission {
    None, // Remove all permissions i.e. no permissions at all.
    ReadOnly, // The pages can be read.
};
WTF_EXPORT_PRIVATE void permanentlyFreezePages(void* base, size_t, FreezePagePermission);

// Workaround to localize bounds safety warnings to this file.
// FIXME: Use real types to make materializing WTF::Config* bounds-safe and type-safe.
inline Config* addressOfWTFConfig() { return std::bit_cast<Config*>(&WebConfig::g_config[startSlotOfWTFConfig]); }

#define g_wtfConfig (*WTF::addressOfWTFConfig())

constexpr size_t offsetOfWTFConfigLowestAccessibleAddress = offsetof(WTF::Config, lowestAccessibleAddress);

ALWAYS_INLINE Config::AssertNotFrozenScope::AssertNotFrozenScope()
{
    RELEASE_ASSERT(!g_wtfConfig.isPermanentlyFrozen);
    compilerFence();
};

ALWAYS_INLINE Config::AssertNotFrozenScope::~AssertNotFrozenScope()
{
    compilerFence();
    RELEASE_ASSERT(!g_wtfConfig.isPermanentlyFrozen);
};

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

