/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

NS_ASSUME_NONNULL_BEGIN

@class _WKUserContentWorld;
@class WKWebView;

typedef NS_ENUM(NSInteger, _WKUserStyleLevel) {
    _WKUserStyleUserLevel,
    _WKUserStyleAuthorLevel
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

WK_CLASS_AVAILABLE(macos(10.12), ios(10.0))
@interface _WKUserStyleSheet : NSObject <NSCopying>

@property (nonatomic, readonly, copy) NSString *source;

@property (nonatomic, readonly, copy) NSURL *baseURL;

@property (nonatomic, readonly, getter=isForMainFrameOnly) BOOL forMainFrameOnly;

- (instancetype)initWithSource:(NSString *)source forMainFrameOnly:(BOOL)forMainFrameOnly;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
- (instancetype)initWithSource:(NSString *)source forWKWebView:(WKWebView *)webView forMainFrameOnly:(BOOL)forMainFrameOnly level:(_WKUserStyleLevel)level userContentWorld:(_WKUserContentWorld *)userContentWorld WK_API_DEPRECATED_WITH_REPLACEMENT("_initWithSource:forWKWebView:forMainFrameOnly:includeMatchPatternStrings:excludeMatchPatternStrings:baseURL:level:contentWorld:", macos(WK_MAC_TBA, WK_MAC_TBA), ios(WK_IOS_TBA, WK_IOS_TBA));
- (instancetype)initWithSource:(NSString *)source forWKWebView:(WKWebView *)webView forMainFrameOnly:(BOOL)forMainFrameOnly baseURL:(NSURL *)baseURL level:(_WKUserStyleLevel)level userContentWorld:(_WKUserContentWorld *)userContentWorld WK_API_DEPRECATED_WITH_REPLACEMENT("_initWithSource:forWKWebView:forMainFrameOnly:includeMatchPatternStrings:excludeMatchPatternStrings:baseURL:level:contentWorld:", macos(WK_MAC_TBA, WK_MAC_TBA), ios(WK_IOS_TBA, WK_IOS_TBA));
- (instancetype)initWithSource:(NSString *)source forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray<NSString *> *)legacyWhitelist legacyBlacklist:(NSArray<NSString *> *)legacyBlacklist userContentWorld:(_WKUserContentWorld *)userContentWorld WK_API_DEPRECATED_WITH_REPLACEMENT("_initWithSource:forWKWebView:forMainFrameOnly:includeMatchPatternStrings:excludeMatchPatternStrings:baseURL:level:contentWorld:", macos(10.12, WK_MAC_TBA), ios(10.0, WK_IOS_TBA));
- (instancetype)initWithSource:(NSString *)source forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray<NSString *> *)legacyWhitelist legacyBlacklist:(NSArray<NSString *> *)legacyBlacklist baseURL:(NSURL *)baseURL userContentWorld:(_WKUserContentWorld *)userContentWorld WK_API_DEPRECATED_WITH_REPLACEMENT("_initWithSource:forWKWebView:forMainFrameOnly:includeMatchPatternStrings:excludeMatchPatternStrings:baseURL:level:contentWorld:", macos(10.12, WK_MAC_TBA), ios(10.0, WK_IOS_TBA));
- (instancetype)initWithSource:(NSString *)source forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray<NSString *> *)legacyWhitelist legacyBlacklist:(NSArray<NSString *> *)legacyBlacklist baseURL:(NSURL *)baseURL level:(_WKUserStyleLevel)level userContentWorld:(_WKUserContentWorld *)userContentWorld WK_API_DEPRECATED_WITH_REPLACEMENT("_initWithSource:forWKWebView:forMainFrameOnly:includeMatchPatternStrings:excludeMatchPatternStrings:baseURL:level:contentWorld:", macos(WK_MAC_TBA, WK_MAC_TBA), ios(WK_IOS_TBA, WK_IOS_TBA));
#pragma clang diagnostic pop

// _WKUserStyleUserLevel was the default
- (instancetype)initWithSource:(NSString *)source forWKWebView:(nullable WKWebView *)webView forMainFrameOnly:(BOOL)forMainFrameOnly includeMatchPatternStrings:(nullable NSArray<NSString *> *)includeMatchPatternStrings excludeMatchPatternStrings:(nullable NSArray<NSString *> *)excludeMatchPatternStrings baseURL:(nullable NSURL *)baseURL level:(_WKUserStyleLevel)level contentWorld:(nullable WKContentWorld *)contentWorld WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

@end

NS_ASSUME_NONNULL_END
