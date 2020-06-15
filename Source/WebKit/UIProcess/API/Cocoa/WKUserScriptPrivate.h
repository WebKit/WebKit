/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#import <WebKit/WKUserScript.h>

NS_ASSUME_NONNULL_BEGIN

@class WKContentWorld;
@class _WKUserContentWorld;

@interface WKUserScript (WKPrivate)

- (instancetype)_initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray<NSString *> *)legacyWhitelist legacyBlacklist:(NSArray<NSString *> *)legacyBlacklist userContentWorld:(_WKUserContentWorld *)userContentWorld WK_API_DEPRECATED_WITH_REPLACEMENT("_initWithSource:injectionTime:forMainFrameOnly:includeMatchPatternStrings:excludeMatchPatternStrings:associatedURL:contentWorld:deferRunningUntilNotification:", macos(10.12, WK_MAC_TBA), ios(10.0, WK_IOS_TBA));
- (instancetype)_initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray<NSString *> *)legacyWhitelist legacyBlacklist:(NSArray<NSString *> *)legacyBlacklist associatedURL:(NSURL *)associatedURL userContentWorld:(_WKUserContentWorld *)userContentWorld WK_API_DEPRECATED_WITH_REPLACEMENT("_initWithSource:injectionTime:forMainFrameOnly:includeMatchPatternStrings:excludeMatchPatternStrings:associatedURL:contentWorld:deferRunningUntilNotification:", macos(10.12, WK_MAC_TBA), ios(10.0, WK_IOS_TBA));

- (instancetype)_initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray<NSString *> *)legacyWhitelist legacyBlacklist:(NSArray<NSString *> *)legacyBlacklist contentWorld:(WKContentWorld *)contentWorld WK_API_DEPRECATED_WITH_REPLACEMENT("_initWithSource:injectionTime:forMainFrameOnly:includeMatchPatternStrings:excludeMatchPatternStrings:associatedURL:contentWorld:deferRunningUntilNotification:", macos(WK_MAC_TBA, WK_MAC_TBA), ios(WK_IOS_TBA, WK_IOS_TBA));
- (instancetype)_initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray<NSString *> *)legacyWhitelist legacyBlacklist:(NSArray<NSString *> *)legacyBlacklist associatedURL:(NSURL *)associatedURL contentWorld:(WKContentWorld *)contentWorld WK_API_DEPRECATED_WITH_REPLACEMENT("_initWithSource:injectionTime:forMainFrameOnly:includeMatchPatternStrings:excludeMatchPatternStrings:associatedURL:contentWorld:deferRunningUntilNotification:", macos(WK_MAC_TBA, WK_MAC_TBA), ios(WK_IOS_TBA, WK_IOS_TBA));

- (instancetype)_initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray<NSString *> *)legacyWhitelist legacyBlacklist:(NSArray<NSString *> *)legacyBlacklist associatedURL:(NSURL *)associatedURL contentWorld:(WKContentWorld *)contentWorld deferRunningUntilNotification:(BOOL)deferRunningUntilNotification WK_API_DEPRECATED_WITH_REPLACEMENT("_initWithSource:injectionTime:forMainFrameOnly:includeMatchPatternStrings:excludeMatchPatternStrings:associatedURL:contentWorld:deferRunningUntilNotification:", macos(WK_MAC_TBA, WK_MAC_TBA), ios(WK_IOS_TBA, WK_IOS_TBA));

- (instancetype)_initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly includeMatchPatternStrings:(nullable NSArray<NSString *> *)includeMatchPatternStrings excludeMatchPatternStrings:(nullable NSArray<NSString *> *)excludeMatchPatternStrings associatedURL:(nullable NSURL *)associatedURL contentWorld:(nullable WKContentWorld *)contentWorld deferRunningUntilNotification:(BOOL)deferRunningUntilNotification WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

@property (nonatomic, readonly) _WKUserContentWorld *_userContentWorld WK_API_DEPRECATED_WITH_REPLACEMENT("_contentWorld", macos(10.12, WK_MAC_TBA), ios(10.0, WK_IOS_TBA));
@property (nonatomic, readonly) WKContentWorld *_contentWorld WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

@end

NS_ASSUME_NONNULL_END
