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

#import "config.h"
#import "PluginBlocklist.h"

#if PLATFORM(MAC)

#import "BlocklistUpdater.h"
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <sys/stat.h>
#import <sys/time.h>

namespace WebCore {

auto PluginBlocklist::loadPolicyForPluginVersion(NSString *bundleIdentifier, NSString *bundleVersionString) -> LoadPolicy
{
    auto loadPolicy = LoadPolicy::LoadNormally;
    BlocklistUpdater::queue().dispatchSync([&] {
        BlocklistUpdater::reloadIfNecessary();
        if (auto* pluginBlocklist = BlocklistUpdater::pluginBlocklist())
            loadPolicy = pluginBlocklist->loadPolicyForPlugin(bundleIdentifier, bundleVersionString);
    });

    return loadPolicy;
}

bool PluginBlocklist::isPluginUpdateAvailable(NSString *bundleIdentifier)
{
    bool isPluginUpdateAvailable = false;
    BlocklistUpdater::queue().dispatchSync([&] {
        BlocklistUpdater::reloadIfNecessary();
        if (auto* pluginBlocklist = BlocklistUpdater::pluginBlocklist())
            isPluginUpdateAvailable = pluginBlocklist->isUpdateAvailable(bundleIdentifier);
    });

    return isPluginUpdateAvailable;
}

std::unique_ptr<PluginBlocklist> PluginBlocklist::create(NSDictionary *propertyList)
{
    auto systemVersionDictionary = adoptCF(_CFCopySystemVersionDictionary());
    CFStringRef osVersion = static_cast<CFStringRef>(CFDictionaryGetValue(systemVersionDictionary.get(), _kCFSystemVersionProductVersionKey));

    NSDictionary *dictionary = [propertyList objectForKey:@"PlugInBlocklist"];

    NSMutableDictionary *bundleIDToMinimumSecureVersion = [NSMutableDictionary dictionary];
    NSMutableDictionary *bundleIDToMinimumCompatibleVersion = [NSMutableDictionary dictionary];
    NSMutableDictionary *bundleIDToBlockedVersions = [NSMutableDictionary dictionary];
    NSMutableSet *bundleIDsWithAvailableUpdates = [NSMutableSet set];
    
    for (NSString *osVersionComponent in splitOSVersion((__bridge NSString *)osVersion)) {
        NSDictionary *bundleIDs = [dictionary objectForKey:osVersionComponent];
        if (!bundleIDs)
            continue;

        for (NSString *bundleID in bundleIDs) {
            NSDictionary *versionInfo = [bundleIDs objectForKey:bundleID];
            assert(versionInfo);

            if (![versionInfo isKindOfClass:[NSDictionary class]])
                continue;

            [bundleIDToMinimumSecureVersion removeObjectForKey:bundleID];
            [bundleIDToMinimumCompatibleVersion removeObjectForKey:bundleID];
            [bundleIDToBlockedVersions removeObjectForKey:bundleID];

            if (NSArray *blockedVersions = [versionInfo objectForKey:@"BlockedPlugInBundleVersions"])
                [bundleIDToBlockedVersions setObject:blockedVersions forKey:bundleID];

            if (NSString *minimumSecureVersion = [versionInfo objectForKey:@"MinimumPlugInBundleVersion"])
                [bundleIDToMinimumSecureVersion setObject:minimumSecureVersion forKey:bundleID];

            if (NSString *minimumCompatibleVersion = [versionInfo objectForKey:@"MinimumCompatiblePlugInBundleVersion"])
                [bundleIDToMinimumCompatibleVersion setObject:minimumCompatibleVersion forKey:bundleID];

            if (NSNumber *updateAvailable = [versionInfo objectForKey:@"PlugInUpdateAvailable"]) {
                // A missing PlugInUpdateAvailable key means that there is a plug-in update available.
                if (!updateAvailable || [updateAvailable boolValue])
                    [bundleIDsWithAvailableUpdates addObject:bundleID];
            }
        }
    }

    return std::unique_ptr<PluginBlocklist>(new PluginBlocklist(bundleIDToMinimumSecureVersion, bundleIDToMinimumCompatibleVersion, bundleIDToBlockedVersions, bundleIDsWithAvailableUpdates));
}

PluginBlocklist::~PluginBlocklist()
{
}

NSArray *PluginBlocklist::splitOSVersion(NSString *osVersion)
{
    NSArray *components = [osVersion componentsSeparatedByString:@"."];

    NSMutableArray *result = [NSMutableArray array];

    for (NSUInteger i = 0; i < [components count]; ++i) {
        NSString *versionString = [[components subarrayWithRange:NSMakeRange(0, i + 1)] componentsJoinedByString:@"."];

        [result addObject:versionString];
    }

    return result;
}


PluginBlocklist::LoadPolicy PluginBlocklist::loadPolicyForPlugin(NSString *bundleIdentifier, NSString *bundleVersionString) const
{
    if (!bundleIdentifier || !bundleVersionString)
        return LoadPolicy::LoadNormally;

    // First, check for explicitly blocked versions.
    for (NSString *blockedVersion in [m_bundleIDToBlockedVersions objectForKey:bundleIdentifier]) {
        if ([blockedVersion isEqualToString:bundleVersionString])
            return LoadPolicy::BlockedForSecurity;
    }

    // Then, check if there's a forced minimum version for security issues.
    if (NSString *minimumSecureVersion = [m_bundleIDToMinimumSecureVersion objectForKey:bundleIdentifier]) {
        if ([bundleVersionString compare:minimumSecureVersion options:NSNumericSearch] == NSOrderedAscending)
            return LoadPolicy::BlockedForSecurity;
    }

    // Then, check if there's a forced minimum version for compatibility issues.
    if (NSString *minimumCompatibleVersion = [m_bundleIDToMinimumCompatibleVersion objectForKey:bundleIdentifier]) {
        if ([bundleVersionString compare:minimumCompatibleVersion options:NSNumericSearch] == NSOrderedAscending)
            return LoadPolicy::BlockedForCompatibility;
    }

    return LoadPolicy::LoadNormally;
}

bool PluginBlocklist::isUpdateAvailable(NSString *bundleIdentifier) const
{
    return [m_bundleIDsWithAvailableUpdates containsObject:bundleIdentifier];
}

PluginBlocklist::PluginBlocklist(NSDictionary *bundleIDToMinimumSecureVersion, NSDictionary *bundleIDToMinimumCompatibleVersion, NSDictionary *bundleIDToBlockedVersions, NSSet *bundleIDsWithAvailableUpdates)
    : m_bundleIDToMinimumSecureVersion { adoptNS([bundleIDToMinimumSecureVersion copy]) }
    , m_bundleIDToMinimumCompatibleVersion { adoptNS([bundleIDToMinimumCompatibleVersion copy]) }
    , m_bundleIDToBlockedVersions { adoptNS([bundleIDToBlockedVersions copy]) }
    , m_bundleIDsWithAvailableUpdates { adoptNS([bundleIDsWithAvailableUpdates copy]) }
{
}

}

#endif
