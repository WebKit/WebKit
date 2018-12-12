/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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
} WK_API_AVAILABLE(macosx(10.10), ios(8.0));

typedef NS_OPTIONS(NSUInteger, _WKDebugOverlayRegions) {
    _WKNonFastScrollableRegion = 1 << 0,
    _WKWheelEventHandlerRegion = 1 << 1
} WK_API_AVAILABLE(macosx(10.11), ios(9.0));

typedef NS_OPTIONS(NSUInteger, _WKJavaScriptRuntimeFlags) {
    _WKJavaScriptRuntimeFlagsAllEnabled = 0
} WK_API_AVAILABLE(macosx(10.11), ios(9.0));

typedef NS_ENUM(NSInteger, _WKEditableLinkBehavior) {
    _WKEditableLinkBehaviorDefault,
    _WKEditableLinkBehaviorAlwaysLive,
    _WKEditableLinkBehaviorOnlyLiveWithShiftKey,
    _WKEditableLinkBehaviorLiveWhenNotFocused,
    _WKEditableLinkBehaviorNeverLive,
} WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));

@class _WKExperimentalFeature;
@class _WKInternalDebugFeature;

@interface WKPreferences () <NSCopying>
@end

@interface WKPreferences (WKPrivate)

// FIXME: This property should not have the verb "is" in it.
@property (nonatomic, setter=_setTelephoneNumberDetectionIsEnabled:) BOOL _telephoneNumberDetectionIsEnabled;
@property (nonatomic, setter=_setStorageBlockingPolicy:) _WKStorageBlockingPolicy _storageBlockingPolicy;

@property (nonatomic, setter=_setCompositingBordersVisible:) BOOL _compositingBordersVisible;
@property (nonatomic, setter=_setCompositingRepaintCountersVisible:) BOOL _compositingRepaintCountersVisible;
@property (nonatomic, setter=_setTiledScrollingIndicatorVisible:) BOOL _tiledScrollingIndicatorVisible;
@property (nonatomic, setter=_setResourceUsageOverlayVisible:) BOOL _resourceUsageOverlayVisible WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setVisibleDebugOverlayRegions:) _WKDebugOverlayRegions _visibleDebugOverlayRegions WK_API_AVAILABLE(macosx(10.11), ios(9.0));
@property (nonatomic, setter=_setSimpleLineLayoutEnabled:) BOOL _simpleLineLayoutEnabled WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setSimpleLineLayoutDebugBordersEnabled:) BOOL _simpleLineLayoutDebugBordersEnabled WK_API_AVAILABLE(macosx(10.11), ios(9.0));
@property (nonatomic, setter=_setAcceleratedDrawingEnabled:) BOOL _acceleratedDrawingEnabled WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setDisplayListDrawingEnabled:) BOOL _displayListDrawingEnabled WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setVisualViewportEnabled:) BOOL _visualViewportEnabled WK_API_AVAILABLE(macosx(10.12.3), ios(10.3));
@property (nonatomic, setter=_setLargeImageAsyncDecodingEnabled:) BOOL _largeImageAsyncDecodingEnabled WK_API_AVAILABLE(macosx(10.12.3), ios(10.3));
@property (nonatomic, setter=_setAnimatedImageAsyncDecodingEnabled:) BOOL _animatedImageAsyncDecodingEnabled WK_API_AVAILABLE(macosx(10.12.3), ios(10.3));
@property (nonatomic, setter=_setTextAutosizingEnabled:) BOOL _textAutosizingEnabled WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setSubpixelAntialiasedLayerTextEnabled:) BOOL _subpixelAntialiasedLayerTextEnabled WK_API_AVAILABLE(macosx(10.12), ios(10.0));

@property (nonatomic, setter=_setDeveloperExtrasEnabled:) BOOL _developerExtrasEnabled WK_API_AVAILABLE(macosx(10.11), ios(9.0));

@property (nonatomic, setter=_setLogsPageMessagesToSystemConsoleEnabled:) BOOL _logsPageMessagesToSystemConsoleEnabled WK_API_AVAILABLE(macosx(10.11), ios(9.0));

@property (nonatomic, setter=_setHiddenPageDOMTimerThrottlingEnabled:) BOOL _hiddenPageDOMTimerThrottlingEnabled WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setHiddenPageDOMTimerThrottlingAutoIncreases:) BOOL _hiddenPageDOMTimerThrottlingAutoIncreases WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setPageVisibilityBasedProcessSuppressionEnabled:) BOOL _pageVisibilityBasedProcessSuppressionEnabled WK_API_AVAILABLE(macosx(10.12), ios(10.0));

@property (nonatomic, setter=_setAllowFileAccessFromFileURLs:) BOOL _allowFileAccessFromFileURLs WK_API_AVAILABLE(macosx(10.11), ios(9.0));
@property (nonatomic, setter=_setJavaScriptRuntimeFlags:) _WKJavaScriptRuntimeFlags _javaScriptRuntimeFlags WK_API_AVAILABLE(macosx(10.11), ios(9.0));

@property (nonatomic, setter=_setStandalone:, getter=_isStandalone) BOOL _standalone WK_API_AVAILABLE(macosx(10.11), ios(9.0));

@property (nonatomic, setter=_setDiagnosticLoggingEnabled:) BOOL _diagnosticLoggingEnabled WK_API_AVAILABLE(macosx(10.11), ios(9.0));

@property (nonatomic, setter=_setDefaultFontSize:) NSUInteger _defaultFontSize WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, setter=_setDefaultFixedPitchFontSize:) NSUInteger _defaultFixedPitchFontSize WK_API_AVAILABLE(macosx(10.12), ios(10.0));
@property (nonatomic, copy, setter=_setFixedPitchFontFamily:) NSString *_fixedPitchFontFamily WK_API_AVAILABLE(macosx(10.12), ios(10.0));

// FIXME: This should be configured on the WKWebsiteDataStore.
// FIXME: This property should not have the verb "is" in it.
@property (nonatomic, setter=_setOfflineApplicationCacheIsEnabled:) BOOL _offlineApplicationCacheIsEnabled;
@property (nonatomic, setter=_setFullScreenEnabled:) BOOL _fullScreenEnabled WK_API_AVAILABLE(macosx(10.11), ios(9.0));
@property (nonatomic, setter=_setShouldSuppressKeyboardInputDuringProvisionalNavigation:) BOOL _shouldSuppressKeyboardInputDuringProvisionalNavigation WK_API_AVAILABLE(macosx(10.12.3), ios(10.3));
@property (nonatomic, setter=_setAllowsPictureInPictureMediaPlayback:) BOOL _allowsPictureInPictureMediaPlayback WK_API_AVAILABLE(macosx(10.13), ios(11.0));

@property (nonatomic, setter=_setApplePayCapabilityDisclosureAllowed:) BOOL _applePayCapabilityDisclosureAllowed WK_API_AVAILABLE(macosx(10.12), ios(10.0));

@property (nonatomic, setter=_setLoadsImagesAutomatically:) BOOL _loadsImagesAutomatically WK_API_AVAILABLE(macosx(10.13), ios(11.0));

@property (nonatomic, setter=_setPeerConnectionEnabled:) BOOL _peerConnectionEnabled WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));
@property (nonatomic, setter=_setMediaDevicesEnabled:) BOOL _mediaDevicesEnabled WK_API_AVAILABLE(macosx(10.13), ios(11.0));
@property (nonatomic, setter=_setScreenCaptureEnabled:) BOOL _screenCaptureEnabled WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));
@property (nonatomic, setter=_setMockCaptureDevicesEnabled:) BOOL _mockCaptureDevicesEnabled WK_API_AVAILABLE(macosx(10.13), ios(11.0));
@property (nonatomic, setter=_setMockCaptureDevicesPromptEnabled:) BOOL _mockCaptureDevicesPromptEnabled WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));
@property (nonatomic, setter=_setMediaCaptureRequiresSecureConnection:) BOOL _mediaCaptureRequiresSecureConnection WK_API_AVAILABLE(macosx(10.13), ios(11.0));
@property (nonatomic, setter=_setEnumeratingAllNetworkInterfacesEnabled:) BOOL _enumeratingAllNetworkInterfacesEnabled WK_API_AVAILABLE(macosx(10.13), ios(11.0));
@property (nonatomic, setter=_setICECandidateFilteringEnabled:) BOOL _iceCandidateFilteringEnabled WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));
@property (nonatomic, setter=_setWebRTCLegacyAPIEnabled:) BOOL _webRTCLegacyAPIEnabled WK_API_AVAILABLE(macosx(10.13), ios(11.0));
@property (nonatomic, setter=_setInactiveMediaCaptureSteamRepromptIntervalInMinutes:) double _inactiveMediaCaptureSteamRepromptIntervalInMinutes WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));

@property (nonatomic, setter=_setJavaScriptCanAccessClipboard:) BOOL _javaScriptCanAccessClipboard WK_API_AVAILABLE(macosx(10.13), ios(11.0));
@property (nonatomic, setter=_setDOMPasteAllowed:) BOOL _domPasteAllowed WK_API_AVAILABLE(macosx(10.13), ios(11.0));

@property (nonatomic, setter=_setShouldAllowUserInstalledFonts:) BOOL _shouldAllowUserInstalledFonts WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));

@property (nonatomic, setter=_setEditableLinkBehavior:) _WKEditableLinkBehavior _editableLinkBehavior WK_API_AVAILABLE(macosx(10.13.4), ios(11.3));

@property (nonatomic, setter=_setAVFoundationEnabled:) BOOL _avFoundationEnabled WK_API_AVAILABLE(macosx(10.10), ios(12.0));

+ (NSArray<_WKInternalDebugFeature *> *)_internalDebugFeatures WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));
- (BOOL)_isEnabledForInternalDebugFeature:(_WKInternalDebugFeature *)feature WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));
- (void)_setEnabled:(BOOL)value forInternalDebugFeature:(_WKInternalDebugFeature *)feature WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));

+ (NSArray<_WKExperimentalFeature *> *)_experimentalFeatures WK_API_AVAILABLE(macosx(10.12), ios(10.0));
- (BOOL)_isEnabledForFeature:(_WKExperimentalFeature *)feature WK_API_AVAILABLE(macosx(10.12), ios(10.0));
- (void)_setEnabled:(BOOL)value forFeature:(_WKExperimentalFeature *)feature WK_API_AVAILABLE(macosx(10.12), ios(10.0));
- (BOOL)_isEnabledForExperimentalFeature:(_WKExperimentalFeature *)feature WK_API_AVAILABLE(macosx(10.12), ios(10.0));
- (void)_setEnabled:(BOOL)value forExperimentalFeature:(_WKExperimentalFeature *)feature WK_API_AVAILABLE(macosx(10.12), ios(10.0));

@property (nonatomic, setter=_setShouldEnableTextAutosizingBoost:) BOOL _shouldEnableTextAutosizingBoost WK_API_AVAILABLE(macosx(10.14), ios(12.0));

@property (nonatomic, setter=_setStorageAccessPromptsEnabled:) BOOL _storageAccessPromptsEnabled WK_API_AVAILABLE(macosx(10.14), ios(12.0));
@property (nonatomic, setter=_setColorFilterEnabled:) BOOL _colorFilterEnabled WK_API_AVAILABLE(macosx(10.14), ios(12.0));
@property (nonatomic, setter=_setPunchOutWhiteBackgroundsInDarkMode:) BOOL _punchOutWhiteBackgroundsInDarkMode WK_API_AVAILABLE(macosx(10.14), ios(12.0));
@property (nonatomic, setter=_setLowPowerVideoAudioBufferSizeEnabled:) BOOL _lowPowerVideoAudioBufferSizeEnabled WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setShouldIgnoreMetaViewport:) BOOL _shouldIgnoreMetaViewport WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setVideoQualityIncludesDisplayCompositingEnabled:) BOOL _videoQualityIncludesDisplayCompositingEnabled WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setWebAnimationsCSSIntegrationEnabled:) BOOL _webAnimationsCSSIntegrationEnabled WK_API_AVAILABLE(macosx(10.14), ios(WK_IOS_TBA));
@property (nonatomic, setter=_setDeviceOrientationEventEnabled:) BOOL _deviceOrientationEventEnabled WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA));

#if !TARGET_OS_IPHONE
@property (nonatomic, setter=_setWebGLEnabled:) BOOL _webGLEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setJavaEnabledForLocalFiles:) BOOL _javaEnabledForLocalFiles WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setCanvasUsesAcceleratedDrawing:) BOOL _canvasUsesAcceleratedDrawing WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setAcceleratedCompositingEnabled:) BOOL _acceleratedCompositingEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setDefaultTextEncodingName:) NSString *_defaultTextEncodingName WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setNeedsSiteSpecificQuirks:) BOOL _needsSiteSpecificQuirks WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setAuthorAndUserStylesEnabled:) BOOL _authorAndUserStylesEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setDOMTimersThrottlingEnabled:) BOOL _domTimersThrottlingEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setWebArchiveDebugModeEnabled:) BOOL _webArchiveDebugModeEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setLocalFileContentSniffingEnabled:) BOOL _localFileContentSniffingEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setUsesPageCache:) BOOL _usesPageCache WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setPageCacheSupportsPlugins:) BOOL _pageCacheSupportsPlugins WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setShouldPrintBackgrounds:) BOOL _shouldPrintBackgrounds WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setWebSecurityEnabled:) BOOL _webSecurityEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setUniversalAccessFromFileURLsAllowed:) BOOL _universalAccessFromFileURLsAllowed WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setSuppressesIncrementalRendering:) BOOL _suppressesIncrementalRendering WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setAsynchronousPluginInitializationEnabled:) BOOL _asynchronousPluginInitializationEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setArtificialPluginInitializationDelayEnabled:) BOOL _artificialPluginInitializationDelayEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setExperimentalPlugInSandboxProfilesEnabled:) BOOL _experimentalPlugInSandboxProfilesEnabled WK_API_AVAILABLE(macosx(WK_MAC_TBA));
@property (nonatomic, setter=_setCookieEnabled:) BOOL _cookieEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setPlugInSnapshottingEnabled:) BOOL _plugInSnapshottingEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setSubpixelCSSOMElementMetricsEnabled:) BOOL _subpixelCSSOMElementMetricsEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setMediaSourceEnabled:) BOOL _mediaSourceEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setViewGestureDebuggingEnabled:) BOOL _viewGestureDebuggingEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setStandardFontFamily:) NSString *_standardFontFamily WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setNotificationsEnabled:) BOOL _notificationsEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setBackspaceKeyNavigationEnabled:) BOOL _backspaceKeyNavigationEnabled WK_API_AVAILABLE(macosx(10.13.4));
@property (nonatomic, setter=_setAllowsInlineMediaPlayback:) BOOL _allowsInlineMediaPlayback WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setApplePayEnabled:) BOOL _applePayEnabled WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setDNSPrefetchingEnabled:) BOOL _dnsPrefetchingEnabled WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setInlineMediaPlaybackRequiresPlaysInlineAttribute:) BOOL _inlineMediaPlaybackRequiresPlaysInlineAttribute WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setInvisibleMediaAutoplayNotPermitted:) BOOL _invisibleMediaAutoplayNotPermitted WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setLegacyEncryptedMediaAPIEnabled:) BOOL _legacyEncryptedMediaAPIEnabled WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setMainContentUserGestureOverrideEnabled:) BOOL _mainContentUserGestureOverrideEnabled WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setMediaStreamEnabled:) BOOL _mediaStreamEnabled WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setNeedsStorageAccessFromFileURLsQuirk:) BOOL _needsStorageAccessFromFileURLsQuirk WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setPDFPluginEnabled:) BOOL _pdfPluginEnabled WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setRequiresUserGestureForAudioPlayback:) BOOL _requiresUserGestureForAudioPlayback WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setRequiresUserGestureForVideoPlayback:) BOOL _requiresUserGestureForVideoPlayback WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setServiceControlsEnabled:) BOOL _serviceControlsEnabled WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setShowsToolTipOverTruncatedText:) BOOL _showsToolTipOverTruncatedText WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setTextAreasAreResizable:) BOOL _textAreasAreResizable WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setUseGiantTiles:) BOOL _useGiantTiles WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setWantsBalancedSetDefersLoadingBehavior:) BOOL _wantsBalancedSetDefersLoadingBehavior WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setWebAudioEnabled:) BOOL _webAudioEnabled WK_API_AVAILABLE(macosx(10.14));
@property (nonatomic, setter=_setAggressiveTileRetentionEnabled:) BOOL _aggressiveTileRetentionEnabled WK_API_AVAILABLE(macosx(10.14));
#endif

@end

#endif

#endif
