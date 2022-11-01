/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKit/WKFoundation.h>

#if TARGET_OS_IPHONE
@class UIColor;
#else
@class NSColor;
#endif
@class _WKApplicationManifestIcon;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, _WKApplicationManifestDisplayMode) {
    _WKApplicationManifestDisplayModeBrowser,
    _WKApplicationManifestDisplayModeMinimalUI,
    _WKApplicationManifestDisplayModeStandalone,
    _WKApplicationManifestDisplayModeFullScreen,
} WK_API_AVAILABLE(macos(10.13.4), ios(11.3));

typedef NS_ENUM(NSInteger, _WKApplicationManifestIconPurpose) {
    _WKApplicationManifestIconPurposeAny = (1 << 0),
    _WKApplicationManifestIconPurposeMonochrome = (1 << 1),
    _WKApplicationManifestIconPurposeMaskable = (1 << 2),
} WK_API_AVAILABLE(macos(13.0), ios(16.0));

WK_CLASS_AVAILABLE(macos(10.13.4), ios(11.3))
@interface _WKApplicationManifest : NSObject <NSSecureCoding>

@property (nonatomic, readonly, nullable, copy) NSString *name;
@property (nonatomic, readonly, nullable, copy) NSString *shortName;
@property (nonatomic, readonly, nullable, copy) NSString *applicationDescription;
@property (nonatomic, readonly, nullable, copy) NSURL *scope;
@property (nonatomic, readonly, copy) NSURL *startURL;
@property (nonatomic, readonly, copy) NSURL *manifestId WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, readonly) _WKApplicationManifestDisplayMode displayMode;
@property (nonatomic, readonly, copy) NSArray<_WKApplicationManifestIcon *> *icons WK_API_AVAILABLE(macos(13.0), ios(16.0));

#if TARGET_OS_IPHONE
@property (nonatomic, readonly, nullable, copy) UIColor *themeColor WK_API_AVAILABLE(ios(15.0));
#else
@property (nonatomic, readonly, nullable, copy) NSColor *themeColor WK_API_AVAILABLE(macos(12.0));
#endif

+ (_WKApplicationManifest *)applicationManifestFromJSON:(NSString *)json manifestURL:(nullable NSURL *)manifestURL documentURL:(nullable NSURL *)documentURL;

@end

WK_CLASS_AVAILABLE(macos(13.0), ios(16.0))
@interface _WKApplicationManifestIcon : NSObject <NSSecureCoding>

@property (nonatomic, readonly, copy) NSURL *src;
@property (nonatomic, readonly, copy) NSArray<NSString *> *sizes;
@property (nonatomic, readonly, copy) NSString *type;
@property (nonatomic, readonly) NSArray<NSNumber *> *purposes;

@end

NS_ASSUME_NONNULL_END
