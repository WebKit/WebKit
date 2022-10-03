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

NS_ASSUME_NONNULL_BEGIN

WK_EXTERN NSErrorDomain const _WKWebExtensionMatchPatternErrorDomain WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

typedef NS_ERROR_ENUM(_WKWebExtensionMatchPatternErrorDomain, _WKWebExtensionMatchPatternError) {
    _WKWebExtensionMatchPatternErrorUnknown,
    _WKWebExtensionMatchPatternErrorInvalidScheme,
    _WKWebExtensionMatchPatternErrorInvalidHost,
    _WKWebExtensionMatchPatternErrorInvalidPath,
} NS_SWIFT_NAME(_WKWebExtensionMatchPattern.Error) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

typedef NS_OPTIONS(NSUInteger, _WKWebExtensionMatchPatternOptions) {
    _WKWebExtensionMatchPatternOptionsNone                 = 0,
    _WKWebExtensionMatchPatternOptionsIgnoreSchemes        = 1 << 0,
    _WKWebExtensionMatchPatternOptionsIgnorePaths          = 1 << 1,
    _WKWebExtensionMatchPatternOptionsMatchBidirectionally = 1 << 2,
} NS_SWIFT_NAME(_WKWebExtensionMatchPattern.Options) WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKWebExtensionMatchPattern : NSObject <NSSecureCoding, NSCopying>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

+ (instancetype)allURLsMatchPattern;
+ (instancetype)allHostsAndSchemesMatchPattern;

+ (nullable instancetype)matchPatternWithString:(NSString *)string NS_SWIFT_UNAVAILABLE("Use error version");
+ (nullable instancetype)matchPatternWithScheme:(NSString *)scheme host:(NSString *)host path:(NSString *)path NS_SWIFT_UNAVAILABLE("Use error version");

- (nullable instancetype)initWithString:(NSString *)string NS_SWIFT_UNAVAILABLE("Use error version");
- (nullable instancetype)initWithString:(NSString *)string error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (nullable instancetype)initWithScheme:(NSString *)scheme host:(NSString *)host path:(NSString *)path NS_SWIFT_UNAVAILABLE("Use error version");
- (nullable instancetype)initWithScheme:(NSString *)scheme host:(NSString *)host path:(NSString *)path error:(NSError **)error NS_DESIGNATED_INITIALIZER;

@property (nonatomic, readonly) NSString *string;

@property (nonatomic, nullable, readonly) NSString *scheme;
@property (nonatomic, nullable, readonly) NSString *host;
@property (nonatomic, nullable, readonly) NSString *path;

@property (nonatomic, readonly) BOOL matchesAllURLs;
@property (nonatomic, readonly) BOOL matchesAllHosts;

- (BOOL)matchesURL:(NSURL *)url NS_SWIFT_NAME(matches(url:));
- (BOOL)matchesURL:(NSURL *)url options:(_WKWebExtensionMatchPatternOptions)options NS_SWIFT_NAME(matches(url:options:));

- (BOOL)matchesPattern:(_WKWebExtensionMatchPattern *)pattern NS_SWIFT_NAME(matches(pattern:));
- (BOOL)matchesPattern:(_WKWebExtensionMatchPattern *)pattern options:(_WKWebExtensionMatchPatternOptions)options NS_SWIFT_NAME(matches(pattern:options:));

@end

NS_ASSUME_NONNULL_END
