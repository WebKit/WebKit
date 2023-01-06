/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#endif

#if PLATFORM(COCOA)
#include <wtf/spi/cocoa/MachVMSPI.h>
#include <mach/mach.h>
#elif OS(LINUX)
#include <sys/mman.h>
#endif

#include <mutex>

#if ENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)

namespace WebConfig {

alignas(WTF::ConfigAlignment) Slot g_config[WTF::ConfigSizeToProtect / sizeof(Slot)];

} // namespace WebConfig

#else // not ENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)

namespace WTF {

Config g_wtfConfig;

} // namespace WTF

#endif // ENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)

namespace WTF {

#if ENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)
void setPermissionsOfConfigPage()
{
#if PLATFORM(COCOA)
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        mach_vm_address_t addr = bitwise_cast<uintptr_t>(static_cast<void*>(WebConfig::g_config));
        auto flags = VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE;
#if HAVE(VM_FLAGS_PERMANENT)
        flags |= VM_FLAGS_PERMANENT;
#endif

        auto attemptVMMapping = [&] {
            return mach_vm_map(mach_task_self(), &addr, ConfigSizeToProtect, pageSize() - 1, flags, MEMORY_OBJECT_NULL, 0, false, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_DEFAULT);
        };

        auto result = attemptVMMapping();

#if HAVE(VM_FLAGS_PERMANENT)
        if (result != KERN_SUCCESS) {
            flags &= ~VM_FLAGS_PERMANENT;
            result = attemptVMMapping();
        }
#endif

        RELEASE_ASSERT(result == KERN_SUCCESS);
    });
#endif // PLATFORM(COCOA)
}
#endif // ENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)

void Config::initialize()
{
    []() -> void {
        uintptr_t onePage = pageSize(); // At least, first one page must be unmapped.
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
                uintptr_t afterZeroPages = bitwise_cast<uintptr_t>(data) + size;
                g_wtfConfig.lowestAccessibleAddress = roundDownToMultipleOf(onePage, std::max<uintptr_t>(onePage, afterZeroPages));
                return;
            }
        }
#endif
        g_wtfConfig.lowestAccessibleAddress = onePage;
    }();
    g_wtfConfig.highestAccessibleAddress = static_cast<uintptr_t>((1ULL << OS_CONSTANT(EFFECTIVE_ADDRESS_WIDTH)) - 1);
}

void Config::permanentlyFreeze()
{
    static Lock configLock;
    Locker locker { configLock };

    RELEASE_ASSERT(roundUpToMultipleOf(pageSize(), ConfigSizeToProtect) == ConfigSizeToProtect);

    if (!g_wtfConfig.isPermanentlyFrozen) {
        g_wtfConfig.isPermanentlyFrozen = true;
#if GIGACAGE_ENABLED
        g_gigacageConfig.isPermanentlyFrozen = true;
#endif
    }

    int result = 0;

#if ENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)
#if PLATFORM(COCOA)
    enum {
        DontUpdateMaximumPermission = false,
        UpdateMaximumPermission = true
    };

    // There's no going back now!
    result = vm_protect(mach_task_self(), reinterpret_cast<vm_address_t>(&WebConfig::g_config), ConfigSizeToProtect, UpdateMaximumPermission, VM_PROT_READ);
#elif OS(LINUX)
    result = mprotect(&WebConfig::g_config, ConfigSizeToProtect, PROT_READ);
#elif OS(WINDOWS)
    // FIXME: Implement equivalent, maybe with VirtualProtect.
    // Also need to fix WebKitTestRunner.

    // Note: the Windows port also currently does not support a unified Config
    // record, which is needed for the current form of the freezing feature to
    // work. See comments in PlatformEnable.h for UNIFIED_AND_FREEZABLE_CONFIG_RECORD.
#endif
#endif // ENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)

    RELEASE_ASSERT(!result);
    RELEASE_ASSERT(g_wtfConfig.isPermanentlyFrozen);
}

} // namespace WTF
