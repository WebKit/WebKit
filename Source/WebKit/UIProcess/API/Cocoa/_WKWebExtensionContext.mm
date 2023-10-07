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
#import "_WKWebExtensionContextInternal.h"

#import "CocoaHelpers.h"
#import "WKWebView.h"
#import "WebExtension.h"
#import "WebExtensionAction.h"
#import "WebExtensionContext.h"
#import "_WKWebExtensionControllerInternal.h"
#import "_WKWebExtensionInternal.h"
#import "_WKWebExtensionMatchPatternInternal.h"
#import "_WKWebExtensionTab.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/URLParser.h>

NSErrorDomain const _WKWebExtensionContextErrorDomain = @"_WKWebExtensionContextErrorDomain";

NSNotificationName const _WKWebExtensionContextPermissionsWereGrantedNotification = @"_WKWebExtensionContextPermissionsWereGranted";
NSNotificationName const _WKWebExtensionContextPermissionsWereDeniedNotification = @"_WKWebExtensionContextPermissionsWereDenied";
NSNotificationName const _WKWebExtensionContextGrantedPermissionsWereRemovedNotification = @"_WKWebExtensionContextGrantedPermissionsWereRemoved";
NSNotificationName const _WKWebExtensionContextDeniedPermissionsWereRemovedNotification = @"_WKWebExtensionContextDeniedPermissionsWereRemoved";

NSNotificationName const _WKWebExtensionContextPermissionMatchPatternsWereGrantedNotification = @"_WKWebExtensionContextPermissionMatchPatternsWereGranted";
NSNotificationName const _WKWebExtensionContextPermissionMatchPatternsWereDeniedNotification = @"_WKWebExtensionContextPermissionMatchPatternsWereDenied";
NSNotificationName const _WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification = @"_WKWebExtensionContextGrantedPermissionMatchPatternsWereRemoved";
NSNotificationName const _WKWebExtensionContextDeniedPermissionMatchPatternsWereRemovedNotification = @"_WKWebExtensionContextDeniedPermissionMatchPatternsWereRemoved";

_WKWebExtensionContextNotificationUserInfoKey const _WKWebExtensionContextNotificationUserInfoKeyPermissions = @"permissions";
_WKWebExtensionContextNotificationUserInfoKey const _WKWebExtensionContextNotificationUserInfoKeyMatchPatterns = @"matchPatterns";

@implementation _WKWebExtensionContext

#if ENABLE(WK_WEB_EXTENSIONS)

+ (instancetype)contextForExtension:(_WKWebExtension *)extension
{
    NSParameterAssert([extension isKindOfClass:_WKWebExtension.class]);

    return [[self alloc] initForExtension:extension];
}

- (instancetype)initForExtension:(_WKWebExtension *)extension
{
    NSParameterAssert([extension isKindOfClass:_WKWebExtension.class]);

    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebExtensionContext>(self, extension._webExtension);

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKWebExtensionContext.class, self))
        return;

    _webExtensionContext->~WebExtensionContext();
}

- (_WKWebExtension *)webExtension
{
    return wrapper(&_webExtensionContext->extension());
}

- (_WKWebExtensionController *)webExtensionController
{
    return wrapper(_webExtensionContext->extensionController());
}

- (BOOL)isLoaded
{
    return _webExtensionContext->isLoaded();
}

- (NSURL *)baseURL
{
    return _webExtensionContext->baseURL();
}

- (void)setBaseURL:(NSURL *)baseURL
{
    NSParameterAssert([baseURL isKindOfClass:NSURL.class]);
    NSAssert1(WTF::URLParser::maybeCanonicalizeScheme(String(baseURL.scheme)), @"Invalid parameter: '%@' is not a valid URL scheme", baseURL.scheme);
    NSAssert1(![WKWebView handlesURLScheme:baseURL.scheme], @"Invalid parameter: '%@' is a URL scheme that WKWebView handles natively and cannot be used for extensions", baseURL.scheme);
    NSAssert(!baseURL.path.length || [baseURL.path isEqualToString:@"/"], @"Invalid parameter: a URL with a path cannot be used");

    _webExtensionContext->setBaseURL(baseURL);
}

- (NSString *)uniqueIdentifier
{
    return _webExtensionContext->uniqueIdentifier();
}

- (void)setUniqueIdentifier:(NSString *)uniqueIdentifier
{
    NSParameterAssert([uniqueIdentifier isKindOfClass:NSString.class]);

    _webExtensionContext->setUniqueIdentifier(uniqueIdentifier);
}

- (BOOL)isInspectable
{
    return _webExtensionContext->isInspectable();
}

- (void)setInspectable:(BOOL)inspectable
{
    _webExtensionContext->setInspectable(inspectable);
}

static inline WallTime toImpl(NSDate *date)
{
    NSCParameterAssert(!date || [date isKindOfClass:NSDate.class]);
    return date ? WebKit::toImpl(date) : WebKit::toImpl(NSDate.distantFuture);
}

static inline NSDictionary<_WKWebExtensionPermission, NSDate *> *toAPI(const WebKit::WebExtensionContext::PermissionsMap& permissions)
{
    NSMutableDictionary<_WKWebExtensionPermission, NSDate *> *result = [NSMutableDictionary dictionaryWithCapacity:permissions.size()];

    for (auto& entry : permissions)
        [result setObject:WebKit::toAPI(entry.value) forKey:entry.key];

    return [result copy];
}

static inline NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *toAPI(const WebKit::WebExtensionContext::PermissionMatchPatternsMap& permissionMatchPatterns)
{
    NSMutableDictionary<_WKWebExtensionMatchPattern *, NSDate *> *result = [NSMutableDictionary dictionaryWithCapacity:permissionMatchPatterns.size()];

    for (auto& entry : permissionMatchPatterns)
        [result setObject:WebKit::toAPI(entry.value) forKey:wrapper(entry.key)];

    return [result copy];
}

static inline WebKit::WebExtensionContext::PermissionsMap toImpl(NSDictionary<_WKWebExtensionPermission, NSDate *> *permissions)
{
    __block WebKit::WebExtensionContext::PermissionsMap result;
    result.reserveInitialCapacity(permissions.count);

    [permissions enumerateKeysAndObjectsUsingBlock:^(_WKWebExtensionPermission permission, NSDate *date, BOOL *) {
        NSCParameterAssert([permission isKindOfClass:NSString.class]);
        NSCParameterAssert([date isKindOfClass:NSDate.class]);
        result.set(permission, toImpl(date));
    }];

    return result;
}

static inline WebKit::WebExtensionContext::PermissionMatchPatternsMap toImpl(NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *permissionMatchPatterns)
{
    __block WebKit::WebExtensionContext::PermissionMatchPatternsMap result;
    result.reserveInitialCapacity(permissionMatchPatterns.count);

    [permissionMatchPatterns enumerateKeysAndObjectsUsingBlock:^(_WKWebExtensionMatchPattern *origin, NSDate *date, BOOL *) {
        NSCParameterAssert([origin isKindOfClass:_WKWebExtensionMatchPattern.class]);
        NSCParameterAssert([date isKindOfClass:NSDate.class]);
        result.set(origin._webExtensionMatchPattern, toImpl(date));
    }];

    return result;
}

- (NSDictionary<_WKWebExtensionPermission, NSDate *> *)grantedPermissions
{
    return toAPI(_webExtensionContext->grantedPermissions());
}

- (void)setGrantedPermissions:(NSDictionary<_WKWebExtensionPermission, NSDate *> *)grantedPermissions
{
    NSParameterAssert([grantedPermissions isKindOfClass:NSDictionary.class]);

    _webExtensionContext->setGrantedPermissions(toImpl(grantedPermissions));
}

- (NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *)grantedPermissionMatchPatterns
{
    return toAPI(_webExtensionContext->grantedPermissionMatchPatterns());
}

- (void)setGrantedPermissionMatchPatterns:(NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *)grantedPermissionMatchPatterns
{
    NSParameterAssert([grantedPermissionMatchPatterns isKindOfClass:NSDictionary.class]);

    _webExtensionContext->setGrantedPermissionMatchPatterns(toImpl(grantedPermissionMatchPatterns));
}

- (NSDictionary<_WKWebExtensionPermission, NSDate *> *)deniedPermissions
{
    return toAPI(_webExtensionContext->deniedPermissions());
}

- (void)setDeniedPermissions:(NSDictionary<_WKWebExtensionPermission, NSDate *> *)deniedPermissions
{
    NSParameterAssert([deniedPermissions isKindOfClass:NSDictionary.class]);

    _webExtensionContext->setDeniedPermissions(toImpl(deniedPermissions));
}

- (NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *)deniedPermissionMatchPatterns
{
    return toAPI(_webExtensionContext->deniedPermissionMatchPatterns());
}

- (void)setDeniedPermissionMatchPatterns:(NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *)deniedPermissionMatchPatterns
{
    NSParameterAssert([deniedPermissionMatchPatterns isKindOfClass:NSDictionary.class]);

    _webExtensionContext->setDeniedPermissionMatchPatterns(toImpl(deniedPermissionMatchPatterns));
}

- (BOOL)requestedOptionalAccessToAllHosts
{
    return _webExtensionContext->requestedOptionalAccessToAllHosts();
}

- (void)setRequestedOptionalAccessToAllHosts:(BOOL)requested
{
    return _webExtensionContext->setRequestedOptionalAccessToAllHosts(requested);
}

- (BOOL)hasAccessInPrivateBrowsing
{
    return _webExtensionContext->hasAccessInPrivateBrowsing();
}

- (void)setHasAccessInPrivateBrowsing:(BOOL)hasAccess
{
    return _webExtensionContext->setHasAccessInPrivateBrowsing(hasAccess);
}

static inline NSSet<_WKWebExtensionPermission> *toAPI(const WebKit::WebExtensionContext::PermissionsMap::KeysConstIteratorRange& permissions)
{
    if (!permissions.size())
        return [NSSet set];

    NSMutableSet<_WKWebExtensionPermission> *result = [NSMutableSet setWithCapacity:permissions.size()];

    for (auto& permission : permissions)
        [result addObject:permission];

    return [result copy];
}

static inline NSSet<_WKWebExtensionMatchPattern *> *toAPI(const WebKit::WebExtensionContext::PermissionMatchPatternsMap::KeysConstIteratorRange& permissionMatchPatterns)
{
    if (!permissionMatchPatterns.size())
        return [NSSet set];

    NSMutableSet<_WKWebExtensionMatchPattern *> *result = [NSMutableSet setWithCapacity:permissionMatchPatterns.size()];

    for (auto& origin : permissionMatchPatterns)
        [result addObject:wrapper(origin)];

    return [result copy];
}

- (NSSet<_WKWebExtensionPermission> *)currentPermissions
{
    return toAPI(_webExtensionContext->currentPermissions());
}

- (NSSet<_WKWebExtensionMatchPattern *> *)currentPermissionMatchPatterns
{
    return toAPI(_webExtensionContext->currentPermissionMatchPatterns());
}

- (BOOL)hasPermission:(_WKWebExtensionPermission)permission
{
    NSParameterAssert([permission isKindOfClass:NSString.class]);

    return [self hasPermission:permission inTab:nil];
}

- (BOOL)hasPermission:(_WKWebExtensionPermission)permission inTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert([permission isKindOfClass:NSString.class]);
    if (tab)
        NSParameterAssert([tab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    return _webExtensionContext->hasPermission(permission, toImplNullable(tab, *_webExtensionContext).get());
}

- (BOOL)hasAccessToURL:(NSURL *)url
{
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    return [self hasAccessToURL:url inTab:nil];
}

- (BOOL)hasAccessToURL:(NSURL *)url inTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert([url isKindOfClass:NSURL.class]);
    if (tab)
        NSParameterAssert([tab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    return _webExtensionContext->hasPermission(url, toImplNullable(tab, *_webExtensionContext).get());
}

static inline _WKWebExtensionContextPermissionStatus toAPI(WebKit::WebExtensionContext::PermissionState status)
{
    switch (status) {
    case WebKit::WebExtensionContext::PermissionState::DeniedExplicitly:
        return _WKWebExtensionContextPermissionStatusDeniedExplicitly;
    case WebKit::WebExtensionContext::PermissionState::DeniedImplicitly:
        return _WKWebExtensionContextPermissionStatusDeniedImplicitly;
    case WebKit::WebExtensionContext::PermissionState::RequestedImplicitly:
        return _WKWebExtensionContextPermissionStatusRequestedImplicitly;
    case WebKit::WebExtensionContext::PermissionState::Unknown:
        return _WKWebExtensionContextPermissionStatusUnknown;
    case WebKit::WebExtensionContext::PermissionState::RequestedExplicitly:
        return _WKWebExtensionContextPermissionStatusRequestedExplicitly;
    case WebKit::WebExtensionContext::PermissionState::GrantedImplicitly:
        return _WKWebExtensionContextPermissionStatusGrantedImplicitly;
    case WebKit::WebExtensionContext::PermissionState::GrantedExplicitly:
        return _WKWebExtensionContextPermissionStatusGrantedExplicitly;
    }
}

static inline WebKit::WebExtensionContext::PermissionState toImpl(_WKWebExtensionContextPermissionStatus status)
{
    switch (status) {
    case _WKWebExtensionContextPermissionStatusDeniedExplicitly:
        return WebKit::WebExtensionContext::PermissionState::DeniedExplicitly;
    case _WKWebExtensionContextPermissionStatusDeniedImplicitly:
        return WebKit::WebExtensionContext::PermissionState::DeniedImplicitly;
    case _WKWebExtensionContextPermissionStatusRequestedImplicitly:
        return WebKit::WebExtensionContext::PermissionState::RequestedImplicitly;
    case _WKWebExtensionContextPermissionStatusUnknown:
        return WebKit::WebExtensionContext::PermissionState::Unknown;
    case _WKWebExtensionContextPermissionStatusRequestedExplicitly:
        return WebKit::WebExtensionContext::PermissionState::RequestedExplicitly;
    case _WKWebExtensionContextPermissionStatusGrantedImplicitly:
        return WebKit::WebExtensionContext::PermissionState::GrantedImplicitly;
    case _WKWebExtensionContextPermissionStatusGrantedExplicitly:
        return WebKit::WebExtensionContext::PermissionState::GrantedExplicitly;
    }

    ASSERT_NOT_REACHED();
    return WebKit::WebExtensionContext::PermissionState::Unknown;
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForPermission:(_WKWebExtensionPermission)permission
{
    NSParameterAssert([permission isKindOfClass:NSString.class]);

    return [self permissionStatusForPermission:permission inTab:nil];
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForPermission:(_WKWebExtensionPermission)permission inTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert([permission isKindOfClass:NSString.class]);
    if (tab)
        NSParameterAssert([tab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    return toAPI(_webExtensionContext->permissionState(permission, toImplNullable(tab, *_webExtensionContext).get()));
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forPermission:(_WKWebExtensionPermission)permission
{
    NSParameterAssert(status == _WKWebExtensionContextPermissionStatusDeniedExplicitly || status == _WKWebExtensionContextPermissionStatusUnknown || status == _WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert([permission isKindOfClass:NSString.class]);

    [self setPermissionStatus:status forPermission:permission expirationDate:nil];
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forPermission:(_WKWebExtensionPermission)permission expirationDate:(NSDate *)expirationDate
{
    NSParameterAssert(status == _WKWebExtensionContextPermissionStatusDeniedExplicitly || status == _WKWebExtensionContextPermissionStatusUnknown || status == _WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert([permission isKindOfClass:NSString.class]);

    _webExtensionContext->setPermissionState(toImpl(status), permission, toImpl(expirationDate));
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForURL:(NSURL *)url
{
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    return [self permissionStatusForURL:url inTab:nil];
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForURL:(NSURL *)url inTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert([url isKindOfClass:NSURL.class]);
    if (tab)
        NSParameterAssert([tab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    return toAPI(_webExtensionContext->permissionState(url, toImplNullable(tab, *_webExtensionContext).get()));
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forURL:(NSURL *)url
{
    NSParameterAssert(status == _WKWebExtensionContextPermissionStatusDeniedExplicitly || status == _WKWebExtensionContextPermissionStatusUnknown || status == _WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    [self setPermissionStatus:status forURL:url expirationDate:nil];
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forURL:(NSURL *)url expirationDate:(NSDate *)expirationDate
{
    NSParameterAssert(status == _WKWebExtensionContextPermissionStatusDeniedExplicitly || status == _WKWebExtensionContextPermissionStatusUnknown || status == _WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    _webExtensionContext->setPermissionState(toImpl(status), url, toImpl(expirationDate));
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForMatchPattern:(_WKWebExtensionMatchPattern *)pattern
{
    NSParameterAssert([pattern isKindOfClass:_WKWebExtensionMatchPattern.class]);

    return [self permissionStatusForMatchPattern:pattern inTab:nil];
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForMatchPattern:(_WKWebExtensionMatchPattern *)pattern inTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert([pattern isKindOfClass:_WKWebExtensionMatchPattern.class]);
    if (tab)
        NSParameterAssert([tab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    return toAPI(_webExtensionContext->permissionState(pattern._webExtensionMatchPattern, toImplNullable(tab, *_webExtensionContext).get()));
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forMatchPattern:(_WKWebExtensionMatchPattern *)pattern
{
    NSParameterAssert(status == _WKWebExtensionContextPermissionStatusDeniedExplicitly || status == _WKWebExtensionContextPermissionStatusUnknown || status == _WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert([pattern isKindOfClass:_WKWebExtensionMatchPattern.class]);

    [self setPermissionStatus:status forMatchPattern:pattern expirationDate:nil];
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forMatchPattern:(_WKWebExtensionMatchPattern *)pattern expirationDate:(NSDate *)expirationDate
{
    NSParameterAssert(status == _WKWebExtensionContextPermissionStatusDeniedExplicitly || status == _WKWebExtensionContextPermissionStatusUnknown || status == _WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert([pattern isKindOfClass:_WKWebExtensionMatchPattern.class]);

    _webExtensionContext->setPermissionState(toImpl(status), pattern._webExtensionMatchPattern, toImpl(expirationDate));
}

- (BOOL)hasAccessToAllURLs
{
    return _webExtensionContext->hasAccessToAllURLs();
}

- (BOOL)hasAccessToAllHosts
{
    return _webExtensionContext->hasAccessToAllHosts();
}

- (BOOL)hasInjectedContentForURL:(NSURL *)url
{
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    return _webExtensionContext->hasInjectedContentForURL(url);
}

- (_WKWebExtensionAction *)actionForTab:(id<_WKWebExtensionTab>)tab
{
    if (tab)
        NSParameterAssert([tab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    return _webExtensionContext->getOrCreateAction(toImplNullable(tab, *_webExtensionContext).get())->wrapper();
}

- (void)performActionForTab:(id<_WKWebExtensionTab>)tab
{
    if (tab)
        NSParameterAssert([tab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    _webExtensionContext->performAction(toImplNullable(tab, *_webExtensionContext).get(), WebKit::WebExtensionContext::UserTriggered::Yes);
}

- (void)userGesturePerformedInTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert([tab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    _webExtensionContext->userGesturePerformed(toImpl(tab, *_webExtensionContext));
}

- (BOOL)hasActiveUserGestureInTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert([tab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    return _webExtensionContext->hasActiveUserGesture(toImpl(tab, *_webExtensionContext));
}

- (void)clearUserGestureInTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert([tab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    _webExtensionContext->clearUserGesture(toImpl(tab, *_webExtensionContext));
}

static inline id<_WKWebExtensionWindow> toAPI(const RefPtr<WebKit::WebExtensionWindow>& window)
{
    return window ? window->delegate() : nil;
}

static inline NSArray *toAPI(const WebKit::WebExtensionContext::WindowVector& windows)
{
    if (windows.isEmpty())
        return [NSArray array];

    NSMutableArray *result = [[NSMutableArray alloc] initWithCapacity:windows.size()];

    for (auto& window : windows) {
        if (auto delegate = window->delegate())
            [result addObject:delegate];
    }

    return [result copy];
}

- (NSArray<id<_WKWebExtensionWindow>> *)openWindows
{
    return toAPI(_webExtensionContext->openWindows());
}

- (id<_WKWebExtensionWindow>)focusedWindow
{
    return toAPI(_webExtensionContext->focusedWindow(WebKit::WebExtensionContext::IgnoreExtensionAccess::Yes));
}

static inline NSSet *toAPI(const WebKit::WebExtensionContext::TabMapValueIterator& tabs)
{
    if (tabs.isEmpty())
        return [NSSet set];

    NSMutableSet *result = [[NSMutableSet alloc] initWithCapacity:tabs.size()];

    for (auto& tab : tabs) {
        if (auto delegate = tab->delegate())
            [result addObject:delegate];
    }

    return [result copy];
}

- (NSSet<id<_WKWebExtensionTab>> *)openTabs
{
    return toAPI(_webExtensionContext->openTabs());
}

static inline Ref<WebKit::WebExtensionWindow> toImpl(id<_WKWebExtensionWindow> window, WebKit::WebExtensionContext& context)
{
    return context.getOrCreateWindow(window);
}

- (void)didOpenWindow:(id<_WKWebExtensionWindow>)newWindow
{
    NSParameterAssert([newWindow conformsToProtocol:@protocol(_WKWebExtensionWindow)]);

    _webExtensionContext->didOpenWindow(toImpl(newWindow, *_webExtensionContext));
}

- (void)didCloseWindow:(id<_WKWebExtensionWindow>)closedWindow
{
    NSParameterAssert([closedWindow conformsToProtocol:@protocol(_WKWebExtensionWindow)]);

    _webExtensionContext->didCloseWindow(toImpl(closedWindow, *_webExtensionContext));
}

- (void)didFocusWindow:(id<_WKWebExtensionWindow>)focusedWindow
{
    if (focusedWindow)
        NSParameterAssert([focusedWindow conformsToProtocol:@protocol(_WKWebExtensionWindow)]);

    _webExtensionContext->didFocusWindow(focusedWindow ? toImpl(focusedWindow, *_webExtensionContext).ptr() : nullptr);
}

static inline Ref<WebKit::WebExtensionTab> toImpl(id<_WKWebExtensionTab> tab, WebKit::WebExtensionContext& context)
{
    return context.getOrCreateTab(tab);
}

static inline RefPtr<WebKit::WebExtensionTab> toImplNullable(id<_WKWebExtensionTab> tab, WebKit::WebExtensionContext& context)
{
    return tab ? toImpl(tab, context).ptr() : nullptr;
}

static inline WebKit::WebExtensionContext::TabSet toImpl(NSSet<id<_WKWebExtensionTab>> *tabs, WebKit::WebExtensionContext& context)
{
    WebKit::WebExtensionContext::TabSet result;
    result.reserveInitialCapacity(tabs.count);

    for (id<_WKWebExtensionTab> tab in tabs) {
        NSCParameterAssert([tab conformsToProtocol:@protocol(_WKWebExtensionTab)]);
        result.addVoid(context.getOrCreateTab(tab));
    }

    return result;
}

- (void)didOpenTab:(id<_WKWebExtensionTab>)newTab
{
    NSParameterAssert([newTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    _webExtensionContext->didOpenTab(toImpl(newTab, *_webExtensionContext));
}

- (void)didCloseTab:(id<_WKWebExtensionTab>)closedTab windowIsClosing:(BOOL)windowIsClosing
{
    NSParameterAssert([closedTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    _webExtensionContext->didCloseTab(toImpl(closedTab, *_webExtensionContext), windowIsClosing ? WebKit::WebExtensionContext::WindowIsClosing::Yes : WebKit::WebExtensionContext::WindowIsClosing::No);
}

- (void)didActivateTab:(id<_WKWebExtensionTab>)activatedTab previousActiveTab:(id<_WKWebExtensionTab>)previousTab
{
    NSParameterAssert([activatedTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);
    if (previousTab)
        NSParameterAssert([previousTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    _webExtensionContext->didActivateTab(toImpl(activatedTab, *_webExtensionContext), toImplNullable(previousTab, *_webExtensionContext).get());
}

- (void)didSelectTabs:(NSSet<id<_WKWebExtensionTab>> *)selectedTabs
{
    NSParameterAssert([selectedTabs isKindOfClass:NSSet.class]);

    _webExtensionContext->didSelectOrDeselectTabs(toImpl(selectedTabs, *_webExtensionContext));
}

- (void)didDeselectTabs:(NSSet<id<_WKWebExtensionTab>> *)deselectedTabs
{
    NSParameterAssert([deselectedTabs isKindOfClass:NSSet.class]);

    _webExtensionContext->didSelectOrDeselectTabs(toImpl(deselectedTabs, *_webExtensionContext));
}

- (void)didMoveTab:(id<_WKWebExtensionTab>)movedTab fromIndex:(NSUInteger)index inWindow:(id<_WKWebExtensionWindow>)oldWindow
{
    NSParameterAssert([movedTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);
    if (oldWindow)
        NSParameterAssert([oldWindow conformsToProtocol:@protocol(_WKWebExtensionWindow)]);

    _webExtensionContext->didMoveTab(toImpl(movedTab, *_webExtensionContext), index, oldWindow ? toImpl(oldWindow, *_webExtensionContext).ptr() : nullptr);
}

- (void)didReplaceTab:(id<_WKWebExtensionTab>)oldTab withTab:(id<_WKWebExtensionTab>)newTab
{
    NSParameterAssert([oldTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);
    NSParameterAssert([newTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    _webExtensionContext->didReplaceTab(toImpl(oldTab, *_webExtensionContext), toImpl(newTab, *_webExtensionContext));
}

static inline OptionSet<WebKit::WebExtensionTab::ChangedProperties> toImpl(_WKWebExtensionTabChangedProperties properties)
{
    OptionSet<WebKit::WebExtensionTab::ChangedProperties> result;

    if (properties == _WKWebExtensionTabChangedPropertiesNone)
        return result;

    if (properties == _WKWebExtensionTabChangedPropertiesAll) {
        result.add(WebKit::WebExtensionTab::ChangedProperties::All);
        return result;
    }

    if (properties & _WKWebExtensionTabChangedPropertiesAudible)
        result.add(WebKit::WebExtensionTab::ChangedProperties::Audible);

    if (properties & _WKWebExtensionTabChangedPropertiesLoading)
        result.add(WebKit::WebExtensionTab::ChangedProperties::Loading);

    if (properties & _WKWebExtensionTabChangedPropertiesMuted)
        result.add(WebKit::WebExtensionTab::ChangedProperties::Muted);

    if (properties & _WKWebExtensionTabChangedPropertiesPinned)
        result.add(WebKit::WebExtensionTab::ChangedProperties::Pinned);

    if (properties & _WKWebExtensionTabChangedPropertiesReaderMode)
        result.add(WebKit::WebExtensionTab::ChangedProperties::ReaderMode);

    if (properties & _WKWebExtensionTabChangedPropertiesSize)
        result.add(WebKit::WebExtensionTab::ChangedProperties::Size);

    if (properties & _WKWebExtensionTabChangedPropertiesTitle)
        result.add(WebKit::WebExtensionTab::ChangedProperties::Title);

    if (properties & _WKWebExtensionTabChangedPropertiesURL)
        result.add(WebKit::WebExtensionTab::ChangedProperties::URL);

    if (properties & _WKWebExtensionTabChangedPropertiesZoomFactor)
        result.add(WebKit::WebExtensionTab::ChangedProperties::ZoomFactor);

    return result;
}

- (void)didChangeTabProperties:(_WKWebExtensionTabChangedProperties)properties forTab:(id<_WKWebExtensionTab>)changedTab
{
    NSParameterAssert([changedTab conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    _webExtensionContext->didChangeTabProperties(toImpl(changedTab, *_webExtensionContext), toImpl(properties));
}

- (BOOL)_inTestingMode
{
    return _webExtensionContext->inTestingMode();
}

- (void)_setTestingMode:(BOOL)testingMode
{
    _webExtensionContext->setTestingMode(testingMode);
}

- (WKWebView *)_backgroundWebView
{
    return _webExtensionContext->backgroundWebView();
}

- (NSURL *)_backgroundContentURL
{
    return _webExtensionContext->backgroundContentURL();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_webExtensionContext;
}

- (WebKit::WebExtensionContext&)_webExtensionContext
{
    return *_webExtensionContext;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

+ (instancetype)contextForExtension:(_WKWebExtension *)extension
{
    return nil;
}

- (instancetype)initForExtension:(_WKWebExtension *)extension
{
    return nil;
}

- (_WKWebExtension *)webExtension
{
    return nil;
}

- (_WKWebExtensionController *)webExtensionController
{
    return nil;
}

- (BOOL)isLoaded
{
    return NO;
}

- (NSURL *)baseURL
{
    return nil;
}

- (void)setBaseURL:(NSURL *)baseURL
{
}

- (NSString *)uniqueIdentifier
{
    return nil;
}

- (void)setUniqueIdentifier:(NSString *)uniqueIdentifier
{
}

- (BOOL)isInspectable
{
    return NO;
}

- (void)setInspectable:(BOOL)inspectable
{
}

- (NSDictionary<_WKWebExtensionPermission, NSDate *> *)grantedPermissions
{
    return nil;
}

- (void)setGrantedPermissions:(NSDictionary<_WKWebExtensionPermission, NSDate *> *)grantedPermissions
{
}

- (NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *)grantedPermissionMatchPatterns
{
    return nil;
}

- (void)setGrantedPermissionMatchPatterns:(NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *)grantedPermissionMatchPatterns
{
}

- (NSDictionary<_WKWebExtensionPermission, NSDate *> *)deniedPermissions
{
    return nil;
}

- (void)setDeniedPermissions:(NSDictionary<_WKWebExtensionPermission, NSDate *> *)deniedPermissions
{
}

- (NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *)deniedPermissionMatchPatterns
{
    return nil;
}

- (void)setDeniedPermissionMatchPatterns:(NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *)deniedPermissionMatchPatterns
{
}

- (BOOL)requestedOptionalAccessToAllHosts
{
    return NO;
}

- (void)setRequestedOptionalAccessToAllHosts:(BOOL)requested
{
}

- (BOOL)hasAccessInPrivateBrowsing
{
    return NO;
}

- (void)setHasAccessInPrivateBrowsing:(BOOL)hasAccess
{
}

- (NSSet<_WKWebExtensionPermission> *)currentPermissions
{
    return nil;
}

- (NSSet<_WKWebExtensionMatchPattern *> *)currentPermissionMatchPatterns
{
    return nil;
}

- (BOOL)hasPermission:(_WKWebExtensionPermission)permission
{
    return NO;
}

- (BOOL)hasPermission:(_WKWebExtensionPermission)permission inTab:(id<_WKWebExtensionTab>)tab
{
    return NO;
}

- (BOOL)hasAccessToURL:(NSURL *)url
{
    return NO;
}

- (BOOL)hasAccessToURL:(NSURL *)url inTab:(id<_WKWebExtensionTab>)tab
{
    return NO;
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForPermission:(_WKWebExtensionPermission)permission
{
    return _WKWebExtensionContextPermissionStatusUnknown;
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForPermission:(_WKWebExtensionPermission)permission inTab:(id<_WKWebExtensionTab>)tab
{
    return _WKWebExtensionContextPermissionStatusUnknown;
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forPermission:(_WKWebExtensionPermission)permission
{
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forPermission:(_WKWebExtensionPermission)permission expirationDate:(NSDate *)expirationDate
{
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForURL:(NSURL *)url
{
    return _WKWebExtensionContextPermissionStatusUnknown;
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForURL:(NSURL *)url inTab:(id<_WKWebExtensionTab>)tab
{
    return _WKWebExtensionContextPermissionStatusUnknown;
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forURL:(NSURL *)url
{
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forURL:(NSURL *)url expirationDate:(NSDate *)expirationDate
{
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForMatchPattern:(_WKWebExtensionMatchPattern *)pattern
{
    return _WKWebExtensionContextPermissionStatusUnknown;
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForMatchPattern:(_WKWebExtensionMatchPattern *)pattern inTab:(id<_WKWebExtensionTab>)tab
{
    return _WKWebExtensionContextPermissionStatusUnknown;
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forMatchPattern:(_WKWebExtensionMatchPattern *)pattern
{
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forMatchPattern:(_WKWebExtensionMatchPattern *)pattern expirationDate:(NSDate *)expirationDate
{
}

- (BOOL)hasAccessToAllURLs
{
    return NO;
}

- (BOOL)hasAccessToAllHosts
{
    return NO;
}

- (BOOL)hasInjectedContentForURL:(NSURL *)url
{
    return NO;
}

- (_WKWebExtensionAction *)actionForTab:(id<_WKWebExtensionTab>)tab NS_SWIFT_NAME(action(for:))
{
    return nil;
}

- (void)performActionForTab:(id<_WKWebExtensionTab>)tab NS_SWIFT_NAME(performAction(for:))
{
}

- (void)userGesturePerformedInTab:(id<_WKWebExtensionTab>)tab
{
}

- (BOOL)hasActiveUserGestureInTab:(id<_WKWebExtensionTab>)tab
{
    return NO;
}

- (void)clearUserGestureInTab:(id<_WKWebExtensionTab>)tab
{
}

- (NSArray<id<_WKWebExtensionWindow>> *)openWindows
{
    return nil;
}

- (id<_WKWebExtensionWindow>)focusedWindow
{
    return nil;
}

- (NSSet<id<_WKWebExtensionTab>> *)openTabs
{
    return nil;
}

- (void)didOpenWindow:(id<_WKWebExtensionWindow>)newWindow
{
}

- (void)didCloseWindow:(id<_WKWebExtensionWindow>)closedWindow
{
}

- (void)didFocusWindow:(id<_WKWebExtensionWindow>)focusedWindow
{
}

- (void)didOpenTab:(id<_WKWebExtensionTab>)newTab
{
}

- (void)didCloseTab:(id<_WKWebExtensionTab>)closedTab windowIsClosing:(BOOL)windowIsClosing
{
}

- (void)didActivateTab:(id<_WKWebExtensionTab>)activatedTab previousActiveTab:(id<_WKWebExtensionTab>)previousTab
{
}

- (void)didSelectTabs:(NSSet<id<_WKWebExtensionTab>> *)selectedTabs
{
}

- (void)didDeselectTabs:(NSSet<id<_WKWebExtensionTab>> *)deselectedTabs
{
}

- (void)didMoveTab:(id<_WKWebExtensionTab>)movedTab fromIndex:(NSUInteger)index inWindow:(id<_WKWebExtensionWindow>)oldWindow
{
}

- (void)didReplaceTab:(id<_WKWebExtensionTab>)oldTab withTab:(id<_WKWebExtensionTab>)newTab
{
}

- (void)didChangeTabProperties:(_WKWebExtensionTabChangedProperties)properties forTab:(id<_WKWebExtensionTab>)changedTab
{
}

- (BOOL)_inTestingMode
{
    return NO;
}

- (void)_setTestingMode:(BOOL)testingMode
{
}

- (WKWebView *)_backgroundWebView
{
    return nil;
}

- (NSURL *)_backgroundContentURL
{
    return nil;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
