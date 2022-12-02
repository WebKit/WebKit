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

/*! @abstract Indicates a @link WKWebExtensionContext @/link error. */
WK_EXTERN NSErrorDomain const _WKWebExtensionContextErrorDomain WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

/*!
 @abstract Constants used by NSError to indicate errors in the @link WKWebExtensionContext @/link domain.
 @constant WKWebExtensionContextErrorUnknown  Indicates that an unknown error occurred.
 @constant WKWebExtensionContextErrorAlreadyLoaded  Indicates that the context is already loaded by a @link WKWebExtensionController @/link.
 @constant WKWebExtensionContextErrorNotLoaded  Indicates that the context is not loaded by a @link WKWebExtensionController @/link.
 @constant WKWebExtensionContextErrorBaseURLTaken  Indicates that another context is already using the specified base URL.
 */
typedef NS_ERROR_ENUM(_WKWebExtensionContextErrorDomain, _WKWebExtensionContextError) {
    _WKWebExtensionContextErrorUnknown = 1,
    _WKWebExtensionContextErrorAlreadyLoaded,
    _WKWebExtensionContextErrorNotLoaded,
    _WKWebExtensionContextErrorBaseURLTaken,
} NS_SWIFT_NAME(_WKWebExtensionContext.Error) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

/*!
 @abstract Constants used to indicate permission state in @link WKWebExtensionContext @/link.
 @constant WKWebExtensionContextPermissionStateDeniedExplicitly  Indicates that the permission was explicitly denied.
 @constant WKWebExtensionContextPermissionStateDeniedImplicitly  Indicates that the permission was implicitly denied because of another explicitly denied permission.
 @constant WKWebExtensionContextPermissionStateRequestedImplicitly  Indicates that the permission was implicitly requested because of another explicitly requested permission.
 @constant WKWebExtensionContextPermissionStateUnknown  Indicates that an unknown permission state.
 @constant WKWebExtensionContextPermissionStateRequestedExplicitly  Indicates that the permission was explicitly requested.
 @constant WKWebExtensionContextPermissionStateGrantedImplicitly  Indicates that the permission was implicitly granted because of another explicitly granted permission.
 @constant WKWebExtensionContextPermissionStateGrantedExplicitly  Indicates that the permission was explicitly granted permission.
 */
typedef NS_ENUM(NSInteger, _WKWebExtensionContextPermissionState) {
    _WKWebExtensionContextPermissionStateDeniedExplicitly    = -3,
    _WKWebExtensionContextPermissionStateDeniedImplicitly    = -2,
    _WKWebExtensionContextPermissionStateRequestedImplicitly = -1,
    _WKWebExtensionContextPermissionStateUnknown             =  0,
    _WKWebExtensionContextPermissionStateRequestedExplicitly =  1,
    _WKWebExtensionContextPermissionStateGrantedImplicitly   =  2,
    _WKWebExtensionContextPermissionStateGrantedExplicitly   =  3,
} NS_SWIFT_NAME(_WKWebExtensionContext.PermissionState) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly granted permissions. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextPermissionsWereGrantedNotification NS_SWIFT_NAME(_WKWebExtensionContext.permissionsWereGrantedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly denied permissions. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextPermissionsWereDeniedNotification NS_SWIFT_NAME(_WKWebExtensionContext.permissionsWereDeniedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly removed granted permissions. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextGrantedPermissionsWereRemovedNotification NS_SWIFT_NAME(_WKWebExtensionContext.grantedPermissionsWereRemovedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly removed denied permissions. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextDeniedPermissionsWereRemovedNotification NS_SWIFT_NAME(_WKWebExtensionContext.deniedPermissionsWereRemovedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly granted permission match patterns. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextPermissionMatchPatternsWereGrantedNotification NS_SWIFT_NAME(_WKWebExtensionContext.permissionMatchPatternsWereGrantedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly denied permission match patterns. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextPermissionMatchPatternsWereDeniedNotification NS_SWIFT_NAME(_WKWebExtensionContext.permissionMatchPatternsWereDeniedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly removed granted permission match patterns. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification NS_SWIFT_NAME(_WKWebExtensionContext.grantedPermissionMatchPatternsWereRemovedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly removed denied permission match patterns. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionContextDeniedPermissionMatchPatternsWereRemovedNotification NS_SWIFT_NAME(_WKWebExtensionContext.deniedPermissionMatchPatternsWereRemovedNotification);

/*! @abstract Constants for specifying @link WKWebExtensionContext @/link information in notifications. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
typedef NSString * _WKWebExtensionContextNotificationUserInfoKey NS_TYPED_EXTENSIBLE_ENUM NS_SWIFT_NAME(_WKWebExtensionContext.NotificationUserInfoKey);

/*! @abstract The corresponding value represents the affected permissions in @link WKWebExtensionContext @/link notifications. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionContextNotificationUserInfoKey const _WKWebExtensionContextNotificationUserInfoKeyPermissions;

/*! @abstract The corresponding value represents the affected permission match patterns in @link WKWebExtensionContext @/link notifications. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionContextNotificationUserInfoKey const _WKWebExtensionContextNotificationUserInfoKeyMatchPatterns;

/*!
 @abstract A `WKWebExtensionContext` object encapsulates a web extension’s runtime environment.
 @discussion This class handles the access permissions, content injection, storage, and background logic for the extension.
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKWebExtensionContext : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*!
 @abstract Returns a web extension context initialized with the specified extension.
 @param extension The extension to use for the new web extension context.
 @result An initialized web extension context.
 */
+ (instancetype)contextWithExtension:(_WKWebExtension *)extension;

/*!
 @abstract Returns a web extension context initialized with a specified extension.
 @param extension The extension to use for the new web extension context.
 @result An initialized web extension context.
 @discussion This is a designated initializer.
 */
- (instancetype)initWithExtension:(_WKWebExtension *)extension NS_DESIGNATED_INITIALIZER;

/*! @abstract The extension this context represents. */
@property (nonatomic, readonly, strong) _WKWebExtension *extension;

/*! @abstract The extension controller this context is loaded in, otherwise `nil` if it isn't loaded. */
@property (nonatomic, readonly, weak) _WKWebExtensionController *extensionController;

/*! @abstract A Boolean value indicating if this context is loaded in an extension controller. */
@property (nonatomic, readonly, getter=isLoaded) BOOL loaded;

/*!
 @abstract The base URL the context uses for loading extension resources or injecting content into webpages.
 @discussion The default value is a unique URL using the `webkit-extension` scheme.
 The base URL can be set to any URL, but only the scheme and host will be used. The scheme cannot be a scheme that is
 already supported by @link WKWebView @/link (e.g. http, https, etc.) Setting is only allowed when the context is not loaded.
 */
@property (nonatomic, copy) NSURL *baseURL;

/*!
 @abstract An unique identifier used to distinguish the extension from other extensions and target it for messages.
 @discussion The default value is a unique value that matches the host in the default base URL. The identifier can be any
 value that is unique. Setting is only allowed when the context is not loaded.

 This value is accessable by the extension via `browser.runtime.id` and is used for messaging the extension as the first
 argumet of `browser.runtime.sendMessage(extensionId, message, options)`.
 */
@property (nonatomic, copy) NSString *uniqueIdentifier;

/*!
 @abstract The currently granted permissions and their expiration dates.
 @discussion Permissions that don't expire will have a distant future date. This will never include expired entries at time of access.
 Setting this property will replace all existing entries. Use this property for saving and restoring permission state in bulk.

 Permissions in this dictionary should be explicitly granted by the user before being added. Any permissions in this collection will not be
 presented for approval again until they expire.
 @seealso setPermissionState:forPermission:
 @seealso setPermissionState:forPermission:expirationDate:
 */
@property (nonatomic, copy) NSDictionary<_WKWebExtensionPermission, NSDate *> *grantedPermissions;

/*!
 @abstract The currently granted permission match patterns and their expiration dates.
 @discussion Match patterns that don't expire will have a distant future date. This will never include expired entries at time of access.
 Setting this property will replace all existing entries. Use this property for saving and restoring permission state in bulk.

 Match patterns in this dictionary should be explicitly granted by the user before being added. Any match pattern in this collection will not be
 presented for approval again until they expire.
 @seealso setPermissionState:forMatchPattern:
 @seealso setPermissionState:forMatchPattern:expirationDate:
 */
@property (nonatomic, copy) NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *grantedPermissionMatchPatterns;

/*!
 @abstract The currently denied permissions and their expiration dates.
 @discussion Permissions that don't expire will have a distant future date. This will never include expired entries at time of access.
 Setting this property will replace all existing entries. Use this property for saving and restoring permission state in bulk.

 Permissions in this dictionary should be explicitly denied by the user before being added. Any match pattern in this collection will not be
 presented for approval again until they expire.
 @seealso setPermissionState:forPermission:
 @seealso setPermissionState:forPermission:expirationDate:
 */
@property (nonatomic, copy) NSDictionary<_WKWebExtensionPermission, NSDate *> *deniedPermissions;

/*!
 @abstract The currently denied permission match patterns and their expiration dates.
 @discussion Match patterns that don't expire will have a distant future date. This will never include expired entries at time of access.
 Setting this property will replace all existing entries. Use this property for saving and restoring permission state in bulk.

 Match patterns in this dictionary should be explicitly denied by the user before being added. Any match pattern in this collection will not be
 presented for approval again until they expire.
 @seealso setPermissionState:forMatchPattern:
 @seealso setPermissionState:forMatchPattern:expirationDate:
 */
@property (nonatomic, copy) NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *deniedPermissionMatchPatterns;

/*!
 @abstract A Boolean value indicating if the extension has requested optional access to all hosts.
 @discussion When this value is `YES` the extension has asked for access to all hosts in a call to `browser.runtime.permissions.request()`
 and future permission checks will present discrete hosts for approval as being implicty requested. This value should be saved and restored as needed.
 */
@property (nonatomic) BOOL requestedOptionalAccessToAllHosts;

/*!
 @abstract The currently granted permissions that have not expired.
 @seealso grantedPermissions
 */
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionPermission> *currentPermissions;

/*!
 @abstract The currently granted permission match patterns that have not expired.
 @seealso grantedPermissionMatchPatterns
 */
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionMatchPattern *> *currentPermissionMatchPatterns;

/*!
 @abstract Checks the specified permission against the currently granted permissions.
 @seealso currentPermissions
 @seealso hasPermission:inTab:
 @seealso permissionStateForPermission:
 @seealso permissionStateForPermission:inTab:
*/
- (BOOL)hasPermission:(_WKWebExtensionPermission)permission;

/*!
 @abstract Checks the specified permission against the currently granted permissions in a specific tab.
 @discussion Permissions can be granted on a per-tab basis. When the tab is known, permission checks should always use this method.
 @seealso currentPermissions
 @seealso hasPermission:
 @seealso permissionStateForPermission:
 @seealso permissionStateForPermission:inTab:
 */
- (BOOL)hasPermission:(_WKWebExtensionPermission)permission inTab:(nullable id <_WKWebExtensionTab>)tab;

/*!
 @abstract Checks the specified URL against the currently granted permission match patterns.
 @seealso currentPermissionMatchPatterns
 @seealso hasAccessToURL:inTab:
 @seealso permissionStateForURL:
 @seealso permissionStateForURL:inTab:
 @seealso permissionStateForMatchPattern:
 @seealso permissionStateForMatchPattern:inTab:
 */
- (BOOL)hasAccessToURL:(NSURL *)url;

/*!
 @abstract Checks the specified URL against the currently granted permission match patterns in a specific tab.
 @discussion Some match patterns can be granted on a per-tab basis. When the tab is known, access checks should always use this method.
 @seealso currentPermissionMatchPatterns
 @seealso hasAccessToURL:
 @seealso permissionStateForURL:
 @seealso permissionStateForURL:inTab:
 @seealso permissionStateForMatchPattern:
 @seealso permissionStateForMatchPattern:inTab:
 */
- (BOOL)hasAccessToURL:(NSURL *)url inTab:(nullable id <_WKWebExtensionTab>)tab;

/*!
 @abstract Checks if the currently granted permission match patterns set contains the `<all_urls>` pattern.
 @discussion This does not check for any `*` host patterns. In most cases you should use the broader `hasAccessToAllHosts`.
 This check is primarily needed for APIs like `browser.tabs.captureVisibleTab()` that need to check specifically for `<all_urls>`.
 @seealso currentPermissionMatchPatterns
 @seealso hasAccessToAllHosts
 */
@property (nonatomic, readonly) BOOL hasAccessToAllURLs;

/*!
 @abstract Checks if the currently granted permission match patterns set contains the `<all_urls>` pattern or any `*` host patterns.
 @seealso currentPermissionMatchPatterns
 @seealso hasAccessToAllURLs
 */
@property (nonatomic, readonly) BOOL hasAccessToAllHosts;

/*!
 @abstract Checks if the extension has script or stylesheet content that can be injected into the specified URL.
 @param url The webpage URL to check.
 @result Returns `YES` if the extension has content that can be injected by matching the `url` against the extension's requested match patterns.
 @discussion The extension context will still need to be loaded and have granted website permissions for its content to actually be injected.
 */
- (BOOL)hasInjectedContentForURL:(NSURL *)url;

/*!
 @abstract Checks the specified permission against the currently denied, granted, and requested permissions.
 @discussion Permissions can be granted on a per-tab basis. When the tab is known, access checks should always use the method that checks in a tab.
 @seealso permissionStateForPermission:inTab:
 @seealso hasPermission:
*/
- (_WKWebExtensionContextPermissionState)permissionStateForPermission:(_WKWebExtensionPermission)permission;

/*!
 @abstract Checks the specified permission against the currently denied, granted, and requested permissions in a specific tab.
 @discussion Permissions can be granted on a per-tab basis. When the tab is known, access checks should always use this method.
 @seealso permissionStateForPermission:
 @seealso hasPermission:inTab:
*/
- (_WKWebExtensionContextPermissionState)permissionStateForPermission:(_WKWebExtensionPermission)permission inTab:(nullable id <_WKWebExtensionTab>)tab;

/*!
 @abstract Sets the state of a permission in all tabs and global contexts with a distant future expiration date.
 @discussion This method will update `grantedPermissions` and `deniedPermissions`. Use this method for changing a single permission's state.
 Only `WKWebExtensionContextPermissionStateDeniedExplicitly`, `WKWebExtensionContextPermissionStateUnknown`, and `WKWebExtensionContextPermissionStateGrantedExplicitly`
 states are allowed to be set using this method.
 @seealso setPermissionState:forPermission:expirationDate:
 @seealso setPermissionState:forPermission:inTab:
*/
- (void)setPermissionState:(_WKWebExtensionContextPermissionState)state forPermission:(_WKWebExtensionPermission)permission;

/*!
 @abstract Sets the state of a permission in all tabs and global contexts with a specific expiration date.
 @discussion This method will update `grantedPermissions` and `deniedPermissions`. Use this method for changing a single permission's state.
 Passing a `nil` expiration date will be treated as a distant future date. Only `WKWebExtensionContextPermissionStateDeniedExplicitly`, `WKWebExtensionContextPermissionStateUnknown`,
 and `WKWebExtensionContextPermissionStateGrantedExplicitly` states are allowed to be set using this method.
 @seealso setPermissionState:forPermission:
 @seealso setPermissionState:forPermission:inTab:
*/
- (void)setPermissionState:(_WKWebExtensionContextPermissionState)state forPermission:(_WKWebExtensionPermission)permission expirationDate:(nullable NSDate *)expirationDate;

/*!
 @abstract Checks the specified URL against the currently denied, granted, and requested permission match patterns.
 @discussion URLs and match patterns can be granted on a per-tab basis. When the tab is known, access checks should always use the method that checks in a tab.
 @seealso permissionStateForURL:inTab:
 @seealso hasAccessToURL:
*/
- (_WKWebExtensionContextPermissionState)permissionStateForURL:(NSURL *)url NS_SWIFT_NAME(permissionState(forURL:));

/*!
 @abstract Checks the specified URL against the currently denied, granted, and requested permission match patterns in a specific tab.
 @discussion URLs and match patterns can be granted on a per-tab basis. When the tab is known, access checks should always use this method.
 @seealso permissionStateForURL:
 @seealso hasAccessToURL:inTab:
*/
- (_WKWebExtensionContextPermissionState)permissionStateForURL:(NSURL *)url inTab:(nullable id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(permissionState(forURL:in:));

/*!
 @abstract Sets the state of a URL in all tabs and global contexts with a distant future expiration date.
 @discussion The URL is converted into a match pattern and will update `grantedPermissionMatchPatterns` and `deniedPermissionMatchPatterns`. Use this method for changing a single URL's state.
 Only `WKWebExtensionContextPermissionStateDeniedExplicitly`, `WKWebExtensionContextPermissionStateUnknown`, and `WKWebExtensionContextPermissionStateGrantedExplicitly`
 states are allowed to be set using this method.
 @seealso setPermissionState:forURL:expirationDate:
 @seealso setPermissionState:forURL:inTab:
*/
- (void)setPermissionState:(_WKWebExtensionContextPermissionState)state forURL:(NSURL *)url NS_SWIFT_NAME(setPermissionState(_:forURL:));

/*!
 @abstract Sets the state of a URL in all tabs and global contexts with a specific expiration date.
 @discussion The URL is converted into a match pattern and will update `grantedPermissionMatchPatterns` and `deniedPermissionMatchPatterns`. Use this method for changing a single URL's state.
 Passing a `nil` expiration date will be treated as a distant future date. Only `WKWebExtensionContextPermissionStateDeniedExplicitly`, `WKWebExtensionContextPermissionStateUnknown`,
 and `WKWebExtensionContextPermissionStateGrantedExplicitly` states are allowed to be set using this method.
 @seealso setPermissionState:forURL:
 @seealso setPermissionState:forURL:inTab:
*/
- (void)setPermissionState:(_WKWebExtensionContextPermissionState)state forURL:(NSURL *)url expirationDate:(nullable NSDate *)expirationDate NS_SWIFT_NAME(setPermissionState(_:forURL:expirationDate:));

/*!
 @abstract Checks the specified match pattern against the currently denied, granted, and requested permission match patterns.
 @discussion Match patterns can be granted on a per-tab basis. When the tab is known, access checks should always use the method that checks in a tab.
 @seealso permissionStateForMatchPattern:inTab:
 @seealso hasAccessToURL:inTab:
*/
- (_WKWebExtensionContextPermissionState)permissionStateForMatchPattern:(_WKWebExtensionMatchPattern *)pattern;

/*!
 @abstract Checks the specified match pattern against the currently denied, granted, and requested permission match patterns in a specific tab.
 @discussion Match patterns can be granted on a per-tab basis. When the tab is known, access checks should always use this method.
 @seealso permissionStateForMatchPattern:
 @seealso hasAccessToURL:inTab:
*/
- (_WKWebExtensionContextPermissionState)permissionStateForMatchPattern:(_WKWebExtensionMatchPattern *)pattern inTab:(nullable id <_WKWebExtensionTab>)tab;

/*!
 @abstract Sets the state of a match pattern in all tabs and global contexts with a distant future expiration date.
 @discussion This method will update `grantedPermissionMatchPatterns` and `deniedPermissionMatchPatterns`. Use this method for changing a single match pattern's state.
 Only `WKWebExtensionContextPermissionStateDeniedExplicitly`, `WKWebExtensionContextPermissionStateUnknown`, and `WKWebExtensionContextPermissionStateGrantedExplicitly`
 states are allowed to be set using this method.
 @seealso setPermissionState:forMatchPattern:expirationDate:
 @seealso setPermissionState:forMatchPattern:inTab:
*/
- (void)setPermissionState:(_WKWebExtensionContextPermissionState)state forMatchPattern:(_WKWebExtensionMatchPattern *)pattern;

/*!
 @abstract Sets the state of a match pattern in all tabs and global contexts with a specific expiration date.
 @discussion This method will update `grantedPermissionMatchPatterns` and `deniedPermissionMatchPatterns`. Use this method for changing a single match pattern's state.
 Passing a `nil` expiration date will be treated as a distant future date. Only `WKWebExtensionContextPermissionStateDeniedExplicitly`, `WKWebExtensionContextPermissionStateUnknown`,
 and `WKWebExtensionContextPermissionStateGrantedExplicitly` states are allowed to be set using this method.
 @seealso setPermissionState:forMatchPattern:
 @seealso setPermissionState:forMatchPattern:inTab:
*/
- (void)setPermissionState:(_WKWebExtensionContextPermissionState)state forMatchPattern:(_WKWebExtensionMatchPattern *)pattern expirationDate:(nullable NSDate *)expirationDate;

/*!
 @abstract Notes that a user gesture has been performed in the specified tab (e.g. interacting with a toolbar button, menu item, etc.)
 @discussion This method might grant per-tab permissions and/or match patterns for the current website if the extension has the `activeTab` permission.
 This method can also propagate the user gesture state to the tab's page when the extension sends a message to the tab.
 @seealso hasActiveUserGestureInTab:
 @seealso cancelUserGestureForTab:
*/
- (void)userGesturePerformedInTab:(id <_WKWebExtensionTab>)tab;

/*!
 @abstract Returns a Boolean value indicating if a user gesture has been noted for the specified tab.
 @seealso userGesturePerformedInTab:
 @seealso cancelUserGestureForTab:
*/
- (BOOL)hasActiveUserGestureInTab:(id <_WKWebExtensionTab>)tab;

/*!
 @abstract Clears the current user gesture state for the specified tab.
 @seealso userGesturePerformedInTab:
 @seealso hasActiveUserGestureInTab:
*/
- (void)cancelUserGestureForTab:(id <_WKWebExtensionTab>)tab;

@end

NS_ASSUME_NONNULL_END
