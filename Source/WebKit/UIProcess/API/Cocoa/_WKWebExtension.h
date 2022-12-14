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

#if TARGET_OS_IPHONE
@class UIImage;
#else
@class NSImage;
#endif

NS_ASSUME_NONNULL_BEGIN

/*! @abstract Indicates a @link WKWebExtension @/link error. */
WK_EXTERN NSErrorDomain const _WKWebExtensionErrorDomain NS_SWIFT_NAME(_WKWebExtension.ErrorDomain) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

/*!
 @abstract Constants used by NSError to indicate errors in the @link WKWebExtension @/link domain.
 @constant WKWebExtensionErrorUnknown  Indicates that an unknown error occurred.
 @constant WKWebExtensionErrorResourceNotFound  Indicates that a specified resource was not found on disk.
 @constant WKWebExtensionErrorInvalidResourceCodeSignature  Indicates that a resource failed the bundle's code signature checks.
 @constant WKWebExtensionErrorInvalidManifest  Indicates that an invalid `manifest.json` was encountered.
 @constant WKWebExtensionErrorUnsupportedManifestVersion  Indicates that the manifest version is not supported.
 @constant WKWebExtensionErrorInvalidManifestEntry  Indicates that an invalid manifest entry was encountered.
 @constant WKWebExtensionErrorInvalidDeclarativeNetRequestEntry  Indicates that an invalid declarative net request entry was encountered.
 @constant WKWebExtensionErrorInvalidBackgroundPersistence  Indicates that the extension specified background persistence that was not compatible with the platform or features requested.
 @constant WKWebExtensionErrorBackgroundContentFailedToLoad  Indicates that an error occurred loading the background content.
 */
typedef NS_ERROR_ENUM(_WKWebExtensionErrorDomain, _WKWebExtensionError) {
    _WKWebExtensionErrorUnknown = 1,
    _WKWebExtensionErrorResourceNotFound,
    _WKWebExtensionErrorInvalidResourceCodeSignature,
    _WKWebExtensionErrorInvalidManifest,
    _WKWebExtensionErrorUnsupportedManifestVersion,
    _WKWebExtensionErrorInvalidManifestEntry,
    _WKWebExtensionErrorInvalidDeclarativeNetRequestEntry,
    _WKWebExtensionErrorInvalidBackgroundPersistence,
    _WKWebExtensionErrorBackgroundContentFailedToLoad,
} NS_SWIFT_NAME(_WKWebExtension.Error) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

/*! @abstract This notification is sent whenever a @link WKWebExtension @/link has new errors or errors were cleared. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionErrorsWereUpdatedNotification NS_SWIFT_NAME(_WKWebExtension.errorsWereUpdatedNotification);

/*!
 @abstract A `WKWebExtension` object encapsulates a web extension’s resources that are defined by a `manifest.json` file.
 @discussion This class handles the reading and parsing of the manifest file along with the supporting resources like icons and localizations.
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKWebExtension : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*!
 @abstract Returns a web extension initialized with a specified app extension bundle.
 @param appExtensionBundle The bundle to use for the new web extension.
 @result An initialized web extension, or `nil` if the object could not be initialized due to an error.
 @seealso initWithAppExtensionBundle:error:
 */
+ (nullable instancetype)extensionWithAppExtensionBundle:(NSBundle *)appExtensionBundle;

/*!
 @abstract Returns a web extension initialized with a specified resource base URL.
 @param resourceBaseURL The directory URL to use for the new web extension.
 @result An initialized web extension, or `nil` if the object could not be initialized due to an error.
 @seealso initWithResourceBaseURL:error:
 */
+ (nullable instancetype)extensionWithResourceBaseURL:(NSURL *)resourceBaseURL;

/*!
 @abstract Returns a web extension initialized with a specified app extension bundle.
 @param appExtensionBundle The bundle to use for the new web extension.
 @param error Set to \c nil or an \c NSError instance if an error occurred.
 @result An initialized web extension, or `nil` if the object could not be initialized due to an error.
 @discussion This is a designated initializer.
 */
- (nullable instancetype)initWithAppExtensionBundle:(NSBundle *)appExtensionBundle error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/*!
 @abstract Returns a web extension initialized with a specified resource base URL.
 @param resourceBaseURL The directory URL to use for the new web extension.
 @param error Set to \c nil or an \c NSError instance if an error occurred.
 @result An initialized web extension, or `nil` if the object could not be initialized due to an error.
 @discussion This is a designated initializer. The URL must be a file URL that points to a directory containing a `manifest.json` file.
 */
- (nullable instancetype)initWithResourceBaseURL:(NSURL *)resourceBaseURL error:(NSError **)error NS_DESIGNATED_INITIALIZER;

/*!
 @abstract The active errors for the extension.
 @discussion This property returns an array of NSError objects if there are any errors, or an empty array if there are no errors.
 */
@property (nonatomic, readonly, copy) NSArray<NSError *> *errors;

/*! @abstract The parsed manifest as a dictionary. */
@property (nonatomic, readonly, copy) NSDictionary<NSString *, id> *manifest;

/*!
 @abstract The parsed manifest version, or `0` if there is no version specified in the manifest.
 @note An `WKWebExtensionErrorUnsupportedManifestVersion` error will be reported if the manifest version isn't specified.
 */
@property (nonatomic, readonly) double manifestVersion;

/*!
 @abstract Checks if a manifest version is supported by the extension.
 @param manifestVersion The version number to check.
 @result Returns `YES` if the extension specified a manifest version that is greater than or equal to `manifestVersion`.
 */
- (BOOL)supportsManifestVersion:(double)manifestVersion;

/*! @abstract The default locale for the extension. Returns `nil` if there was no default locale specified. */
@property (nonatomic, nullable, readonly, copy) NSLocale *defaultLocale;

/*! @abstract The localized extension name. Returns `nil` if there was no name specified. */
@property (nonatomic, nullable, readonly, copy) NSString *displayName;

/*! @abstract The localized extension short name. Returns `nil` if there was no short name specified. */
@property (nonatomic, nullable, readonly, copy) NSString *displayShortName;

/*! @abstract The localized extension display version. Returns `nil` if there was no display version specified. */
@property (nonatomic, nullable, readonly, copy) NSString *displayVersion;

/*! @abstract The localized extension description. Returns `nil` if there was no description specified. */
@property (nonatomic, nullable, readonly, copy) NSString *displayDescription;

/*! @abstract The localized extension action label. Returns `nil` if there was no action label specified. */
@property (nonatomic, nullable, readonly, copy) NSString *displayActionLabel;

/*! @abstract The extension version. Returns `nil` if there was no version specified. */
@property (nonatomic, nullable, readonly, copy) NSString *version;

/*!
 @abstract Returns the extension's icon image for the specified size.
 @param size The size to use when looking up the icon.
 @result The extension's icon image, or `nil` if the icon was unable to be loaded.
 @discussion This icon should represent the extension in settings or other areas that show the extension. The returned image will be the best
 match for the specified size that is available in the extension's icon set. If no matching icon can be found, the method will return `nil`.
 @seealso actionIconForSize:
 */
#if TARGET_OS_IPHONE
- (nullable UIImage *)iconForSize:(CGSize)size;
#else
- (nullable NSImage *)iconForSize:(CGSize)size;
#endif

/*!
 @abstract Returns the action icon for the specified size.
 @param size The size to use when looking up the action icon.
 @result The action icon, or `nil` if the icon was unable to be loaded.
 @discussion This icon should represent the extension in action sheets or toolbars. The returned image will be the best match for the specified
 size that is available in the extension's action icon set. If no matching icon is available, the method will fall back to the extension's icon.
 @seealso iconForSize:
 */
#if TARGET_OS_IPHONE
- (nullable UIImage *)actionIconForSize:(CGSize)size;
#else
- (nullable NSImage *)actionIconForSize:(CGSize)size;
#endif

/*! @abstract The set of permissions that the extension requires for its base functionality. */
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionPermission> *requestedPermissions;

/*! @abstract The set of permissions that the extension may need for optional functionality. These permissions can be requested by the extension at a later time. */
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionPermission> *optionalPermissions;

/*! @abstract The set of websites that the extension requires access to for its base functionality. */
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionMatchPattern *> *requestedPermissionMatchPatterns;

/*! @abstract The set of websites that the extension may need access to for optional functionality. These match patterns can be requested by the extension at a later time. */
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionMatchPattern *> *optionalPermissionMatchPatterns;

/*! @abstract The set of websites that the extension requires access to for injected content and for receiving messages from websites. */
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionMatchPattern *> *allRequestedMatchPatterns;

/*!
 @abstract A Boolean value indicating whether the extension has background content that can run when needed.
 @discussion If this property is `YES`, the extension can run in the background even when no web pages are open.
 */
@property (nonatomic, readonly) BOOL hasBackgroundContent;

/*!
 @abstract A Boolean value indicating whether the extension has background content that stays in memory as long as the extension is loaded.
 @note Note that extensions are only allowed to have persistent background content on macOS. An `WKWebExtensionErrorInvalidBackgroundPersistence`
 error will be reported on iOS if an attempt is made to load a persistent extension.
 */
@property (nonatomic, readonly) BOOL backgroundContentIsPersistent;

@end

NS_ASSUME_NONNULL_END
