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

#import "InjectUserScriptImmediately.h"
#import "WKNavigationActionPrivate.h"
#import "WKNavigationDelegatePrivate.h"
#import "WKPreferencesPrivate.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewInternal.h"
#import "WebExtensionURLSchemeHandler.h"
#import "WebPageProxy.h"
#import "WebUserContentControllerProxy.h"
#import "_WKWebExtensionContextInternal.h"
#import "_WKWebExtensionPermission.h"
#import "_WKWebExtensionTab.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/UserScript.h>
#import <wtf/BlockPtr.h>
#import <wtf/URLParser.h>
#import <wtf/cocoa/VectorCocoa.h>

// This number was chosen arbitrarily based on testing with some popular extensions.
static constexpr size_t maximumCachedPermissionResults = 256;

@interface _WKWebExtensionContextDelegate : NSObject <WKNavigationDelegate, WKUIDelegate> {
    WeakPtr<WebKit::WebExtensionContext> _webExtensionContext;
}

- (instancetype)initWithWebExtensionContext:(WebKit::WebExtensionContext&)context;

@end

@implementation _WKWebExtensionContextDelegate

- (instancetype)initWithWebExtensionContext:(WebKit::WebExtensionContext&)context
{
    if (!(self = [super init]))
        return nil;

    _webExtensionContext = context;

    return self;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if (!_webExtensionContext)
        return;

    if (_webExtensionContext->decidePolicyForNavigationAction(webView, navigationAction)) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }

    decisionHandler(WKNavigationActionPolicyCancel);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    if (!_webExtensionContext)
        return;

    _webExtensionContext->didFinishNavigation(webView, navigation);
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    if (!_webExtensionContext)
        return;

    _webExtensionContext->didFailNavigation(webView, navigation, error);
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    if (!_webExtensionContext)
        return;

    _webExtensionContext->webViewWebContentProcessDidTerminate(webView);
}

@end

namespace WebKit {

WebExtensionContext::WebExtensionContext(Ref<WebExtension>&& extension)
    : WebExtensionContext()
{
    m_extension = extension.ptr();

    StringBuilder baseURLBuilder;
    baseURLBuilder.append("webkit-extension://", uniqueIdentifier(), "/");

    m_baseURL = URL { baseURLBuilder.toString() };

    m_delegate = [[_WKWebExtensionContextDelegate alloc] initWithWebExtensionContext:*this];
}

static _WKWebExtensionContextError toAPI(WebExtensionContext::Error error)
{
    switch (error) {
    case WebExtensionContext::Error::Unknown:
        return _WKWebExtensionContextErrorUnknown;
    case WebExtensionContext::Error::AlreadyLoaded:
        return _WKWebExtensionContextErrorAlreadyLoaded;
    case WebExtensionContext::Error::NotLoaded:
        return _WKWebExtensionContextErrorNotLoaded;
    case WebExtensionContext::Error::BaseURLTaken:
        return _WKWebExtensionContextErrorBaseURLTaken;
    }
}

NSError *WebExtensionContext::createError(Error error, NSString *customLocalizedDescription, NSError *underlyingError)
{
    auto errorCode = toAPI(error);
    NSString *localizedDescription;

    switch (error) {
    case Error::Unknown:
        localizedDescription = WEB_UI_STRING_KEY("An unknown error has occurred.", "An unknown error has occurred. (WKWebExtensionContext)", "WKWebExtensionContextErrorUnknown description");
        break;

    case Error::AlreadyLoaded:
        localizedDescription = WEB_UI_STRING("Extension context is already loaded.", "WKWebExtensionContextErrorAlreadyLoaded description");
        break;

    case Error::NotLoaded:
        localizedDescription = WEB_UI_STRING("Extension context is not loaded.", "WKWebExtensionContextErrorNotLoaded description");
        break;

    case Error::BaseURLTaken:
        localizedDescription = WEB_UI_STRING("Another extension context is loaded with the same base URL.", "WKWebExtensionContextErrorBaseURLTaken description");
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
    m_contentScriptWorld = API::ContentWorld::sharedWorldWithName(makeString("WebExtension-", m_uniqueIdentifier));

    // FIXME: <https://webkit.org/b/248430> Move local storage (if base URL changed).

    loadBackgroundWebViewDuringLoad();

    // FIXME: <https://webkit.org/b/248429> Support dynamic content scripts by loading them from storage here.

    addInjectedContent();

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

    unloadBackgroundWebView();
    removeInjectedContent();

    m_extensionController = nil;
    m_contentScriptWorld = nullptr;

    return true;
}

void WebExtensionContext::setBaseURL(URL&& url)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    if (!url.isValid())
        return;

    auto canonicalScheme = WTF::URLParser::maybeCanonicalizeScheme(url.protocol());
    ASSERT(canonicalScheme);
    if (!canonicalScheme)
        return;

    StringBuilder baseURLBuilder;
    baseURLBuilder.append(canonicalScheme.value(), "://", url.host(), "/");

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

const WebExtensionContext::InjectedContentVector& WebExtensionContext::injectedContents()
{
    // FIXME: <https://webkit.org/b/248429> Support dynamic content scripts by including them here.
    return m_extension->staticInjectedContents();
}

bool WebExtensionContext::hasInjectedContentForURL(NSURL *url)
{
    ASSERT(url);

    for (auto& injectedContent : injectedContents()) {
        // FIXME: <https://webkit.org/b/246492> Add support for exclude globs.
        bool isExcluded = false;
        for (auto& excludeMatchPattern : injectedContent.excludeMatchPatterns) {
            if (excludeMatchPattern->matchesURL(url)) {
                isExcluded = true;
                break;
            }
        }

        if (isExcluded)
            continue;

        // FIXME: <https://webkit.org/b/246492> Add support for include globs.
        for (auto& includeMatchPattern : injectedContent.includeMatchPatterns) {
            if (includeMatchPattern->matchesURL(url))
                return true;
        }
    }

    return false;
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

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, notificationName = retainPtr(notificationName), permissions]() {
        [NSNotificationCenter.defaultCenter postNotificationName:notificationName.get() object:wrapper() userInfo:@{ _WKWebExtensionContextNotificationUserInfoKeyPermissions: toAPI(permissions) }];
    }).get());
}

void WebExtensionContext::postAsyncNotification(NSNotificationName notificationName, MatchPatternSet& matchPatterns)
{
    if (matchPatterns.isEmpty())
        return;

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, notificationName = retainPtr(notificationName), matchPatterns]() {
        [NSNotificationCenter.defaultCenter postNotificationName:notificationName.get() object:wrapper() userInfo:@{ _WKWebExtensionContextNotificationUserInfoKeyMatchPatterns: toAPI(matchPatterns) }];
    }).get());
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

    addInjectedContent(injectedContents(), permissionMatchPatterns);

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

    updateInjectedContent();

    postAsyncNotification(_WKWebExtensionContextPermissionMatchPatternsWereDeniedNotification, permissionMatchPatterns);
}

void WebExtensionContext::removeGrantedPermissions(PermissionsSet& permissionsToRemove)
{
    removePermissions(m_grantedPermissions, permissionsToRemove, _WKWebExtensionContextGrantedPermissionsWereRemovedNotification);
}

void WebExtensionContext::removeGrantedPermissionMatchPatterns(MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly)
{
    removePermissionMatchPatterns(m_grantedPermissionMatchPatterns, matchPatternsToRemove, equalityOnly, _WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification);

    removeInjectedContent(matchPatternsToRemove);
}

void WebExtensionContext::removeDeniedPermissions(PermissionsSet& permissionsToRemove)
{
    removePermissions(m_deniedPermissions, permissionsToRemove, _WKWebExtensionContextDeniedPermissionsWereRemovedNotification);
}

void WebExtensionContext::removeDeniedPermissionMatchPatterns(MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly)
{
    removePermissionMatchPatterns(m_deniedPermissionMatchPatterns, matchPatternsToRemove, equalityOnly, _WKWebExtensionContextDeniedPermissionsWereRemovedNotification);

    updateInjectedContent();
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

void WebExtensionContext::setTestingMode(bool testingMode)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    m_testingMode = testingMode;
}

WKWebViewConfiguration *WebExtensionContext::webViewConfiguration()
{
    ASSERT(isLoaded());

    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];

    // Use the weak property to avoid a reference cycle while an extension web view is being held by the context.
    configuration._weakWebExtensionController = m_extensionController->wrapper();

    configuration._processDisplayName = extension().webProcessDisplayName();

    WKPreferences *preferences = configuration.preferences;
#if PLATFORM(MAC)
    preferences._domTimersThrottlingEnabled = NO;
#endif
    preferences._pageVisibilityBasedProcessSuppressionEnabled = NO;

    // FIXME: Configure other extension web view configuration properties.

    return configuration;
}

URL WebExtensionContext::backgroundContentURL()
{
    ASSERT(extension().hasBackgroundContent());
    return URL { m_baseURL, extension().backgroundContentPath() };
}

void WebExtensionContext::loadBackgroundWebViewDuringLoad()
{
    ASSERT(isLoaded());

    if (!extension().hasBackgroundContent())
        return;

    // FIXME: <https://webkit.org/b/246483> Handle non-persistent background pages differently here.
    loadBackgroundWebView();
}

void WebExtensionContext::loadBackgroundWebView()
{
    ASSERT(isLoaded());

    if (!extension().hasBackgroundContent())
        return;

    ASSERT(!m_backgroundWebView);
    m_backgroundWebView = [[WKWebView alloc] initWithFrame:CGRectZero configuration:webViewConfiguration()];

    m_backgroundWebView.get().UIDelegate = m_delegate.get();
    m_backgroundWebView.get().navigationDelegate = m_delegate.get();

    if (extension().backgroundContentIsServiceWorker())
        m_backgroundWebView.get()._remoteInspectionNameOverride = WEB_UI_FORMAT_CFSTRING("%@ — Extension Service Worker", "Label for an inspectable Web Extension service worker", (__bridge CFStringRef)extension().displayShortName());
    else
        m_backgroundWebView.get()._remoteInspectionNameOverride = WEB_UI_FORMAT_CFSTRING("%@ — Extension Background Page", "Label for an inspectable Web Extension background page", (__bridge CFStringRef)extension().displayShortName());

    extension().removeError(WebExtension::Error::BackgroundContentFailedToLoad);

    if (!extension().backgroundContentIsServiceWorker()) {
        [m_backgroundWebView loadRequest:[NSURLRequest requestWithURL:backgroundContentURL()]];
        return;
    }

    [m_backgroundWebView _loadServiceWorker:backgroundContentURL() usingModules:extension().backgroundContentUsesModules() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }](BOOL success) {
        if (!success) {
            extension().recordError(extension().createError(WebExtension::Error::BackgroundContentFailedToLoad), WebExtension::SuppressNotification::No);
            return;
        }

        performTasksAfterBackgroundContentLoads();
    }).get()];
}

void WebExtensionContext::unloadBackgroundWebView()
{
    if (!m_backgroundWebView)
        return;

    // FIXME: <https://webkit.org/b/246484> Disconnect message ports for the background web view.

    [m_backgroundWebView _close];
    m_backgroundWebView = nil;
}

void WebExtensionContext::performTasksAfterBackgroundContentLoads()
{
    // FIXME: <https://webkit.org/b/246483> Implement. Fire setup and install events (if needed), perform pending actions, schedule non-persistent page to unload (if needed), etc.
}

bool WebExtensionContext::decidePolicyForNavigationAction(WKWebView *webView, WKNavigationAction *navigationAction)
{
    // FIXME: <https://webkit.org/b/246485> Handle inspector background pages in the assert.
    ASSERT(webView == m_backgroundWebView);

    NSURL *url = navigationAction.request.URL;
    if (!navigationAction.targetFrame.isMainFrame || isURLForThisExtension(url))
        return true;

    return false;
}

void WebExtensionContext::didFinishNavigation(WKWebView *webView, WKNavigation *)
{
    if (webView != m_backgroundWebView)
        return;

    // When didFinishNavigation fires for a service worker, the service worker has not executed yet.
    // The service worker will notify the load via a completion handler instead.
    if (extension().backgroundContentIsServiceWorker())
        return;

    performTasksAfterBackgroundContentLoads();
}

void WebExtensionContext::didFailNavigation(WKWebView *webView, WKNavigation *, NSError *error)
{
    if (webView != m_backgroundWebView)
        return;

    extension().recordError(extension().createError(WebExtension::Error::BackgroundContentFailedToLoad, nil, error), WebExtension::SuppressNotification::No);
}

void WebExtensionContext::webViewWebContentProcessDidTerminate(WKWebView *webView)
{
    // FIXME: <https://webkit.org/b/246484> Disconnect message ports for the crashed web view.

    if (webView == m_backgroundWebView) {
        m_backgroundWebView = nullptr;
        loadBackgroundWebView();
        return;
    }

    // FIXME: <https://webkit.org/b/246485> Handle inspector background pages too.

    ASSERT_NOT_REACHED();
}

void WebExtensionContext::addInjectedContent(const InjectedContentVector& injectedContents)
{
    if (!isLoaded())
        return;

    // Only add content for one "all hosts" pattern if the extension has the permission.
    // This avoids duplicate injected content if individual hosts are granted in addition to "all hosts".
    if (hasAccessToAllHosts()) {
        addInjectedContent(injectedContents, WebExtensionMatchPattern::allHostsAndSchemesMatchPattern());
        return;
    }

    MatchPatternSet grantedMatchPatterns;
    for (auto& pattern : currentPermissionMatchPatterns())
        grantedMatchPatterns.add(pattern);

    addInjectedContent(injectedContents, grantedMatchPatterns);
}

void WebExtensionContext::addInjectedContent(const InjectedContentVector& injectedContents, MatchPatternSet& grantedMatchPatterns)
{
    if (!isLoaded())
        return;

    if (hasAccessToAllHosts()) {
        // If this is not currently granting "all hosts", then we can return early. This means
        // the "all hosts" pattern injected content was added already, and no content needs added.
        // Continuing here would add multiple copies of injected content, one for "all hosts" and
        // another for individually granted hosts.
        if (!WebExtensionMatchPattern::patternsMatchAllHosts(grantedMatchPatterns))
            return;

        // Since we are granting "all hosts" we want to remove any previously added content since
        // "all hosts" will cover any hosts previously added, and we don't want duplicate scripts.
        MatchPatternSet patternsToRemove;
        for (auto& entry : m_injectedScriptsPerPatternMap)
            patternsToRemove.add(entry.key);

        for (auto& entry : m_injectedStyleSheetsPerPatternMap)
            patternsToRemove.add(entry.key);

        for (auto& pattern : patternsToRemove)
            removeInjectedContent(pattern);
    }

    for (auto& pattern : grantedMatchPatterns)
        addInjectedContent(injectedContents, pattern);
}

static WebCore::UserScriptInjectionTime toImpl(WebExtension::InjectionTime injectionTime)
{
    switch (injectionTime) {
    case WebExtension::InjectionTime::DocumentStart:
        return WebCore::UserScriptInjectionTime::DocumentStart;
    case WebExtension::InjectionTime::DocumentIdle:
        // FIXME: <rdar://problem/57613315> Implement idle injection time. For now, the end injection time is fine.
    case WebExtension::InjectionTime::DocumentEnd:
        return WebCore::UserScriptInjectionTime::DocumentEnd;
    }
}

void WebExtensionContext::addInjectedContent(const InjectedContentVector& injectedContents, WebExtensionMatchPattern& pattern)
{
    if (!isLoaded())
        return;

    auto scriptAddResult = m_injectedScriptsPerPatternMap.ensure(pattern, [&] {
        return UserScriptVector { };
    });

    auto styleSheetAddResult = m_injectedStyleSheetsPerPatternMap.ensure(pattern, [&] {
        return UserStyleSheetVector { };
    });

    auto& originInjectedScripts = scriptAddResult.iterator->value;
    auto& originInjectedStyleSheets = styleSheetAddResult.iterator->value;

    NSMutableSet<NSString *> *baseExcludeMatchPatternsSet = [NSMutableSet set];

    auto& deniedMatchPatterns = deniedPermissionMatchPatterns();
    for (auto& deniedEntry : deniedMatchPatterns) {
        // Granted host patterns always win over revoked host patterns. Skip any revoked "all hosts" patterns.
        // This supports the case where "all hosts" is revoked and a handful of specific hosts are granted.
        if (deniedEntry.key->matchesAllHosts())
            continue;

        // Only revoked patterns that match the granted pattern need to be included. This limits
        // the size of the exclude match patterns list to speed up processing.
        if (!pattern.matchesPattern(deniedEntry.key, { WebExtensionMatchPattern::Options::IgnorePaths, WebExtensionMatchPattern::Options::MatchBidirectionally }))
            continue;

        [baseExcludeMatchPatternsSet addObjectsFromArray:deniedEntry.key->expandedStrings()];
    }

    auto allUserContentControllers = extensionController()->allUserContentControllers();

    for (auto& injectedContentData : injectedContents) {
        NSMutableSet<NSString *> *includeMatchPatternsSet = [NSMutableSet set];

        for (auto& includeMatchPattern : injectedContentData.includeMatchPatterns) {
            // Paths are not matched here since all we need to match at this point is scheme and host.
            // The path matching will happen in WebKit when deciding to inject content into a frame.

            // When the include pattern matches all hosts, we can generate a restricted patten here and skip
            // the more expensive calls to matchesPattern() below since we know they will match.
            if (includeMatchPattern->matchesAllHosts()) {
                auto restrictedPattern = WebExtensionMatchPattern::getOrCreate(includeMatchPattern->scheme(), pattern.host(), includeMatchPattern->path());
                if (!restrictedPattern)
                    continue;

                [includeMatchPatternsSet addObjectsFromArray:restrictedPattern->expandedStrings()];
                continue;
            }

            // When deciding if injected content patterns match, we need to check bidirectionally.
            // This allows an extension that requests *.wikipedia.org, to still inject content when
            // it is granted more specific access to *.en.wikipedia.org.
            if (!includeMatchPattern->matchesPattern(pattern, { WebExtensionMatchPattern::Options::IgnorePaths, WebExtensionMatchPattern::Options::MatchBidirectionally }))
                continue;

            // Pick the most restrictive match pattern by comparing unidirectionally to the granted origin pattern.
            // If the include pattern still matches the granted origin pattern, it is not restrictive enough.
            // In that case we need to use the include pattern scheme and path, but with the granted pattern host.
            RefPtr restrictedPattern = includeMatchPattern.ptr();
            if (includeMatchPattern->matchesPattern(pattern, { WebExtensionMatchPattern::Options::IgnoreSchemes, WebExtensionMatchPattern::Options::IgnorePaths }))
                restrictedPattern = WebExtensionMatchPattern::getOrCreate(includeMatchPattern->scheme(), pattern.host(), includeMatchPattern->path());
            if (!restrictedPattern)
                continue;

            [includeMatchPatternsSet addObjectsFromArray:restrictedPattern->expandedStrings()];
        }

        if (!includeMatchPatternsSet.count)
            continue;

        // FIXME: <rdar://problem/57613243> Support injecting into about:blank, honoring self.contentMatchesAboutBlank. Appending @"about:blank" to the includeMatchPatterns does not work currently.
        NSArray<NSString *> *includeMatchPatterns = includeMatchPatternsSet.allObjects;

        NSMutableSet<NSString *> *excludeMatchPatternsSet = [NSMutableSet setWithArray:injectedContentData.expandedExcludeMatchPatternStrings()];
        [excludeMatchPatternsSet unionSet:baseExcludeMatchPatternsSet];

        NSArray<NSString *> *excludeMatchPatterns = excludeMatchPatternsSet.allObjects;

        auto injectedFrames = injectedContentData.injectsIntoAllFrames ? WebCore::UserContentInjectedFrames::InjectInAllFrames : WebCore::UserContentInjectedFrames::InjectInTopFrameOnly;
        auto injectionTime = toImpl(injectedContentData.injectionTime);
        auto waitForNotification = WebCore::WaitForNotificationBeforeInjecting::No;
        auto& executionWorld = injectedContentData.forMainWorld ? API::ContentWorld::pageContentWorld() : *m_contentScriptWorld;

        for (NSString *scriptPath in injectedContentData.scriptPaths.get()) {
            NSString *scriptString = m_extension->resourceStringForPath(scriptPath, WebExtension::CacheResult::Yes);
            if (!scriptString)
                continue;

            auto userScript = API::UserScript::create(WebCore::UserScript { scriptString, URL { m_baseURL, scriptPath }, makeVector<String>(includeMatchPatterns), makeVector<String>(excludeMatchPatterns), injectionTime, injectedFrames, waitForNotification }, executionWorld);
            originInjectedScripts.append(userScript);

            for (auto& userContentController : allUserContentControllers)
                userContentController.addUserScript(userScript, InjectUserScriptImmediately::Yes);
        }

        for (NSString *styleSheetPath in injectedContentData.styleSheetPaths.get()) {
            NSString *styleSheetString = m_extension->resourceStringForPath(styleSheetPath, WebExtension::CacheResult::Yes);
            if (!styleSheetString)
                continue;

            auto userStyleSheet = API::UserStyleSheet::create(WebCore::UserStyleSheet { styleSheetString, URL { m_baseURL, styleSheetPath }, makeVector<String>(includeMatchPatterns), makeVector<String>(excludeMatchPatterns), injectedFrames, WebCore::UserStyleUserLevel, std::nullopt }, executionWorld);
            originInjectedStyleSheets.append(userStyleSheet);

            for (auto& userContentController : allUserContentControllers)
                userContentController.addUserStyleSheet(userStyleSheet);
        }
    }
}

void WebExtensionContext::addInjectedContent(WebUserContentControllerProxy& userContentController)
{
    for (auto& entry : m_injectedScriptsPerPatternMap) {
        for (auto& userScript : entry.value)
            userContentController.addUserScript(userScript, InjectUserScriptImmediately::Yes);
    }

    for (auto& entry : m_injectedStyleSheetsPerPatternMap) {
        for (auto& userStyleSheet : entry.value)
            userContentController.addUserStyleSheet(userStyleSheet);
    }
}

void WebExtensionContext::removeInjectedContent()
{
    if (!isLoaded())
        return;

    auto allUserContentControllers = extensionController()->allUserContentControllers();
    for (auto& userContentController : allUserContentControllers) {
        for (auto& entry : m_injectedScriptsPerPatternMap) {
            for (auto& userScript : entry.value)
                userContentController.removeUserScript(userScript);
        }

        for (auto& entry : m_injectedStyleSheetsPerPatternMap) {
            for (auto& userStyleSheet : entry.value)
                userContentController.removeUserStyleSheet(userStyleSheet);
        }
    }

    m_injectedScriptsPerPatternMap.clear();
    m_injectedStyleSheetsPerPatternMap.clear();
}

void WebExtensionContext::removeInjectedContent(MatchPatternSet& removedMatchPatterns)
{
    if (!isLoaded())
        return;

    for (auto& removedPattern : removedMatchPatterns)
        removeInjectedContent(removedPattern);

    // If "all hosts" was removed, then we need to add back any individual granted hosts,
    // now that the catch all pattern has been removed.
    if (WebExtensionMatchPattern::patternsMatchAllHosts(removedMatchPatterns))
        addInjectedContent();
}

void WebExtensionContext::removeInjectedContent(WebExtensionMatchPattern& pattern)
{
    if (!isLoaded())
        return;

    auto originInjectedScripts = m_injectedScriptsPerPatternMap.take(pattern);
    auto originInjectedStyleSheets = m_injectedStyleSheetsPerPatternMap.take(pattern);

    if (originInjectedScripts.isEmpty() && originInjectedStyleSheets.isEmpty())
        return;

    auto allUserContentControllers = extensionController()->allUserContentControllers();

    for (auto& userContentController : allUserContentControllers) {
        for (auto& userScript : originInjectedScripts)
            userContentController.removeUserScript(userScript);

        for (auto& userStyleSheet : originInjectedStyleSheets)
            userContentController.removeUserStyleSheet(userStyleSheet);
    }

    auto *tabsToRemove = [NSMutableSet set];
    for (id<_WKWebExtensionTab> tab in m_temporaryTabPermissionMatchPatterns.get().keyEnumerator) {
        if (![tab respondsToSelector:@selector(urlForWebExtensionContext:)])
            continue;

        NSURL *currentURL = [tab urlForWebExtensionContext:wrapper()];
        if (!currentURL)
            continue;

        if (pattern.matchesURL(currentURL))
            [tabsToRemove addObject:tab];
    }

    for (id tab in tabsToRemove)
        [m_temporaryTabPermissionMatchPatterns removeObjectForKey:tab];
}

void WebExtensionContext::removeInjectedContent(WebUserContentControllerProxy& userContentController)
{
    for (auto& entry : m_injectedScriptsPerPatternMap) {
        for (auto& userScript : entry.value)
            userContentController.removeUserScript(userScript);
    }

    for (auto& entry : m_injectedStyleSheetsPerPatternMap) {
        for (auto& userStyleSheet : entry.value)
            userContentController.removeUserStyleSheet(userStyleSheet);
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
