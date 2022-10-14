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

#import <WebKit/WKFoundation.h>

#import <Foundation/Foundation.h>

#import <WebKit/_WKWebExtensionMatchPattern.h>
#import <WebKit/_WKWebExtensionPermission.h>

@class _WKWebExtension;
@class _WKWebExtensionController;
@protocol _WKWebExtensionTab;

NS_ASSUME_NONNULL_BEGIN

WK_EXTERN NSErrorDomain const _WKWebExtensionContextErrorDomain WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

typedef NS_ERROR_ENUM(_WKWebExtensionContextErrorDomain, _WKWebExtensionContextError) {
    _WKWebExtensionContextErrorUnknown = 1,
    _WKWebExtensionContextErrorAlreadyLoaded,
    _WKWebExtensionContextErrorNotLoaded,
    _WKWebExtensionContextErrorBaseURLTaken,
} NS_SWIFT_NAME(_WKWebExtensionContext.Error) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

typedef NS_ENUM(NSInteger, _WKWebExtensionContextPermissionState) {
    _WKWebExtensionContextPermissionStateDeniedExplicitly    = -3,
    _WKWebExtensionContextPermissionStateDeniedImplicitly    = -2,
    _WKWebExtensionContextPermissionStateRequestedImplicitly = -1,
    _WKWebExtensionContextPermissionStateUnknown             = 0,
    _WKWebExtensionContextPermissionStateRequestedExplicitly = 1,
    _WKWebExtensionContextPermissionStateGrantedImplicitly   = 2,
    _WKWebExtensionContextPermissionStateGrantedExplicitly   = 3,
} NS_SWIFT_NAME(_WKWebExtensionContext.PermissionState) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextPermissionsWereGrantedNotification NS_SWIFT_NAME(_WKWebExtensionContext.permissionsWereGrantedNotification);

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextPermissionsWereDeniedNotification NS_SWIFT_NAME(_WKWebExtensionContext.permissionsWereDeniedNotification);

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextGrantedPermissionsWereRemovedNotification NS_SWIFT_NAME(_WKWebExtensionContext.grantedPermissionsWereRemovedNotification);

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextDeniedPermissionsWereRemovedNotification NS_SWIFT_NAME(_WKWebExtensionContext.deniedPermissionsWereRemovedNotification);

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextPermissionMatchPatternsWereGrantedNotification NS_SWIFT_NAME(_WKWebExtensionContext.permissionMatchPatternsWereGrantedNotification);

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextPermissionMatchPatternsWereDeniedNotification NS_SWIFT_NAME(_WKWebExtensionContext.permissionMatchPatternsWereDeniedNotification);

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification NS_SWIFT_NAME(_WKWebExtensionContext.grantedPermissionMatchPatternsWereRemovedNotification);

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextDeniedPermissionMatchPatternsWereRemovedNotification NS_SWIFT_NAME(_WKWebExtensionContext.deniedPermissionMatchPatternsWereRemovedNotification);

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
typedef NSString * _WKWebExtensionContextNotificationUserInfoKey NS_TYPED_EXTENSIBLE_ENUM NS_SWIFT_NAME(_WKWebExtensionContext.NotificationUserInfoKey);

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionContextNotificationUserInfoKey const _WKWebExtensionContextNotificationUserInfoKeyPermissions;

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionContextNotificationUserInfoKey const _WKWebExtensionContextNotificationUserInfoKeyMatchPatterns;

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKWebExtensionContext : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

+ (instancetype)contextWithExtension:(_WKWebExtension *)extension;

- (instancetype)initWithExtension:(_WKWebExtension *)extension NS_DESIGNATED_INITIALIZER;

@property (nonatomic, readonly, strong) _WKWebExtension *extension;
@property (nonatomic, readonly, weak) _WKWebExtensionController *extensionController;

@property (nonatomic, readonly, getter=isLoaded) BOOL loaded;

@property (nonatomic, copy) NSURL *baseURL;
@property (nonatomic, copy) NSString *uniqueIdentifier;

// Granted permissions with their expiration date. Never includes expired entries at time of access.
@property (nonatomic, copy) NSDictionary<_WKWebExtensionPermission, NSDate *> *grantedPermissions;
@property (nonatomic, copy) NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *grantedPermissionMatchPatterns;

// Denied permissions with their expiration date. Never includes expired entries at time of access.
@property (nonatomic, copy) NSDictionary<_WKWebExtensionPermission, NSDate *> *deniedPermissions;
@property (nonatomic, copy) NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *deniedPermissionMatchPatterns;

// When set to YES, this allows an extension to upgrade to asking for all URLs
// without actually granting all URLs from an optional permissions request.
@property (nonatomic) BOOL requestedOptionalAccessToAllHosts;

// All permissions currently allowed, including non-expired granted permissions.
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionPermission> *currentPermissions;
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionMatchPattern *> *currentPermissionMatchPatterns;

- (BOOL)hasPermission:(_WKWebExtensionPermission)permission;
- (BOOL)hasPermission:(_WKWebExtensionPermission)permission inTab:(nullable id <_WKWebExtensionTab>)tab;

- (BOOL)hasAccessToURL:(NSURL *)url;
- (BOOL)hasAccessToURL:(NSURL *)url inTab:(nullable id <_WKWebExtensionTab>)tab;

// Specifically checks for any <all_urls> patterns. It does not check for all hosts patterns.
// This is needed for APIs like tabs.captureVisibleTab that need to check specifically for <all_urls>.
// You likely want to use hasPermissionToAccessAllHosts instead.
@property (nonatomic, readonly) BOOL hasAccessToAllURLs;

// Checks for any <all_urls>, or all hosts patterns.
@property (nonatomic, readonly) BOOL hasAccessToAllHosts;

- (_WKWebExtensionContextPermissionState)permissionStateForPermission:(_WKWebExtensionPermission)permission;
- (_WKWebExtensionContextPermissionState)permissionStateForPermission:(_WKWebExtensionPermission)permission inTab:(nullable id <_WKWebExtensionTab>)tab;

- (void)setPermissionState:(_WKWebExtensionContextPermissionState)state forPermission:(_WKWebExtensionPermission)permission;
- (void)setPermissionState:(_WKWebExtensionContextPermissionState)state forPermission:(_WKWebExtensionPermission)permission expirationDate:(nullable NSDate *)expirationDate;

- (_WKWebExtensionContextPermissionState)permissionStateForURL:(NSURL *)url NS_SWIFT_NAME(permissionState(forURL:));
- (_WKWebExtensionContextPermissionState)permissionStateForURL:(NSURL *)url inTab:(nullable id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(permissionState(forURL:in:));

- (void)setPermissionState:(_WKWebExtensionContextPermissionState)state forURL:(NSURL *)url;
- (void)setPermissionState:(_WKWebExtensionContextPermissionState)state forURL:(NSURL *)url expirationDate:(nullable NSDate *)expirationDate;

- (_WKWebExtensionContextPermissionState)permissionStateForMatchPattern:(_WKWebExtensionMatchPattern *)pattern;
- (_WKWebExtensionContextPermissionState)permissionStateForMatchPattern:(_WKWebExtensionMatchPattern *)pattern inTab:(nullable id <_WKWebExtensionTab>)tab;

- (void)setPermissionState:(_WKWebExtensionContextPermissionState)state forMatchPattern:(_WKWebExtensionMatchPattern *)pattern;
- (void)setPermissionState:(_WKWebExtensionContextPermissionState)state forMatchPattern:(_WKWebExtensionMatchPattern *)pattern expirationDate:(nullable NSDate *)expirationDate;

- (void)userGesturePerformedInTab:(id <_WKWebExtensionTab>)tab;
- (BOOL)hasActiveUserGestureInTab:(id <_WKWebExtensionTab>)tab;
- (void)cancelUserGestureForTab:(id <_WKWebExtensionTab>)tab;

@end

NS_ASSUME_NONNULL_END
