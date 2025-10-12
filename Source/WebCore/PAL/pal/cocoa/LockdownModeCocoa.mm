/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "LockdownModeCocoa.h"

#if ENABLE(LOCKDOWN_MODE_API)

#import <sys/sysctl.h>
#import <wtf/NeverDestroyed.h>

#if HAVE(LOCKDOWN_MODE_FRAMEWORK)
#import <LockdownMode/LockdownMode.h>
#import <wtf/SoftLinking.h>
#endif

#if HAVE(LOCKDOWN_MODE_FRAMEWORK)
OBJC_CLASS LockdownModeManager;

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(LockdownMode)
SOFT_LINK_CLASS_OPTIONAL(LockdownMode, LockdownModeManager)
#endif

namespace PAL {

bool isLockdownModeEnabled()
{
#if HAVE(LOCKDOWN_MODE_FRAMEWORK)
    if (LockdownModeLibrary())
        return [(LockdownModeManager *)[getLockdownModeManagerClassSingleton() shared] enabled];
#endif

    // FIXME(<rdar://108208100>): Remove this fallback once recoveryOS includes the framework.
    uint64_t ldmState = 0;
    size_t sysCtlLen = sizeof(ldmState);
    if (!sysctlbyname("security.mac.lockdown_mode_state", &ldmState, &sysCtlLen, NULL, 0))
        return ldmState == 1;

    return false;
}

static std::optional<bool>& isLockdownModeEnabledForCurrentProcessCached()
{
    static NeverDestroyed<std::optional<bool>> cachedIsLockdownModeEnabledForCurrentProcess;
    return cachedIsLockdownModeEnabledForCurrentProcess;
}

bool isLockdownModeEnabledForCurrentProcess()
{
    return isLockdownModeEnabledForCurrentProcessCached().value_or(isLockdownModeEnabled());
}

void setLockdownModeEnabledForCurrentProcess(bool isLockdownModeEnabled)
{
    isLockdownModeEnabledForCurrentProcessCached() = isLockdownModeEnabled;
}

} // namespace PAL

#endif
