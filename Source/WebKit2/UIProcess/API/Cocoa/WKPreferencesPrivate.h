/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include <WebKit/WKPreferencesRefPrivate.h>

#ifdef __OBJC__

#import <WebKit/WKPreferences.h>

#if WK_API_ENABLED

typedef NS_ENUM(NSInteger, _WKStorageBlockingPolicy) {
    _WKStorageBlockingPolicyAllowAll,
    _WKStorageBlockingPolicyBlockThirdParty,
    _WKStorageBlockingPolicyBlockAll,
} WK_ENUM_AVAILABLE(10_10, 8_0);

typedef NS_OPTIONS(NSUInteger, _WKDebugOverlayRegions) {
    _WKNonFastScrollableRegion = 1 << 0,
    _WKWheelEventHandlerRegion = 1 << 1
} WK_ENUM_AVAILABLE(10_11, 9_0);

typedef NS_OPTIONS(NSUInteger, _WKJavaScriptRuntimeFlags) {
    _WKJavaScriptRuntimeFlagsAllEnabled = 0
} WK_ENUM_AVAILABLE(10_11, 9_0);

@interface WKPreferences (WKPrivate)

// FIXME: This property should not have the verb "is" in it.
@property (nonatomic, setter=_setTelephoneNumberDetectionIsEnabled:) BOOL _telephoneNumberDetectionIsEnabled;
@property (nonatomic, setter=_setStorageBlockingPolicy:) _WKStorageBlockingPolicy _storageBlockingPolicy;

@property (nonatomic, setter=_setCompositingBordersVisible:) BOOL _compositingBordersVisible;
@property (nonatomic, setter=_setCompositingRepaintCountersVisible:) BOOL _compositingRepaintCountersVisible;
@property (nonatomic, setter=_setTiledScrollingIndicatorVisible:) BOOL _tiledScrollingIndicatorVisible;
@property (nonatomic, setter=_setResourceUsageOverlayVisible:) BOOL _resourceUsageOverlayVisible WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, setter=_setVisibleDebugOverlayRegions:) _WKDebugOverlayRegions _visibleDebugOverlayRegions WK_AVAILABLE(10_11, 9_0);
@property (nonatomic, setter=_setSimpleLineLayoutDebugBordersEnabled:) BOOL _simpleLineLayoutDebugBordersEnabled WK_AVAILABLE(10_11, 9_0);
@property (nonatomic, setter=_setAcceleratedDrawingEnabled:) BOOL _acceleratedDrawingEnabled WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, setter=_setDisplayListDrawingEnabled:) BOOL _displayListDrawingEnabled WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

@property (nonatomic, setter=_setDeveloperExtrasEnabled:) BOOL _developerExtrasEnabled WK_AVAILABLE(10_11, 9_0);

@property (nonatomic, setter=_setLogsPageMessagesToSystemConsoleEnabled:) BOOL _logsPageMessagesToSystemConsoleEnabled WK_AVAILABLE(10_11, 9_0);

@property (nonatomic, setter=_setHiddenPageDOMTimerThrottlingEnabled:) BOOL _hiddenPageDOMTimerThrottlingEnabled WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, setter=_setPageVisibilityBasedProcessSuppressionEnabled:) BOOL _pageVisibilityBasedProcessSuppressionEnabled WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

@property (nonatomic, setter=_setAllowFileAccessFromFileURLs:) BOOL _allowFileAccessFromFileURLs WK_AVAILABLE(10_11, 9_0);
@property (nonatomic, setter=_setJavaScriptRuntimeFlags:) _WKJavaScriptRuntimeFlags _javaScriptRuntimeFlags WK_AVAILABLE(10_11, 9_0);

@property (nonatomic, setter=_setStandalone:, getter=_isStandalone) BOOL _standalone WK_AVAILABLE(10_11, 9_0);

@property (nonatomic, setter=_setDiagnosticLoggingEnabled:) BOOL _diagnosticLoggingEnabled WK_AVAILABLE(10_11, 9_0);

@property (nonatomic, setter=_setAntialiasedFontDilationEnabled:) BOOL _antialiasedFontDilationEnabled WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

@property (nonatomic, setter=_setDefaultFontSize:) NSUInteger _defaultFontSize WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, setter=_setDefaultFixedPitchFontSize:) NSUInteger _defaultFixedPitchFontSize WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
@property (nonatomic, copy, setter=_setFixedPitchFontFamily:) NSString *_fixedPitchFontFamily WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);

// FIXME: This should be configured on the WKWebsiteDataStore.
// FIXME: This property should not have the verb "is" in it.
@property (nonatomic, setter=_setOfflineApplicationCacheIsEnabled:) BOOL _offlineApplicationCacheIsEnabled;
@property (nonatomic, setter=_setFullScreenEnabled:) BOOL _fullScreenEnabled WK_AVAILABLE(10_11, 9_0);

@end

#endif

#endif
