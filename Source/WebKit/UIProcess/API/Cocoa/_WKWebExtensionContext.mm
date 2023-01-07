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
    NSParameterAssert(extension);

    return [[self alloc] initForExtension:extension];
}

- (instancetype)initForExtension:(_WKWebExtension *)extension
{
    NSParameterAssert(extension);

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
    NSParameterAssert(baseURL);
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
    NSParameterAssert(uniqueIdentifier);

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
        result.set(permission, toImpl(date));
    }];

    return result;
}

static inline WebKit::WebExtensionContext::PermissionMatchPatternsMap toImpl(NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *permissionMatchPatterns)
{
    __block WebKit::WebExtensionContext::PermissionMatchPatternsMap result;
    result.reserveInitialCapacity(permissionMatchPatterns.count);

    [permissionMatchPatterns enumerateKeysAndObjectsUsingBlock:^(_WKWebExtensionMatchPattern *origin, NSDate *date, BOOL *) {
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
    NSParameterAssert(grantedPermissions);

    _webExtensionContext->setGrantedPermissions(toImpl(grantedPermissions));
}

- (NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *)grantedPermissionMatchPatterns
{
    return toAPI(_webExtensionContext->grantedPermissionMatchPatterns());
}

- (void)setGrantedPermissionMatchPatterns:(NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *)grantedPermissionMatchPatterns
{
    NSParameterAssert(grantedPermissionMatchPatterns);

    _webExtensionContext->setGrantedPermissionMatchPatterns(toImpl(grantedPermissionMatchPatterns));
}

- (NSDictionary<_WKWebExtensionPermission, NSDate *> *)deniedPermissions
{
    return toAPI(_webExtensionContext->deniedPermissions());
}

- (void)setDeniedPermissions:(NSDictionary<_WKWebExtensionPermission, NSDate *> *)deniedPermissions
{
    NSParameterAssert(deniedPermissions);

    _webExtensionContext->setDeniedPermissions(toImpl(deniedPermissions));
}

- (NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *)deniedPermissionMatchPatterns
{
    return toAPI(_webExtensionContext->deniedPermissionMatchPatterns());
}

- (void)setDeniedPermissionMatchPatterns:(NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *)deniedPermissionMatchPatterns
{
    NSParameterAssert(deniedPermissionMatchPatterns);

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

static inline NSSet<_WKWebExtensionPermission> *toAPI(const WebKit::WebExtensionContext::PermissionsMap::KeysConstIteratorRange& permissions)
{
    NSMutableSet<_WKWebExtensionPermission> *result = [NSMutableSet setWithCapacity:permissions.size()];

    for (auto& permission : permissions)
        [result addObject:permission];

    return [result copy];
}

static inline NSSet<_WKWebExtensionMatchPattern *> *toAPI(const WebKit::WebExtensionContext::PermissionMatchPatternsMap::KeysConstIteratorRange& permissionMatchPatterns)
{
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
    NSParameterAssert(permission);

    return [self hasPermission:permission inTab:nil];
}

- (BOOL)hasPermission:(_WKWebExtensionPermission)permission inTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert(permission);

    return _webExtensionContext->hasPermission(permission, tab);
}

- (BOOL)hasAccessToURL:(NSURL *)url
{
    NSParameterAssert(url);

    return [self hasAccessToURL:url inTab:nil];
}

- (BOOL)hasAccessToURL:(NSURL *)url inTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert(url);

    return _webExtensionContext->hasPermission(url, tab);
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
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForPermission:(_WKWebExtensionPermission)permission
{
    NSParameterAssert(permission);

    return [self permissionStatusForPermission:permission inTab:nil];
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForPermission:(_WKWebExtensionPermission)permission inTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert(permission);

    return toAPI(_webExtensionContext->permissionState(permission, tab));
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forPermission:(_WKWebExtensionPermission)permission
{
    NSParameterAssert(status == _WKWebExtensionContextPermissionStatusDeniedExplicitly || status == _WKWebExtensionContextPermissionStatusUnknown || status == _WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert(permission);

    [self setPermissionStatus:status forPermission:permission expirationDate:nil];
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forPermission:(_WKWebExtensionPermission)permission expirationDate:(NSDate *)expirationDate
{
    NSParameterAssert(status == _WKWebExtensionContextPermissionStatusDeniedExplicitly || status == _WKWebExtensionContextPermissionStatusUnknown || status == _WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert(permission);

    _webExtensionContext->setPermissionState(toImpl(status), permission, toImpl(expirationDate));
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForURL:(NSURL *)url
{
    NSParameterAssert(url);

    return [self permissionStatusForURL:url inTab:nil];
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForURL:(NSURL *)url inTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert(url);

    return toAPI(_webExtensionContext->permissionState(url, tab));
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forURL:(NSURL *)url
{
    NSParameterAssert(status == _WKWebExtensionContextPermissionStatusDeniedExplicitly || status == _WKWebExtensionContextPermissionStatusUnknown || status == _WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert(url);

    [self setPermissionStatus:status forURL:url expirationDate:nil];
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forURL:(NSURL *)url expirationDate:(NSDate *)expirationDate
{
    NSParameterAssert(status == _WKWebExtensionContextPermissionStatusDeniedExplicitly || status == _WKWebExtensionContextPermissionStatusUnknown || status == _WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert(url);

    _webExtensionContext->setPermissionState(toImpl(status), url, toImpl(expirationDate));
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForMatchPattern:(_WKWebExtensionMatchPattern *)pattern
{
    NSParameterAssert(pattern);

    return [self permissionStatusForMatchPattern:pattern inTab:nil];
}

- (_WKWebExtensionContextPermissionStatus)permissionStatusForMatchPattern:(_WKWebExtensionMatchPattern *)pattern inTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert(pattern);

    return toAPI(_webExtensionContext->permissionState(pattern._webExtensionMatchPattern, tab));
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forMatchPattern:(_WKWebExtensionMatchPattern *)pattern
{
    NSParameterAssert(status == _WKWebExtensionContextPermissionStatusDeniedExplicitly || status == _WKWebExtensionContextPermissionStatusUnknown || status == _WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert(pattern);

    [self setPermissionStatus:status forMatchPattern:pattern expirationDate:nil];
}

- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forMatchPattern:(_WKWebExtensionMatchPattern *)pattern expirationDate:(NSDate *)expirationDate
{
    NSParameterAssert(status == _WKWebExtensionContextPermissionStatusDeniedExplicitly || status == _WKWebExtensionContextPermissionStatusUnknown || status == _WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert(pattern);

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
    return _webExtensionContext->hasInjectedContentForURL(url);
}

- (BOOL)hasActiveUserGestureInTab:(id<_WKWebExtensionTab>)tab
{
    NSParameterAssert(tab);

    return _webExtensionContext->hasActiveUserGesture(tab);
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

- (BOOL)hasActiveUserGestureInTab:(id<_WKWebExtensionTab>)tab
{
    return NO;
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
