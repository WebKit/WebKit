/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "BrowsingContextGroupModes.h"

#include "APIPageConfiguration.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"

namespace WebKit {

BrowsingContextGroupModes BrowsingContextGroupModes::forConfiguration(const API::PageConfiguration& configuration)
{
    bool lockdownModeEnabled = configuration.lockdownModeEnabled();
    bool useEnhancedSecurityFallback = !lockdownModeEnabled && lockdownModeEnabledBySystem();
    bool enhancedSecurityEnabled = protect(configuration.preferences())->forceEnhancedSecurity() || configuration.isEnhancedSecurityEnabled() || configuration.isEnhancedSecurityEnabledByDefault(lockdownModeEnabled) || useEnhancedSecurityFallback;
    return {
        lockdownModeEnabled ? WebProcessProxy::LockdownMode::Enabled : WebProcessProxy::LockdownMode::Disabled,
        enhancedSecurityEnabled ? EnhancedSecurity::EnabledPolicy : EnhancedSecurity::Disabled,
        configuration.websiteDataStore().sessionID()
    };
}

bool BrowsingContextGroupModes::matches(WebProcessProxy::LockdownMode otherLockdownMode, EnhancedSecurity otherEnhancedSecurity, const WebsiteDataStore& websiteDataStore) const
{
    return lockdownMode == otherLockdownMode && enhancedSecurityStatesAreConsistent(enhancedSecurity, otherEnhancedSecurity) && websiteDataStoreSessionID == websiteDataStore.sessionID();
}

bool BrowsingContextGroupModes::matches(const WebProcessProxy& process) const
{
    return process.matchesLockdownModeAndEnhancedSecurity(lockdownMode, enhancedSecurity) && process.sessionID() == websiteDataStoreSessionID;
}

} // namespace WebKit
