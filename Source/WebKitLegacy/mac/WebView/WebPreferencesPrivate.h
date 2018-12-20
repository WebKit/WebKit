/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKitLegacy/WebPreferences.h>

#if !TARGET_OS_IPHONE
#import <Quartz/Quartz.h>
#endif

typedef enum {
    WebKitEditableLinkDefaultBehavior,
    WebKitEditableLinkAlwaysLive,
    WebKitEditableLinkOnlyLiveWithShiftKey,
    WebKitEditableLinkLiveWhenNotFocused,
    WebKitEditableLinkNeverLive
} WebKitEditableLinkBehavior;

typedef enum {
    WebTextDirectionSubmenuNeverIncluded,
    WebTextDirectionSubmenuAutomaticallyIncluded,
    WebTextDirectionSubmenuAlwaysIncluded
} WebTextDirectionSubmenuInclusionBehavior;

typedef enum {
    WebAllowAllStorage = 0,
    WebBlockThirdPartyStorage,
    WebBlockAllStorage
} WebStorageBlockingPolicy;

typedef enum {
    WebKitJavaScriptRuntimeFlagsAllEnabled = 0
} WebKitJavaScriptRuntimeFlags;

typedef enum {
    WebKitFrameFlatteningDisabled,
    WebKitFrameFlatteningEnabledForNonFullScreenIFrames,
    WebKitFrameFlatteningFullyEnabled
} WebKitFrameFlattening;

extern NSString *WebPreferencesChangedNotification WEBKIT_DEPRECATED_MAC(10_3, 10_14);
extern NSString *WebPreferencesRemovedNotification WEBKIT_DEPRECATED_MAC(10_3, 10_14);
extern NSString *WebPreferencesChangedInternalNotification WEBKIT_DEPRECATED_MAC(10_3, 10_14);
extern NSString *WebPreferencesCacheModelChangedInternalNotification WEBKIT_DEPRECATED_MAC(10_5, 10_14);

@interface WebPreferences (WebPrivate)

// Preferences that might be public in a future release

- (BOOL)isDNSPrefetchingEnabled;
- (void)setDNSPrefetchingEnabled:(BOOL)flag;

- (BOOL)developerExtrasEnabled;
- (void)setDeveloperExtrasEnabled:(BOOL)flag;

- (WebKitJavaScriptRuntimeFlags)javaScriptRuntimeFlags;
- (void)setJavaScriptRuntimeFlags:(WebKitJavaScriptRuntimeFlags)flags;

- (BOOL)authorAndUserStylesEnabled;
- (void)setAuthorAndUserStylesEnabled:(BOOL)flag;

- (BOOL)applicationChromeModeEnabled;
- (void)setApplicationChromeModeEnabled:(BOOL)flag;

- (BOOL)usesEncodingDetector;
- (void)setUsesEncodingDetector:(BOOL)flag;

#if !TARGET_OS_IPHONE
- (BOOL)respectStandardStyleKeyEquivalents;
- (void)setRespectStandardStyleKeyEquivalents:(BOOL)flag;

- (BOOL)showsURLsInToolTips;
- (void)setShowsURLsInToolTips:(BOOL)flag;

- (BOOL)showsToolTipOverTruncatedText;
- (void)setShowsToolTipOverTruncatedText:(BOOL)flag;

- (BOOL)textAreasAreResizable;
- (void)setTextAreasAreResizable:(BOOL)flag;

- (PDFDisplayMode)PDFDisplayMode;
- (void)setPDFDisplayMode:(PDFDisplayMode)mode;
#endif

- (BOOL)shrinksStandaloneImagesToFit;
- (void)setShrinksStandaloneImagesToFit:(BOOL)flag;

- (BOOL)automaticallyDetectsCacheModel;
- (void)setAutomaticallyDetectsCacheModel:(BOOL)automaticallyDetectsCacheModel;

- (BOOL)domTimersThrottlingEnabled;
- (void)setDOMTimersThrottlingEnabled:(BOOL)domTimersThrottlingEnabled;

- (BOOL)webArchiveDebugModeEnabled;
- (void)setWebArchiveDebugModeEnabled:(BOOL)webArchiveDebugModeEnabled;

- (BOOL)localFileContentSniffingEnabled;
- (void)setLocalFileContentSniffingEnabled:(BOOL)localFileContentSniffingEnabled;

- (BOOL)offlineWebApplicationCacheEnabled;
- (void)setOfflineWebApplicationCacheEnabled:(BOOL)offlineWebApplicationCacheEnabled;

- (BOOL)databasesEnabled;
- (void)setDatabasesEnabled:(BOOL)databasesEnabled;

#if TARGET_OS_IPHONE
- (BOOL)storageTrackerEnabled;
- (void)setStorageTrackerEnabled:(BOOL)storageTrackerEnabled;
#endif

- (BOOL)localStorageEnabled;
- (void)setLocalStorageEnabled:(BOOL)localStorageEnabled;

- (BOOL)isWebSecurityEnabled;
- (void)setWebSecurityEnabled:(BOOL)flag;

- (BOOL)allowUniversalAccessFromFileURLs;
- (void)setAllowUniversalAccessFromFileURLs:(BOOL)flag;

- (BOOL)allowFileAccessFromFileURLs;
- (void)setAllowFileAccessFromFileURLs:(BOOL)flag;

- (BOOL)allowCrossOriginSubresourcesToAskForCredentials;
- (void)setAllowCrossOriginSubresourcesToAskForCredentials:(BOOL)flag;

- (BOOL)needsStorageAccessFromFileURLsQuirk;
- (void)setNeedsStorageAccessFromFileURLsQuirk:(BOOL)flag;

- (BOOL)zoomsTextOnly;
- (void)setZoomsTextOnly:(BOOL)zoomsTextOnly;

- (BOOL)javaScriptCanAccessClipboard;
- (void)setJavaScriptCanAccessClipboard:(BOOL)flag;

- (BOOL)isXSSAuditorEnabled;
- (void)setXSSAuditorEnabled:(BOOL)flag;

- (BOOL)experimentalNotificationsEnabled;
- (void)setExperimentalNotificationsEnabled:(BOOL)notificationsEnabled;

- (BOOL)isFrameFlatteningEnabled;
- (void)setFrameFlatteningEnabled:(BOOL)flag;

- (WebKitFrameFlattening)frameFlattening;
- (void)setFrameFlattening:(WebKitFrameFlattening)flag;

- (BOOL)asyncFrameScrollingEnabled;
- (void)setAsyncFrameScrollingEnabled:(BOOL)enabled;

- (BOOL)isSpatialNavigationEnabled;
- (void)setSpatialNavigationEnabled:(BOOL)flag;

- (void)setMediaDevicesEnabled:(BOOL)flag;
- (BOOL)mediaDevicesEnabled;

- (void)setMediaStreamEnabled:(BOOL)flag;
- (BOOL)mediaStreamEnabled;

- (void)setPeerConnectionEnabled:(BOOL)flag;
- (BOOL)peerConnectionEnabled;

#if !TARGET_OS_IPHONE
// zero means do AutoScale
- (float)PDFScaleFactor;
- (void)setPDFScaleFactor:(float)scale;
#endif

- (int64_t)applicationCacheTotalQuota;
- (void)setApplicationCacheTotalQuota:(int64_t)quota;

- (int64_t)applicationCacheDefaultOriginQuota;
- (void)setApplicationCacheDefaultOriginQuota:(int64_t)quota;

- (WebKitEditableLinkBehavior)editableLinkBehavior;
- (void)setEditableLinkBehavior:(WebKitEditableLinkBehavior)behavior;

- (WebTextDirectionSubmenuInclusionBehavior)textDirectionSubmenuInclusionBehavior;
- (void)setTextDirectionSubmenuInclusionBehavior:(WebTextDirectionSubmenuInclusionBehavior)behavior;

// Used to set preference specified in the test via LayoutTestController.overridePreference(..).
// For use with DumpRenderTree only.
- (void)_setPreferenceForTestWithValue:(NSString *)value forKey:(NSString *)key;

// If site-specific spoofing is enabled, some pages that do inappropriate user-agent string checks will be
// passed a nonstandard user-agent string to get them to work correctly. This method might be removed in
// the future when there's no more need for it.
- (BOOL)_useSiteSpecificSpoofing;
- (void)_setUseSiteSpecificSpoofing:(BOOL)newValue;

// WARNING: Allowing paste through the DOM API opens a security hole. We only use it for testing purposes.
- (BOOL)isDOMPasteAllowed;
- (void)setDOMPasteAllowed:(BOOL)DOMPasteAllowed;

- (NSString *)_ftpDirectoryTemplatePath;
- (void)_setFTPDirectoryTemplatePath:(NSString *)path;

- (void)_setForceFTPDirectoryListings:(BOOL)force;
- (BOOL)_forceFTPDirectoryListings;

- (NSString *)_localStorageDatabasePath;
- (void)_setLocalStorageDatabasePath:(NSString *)path;

- (BOOL)acceleratedDrawingEnabled;
- (void)setAcceleratedDrawingEnabled:(BOOL)enabled;

- (BOOL)displayListDrawingEnabled;
- (void)setDisplayListDrawingEnabled:(BOOL)enabled;

- (BOOL)resourceLoadStatisticsEnabled;
- (void)setResourceLoadStatisticsEnabled:(BOOL)enabled;

- (BOOL)canvasUsesAcceleratedDrawing;
- (void)setCanvasUsesAcceleratedDrawing:(BOOL)enabled;

- (BOOL)acceleratedCompositingEnabled;
- (void)setAcceleratedCompositingEnabled:(BOOL)enabled;

- (BOOL)subpixelAntialiasedLayerTextEnabled;
- (void)setSubpixelAntialiasedLayerTextEnabled:(BOOL)enabled;

- (BOOL)showDebugBorders;
- (void)setShowDebugBorders:(BOOL)show;

- (BOOL)simpleLineLayoutEnabled;
- (void)setSimpleLineLayoutEnabled:(BOOL)enabled;

- (BOOL)simpleLineLayoutDebugBordersEnabled;
- (void)setSimpleLineLayoutDebugBordersEnabled:(BOOL)enabled;

- (BOOL)showRepaintCounter;
- (void)setShowRepaintCounter:(BOOL)show;

- (BOOL)webAudioEnabled;
- (void)setWebAudioEnabled:(BOOL)enabled;

- (BOOL)subpixelCSSOMElementMetricsEnabled;
- (void)setSubpixelCSSOMElementMetricsEnabled:(BOOL)enabled;

- (BOOL)webGLEnabled;
- (void)setWebGLEnabled:(BOOL)enabled;

- (BOOL)webGL2Enabled;
- (void)setWebGL2Enabled:(BOOL)enabled;

- (BOOL)forceSoftwareWebGLRendering;
- (void)setForceSoftwareWebGLRendering:(BOOL)forced;

- (BOOL)forceLowPowerGPUForWebGL;
- (void)setForceWebGLUsesLowPower:(BOOL)forceLowPower;

- (BOOL)webGPUEnabled;
- (void)setWebGPUEnabled:(BOOL)enabled;

- (BOOL)webMetalEnabled;
- (void)setWebMetalEnabled:(BOOL)enabled;

- (BOOL)accelerated2dCanvasEnabled;
- (void)setAccelerated2dCanvasEnabled:(BOOL)enabled;

- (BOOL)paginateDuringLayoutEnabled;
- (void)setPaginateDuringLayoutEnabled:(BOOL)flag;

- (BOOL)hyperlinkAuditingEnabled;
- (void)setHyperlinkAuditingEnabled:(BOOL)enabled;

// Deprecated. Use -setVideoPlaybackRequiresUserGesture and -setAudioPlaybackRequiresUserGesture instead.
- (void)setMediaPlaybackRequiresUserGesture:(BOOL)flag;
// Deprecated. Use -videoPlaybackRequiresUserGesture and -audioPlaybackRequiresUserGesture instead.
- (BOOL)mediaPlaybackRequiresUserGesture;

- (void)setVideoPlaybackRequiresUserGesture:(BOOL)flag;
- (BOOL)videoPlaybackRequiresUserGesture;

- (void)setAudioPlaybackRequiresUserGesture:(BOOL)flag;
- (BOOL)audioPlaybackRequiresUserGesture;

- (void)setOverrideUserGestureRequirementForMainContent:(BOOL)flag;
- (BOOL)overrideUserGestureRequirementForMainContent;

- (void)setMediaPlaybackAllowsInline:(BOOL)flag;
- (BOOL)mediaPlaybackAllowsInline;

- (void)setInlineMediaPlaybackRequiresPlaysInlineAttribute:(BOOL)flag;
- (BOOL)inlineMediaPlaybackRequiresPlaysInlineAttribute;

- (void)setInvisibleAutoplayNotPermitted:(BOOL)flag;
- (BOOL)invisibleAutoplayNotPermitted;

- (void)setMediaControlsScaleWithPageZoom:(BOOL)flag;
- (BOOL)mediaControlsScaleWithPageZoom;

- (void)setAllowsAlternateFullscreen:(BOOL)flag;
- (BOOL)allowsAlternateFullscreen;

- (void)setAllowsPictureInPictureMediaPlayback:(BOOL)flag;
- (BOOL)allowsPictureInPictureMediaPlayback;

- (NSString *)pictographFontFamily;
- (void)setPictographFontFamily:(NSString *)family;

- (BOOL)pageCacheSupportsPlugins;
- (void)setPageCacheSupportsPlugins:(BOOL)flag;

// This is a global setting.
- (BOOL)mockScrollbarsEnabled;
- (void)setMockScrollbarsEnabled:(BOOL)flag;

#if TARGET_OS_IPHONE
// This is a global setting.
- (unsigned)audioSessionCategoryOverride;
- (void)setAudioSessionCategoryOverride:(unsigned)override;

- (BOOL)avKitEnabled;
- (void)setAVKitEnabled:(bool)flag;

// WARNING: this affect network performance. This must not be enabled for production use.
// Enabling this makes WebCore reports the network data usage.
// This is a global setting.
- (void)setNetworkDataUsageTrackingEnabled:(bool)trackingEnabled;
- (BOOL)networkDataUsageTrackingEnabled;

- (void)setNetworkInterfaceName:(NSString *)name;
- (NSString *)networkInterfaceName;

- (void)_setMinimumZoomFontSize:(float)size;
- (float)_minimumZoomFontSize;

// Deprecated. Has no effect.
- (void)setDiskImageCacheEnabled:(BOOL)enabled;

- (void)setMediaPlaybackAllowsAirPlay:(BOOL)flag;
- (BOOL)mediaPlaybackAllowsAirPlay;
#endif

- (void)_setTextAutosizingEnabled:(BOOL)enabled;
- (BOOL)_textAutosizingEnabled;

- (BOOL)isInheritURIQueryComponentEnabled;
- (void)setEnableInheritURIQueryComponent:(BOOL)flag;

// Other private methods
#if TARGET_OS_IPHONE
- (BOOL)_standalone;
- (void)_setStandalone:(BOOL)flag;
- (void)_setTelephoneNumberParsingEnabled:(BOOL)flag;
- (BOOL)_telephoneNumberParsingEnabled;
- (void)_setAllowMultiElementImplicitFormSubmission:(BOOL)flag;
- (BOOL)_allowMultiElementImplicitFormSubmission;
- (void)_setAlwaysRequestGeolocationPermission:(BOOL)flag;
- (BOOL)_alwaysRequestGeolocationPermission;
- (void)_setAlwaysUseAcceleratedOverflowScroll:(BOOL)flag;
- (BOOL)_alwaysUseAcceleratedOverflowScroll;
- (void)_setLayoutInterval:(int)milliseconds;
- (int)_layoutInterval; // Milliseonds.
- (void)_setMaxParseDuration:(float)d;
- (float)_maxParseDuration;
- (void)_setInterpolationQuality:(int)quality;
- (int)_interpolationQuality;
- (BOOL)_allowPasswordEcho;
- (float)_passwordEchoDuration;
#endif
- (void)_postPreferencesChangedNotification;
- (void)_postPreferencesChangedAPINotification;
+ (WebPreferences *)_getInstanceForIdentifier:(NSString *)identifier;
+ (void)_setInstance:(WebPreferences *)instance forIdentifier:(NSString *)identifier;
+ (void)_removeReferenceForIdentifier:(NSString *)identifier;
- (NSTimeInterval)_backForwardCacheExpirationInterval;
+ (CFStringEncoding)_systemCFStringEncoding;
+ (void)_setInitialDefaultTextEncodingToSystemEncoding;
+ (void)_setIBCreatorID:(NSString *)string;

// For DumpRenderTree use only.
+ (void)_switchNetworkLoaderToNewTestingSession;
+ (void)_setCurrentNetworkLoaderSessionCookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)cookieAcceptPolicy;
+ (void)_clearNetworkLoaderSession;

+ (void)setWebKitLinkTimeVersion:(int)version;

// For WebView's use only.
- (void)willAddToWebView;
- (void)didRemoveFromWebView;

// Full screen support is dependent on WebCore/WebKit being
// compiled with ENABLE_FULLSCREEN_API. 
- (void)setFullScreenEnabled:(BOOL)flag;
- (BOOL)fullScreenEnabled;

- (void)setAsynchronousSpellCheckingEnabled:(BOOL)flag;
- (BOOL)asynchronousSpellCheckingEnabled;

- (void)setUsePreHTML5ParserQuirks:(BOOL)flag;
- (BOOL)usePreHTML5ParserQuirks;

- (void)setLoadsSiteIconsIgnoringImageLoadingPreference: (BOOL)flag;
- (BOOL)loadsSiteIconsIgnoringImageLoadingPreference;

// AVFoundation support is dependent on WebCore/WebKit being
// compiled with USE_AVFOUNDATION.
- (void)setAVFoundationEnabled:(BOOL)flag;
- (BOOL)isAVFoundationEnabled;

- (void)setAVFoundationNSURLSessionEnabled:(BOOL)flag;
- (BOOL)isAVFoundationNSURLSessionEnabled;

// Deprecated, has no effect.
- (void)setVideoPluginProxyEnabled:(BOOL)flag;
- (BOOL)isVideoPluginProxyEnabled;

- (void)setHixie76WebSocketProtocolEnabled:(BOOL)flag;
- (BOOL)isHixie76WebSocketProtocolEnabled;

#if TARGET_OS_IPHONE
- (void)_invalidateCachedPreferences;
- (void)_synchronizeWebStoragePolicyWithCookiePolicy;
#endif

- (void)setBackspaceKeyNavigationEnabled:(BOOL)flag;
- (BOOL)backspaceKeyNavigationEnabled;

- (void)setWantsBalancedSetDefersLoadingBehavior:(BOOL)flag;
- (BOOL)wantsBalancedSetDefersLoadingBehavior;

- (void)setShouldDisplaySubtitles:(BOOL)flag;
- (BOOL)shouldDisplaySubtitles;

- (void)setShouldDisplayCaptions:(BOOL)flag;
- (BOOL)shouldDisplayCaptions;

- (void)setShouldDisplayTextDescriptions:(BOOL)flag;
- (BOOL)shouldDisplayTextDescriptions;

- (void)setNotificationsEnabled:(BOOL)flag;
- (BOOL)notificationsEnabled;

- (void)setShouldRespectImageOrientation:(BOOL)flag;
- (BOOL)shouldRespectImageOrientation;

- (BOOL)requestAnimationFrameEnabled;
- (void)setRequestAnimationFrameEnabled:(BOOL)enabled;

- (void)setIncrementalRenderingSuppressionTimeoutInSeconds:(NSTimeInterval)timeout;
- (NSTimeInterval)incrementalRenderingSuppressionTimeoutInSeconds;

- (BOOL)diagnosticLoggingEnabled;
- (void)setDiagnosticLoggingEnabled:(BOOL)enabled;

- (void)setStorageBlockingPolicy:(WebStorageBlockingPolicy)storageBlockingPolicy;
- (WebStorageBlockingPolicy)storageBlockingPolicy;

- (BOOL)plugInSnapshottingEnabled;
- (void)setPlugInSnapshottingEnabled:(BOOL)enabled;

- (BOOL)hiddenPageDOMTimerThrottlingEnabled;
- (void)setHiddenPageDOMTimerThrottlingEnabled:(BOOL)flag;

- (BOOL)hiddenPageCSSAnimationSuspensionEnabled;
- (void)setHiddenPageCSSAnimationSuspensionEnabled:(BOOL)flag;

- (BOOL)lowPowerVideoAudioBufferSizeEnabled;
- (void)setLowPowerVideoAudioBufferSizeEnabled:(BOOL)enabled;

- (void)setUseLegacyTextAlignPositionedElementBehavior:(BOOL)flag;
- (BOOL)useLegacyTextAlignPositionedElementBehavior;

- (void)setMediaSourceEnabled:(BOOL)flag;
- (BOOL)mediaSourceEnabled;

- (void)setShouldConvertPositionStyleOnCopy:(BOOL)flag;
- (BOOL)shouldConvertPositionStyleOnCopy;

- (void)setImageControlsEnabled:(BOOL)flag;
- (BOOL)imageControlsEnabled;

- (void)setServiceControlsEnabled:(BOOL)flag;
- (BOOL)serviceControlsEnabled;

- (void)setGamepadsEnabled:(BOOL)flag;
- (BOOL)gamepadsEnabled;

- (void)setMediaPreloadingEnabled:(BOOL)flag;
- (BOOL)mediaPreloadingEnabled;

- (void)setMediaKeysStorageDirectory:(NSString *)directory;
- (NSString *)mediaKeysStorageDirectory;

- (void)setMetaRefreshEnabled:(BOOL)flag;
- (BOOL)metaRefreshEnabled;

- (void)setHTTPEquivEnabled:(BOOL)flag;
- (BOOL)httpEquivEnabled;

- (void)setMockCaptureDevicesEnabled:(BOOL)flag;
- (BOOL)mockCaptureDevicesEnabled;

- (void)setMockCaptureDevicesPromptEnabled:(BOOL)flag;
- (BOOL)mockCaptureDevicesPromptEnabled;

- (void)setEnumeratingAllNetworkInterfacesEnabled:(BOOL)flag;
- (BOOL)enumeratingAllNetworkInterfacesEnabled;

- (void)setIceCandidateFilteringEnabled:(BOOL)flag;
- (BOOL)iceCandidateFilteringEnabled;

- (void)setMediaCaptureRequiresSecureConnection:(BOOL)flag;
- (BOOL)mediaCaptureRequiresSecureConnection;

- (void)setShadowDOMEnabled:(BOOL)flag;
- (BOOL)shadowDOMEnabled;

- (void)setCustomElementsEnabled:(BOOL)flag;
- (BOOL)customElementsEnabled;

- (void)setDataTransferItemsEnabled:(BOOL)flag;
- (BOOL)dataTransferItemsEnabled;

- (void)setCustomPasteboardDataEnabled:(BOOL)flag;
- (BOOL)customPasteboardDataEnabled;

- (BOOL)cacheAPIEnabled;
- (void)setCacheAPIEnabled:(BOOL)enabled;

- (void)setFetchAPIEnabled:(BOOL)flag;
- (BOOL)fetchAPIEnabled;

- (void)setReadableByteStreamAPIEnabled:(BOOL)flag;
- (BOOL)readableByteStreamAPIEnabled;

- (void)setWritableStreamAPIEnabled:(BOOL)flag;
- (BOOL)writableStreamAPIEnabled;

- (void)setDownloadAttributeEnabled:(BOOL)flag;
- (BOOL)downloadAttributeEnabled;

- (void)setDirectoryUploadEnabled:(BOOL)flag;
- (BOOL)directoryUploadEnabled;

- (void)setWebAnimationsEnabled:(BOOL)flag;
- (BOOL)webAnimationsEnabled;

- (void)setFetchAPIKeepAliveEnabled:(BOOL)flag;
- (BOOL)fetchAPIKeepAliveEnabled;

- (void)setModernMediaControlsEnabled:(BOOL)flag;
- (BOOL)modernMediaControlsEnabled;

- (void)setWebAnimationsCSSIntegrationEnabled:(BOOL)flag;
- (BOOL)webAnimationsCSSIntegrationEnabled;

- (void)setIntersectionObserverEnabled:(BOOL)flag;
- (BOOL)intersectionObserverEnabled;

- (void)setIsSecureContextAttributeEnabled:(BOOL)flag;
- (BOOL)isSecureContextAttributeEnabled;

- (void)setServerTimingEnabled:(BOOL)flag;
- (BOOL)serverTimingEnabled;

- (void)setSelectionAcrossShadowBoundariesEnabled:(BOOL)flag;
- (BOOL)selectionAcrossShadowBoundariesEnabled;

@property (nonatomic) BOOL visualViewportEnabled;
@property (nonatomic) BOOL visualViewportAPIEnabled;
@property (nonatomic) BOOL CSSOMViewScrollingAPIEnabled;
@property (nonatomic) BOOL largeImageAsyncDecodingEnabled;
@property (nonatomic) BOOL animatedImageAsyncDecodingEnabled;
@property (nonatomic) BOOL javaScriptMarkupEnabled;
@property (nonatomic) BOOL mediaDataLoadsAutomatically;
@property (nonatomic) BOOL attachmentElementEnabled;
@property (nonatomic) BOOL allowsInlineMediaPlaybackAfterFullscreen;
@property (nonatomic) BOOL intersectionObserverEnabled;
@property (nonatomic) BOOL menuItemElementEnabled;
@property (nonatomic) BOOL displayContentsEnabled;
@property (nonatomic) BOOL userTimingEnabled;
@property (nonatomic) BOOL resourceTimingEnabled;
@property (nonatomic) BOOL linkPreloadEnabled;
@property (nonatomic) BOOL mediaUserGestureInheritsFromDocument;
@property (nonatomic) BOOL isSecureContextAttributeEnabled;
@property (nonatomic) BOOL legacyEncryptedMediaAPIEnabled;
@property (nonatomic) BOOL encryptedMediaAPIEnabled;
@property (nonatomic) BOOL viewportFitEnabled;
@property (nonatomic) BOOL constantPropertiesEnabled;
@property (nonatomic) BOOL colorFilterEnabled;
@property (nonatomic) BOOL punchOutWhiteBackgroundsInDarkMode;
@property (nonatomic) BOOL inspectorAdditionsEnabled;
@property (nonatomic) BOOL allowMediaContentTypesRequiringHardwareSupportAsFallback;
@property (nonatomic) BOOL accessibilityObjectModelEnabled;
@property (nonatomic) BOOL ariaReflectionEnabled;
@property (nonatomic) BOOL mediaCapabilitiesEnabled;
@property (nonatomic) BOOL mediaRecorderEnabled;
@property (nonatomic) BOOL allowCrossOriginSubresourcesToAskForCredentials;
@property (nonatomic) BOOL sourceBufferChangeTypeEnabled;

#if TARGET_OS_IPHONE
@property (nonatomic) BOOL quickLookDocumentSavingEnabled;
#endif

@property (nonatomic) NSString *mediaContentTypesRequiringHardwareSupport;

@end
