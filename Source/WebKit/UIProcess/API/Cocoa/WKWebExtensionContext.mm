/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#import "WKWebExtensionContextInternal.h"

#import "CocoaHelpers.h"
#import "WKWebExtensionCommandInternal.h"
#import "WKWebExtensionControllerInternal.h"
#import "WKWebExtensionInternal.h"
#import "WKWebExtensionMatchPatternInternal.h"
#import "WKWebExtensionTab.h"
#import "WKWebView.h"
#import "WebExtension.h"
#import "WebExtensionAction.h"
#import "WebExtensionCommand.h"
#import "WebExtensionContext.h"
#import "WebExtensionMatchPattern.h"
#import <wtf/BlockPtr.h>
#import <wtf/URLParser.h>
#import <wtf/cocoa/VectorCocoa.h>

NSErrorDomain const WKWebExtensionContextErrorDomain = @"WKWebExtensionContextErrorDomain";
NSNotificationName const WKWebExtensionContextErrorsDidUpdateNotification = @"WKWebExtensionContextErrorsDidUpdate";

NSNotificationName const WKWebExtensionContextPermissionsWereGrantedNotification = @"WKWebExtensionContextPermissionsWereGranted";
NSNotificationName const WKWebExtensionContextPermissionsWereDeniedNotification = @"WKWebExtensionContextPermissionsWereDenied";
NSNotificationName const WKWebExtensionContextGrantedPermissionsWereRemovedNotification = @"WKWebExtensionContextGrantedPermissionsWereRemoved";
NSNotificationName const WKWebExtensionContextDeniedPermissionsWereRemovedNotification = @"WKWebExtensionContextDeniedPermissionsWereRemoved";

NSNotificationName const WKWebExtensionContextPermissionMatchPatternsWereGrantedNotification = @"WKWebExtensionContextPermissionMatchPatternsWereGranted";
NSNotificationName const WKWebExtensionContextPermissionMatchPatternsWereDeniedNotification = @"WKWebExtensionContextPermissionMatchPatternsWereDenied";
NSNotificationName const WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification = @"WKWebExtensionContextGrantedPermissionMatchPatternsWereRemoved";
NSNotificationName const WKWebExtensionContextDeniedPermissionMatchPatternsWereRemovedNotification = @"WKWebExtensionContextDeniedPermissionMatchPatternsWereRemoved";

WKWebExtensionContextNotificationUserInfoKey const WKWebExtensionContextNotificationUserInfoKeyPermissions = @"permissions";
WKWebExtensionContextNotificationUserInfoKey const WKWebExtensionContextNotificationUserInfoKeyMatchPatterns = @"matchPatterns";

#if USE(APPKIT)
using CocoaMenuItem = NSMenuItem;
#else
using CocoaMenuItem = UIMenuElement;
#endif

@implementation WKWebExtensionContext

#if ENABLE(WK_WEB_EXTENSIONS)

WK_OBJECT_DEALLOC_IMPL_ON_MAIN_THREAD(WKWebExtensionContext, WebExtensionContext, Ref { *_webExtensionContext });

+ (instancetype)contextForExtension:(WKWebExtension *)extension
{
    NSParameterAssert([extension isKindOfClass:WKWebExtension.class]);

    return [[self alloc] initForExtension:extension];
}

- (instancetype)initForExtension:(WKWebExtension *)extension
{
    NSParameterAssert([extension isKindOfClass:WKWebExtension.class]);

    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebExtensionContext>(self, extension._protectedWebExtension.get());

    return self;
}

- (WKWebExtension *)webExtension
{
    return wrapper(Ref { *_webExtensionContext }->protectedExtension().get());
}

- (WKWebExtensionController *)webExtensionController
{
    return wrapper(Ref { *_webExtensionContext }->protectedExtensionController().get());
}

- (BOOL)isLoaded
{
    return Ref { *_webExtensionContext }->isLoaded();
}

-(NSArray<NSError *> *)errors
{
    return Ref { *_webExtensionContext }->errors();
}

- (NSURL *)baseURL
{
    return Ref { *_webExtensionContext }->baseURL();
}

- (void)setBaseURL:(NSURL *)baseURL
{
    NSParameterAssert([baseURL isKindOfClass:NSURL.class]);
    NSAssert1(WTF::URLParser::maybeCanonicalizeScheme(String(baseURL.scheme)), @"Invalid parameter: '%@' is not a valid URL scheme", baseURL.scheme);
    NSAssert1(![WKWebView handlesURLScheme:baseURL.scheme], @"Invalid parameter: '%@' is a URL scheme that WKWebView handles natively and cannot be used for extensions", baseURL.scheme);
    NSAssert1(WebKit::WebExtensionMatchPattern::extensionSchemes().contains(baseURL.scheme), @"Invalid parameter: '%@' is not a registered custom scheme with WKWebExtensionMatchPattern", baseURL.scheme);
    NSAssert(!baseURL.path.length || [baseURL.path isEqualToString:@"/"], @"Invalid parameter: a URL with a path cannot be used");

    Ref { *_webExtensionContext }->setBaseURL(baseURL);
}

- (NSString *)uniqueIdentifier
{
    return Ref { *_webExtensionContext }->uniqueIdentifier();
}

- (void)setUniqueIdentifier:(NSString *)uniqueIdentifier
{
    NSParameterAssert([uniqueIdentifier isKindOfClass:NSString.class]);

    Ref { *_webExtensionContext }->setUniqueIdentifier(uniqueIdentifier);
}

- (BOOL)isInspectable
{
    return Ref { *_webExtensionContext }->isInspectable();
}

- (void)setInspectable:(BOOL)inspectable
{
    Ref { *_webExtensionContext }->setInspectable(inspectable);
}

- (NSString *)inspectionName
{
    return Ref { *_webExtensionContext }->backgroundWebViewInspectionName();
}

- (void)setInspectionName:(NSString *)name
{
    Ref { *_webExtensionContext }->setBackgroundWebViewInspectionName(name);
}

- (NSSet<NSString *> *)unsupportedAPIs
{
    return WebKit::toAPI(Ref { *_webExtensionContext }->unsupportedAPIs());
}

- (void)setUnsupportedAPIs:(NSSet<NSString *> *)unsupportedAPIs
{
    NSParameterAssert(!unsupportedAPIs || [unsupportedAPIs isKindOfClass:NSSet.class]);

    Ref { *_webExtensionContext }->setUnsupportedAPIs(WebKit::toImpl(unsupportedAPIs));
}

- (WKWebViewConfiguration *)webViewConfiguration
{
    return Ref { *_webExtensionContext }->webViewConfiguration(WebKit::WebExtensionContext::WebViewPurpose::Tab);
}

- (NSURL *)optionsPageURL
{
    return Ref { *_webExtensionContext }->optionsPageURL();
}

- (NSURL *)overrideNewTabPageURL
{
    return Ref { *_webExtensionContext }->overrideNewTabPageURL();
}

static inline WallTime toImpl(NSDate *date)
{
    NSCParameterAssert(!date || [date isKindOfClass:NSDate.class]);
    return date ? WebKit::toImpl(date) : WebKit::toImpl(NSDate.distantFuture);
}

static inline NSDictionary<WKWebExtensionPermission, NSDate *> *toAPI(const WebKit::WebExtensionContext::PermissionsMap& permissions)
{
    NSMutableDictionary<WKWebExtensionPermission, NSDate *> *result = [NSMutableDictionary dictionaryWithCapacity:permissions.size()];

    for (auto& entry : permissions)
        [result setObject:WebKit::toAPI(entry.value) forKey:entry.key];

    return [result copy];
}

static inline NSDictionary<WKWebExtensionMatchPattern *, NSDate *> *toAPI(const WebKit::WebExtensionContext::PermissionMatchPatternsMap& permissionMatchPatterns)
{
    NSMutableDictionary<WKWebExtensionMatchPattern *, NSDate *> *result = [NSMutableDictionary dictionaryWithCapacity:permissionMatchPatterns.size()];

    for (auto& entry : permissionMatchPatterns)
        [result setObject:WebKit::toAPI(entry.value) forKey:wrapper(entry.key)];

    return [result copy];
}

static inline WebKit::WebExtensionContext::PermissionsMap toImpl(NSDictionary<WKWebExtensionPermission, NSDate *> *permissions)
{
    __block WebKit::WebExtensionContext::PermissionsMap result;
    result.reserveInitialCapacity(permissions.count);

    [permissions enumerateKeysAndObjectsUsingBlock:^(WKWebExtensionPermission permission, NSDate *date, BOOL *) {
        NSCParameterAssert([permission isKindOfClass:NSString.class]);
        NSCParameterAssert([date isKindOfClass:NSDate.class]);
        result.set(permission, toImpl(date));
    }];

    return result;
}

static inline WebKit::WebExtensionContext::PermissionMatchPatternsMap toImpl(NSDictionary<WKWebExtensionMatchPattern *, NSDate *> *permissionMatchPatterns)
{
    __block WebKit::WebExtensionContext::PermissionMatchPatternsMap result;
    result.reserveInitialCapacity(permissionMatchPatterns.count);

    [permissionMatchPatterns enumerateKeysAndObjectsUsingBlock:^(WKWebExtensionMatchPattern *origin, NSDate *date, BOOL *) {
        NSCParameterAssert([origin isKindOfClass:WKWebExtensionMatchPattern.class]);
        NSCParameterAssert([date isKindOfClass:NSDate.class]);
        result.set(origin._webExtensionMatchPattern, toImpl(date));
    }];

    return result;
}

- (NSDictionary<WKWebExtensionPermission, NSDate *> *)grantedPermissions
{
    return toAPI(Ref { *_webExtensionContext }->grantedPermissions());
}

- (void)setGrantedPermissions:(NSDictionary<WKWebExtensionPermission, NSDate *> *)grantedPermissions
{
    NSParameterAssert([grantedPermissions isKindOfClass:NSDictionary.class]);

    Ref { *_webExtensionContext }->setGrantedPermissions(toImpl(grantedPermissions));
}

- (NSDictionary<WKWebExtensionMatchPattern *, NSDate *> *)grantedPermissionMatchPatterns
{
    return toAPI(Ref { *_webExtensionContext }->grantedPermissionMatchPatterns());
}

- (void)setGrantedPermissionMatchPatterns:(NSDictionary<WKWebExtensionMatchPattern *, NSDate *> *)grantedPermissionMatchPatterns
{
    NSParameterAssert([grantedPermissionMatchPatterns isKindOfClass:NSDictionary.class]);

    Ref { *_webExtensionContext }->setGrantedPermissionMatchPatterns(toImpl(grantedPermissionMatchPatterns));
}

- (NSDictionary<WKWebExtensionPermission, NSDate *> *)deniedPermissions
{
    return toAPI(Ref { *_webExtensionContext }->deniedPermissions());
}

- (void)setDeniedPermissions:(NSDictionary<WKWebExtensionPermission, NSDate *> *)deniedPermissions
{
    NSParameterAssert([deniedPermissions isKindOfClass:NSDictionary.class]);

    Ref { *_webExtensionContext }->setDeniedPermissions(toImpl(deniedPermissions));
}

- (NSDictionary<WKWebExtensionMatchPattern *, NSDate *> *)deniedPermissionMatchPatterns
{
    return toAPI(Ref { *_webExtensionContext }->deniedPermissionMatchPatterns());
}

- (void)setDeniedPermissionMatchPatterns:(NSDictionary<WKWebExtensionMatchPattern *, NSDate *> *)deniedPermissionMatchPatterns
{
    NSParameterAssert([deniedPermissionMatchPatterns isKindOfClass:NSDictionary.class]);

    Ref { *_webExtensionContext }->setDeniedPermissionMatchPatterns(toImpl(deniedPermissionMatchPatterns));
}

- (BOOL)hasRequestedOptionalAccessToAllHosts
{
    return Ref { *_webExtensionContext }->requestedOptionalAccessToAllHosts();
}

- (void)setHasRequestedOptionalAccessToAllHosts:(BOOL)requested
{
    return Ref { *_webExtensionContext }->setRequestedOptionalAccessToAllHosts(requested);
}

- (BOOL)hasAccessToPrivateData
{
    return Ref { *_webExtensionContext }->hasAccessToPrivateData();
}

- (void)setHasAccessToPrivateData:(BOOL)hasAccess
{
    return Ref { *_webExtensionContext }->setHasAccessToPrivateData(hasAccess);
}

static inline NSSet<WKWebExtensionPermission> *toAPI(const WebKit::WebExtensionContext::PermissionsMap::KeysConstIteratorRange& permissions)
{
    if (!permissions.size())
        return [NSSet set];

    NSMutableSet<WKWebExtensionPermission> *result = [NSMutableSet setWithCapacity:permissions.size()];

    for (auto& permission : permissions)
        [result addObject:permission];

    return [result copy];
}

static inline NSSet<WKWebExtensionMatchPattern *> *toAPI(const WebKit::WebExtensionContext::PermissionMatchPatternsMap::KeysConstIteratorRange& permissionMatchPatterns)
{
    if (!permissionMatchPatterns.size())
        return [NSSet set];

    NSMutableSet<WKWebExtensionMatchPattern *> *result = [NSMutableSet setWithCapacity:permissionMatchPatterns.size()];

    for (auto& origin : permissionMatchPatterns)
        [result addObject:wrapper(origin)];

    return [result copy];
}

- (NSSet<WKWebExtensionPermission> *)currentPermissions
{
    return toAPI(Ref { *_webExtensionContext }->currentPermissions());
}

- (NSSet<WKWebExtensionMatchPattern *> *)currentPermissionMatchPatterns
{
    return toAPI(Ref { *_webExtensionContext }->currentPermissionMatchPatterns());
}

- (BOOL)hasPermission:(WKWebExtensionPermission)permission
{
    NSParameterAssert([permission isKindOfClass:NSString.class]);

    return [self hasPermission:permission inTab:nil];
}

- (BOOL)hasPermission:(WKWebExtensionPermission)permission inTab:(id<WKWebExtensionTab>)tab
{
    NSParameterAssert([permission isKindOfClass:NSString.class]);

    Ref extensionContext { *_webExtensionContext };
    return extensionContext->hasPermission(permission, toImplNullable(tab, extensionContext.get()).get());
}

- (BOOL)hasAccessToURL:(NSURL *)url
{
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    return [self hasAccessToURL:url inTab:nil];
}

- (BOOL)hasAccessToURL:(NSURL *)url inTab:(id<WKWebExtensionTab>)tab
{
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    Ref extensionContext { *_webExtensionContext };
    return extensionContext->hasPermission(url, toImplNullable(tab, extensionContext.get()).get());
}

static inline WKWebExtensionContextPermissionStatus toAPI(WebKit::WebExtensionContext::PermissionState status)
{
    switch (status) {
    case WebKit::WebExtensionContext::PermissionState::DeniedExplicitly:
        return WKWebExtensionContextPermissionStatusDeniedExplicitly;
    case WebKit::WebExtensionContext::PermissionState::DeniedImplicitly:
        return WKWebExtensionContextPermissionStatusDeniedImplicitly;
    case WebKit::WebExtensionContext::PermissionState::RequestedImplicitly:
        return WKWebExtensionContextPermissionStatusRequestedImplicitly;
    case WebKit::WebExtensionContext::PermissionState::Unknown:
        return WKWebExtensionContextPermissionStatusUnknown;
    case WebKit::WebExtensionContext::PermissionState::RequestedExplicitly:
        return WKWebExtensionContextPermissionStatusRequestedExplicitly;
    case WebKit::WebExtensionContext::PermissionState::GrantedImplicitly:
        return WKWebExtensionContextPermissionStatusGrantedImplicitly;
    case WebKit::WebExtensionContext::PermissionState::GrantedExplicitly:
        return WKWebExtensionContextPermissionStatusGrantedExplicitly;
    }
}

static inline WebKit::WebExtensionContext::PermissionState toImpl(WKWebExtensionContextPermissionStatus status)
{
    switch (status) {
    case WKWebExtensionContextPermissionStatusDeniedExplicitly:
        return WebKit::WebExtensionContext::PermissionState::DeniedExplicitly;
    case WKWebExtensionContextPermissionStatusDeniedImplicitly:
        return WebKit::WebExtensionContext::PermissionState::DeniedImplicitly;
    case WKWebExtensionContextPermissionStatusRequestedImplicitly:
        return WebKit::WebExtensionContext::PermissionState::RequestedImplicitly;
    case WKWebExtensionContextPermissionStatusUnknown:
        return WebKit::WebExtensionContext::PermissionState::Unknown;
    case WKWebExtensionContextPermissionStatusRequestedExplicitly:
        return WebKit::WebExtensionContext::PermissionState::RequestedExplicitly;
    case WKWebExtensionContextPermissionStatusGrantedImplicitly:
        return WebKit::WebExtensionContext::PermissionState::GrantedImplicitly;
    case WKWebExtensionContextPermissionStatusGrantedExplicitly:
        return WebKit::WebExtensionContext::PermissionState::GrantedExplicitly;
    }

    ASSERT_NOT_REACHED();
    return WebKit::WebExtensionContext::PermissionState::Unknown;
}

- (WKWebExtensionContextPermissionStatus)permissionStatusForPermission:(WKWebExtensionPermission)permission
{
    NSParameterAssert([permission isKindOfClass:NSString.class]);

    return [self permissionStatusForPermission:permission inTab:nil];
}

- (WKWebExtensionContextPermissionStatus)permissionStatusForPermission:(WKWebExtensionPermission)permission inTab:(id<WKWebExtensionTab>)tab
{
    NSParameterAssert([permission isKindOfClass:NSString.class]);

    Ref extensionContext { *_webExtensionContext };
    return toAPI(extensionContext->permissionState(permission, toImplNullable(tab, extensionContext.get()).get()));
}

- (void)setPermissionStatus:(WKWebExtensionContextPermissionStatus)status forPermission:(WKWebExtensionPermission)permission
{
    NSParameterAssert(status == WKWebExtensionContextPermissionStatusDeniedExplicitly || status == WKWebExtensionContextPermissionStatusUnknown || status == WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert([permission isKindOfClass:NSString.class]);

    [self setPermissionStatus:status forPermission:permission expirationDate:nil];
}

- (void)setPermissionStatus:(WKWebExtensionContextPermissionStatus)status forPermission:(WKWebExtensionPermission)permission expirationDate:(NSDate *)expirationDate
{
    NSParameterAssert(status == WKWebExtensionContextPermissionStatusDeniedExplicitly || status == WKWebExtensionContextPermissionStatusUnknown || status == WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert([permission isKindOfClass:NSString.class]);

    Ref { *_webExtensionContext }->setPermissionState(toImpl(status), permission, toImpl(expirationDate));
}

- (WKWebExtensionContextPermissionStatus)permissionStatusForURL:(NSURL *)url
{
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    return [self permissionStatusForURL:url inTab:nil];
}

- (WKWebExtensionContextPermissionStatus)permissionStatusForURL:(NSURL *)url inTab:(id<WKWebExtensionTab>)tab
{
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    Ref extensionContext { *_webExtensionContext };
    return toAPI(extensionContext->permissionState(url, toImplNullable(tab, extensionContext.get()).get()));
}

- (void)setPermissionStatus:(WKWebExtensionContextPermissionStatus)status forURL:(NSURL *)url
{
    NSParameterAssert(status == WKWebExtensionContextPermissionStatusDeniedExplicitly || status == WKWebExtensionContextPermissionStatusUnknown || status == WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    [self setPermissionStatus:status forURL:url expirationDate:nil];
}

- (void)setPermissionStatus:(WKWebExtensionContextPermissionStatus)status forURL:(NSURL *)url expirationDate:(NSDate *)expirationDate
{
    NSParameterAssert(status == WKWebExtensionContextPermissionStatusDeniedExplicitly || status == WKWebExtensionContextPermissionStatusUnknown || status == WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    Ref { *_webExtensionContext }->setPermissionState(toImpl(status), url, toImpl(expirationDate));
}

- (WKWebExtensionContextPermissionStatus)permissionStatusForMatchPattern:(WKWebExtensionMatchPattern *)pattern
{
    NSParameterAssert([pattern isKindOfClass:WKWebExtensionMatchPattern.class]);

    return [self permissionStatusForMatchPattern:pattern inTab:nil];
}

- (WKWebExtensionContextPermissionStatus)permissionStatusForMatchPattern:(WKWebExtensionMatchPattern *)pattern inTab:(id<WKWebExtensionTab>)tab
{
    NSParameterAssert([pattern isKindOfClass:WKWebExtensionMatchPattern.class]);

    Ref extensionContext { *_webExtensionContext };
    return toAPI(extensionContext->permissionState(pattern._protectedWebExtensionMatchPattern, toImplNullable(tab, extensionContext.get()).get()));
}

- (void)setPermissionStatus:(WKWebExtensionContextPermissionStatus)status forMatchPattern:(WKWebExtensionMatchPattern *)pattern
{
    NSParameterAssert(status == WKWebExtensionContextPermissionStatusDeniedExplicitly || status == WKWebExtensionContextPermissionStatusUnknown || status == WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert([pattern isKindOfClass:WKWebExtensionMatchPattern.class]);

    [self setPermissionStatus:status forMatchPattern:pattern expirationDate:nil];
}

- (void)setPermissionStatus:(WKWebExtensionContextPermissionStatus)status forMatchPattern:(WKWebExtensionMatchPattern *)pattern expirationDate:(NSDate *)expirationDate
{
    NSParameterAssert(status == WKWebExtensionContextPermissionStatusDeniedExplicitly || status == WKWebExtensionContextPermissionStatusUnknown || status == WKWebExtensionContextPermissionStatusGrantedExplicitly);
    NSParameterAssert([pattern isKindOfClass:WKWebExtensionMatchPattern.class]);

    Ref { *_webExtensionContext }->setPermissionState(toImpl(status), pattern._protectedWebExtensionMatchPattern, toImpl(expirationDate));
}

- (BOOL)hasAccessToAllURLs
{
    return Ref { *_webExtensionContext }->hasAccessToAllURLs();
}

- (BOOL)hasAccessToAllHosts
{
    return Ref { *_webExtensionContext }->hasAccessToAllHosts();
}

- (BOOL)hasInjectedContent
{
    return Ref { *_webExtensionContext }->hasInjectedContent();
}

- (BOOL)hasInjectedContentForURL:(NSURL *)url
{
    NSParameterAssert([url isKindOfClass:NSURL.class]);

    return Ref { *_webExtensionContext }->hasInjectedContentForURL(url);
}

- (BOOL)hasContentModificationRules
{
    return Ref { *_webExtensionContext }->hasContentModificationRules();
}

- (void)loadBackgroundContentWithCompletionHandler:(void (^)(NSError *error))completionHandler
{
    Ref { *_webExtensionContext }->loadBackgroundContent(makeBlockPtr(completionHandler));
}

- (WKWebExtensionAction *)actionForTab:(id<WKWebExtensionTab>)tab
{
    Ref extensionContext { *_webExtensionContext };
    return extensionContext->getOrCreateAction(toImplNullable(tab, extensionContext.get()).get())->wrapper();
}

- (void)performActionForTab:(id<WKWebExtensionTab>)tab
{
    Ref extensionContext { *_webExtensionContext };
    extensionContext->performAction(toImplNullable(tab, extensionContext.get()).get(), WebKit::WebExtensionContext::UserTriggered::Yes);
}

- (NSArray<WKWebExtensionCommand *> *)commands
{
    return createNSArray(Ref { *_webExtensionContext }->commands(), [](auto& command) {
        return command->wrapper();
    }).get();
}

- (void)performCommand:(WKWebExtensionCommand *)command
{
    NSParameterAssert([command isKindOfClass:WKWebExtensionCommand.class]);

    Ref { *_webExtensionContext }->performCommand([command _protectedWebExtensionCommand].get(), WebKit::WebExtensionContext::UserTriggered::Yes);
}

#if TARGET_OS_IPHONE
- (BOOL)performCommandForKeyCommand:(UIKeyCommand *)keyCommand
{
    NSParameterAssert([keyCommand isKindOfClass:UIKeyCommand.class]);

    return Ref { *_webExtensionContext }->performCommand(keyCommand);
}
#endif

#if USE(APPKIT)
- (BOOL)performCommandForEvent:(NSEvent *)event
{
    NSParameterAssert([event isKindOfClass:NSEvent.class]);

    return Ref { *_webExtensionContext }->performCommand(event);
}

- (WKWebExtensionCommand *)commandForEvent:(NSEvent *)event
{
    NSParameterAssert([event isKindOfClass:NSEvent.class]);

    if (RefPtr result = Ref { *_webExtensionContext }->command(event))
        return result->wrapper();
    return nil;
}
#endif // USE(APPKIT)

- (NSArray<CocoaMenuItem *> *)menuItemsForTab:(id<WKWebExtensionTab>)tab
{
    NSParameterAssert(tab != nil);

    Ref extensionContext { *_webExtensionContext };
    return extensionContext->platformMenuItems(toImpl(tab, extensionContext.get()));
}

- (void)userGesturePerformedInTab:(id<WKWebExtensionTab>)tab
{
    NSParameterAssert(tab != nil);

    Ref extensionContext { *_webExtensionContext };
    extensionContext->userGesturePerformed(toImpl(tab, extensionContext.get()));
}

- (BOOL)hasActiveUserGestureInTab:(id<WKWebExtensionTab>)tab
{
    NSParameterAssert(tab != nil);

    Ref extensionContext { *_webExtensionContext };
    return extensionContext->hasActiveUserGesture(toImpl(tab, extensionContext.get()));
}

- (void)clearUserGestureInTab:(id<WKWebExtensionTab>)tab
{
    NSParameterAssert(tab != nil);

    Ref extensionContext { *_webExtensionContext };
    extensionContext->clearUserGesture(toImpl(tab, extensionContext.get()));
}

static inline id<WKWebExtensionWindow> toAPI(const RefPtr<WebKit::WebExtensionWindow>& window)
{
    return window ? window->delegate() : nil;
}

static inline NSArray *toAPI(const WebKit::WebExtensionContext::WindowVector& windows)
{
    if (windows.isEmpty())
        return [NSArray array];

    NSMutableArray *result = [[NSMutableArray alloc] initWithCapacity:windows.size()];

    for (Ref window : windows) {
        if (auto delegate = window->delegate())
            [result addObject:delegate];
    }

    return [result copy];
}

- (NSArray<id<WKWebExtensionWindow>> *)openWindows
{
    return toAPI(Ref { *_webExtensionContext }->openWindows(WebKit::WebExtensionContext::IgnoreExtensionAccess::Yes));
}

- (id<WKWebExtensionWindow>)focusedWindow
{
    return toAPI(Ref { *_webExtensionContext }->focusedWindow(WebKit::WebExtensionContext::IgnoreExtensionAccess::Yes));
}

static inline NSSet *toAPI(const WebKit::WebExtensionContext::TabVector& tabs)
{
    if (tabs.isEmpty())
        return [NSSet set];

    NSMutableSet *result = [[NSMutableSet alloc] initWithCapacity:tabs.size()];

    for (Ref tab : tabs) {
        if (auto delegate = tab->delegate())
            [result addObject:delegate];
    }

    return [result copy];
}

- (NSSet<id<WKWebExtensionTab>> *)openTabs
{
    return toAPI(Ref { *_webExtensionContext }->openTabs(WebKit::WebExtensionContext::IgnoreExtensionAccess::Yes));
}

static inline Ref<WebKit::WebExtensionWindow> toImpl(id<WKWebExtensionWindow> window, WebKit::WebExtensionContext& context)
{
    return context.getOrCreateWindow(window);
}

- (void)didOpenWindow:(id<WKWebExtensionWindow>)newWindow
{
    NSParameterAssert(newWindow != nil);

    Ref extensionContext { *_webExtensionContext };
    extensionContext->didOpenWindow(toImpl(newWindow, extensionContext.get()));
}

- (void)didCloseWindow:(id<WKWebExtensionWindow>)closedWindow
{
    NSParameterAssert(closedWindow != nil);

    Ref extensionContext { *_webExtensionContext };
    extensionContext->didCloseWindow(toImpl(closedWindow, extensionContext.get()));
}

- (void)didFocusWindow:(id<WKWebExtensionWindow>)focusedWindow
{
    Ref extensionContext { *_webExtensionContext };
    extensionContext->didFocusWindow(focusedWindow ? toImpl(focusedWindow, extensionContext.get()).ptr() : nullptr);
}

static inline Ref<WebKit::WebExtensionTab> toImpl(id<WKWebExtensionTab> tab, WebKit::WebExtensionContext& context)
{
    return context.getOrCreateTab(tab);
}

static inline RefPtr<WebKit::WebExtensionTab> toImplNullable(id<WKWebExtensionTab> tab, WebKit::WebExtensionContext& context)
{
    return tab ? toImpl(tab, context).ptr() : nullptr;
}

static inline WebKit::WebExtensionContext::TabSet toImpl(NSArray<id<WKWebExtensionTab>> *tabs, WebKit::WebExtensionContext& context)
{
    WebKit::WebExtensionContext::TabSet result;
    result.reserveInitialCapacity(tabs.count);

    for (id tab in tabs)
        result.addVoid(context.getOrCreateTab(tab));

    return result;
}

- (void)didOpenTab:(id<WKWebExtensionTab>)newTab
{
    NSParameterAssert(newTab != nil);

    Ref extensionContext { *_webExtensionContext };
    extensionContext->didOpenTab(toImpl(newTab, extensionContext.get()));
}

- (void)didCloseTab:(id<WKWebExtensionTab>)closedTab windowIsClosing:(BOOL)windowIsClosing
{
    NSParameterAssert(closedTab != nil);

    Ref extensionContext { *_webExtensionContext };
    extensionContext->didCloseTab(toImpl(closedTab, extensionContext.get()), windowIsClosing ? WebKit::WebExtensionContext::WindowIsClosing::Yes : WebKit::WebExtensionContext::WindowIsClosing::No);
}

- (void)didActivateTab:(id<WKWebExtensionTab>)activatedTab previousActiveTab:(id<WKWebExtensionTab>)previousTab
{
    NSParameterAssert(activatedTab != nil);

    Ref extensionContext { *_webExtensionContext };
    extensionContext->didActivateTab(toImpl(activatedTab, extensionContext.get()), toImplNullable(previousTab, extensionContext.get()).get());
}

- (void)didSelectTabs:(NSArray<id<WKWebExtensionTab>> *)selectedTabs
{
    NSParameterAssert([selectedTabs isKindOfClass:NSArray.class]);

    Ref extensionContext { *_webExtensionContext };
    extensionContext->didSelectOrDeselectTabs(toImpl(selectedTabs, extensionContext.get()));
}

- (void)didDeselectTabs:(NSArray<id<WKWebExtensionTab>> *)deselectedTabs
{
    NSParameterAssert([deselectedTabs isKindOfClass:NSArray.class]);

    Ref extensionContext { *_webExtensionContext };
    extensionContext->didSelectOrDeselectTabs(toImpl(deselectedTabs, extensionContext.get()));
}

- (void)didMoveTab:(id<WKWebExtensionTab>)movedTab fromIndex:(NSUInteger)index inWindow:(id<WKWebExtensionWindow>)oldWindow
{
    NSParameterAssert(movedTab != nil);

    Ref extensionContext { *_webExtensionContext };
    extensionContext->didMoveTab(toImpl(movedTab, extensionContext.get()), index != NSNotFound ? index : notFound, oldWindow ? toImpl(oldWindow, extensionContext.get()).ptr() : nullptr);
}

- (void)didReplaceTab:(id<WKWebExtensionTab>)oldTab withTab:(id<WKWebExtensionTab>)newTab
{
    NSParameterAssert(oldTab != nil);
    NSParameterAssert(newTab != nil);

    Ref extensionContext { *_webExtensionContext };
    extensionContext->didReplaceTab(toImpl(oldTab, extensionContext.get()), toImpl(newTab, extensionContext.get()));
}

static inline OptionSet<WebKit::WebExtensionTab::ChangedProperties> toImpl(WKWebExtensionTabChangedProperties properties)
{
    if (properties == WKWebExtensionTabChangedPropertiesNone)
        return { };

    OptionSet<WebKit::WebExtensionTab::ChangedProperties> result;

    if (properties & WKWebExtensionTabChangedPropertiesLoading)
        result.add(WebKit::WebExtensionTab::ChangedProperties::Loading);

    if (properties & WKWebExtensionTabChangedPropertiesMuted)
        result.add(WebKit::WebExtensionTab::ChangedProperties::Muted);

    if (properties & WKWebExtensionTabChangedPropertiesPinned)
        result.add(WebKit::WebExtensionTab::ChangedProperties::Pinned);

    if (properties & WKWebExtensionTabChangedPropertiesPlayingAudio)
        result.add(WebKit::WebExtensionTab::ChangedProperties::PlayingAudio);

    if (properties & WKWebExtensionTabChangedPropertiesReaderMode)
        result.add(WebKit::WebExtensionTab::ChangedProperties::ReaderMode);

    if (properties & WKWebExtensionTabChangedPropertiesSize)
        result.add(WebKit::WebExtensionTab::ChangedProperties::Size);

    if (properties & WKWebExtensionTabChangedPropertiesTitle)
        result.add(WebKit::WebExtensionTab::ChangedProperties::Title);

    if (properties & WKWebExtensionTabChangedPropertiesURL)
        result.add(WebKit::WebExtensionTab::ChangedProperties::URL);

    if (properties & WKWebExtensionTabChangedPropertiesZoomFactor)
        result.add(WebKit::WebExtensionTab::ChangedProperties::ZoomFactor);

    return result;
}

- (void)didChangeTabProperties:(WKWebExtensionTabChangedProperties)properties forTab:(id<WKWebExtensionTab>)changedTab
{
    NSParameterAssert(changedTab != nil);

    Ref extensionContext { *_webExtensionContext };
    extensionContext->didChangeTabProperties(toImpl(changedTab, extensionContext.get()), toImpl(properties));
}

- (WKWebView *)_backgroundWebView
{
    return self._protectedWebExtensionContext->backgroundWebView();
}

- (NSURL *)_backgroundContentURL
{
    return self._protectedWebExtensionContext->backgroundContentURL();
}

- (void)_sendTestMessage:(NSString *)message withArgument:(id)argument
{
    NSParameterAssert([message isKindOfClass:NSString.class]);

    self._protectedWebExtensionContext->sendTestMessage(message, argument);
}

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
- (nullable _WKWebExtensionSidebar *)sidebarForTab:(id<WKWebExtensionTab>)tab
{
    Ref extensionContext { *_webExtensionContext };
    if (RefPtr maybeSidebar = extensionContext->getOrCreateSidebar(toImplNullable(tab, extensionContext.get())))
        return maybeSidebar->wrapper();
    return nil;
}
#else
- (nullable _WKWebExtensionSidebar *)sidebarForTab:(id<WKWebExtensionTab>)tab
{
    return nil;
}
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_webExtensionContext;
}

- (WebKit::WebExtensionContext&)_webExtensionContext
{
    return *_webExtensionContext;
}

- (Ref<WebKit::WebExtensionContext>)_protectedWebExtensionContext
{
    return *_webExtensionContext;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

+ (instancetype)contextForExtension:(WKWebExtension *)extension
{
    return nil;
}

- (instancetype)initForExtension:(WKWebExtension *)extension
{
    return nil;
}

- (WKWebExtension *)webExtension
{
    return nil;
}

- (WKWebExtensionController *)webExtensionController
{
    return nil;
}

- (BOOL)isLoaded
{
    return NO;
}

-(NSArray<NSError *> *)errors
{
    return nil;
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

- (NSString *)inspectionName
{
    return nil;
}

- (void)setInspectionName:(NSString *)name
{
}

- (NSSet<NSString *> *)unsupportedAPIs
{
    return nil;
}

- (void)setUnsupportedAPIs:(NSSet<NSString *> *)unsupportedAPIs
{
}

- (WKWebViewConfiguration *)webViewConfiguration
{
    return nil;
}

- (NSURL *)optionsPageURL
{
    return nil;
}

- (NSURL *)overrideNewTabPageURL
{
    return nil;
}

- (NSDictionary<WKWebExtensionPermission, NSDate *> *)grantedPermissions
{
    return nil;
}

- (void)setGrantedPermissions:(NSDictionary<WKWebExtensionPermission, NSDate *> *)grantedPermissions
{
}

- (NSDictionary<WKWebExtensionMatchPattern *, NSDate *> *)grantedPermissionMatchPatterns
{
    return nil;
}

- (void)setGrantedPermissionMatchPatterns:(NSDictionary<WKWebExtensionMatchPattern *, NSDate *> *)grantedPermissionMatchPatterns
{
}

- (NSDictionary<WKWebExtensionPermission, NSDate *> *)deniedPermissions
{
    return nil;
}

- (void)setDeniedPermissions:(NSDictionary<WKWebExtensionPermission, NSDate *> *)deniedPermissions
{
}

- (NSDictionary<WKWebExtensionMatchPattern *, NSDate *> *)deniedPermissionMatchPatterns
{
    return nil;
}

- (void)setDeniedPermissionMatchPatterns:(NSDictionary<WKWebExtensionMatchPattern *, NSDate *> *)deniedPermissionMatchPatterns
{
}

- (BOOL)hasRequestedOptionalAccessToAllHosts
{
    return NO;
}

- (void)setHasRequestedOptionalAccessToAllHosts:(BOOL)requested
{
}

- (BOOL)hasAccessToPrivateData
{
    return NO;
}

- (void)setHasAccessToPrivateData:(BOOL)hasAccess
{
}

- (NSSet<WKWebExtensionPermission> *)currentPermissions
{
    return nil;
}

- (NSSet<WKWebExtensionMatchPattern *> *)currentPermissionMatchPatterns
{
    return nil;
}

- (BOOL)hasPermission:(WKWebExtensionPermission)permission
{
    return NO;
}

- (BOOL)hasPermission:(WKWebExtensionPermission)permission inTab:(id<WKWebExtensionTab>)tab
{
    return NO;
}

- (BOOL)hasAccessToURL:(NSURL *)url
{
    return NO;
}

- (BOOL)hasAccessToURL:(NSURL *)url inTab:(id<WKWebExtensionTab>)tab
{
    return NO;
}

- (WKWebExtensionContextPermissionStatus)permissionStatusForPermission:(WKWebExtensionPermission)permission
{
    return WKWebExtensionContextPermissionStatusUnknown;
}

- (WKWebExtensionContextPermissionStatus)permissionStatusForPermission:(WKWebExtensionPermission)permission inTab:(id<WKWebExtensionTab>)tab
{
    return WKWebExtensionContextPermissionStatusUnknown;
}

- (void)setPermissionStatus:(WKWebExtensionContextPermissionStatus)status forPermission:(WKWebExtensionPermission)permission
{
}

- (void)setPermissionStatus:(WKWebExtensionContextPermissionStatus)status forPermission:(WKWebExtensionPermission)permission expirationDate:(NSDate *)expirationDate
{
}

- (WKWebExtensionContextPermissionStatus)permissionStatusForURL:(NSURL *)url
{
    return WKWebExtensionContextPermissionStatusUnknown;
}

- (WKWebExtensionContextPermissionStatus)permissionStatusForURL:(NSURL *)url inTab:(id<WKWebExtensionTab>)tab
{
    return WKWebExtensionContextPermissionStatusUnknown;
}

- (void)setPermissionStatus:(WKWebExtensionContextPermissionStatus)status forURL:(NSURL *)url
{
}

- (void)setPermissionStatus:(WKWebExtensionContextPermissionStatus)status forURL:(NSURL *)url expirationDate:(NSDate *)expirationDate
{
}

- (WKWebExtensionContextPermissionStatus)permissionStatusForMatchPattern:(WKWebExtensionMatchPattern *)pattern
{
    return WKWebExtensionContextPermissionStatusUnknown;
}

- (WKWebExtensionContextPermissionStatus)permissionStatusForMatchPattern:(WKWebExtensionMatchPattern *)pattern inTab:(id<WKWebExtensionTab>)tab
{
    return WKWebExtensionContextPermissionStatusUnknown;
}

- (void)setPermissionStatus:(WKWebExtensionContextPermissionStatus)status forMatchPattern:(WKWebExtensionMatchPattern *)pattern
{
}

- (void)setPermissionStatus:(WKWebExtensionContextPermissionStatus)status forMatchPattern:(WKWebExtensionMatchPattern *)pattern expirationDate:(NSDate *)expirationDate
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

- (BOOL)hasInjectedContent
{
    return NO;
}

- (BOOL)hasInjectedContentForURL:(NSURL *)url
{
    return NO;
}

- (BOOL)hasContentModificationRules
{
    return NO;
}

- (void)loadBackgroundContentWithCompletionHandler:(void (^)(NSError *error))completionHandler
{
    completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil]);
}

- (WKWebExtensionAction *)actionForTab:(id<WKWebExtensionTab>)tab NS_SWIFT_NAME(action(for:))
{
    return nil;
}

- (void)performActionForTab:(id<WKWebExtensionTab>)tab NS_SWIFT_NAME(performAction(for:))
{
}

- (NSArray<WKWebExtensionCommand *> *)commands
{
    return nil;
}

- (void)performCommand:(WKWebExtensionCommand *)command
{
}

#if TARGET_OS_IPHONE
- (BOOL)performCommandForKeyCommand:(UIKeyCommand *)keyCommand
{
    return NO;
}
#endif

#if USE(APPKIT)
- (BOOL)performCommandForEvent:(NSEvent *)event
{
    return NO;
}

- (WKWebExtensionCommand *)commandForEvent:(NSEvent *)event
{
    return nil;
}
#endif // USE(APPKIT)

- (NSArray<CocoaMenuItem *> *)menuItemsForTab:(id<WKWebExtensionTab>)tab
{
    return nil;
}

- (void)userGesturePerformedInTab:(id<WKWebExtensionTab>)tab
{
}

- (BOOL)hasActiveUserGestureInTab:(id<WKWebExtensionTab>)tab
{
    return NO;
}

- (void)clearUserGestureInTab:(id<WKWebExtensionTab>)tab
{
}

- (NSArray<id<WKWebExtensionWindow>> *)openWindows
{
    return nil;
}

- (id<WKWebExtensionWindow>)focusedWindow
{
    return nil;
}

- (NSSet<id<WKWebExtensionTab>> *)openTabs
{
    return nil;
}

- (void)didOpenWindow:(id<WKWebExtensionWindow>)newWindow
{
}

- (void)didCloseWindow:(id<WKWebExtensionWindow>)closedWindow
{
}

- (void)didFocusWindow:(id<WKWebExtensionWindow>)focusedWindow
{
}

- (void)didOpenTab:(id<WKWebExtensionTab>)newTab
{
}

- (void)didCloseTab:(id<WKWebExtensionTab>)closedTab windowIsClosing:(BOOL)windowIsClosing
{
}

- (void)didActivateTab:(id<WKWebExtensionTab>)activatedTab previousActiveTab:(id<WKWebExtensionTab>)previousTab
{
}

- (void)didSelectTabs:(NSSet<id<WKWebExtensionTab>> *)selectedTabs
{
}

- (void)didDeselectTabs:(NSSet<id<WKWebExtensionTab>> *)deselectedTabs
{
}

- (void)didMoveTab:(id<WKWebExtensionTab>)movedTab fromIndex:(NSUInteger)index inWindow:(id<WKWebExtensionWindow>)oldWindow
{
}

- (void)didReplaceTab:(id<WKWebExtensionTab>)oldTab withTab:(id<WKWebExtensionTab>)newTab
{
}

- (void)didChangeTabProperties:(WKWebExtensionTabChangedProperties)properties forTab:(id<WKWebExtensionTab>)changedTab
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

- (void)_sendTestMessage:(NSString *)message withArgument:(id)argument
{
}

- (nullable _WKWebExtensionSidebar *)sidebarForTab:(id<WKWebExtensionTab>)tab
{
    return nil;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
