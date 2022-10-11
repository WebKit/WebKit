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

WK_EXTERN NSErrorDomain const _WKWebExtensionErrorDomain WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

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

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN NSNotificationName const _WKWebExtensionErrorsWereUpdatedNotification NS_SWIFT_NAME(_WKWebExtension.errorsWereUpdatedNotification);

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKWebExtension : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithAppExtensionBundle:(NSBundle *)appExtensionBundle NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithResourceBaseURL:(NSURL *)resourceBaseURL NS_DESIGNATED_INITIALIZER;

@property (readonly, nullable, nonatomic) NSArray<NSError *> *errors;

@property (readonly, nullable, nonatomic) NSDictionary<NSString *, id> *manifest;

@property (readonly, nonatomic) double manifestVersion;
- (BOOL)usesManifestVersion:(double)manifestVersion;

@property (readonly, nullable, nonatomic) NSString *displayName;
@property (readonly, nullable, nonatomic) NSString *displayShortName;
@property (readonly, nullable, nonatomic) NSString *displayVersion;
@property (readonly, nullable, nonatomic) NSString *displayDescription;
@property (readonly, nullable, nonatomic) NSString *displayActionLabel;

@property (readonly, nullable, nonatomic) NSString *version;

#if TARGET_OS_IPHONE
- (nullable UIImage *)iconForSize:(CGSize)size;
- (nullable UIImage *)actionIconForSize:(CGSize)size;
#else
- (nullable NSImage *)iconForSize:(CGSize)size;
- (nullable NSImage *)actionIconForSize:(CGSize)size;
#endif

@property (readonly, nonatomic) NSSet<_WKWebExtensionPermission> *requestedPermissions;
@property (readonly, nonatomic) NSSet<_WKWebExtensionPermission> *optionalPermissions;

@property (readonly, nonatomic) NSSet<_WKWebExtensionMatchPattern *> *requestedPermissionMatchPatterns;
@property (readonly, nonatomic) NSSet<_WKWebExtensionMatchPattern *> *optionalPermissionMatchPatterns;

@property (readonly, nonatomic) NSSet<_WKWebExtensionMatchPattern *> *allRequestedMatchPatterns;

@property (readonly, nonatomic) BOOL hasBackgroundContent;
@property (readonly, nonatomic) BOOL backgroundContentIsPersistent;

- (BOOL)hasInjectedContentForURL:(NSURL *)url;

@end

NS_ASSUME_NONNULL_END
