/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef WebPreferencesDefinitions_h
#define WebPreferencesDefinitions_h

#if PLATFORM(GTK)
#define DEFAULT_WEBKIT_TABSTOLINKS_ENABLED true
#else
#define DEFAULT_WEBKIT_TABSTOLINKS_ENABLED false
#endif

#if ENABLE(SMOOTH_SCROLLING)
#define DEFAULT_WEBKIT_SCROLL_ANIMATOR_ENABLED true
#else
#define DEFAULT_WEBKIT_SCROLL_ANIMATOR_ENABLED false
#endif

#if PLATFORM(COCOA)
#define DEFAULT_HIDDEN_PAGE_DOM_TIMER_THROTTLING_ENABLED true
#define DEFAULT_HIDDEN_PAGE_CSS_ANIMATION_SUSPENSION_ENABLED true
#define DEFAULT_PDFPLUGIN_ENABLED true
#else
#define DEFAULT_HIDDEN_PAGE_DOM_TIMER_THROTTLING_ENABLED false
#define DEFAULT_HIDDEN_PAGE_CSS_ANIMATION_SUSPENSION_ENABLED false
#define DEFAULT_PDFPLUGIN_ENABLED false
#endif

#if PLATFORM(IOS)
#define DEFAULT_BACKSPACE_KEY_NAVIGATION_ENABLED false
#define DEFAULT_FRAME_FLATTENING_ENABLED true
#define DEFAULT_SHOULD_PRINT_BACKGROUNDS true
#define DEFAULT_TEXT_AREAS_ARE_RESIZABLE false
#define DEFAULT_JAVASCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY false
#define DEFAULT_SHOULD_RESPECT_IMAGE_ORIENTATION true
#define DEFAULT_MINIMUM_FONT_ZOOM_SIZE WebCore::Settings::defaultMinimumZoomFontSize()
#define DEFAULT_PASSWORD_ECHO_ENABLED true
#define DEFAULT_ALLOWS_INLINE_MEDIA_PLAYBACK false
#define DEFAULT_INLINE_MEDIA_PLAYBACK_REQUIRES_PLAYS_INLINE_ATTRIBUTE true
#define DEFAULT_INVISIBLE_AUTOPLAY_NOT_PERMITTED true
#define DEFAULT_MEDIA_DATA_LOADS_AUTOMATICALLY false
#define DEFAULT_REQUIRES_USER_GESTURE_FOR_MEDIA_PLAYBACK true
#define DEFAULT_REQUIRES_USER_GESTURE_FOR_AUDIO_PLAYBACK true
#define DEFAULT_MEDIA_CONTROLS_SCALE_WITH_PAGE_ZOOM false
#define DEFAULT_TEMPORARY_TILE_COHORT_RETENTION_ENABLED false
#else
#define DEFAULT_BACKSPACE_KEY_NAVIGATION_ENABLED true
#define DEFAULT_FRAME_FLATTENING_ENABLED false
#define DEFAULT_SHOULD_PRINT_BACKGROUNDS false
#define DEFAULT_TEXT_AREAS_ARE_RESIZABLE true
#define DEFAULT_JAVASCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY true
#define DEFAULT_SHOULD_RESPECT_IMAGE_ORIENTATION false
#define DEFAULT_MINIMUM_FONT_ZOOM_SIZE 0
#define DEFAULT_PASSWORD_ECHO_ENABLED false
#define DEFAULT_ALLOWS_INLINE_MEDIA_PLAYBACK true
#define DEFAULT_INLINE_MEDIA_PLAYBACK_REQUIRES_PLAYS_INLINE_ATTRIBUTE false
#define DEFAULT_INVISIBLE_AUTOPLAY_NOT_PERMITTED false
#define DEFAULT_MEDIA_DATA_LOADS_AUTOMATICALLY true
#define DEFAULT_REQUIRES_USER_GESTURE_FOR_MEDIA_PLAYBACK false
#define DEFAULT_REQUIRES_USER_GESTURE_FOR_AUDIO_PLAYBACK false
#define DEFAULT_MEDIA_CONTROLS_SCALE_WITH_PAGE_ZOOM true
#define DEFAULT_TEMPORARY_TILE_COHORT_RETENTION_ENABLED true
#endif

#if PLATFORM(IOS_SIMULATOR)
#define DEFAULT_ACCELERATED_DRAWING_ENABLED false
#define DEFAULT_CANVAS_USES_ACCELERATED_DRAWING false
#else
#define DEFAULT_ACCELERATED_DRAWING_ENABLED true
#define DEFAULT_CANVAS_USES_ACCELERATED_DRAWING true
#endif

// macro(KeyUpper, KeyLower, TypeNameUpper, TypeName, DefaultValue)

#define FOR_EACH_WEBKIT_BOOL_PREFERENCE(macro) \
    macro(JavaScriptEnabled, javaScriptEnabled, Bool, bool, true) \
    macro(JavaScriptMarkupEnabled, javaScriptMarkupEnabled, Bool, bool, true) \
    macro(LoadsImagesAutomatically, loadsImagesAutomatically, Bool, bool, true) \
    macro(LoadsSiteIconsIgnoringImageLoadingPreference, loadsSiteIconsIgnoringImageLoadingPreference, Bool, bool, false) \
    macro(PluginsEnabled, pluginsEnabled, Bool, bool, false) \
    macro(JavaEnabled, javaEnabled, Bool, bool, false) \
    macro(JavaEnabledForLocalFiles, javaEnabledForLocalFiles, Bool, bool, false) \
    macro(OfflineWebApplicationCacheEnabled, offlineWebApplicationCacheEnabled, Bool, bool, false) \
    macro(LocalStorageEnabled, localStorageEnabled, Bool, bool, true) \
    macro(DatabasesEnabled, databasesEnabled, Bool, bool, true) \
    macro(XSSAuditorEnabled, xssAuditorEnabled, Bool, bool, true) \
    macro(FrameFlatteningEnabled, frameFlatteningEnabled, Bool, bool, DEFAULT_FRAME_FLATTENING_ENABLED) \
    macro(PrivateBrowsingEnabled, privateBrowsingEnabled, Bool, bool, false) \
    macro(TextAreasAreResizable, textAreasAreResizable, Bool, bool, DEFAULT_TEXT_AREAS_ARE_RESIZABLE) \
    macro(JavaScriptCanOpenWindowsAutomatically, javaScriptCanOpenWindowsAutomatically, Bool, bool, DEFAULT_JAVASCRIPT_CAN_OPEN_WINDOWS_AUTOMATICALLY) \
    macro(HyperlinkAuditingEnabled, hyperlinkAuditingEnabled, Bool, bool, true) \
    macro(NeedsSiteSpecificQuirks, needsSiteSpecificQuirks, Bool, bool, false) \
    macro(AcceleratedCompositingEnabled, acceleratedCompositingEnabled, Bool, bool, true) \
    macro(ForceCompositingMode, forceCompositingMode, Bool, bool, false) \
    macro(CanvasUsesAcceleratedDrawing, canvasUsesAcceleratedDrawing, Bool, bool, DEFAULT_CANVAS_USES_ACCELERATED_DRAWING) \
    macro(WebGLEnabled, webGLEnabled, Bool, bool, true) \
    macro(ForceSoftwareWebGLRendering, forceSoftwareWebGLRendering, Bool, bool, false) \
    macro(Accelerated2dCanvasEnabled, accelerated2dCanvasEnabled, Bool, bool, false) \
    macro(CSSAnimationTriggersEnabled, cssAnimationTriggersEnabled, Bool, bool, true) \
    macro(WebAnimationsEnabled, webAnimationsEnabled, Bool, bool, false) \
    macro(CSSRegionsEnabled, cssRegionsEnabled, Bool, bool, true) \
    macro(CSSCompositingEnabled, cssCompositingEnabled, Bool, bool, true) \
    macro(ForceFTPDirectoryListings, forceFTPDirectoryListings, Bool, bool, false) \
    macro(TabsToLinks, tabsToLinks, Bool, bool, DEFAULT_WEBKIT_TABSTOLINKS_ENABLED) \
    macro(DNSPrefetchingEnabled, dnsPrefetchingEnabled, Bool, bool, false) \
    macro(DOMTimersThrottlingEnabled, domTimersThrottlingEnabled, Bool, bool, true) \
    macro(WebArchiveDebugModeEnabled, webArchiveDebugModeEnabled, Bool, bool, false) \
    macro(LocalFileContentSniffingEnabled, localFileContentSniffingEnabled, Bool, bool, false) \
    macro(UsesPageCache, usesPageCache, Bool, bool, true) \
    macro(PageCacheSupportsPlugins, pageCacheSupportsPlugins, Bool, bool, true) \
    macro(AuthorAndUserStylesEnabled, authorAndUserStylesEnabled, Bool, bool, true) \
    macro(PaginateDuringLayoutEnabled, paginateDuringLayoutEnabled, Bool, bool, false) \
    macro(DOMPasteAllowed, domPasteAllowed, Bool, bool, false) \
    macro(JavaScriptCanAccessClipboard, javaScriptCanAccessClipboard, Bool, bool, false) \
    macro(ShouldPrintBackgrounds, shouldPrintBackgrounds, Bool, bool, DEFAULT_SHOULD_PRINT_BACKGROUNDS) \
    macro(FullScreenEnabled, fullScreenEnabled, Bool, bool, false) \
    macro(AsynchronousSpellCheckingEnabled, asynchronousSpellCheckingEnabled, Bool, bool, false) \
    macro(WebSecurityEnabled, webSecurityEnabled, Bool, bool, true) \
    macro(AllowUniversalAccessFromFileURLs, allowUniversalAccessFromFileURLs, Bool, bool, false) \
    macro(AllowFileAccessFromFileURLs, allowFileAccessFromFileURLs, Bool, bool, false) \
    macro(AVFoundationEnabled, isAVFoundationEnabled, Bool, bool, true) \
    macro(RequiresUserGestureForMediaPlayback, requiresUserGestureForMediaPlayback, Bool, bool, DEFAULT_REQUIRES_USER_GESTURE_FOR_MEDIA_PLAYBACK) \
    macro(RequiresUserGestureForAudioPlayback, requiresUserGestureForAudioPlayback, Bool, bool, DEFAULT_REQUIRES_USER_GESTURE_FOR_AUDIO_PLAYBACK) \
    macro(AllowsInlineMediaPlayback, allowsInlineMediaPlayback, Bool, bool, DEFAULT_ALLOWS_INLINE_MEDIA_PLAYBACK) \
    macro(InlineMediaPlaybackRequiresPlaysInlineAttribute, inlineMediaPlaybackRequiresPlaysInlineAttribute, Bool, bool, DEFAULT_INLINE_MEDIA_PLAYBACK_REQUIRES_PLAYS_INLINE_ATTRIBUTE) \
    macro(InvisibleAutoplayNotPermitted, invisibleAutoplayNotPermitted, Bool, bool, DEFAULT_INVISIBLE_AUTOPLAY_NOT_PERMITTED) \
    macro(MediaDataLoadsAutomatically, mediaDataLoadsAutomatically, Bool, bool, DEFAULT_MEDIA_DATA_LOADS_AUTOMATICALLY) \
    macro(AllowsPictureInPictureMediaPlayback, allowsPictureInPictureMediaPlayback, Bool, bool, true) \
    macro(AllowsAirPlayForMediaPlayback, allowsAirPlayForMediaPlayback, Bool, bool, true) \
    macro(MediaControlsScaleWithPageZoom, mediaControlsScaleWithPageZoom, Bool, bool, DEFAULT_MEDIA_CONTROLS_SCALE_WITH_PAGE_ZOOM) \
    macro(InspectorStartsAttached, inspectorStartsAttached, Bool, bool, true) \
    macro(ShowsToolTipOverTruncatedText, showsToolTipOverTruncatedText, Bool, bool, false) \
    macro(MockScrollbarsEnabled, mockScrollbarsEnabled, Bool, bool, false) \
    macro(WebAudioEnabled, webAudioEnabled, Bool, bool, true) \
    macro(SuppressesIncrementalRendering, suppressesIncrementalRendering, Bool, bool, false) \
    macro(BackspaceKeyNavigationEnabled, backspaceKeyNavigationEnabled, Bool, bool, DEFAULT_BACKSPACE_KEY_NAVIGATION_ENABLED) \
    macro(CaretBrowsingEnabled, caretBrowsingEnabled, Bool, bool, false) \
    macro(ShouldDisplaySubtitles, shouldDisplaySubtitles, Bool, bool, false) \
    macro(ShouldDisplayCaptions, shouldDisplayCaptions, Bool, bool, false) \
    macro(ShouldDisplayTextDescriptions, shouldDisplayTextDescriptions, Bool, bool, false) \
    macro(NotificationsEnabled, notificationsEnabled, Bool, bool, true) \
    macro(ShouldRespectImageOrientation, shouldRespectImageOrientation, Bool, bool, DEFAULT_SHOULD_RESPECT_IMAGE_ORIENTATION) \
    macro(WantsBalancedSetDefersLoadingBehavior, wantsBalancedSetDefersLoadingBehavior, Bool, bool, false) \
    macro(RequestAnimationFrameEnabled, requestAnimationFrameEnabled, Bool, bool, true) \
    macro(DiagnosticLoggingEnabled, diagnosticLoggingEnabled, Bool, bool, false) \
    macro(AsynchronousPluginInitializationEnabled, asynchronousPluginInitializationEnabled, Bool, bool, false) \
    macro(AsynchronousPluginInitializationEnabledForAllPlugins, asynchronousPluginInitializationEnabledForAllPlugins, Bool, bool, false) \
    macro(ArtificialPluginInitializationDelayEnabled, artificialPluginInitializationDelayEnabled, Bool, bool, false) \
    macro(TabToLinksEnabled, tabToLinksEnabled, Bool, bool, false) \
    macro(InteractiveFormValidationEnabled, interactiveFormValidationEnabled, Bool, bool, false) \
    macro(ScrollingPerformanceLoggingEnabled, scrollingPerformanceLoggingEnabled, Bool, bool, false) \
    macro(ScrollAnimatorEnabled, scrollAnimatorEnabled, Bool, bool, DEFAULT_WEBKIT_SCROLL_ANIMATOR_ENABLED) \
    macro(ForceUpdateScrollbarsOnMainThreadForPerformanceTesting, forceUpdateScrollbarsOnMainThreadForPerformanceTesting, Bool, bool, false) \
    macro(CookieEnabled, cookieEnabled, Bool, bool, true) \
    macro(PlugInSnapshottingEnabled, plugInSnapshottingEnabled, Bool, bool, false) \
    macro(SnapshotAllPlugIns, snapshotAllPlugIns, Bool, bool, false) \
    macro(AutostartOriginPlugInSnapshottingEnabled, autostartOriginPlugInSnapshottingEnabled, Bool, bool, true) \
    macro(PrimaryPlugInSnapshotDetectionEnabled, primaryPlugInSnapshotDetectionEnabled, Bool, bool, true) \
    macro(PDFPluginEnabled, pdfPluginEnabled, Bool, bool, DEFAULT_PDFPLUGIN_ENABLED) \
    macro(UsesEncodingDetector, usesEncodingDetector, Bool, bool, false) \
    macro(TextAutosizingEnabled, textAutosizingEnabled, Bool, bool, false) \
    macro(AggressiveTileRetentionEnabled, aggressiveTileRetentionEnabled, Bool, bool, false) \
    macro(TemporaryTileCohortRetentionEnabled, temporaryTileCohortRetentionEnabled, Bool, bool, DEFAULT_TEMPORARY_TILE_COHORT_RETENTION_ENABLED) \
    macro(QTKitEnabled, isQTKitEnabled, Bool, bool, WebCore::Settings::isQTKitEnabled()) \
    macro(PageVisibilityBasedProcessSuppressionEnabled, pageVisibilityBasedProcessSuppressionEnabled, Bool, bool, true) \
    macro(SmartInsertDeleteEnabled, smartInsertDeleteEnabled, Bool, bool, true) \
    macro(SelectTrailingWhitespaceEnabled, selectTrailingWhitespaceEnabled, Bool, bool, false) \
    macro(ShowsURLsInToolTipsEnabled, showsURLsInToolTipsEnabled, Bool, bool, false) \
    macro(AcceleratedCompositingForOverflowScrollEnabled, acceleratedCompositingForOverflowScrollEnabled, Bool, bool, false) \
    macro(HiddenPageDOMTimerThrottlingEnabled, hiddenPageDOMTimerThrottlingEnabled, Bool, bool, DEFAULT_HIDDEN_PAGE_DOM_TIMER_THROTTLING_ENABLED) \
    macro(HiddenPageCSSAnimationSuspensionEnabled, hiddenPageCSSAnimationSuspensionEnabled, Bool, bool, DEFAULT_HIDDEN_PAGE_CSS_ANIMATION_SUSPENSION_ENABLED) \
    macro(LowPowerVideoAudioBufferSizeEnabled, lowPowerVideoAudioBufferSizeEnabled, Bool, bool, false) \
    macro(ThreadedScrollingEnabled, threadedScrollingEnabled, Bool, bool, true) \
    macro(SimpleLineLayoutEnabled, simpleLineLayoutEnabled, Bool, bool, true) \
    macro(SubpixelCSSOMElementMetricsEnabled, subpixelCSSOMElementMetricsEnabled, Bool, bool, false) \
    macro(UseGiantTiles, useGiantTiles, Bool, bool, false) \
    macro(MediaStreamEnabled, mediaStreamEnabled, Bool, bool, false) \
    macro(UseLegacyTextAlignPositionedElementBehavior, useLegacyTextAlignPositionedElementBehavior, Bool, bool, false) \
    macro(SpatialNavigationEnabled, spatialNavigationEnabled, Bool, bool, false) \
    macro(MediaSourceEnabled, mediaSourceEnabled, Bool, bool, true) \
    macro(ViewGestureDebuggingEnabled, viewGestureDebuggingEnabled, Bool, bool, false) \
    macro(ShouldConvertPositionStyleOnCopy, shouldConvertPositionStyleOnCopy, Bool, bool, false) \
    macro(Standalone, standalone, Bool, bool, false) \
    macro(TelephoneNumberParsingEnabled, telephoneNumberParsingEnabled, Bool, bool, false) \
    macro(AllowMultiElementImplicitSubmission, allowMultiElementImplicitSubmission, Bool, bool, false) \
    macro(AlwaysUseAcceleratedOverflowScroll, alwaysUseAcceleratedOverflowScroll, Bool, bool, false) \
    macro(PasswordEchoEnabled, passwordEchoEnabled, Bool, bool, DEFAULT_PASSWORD_ECHO_ENABLED) \
    macro(ImageControlsEnabled, imageControlsEnabled, Bool, bool, false) \
    macro(EnableInheritURIQueryComponent, enableInheritURIQueryComponent, Bool, bool, false) \
    macro(ServiceControlsEnabled, serviceControlsEnabled, Bool, bool, false) \
    macro(GamepadsEnabled, gamepadsEnabled, Bool, bool, false) \
    macro(NewBlockInsideInlineModelEnabled, newBlockInsideInlineModelEnabled, Bool, bool, false) \
    macro(AntialiasedFontDilationEnabled, antialiasedFontDilationEnabled, Bool, bool, false) \
    macro(HTTPEquivEnabled, httpEquivEnabled, Bool, bool, true) \
    macro(MockCaptureDevicesEnabled, mockCaptureDevicesEnabled, Bool, bool, false) \

#define FOR_EACH_WEBKIT_DOUBLE_PREFERENCE(macro) \
    macro(IncrementalRenderingSuppressionTimeout, incrementalRenderingSuppressionTimeout, Double, double, 5) \
    macro(MinimumFontSize, minimumFontSize, Double, double, 0) \
    macro(MinimumLogicalFontSize, minimumLogicalFontSize, Double, double, 9) \
    macro(MinimumZoomFontSize, minimumZoomFontSize, Double, double, DEFAULT_MINIMUM_FONT_ZOOM_SIZE) \
    macro(DefaultFontSize, defaultFontSize, Double, double, 16) \
    macro(DefaultFixedFontSize, defaultFixedFontSize, Double, double, 13) \
    macro(LayoutInterval, layoutInterval, Double, double, -1) \
    macro(MaxParseDuration, maxParseDuration, Double, double, -1) \
    macro(PasswordEchoDuration, passwordEchoDuration, Double, double, 2) \
    \

#define FOR_EACH_WEBKIT_UINT32_PREFERENCE(macro) \
    macro(FontSmoothingLevel, fontSmoothingLevel, UInt32, uint32_t, FontSmoothingLevelMedium) \
    macro(LayoutFallbackWidth, layoutFallbackWidth, UInt32, uint32_t, 980) \
    macro(DeviceWidth, deviceWidth, UInt32, uint32_t, 0) \
    macro(DeviceHeight, deviceHeight, UInt32, uint32_t, 0) \
    macro(EditableLinkBehavior, editableLinkBehavior, UInt32, uint32_t, WebCore::EditableLinkNeverLive) \
    macro(InspectorAttachedHeight, inspectorAttachedHeight, UInt32, uint32_t, 300) \
    macro(InspectorAttachedWidth, inspectorAttachedWidth, UInt32, uint32_t, 750) \
    macro(InspectorAttachmentSide, inspectorAttachmentSide, UInt32, uint32_t, 0) \
    macro(StorageBlockingPolicy, storageBlockingPolicy, UInt32, uint32_t, WebCore::SecurityOrigin::BlockThirdPartyStorage) \
    macro(JavaScriptRuntimeFlags, javaScriptRuntimeFlags, UInt32, uint32_t, 0) \
    \

#define FOR_EACH_WEBKIT_DEBUG_BOOL_PREFERENCE(macro) \
    macro(AcceleratedDrawingEnabled, acceleratedDrawingEnabled, Bool, bool, DEFAULT_ACCELERATED_DRAWING_ENABLED) \
    macro(CompositingBordersVisible, compositingBordersVisible, Bool, bool, false) \
    macro(CompositingRepaintCountersVisible, compositingRepaintCountersVisible, Bool, bool, false) \
    macro(TiledScrollingIndicatorVisible, tiledScrollingIndicatorVisible, Bool, bool, false) \
    macro(SimpleLineLayoutDebugBordersEnabled, simpleLineLayoutDebugBordersEnabled, Bool, bool, false) \
    macro(DeveloperExtrasEnabled, developerExtrasEnabled, Bool, bool, false) \
    macro(LogsPageMessagesToSystemConsoleEnabled, logsPageMessagesToSystemConsoleEnabled, Bool, bool, false) \
    macro(IgnoreViewportScalingConstraints, ignoreViewportScalingConstraints, Bool, bool, true) \
    macro(ForceAlwaysUserScalable, forceAlwaysUserScalable, Bool, bool, false) \
    macro(ResourceUsageOverlayVisible, resourceUsageOverlayVisible, Bool, bool, false) \
    \

#define FOR_EACH_WEBKIT_DEBUG_UINT32_PREFERENCE(macro) \
    macro(VisibleDebugOverlayRegions, visibleDebugOverlayRegions, UInt32, uint32_t, 0)

#if PLATFORM(COCOA)

#if PLATFORM(IOS)
#define DEFAULT_CURSIVE_FONT_FAMILY "Snell Roundhand"
#define DEFAULT_PICTOGRAPH_FONT_FAMILY "AppleColorEmoji"
#else
#define DEFAULT_CURSIVE_FONT_FAMILY "Apple Chancery"
#define DEFAULT_PICTOGRAPH_FONT_FAMILY "Apple Color Emoji"
#endif


#define FOR_EACH_WEBKIT_FONT_FAMILY_PREFERENCE(macro) \
    macro(StandardFontFamily, standardFontFamily, String, String, "Times") \
    macro(CursiveFontFamily, cursiveFontFamily, String, String, DEFAULT_CURSIVE_FONT_FAMILY) \
    macro(FantasyFontFamily, fantasyFontFamily, String, String, "Papyrus") \
    macro(FixedFontFamily, fixedFontFamily, String, String, "Courier") \
    macro(SansSerifFontFamily, sansSerifFontFamily, String, String, "Helvetica") \
    macro(SerifFontFamily, serifFontFamily, String, String, "Times") \
    macro(PictographFontFamily, pictographFontFamily, String, String, "Apple Color Emoji") \
    \

#elif PLATFORM(GTK) || PLATFORM(EFL)

#define FOR_EACH_WEBKIT_FONT_FAMILY_PREFERENCE(macro) \
    macro(StandardFontFamily, standardFontFamily, String, String, "Times") \
    macro(CursiveFontFamily, cursiveFontFamily, String, String, "Comic Sans MS") \
    macro(FantasyFontFamily, fantasyFontFamily, String, String, "Impact") \
    macro(FixedFontFamily, fixedFontFamily, String, String, "Courier New") \
    macro(SansSerifFontFamily, sansSerifFontFamily, String, String, "Helvetica") \
    macro(SerifFontFamily, serifFontFamily, String, String, "Times") \
    macro(PictographFontFamily, pictographFontFamily, String, String, "Times") \
    \

#endif

#define FOR_EACH_WEBKIT_STRING_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_FONT_FAMILY_PREFERENCE(macro) \
    macro(DefaultTextEncodingName, defaultTextEncodingName, String, String, defaultTextEncodingNameForSystemLanguage()) \
    macro(FTPDirectoryTemplatePath, ftpDirectoryTemplatePath, String, String, "") \
    \

#define FOR_EACH_WEBKIT_STRING_PREFERENCE_NOT_IN_WEBCORE(macro) \
    macro(InspectorWindowFrame, inspectorWindowFrame, String, String, "") \
    \

#define FOR_EACH_WEBKIT_DEBUG_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_DEBUG_BOOL_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_DEBUG_UINT32_PREFERENCE(macro) \
    \

#define FOR_EACH_WEBKIT_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_BOOL_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_DOUBLE_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_UINT32_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_STRING_PREFERENCE(macro) \
    FOR_EACH_WEBKIT_STRING_PREFERENCE_NOT_IN_WEBCORE(macro) \
    \


#endif // WebPreferencesDefinitions_h
