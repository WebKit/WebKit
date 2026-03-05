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

#include "config.h"
#include <wtf/WTFConfig.h>

#include <cstdio>

#include <wtf/FastMalloc.h>
#include <wtf/Gigacage.h>
#include <wtf/Lock.h>
#include <wtf/MathExtras.h>
#include <wtf/PageBlock.h>
#include <wtf/StdLibExtras.h>

#if OS(DARWIN)
#include <dlfcn.h>
#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
#include <mach/vm_param.h>
#include "unistd.h"
#endif

#if OS(WINDOWS)
#include <windows.h>
#endif

#if defined(__has_include)
#if __has_include(<libproc.h>)
#include <libproc.h>
#endif // __has_include(<libproc.h>)
#endif // defined(__has_include)

#if PLATFORM(COCOA)
#include <wtf/spi/cocoa/MachVMSPI.h>
#include <mach/mach.h>
#elif OS(LINUX)
#include <sys/mman.h>
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WTFConfigAdditions.h>
#endif
#if BUSE(LIBPAS)
#include "bmalloc/pas_mte_config.h"
#endif

#include <mutex>

#if OS(DARWIN) && BUSE(LIBPAS)
#if HAVE(36BIT_ADDRESS) && !PAS_HAVE(36BIT_ADDRESS)
#error HAVE(36BIT_ADDRESS) is true, but PAS_HAVE(36BIT_ADDRESS) is false. They should match.
#elif !HAVE(36BIT_ADDRESS) && PAS_HAVE(36BIT_ADDRESS)
#error HAVE(36BIT_ADDRESS) is false, but PAS_HAVE(36BIT_ADDRESS) is true. They should match.
#endif
#endif // OS(DARWIN) && BUSE(LIBPAS)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebConfig {

#if COMPILER(CLANG)
// The purpose of applying this attribute is to move the g_config away from other
// global variables, and allow the linker to pack them in more efficiently. Without this, the
// linker currently leaves multiple KBs of unused padding before this page though there are many
// other variables that come afterwards that would have fit in there. Adding this attribute was
// shown to resolve a memory regression on our small memory footprint benchmark.
#define WTF_CONFIG_SECTION __attribute__((used, section("__DATA,__wtf_config")))
#else
#define WTF_CONFIG_SECTION
#endif

alignas(WTF::ConfigAlignment) WTF_CONFIG_SECTION Slot g_config[WTF::ConfigSizeToProtect / sizeof(Slot)];

} // namespace WebConfig

#if !USE(SYSTEM_MALLOC)
static_assert(Gigacage::startSlotOfGigacageConfig == WebConfig::NumberOfReservedConfigBytes);
#endif

namespace WTF {

// Works together with permanentlyFreezePages().
void makePagesFreezable(void* base, size_t size)
{
    RELEASE_ASSERT(roundUpToMultipleOf(pageSize(), size) == size);

#if PLATFORM(COCOA)
    mach_vm_address_t addr = std::bit_cast<uintptr_t>(base);
    auto flags = VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE | VM_FLAGS_PERMANENT;

    auto attemptVMMapping = [&] {
        auto result = mach_vm_map(mach_task_self(), &addr, size, pageSize() - 1, flags, MEMORY_OBJECT_NULL, 0, false, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_DEFAULT);
        return result;
    };

    auto result = attemptVMMapping();
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    if (result != KERN_SUCCESS) {
        flags &= ~VM_FLAGS_PERMANENT; // See rdar://75747788.
        result = attemptVMMapping();
    }
#endif
    RELEASE_ASSERT(result == KERN_SUCCESS);
#else
    UNUSED_PARAM(base);
    UNUSED_PARAM(size);
#endif
}

void setPermissionsOfConfigPage()
{
#if PLATFORM(COCOA)
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        constexpr size_t preWTFConfigSize = Gigacage::startOffsetOfGigacageConfig + Gigacage::reservedBytesForGigacageConfig;

        // We may have potentially initialized some of g_config, namely the
        // gigacage config, prior to reaching this function. We need to
        // preserve these config contents across the mach_vm_map.
        uint8_t preWTFConfigContents[preWTFConfigSize];
        memcpySpan(std::span<uint8_t> { preWTFConfigContents, preWTFConfigSize }, std::span<uint8_t> { std::bit_cast<uint8_t*>(&WebConfig::g_config), preWTFConfigSize });

        makePagesFreezable(&WebConfig::g_config, ConfigSizeToProtect);

        memcpySpan(std::span<uint8_t> { std::bit_cast<uint8_t*>(&WebConfig::g_config), preWTFConfigSize }, std::span<uint8_t> { preWTFConfigContents, preWTFConfigSize });
    });
#endif // PLATFORM(COCOA)
}

void Config::initialize()
{
    // FIXME: We should do a placement new for Config so we can use default initializers.
    []() -> void {
        uintptr_t onePage = pageSize(); // At least, first one page must be unmapped.
        uintptr_t address = 0;
#if OS(DARWIN)
#ifdef __LP64__
        using Header = struct mach_header_64;
#else
        using Header = struct mach_header;
#endif
        const auto* header = static_cast<const Header*>(dlsym(RTLD_MAIN_ONLY, MH_EXECUTE_SYM));
        if (header) {
            unsigned long size = 0;
            const auto* data = getsegmentdata(header, "__PAGEZERO", &size);
            if (!data && size) {
                // If __PAGEZERO starts with 0 address and it has size. [0, size] region cannot be
                // mapped for accessible pages.
                address = reinterpret_cast<uintptr_t>(data) + size;
            }
        }
#elif OS(WINDOWS)
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        address = reinterpret_cast<uintptr_t>(sysInfo.lpMinimumApplicationAddress);
#elif OS(LINUX)
        FILE* file = fopen("/proc/sys/vm/mmap_min_addr", "r");
        if (file) {
            unsigned long value = 0;
            if (fscanf(file, "%lu", &value) == 1)
                address = static_cast<uintptr_t>(value);
            fclose(file);
        }
#endif
        g_wtfConfig.lowestAccessibleAddress = roundDownToMultipleOf(onePage, std::max<uintptr_t>(onePage, address));
    }();
    g_wtfConfig.highestAccessibleAddress = static_cast<uintptr_t>((1ULL << OS_CONSTANT(EFFECTIVE_ADDRESS_WIDTH)) - 1);
    SignalHandlers::initialize();

    [[maybe_unused]] uint8_t* reservedConfigBytes = reinterpret_cast_ptr<uint8_t*>(WebConfig::g_config);

#if USE(LIBPAS) && defined(PAS_MTE_INITIALIZE_IN_WTF_CONFIG)
    PAS_MTE_INITIALIZE_IN_WTF_CONFIG;
#endif // USE(LIBPAS)
    const char* useAllocationProfilingRaw = getenv("JSC_useAllocationProfiling");
    if (useAllocationProfilingRaw) {
        auto useAllocationProfiling = unsafeSpan(useAllocationProfilingRaw);
        if (equalLettersIgnoringASCIICase(useAllocationProfiling, "true"_s)
            || equalLettersIgnoringASCIICase(useAllocationProfiling, "yes"_s)
            || equal(useAllocationProfiling, "1"_s))
            reservedConfigBytes[WebConfig::ReservedByteForAllocationProfiling] = 1;
        else if (equalLettersIgnoringASCIICase(useAllocationProfiling, "false"_s)
            || equalLettersIgnoringASCIICase(useAllocationProfiling, "no"_s)
            || equal(useAllocationProfiling, "0"_s))
            reservedConfigBytes[WebConfig::ReservedByteForAllocationProfiling] = 0;

        const char* useAllocationProfilingModeRaw = getenv("JSC_allocationProfilingMode");
        if (useAllocationProfilingModeRaw && reservedConfigBytes[WebConfig::ReservedByteForAllocationProfiling] == 1) {
            unsigned value { 0 };
            if (sscanf(useAllocationProfilingModeRaw, "%u", &value) == 1) {
                RELEASE_ASSERT(value <= 0xFF);
                reservedConfigBytes[WebConfig::ReservedByteForAllocationProfilingMode] = static_cast<uint8_t>(value & 0xFF);
            }
        }
    }
}

void Config::finalize()
{
    static std::once_flag once;
    std::call_once(once, [] {
        SignalHandlers::finalize();
        if (!g_wtfConfig.disabledFreezingForTesting)
            Config::permanentlyFreeze();
    });
}

void Config::permanentlyFreeze()
{
    ASSERT(!g_wtfConfig.disabledFreezingForTesting);

    if (!g_wtfConfig.isPermanentlyFrozen) {
        g_wtfConfig.isPermanentlyFrozen = true;
#if GIGACAGE_ENABLED
        g_gigacageConfig.isPermanentlyFrozen = true;
#endif
    }
    permanentlyFreezePages(&WebConfig::g_config, ConfigSizeToProtect, WTF::FreezePagePermission::ReadOnly);
    RELEASE_ASSERT(g_wtfConfig.isPermanentlyFrozen);
}

void permanentlyFreezePages(void* base, size_t size, FreezePagePermission permission)
{
    RELEASE_ASSERT(roundUpToMultipleOf(pageSize(), size) == size);

    int result = 0;
#if PLATFORM(COCOA)
    enum {
        DontUpdateMaximumPermission = false,
        UpdateMaximumPermission = true
    };

    // There's no going back now!
    result = vm_protect(mach_task_self(), reinterpret_cast<vm_address_t>(base), size, UpdateMaximumPermission, permission == FreezePagePermission::ReadOnly ? VM_PROT_READ : VM_PROT_NONE);
#elif OS(LINUX)
    result = mprotect(base, size, permission == FreezePagePermission::ReadOnly ? PROT_READ : PROT_NONE);
#else
    // FIXME: Implement equivalent for Windows, maybe with VirtualProtect.
    // Also need to fix WebKitTestRunner.
    UNUSED_PARAM(base);
    UNUSED_PARAM(size);
    UNUSED_PARAM(permission);
#endif
    RELEASE_ASSERT(!result);
}

void Config::disableFreezingForTesting()
{
    RELEASE_ASSERT(!g_wtfConfig.isPermanentlyFrozen);
    g_wtfConfig.disabledFreezingForTesting = true;
}

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
