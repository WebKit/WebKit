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

#import <WebKit/_WKWebExtensionPermission.h>
#import <WebKit/_WKWebExtensionMatchPattern.h>

#if TARGET_OS_IPHONE
@class UIImage;
#else
@class NSImage;
#endif

NS_ASSUME_NONNULL_BEGIN

/*! @abstract Indicates a @link WKWebExtension @/link error. */
WK_EXTERN NSErrorDomain const _WKWebExtensionErrorDomain WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

/*!
 @abstract Constants used by NSError to indicate errors in the @link WKWebExtension @/link domain.
 @constant WKWebExtensionErrorUnknown  Indicates that an unknown error occurred.
 @constant WKWebExtensionErrorResourceNotFound  Indicates that a specified resource was not found on disk.
 @constant WKWebExtensionErrorInvalidResourceCodeSignature  Indicates that a resource failed the bundle's code signature checks.
 @constant WKWebExtensionErrorInvalidManifest  Indicates that an invalid `manifest.json` was encountered.
 @constant WKWebExtensionErrorUnsupportedManifestVersion  Indicates that a the manifest version is not supported.
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
 @param bundle The bundle to use for the new web extension.
 @result An initialized web extension, or nil if the object could not be initialized.
 @discussion This is a designated initializer.
 */
- (instancetype)initWithAppExtensionBundle:(NSBundle *)appExtensionBundle NS_DESIGNATED_INITIALIZER;

/*!
 @abstract Returns a web extension initialized with a specified resource base URL.
 @param resourceBaseURL The directory URL to use for the new web extension.
 @result An initialized web extension, or nil if the object could not be initialized.
 @discussion This is a designated initializer. The URL must be a file URL that points
 to a directory containing a `manifest.json` file.
 */
- (instancetype)initWithResourceBaseURL:(NSURL *)resourceBaseURL NS_DESIGNATED_INITIALIZER;

/*! @abstract The active errors for the extension. Returns `nil` if there are no errors. */
@property (nonatomic, nullable, readonly, copy) NSArray<NSError *> *errors;

/*! @abstract The parsed manifest as a dictionary. Returns `nil` if the manifest was unable to be parsed. */
@property (nonatomic, nullable, readonly, copy) NSDictionary<NSString *, id> *manifest;

/*! @abstract The parsed manifest version. */
@property (nonatomic, readonly) double manifestVersion;

/*!
 @abstract Checks if a maniferst version is supported by the extension.
 @param manifestVersion The version number to check.
 @result Returns `YES` if the extension specified a manifest version that is is greater than or equal to `manifestVersion`.
 */
- (BOOL)usesManifestVersion:(double)manifestVersion;

/*! @abstract The parsed and localized extension name. Returns `nil` if the manifest was unable to be parsed or there was no name specified. */
@property (nonatomic, nullable, readonly, copy) NSString *displayName;

/*! @abstract The parsed and localized extension short name. Returns `nil` if the manifest was unable to be parsed or there was no short name specified. */
@property (nonatomic, nullable, readonly, copy) NSString *displayShortName;

/*! @abstract The parsed and localized extension display version. Returns `nil` if the manifest was unable to be parsed or there was no display version specified. */
@property (nonatomic, nullable, readonly, copy) NSString *displayVersion;

/*! @abstract The parsed and localized extension description. Returns `nil` if the manifest was unable to be parsed or there was no description specified. */
@property (nonatomic, nullable, readonly, copy) NSString *displayDescription;

/*! @abstract The parsed and localized extension action label. Returns `nil` if the manifest was unable to be parsed or there was no action label specified. */
@property (nonatomic, nullable, readonly, copy) NSString *displayActionLabel;

/*! @abstract The parsed extension version. Returns `nil` if the manifest was unable to be parsed or there was no version specified. */
@property (nonatomic, nullable, readonly, copy) NSString *version;

#if TARGET_OS_IPHONE

/*!
 @abstract Returns an icon for the extension that is best for the specified size.
 @param size The size the image should be able to fill.
 @result An image that is best for the size. Returns `nil` if the manifest was unable to be parsed or no icons were specified.
 @discussion This icon should respesent the extension in Settings or other general user interface areas. If the extension does not specify an icon large enough
 for the size, then the largest icon specified will be returned. No image scaling is performed.
 @seealso actionIconForSize:
 */
- (nullable UIImage *)iconForSize:(CGSize)size;

/*!
 @abstract Returns an action icon for the extension that is best for the specified size.
 @param size The size the image should be able to fill.
 @result An image that is best for the size. Returns `nil` if the manifest was unable to be parsed or no icons were specified.
 @discussion This icon should respesent the extension in action sheets or toolbars. If the extension does not specify an icon large enough
 for the size, then the largest icon specified will be returned. No image scaling is performed.
 @seealso iconForSize:
 */
- (nullable UIImage *)actionIconForSize:(CGSize)size;

#else

/*!
 @abstract Returns an icon for the extension that is best for the specified size.
 @param size The size the image should be able to fill.
 @result An image that is best for the size. Returns `nil` if the manifest was unable to be parsed or no icons were specified.
 @discussion This icon should respesent the extension in Settings or other general user interface areas. If the extension does not specify an icon large enough
 for the size, then the largest icon specified will be returned. No image scaling is performed.
 @seealso actionIconForSize:
 */
- (nullable NSImage *)iconForSize:(CGSize)size;

/*!
 @abstract Returns an action icon for the extension that is best for the specified size.
 @param size The size the image should be able to fill.
 @result An image that is best for the size. Returns `nil` if the manifest was unable to be parsed or no icons were specified.
 @discussion This icon should respesent the extension in action sheets or toolbars. If the extension does not specify an icon large enough
 for the size, then the largest icon specified will be returned. No image scaling is performed.
 @seealso iconForSize:
 */
- (nullable NSImage *)actionIconForSize:(CGSize)size;

#endif

/*! @abstract The permissions that the extension needs for base functionality. */
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionPermission> *requestedPermissions;

/*! @abstract The permissions that the extension might need for optional functionality. These can be requested later by the extension when needed. */
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionPermission> *optionalPermissions;

/*! @abstract The websites that the extension needs to access for base functionality. */
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionMatchPattern *> *requestedPermissionMatchPatterns;

/*! @abstract The websites that the extension might need to access for optional functionality. */
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionMatchPattern *> *optionalPermissionMatchPatterns;

/*! @abstract The websites that the extension needs to access for base functionality, including injected content and websites that can send messages to the extension. */
@property (nonatomic, readonly, copy) NSSet<_WKWebExtensionMatchPattern *> *allRequestedMatchPatterns;

/*! @abstract A Boolean value indicating whether the extension has background content that can run when needed. */
@property (nonatomic, readonly) BOOL hasBackgroundContent;

/*! @abstract A Boolean value indicating whether the extension has background content that stays in memory as long as the extension is loaded. */
@property (nonatomic, readonly) BOOL backgroundContentIsPersistent;

/*!
 @abstract Checks if the extension has script or stylesheet content that can be injected into the specified URL.
 @param url The webpage URL to check.
 @result Returns `YES` if the extension has content that can be injected by matching the `url` against the extension's requested match patterns.
 @discussion The extension will still need to be loaded and have granted website permissions for its content to actually be injected.
 */
- (BOOL)hasInjectedContentForURL:(NSURL *)url;

@end

NS_ASSUME_NONNULL_END
