/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(MAC)

#include <memory>
#include <wtf/RetainPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSSet;
OBJC_CLASS NSString;

namespace WebCore {

class PluginBlocklist {
public:
    enum class LoadPolicy {
        LoadNormally,
        BlockedForSecurity,
        BlockedForCompatibility,
    };

    WEBCORE_EXPORT static LoadPolicy loadPolicyForPluginVersion(NSString *bundleIdentifier, NSString *bundleVersionString);
    WEBCORE_EXPORT static bool isPluginUpdateAvailable(NSString *bundleIdentifier);

    static std::unique_ptr<PluginBlocklist> create(NSDictionary *);
    ~PluginBlocklist();

    static NSArray *splitOSVersion(NSString *osVersion);

    LoadPolicy loadPolicyForPlugin(NSString *bundleIdentifier, NSString *bundleVersionString) const;
    bool isUpdateAvailable(NSString *bundleIdentifier) const;

private:
    PluginBlocklist(NSDictionary *bundleIDToMinimumSecureVersion, NSDictionary *bundleIDToMinimumCompatibleVersion, NSDictionary *bundleIDToBlockedVersions, NSSet *bundleIDsWithAvailableUpdates);

    RetainPtr<NSDictionary> m_bundleIDToMinimumSecureVersion;
    RetainPtr<NSDictionary> m_bundleIDToMinimumCompatibleVersion;
    RetainPtr<NSDictionary> m_bundleIDToBlockedVersions;
    RetainPtr<NSSet> m_bundleIDsWithAvailableUpdates;
};

}

#endif
