/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "_WKWebExtensionContextInternal.h"
#import "_WKWebExtensionPermission.h"
#import "_WKWebExtensionTab.h"
#import <WebCore/LocalizedStrings.h>

// This number was chosen arbitrarily based on testing with some popular extensions.
static constexpr size_t maximumCachedPermissionResults = 256;

namespace WebKit {

WebExtensionContext::WebExtensionContext(Ref<WebExtension>&& extension)
    : WebExtensionContext()
{
    m_extension = extension.ptr();

    StringBuilder baseURLBuilder;
    baseURLBuilder.append("webkit-extension://", uniqueIdentifier(), "/");

    m_baseURL = URL { baseURLBuilder.toString() };
}

NSError *WebExtensionContext::createError(Error error, NSString *customLocalizedDescription, NSError *underlyingError)
{
    _WKWebExtensionContextError errorCode;
    NSString *localizedDescription;

    switch (error) {
    case Error::Unknown:
        errorCode = _WKWebExtensionContextErrorUnknown;
        localizedDescription = WEB_UI_STRING("An unknown error has occurred.", "WKWebExtensionContextErrorUnknown description");
        break;

    case Error::AlreadyLoaded:
        errorCode = _WKWebExtensionContextErrorAlreadyLoaded;
        localizedDescription = WEB_UI_STRING("Extension context is already loaded.", "WKWebExtensionContextErrorAlreadyLoaded description");
        break;

    case Error::NotLoaded:
        errorCode = _WKWebExtensionContextErrorNotLoaded;
        localizedDescription = WEB_UI_STRING("Extension context is not loaded.", "WKWebExtensionContextErrorNotLoaded description");
        break;
    }

    if (customLocalizedDescription.length)
        localizedDescription = customLocalizedDescription;

    NSDictionary *userInfo = @{ NSLocalizedDescriptionKey: localizedDescription };
    if (underlyingError)
        userInfo = @{ NSLocalizedDescriptionKey: localizedDescription, NSUnderlyingErrorKey: underlyingError };

    return [[NSError alloc] initWithDomain:_WKWebExtensionContextErrorDomain code:errorCode userInfo:userInfo];
}

bool WebExtensionContext::load(WebExtensionController& controller, NSError **outError)
{
    if (outError)
        *outError = nil;

    if (isLoaded()) {
        if (outError)
            *outError = createError(Error::AlreadyLoaded);
        return false;
    }

    m_extensionController = controller;

    // FIXME: Load background page, inject content, etc.

    return true;
}

bool WebExtensionContext::unload(NSError **outError)
{
    if (outError)
        *outError = nil;

    if (!isLoaded()) {
        if (outError)
            *outError = createError(Error::NotLoaded);
        return false;
    }

    m_extensionController = nil;

    // FIXME: Unload background page, remove injected content, etc.

    return true;
}

void WebExtensionContext::setBaseURL(URL&& url)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    if (!url.isValid())
        return;

    StringBuilder baseURLBuilder;
    baseURLBuilder.append(url.protocol(), "://", url.host(), "/");

    m_baseURL = URL { baseURLBuilder.toString() };
}

bool WebExtensionContext::isURLForThisExtension(const URL& url)
{
    return protocolHostAndPortAreEqual(baseURL(), url);
}

void WebExtensionContext::setUniqueIdentifier(String&& uniqueIdentifier)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    if (uniqueIdentifier.isEmpty())
        uniqueIdentifier = m_baseURL.host().toString();

    m_uniqueIdentifier = uniqueIdentifier;
}

const WebExtensionContext::PermissionsMap& WebExtensionContext::grantedPermissions()
{
    return removeExpired(m_grantedPermissions, _WKWebExtensionContextGrantedPermissionsWereRemovedNotification);
}

void WebExtensionContext::setGrantedPermissions(PermissionsMap&& grantedPermissions)
{
    PermissionsSet removedPermissions;
    for (auto& entry : m_grantedPermissions)
        removedPermissions.add(entry.key);

    m_grantedPermissions = removeExpired(grantedPermissions);

    PermissionsSet addedPermissions;
    for (auto& entry : m_grantedPermissions) {
        if (removedPermissions.contains(entry.key)) {
            removedPermissions.remove(entry.key);
            continue;
        }

        addedPermissions.add(entry.key);
    }

    if (addedPermissions.isEmpty() && removedPermissions.isEmpty())
        return;

    removeDeniedPermissions(addedPermissions);

    postAsyncNotification(_WKWebExtensionContextGrantedPermissionsWereRemovedNotification, removedPermissions);
    postAsyncNotification(_WKWebExtensionContextPermissionsWereGrantedNotification, addedPermissions);
}

const WebExtensionContext::PermissionsMap& WebExtensionContext::deniedPermissions()
{
    return removeExpired(m_deniedPermissions, _WKWebExtensionContextDeniedPermissionsWereRemovedNotification);
}

void WebExtensionContext::setDeniedPermissions(PermissionsMap&& deniedPermissions)
{
    PermissionsSet removedPermissions;
    for (auto& entry : m_deniedPermissions)
        removedPermissions.add(entry.key);

    m_deniedPermissions = removeExpired(deniedPermissions);

    PermissionsSet addedPermissions;
    for (auto& entry : m_deniedPermissions) {
        if (removedPermissions.contains(entry.key)) {
            removedPermissions.remove(entry.key);
            continue;
        }

        addedPermissions.add(entry.key);
    }

    if (addedPermissions.isEmpty() && removedPermissions.isEmpty())
        return;

    removeGrantedPermissions(addedPermissions);

    postAsyncNotification(_WKWebExtensionContextDeniedPermissionsWereRemovedNotification, removedPermissions);
    postAsyncNotification(_WKWebExtensionContextPermissionsWereDeniedNotification, addedPermissions);
}

const WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::grantedPermissionMatchPatterns()
{
    return removeExpired(m_grantedPermissionMatchPatterns, _WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification);
}

void WebExtensionContext::setGrantedPermissionMatchPatterns(PermissionMatchPatternsMap&& grantedPermissionMatchPatterns)
{
    MatchPatternSet removedMatchPatterns;
    for (auto& entry : m_grantedPermissionMatchPatterns)
        removedMatchPatterns.add(entry.key);

    m_grantedPermissionMatchPatterns = removeExpired(grantedPermissionMatchPatterns);

    MatchPatternSet addedMatchPatterns;
    for (auto& entry : m_grantedPermissionMatchPatterns) {
        if (removedMatchPatterns.contains(entry.key)) {
            removedMatchPatterns.remove(entry.key);
            continue;
        }

        addedMatchPatterns.add(entry.key);
    }

    if (addedMatchPatterns.isEmpty() && removedMatchPatterns.isEmpty())
        return;

    removeDeniedPermissionMatchPatterns(addedMatchPatterns, EqualityOnly::Yes);
    clearCachedPermissionStates();

    postAsyncNotification(_WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification, removedMatchPatterns);
    postAsyncNotification(_WKWebExtensionContextPermissionMatchPatternsWereGrantedNotification, addedMatchPatterns);
}

const WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::deniedPermissionMatchPatterns()
{
    return removeExpired(m_deniedPermissionMatchPatterns, _WKWebExtensionContextDeniedPermissionMatchPatternsWereRemovedNotification);
}

void WebExtensionContext::setDeniedPermissionMatchPatterns(PermissionMatchPatternsMap&& deniedPermissionMatchPatterns)
{
    MatchPatternSet removedMatchPatterns;
    for (auto& entry : m_deniedPermissionMatchPatterns)
        removedMatchPatterns.add(entry.key);

    m_deniedPermissionMatchPatterns = removeExpired(deniedPermissionMatchPatterns);

    MatchPatternSet addedMatchPatterns;
    for (auto& entry : m_deniedPermissionMatchPatterns) {
        if (removedMatchPatterns.contains(entry.key)) {
            removedMatchPatterns.remove(entry.key);
            continue;
        }

        addedMatchPatterns.add(entry.key);
    }

    if (addedMatchPatterns.isEmpty() && removedMatchPatterns.isEmpty())
        return;

    removeGrantedPermissionMatchPatterns(addedMatchPatterns, EqualityOnly::Yes);
    clearCachedPermissionStates();

    postAsyncNotification(_WKWebExtensionContextDeniedPermissionMatchPatternsWereRemovedNotification, removedMatchPatterns);
    postAsyncNotification(_WKWebExtensionContextPermissionMatchPatternsWereDeniedNotification, addedMatchPatterns);
}

void WebExtensionContext::postAsyncNotification(NSNotificationName notificationName, PermissionsSet& permissions)
{
    if (permissions.isEmpty())
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
        [NSNotificationCenter.defaultCenter postNotificationName:notificationName object:wrapper() userInfo:@{ _WKWebExtensionContextNotificationUserInfoKeyPermissions: toAPI(permissions) }];
    });
}

void WebExtensionContext::postAsyncNotification(NSNotificationName notificationName, MatchPatternSet& matchPatterns)
{
    if (matchPatterns.isEmpty())
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
        [NSNotificationCenter.defaultCenter postNotificationName:notificationName object:wrapper() userInfo:@{ _WKWebExtensionContextNotificationUserInfoKeyMatchPatterns: toAPI(matchPatterns) }];
    });
}

void WebExtensionContext::grantPermissions(PermissionsSet&& permissions, WallTime expirationDate)
{
    if (permissions.isEmpty())
        return;

    for (auto& permission : permissions)
        m_grantedPermissions.add(permission, expirationDate);

    removeDeniedPermissions(permissions);

    postAsyncNotification(_WKWebExtensionContextPermissionsWereGrantedNotification, permissions);
}

void WebExtensionContext::denyPermissions(PermissionsSet&& permissions, WallTime expirationDate)
{
    if (permissions.isEmpty())
        return;

    for (auto& permission : permissions)
        m_deniedPermissions.add(permission, expirationDate);

    removeGrantedPermissions(permissions);

    postAsyncNotification(_WKWebExtensionContextPermissionsWereDeniedNotification, permissions);
}

void WebExtensionContext::grantPermissionMatchPatterns(MatchPatternSet&& permissionMatchPatterns, WallTime expirationDate)
{
    if (permissionMatchPatterns.isEmpty())
        return;

    for (auto& pattern : permissionMatchPatterns)
        m_grantedPermissionMatchPatterns.add(pattern, expirationDate);

    removeDeniedPermissionMatchPatterns(permissionMatchPatterns, EqualityOnly::Yes);
    clearCachedPermissionStates();

    postAsyncNotification(_WKWebExtensionContextPermissionMatchPatternsWereGrantedNotification, permissionMatchPatterns);
}

void WebExtensionContext::denyPermissionMatchPatterns(MatchPatternSet&& permissionMatchPatterns, WallTime expirationDate)
{
    if (permissionMatchPatterns.isEmpty())
        return;

    for (auto& pattern : permissionMatchPatterns)
        m_deniedPermissionMatchPatterns.add(pattern, expirationDate);

    removeGrantedPermissionMatchPatterns(permissionMatchPatterns, EqualityOnly::Yes);
    clearCachedPermissionStates();

    postAsyncNotification(_WKWebExtensionContextPermissionMatchPatternsWereDeniedNotification, permissionMatchPatterns);
}

void WebExtensionContext::removeGrantedPermissions(PermissionsSet& permissionsToRemove)
{
    removePermissions(m_grantedPermissions, permissionsToRemove, _WKWebExtensionContextGrantedPermissionsWereRemovedNotification);
}

void WebExtensionContext::removeGrantedPermissionMatchPatterns(MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly)
{
    removePermissionMatchPatterns(m_grantedPermissionMatchPatterns, matchPatternsToRemove, equalityOnly, _WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification);
}

void WebExtensionContext::removeDeniedPermissions(PermissionsSet& permissionsToRemove)
{
    removePermissions(m_deniedPermissions, permissionsToRemove, _WKWebExtensionContextDeniedPermissionsWereRemovedNotification);
}

void WebExtensionContext::removeDeniedPermissionMatchPatterns(MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly)
{
    removePermissionMatchPatterns(m_deniedPermissionMatchPatterns, matchPatternsToRemove, equalityOnly, _WKWebExtensionContextDeniedPermissionsWereRemovedNotification);
}

void WebExtensionContext::removePermissions(PermissionsMap& permissionMap, PermissionsSet& permissionsToRemove, NSNotificationName notificationName)
{
    if (permissionsToRemove.isEmpty())
        return;

    PermissionsSet removedPermissions;
    permissionMap.removeIf([&](auto& entry) {
        if (permissionsToRemove.contains(entry.key)) {
            removedPermissions.add(entry.key);
            return true;
        }

        return false;
    });

    if (removedPermissions.isEmpty() || !notificationName)
        return;

    postAsyncNotification(notificationName, removedPermissions);
}

void WebExtensionContext::removePermissionMatchPatterns(PermissionMatchPatternsMap& matchPatternMap, MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly, NSNotificationName notificationName)
{
    if (matchPatternsToRemove.isEmpty())
        return;

    MatchPatternSet removedMatchPatterns;
    matchPatternMap.removeIf([&](auto& entry) {
        if (matchPatternsToRemove.contains(entry.key)) {
            removedMatchPatterns.add(entry.key);
            return true;
        }

        if (equalityOnly == EqualityOnly::Yes)
            return false;

        for (auto& patternToRemove : matchPatternsToRemove) {
            if (patternToRemove->matchesPattern(entry.key, WebExtensionMatchPattern::Options::IgnorePaths)) {
                removedMatchPatterns.add(entry.key);
                return true;
            }
        }

        return false;
    });

    if (removedMatchPatterns.isEmpty() || !notificationName)
        return;

    clearCachedPermissionStates();

    postAsyncNotification(notificationName, removedMatchPatterns);
}

WebExtensionContext::PermissionsMap& WebExtensionContext::removeExpired(PermissionsMap& permissionMap, NSNotificationName notificationName)
{
    WallTime currentTime = WallTime::now();

    PermissionsSet removedPermissions;
    permissionMap.removeIf([&](auto& entry) {
        if (entry.value <= currentTime) {
            removedPermissions.add(entry.key);
            return true;
        }

        return false;
    });

    if (removedPermissions.isEmpty() || !notificationName)
        return permissionMap;

    postAsyncNotification(notificationName, removedPermissions);

    return permissionMap;
}

WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::removeExpired(PermissionMatchPatternsMap& matchPatternMap, NSNotificationName notificationName)
{
    WallTime currentTime = WallTime::now();

    MatchPatternSet removedMatchPatterns;
    matchPatternMap.removeIf([&](auto& entry) {
        if (entry.value <= currentTime) {
            removedMatchPatterns.add(entry.key);
            return true;
        }

        return false;
    });

    if (removedMatchPatterns.isEmpty() || !notificationName)
        return matchPatternMap;

    clearCachedPermissionStates();

    postAsyncNotification(notificationName, removedMatchPatterns);

    return matchPatternMap;
}

bool WebExtensionContext::hasPermission(const String& permission, _WKWebExtensionTab *tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!permission.isEmpty());

    options.add(PermissionStateOptions::SkipRequestedPermissions);

    switch (permissionState(permission, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return false;

    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return true;
    }
}

bool WebExtensionContext::hasPermission(const URL& url, _WKWebExtensionTab *tab, OptionSet<PermissionStateOptions> options)
{
    options.add(PermissionStateOptions::SkipRequestedPermissions);

    switch (permissionState(url, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return false;

    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return true;
    }
}

WebExtensionContext::PermissionState WebExtensionContext::permissionState(const String& permission, _WKWebExtensionTab *tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!permission.isEmpty());

    if (tab && hasActiveUserGesture(tab)) {
        // An active user gesture grants the "tabs" permission.
        if (permission == String(_WKWebExtensionPermissionTabs))
            return PermissionState::GrantedExplicitly;
    }

    if (!WebExtension::supportedPermissions().contains(permission))
        return PermissionState::Unknown;

    if (deniedPermissions().contains(permission))
        return PermissionState::DeniedExplicitly;

    if (grantedPermissions().contains(permission))
        return PermissionState::GrantedExplicitly;

    if (options.contains(PermissionStateOptions::SkipRequestedPermissions))
        return PermissionState::Unknown;

    if (extension().hasRequestedPermission(permission))
        return PermissionState::RequestedExplicitly;

    return PermissionState::Unknown;
}

WebExtensionContext::PermissionState WebExtensionContext::permissionState(const URL& coreURL, _WKWebExtensionTab *tab, OptionSet<PermissionStateOptions> options)
{
    if (coreURL.isEmpty())
        return PermissionState::Unknown;

    if (isURLForThisExtension(coreURL))
        return PermissionState::GrantedImplicitly;

    NSURL *url = coreURL;
    ASSERT(url);

    if (!WebExtensionMatchPattern::validSchemes().contains(url.scheme))
        return PermissionState::Unknown;

    if (tab && [[m_temporaryTabPermissionMatchPatterns objectForKey:tab] matchesURL:url])
        return PermissionState::GrantedExplicitly;

    bool skipRequestedPermissions = options.contains(PermissionStateOptions::SkipRequestedPermissions);

    // Access the maps here to remove any expired entries, and only do it once for this call.
    auto& grantedPermissionMatchPatterns = this->grantedPermissionMatchPatterns();
    auto& deniedPermissionMatchPatterns = this->deniedPermissionMatchPatterns();

    // If the cache still has the URL, then it has not expired.
    if (m_cachedPermissionURLs.contains(coreURL)) {
        PermissionState cachedState = m_cachedPermissionStates.get(coreURL);

        // We only want to return an unknown cached state if the SkippingRequestedPermissions option isn't used.
        if (cachedState != PermissionState::Unknown || skipRequestedPermissions) {
            // Move the URL to the end, so it stays in the cache longer as a recent hit.
            m_cachedPermissionURLs.appendOrMoveToLast(coreURL);

            if ((cachedState == PermissionState::RequestedExplicitly || cachedState == PermissionState::RequestedImplicitly) && skipRequestedPermissions)
                return PermissionState::Unknown;

            return cachedState;
        }
    }

    auto cacheResultAndReturn = ^PermissionState(PermissionState result) {
        m_cachedPermissionURLs.appendOrMoveToLast(coreURL);
        m_cachedPermissionStates.set(coreURL, result);

        ASSERT(m_cachedPermissionURLs.size() == m_cachedPermissionURLs.size());

        if (m_cachedPermissionURLs.size() <= maximumCachedPermissionResults)
            return result;

        URL firstCachedURL = m_cachedPermissionURLs.takeFirst();
        m_cachedPermissionStates.remove(firstCachedURL);

        ASSERT(m_cachedPermissionURLs.size() == m_cachedPermissionURLs.size());

        return result;
    };

    // First, check for patterns that are specific to certain domains, ignoring wildcard host patterns that
    // match all hosts. The order is denied, then granted. This makes sure denied takes precedence over granted.

    auto urlMatchesPatternIgnoringWildcardHostPatterns = ^(WebExtensionMatchPattern& pattern) {
        if (pattern.matchesAllHosts())
            return false;
        return pattern.matchesURL(url);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(deniedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::DeniedExplicitly);
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(grantedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::GrantedExplicitly);
    }

    // Next, check for patterns that are wildcard host patterns that match all hosts (<all_urls>, *://*/*, etc),
    // also checked in denied, then granted order. Doing these wildcard patterns separately allows for blanket
    // patterns to be set as default policies while allowing for specific domains to still be granted or denied.

    auto urlMatchesWildcardHostPatterns = ^(WebExtensionMatchPattern& pattern) {
        if (!pattern.matchesAllHosts())
            return false;
        return pattern.matchesURL(url);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(deniedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::DeniedImplicitly);
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(grantedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::GrantedImplicitly);
    }

    // Finally, check for requested patterns, allowing any pattern that matches. This is the default state
    // of the extension before any patterns are granted or denied, so it should always be last.

    if (skipRequestedPermissions)
        return cacheResultAndReturn(PermissionState::Unknown);

    auto requestedMatchPatterns = m_extension->allRequestedMatchPatterns();
    for (auto& requestedMatchPattern : requestedMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(requestedMatchPattern))
            return cacheResultAndReturn(PermissionState::RequestedExplicitly);

        if (urlMatchesWildcardHostPatterns(requestedMatchPattern))
            return cacheResultAndReturn(PermissionState::RequestedImplicitly);
    }

    if (options.contains(PermissionStateOptions::RequestedWithTabsPermission) && hasPermission(_WKWebExtensionPermissionTabs, tab, options))
        return PermissionState::RequestedImplicitly;

    return cacheResultAndReturn(PermissionState::Unknown);
}

WebExtensionContext::PermissionState WebExtensionContext::permissionState(WebExtensionMatchPattern& pattern, _WKWebExtensionTab *tab, OptionSet<PermissionStateOptions> options)
{
    if (!pattern.isValid())
        return PermissionState::Unknown;

    if (pattern.matchesURL(baseURL()))
        return PermissionState::GrantedImplicitly;

    if (!pattern.matchesAllURLs() && !WebExtensionMatchPattern::validSchemes().contains(pattern.scheme()))
        return PermissionState::Unknown;

    if (tab && [[m_temporaryTabPermissionMatchPatterns objectForKey:tab] matchesPattern:pattern.wrapper()])
        return PermissionState::GrantedExplicitly;

    // Access the maps here to remove any expired entries, and only do it once for this call.
    auto& grantedPermissionMatchPatterns = this->grantedPermissionMatchPatterns();
    auto& deniedPermissionMatchPatterns = this->deniedPermissionMatchPatterns();

    // First, check for patterns that are specific to certain domains, ignoring wildcard host patterns that
    // match all hosts. The order is denied, then granted. This makes sure denied takes precedence over granted.

    auto urlMatchesPatternIgnoringWildcardHostPatterns = ^(WebExtensionMatchPattern& otherPattern) {
        if (pattern.matchesAllHosts())
            return false;
        return pattern.matchesPattern(otherPattern);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(deniedPermissionEntry.key))
            return PermissionState::DeniedExplicitly;
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(grantedPermissionEntry.key))
            return PermissionState::GrantedExplicitly;
    }

    // Next, check for patterns that are wildcard host patterns that match all hosts (<all_urls>, *://*/*, etc),
    // also checked in denied, then granted order. Doing these wildcard patterns separately allows for blanket
    // patterns to be set as default policies while allowing for specific domains to still be granted or denied.

    auto urlMatchesWildcardHostPatterns = ^(WebExtensionMatchPattern& otherPattern) {
        if (!pattern.matchesAllHosts())
            return false;
        return pattern.matchesPattern(otherPattern);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(deniedPermissionEntry.key))
            return PermissionState::DeniedImplicitly;
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(grantedPermissionEntry.key))
            return PermissionState::GrantedImplicitly;
    }

    // Finally, check for requested patterns, allowing any pattern that matches. This is the default state
    // of the extension before any patterns are granted or denied, so it should always be last.

    if (options.contains(PermissionStateOptions::SkipRequestedPermissions))
        return PermissionState::Unknown;

    auto requestedMatchPatterns = m_extension->allRequestedMatchPatterns();
    for (auto& requestedMatchPattern : requestedMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(requestedMatchPattern))
            return PermissionState::RequestedExplicitly;

        if (urlMatchesWildcardHostPatterns(requestedMatchPattern))
            return PermissionState::RequestedImplicitly;
    }

    if (options.contains(PermissionStateOptions::RequestedWithTabsPermission) && hasPermission(_WKWebExtensionPermissionTabs, tab, options))
        return PermissionState::RequestedImplicitly;

    return PermissionState::Unknown;
}

void WebExtensionContext::setPermissionState(PermissionState state, const String& permission, WallTime expirationDate)
{
    ASSERT(!permission.isEmpty());

    switch (state) {
    case PermissionState::DeniedExplicitly:
        denyPermissions({ permission }, expirationDate);
        break;

    case PermissionState::Unknown: {
        PermissionsSet permissionsToRemove = { permission };
        removeGrantedPermissions(permissionsToRemove);
        removeDeniedPermissions(permissionsToRemove);
        break;
    }

    case PermissionState::GrantedExplicitly:
        grantPermissions({ permission }, expirationDate);
        break;

    case PermissionState::DeniedImplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
    case PermissionState::GrantedImplicitly:
        ASSERT_NOT_REACHED();
        break;
    }
}

void WebExtensionContext::setPermissionState(PermissionState state, const URL& url, WallTime expirationDate)
{
    ASSERT(!url.isEmpty());

    auto pattern = WebExtensionMatchPattern::getOrCreate(url.protocol().toString(), url.host().toString(), url.path().toString());
    if (!pattern)
        return;

    setPermissionState(state, *pattern, expirationDate);
}

void WebExtensionContext::setPermissionState(PermissionState state, WebExtensionMatchPattern& pattern, WallTime expirationDate)
{
    ASSERT(pattern.isValid());

    switch (state) {
    case PermissionState::DeniedExplicitly:
        denyPermissionMatchPatterns({ pattern }, expirationDate);
        break;

    case PermissionState::Unknown: {
        MatchPatternSet patternsToRemove = { pattern };
        removeGrantedPermissionMatchPatterns(patternsToRemove, EqualityOnly::Yes);
        removeDeniedPermissionMatchPatterns(patternsToRemove, EqualityOnly::Yes);
        break;
    }

    case PermissionState::GrantedExplicitly:
        grantPermissionMatchPatterns({ pattern }, expirationDate);
        break;

    case PermissionState::DeniedImplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
    case PermissionState::GrantedImplicitly:
        ASSERT_NOT_REACHED();
        break;
    }
}

void WebExtensionContext::clearCachedPermissionStates()
{
    m_cachedPermissionStates.clear();
    m_cachedPermissionURLs.clear();
}

bool WebExtensionContext::hasAccessToAllURLs()
{
    for (auto& pattern : currentPermissionMatchPatterns()) {
        if (pattern->matchesAllURLs())
            return true;
    }

    return false;
}

bool WebExtensionContext::hasAccessToAllHosts()
{
    for (auto& pattern : currentPermissionMatchPatterns()) {
        if (pattern->matchesAllHosts())
            return true;
    }

    return false;
}

void WebExtensionContext::userGesturePerformed(_WKWebExtensionTab *tab)
{
    ASSERT(tab);

    // Nothing else to do if the extension does not have the activeTab permissions.
    if (!hasPermission(_WKWebExtensionPermissionActiveTab))
        return;

    if (![tab respondsToSelector:@selector(urlForWebExtensionContext:)])
        return;

    NSURL *currentURL = [tab urlForWebExtensionContext:wrapper()];
    if (!currentURL)
        return;

    _WKWebExtensionMatchPattern *pattern = [m_temporaryTabPermissionMatchPatterns objectForKey:tab];

    // Nothing to do if the tab already has a pattern matching the current URL.
    if (pattern && [pattern matchesURL:currentURL])
        return;

    // A pattern should not exist, since it should be cleared in cancelUserGesture
    // on any navigation between different hosts.
    ASSERT(!pattern);

    if (!m_temporaryTabPermissionMatchPatterns)
        m_temporaryTabPermissionMatchPatterns = [NSMapTable weakToStrongObjectsMapTable];

    // Grant the tab a temporary permission to access to a pattern matching the current URL's scheme and host for all paths.
    pattern = [_WKWebExtensionMatchPattern matchPatternWithScheme:currentURL.scheme host:currentURL.host path:@"/*"];
    [m_temporaryTabPermissionMatchPatterns setObject:pattern forKey:tab];
}

bool WebExtensionContext::hasActiveUserGesture(_WKWebExtensionTab *tab) const
{
    ASSERT(tab);

    if (!m_temporaryTabPermissionMatchPatterns)
        return false;

    if (![tab respondsToSelector:@selector(urlForWebExtensionContext:)])
        return false;

    NSURL *currentURL = [tab urlForWebExtensionContext:wrapper()];
    return [[m_temporaryTabPermissionMatchPatterns objectForKey:tab] matchesURL:currentURL];
}

void WebExtensionContext::cancelUserGesture(_WKWebExtensionTab *tab)
{
    ASSERT(tab);

    if (!m_temporaryTabPermissionMatchPatterns)
        return;

    [m_temporaryTabPermissionMatchPatterns removeObjectForKey:tab];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
