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
#import <WebKit/_WKWebExtensionTab.h>

@class _WKWebExtension;
@class _WKWebExtensionController;

NS_ASSUME_NONNULL_BEGIN

/*! @abstract Indicates a @link WKWebExtensionContext @/link error. */
WK_EXTERN NSErrorDomain const _WKWebExtensionContextErrorDomain NS_SWIFT_NAME(_WKWebExtensionContext.ErrorDomain) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

/*!
 @abstract Constants used by NSError to indicate errors in the @link WKWebExtensionContext @/link domain.
 @constant WKWebExtensionContextErrorUnknown  Indicates that an unknown error occurred.
 @constant WKWebExtensionContextErrorAlreadyLoaded  Indicates that the context is already loaded by a @link WKWebExtensionController @/link.
 @constant WKWebExtensionContextErrorNotLoaded  Indicates that the context is not loaded by a @link WKWebExtensionController @/link.
 @constant WKWebExtensionContextErrorBaseURLAlreadyInUse  Indicates that another context is already using the specified base URL.
 */
typedef NS_ERROR_ENUM(_WKWebExtensionContextErrorDomain, _WKWebExtensionContextError) {
    _WKWebExtensionContextErrorUnknown = 1,
    _WKWebExtensionContextErrorAlreadyLoaded,
    _WKWebExtensionContextErrorNotLoaded,
    _WKWebExtensionContextErrorBaseURLAlreadyInUse,
} NS_SWIFT_NAME(_WKWebExtensionContext.Error) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

/*!
 @abstract Constants used to indicate permission status in @link WKWebExtensionContext @/link.
 @constant WKWebExtensionContextPermissionStatusDeniedExplicitly  Indicates that the permission was explicitly denied.
 @constant WKWebExtensionContextPermissionStatusDeniedImplicitly  Indicates that the permission was implicitly denied because of another explicitly denied permission.
 @constant WKWebExtensionContextPermissionStatusRequestedImplicitly  Indicates that the permission was implicitly requested because of another explicitly requested permission.
 @constant WKWebExtensionContextPermissionStatusUnknown  Indicates that an unknown permission status.
 @constant WKWebExtensionContextPermissionStatusRequestedExplicitly  Indicates that the permission was explicitly requested.
 @constant WKWebExtensionContextPermissionStatusGrantedImplicitly  Indicates that the permission was implicitly granted because of another explicitly granted permission.
 @constant WKWebExtensionContextPermissionStatusGrantedExplicitly  Indicates that the permission was explicitly granted permission.
 */
typedef NS_ENUM(NSInteger, _WKWebExtensionContextPermissionStatus) {
    _WKWebExtensionContextPermissionStatusDeniedExplicitly    = -3,
    _WKWebExtensionContextPermissionStatusDeniedImplicitly    = -2,
    _WKWebExtensionContextPermissionStatusRequestedImplicitly = -1,
    _WKWebExtensionContextPermissionStatusUnknown             =  0,
    _WKWebExtensionContextPermissionStatusRequestedExplicitly =  1,
    _WKWebExtensionContextPermissionStatusGrantedImplicitly   =  2,
    _WKWebExtensionContextPermissionStatusGrantedExplicitly   =  3,
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
 @abstract A `WKWebExtensionContext` object represents the runtime environment for a web extension.
 @discussion This class provides methods for managing the extension's permissions, allowing it to inject content, run
 background logic, show popovers, and display other web-based UI to the user.
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
+ (instancetype)contextForExtension:(_WKWebExtension *)extension;

/*!
 @abstract Returns a web extension context initialized with a specified extension.
 @param extension The extension to use for the new web extension context.
 @result An initialized web extension context.
 @discussion This is a designated initializer.
 */
- (instancetype)initForExtension:(_WKWebExtension *)extension NS_DESIGNATED_INITIALIZER;

/*! @abstract The extension this context represents. */
@property (nonatomic, readonly, strong) _WKWebExtension *webExtension;

/*! @abstract The extension controller this context is loaded in, otherwise `nil` if it isn't loaded. */
@property (nonatomic, readonly, weak) _WKWebExtensionController *webExtensionController;

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

 This value is accessible by the extension via `browser.runtime.id` and is used for messaging the extension as the first
 argument of `browser.runtime.sendMessage(extensionId, message, options)`.
 */
@property (nonatomic, copy) NSString *uniqueIdentifier;

/*!
 @abstract Determines whether Web Inspector can inspect the @link WKWebView @/link instances for this context.
 @discussion A context can control multiple `WKWebView` instances, from the background content, to the popover.
 You should set this to `YES` when needed for debugging purposes. The default value is `NO`.
*/
@property (nonatomic, getter=isInspectable) BOOL inspectable;

/*!
 @abstract The currently granted permissions and their expiration dates.
 @discussion Permissions that don't expire will have a distant future date. This will never include expired entries at time of access.
 Setting this property will replace all existing entries. Use this property for saving and restoring permission status in bulk.

 Permissions in this dictionary should be explicitly granted by the user before being added. Any permissions in this collection will not be
 presented for approval again until they expire.
 @seealso setPermissionStatus:forPermission:
 @seealso setPermissionStatus:forPermission:expirationDate:
 */
@property (nonatomic, copy) NSDictionary<_WKWebExtensionPermission, NSDate *> *grantedPermissions;

/*!
 @abstract The currently granted permission match patterns and their expiration dates.
 @discussion Match patterns that don't expire will have a distant future date. This will never include expired entries at time of access.
 Setting this property will replace all existing entries. Use this property for saving and restoring permission status in bulk.

 Match patterns in this dictionary should be explicitly granted by the user before being added. Any match pattern in this collection will not be
 presented for approval again until they expire.
 @seealso setPermissionStatus:forMatchPattern:
 @seealso setPermissionStatus:forMatchPattern:expirationDate:
 */
@property (nonatomic, copy) NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *grantedPermissionMatchPatterns;

/*!
 @abstract The currently denied permissions and their expiration dates.
 @discussion Permissions that don't expire will have a distant future date. This will never include expired entries at time of access.
 Setting this property will replace all existing entries. Use this property for saving and restoring permission status in bulk.

 Permissions in this dictionary should be explicitly denied by the user before being added. Any match pattern in this collection will not be
 presented for approval again until they expire.
 @seealso setPermissionStatus:forPermission:
 @seealso setPermissionStatus:forPermission:expirationDate:
 */
@property (nonatomic, copy) NSDictionary<_WKWebExtensionPermission, NSDate *> *deniedPermissions;

/*!
 @abstract The currently denied permission match patterns and their expiration dates.
 @discussion Match patterns that don't expire will have a distant future date. This will never include expired entries at time of access.
 Setting this property will replace all existing entries. Use this property for saving and restoring permission status in bulk.

 Match patterns in this dictionary should be explicitly denied by the user before being added. Any match pattern in this collection will not be
 presented for approval again until they expire.
 @seealso setPermissionStatus:forMatchPattern:
 @seealso setPermissionStatus:forMatchPattern:expirationDate:
 */
@property (nonatomic, copy) NSDictionary<_WKWebExtensionMatchPattern *, NSDate *> *deniedPermissionMatchPatterns;

/*!
 @abstract A Boolean value indicating if the extension has requested optional access to all hosts.
 @discussion When this value is `YES` the extension has asked for access to all hosts in a call to `browser.runtime.permissions.request()`
 and future permission checks will present discrete hosts for approval as being implicitly requested. This value should be saved and restored as needed.
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
 @param permission The permission for which to return the status.
 @seealso currentPermissions
 @seealso hasPermission:inTab:
 @seealso permissionStatusForPermission:
 @seealso permissionStatusForPermission:inTab:
*/
- (BOOL)hasPermission:(_WKWebExtensionPermission)permission NS_SWIFT_UNAVAILABLE("Use tab version with nil");

/*!
 @abstract Checks the specified permission against the currently granted permissions in a specific tab.
 @param permission The permission for which to return the status.
 @param tab The tab in which to return the permission status, or \c nil if the tab is not known or the global status is desired.
 @discussion Permissions can be granted on a per-tab basis. When the tab is known, permission checks should always use this method.
 @seealso currentPermissions
 @seealso hasPermission:
 @seealso permissionStatusForPermission:
 @seealso permissionStatusForPermission:inTab:
 */
- (BOOL)hasPermission:(_WKWebExtensionPermission)permission inTab:(nullable id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(hasPermission(_:in:));

/*!
 @abstract Checks the specified URL against the currently granted permission match patterns.
 @param url The URL for which to return the status.
 @seealso currentPermissionMatchPatterns
 @seealso hasAccessToURL:inTab:
 @seealso permissionStatusForURL:
 @seealso permissionStatusForURL:inTab:
 @seealso permissionStatusForMatchPattern:
 @seealso permissionStatusForMatchPattern:inTab:
 */
- (BOOL)hasAccessToURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use tab version with nil");

/*!
 @abstract Checks the specified URL against the currently granted permission match patterns in a specific tab.
 @param url The URL for which to return the status.
 @param tab The tab in which to return the permission status, or \c nil if the tab is not known or the global status is desired.
 @discussion Some match patterns can be granted on a per-tab basis. When the tab is known, access checks should always use this method.
 @seealso currentPermissionMatchPatterns
 @seealso hasAccessToURL:
 @seealso permissionStatusForURL:
 @seealso permissionStatusForURL:inTab:
 @seealso permissionStatusForMatchPattern:
 @seealso permissionStatusForMatchPattern:inTab:
 */
- (BOOL)hasAccessToURL:(NSURL *)url inTab:(nullable id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(hasAccess(to:in:));

/*!
 @abstract Checks if the currently granted permission match patterns set contains the `<all_urls>` pattern.
 @discussion This does not check for any `*` host patterns. In most cases you should use the broader `hasAccessToAllHosts`.
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
 @param permission The permission for which to return the status.
 @discussion Permissions can be granted on a per-tab basis. When the tab is known, access checks should always use the method that checks in a tab.
 @seealso permissionStatusForPermission:inTab:
 @seealso hasPermission:
*/
- (_WKWebExtensionContextPermissionStatus)permissionStatusForPermission:(_WKWebExtensionPermission)permission NS_SWIFT_UNAVAILABLE("Use tab version with nil");

/*!
 @abstract Checks the specified permission against the currently denied, granted, and requested permissions.
 @param permission The permission for which to return the status.
 @param tab The tab in which to return the permission status, or \c nil if the tab is not known or the global status is desired.
 @discussion Permissions can be granted on a per-tab basis. When the tab is known, access checks should always specify the tab.
 @seealso permissionStatusForPermission:
 @seealso hasPermission:inTab:
*/
- (_WKWebExtensionContextPermissionStatus)permissionStatusForPermission:(_WKWebExtensionPermission)permission inTab:(nullable id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(permissionStatus(for:in:));

/*!
 @abstract Sets the status of a permission with a distant future expiration date.
 @param status The new permission status to set for the given permission.
 @param permission The permission for which to set the status.
 @discussion This method will update `grantedPermissions` and `deniedPermissions`. Use this method for changing a single permission's status.
 Only `WKWebExtensionContextPermissionStatusDeniedExplicitly`, `WKWebExtensionContextPermissionStatusUnknown`, and `WKWebExtensionContextPermissionStatusGrantedExplicitly`
 states are allowed to be set using this method.
 @seealso setPermissionStatus:forPermission:expirationDate:
 @seealso setPermissionStatus:forPermission:inTab:
*/
- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forPermission:(_WKWebExtensionPermission)permission NS_SWIFT_UNAVAILABLE("Use expirationDate version with nil");

/*!
 @abstract Sets the status of a permission with a specific expiration date.
 @param status The new permission status to set for the given permission.
 @param permission The permission for which to set the status.
 @param expirationDate The expiration date for the new permission status, or \c nil for distant future.
 @discussion This method will update `grantedPermissions` and `deniedPermissions`. Use this method for changing a single permission's status.
 Passing a `nil` expiration date will be treated as a distant future date. Only `WKWebExtensionContextPermissionStatusDeniedExplicitly`, `WKWebExtensionContextPermissionStatusUnknown`,
 and `WKWebExtensionContextPermissionStatusGrantedExplicitly` states are allowed to be set using this method.
 @seealso setPermissionStatus:forPermission:
 @seealso setPermissionStatus:forPermission:inTab:
*/
- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forPermission:(_WKWebExtensionPermission)permission expirationDate:(nullable NSDate *)expirationDate NS_SWIFT_NAME(setPermissionStatus(_:for:expirationDate:));

/*!
 @abstract Checks the specified URL against the currently denied, granted, and requested permission match patterns.
 @param url The URL for which to return the status.
 @discussion URLs and match patterns can be granted on a per-tab basis. When the tab is known, access checks should always use the method that checks in a tab.
 @seealso permissionStatusForURL:inTab:
 @seealso hasAccessToURL:
*/
- (_WKWebExtensionContextPermissionStatus)permissionStatusForURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use tab version with nil");

/*!
 @abstract Checks the specified URL against the currently denied, granted, and requested permission match patterns.
 @param url The URL for which to return the status.
 @param tab The tab in which to return the permission status, or \c nil if the tab is not known or the global status is desired.
 @discussion URLs and match patterns can be granted on a per-tab basis. When the tab is known, access checks should always use this method.
 @seealso permissionStatusForURL:
 @seealso hasAccessToURL:inTab:
*/
- (_WKWebExtensionContextPermissionStatus)permissionStatusForURL:(NSURL *)url inTab:(nullable id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(permissionStatus(for:in:));

/*!
 @abstract Sets the permission status of a URL with a distant future expiration date.
 @param status The new permission status to set for the given URL.
 @param url The URL for which to set the status.
 @discussion The URL is converted into a match pattern and will update `grantedPermissionMatchPatterns` and `deniedPermissionMatchPatterns`. Use this method for changing a single URL's status.
 Only `WKWebExtensionContextPermissionStatusDeniedExplicitly`, `WKWebExtensionContextPermissionStatusUnknown`, and `WKWebExtensionContextPermissionStatusGrantedExplicitly`
 states are allowed to be set using this method.
 @seealso setPermissionStatus:forURL:expirationDate:
 @seealso setPermissionStatus:forURL:inTab:
*/
- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forURL:(NSURL *)url NS_SWIFT_UNAVAILABLE("Use expirationDate version with nil");

/*!
 @abstract Sets the permission status of a URL with a distant future expiration date.
 @param status The new permission status to set for the given URL.
 @param url The URL for which to set the status.
 @param expirationDate The expiration date for the new permission status, or \c nil for distant future.
 @discussion The URL is converted into a match pattern and will update `grantedPermissionMatchPatterns` and `deniedPermissionMatchPatterns`. Use this method for changing a single URL's status.
 Passing a `nil` expiration date will be treated as a distant future date. Only `WKWebExtensionContextPermissionStatusDeniedExplicitly`, `WKWebExtensionContextPermissionStatusUnknown`,
 and `WKWebExtensionContextPermissionStatusGrantedExplicitly` states are allowed to be set using this method.
 @seealso setPermissionStatus:forURL:
 @seealso setPermissionStatus:forURL:inTab:
*/
- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forURL:(NSURL *)url expirationDate:(nullable NSDate *)expirationDate NS_SWIFT_NAME(setPermissionStatus(_:for:expirationDate:));

/*!
 @abstract Checks the specified match pattern against the currently denied, granted, and requested permission match patterns.
 @param pattern The pattern for which to return the status.
 @discussion Match patterns can be granted on a per-tab basis. When the tab is known, access checks should always use the method that checks in a tab.
 @seealso permissionStatusForMatchPattern:inTab:
 @seealso hasAccessToURL:inTab:
*/
- (_WKWebExtensionContextPermissionStatus)permissionStatusForMatchPattern:(_WKWebExtensionMatchPattern *)pattern NS_SWIFT_UNAVAILABLE("Use tab version with nil");

/*!
 @abstract Checks the specified match pattern against the currently denied, granted, and requested permission match patterns.
 @param pattern The pattern for which to return the status.
 @param tab The tab in which to return the permission status, or \c nil if the tab is not known or the global status is desired.
 @discussion Match patterns can be granted on a per-tab basis. When the tab is known, access checks should always use this method.
 @seealso permissionStatusForMatchPattern:
 @seealso hasAccessToURL:inTab:
*/
- (_WKWebExtensionContextPermissionStatus)permissionStatusForMatchPattern:(_WKWebExtensionMatchPattern *)pattern inTab:(nullable id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(permissionStatus(for:in:));

/*!
 @abstract Sets the status of a match pattern with a distant future expiration date.
 @param status The new permission status to set for the given match pattern.
 @param pattern The match pattern for which to set the status.
 @discussion This method will update `grantedPermissionMatchPatterns` and `deniedPermissionMatchPatterns`. Use this method for changing a single match pattern's status.
 Only `WKWebExtensionContextPermissionStatusDeniedExplicitly`, `WKWebExtensionContextPermissionStatusUnknown`, and `WKWebExtensionContextPermissionStatusGrantedExplicitly`
 states are allowed to be set using this method.
 @seealso setPermissionStatus:forMatchPattern:expirationDate:
 @seealso setPermissionStatus:forMatchPattern:inTab:
*/
- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forMatchPattern:(_WKWebExtensionMatchPattern *)pattern NS_SWIFT_UNAVAILABLE("Use expirationDate version with nil");

/*!
 @abstract Sets the status of a match pattern with a specific expiration date.
 @param status The new permission status to set for the given match pattern.
 @param pattern The match pattern for which to set the status.
 @param expirationDate The expiration date for the new permission status, or \c nil for distant future.
 @discussion This method will update `grantedPermissionMatchPatterns` and `deniedPermissionMatchPatterns`. Use this method for changing a single match pattern's status.
 Passing a `nil` expiration date will be treated as a distant future date. Only `WKWebExtensionContextPermissionStatusDeniedExplicitly`, `WKWebExtensionContextPermissionStatusUnknown`,
 and `WKWebExtensionContextPermissionStatusGrantedExplicitly` states are allowed to be set using this method.
 @seealso setPermissionStatus:forMatchPattern:
 @seealso setPermissionStatus:forMatchPattern:inTab:
*/
- (void)setPermissionStatus:(_WKWebExtensionContextPermissionStatus)status forMatchPattern:(_WKWebExtensionMatchPattern *)pattern expirationDate:(nullable NSDate *)expirationDate NS_SWIFT_NAME(setPermissionStatus(_:for:expirationDate:));

/*!
 @abstract Returns a Boolean value indicating if a user gesture has been noted for the specified tab.
 @param tab The tab in which to return the user gesture status.
*/
- (BOOL)hasActiveUserGestureInTab:(id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(hasActiveUserGesture(in:));

@end

NS_ASSUME_NONNULL_END
