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

#import "CocoaHelpers.h"
#import "InjectUserScriptImmediately.h"
#import "Logging.h"
#import "WKNavigationActionPrivate.h"
#import "WKNavigationDelegatePrivate.h"
#import "WKPreferencesPrivate.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewInternal.h"
#import "WebExtensionAction.h"
#import "WebExtensionTab.h"
#import "WebExtensionURLSchemeHandler.h"
#import "WebExtensionWindow.h"
#import "WebPageProxy.h"
#import "WebUserContentControllerProxy.h"
#import "_WKWebExtensionContextInternal.h"
#import "_WKWebExtensionControllerDelegatePrivate.h"
#import "_WKWebExtensionControllerInternal.h"
#import "_WKWebExtensionLocalization.h"
#import "_WKWebExtensionMatchPatternInternal.h"
#import "_WKWebExtensionPermission.h"
#import "_WKWebExtensionTab.h"
#import "_WKWebExtensionWindow.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/UserScript.h>
#import <pal/spi/cocoa/NSKeyedUnarchiverSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/FileSystem.h>
#import <wtf/URLParser.h>
#import <wtf/cocoa/VectorCocoa.h>

// This number was chosen arbitrarily based on testing with some popular extensions.
static constexpr size_t maximumCachedPermissionResults = 256;

static NSString * const backgroundContentEventListenersKey = @"BackgroundContentEventListeners";
static NSString * const lastSeenBaseURLStateKey = @"LastSeenBaseURL";
static NSString * const lastSeenVersionStateKey = @"LastSeenVersion";

// Update this value when any changes are made to the WebExtensionEventListenerType enum.
static constexpr NSInteger currentBackgroundPageListenerStateVersion = 1;

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
    m_baseURL = URL { makeString("webkit-extension://", uniqueIdentifier(), '/') };
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
    case WebExtensionContext::Error::BaseURLAlreadyInUse:
        return _WKWebExtensionContextErrorBaseURLAlreadyInUse;
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

    case Error::BaseURLAlreadyInUse:
        localizedDescription = WEB_UI_STRING("Another extension context is loaded with the same base URL.", "WKWebExtensionContextErrorBaseURLAlreadyInUse description");
        break;
    }

    if (customLocalizedDescription.length)
        localizedDescription = customLocalizedDescription;

    NSDictionary *userInfo = @{ NSLocalizedDescriptionKey: localizedDescription };
    if (underlyingError)
        userInfo = @{ NSLocalizedDescriptionKey: localizedDescription, NSUnderlyingErrorKey: underlyingError };

    return [[NSError alloc] initWithDomain:_WKWebExtensionContextErrorDomain code:errorCode userInfo:userInfo];
}

bool WebExtensionContext::load(WebExtensionController& controller, String storageDirectory, NSError **outError)
{
    if (outError)
        *outError = nil;

    if (isLoaded()) {
        RELEASE_LOG_ERROR(Extensions, "Extension context already loaded");
        if (outError)
            *outError = createError(Error::AlreadyLoaded);
        return false;
    }

    m_storageDirectory = storageDirectory;
    m_extensionController = controller;
    m_contentScriptWorld = API::ContentWorld::sharedWorldWithName(makeString("WebExtension-", m_uniqueIdentifier));

    readStateFromStorage();
    writeStateToStorage();

    populateWindowsAndTabs();

    // FIXME: <https://webkit.org/b/248430> Move local storage (if base URL changed).

    // FIXME: <https://webkit.org/b/248889> Check to see if extension is being loaded as part of startup.

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
        RELEASE_LOG_ERROR(Extensions, "Extension context not loaded");
        if (outError)
            *outError = createError(Error::NotLoaded);
        return false;
    }

    writeStateToStorage();

    unloadBackgroundWebView();
    removeInjectedContent();

    m_storageDirectory = nullString();
    m_extensionController = nil;
    m_contentScriptWorld = nullptr;

    return true;
}

String WebExtensionContext::stateFilePath() const
{
    if (!storageIsPersistent())
        return nullString();
    return FileSystem::pathByAppendingComponent(m_storageDirectory, "State.plist"_s);
}

NSDictionary *WebExtensionContext::currentState() const
{
    [m_state setObject:(NSString *)m_baseURL.string() forKey:lastSeenBaseURLStateKey];
    [m_state setObject:m_extension->version() ?: @"" forKey:lastSeenVersionStateKey];

    return [m_state.get() copy];
}

NSDictionary *WebExtensionContext::readStateFromStorage()
{
    if (!storageIsPersistent()) {
        m_state = [NSMutableDictionary dictionary];
        return @{ };
    }

    NSFileCoordinator *fileCoordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];

    __block NSMutableDictionary *savedState;

    NSError *coordinatorError;
    [fileCoordinator coordinateReadingItemAtURL:[NSURL fileURLWithPath:stateFilePath()] options:NSFileCoordinatorReadingWithoutChanges error:&coordinatorError byAccessor:^(NSURL *fileURL) {
        savedState = [NSMutableDictionary dictionaryWithContentsOfURL:fileURL] ?: [NSMutableDictionary dictionary];
    }];

    if (coordinatorError)
        RELEASE_LOG_ERROR(Extensions, "Failed to coordinate reading extension state: %{private}@", coordinatorError);

    m_state = savedState;

    return [savedState copy];
}

void WebExtensionContext::writeStateToStorage() const
{
    if (!storageIsPersistent())
        return;

    NSFileCoordinator *fileCoordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];

    NSError *coordinatorError;
    [fileCoordinator coordinateWritingItemAtURL:[NSURL fileURLWithPath:stateFilePath()] options:NSFileCoordinatorWritingForReplacing error:&coordinatorError byAccessor:^(NSURL *fileURL) {
        NSError *error;
        if (![currentState() writeToURL:fileURL error:&error])
            RELEASE_LOG_ERROR(Extensions, "Unable to save extension state: %{private}@", error);
    }];

    if (coordinatorError)
        RELEASE_LOG_ERROR(Extensions, "Failed to coordinate writing extension state: %{private}@", coordinatorError);
}

void WebExtensionContext::setBaseURL(URL&& url)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    if (!url.isValid())
        return;

    m_baseURL = URL { makeString(url.protocol(), "://", url.host(), '/') };
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

    m_customUniqueIdentifier = !uniqueIdentifier.isEmpty();

    if (uniqueIdentifier.isEmpty())
        uniqueIdentifier = WTF::UUID::createVersion4().toString();

    m_uniqueIdentifier = uniqueIdentifier;
}

void WebExtensionContext::setInspectable(bool inspectable)
{
    m_inspectable = inspectable;

    m_backgroundWebView.get().inspectable = inspectable;

    for (auto entry : m_actionMap) {
        if (auto *webView = entry.value->popupWebView(WebExtensionAction::LoadOnFirstAccess::No))
            webView.inspectable = inspectable;
    }

    if (m_defaultAction) {
        if (auto *webView = m_defaultAction->popupWebView(WebExtensionAction::LoadOnFirstAccess::No))
            webView.inspectable = inspectable;
    }
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
    return removeExpired(m_grantedPermissions, m_nextGrantedPermissionsExpirationDate, _WKWebExtensionContextGrantedPermissionsWereRemovedNotification);
}

void WebExtensionContext::setGrantedPermissions(PermissionsMap&& grantedPermissions)
{
    PermissionsSet removedPermissions;
    for (auto& entry : m_grantedPermissions)
        removedPermissions.add(entry.key);

    m_nextGrantedPermissionsExpirationDate = WallTime::nan();
    m_grantedPermissions = removeExpired(grantedPermissions, m_nextGrantedPermissionsExpirationDate);

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
    return removeExpired(m_deniedPermissions, m_nextDeniedPermissionsExpirationDate, _WKWebExtensionContextDeniedPermissionsWereRemovedNotification);
}

void WebExtensionContext::setDeniedPermissions(PermissionsMap&& deniedPermissions)
{
    PermissionsSet removedPermissions;
    for (auto& entry : m_deniedPermissions)
        removedPermissions.add(entry.key);

    m_nextDeniedPermissionsExpirationDate = WallTime::nan();
    m_deniedPermissions = removeExpired(deniedPermissions, m_nextDeniedPermissionsExpirationDate);

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
    return removeExpired(m_grantedPermissionMatchPatterns, m_nextGrantedPermissionMatchPatternsExpirationDate, _WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification);
}

void WebExtensionContext::setGrantedPermissionMatchPatterns(PermissionMatchPatternsMap&& grantedPermissionMatchPatterns)
{
    MatchPatternSet removedMatchPatterns;
    for (auto& entry : m_grantedPermissionMatchPatterns)
        removedMatchPatterns.add(entry.key);

    m_nextGrantedPermissionMatchPatternsExpirationDate = WallTime::nan();
    m_grantedPermissionMatchPatterns = removeExpired(grantedPermissionMatchPatterns, m_nextGrantedPermissionsExpirationDate);

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
    return removeExpired(m_deniedPermissionMatchPatterns, m_nextDeniedPermissionMatchPatternsExpirationDate, _WKWebExtensionContextDeniedPermissionMatchPatternsWereRemovedNotification);
}

void WebExtensionContext::setDeniedPermissionMatchPatterns(PermissionMatchPatternsMap&& deniedPermissionMatchPatterns)
{
    MatchPatternSet removedMatchPatterns;
    for (auto& entry : m_deniedPermissionMatchPatterns)
        removedMatchPatterns.add(entry.key);

    m_nextDeniedPermissionMatchPatternsExpirationDate = WallTime::nan();
    m_deniedPermissionMatchPatterns = removeExpired(deniedPermissionMatchPatterns, m_nextDeniedPermissionMatchPatternsExpirationDate);

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

    if ([notificationName isEqualToString:_WKWebExtensionContextPermissionsWereGrantedNotification])
        firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnAdded, permissions, { });
    else if ([notificationName isEqualToString:_WKWebExtensionContextGrantedPermissionsWereRemovedNotification])
        firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnRemoved, permissions, { });

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, notificationName = retainPtr(notificationName), permissions]() {
        [NSNotificationCenter.defaultCenter postNotificationName:notificationName.get() object:wrapper() userInfo:@{ _WKWebExtensionContextNotificationUserInfoKeyPermissions: toAPI(permissions) }];
    }).get());
}

void WebExtensionContext::postAsyncNotification(NSNotificationName notificationName, MatchPatternSet& matchPatterns)
{
    if (matchPatterns.isEmpty())
        return;

    if ([notificationName isEqualToString:_WKWebExtensionContextPermissionsWereGrantedNotification])
        firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnAdded, { }, matchPatterns);
    else if ([notificationName isEqualToString:_WKWebExtensionContextGrantedPermissionsWereRemovedNotification])
        firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnRemoved, { }, matchPatterns);

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, notificationName = retainPtr(notificationName), matchPatterns]() {
        [NSNotificationCenter.defaultCenter postNotificationName:notificationName.get() object:wrapper() userInfo:@{ _WKWebExtensionContextNotificationUserInfoKeyMatchPatterns: toAPI(matchPatterns) }];
    }).get());
}

void WebExtensionContext::grantPermissions(PermissionsSet&& permissions, WallTime expirationDate)
{
    if (permissions.isEmpty())
        return;

    if (m_nextGrantedPermissionsExpirationDate > expirationDate)
        m_nextGrantedPermissionsExpirationDate = expirationDate;

    for (auto& permission : permissions)
        m_grantedPermissions.add(permission, expirationDate);

    removeDeniedPermissions(permissions);

    postAsyncNotification(_WKWebExtensionContextPermissionsWereGrantedNotification, permissions);
}

void WebExtensionContext::denyPermissions(PermissionsSet&& permissions, WallTime expirationDate)
{
    if (permissions.isEmpty())
        return;

    if (m_nextDeniedPermissionsExpirationDate > expirationDate)
        m_nextDeniedPermissionsExpirationDate = expirationDate;

    for (auto& permission : permissions)
        m_deniedPermissions.add(permission, expirationDate);

    removeGrantedPermissions(permissions);

    postAsyncNotification(_WKWebExtensionContextPermissionsWereDeniedNotification, permissions);
}

void WebExtensionContext::grantPermissionMatchPatterns(MatchPatternSet&& permissionMatchPatterns, WallTime expirationDate)
{
    if (permissionMatchPatterns.isEmpty())
        return;

    if (m_nextGrantedPermissionMatchPatternsExpirationDate > expirationDate)
        m_nextGrantedPermissionMatchPatternsExpirationDate = expirationDate;

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

    if (m_nextDeniedPermissionMatchPatternsExpirationDate > expirationDate)
        m_nextDeniedPermissionMatchPatternsExpirationDate = expirationDate;

    for (auto& pattern : permissionMatchPatterns)
        m_deniedPermissionMatchPatterns.add(pattern, expirationDate);

    removeGrantedPermissionMatchPatterns(permissionMatchPatterns, EqualityOnly::Yes);
    clearCachedPermissionStates();

    updateInjectedContent();

    postAsyncNotification(_WKWebExtensionContextPermissionMatchPatternsWereDeniedNotification, permissionMatchPatterns);
}

void WebExtensionContext::removeGrantedPermissions(PermissionsSet& permissionsToRemove)
{
    removePermissions(m_grantedPermissions, permissionsToRemove, m_nextGrantedPermissionsExpirationDate, _WKWebExtensionContextGrantedPermissionsWereRemovedNotification);
}

void WebExtensionContext::removeGrantedPermissionMatchPatterns(MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly)
{
    removePermissionMatchPatterns(m_grantedPermissionMatchPatterns, matchPatternsToRemove, equalityOnly, m_nextGrantedPermissionMatchPatternsExpirationDate, _WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification);

    removeInjectedContent(matchPatternsToRemove);
}

void WebExtensionContext::removeDeniedPermissions(PermissionsSet& permissionsToRemove)
{
    removePermissions(m_deniedPermissions, permissionsToRemove, m_nextDeniedPermissionsExpirationDate, _WKWebExtensionContextDeniedPermissionsWereRemovedNotification);
}

void WebExtensionContext::removeDeniedPermissionMatchPatterns(MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly)
{
    removePermissionMatchPatterns(m_deniedPermissionMatchPatterns, matchPatternsToRemove, equalityOnly, m_nextDeniedPermissionMatchPatternsExpirationDate, _WKWebExtensionContextDeniedPermissionsWereRemovedNotification);

    updateInjectedContent();
}

void WebExtensionContext::removePermissions(PermissionsMap& permissionMap, PermissionsSet& permissionsToRemove, WallTime& nextExpirationDate, NSNotificationName notificationName)
{
    if (permissionsToRemove.isEmpty())
        return;

    nextExpirationDate = WallTime::infinity();

    PermissionsSet removedPermissions;
    permissionMap.removeIf([&](auto& entry) {
        if (permissionsToRemove.contains(entry.key)) {
            removedPermissions.add(entry.key);
            return true;
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedPermissions.isEmpty() || !notificationName)
        return;

    postAsyncNotification(notificationName, removedPermissions);
}

void WebExtensionContext::removePermissionMatchPatterns(PermissionMatchPatternsMap& matchPatternMap, MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly, WallTime& nextExpirationDate, NSNotificationName notificationName)
{
    if (matchPatternsToRemove.isEmpty())
        return;

    nextExpirationDate = WallTime::infinity();

    MatchPatternSet removedMatchPatterns;
    matchPatternMap.removeIf([&](auto& entry) {
        if (matchPatternsToRemove.contains(entry.key)) {
            removedMatchPatterns.add(entry.key);
            return true;
        }

        if (equalityOnly == EqualityOnly::Yes) {
            if (entry.value < nextExpirationDate)
                nextExpirationDate = entry.value;

            return false;
        }

        for (auto& patternToRemove : matchPatternsToRemove) {
            Ref pattern = entry.key;
            if (patternToRemove->matchesPattern(pattern, WebExtensionMatchPattern::Options::IgnorePaths)) {
                removedMatchPatterns.add(pattern);
                return true;
            }
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedMatchPatterns.isEmpty() || !notificationName)
        return;

    clearCachedPermissionStates();

    postAsyncNotification(notificationName, removedMatchPatterns);
}

WebExtensionContext::PermissionsMap& WebExtensionContext::removeExpired(PermissionsMap& permissionMap, WallTime& nextExpirationDate, NSNotificationName notificationName)
{
    WallTime currentTime = WallTime::now();

    // If the next expiration date hasn't passed yet, there is nothing to remove.
    if (nextExpirationDate != WallTime::nan() && nextExpirationDate > currentTime)
        return permissionMap;

    nextExpirationDate = WallTime::infinity();

    PermissionsSet removedPermissions;
    permissionMap.removeIf([&](auto& entry) {
        if (entry.value <= currentTime) {
            removedPermissions.add(entry.key);
            return true;
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedPermissions.isEmpty() || !notificationName)
        return permissionMap;

    postAsyncNotification(notificationName, removedPermissions);

    return permissionMap;
}

WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::removeExpired(PermissionMatchPatternsMap& matchPatternMap, WallTime& nextExpirationDate, NSNotificationName notificationName)
{
    WallTime currentTime = WallTime::now();

    // If the next expiration date hasn't passed yet, there is nothing to remove.
    if (nextExpirationDate != WallTime::nan() && nextExpirationDate > currentTime)
        return matchPatternMap;

    nextExpirationDate = WallTime::infinity();

    MatchPatternSet removedMatchPatterns;
    matchPatternMap.removeIf([&](auto& entry) {
        if (entry.value <= currentTime) {
            removedMatchPatterns.add(entry.key);
            return true;
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedMatchPatterns.isEmpty() || !notificationName)
        return matchPatternMap;

    clearCachedPermissionStates();

    postAsyncNotification(notificationName, removedMatchPatterns);

    return matchPatternMap;
}

bool WebExtensionContext::hasPermission(const String& permission, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
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

bool WebExtensionContext::hasPermission(const URL& url, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
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

bool WebExtensionContext::hasPermissions(PermissionsSet permissions, MatchPatternSet matchPatterns)
{
    for (auto& permission : permissions) {
        if (!m_grantedPermissions.contains(permission))
            return false;
    }

    for (auto& pattern : matchPatterns) {
        bool matchFound = false;
        for (auto& grantedPattern : currentPermissionMatchPatterns()) {
            if (grantedPattern->matchesPattern(pattern, { WebExtensionMatchPattern::Options::IgnorePaths })) {
                matchFound = true;
                break;
            }
        }

        if (!matchFound)
            return false;
    }

    return true;
}

WebExtensionContext::PermissionState WebExtensionContext::permissionState(const String& permission, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!permission.isEmpty());

    if (tab && hasActiveUserGesture(*tab)) {
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

WebExtensionContext::PermissionState WebExtensionContext::permissionState(const URL& coreURL, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    if (coreURL.isEmpty())
        return PermissionState::Unknown;

    if (isURLForThisExtension(coreURL))
        return PermissionState::GrantedImplicitly;

    NSURL *url = coreURL;
    ASSERT(url);

    if (!WebExtensionMatchPattern::validSchemes().contains(url.scheme))
        return PermissionState::Unknown;

    if (tab && [[m_temporaryTabPermissionMatchPatterns objectForKey:tab->delegate()] matchesURL:url])
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

WebExtensionContext::PermissionState WebExtensionContext::permissionState(WebExtensionMatchPattern& pattern, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    if (!pattern.isValid())
        return PermissionState::Unknown;

    if (pattern.matchesURL(baseURL()))
        return PermissionState::GrantedImplicitly;

    if (!pattern.matchesAllURLs() && !WebExtensionMatchPattern::validSchemes().contains(pattern.scheme()))
        return PermissionState::Unknown;

    if (tab && [[m_temporaryTabPermissionMatchPatterns objectForKey:tab->delegate()] matchesPattern:pattern.wrapper()])
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

Ref<WebExtensionWindow> WebExtensionContext::getOrCreateWindow(_WKWebExtensionWindow *delegate)
{
    ASSERT(delegate);

    for (auto& window : m_windowMap.values()) {
        if (window->delegate() == delegate)
            return window;
    }

    auto window = adoptRef(new WebExtensionWindow(*this, delegate));
    m_windowMap.set(window->identifier(), *window);

    RELEASE_LOG_DEBUG(Extensions, "Window %{public}llu was created", window->identifier().toUInt64());

    return window.releaseNonNull();
}

RefPtr<WebExtensionWindow> WebExtensionContext::getWindow(WebExtensionWindowIdentifier identifier, std::optional<WebPageProxyIdentifier> webPageProxyIdentifier)
{
    if (!isValid(identifier))
        return nullptr;

    RefPtr<WebExtensionWindow> result;

    if (isCurrent(identifier)) {
        if (webPageProxyIdentifier) {
            if (auto tab = getTab(webPageProxyIdentifier.value()))
                result = tab->window();
        }

        if (!result)
            result = frontmostWindow();
    } else
        result = m_windowMap.get(identifier);

    if (!result) {
        if (isCurrent(identifier)) {
            if (webPageProxyIdentifier)
                RELEASE_LOG_ERROR(Extensions, "Current window for page %{public}llu was not found", webPageProxyIdentifier.value().toUInt64());
            else
                RELEASE_LOG_ERROR(Extensions, "Current window not found (no frontmost window)");
        } else
            RELEASE_LOG_ERROR(Extensions, "Window %{public}llu was not found", identifier.toUInt64());

        return nullptr;
    }

    if (!result->isValid()) {
        RELEASE_LOG_ERROR(Extensions, "Window %{public}llu has nil delegate; reference not removed via didCloseWindow: before release", result->identifier().toUInt64());
        m_windowMap.remove(result->identifier());
        return nullptr;
    }

    return result;
}

Ref<WebExtensionTab> WebExtensionContext::getOrCreateTab(_WKWebExtensionTab *delegate)
{
    ASSERT(delegate);

    for (auto& tab : m_tabMap.values()) {
        if (tab->delegate() == delegate)
            return tab;
    }

    auto tab = adoptRef(new WebExtensionTab(*this, delegate));
    m_tabMap.set(tab->identifier(), *tab);

    RELEASE_LOG_DEBUG(Extensions, "Tab %{public}llu was created", tab->identifier().toUInt64());

    return tab.releaseNonNull();
}

RefPtr<WebExtensionTab> WebExtensionContext::getTab(WebExtensionTabIdentifier identifier)
{
    if (!isValid(identifier))
        return nullptr;

    auto* tab = m_tabMap.get(identifier);
    if (!tab) {
        RELEASE_LOG_ERROR(Extensions, "Tab %{public}llu was not found", identifier.toUInt64());
        return nullptr;
    }

    if (!tab->isValid()) {
        RELEASE_LOG_ERROR(Extensions, "Tab %{public}llu has nil delegate; reference not removed via didCloseTab: before release", identifier.toUInt64());
        m_tabMap.remove(identifier);
        return nullptr;
    }

    return tab;
}

RefPtr<WebExtensionTab> WebExtensionContext::getTab(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> identifier)
{
    if (identifier)
        return getTab(identifier.value());

    RefPtr<WebExtensionTab> result;

    for (auto& tab : openTabs()) {
        for (WKWebView *webView in tab->webViews()) {
            if (webView._page->identifier() == webPageProxyIdentifier) {
                result = tab.ptr();
                break;
            }
        }

        if (result)
            break;
    }

    // FIXME: <https://webkit.org/b/260154> Use the page identifier to get the current tab for popup pages.

    if (!result) {
        RELEASE_LOG_ERROR(Extensions, "Tab for page %{public}llu was not found", webPageProxyIdentifier.toUInt64());
        return nullptr;
    }

    if (!result->isValid()) {
        RELEASE_LOG_ERROR(Extensions, "Tab %{public}llu has nil delegate; reference not removed via didCloseTab: before release", result->identifier().toUInt64());
        m_tabMap.remove(result->identifier());
        return nullptr;
    }

    return result;
}

void WebExtensionContext::populateWindowsAndTabs()
{
    ASSERT(isLoaded());

    auto delegate = m_extensionController->delegate();

    if ([delegate respondsToSelector:@selector(webExtensionController:openWindowsForExtensionContext:)]) {
        auto *openWindows = [delegate webExtensionController:extensionController()->wrapper() openWindowsForExtensionContext:wrapper()];
        THROW_UNLESS([openWindows isKindOfClass:NSArray.class], @"Object returned by webExtensionController:openWindowsForExtensionContext: is not an array");

        for (id<_WKWebExtensionWindow> windowDelegate in openWindows) {
            THROW_UNLESS([windowDelegate conformsToProtocol:@protocol(_WKWebExtensionWindow)], @"Object in array returned by webExtensionController:openWindowsForExtensionContext: does not conform to the _WKWebExtensionWindow protocol");

            auto window = getOrCreateWindow(windowDelegate);
            m_openWindowIdentifiers.append(window->identifier());

            // Request the tabs here so they will be registered and populate openTabs().
            window->tabs();
        }
    }

    if ([delegate respondsToSelector:@selector(webExtensionController:focusedWindowForExtensionContext:)]) {
        id<_WKWebExtensionWindow> focusedWindow = [delegate webExtensionController:extensionController()->wrapper() focusedWindowForExtensionContext:wrapper()];
        THROW_UNLESS(!focusedWindow || [focusedWindow conformsToProtocol:@protocol(_WKWebExtensionWindow)], @"Object returned by webExtensionController:focusedWindowForExtensionContext: does not conform to the _WKWebExtensionWindow protocol");

        m_focusedWindowIdentifier = focusedWindow ? std::optional(getOrCreateWindow(focusedWindow)->identifier()) : std::nullopt;
    } else
        m_focusedWindowIdentifier = !m_openWindowIdentifiers.isEmpty() ? std::optional(m_openWindowIdentifiers.first()) : std::nullopt;
}

WebExtensionContext::WindowVector WebExtensionContext::openWindows() const
{
    WindowVector result;
    result.reserveInitialCapacity(m_openWindowIdentifiers.size());

    for (auto& identifier : m_openWindowIdentifiers) {
        if (auto window = m_windowMap.get(identifier))
            result.uncheckedAppend(*window);
    }

    return result;
}

RefPtr<WebExtensionWindow> WebExtensionContext::focusedWindow()
{
    if (m_focusedWindowIdentifier)
        return getWindow(m_focusedWindowIdentifier.value());
    return nullptr;
}

RefPtr<WebExtensionWindow> WebExtensionContext::frontmostWindow()
{
    if (!m_openWindowIdentifiers.isEmpty())
        return getWindow(m_openWindowIdentifiers.first());
    return nullptr;
}

void WebExtensionContext::didOpenWindow(const WebExtensionWindow& window)
{
    ASSERT(window.extensionContext() == this);
    ASSERT(m_windowMap.contains(window.identifier()));
    ASSERT(m_windowMap.get(window.identifier()) == &window);

    RELEASE_LOG_DEBUG(Extensions, "Opened window %{public}llu", window.identifier().toUInt64());

    m_focusedWindowIdentifier = window.identifier();

    m_openWindowIdentifiers.removeAll(window.identifier());
    m_openWindowIdentifiers.insert(0, window.identifier());

    // Request the tabs here so they will be registered and populate openTabs().
    window.tabs();

    if (!isLoaded())
        return;

    fireWindowsEventIfNeeded(WebExtensionEventListenerType::WindowsOnCreated, window.parameters());
}

void WebExtensionContext::didCloseWindow(const WebExtensionWindow& window)
{
    ASSERT(window.extensionContext() == this);
    ASSERT(m_windowMap.contains(window.identifier()));
    ASSERT(m_windowMap.get(window.identifier()) == &window);

    RELEASE_LOG_DEBUG(Extensions, "Closed window %{public}llu", window.identifier().toUInt64());

    Ref protectedWindow { window };

    if (m_focusedWindowIdentifier == window.identifier())
        m_focusedWindowIdentifier = std::nullopt;

    m_openWindowIdentifiers.removeAll(window.identifier());
    m_windowMap.remove(window.identifier());

    for (auto& tab : window.tabs())
        didCloseTab(tab, WindowIsClosing::Yes);

    if (!isLoaded())
        return;

    fireWindowsEventIfNeeded(WebExtensionEventListenerType::WindowsOnRemoved, window.minimalParameters());
}

void WebExtensionContext::didFocusWindow(WebExtensionWindow* window)
{
    ASSERT(!window || window && window->extensionContext() == this);

    if (window)
        RELEASE_LOG_DEBUG(Extensions, "Focused window %{public}llu", window->identifier().toUInt64());
    else
        RELEASE_LOG_DEBUG(Extensions, "No window focused");

    m_focusedWindowIdentifier = window ? std::optional(window->identifier()) : std::nullopt;

    if (window) {
        ASSERT(m_openWindowIdentifiers.contains(window->identifier()));
        m_openWindowIdentifiers.removeAll(window->identifier());
        m_openWindowIdentifiers.insert(0, window->identifier());
    }

    if (!isLoaded())
        return;

    fireWindowsEventIfNeeded(WebExtensionEventListenerType::WindowsOnFocusChanged, window ? std::optional(window->minimalParameters()) : std::nullopt);
}

void WebExtensionContext::didOpenTab(const WebExtensionTab& tab)
{
    ASSERT(tab.extensionContext() == this);
    ASSERT(m_tabMap.contains(tab.identifier()));
    ASSERT(m_tabMap.get(tab.identifier()) == &tab);

    RELEASE_LOG_DEBUG(Extensions, "Opened tab %{public}llu", tab.identifier().toUInt64());

    if (!isLoaded())
        return;

    fireTabsCreatedEventIfNeeded(tab.parameters());
}

void WebExtensionContext::didCloseTab(const WebExtensionTab& tab, WindowIsClosing windowIsClosing)
{
    ASSERT(tab.extensionContext() == this);
    ASSERT(m_tabMap.contains(tab.identifier()));
    ASSERT(m_tabMap.get(tab.identifier()) == &tab);

    RELEASE_LOG_DEBUG(Extensions, "Closed tab %{public}llu %{public}s", tab.identifier().toUInt64(), windowIsClosing == WindowIsClosing::Yes ? "(window closing)" : "");

    Ref protectedTab { tab };

    m_tabMap.remove(tab.identifier());

    if (!isLoaded())
        return;

    auto window = tab.window(WebExtensionTab::SkipContainsCheck::Yes);
    auto windowIdentifier = window ? window->identifier() : WebExtensionWindowConstants::NoneIdentifier;

    fireTabsRemovedEventIfNeeded(tab.identifier(), windowIdentifier, windowIsClosing);
}

void WebExtensionContext::didActivateTab(const WebExtensionTab& tab, const WebExtensionTab* previousTab)
{
    ASSERT(tab.extensionContext() == this);
    ASSERT(!previousTab || (previousTab && previousTab->extensionContext() == this));
    ASSERT(m_tabMap.contains(tab.identifier()));
    ASSERT(m_tabMap.get(tab.identifier()) == &tab);

    didSelectOrDeselectTabs({ const_cast<WebExtensionTab&>(tab) });

    RELEASE_LOG_DEBUG(Extensions, "Activated tab %{public}llu", tab.identifier().toUInt64());

    if (!isLoaded())
        return;

    auto window = tab.window();
    auto windowIdentifier = window ? window->identifier() : WebExtensionWindowConstants::NoneIdentifier;
    auto previousTabIdentifier = previousTab ? previousTab->identifier() : WebExtensionTabConstants::NoneIdentifier;

    fireTabsActivatedEventIfNeeded(previousTabIdentifier, tab.identifier(), windowIdentifier);
}

void WebExtensionContext::didSelectOrDeselectTabs(const TabSet& tabs)
{
    HashMap<WebExtensionWindowIdentifier, Vector<WebExtensionTabIdentifier>> windowToTabs;

    for (auto& tab : tabs) {
        ASSERT(tab->extensionContext() == this);

        auto window = tab->window();
        if (!window)
            continue;

        windowToTabs.ensure(window->identifier(), [&] {
            RELEASE_LOG_DEBUG(Extensions, "Selected tabs changed for window %{public}llu", window->identifier().toUInt64());

            Vector<WebExtensionTabIdentifier> result;

            for (auto& tab : window->tabs()) {
                if (!tab->isSelected())
                    continue;

                RELEASE_LOG_DEBUG(Extensions, "Selected tab %{public}llu", tab->identifier().toUInt64());

                result.append(tab->identifier());
            }

            return result;
        });
    }

    if (!isLoaded())
        return;

    for (auto& entry : windowToTabs)
        fireTabsHighlightedEventIfNeeded(entry.value, entry.key);
}

void WebExtensionContext::didMoveTab(const WebExtensionTab& tab, size_t oldIndex, const WebExtensionWindow* oldWindow)
{
    ASSERT(tab.extensionContext() == this);
    ASSERT(!oldWindow || (oldWindow && oldWindow->extensionContext() == this));

    if (oldWindow)
        RELEASE_LOG_DEBUG(Extensions, "Moved tab %{public}llu to index %{public}zu from window %{public}llu", tab.identifier().toUInt64(), oldIndex, oldWindow->identifier().toUInt64());
    else
        RELEASE_LOG_DEBUG(Extensions, "Moved tab %{public}llu to index %{public}zu (in same window)", tab.identifier().toUInt64(), oldIndex);

    if (!isLoaded())
        return;

    auto window = tab.window();
    if (oldWindow && window && oldWindow != window) {
        fireTabsDetachedEventIfNeeded(tab.identifier(), oldWindow->identifier(), oldIndex);
        fireTabsAttachedEventIfNeeded(tab.identifier(), window->identifier(), tab.index());
    } else {
        auto windowIdentifier = window ? window->identifier() : WebExtensionWindowConstants::NoneIdentifier;
        fireTabsMovedEventIfNeeded(tab.identifier(), windowIdentifier, oldIndex, tab.index());
    }
}

void WebExtensionContext::didReplaceTab(const WebExtensionTab& oldTab, const WebExtensionTab& newTab)
{
    ASSERT(oldTab.extensionContext() == this);
    ASSERT(newTab.extensionContext() == this);

    RELEASE_LOG_DEBUG(Extensions, "Replaced tab %{public}llu with tab %{public}llu", oldTab.identifier().toUInt64(), newTab.identifier().toUInt64());

    if (!isLoaded())
        return;

    fireTabsReplacedEventIfNeeded(oldTab.identifier(), newTab.identifier());
}

void WebExtensionContext::didChangeTabProperties(const WebExtensionTab& tab, OptionSet<WebExtensionTab::ChangedProperties> properties)
{
    ASSERT(tab.extensionContext() == this);

    RELEASE_LOG_DEBUG(Extensions, "Changed tab properties (%{public}X) for tab %{public}llu", properties.toRaw(), tab.identifier().toUInt64());

    if (!isLoaded())
        return;

    fireTabsUpdatedEventIfNeeded(tab.parameters(), tab.changedParameters(properties));
}

WebExtensionAction& WebExtensionContext::defaultAction()
{
    if (!m_defaultAction)
        m_defaultAction = WebExtensionAction::create(*this);
    return *m_defaultAction;
}

Ref<WebExtensionAction> WebExtensionContext::getOrCreateAction(WebExtensionTab* tab)
{
    if (!tab)
        return defaultAction();

    return m_actionMap.ensure(*tab, [&] {
        return WebExtensionAction::create(*this, *tab);
    }).iterator->value;
}

void WebExtensionContext::performAction(WebExtensionTab* tab)
{
    if (tab)
        userGesturePerformed(*tab);

    auto action = getOrCreateAction(tab);
    if (action->hasPopup()) {
        action->presentPopupWhenReady();
        return;
    }

    // FIXME: <https://webkit.org/b/260154> Implement. Dispatch action event in main world pages.
}

void WebExtensionContext::userGesturePerformed(WebExtensionTab& tab)
{
    // Nothing else to do if the extension does not have the activeTab permissions.
    if (!hasPermission(_WKWebExtensionPermissionActiveTab))
        return;

    NSURL *currentURL = tab.url();
    if (!currentURL)
        return;

    _WKWebExtensionMatchPattern *pattern = [m_temporaryTabPermissionMatchPatterns objectForKey:tab.delegate()];

    // Nothing to do if the tab already has a pattern matching the current URL.
    if (pattern && [pattern matchesURL:currentURL])
        return;

    // A pattern should not exist, since it should be cleared in clearUserGesture
    // on any navigation between different hosts.
    ASSERT(!pattern);

    if (!m_temporaryTabPermissionMatchPatterns)
        m_temporaryTabPermissionMatchPatterns = [NSMapTable weakToStrongObjectsMapTable];

    // Grant the tab a temporary permission to access to a pattern matching the current URL's scheme and host for all paths.
    pattern = [_WKWebExtensionMatchPattern matchPatternWithScheme:currentURL.scheme host:currentURL.host path:@"/*"];
    [m_temporaryTabPermissionMatchPatterns setObject:pattern forKey:tab.delegate()];
}

bool WebExtensionContext::hasActiveUserGesture(WebExtensionTab& tab) const
{
    if (!m_temporaryTabPermissionMatchPatterns)
        return false;

    NSURL *currentURL = tab.url();
    return [[m_temporaryTabPermissionMatchPatterns objectForKey:tab.delegate()] matchesURL:currentURL];
}

void WebExtensionContext::clearUserGesture(WebExtensionTab& tab)
{
    if (!m_temporaryTabPermissionMatchPatterns)
        return;

    [m_temporaryTabPermissionMatchPatterns removeObjectForKey:tab.delegate()];
}

void WebExtensionContext::setTestingMode(bool testingMode)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    m_testingMode = testingMode;
}

WKWebView *WebExtensionContext::relatedWebView()
{
    if (m_backgroundWebView)
        return m_backgroundWebView.get();

    for (auto entry : m_actionMap) {
        if (auto *webView = entry.value->popupWebView(WebExtensionAction::LoadOnFirstAccess::No))
            return webView;
    }

    if (m_defaultAction) {
        if (auto *webView = m_defaultAction->popupWebView(WebExtensionAction::LoadOnFirstAccess::No))
            return webView;
    }

    return nil;
}

WKWebViewConfiguration *WebExtensionContext::webViewConfiguration()
{
    ASSERT(isLoaded());

    WKWebViewConfiguration *configuration = [extensionController()->configuration().webViewConfiguration() copy];

    // When using manifest v2 the web views need to use the same process.
    if (extension().supportsManifestVersion(2))
        configuration._relatedWebView = relatedWebView();

    // Use the weak property to avoid a reference cycle while an extension web view is being held by the context.
    configuration._weakWebExtensionController = extensionController()->wrapper();
    configuration._webExtensionController = nil;

    configuration._processDisplayName = extension().webProcessDisplayName();

    auto *preferences = configuration.preferences;
#if PLATFORM(MAC)
    preferences._domTimersThrottlingEnabled = NO;
#endif
    preferences._pageVisibilityBasedProcessSuppressionEnabled = NO;

    // FIXME: Configure other extension web view configuration properties.

    return configuration;
}

URL WebExtensionContext::backgroundContentURL()
{
    if (!extension().hasBackgroundContent())
        return { };
    return { m_baseURL, extension().backgroundContentPath() };
}

void WebExtensionContext::loadBackgroundWebViewDuringLoad()
{
    ASSERT(isLoaded());

    if (!extension().hasBackgroundContent())
        return;

    if (!extension().backgroundContentIsPersistent()) {
        uint64_t backgroundPageListenersVersionNumber = loadBackgroundPageListenersVersionNumberFromStorage();
        bool savedVersionNumberDoesNotMatchCurrentVersionNumber = backgroundPageListenersVersionNumber != currentBackgroundPageListenerStateVersion;

        // FIXME: <https://webkit.org/b/248889> Check to see if the background page listens to onStartup().
        bool backgroundPageListensToOnStartup = false;
        loadBackgroundPageListenersFromStorage();

        // FIXME: <https://webkit.org/b/248889> Check to see if the extension is being loaded as part of startup.
        if (m_backgroundContentEventListeners.isEmpty() || savedVersionNumberDoesNotMatchCurrentVersionNumber || backgroundPageListensToOnStartup)
            loadBackgroundWebView();
        else
            m_shouldFireStartupEvent = false;
    } else
        loadBackgroundWebView();
}

void WebExtensionContext::loadBackgroundWebView()
{
    ASSERT(isLoaded());

    if (!extension().hasBackgroundContent())
        return;

    ASSERT(!m_backgroundWebView);
    m_backgroundWebView = [[WKWebView alloc] initWithFrame:CGRectZero configuration:webViewConfiguration()];

    m_lastBackgroundContentLoadDate = NSDate.now;

    m_backgroundWebView.get().UIDelegate = m_delegate.get();
    m_backgroundWebView.get().navigationDelegate = m_delegate.get();
    m_backgroundWebView.get().inspectable = m_inspectable;

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

void WebExtensionContext::wakeUpBackgroundContentIfNecessary(CompletionHandler<void()>&& completionHandler)
{
    if (!extension().backgroundContentPath() || extension().backgroundContentIsPersistent()) {
        completionHandler();
        return;
    }

    if (m_backgroundWebView) {
        bool backgroundContentIsLoading = !m_actionsToPerformAfterBackgroundContentLoads.isEmpty();
        if (backgroundContentIsLoading)
            queueEventToFireAfterBackgroundContentLoads(WTFMove(completionHandler));
        else {
            scheduleBackgroundContentToUnload();
            completionHandler();
        }

        return;
    }

    queueEventToFireAfterBackgroundContentLoads(WTFMove(completionHandler));
    loadBackgroundWebView();
}

void WebExtensionContext::scheduleBackgroundContentToUnload()
{
    if (extension().backgroundContentIsPersistent())
        return;

    ASSERT(m_backgroundWebView);

    // FIXME: <https://webkit.org/b/246483> Don't unload the background page if there are open ports, pending website requests, or if an inspector window is open.
}

void WebExtensionContext::queueStartupAndInstallEventsForExtensionIfNecessary()
{
    // FIXME: <https://webkit.org/b/248889> Add support for setup and install events for web extensions.

    bool didQueueStartupEvent = false;

    // FIXME: <https://webkit.org/b/249266> The version number changing isn't the most accurate way to determine if an extension was updated.
    NSString *webExtensionVersion = extension().version();
    NSString *lastSeenVersion = [m_state objectForKey:lastSeenVersionStateKey];
    BOOL extensionVersionDidChange = lastSeenVersion && ![lastSeenVersion isEqualToString:webExtensionVersion];

    if (extensionVersionDidChange) {
        // FIXME: Remove declarative net request modified rulesets.
        [m_state removeObjectForKey:backgroundContentEventListenersKey];

        // FIXME: <https://webkit.org/b/248889> Set queued install event details.
    } else if (!didQueueStartupEvent) {
        // FIXME: <https://webkit.org/b/248889> Set queued install event details.
    }

    [m_state setObject:webExtensionVersion forKey:lastSeenVersionStateKey];
}

void WebExtensionContext::loadBackgroundPageListenersFromStorage()
{
    NSData *listenersData = [m_state objectForKey:backgroundContentEventListenersKey];
    NSCountedSet *savedListeners = [NSKeyedUnarchiver _strictlyUnarchivedObjectOfClasses:[NSSet setWithObjects:NSCountedSet.class, NSNumber.class, nil] fromData:listenersData error:nil];

    m_backgroundContentEventListeners.clear();
    for (NSNumber *entry in savedListeners)
        m_backgroundContentEventListeners.add(static_cast<WebExtensionEventListenerType>(entry.unsignedIntValue), [savedListeners countForObject:entry]);
}

void WebExtensionContext::saveBackgroundPageListenersToStorage()
{
    if (extension().backgroundContentIsPersistent())
        return;

    NSCountedSet *listeners = [NSCountedSet set];
    for (auto& entry : m_backgroundContentEventListeners)
        [listeners addObject:@(static_cast<unsigned>(entry.key))];

    NSData *newBackgroundPageListenersAsData = [NSKeyedArchiver archivedDataWithRootObject:listeners requiringSecureCoding:YES error:nil];
    NSData *savedBackgroundPageListenersAsData = [m_state objectForKey:backgroundContentEventListenersKey];
    [m_state setObject:newBackgroundPageListenersAsData forKey:backgroundContentEventListenersKey];

    NSNumber *savedListenerVersionNumber = [m_state objectForKey:lastSeenVersionStateKey];
    [m_state setObject:@(currentBackgroundPageListenerStateVersion) forKey:lastSeenVersionStateKey];

    bool hasListenerStateChanged = ![newBackgroundPageListenersAsData isEqualToData:savedBackgroundPageListenersAsData];
    bool hasVersionNumberChanged = savedListenerVersionNumber.integerValue != currentBackgroundPageListenerStateVersion;
    if (hasListenerStateChanged || hasVersionNumberChanged)
        writeStateToStorage();
}

uint64_t WebExtensionContext::loadBackgroundPageListenersVersionNumberFromStorage()
{
    return static_cast<uint64_t>([[m_state objectForKey:lastSeenVersionStateKey] integerValue]);
}

void WebExtensionContext::performTasksAfterBackgroundContentLoads()
{
    // FIXME: <https://webkit.org/b/246483> Implement. Fire setup and install events (if needed), etc.

    for (auto& event : m_actionsToPerformAfterBackgroundContentLoads)
        event();
    m_actionsToPerformAfterBackgroundContentLoads.clear();

    saveBackgroundPageListenersToStorage();

    scheduleBackgroundContentToUnload();
}

void WebExtensionContext::wakeUpBackgroundContentIfNecessaryToFireEvents(EventListenerTypeSet types, CompletionHandler<void()>&& completionHandler)
{
    if (extension().backgroundContentIsPersistent()) {
        completionHandler();
        return;
    }

    bool backgroundContentListensToAtLeastOneEvent = false;
    for (auto& type : types) {
        if (m_backgroundContentEventListeners.contains(type)) {
            backgroundContentListensToAtLeastOneEvent = true;
            break;
        }
    }

    if (!m_backgroundWebView && backgroundContentListensToAtLeastOneEvent) {
        queueEventToFireAfterBackgroundContentLoads(WTFMove(completionHandler));
        loadBackgroundWebView();
        return;
    }

    bool backgroundContentIsLoading = !m_actionsToPerformAfterBackgroundContentLoads.isEmpty();
    if (backgroundContentIsLoading)
        queueEventToFireAfterBackgroundContentLoads(WTFMove(completionHandler));
    else {
        if (backgroundContentListensToAtLeastOneEvent)
            scheduleBackgroundContentToUnload();

        completionHandler();
    }
}

void WebExtensionContext::queueEventToFireAfterBackgroundContentLoads(CompletionHandler<void()> &&completionHandler)
{
    ASSERT(extension().backgroundContentPath());

    m_actionsToPerformAfterBackgroundContentLoads.append(WTFMove(completionHandler));
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
        Ref executionWorld = injectedContentData.forMainWorld ? API::ContentWorld::pageContentWorld() : *m_contentScriptWorld;

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
    for (id<_WKWebExtensionTab> tabDelegate in m_temporaryTabPermissionMatchPatterns.get().keyEnumerator) {
        auto tab = getOrCreateTab(tabDelegate);
        NSURL *currentURL = tab->url();
        if (!currentURL)
            continue;

        if (pattern.matchesURL(currentURL))
            [tabsToRemove addObject:tabDelegate];
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
