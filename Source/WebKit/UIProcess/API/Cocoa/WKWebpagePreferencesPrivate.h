/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#import <WebKit/WKFoundation.h>
#import <WebKit/WKWebpagePreferences.h>
#import <WebKit/WKWebsiteDataStore.h>

typedef NS_ENUM(NSInteger, _WKWebsiteAutoplayPolicy) {
    _WKWebsiteAutoplayPolicyDefault,
    _WKWebsiteAutoplayPolicyAllow,
    _WKWebsiteAutoplayPolicyAllowWithoutSound,
    _WKWebsiteAutoplayPolicyDeny
} WK_API_AVAILABLE(macos(10.13), ios(11.0));

typedef NS_OPTIONS(NSUInteger, _WKWebsiteAutoplayQuirk) {
    _WKWebsiteAutoplayQuirkSynthesizedPauseEvents = 1 << 0,
    _WKWebsiteAutoplayQuirkInheritedUserGestures = 1 << 1,
    _WKWebsiteAutoplayQuirkArbitraryUserGestures = 1 << 2,
    _WKWebsiteAutoplayQuirkPerDocumentAutoplayBehavior = 1 << 3,
} WK_API_AVAILABLE(macos(10.13), ios(11.0));

typedef NS_OPTIONS(NSUInteger, _WKWebsitePopUpPolicy) {
    _WKWebsitePopUpPolicyDefault,
    _WKWebsitePopUpPolicyAllow,
    _WKWebsitePopUpPolicyBlock,
} WK_API_AVAILABLE(macos(10.14), ios(12.0));

typedef NS_OPTIONS(NSUInteger, _WKWebsiteDeviceOrientationAndMotionAccessPolicy) {
    _WKWebsiteDeviceOrientationAndMotionAccessPolicyAsk,
    _WKWebsiteDeviceOrientationAndMotionAccessPolicyGrant,
    _WKWebsiteDeviceOrientationAndMotionAccessPolicyDeny,
} WK_API_AVAILABLE(macos(10.14), ios(12.0));

@class _WKCustomHeaderFields;

@interface WKWebpagePreferences (WKPrivate)

@property (nonatomic, setter=_setContentBlockersEnabled:) BOOL _contentBlockersEnabled;
@property (nonatomic, setter=_setAllowedAutoplayQuirks:) _WKWebsiteAutoplayQuirk _allowedAutoplayQuirks;
@property (nonatomic, setter=_setAutoplayPolicy:) _WKWebsiteAutoplayPolicy _autoplayPolicy;
@property (nonatomic, copy, setter=_setCustomHeaderFields:) NSArray<_WKCustomHeaderFields *> *_customHeaderFields;
@property (nonatomic, setter=_setPopUpPolicy:) _WKWebsitePopUpPolicy _popUpPolicy;
@property (nonatomic, strong, setter=_setWebsiteDataStore:) WKWebsiteDataStore *_websiteDataStore;
@property (nonatomic, copy, setter=_setCustomUserAgent:) NSString *_customUserAgent;
@property (nonatomic, copy, setter=_setCustomJavaScriptUserAgentAsSiteSpecificQuirks:) NSString *_customJavaScriptUserAgentAsSiteSpecificQuirks;
@property (nonatomic, copy, setter=_setCustomNavigatorPlatform:) NSString *_customNavigatorPlatform;
@property (nonatomic, setter=_setDeviceOrientationAndMotionAccessPolicy:) _WKWebsiteDeviceOrientationAndMotionAccessPolicy _deviceOrientationAndMotionAccessPolicy;
@property (nonatomic, setter=_setAllowSiteSpecificQuirksToOverrideCompatibilityMode:) BOOL _allowSiteSpecificQuirksToOverrideCompatibilityMode;

@property (nonatomic, copy, setter=_setApplicationNameForUserAgentWithModernCompatibility:) NSString *_applicationNameForUserAgentWithModernCompatibility;

@end
