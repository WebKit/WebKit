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

#include "config.h"
#include <wtf/WTFConfig.h>

#include <wtf/Gigacage.h>
#include <wtf/Lock.h>
#include <wtf/StdLibExtras.h>

#if OS(DARWIN)
#include <mach/mach.h>
#elif OS(LINUX)
#include <sys/mman.h>
#endif

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

void Config::permanentlyFreeze()
{
    static Lock configLock;
    auto locker = holdLock(configLock);

    RELEASE_ASSERT(roundUpToMultipleOf(pageSize(), ConfigSizeToProtect) == ConfigSizeToProtect);

    if (!g_wtfConfig.isPermanentlyFrozen) {
        g_wtfConfig.isPermanentlyFrozen = true;
#if GIGACAGE_ENABLED
        g_gigacageConfig.isPermanentlyFrozen = true;
#endif
    }

    int result = 0;

#if ENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)
#if OS(DARWIN)
    enum {
        AllowPermissionChangesAfterThis = false,
        DisallowPermissionChangesAfterThis = true
    };

    // There's no going back now!
    result = vm_protect(mach_task_self(), reinterpret_cast<vm_address_t>(&WebConfig::g_config), ConfigSizeToProtect, DisallowPermissionChangesAfterThis, VM_PROT_READ);
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
