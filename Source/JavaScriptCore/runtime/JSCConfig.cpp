/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "JSCConfig.h"

#include <wtf/ResourceUsage.h>
#include <wtf/StdLibExtras.h>

#if OS(DARWIN)
#include <mach/mach.h>
#elif OS(LINUX)
#include <sys/mman.h>
#endif

namespace JSC {

alignas(ConfigSizeToProtect) JS_EXPORT_PRIVATE Config g_jscConfig;

void Config::disableFreezingForTesting()
{
    RELEASE_ASSERT(!g_jscConfig.isPermanentlyFrozen);
    g_jscConfig.disabledFreezingForTesting = true;
}

void Config::enableRestrictedOptions()
{
    RELEASE_ASSERT(!g_jscConfig.isPermanentlyFrozen);
    g_jscConfig.restrictedOptionsEnabled = true;
}
    
void Config::permanentlyFreeze()
{
    RELEASE_ASSERT(roundUpToMultipleOf(pageSize(), ConfigSizeToProtect) == ConfigSizeToProtect);

    if (!g_jscConfig.isPermanentlyFrozen)
        g_jscConfig.isPermanentlyFrozen = true;

    int result = 0;
#if OS(DARWIN)
    enum {
        AllowPermissionChangesAfterThis = false,
        DisallowPermissionChangesAfterThis = true
    };

    // There's no going back now!
    result = vm_protect(mach_task_self(), reinterpret_cast<vm_address_t>(&g_jscConfig), ConfigSizeToProtect, DisallowPermissionChangesAfterThis, VM_PROT_READ);
#elif OS(LINUX)
    result = mprotect(&g_jscConfig, ConfigSizeToProtect, PROT_READ);
#elif OS(WINDOWS)
    // FIXME: Implement equivalent, maybe with VirtualProtect.
    // Also need to fix WebKitTestRunner.
#endif
    RELEASE_ASSERT(!result);
    RELEASE_ASSERT(g_jscConfig.isPermanentlyFrozen);
}

} // namespace JSC
