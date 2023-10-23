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

@class WKWebViewConfiguration;
@class _WKWebExtension;
@class _WKWebExtensionAction;
@class _WKWebExtensionController;

NS_ASSUME_NONNULL_BEGIN

/*! @abstract Indicates a @link WKWebExtensionContext @/link error. */
WK_EXTERN NSErrorDomain const _WKWebExtensionContextErrorDomain NS_SWIFT_NAME(_WKWebExtensionContext.ErrorDomain) WK_API_AVAILABLE(macos(13.3), ios(16.4));

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
} NS_SWIFT_NAME(_WKWebExtensionContext.Error) WK_API_AVAILABLE(macos(13.3), ios(16.4));

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
} NS_SWIFT_NAME(_WKWebExtensionContext.PermissionState) WK_API_AVAILABLE(macos(13.3), ios(16.4));

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly granted permissions. */
WK_API_AVAILABLE(macos(13.3), ios(16.4))
WK_EXTERN NSNotificationName const _WKWebExtensionContextPermissionsWereGrantedNotification NS_SWIFT_NAME(_WKWebExtensionContext.permissionsWereGrantedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly denied permissions. */
WK_API_AVAILABLE(macos(13.3), ios(16.4))
WK_EXTERN NSNotificationName const _WKWebExtensionContextPermissionsWereDeniedNotification NS_SWIFT_NAME(_WKWebExtensionContext.permissionsWereDeniedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly removed granted permissions. */
WK_API_AVAILABLE(macos(13.3), ios(16.4))
WK_EXTERN NSNotificationName const _WKWebExtensionContextGrantedPermissionsWereRemovedNotification NS_SWIFT_NAME(_WKWebExtensionContext.grantedPermissionsWereRemovedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly removed denied permissions. */
WK_API_AVAILABLE(macos(13.3), ios(16.4))
WK_EXTERN NSNotificationName const _WKWebExtensionContextDeniedPermissionsWereRemovedNotification NS_SWIFT_NAME(_WKWebExtensionContext.deniedPermissionsWereRemovedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly granted permission match patterns. */
WK_API_AVAILABLE(macos(13.3), ios(16.4))
WK_EXTERN NSNotificationName const _WKWebExtensionContextPermissionMatchPatternsWereGrantedNotification NS_SWIFT_NAME(_WKWebExtensionContext.permissionMatchPatternsWereGrantedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly denied permission match patterns. */
WK_API_AVAILABLE(macos(13.3), ios(16.4))
WK_EXTERN NSNotificationName const _WKWebExtensionContextPermissionMatchPatternsWereDeniedNotification NS_SWIFT_NAME(_WKWebExtensionContext.permissionMatchPatternsWereDeniedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly removed granted permission match patterns. */
WK_API_AVAILABLE(macos(13.3), ios(16.4))
WK_EXTERN NSNotificationName const _WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification NS_SWIFT_NAME(_WKWebExtensionContext.grantedPermissionMatchPatternsWereRemovedNotification);

/*! @abstract This notification is sent whenever a @link WKWebExtensionContext @/link has newly removed denied permission match patterns. */
WK_API_AVAILABLE(macos(13.3), ios(16.4))
WK_EXTERN NSNotificationName const _WKWebExtensionContextDeniedPermissionMatchPatternsWereRemovedNotification NS_SWIFT_NAME(_WKWebExtensionContext.deniedPermissionMatchPatternsWereRemovedNotification);

/*! @abstract Constants for specifying @link WKWebExtensionContext @/link information in notifications. */
WK_API_AVAILABLE(macos(13.3), ios(16.4))
typedef NSString * _WKWebExtensionContextNotificationUserInfoKey NS_TYPED_EXTENSIBLE_ENUM NS_SWIFT_NAME(_WKWebExtensionContext.NotificationUserInfoKey);

/*! @abstract The corresponding value represents the affected permissions in @link WKWebExtensionContext @/link notifications. */
WK_API_AVAILABLE(macos(13.3), ios(16.4))
WK_EXTERN _WKWebExtensionContextNotificationUserInfoKey const _WKWebExtensionContextNotificationUserInfoKeyPermissions;

/*! @abstract The corresponding value represents the affected permission match patterns in @link WKWebExtensionContext @/link notifications. */
WK_API_AVAILABLE(macos(13.3), ios(16.4))
WK_EXTERN _WKWebExtensionContextNotificationUserInfoKey const _WKWebExtensionContextNotificationUserInfoKeyMatchPatterns;

/*!
 @abstract A `WKWebExtensionContext` object represents the runtime environment for a web extension.
 @discussion This class provides methods for managing the extension's permissions, allowing it to inject content, run
 background logic, show popovers, and display other web-based UI to the user.
 */
WK_CLASS_AVAILABLE(macos(13.3), ios(16.4))
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
@property (nonatomic, readonly, weak, nullable) _WKWebExtensionController *webExtensionController;

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
 value that is unique. Setting is only allowed when the context is not loaded. This value is accessible by the extension via
 `browser.runtime.id` and is used for messaging the extension via `browser.runtime.sendMessage()`.
 */
@property (nonatomic, copy) NSString *uniqueIdentifier;

/*!
 @abstract Determines whether Web Inspector can inspect the @link WKWebView @/link instances for this context.
 @discussion A context can control multiple `WKWebView` instances, from the background content, to the popover.
 You should set this to `YES` when needed for debugging purposes. The default value is `NO`.
*/
@property (nonatomic, getter=isInspectable) BOOL inspectable;

/*!
 @abstract The web view configuration to use for web views that load pages from this extension.
 @discussion Returns a customized copy of the configuration, originally set in the web extension controller configuration, for this extension.
 The app must use this configuration when initializing web views intended to navigate to a URL originating from this extension's base URL.
 The app must also swap web views in tabs when navigating to and from web extension URLs. This property returns `nil` if the context isn't
 associated with a web extension controller. The returned configuration copy can be customized prior to web view initialization.
 @note Navigations will fail if a web view using this configuration attempts to navigate to a URL that doesn't originate from this extension's
 base URL. Similarly, navigations will be canceled if a web view not configured with this configuration attempts to navigate to a URL that does
 originate from this extension's base URL.
 */
@property (nonatomic, readonly, copy, nullable) WKWebViewConfiguration *webViewConfiguration;

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
 @abstract A Boolean value indicating if the extension has access to private browsing windows and tabs.
 @discussion When this value is `YES`, the extension is granted permission to interact with private browsing windows and tabs.
 This value should be saved and restored as needed. Access to private browsing should be explicitly allowed by the user before setting this property.
 @note To ensure proper isolation between private and non-private browsing, web views associated with private browsing windows must
 use a different `WKUserContentController`. Likewise, to be identified as a private web view and to ensure that cookies and other
 website data is not shared, private web views must be configured to use a non-persistent `WKWebsiteDataStore`.
 */
@property (nonatomic) BOOL hasAccessInPrivateBrowsing;

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
 @abstract Retrieves the extension action for a given tab, or the default action if `nil` is passed.
 @param tab The tab for which to retrieve the extension action, or `nil` to get the default action.
 @discussion The returned object represents the action specific to the tab when provided; otherwise, it returns the default action. The default
 action is useful when the context is unrelated to a specific tab. When possible, specify the tab to get the most context-relevant action.
 */
- (_WKWebExtensionAction *)actionForTab:(nullable id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(action(for:));

/*!
 @abstract Performs the extension action associated with the specified tab or performs the default action if `nil` is passed.
 @param tab The tab for which to perform the extension action, or `nil` to perform the default action.
 @discussion Performing the action will mark the tab, if specified, as having an active user gesture. When the `tab` parameter is `nil`,
 the default action is performed. The action can either trigger an event or display a popup, depending on how the extension is configured.
 If the action is configured to display a popup, implementing the appropriate web extension controller delegate method is required; otherwise,
 no action is performed for popup actions.
 */
- (void)performActionForTab:(nullable id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(performAction(for:));

/*!
 @abstract Should be called by the app when a user gesture is performed in a specific tab.
 @param tab The tab in which the user gesture was performed.
 @discussion When a user gesture is performed in a tab, this method should be called to update the extension context.
 This enables the extension to be aware of the user gesture, potentially granting it access to features that require user interaction,
 such as `activeTab`. Not required if using `performActionForTab:`.
 @seealso hasActiveUserGestureInTab:
 */
- (void)userGesturePerformedInTab:(id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(userGesturePerformed(in:));

/*!
 @abstract Indicates if a user gesture is currently active in the specified tab.
 @param tab The tab for which to check for an active user gesture.
 @discussion An active user gesture may influence the availability of certain permissions, such as `activeTab`. User gestures can
 be triggered by various user interactions with the web extension, including clicking on extension menu items, executing extension commands,
 or interacting with extension actions. A tab as having an active user gesture enables the extension to access features that require user interaction.
 @seealso userGesturePerformedInTab:
 */
- (BOOL)hasActiveUserGestureInTab:(id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(hasActiveUserGesture(in:));

/*!
 @abstract Should be called by the app to clear a user gesture in a specific tab.
 @param tab The tab from which the user gesture should be cleared.
 @discussion When a user gesture is no longer relevant in a tab, this method should be called to update the extension context.
 This will revoke the extension's access to features that require active user interaction, such as `activeTab`. User gestures are
 automatically cleared during navigation in certain scenarios; this method is needed if the app intends to clear the gesture more aggressively.
 @seealso userGesturePerformedInTab:
 */
- (void)clearUserGestureInTab:(id <_WKWebExtensionTab>)tab NS_SWIFT_NAME(clearUserGesture(in:));

/*!
 @abstract An array of open windows that are exposed to this extension.
 @discussion This property holds an array of window objects that are open and visible to the extension, as updated by the `didOpenWindow:` and `didCloseWindow:` methods.
 Initially populated by the windows returned by the extension controller delegate method `webExtensionController:openWindowsForExtensionContext:`.
 @seealso didOpenWindow:
 @seealso didCloseWindow:
 */
@property (nonatomic, readonly, copy) NSArray<id <_WKWebExtensionWindow>> *openWindows;

/*!
 @abstract The window that currently has focus.
 @discussion This property holds the window object that currently has focus, as set by the `didFocusWindow:` method.
 It will be \c nil if no window has focus or if a window has focus that is not visible to the extension.  Initially populated by the window
 returned by the extension controller delegate method `webExtensionController:focusedWindowForExtensionContext:`.
 @seealso didFocusWindow:
 */
@property (nonatomic, readonly, weak, nullable) id <_WKWebExtensionWindow> focusedWindow;

/*!
 @abstract A set of open tabs in all open windows that are exposed to this extension.
 @discussion This property holds a set of tabs in all open windows that are visible to the extension, as updated by the `didOpenTab:` and `didCloseTab:` methods.
 Initially populated by the tabs in the windows returned by the extension controller delegate method `webExtensionController:openWindowsForExtensionContext:`.
 @seealso didOpenTab:
 @seealso didCloseTab:
 */
@property (nonatomic, readonly, copy) NSSet<id <_WKWebExtensionTab>> *openTabs;

/*!
 @abstract Should be called by the app when a new window is opened to fire appropriate events with only this extension.
 @param newWindow The newly opened window.
 @discussion This method informs only the specific extension of the opening of a new window. If the intention is to inform all loaded
 extensions consistently, you should use the respective method on the extension controller instead.
 @seealso didCloseWindow:
 @seealso openWindows
 */
- (void)didOpenWindow:(id <_WKWebExtensionWindow>)newWindow;

/*!
 @abstract Should be called by the app when a window is closed to fire appropriate events with only this extension.
 @param newWindow The window that was closed.
 @discussion This method informs only the specific extension of the closure of a window. If the intention is to inform all loaded
 extensions consistently, you should use the respective method on the extension controller instead.
 @seealso didOpenWindow:
 @seealso openWindows
 */
- (void)didCloseWindow:(id <_WKWebExtensionWindow>)closedWindow;

/*!
 @abstract Should be called by the app when a window gains focus to fire appropriate events with only this extension.
 @param focusedWindow The window that gained focus, or \c nil if no window has focus or a window has focus that is not visible to this extension.
 @discussion This method informs only the specific extension that a window has gained focus. If the intention is to inform all loaded
 extensions consistently, you should use the respective method on the extension controller instead.
 */
- (void)didFocusWindow:(nullable id <_WKWebExtensionWindow>)focusedWindow;

/*!
 @abstract Should be called by the app when a new tab is opened to fire appropriate events with only this extension.
 @param newTab The newly opened tab.
 @discussion This method informs only the specific extension of the opening of a new tab. If the intention is to inform all loaded
 extensions consistently, you should use the respective method on the extension controller instead.
 @seealso didCloseTab:
 @seealso openTabs
 */
- (void)didOpenTab:(id <_WKWebExtensionTab>)newTab;

/*!
 @abstract Should be called by the app when a tab is closed to fire appropriate events with only this extension.
 @param closedTab The tab that was closed.
 @param windowIsClosing A boolean value indicating whether the window containing the tab is also closing.
 @discussion This method informs only the specific extension of the closure of a tab. If the intention is to inform all loaded
 extensions consistently, you should use the respective method on the extension controller instead.
 @seealso didOpenTab:
 @seealso openTabs
 */
- (void)didCloseTab:(id <_WKWebExtensionTab>)closedTab windowIsClosing:(BOOL)windowIsClosing;

/*!
 @abstract Should be called by the app when a tab is activated to notify only this specific extension.
 @param activatedTab The tab that has become active.
 @param previousTab The tab that was active before. This parameter can be \c nil if there was no previously active tab.
 @discussion This method informs only the specific extension of the tab activation. If the intention is to inform all loaded
 extensions consistently, you should use the respective method on the extension controller instead.
 */
- (void)didActivateTab:(id<_WKWebExtensionTab>)activatedTab previousActiveTab:(nullable id<_WKWebExtensionTab>)previousTab;

/*!
 @abstract Should be called by the app when tabs are selected to fire appropriate events with only this extension.
 @param selectedTabs The set of tabs that were selected.
 @discussion This method informs only the specific extension that tabs have been selected. If the intention is to inform all loaded
 extensions consistently, you should use the respective method on the extension controller instead.
 */
- (void)didSelectTabs:(NSSet<id <_WKWebExtensionTab>> *)selectedTabs;

/*!
 @abstract Should be called by the app when tabs are deselected to fire appropriate events with only this extension.
 @param deselectedTabs The set of tabs that were deselected.
 @discussion This method informs only the specific extension that tabs have been deselected. If the intention is to inform all loaded
 extensions consistently, you should use the respective method on the extension controller instead.
 */
- (void)didDeselectTabs:(NSSet<id <_WKWebExtensionTab>> *)deselectedTabs;

/*!
 @abstract Should be called by the app when a tab is moved to fire appropriate events with only this extension.
 @param movedTab The tab that was moved.
 @param index The old index of the tab within the window.
 @param oldWindow The window that the tab was moved from, or \c nil if the window stayed the same.
 @discussion This method informs only the specific extension that a tab has been moved. If the intention is to inform all loaded
 extensions consistently, you should use the respective method on the extension controller instead.
 */
- (void)didMoveTab:(id <_WKWebExtensionTab>)movedTab fromIndex:(NSUInteger)index inWindow:(nullable id <_WKWebExtensionWindow>)oldWindow NS_SWIFT_NAME(didMoveTab(_:from:in:));

/*!
 @abstract Should be called by the app when a tab is replaced by another tab to fire appropriate events with only this extension.
 @param oldTab The tab that was replaced.
 @param newTab The tab that replaced the old tab.
 @discussion This method informs only the specific extension that a tab has been replaced. If the intention is to inform all loaded
 extensions consistently, you should use the respective method on the extension controller instead.
 */
- (void)didReplaceTab:(id <_WKWebExtensionTab>)oldTab withTab:(id <_WKWebExtensionTab>)newTab NS_SWIFT_NAME(didReplaceTab(_:with:));

/*!
 @abstract Should be called by the app when the properties of a tab are changed to fire appropriate events with only this extension.
 @param properties The properties of the tab that were changed.
 @param changedTab The tab whose properties were changed.
 @discussion This method informs only the specific extension of the changes to a tab's properties. If the intention is to inform all loaded
 extensions consistently, you should use the respective method on the extension controller instead.
 */
- (void)didChangeTabProperties:(_WKWebExtensionTabChangedProperties)properties forTab:(id <_WKWebExtensionTab>)changedTab NS_SWIFT_NAME(didChangeTabProperties(_:for:));

@end

NS_ASSUME_NONNULL_END
