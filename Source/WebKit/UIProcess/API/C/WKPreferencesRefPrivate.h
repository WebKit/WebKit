/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#ifndef WKPreferencesPrivate_h
#define WKPreferencesPrivate_h

#include <WebKit/WKBase.h>
#include <WebKit/WKDeprecated.h>

#ifdef __cplusplus
extern "C" {
#endif

enum WKEditableLinkBehavior {
    kWKEditableLinkBehaviorDefault,
    kWKEditableLinkBehaviorAlwaysLive,
    kWKEditableLinkBehaviorOnlyLiveWithShiftKey,
    kWKEditableLinkBehaviorLiveWhenNotFocused,
    kWKEditableLinkBehaviorNeverLive
};
typedef enum WKEditableLinkBehavior WKEditableLinkBehavior;

enum WKJavaScriptRuntimeFlags {
    kWKJavaScriptRuntimeFlagsAllEnabled = 0
};
typedef unsigned WKJavaScriptRuntimeFlagSet;

// Creates a copy with no identifier.
WK_EXPORT WKPreferencesRef WKPreferencesCreateCopy(WKPreferencesRef);

WK_EXPORT void WKPreferencesStartBatchingUpdates(WKPreferencesRef);
WK_EXPORT void WKPreferencesEndBatchingUpdates(WKPreferencesRef);

WK_EXPORT void WKPreferencesEnableAllExperimentalFeatures(WKPreferencesRef);
WK_EXPORT void WKPreferencesSetExperimentalFeatureForKey(WKPreferencesRef, bool, WKStringRef);
WK_EXPORT void WKPreferencesResetAllInternalDebugFeatures(WKPreferencesRef);
WK_EXPORT void WKPreferencesSetInternalDebugFeatureForKey(WKPreferencesRef, bool, WKStringRef);

// The following generic setter functions are only intended for use by testing infrastructure.
WK_EXPORT void WKPreferencesSetBoolValueForKeyForTesting(WKPreferencesRef preferencesRef, bool value, WKStringRef key);
WK_EXPORT void WKPreferencesSetDoubleValueForKeyForTesting(WKPreferencesRef preferencesRef, double value, WKStringRef key);
WK_EXPORT void WKPreferencesSetUInt32ValueForKeyForTesting(WKPreferencesRef preferencesRef, uint32_t value, WKStringRef key);
WK_EXPORT void WKPreferencesSetStringValueForKeyForTesting(WKPreferencesRef preferencesRef, WKStringRef value, WKStringRef key);

// For Test Runner Use only.
WK_EXPORT void WKPreferencesResetTestRunnerOverrides(WKPreferencesRef preferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetSubpixelAntialiasedLayerTextEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetSubpixelAntialiasedLayerTextEnabled(WKPreferencesRef);

// Defaults to EditableLinkNeverLive.
WK_EXPORT void WKPreferencesSetEditableLinkBehavior(WKPreferencesRef preferencesRef, WKEditableLinkBehavior);
WK_EXPORT WKEditableLinkBehavior WKPreferencesGetEditableLinkBehavior(WKPreferencesRef preferencesRef);
    
// Defaults to false.
WK_EXPORT void WKPreferencesSetAcceleratedDrawingEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetAcceleratedDrawingEnabled(WKPreferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetCanvasUsesAcceleratedDrawing(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetCanvasUsesAcceleratedDrawing(WKPreferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetAcceleratedCompositingEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetAcceleratedCompositingEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetCompositingBordersVisible(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetCompositingBordersVisible(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetCompositingRepaintCountersVisible(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetCompositingRepaintCountersVisible(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetTiledScrollingIndicatorVisible(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetTiledScrollingIndicatorVisible(WKPreferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetWebGLEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetWebGLEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetAccelerated2DCanvasEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetAccelerated2DCanvasEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetNeedsSiteSpecificQuirks(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetNeedsSiteSpecificQuirks(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetForceFTPDirectoryListings(WKPreferencesRef preferences, bool force);
WK_EXPORT bool WKPreferencesGetForceFTPDirectoryListings(WKPreferencesRef preferences);

// Defaults to the empty string.
WK_EXPORT void WKPreferencesSetFTPDirectoryTemplatePath(WKPreferencesRef preferences, WKStringRef path);
WK_EXPORT WKStringRef WKPreferencesCopyFTPDirectoryTemplatePath(WKPreferencesRef preferences);

// Defaults to true.
WK_EXPORT void WKPreferencesSetDOMTimersThrottlingEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetDOMTimersThrottlingEnabled(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetWebArchiveDebugModeEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetWebArchiveDebugModeEnabled(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetLocalFileContentSniffingEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetLocalFileContentSniffingEnabled(WKPreferencesRef preferences);

// Defaults to true.
WK_EXPORT void WKPreferencesSetPageCacheEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetPageCacheEnabled(WKPreferencesRef preferences);

// Defaults to true.
WK_EXPORT void WKPreferencesSetPageCacheSupportsPlugins(WKPreferencesRef preferences, bool pageCacheSupportsPlugins);
WK_EXPORT bool WKPreferencesGetPageCacheSupportsPlugins(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetPaginateDuringLayoutEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetPaginateDuringLayoutEnabled(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetDOMPasteAllowed(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetDOMPasteAllowed(WKPreferencesRef preferences);

// Defaults to true.
WK_EXPORT void WKPreferencesSetWebSecurityEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetWebSecurityEnabled(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetUniversalAccessFromFileURLsAllowed(WKPreferencesRef preferences, bool allowed);
WK_EXPORT bool WKPreferencesGetUniversalAccessFromFileURLsAllowed(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetFileAccessFromFileURLsAllowed(WKPreferencesRef preferences, bool allowed);
WK_EXPORT bool WKPreferencesGetFileAccessFromFileURLsAllowed(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetTopNavigationToDataURLsAllowed(WKPreferencesRef preferences, bool allowed);
WK_EXPORT bool WKPreferencesGetTopNavigationToDataURLsAllowed(WKPreferencesRef preferences);

// Defaults to true
WK_EXPORT void WKPreferencesSetNeedsStorageAccessFromFileURLsQuirk(WKPreferencesRef preferences, bool needsQuirk);
WK_EXPORT bool WKPreferencesGetNeedsStorageAccessFromFileURLsQuirk(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetMediaPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetMediaPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetVideoPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetVideoPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetAudioPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetAudioPlaybackRequiresUserGesture(WKPreferencesRef preferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetMainContentUserGestureOverrideEnabled(WKPreferencesRef preferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetMainContentUserGestureOverrideEnabled(WKPreferencesRef preferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetMediaPlaybackAllowsInline(WKPreferencesRef preferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetMediaPlaybackAllowsInline(WKPreferencesRef preferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetInlineMediaPlaybackRequiresPlaysInlineAttribute(WKPreferencesRef preferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetInlineMediaPlaybackRequiresPlaysInlineAttribute(WKPreferencesRef preferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetBeaconAPIEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetBeaconAPIEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetDirectoryUploadEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetDirectoryUploadEnabled(WKPreferencesRef);

// Defaults to false on iOS, true elsewhere.
WK_EXPORT void WKPreferencesSetMediaControlsScaleWithPageZoom(WKPreferencesRef preferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetMediaControlsScaleWithPageZoom(WKPreferencesRef preferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetShowsToolTipOverTruncatedText(WKPreferencesRef preferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetShowsToolTipOverTruncatedText(WKPreferencesRef preferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetMockScrollbarsEnabled(WKPreferencesRef preferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetMockScrollbarsEnabled(WKPreferencesRef preferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetJavaEnabledForLocalFiles(WKPreferencesRef preferences, bool javaEnabled);
WK_EXPORT bool WKPreferencesGetJavaEnabledForLocalFiles(WKPreferencesRef preferences);

// Defaults to true.
WK_EXPORT void WKPreferencesSetRequestAnimationFrameEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetRequestAnimationFrameEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetDiagnosticLoggingEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetDiagnosticLoggingEnabled(WKPreferencesRef preferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetAsynchronousPluginInitializationEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetAsynchronousPluginInitializationEnabled(WKPreferencesRef preferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetAsynchronousPluginInitializationEnabledForAllPlugins(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetAsynchronousPluginInitializationEnabledForAllPlugins(WKPreferencesRef preferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetPluginSandboxProfilesEnabledForAllPlugins(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetPluginSandboxProfilesEnabledForAllPlugins(WKPreferencesRef preferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetArtificialPluginInitializationDelayEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetArtificialPluginInitializationDelayEnabled(WKPreferencesRef preferencesRef);

// Defaults to true
WK_EXPORT void WKPreferencesSetInteractiveFormValidationEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetInteractiveFormValidationEnabled(WKPreferencesRef preferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetScrollingPerformanceLoggingEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetScrollingPerformanceLoggingEnabled(WKPreferencesRef preferencesRef);

// Defaults to true
WK_EXPORT void WKPreferencesSetCookieEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetCookieEnabled(WKPreferencesRef preferences);

// Defaults to true on Mac, false elsewhere
WK_EXPORT void WKPreferencesSetPDFPluginEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetPDFPluginEnabled(WKPreferencesRef preferences);

// Defaults to false
WK_EXPORT void WKPreferencesSetAggressiveTileRetentionEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetAggressiveTileRetentionEnabled(WKPreferencesRef preferences);

// Defaults to false
WK_EXPORT void WKPreferencesSetLogsPageMessagesToSystemConsoleEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetLogsPageMessagesToSystemConsoleEnabled(WKPreferencesRef preferences);

// Defaults to true
WK_EXPORT void WKPreferencesSetPageVisibilityBasedProcessSuppressionEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetPageVisibilityBasedProcessSuppressionEnabled(WKPreferencesRef);

// Defaults to true
WK_EXPORT void WKPreferencesSetSmartInsertDeleteEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetSmartInsertDeleteEnabled(WKPreferencesRef preferences);

// Defaults to false
WK_EXPORT void WKPreferencesSetSelectTrailingWhitespaceEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetSelectTrailingWhitespaceEnabled(WKPreferencesRef preferences);

// Defaults to false
WK_EXPORT void WKPreferencesSetShowsURLsInToolTipsEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetShowsURLsInToolTipsEnabled(WKPreferencesRef preferences);

// Defaults to true on Mac, false on other platforms.
WK_EXPORT void WKPreferencesSetHiddenPageDOMTimerThrottlingEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetHiddenPageDOMTimerThrottlingEnabled(WKPreferencesRef preferences);

// Defaults to false
WK_EXPORT void WKPreferencesSetHiddenPageDOMTimerThrottlingAutoIncreases(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetHiddenPageDOMTimerThrottlingAutoIncreases(WKPreferencesRef preferences);

// Defaults to true on Mac, false on other platforms.
WK_EXPORT void WKPreferencesSetHiddenPageCSSAnimationSuspensionEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetHiddenPageCSSAnimationSuspensionEnabled(WKPreferencesRef preferences);

// Defaults to false
WK_EXPORT void WKPreferencesSetSnapshotAllPlugIns(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetSnapshotAllPlugIns(WKPreferencesRef preferencesRef);

// Defaults to true
WK_EXPORT void WKPreferencesSetAutostartOriginPlugInSnapshottingEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetAutostartOriginPlugInSnapshottingEnabled(WKPreferencesRef preferencesRef);

// Defaults to true
WK_EXPORT void WKPreferencesSetPrimaryPlugInSnapshotDetectionEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetPrimaryPlugInSnapshotDetectionEnabled(WKPreferencesRef preferencesRef);

// Defaults to true
WK_EXPORT void WKPreferencesSetThreadedScrollingEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetThreadedScrollingEnabled(WKPreferencesRef preferencesRef);

// Defaults to 5 seconds.
WK_EXPORT void WKPreferencesSetIncrementalRenderingSuppressionTimeout(WKPreferencesRef preferencesRef, double timeout);
WK_EXPORT double WKPreferencesGetIncrementalRenderingSuppressionTimeout(WKPreferencesRef preferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetSimpleLineLayoutEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetSimpleLineLayoutEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetLegacyLineLayoutVisualCoverageEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetLegacyLineLayoutVisualCoverageEnabled(WKPreferencesRef);

WK_EXPORT void WKPreferencesSetContentChangeObserverEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetContentChangeObserverEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetDeferredCSSParserEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetDeferredCSSParserEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetUseGiantTiles(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetUseGiantTiles(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetUseLegacyTextAlignPositionedElementBehavior(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesUseLegacyTextAlignPositionedElementBehavior(WKPreferencesRef preferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetMediaSourceEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetMediaSourceEnabled(WKPreferencesRef preferencesRef);

// Defaults to true;
WK_EXPORT void WKPreferencesSetSourceBufferChangeTypeEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetSourceBufferChangeTypeEnabled(WKPreferencesRef preferencesRef);

// Default to false.
WK_EXPORT void WKPreferencesSetViewGestureDebuggingEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetViewGestureDebuggingEnabled(WKPreferencesRef preferencesRef);

// Default to false.
WK_EXPORT void WKPreferencesSetShouldConvertPositionStyleOnCopy(WKPreferencesRef preferencesRef, bool convert);
WK_EXPORT bool WKPreferencesGetShouldConvertPositionStyleOnCopy(WKPreferencesRef preferencesRef);

// Default to false.
WK_EXPORT void WKPreferencesSetTelephoneNumberParsingEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetTelephoneNumberParsingEnabled(WKPreferencesRef preferencesRef);

// Default to false.
WK_EXPORT void WKPreferencesSetEnableInheritURIQueryComponent(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetEnableInheritURIQueryComponent(WKPreferencesRef preferencesRef);

// Default to false.
WK_EXPORT void WKPreferencesSetServiceControlsEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetServiceControlsEnabled(WKPreferencesRef preferencesRef);

// Default to false.
WK_EXPORT void WKPreferencesSetImageControlsEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetImageControlsEnabled(WKPreferencesRef preferencesRef);

// Default to false.
WK_EXPORT void WKPreferencesSetGamepadsEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetGamepadsEnabled(WKPreferencesRef preferencesRef);

// Default to false.
WK_EXPORT void WKPreferencesSetHighlightAPIEnabled(WKPreferencesRef preferencesRef, bool enabled);
WK_EXPORT bool WKPreferencesGetHighlightAPIEnabled(WKPreferencesRef preferencesRef);

// Defaults to 0. Setting this to 0 disables font autosizing on iOS.
WK_EXPORT void WKPreferencesSetMinimumZoomFontSize(WKPreferencesRef preferencesRef, double);
WK_EXPORT double WKPreferencesGetMinimumZoomFontSize(WKPreferencesRef preferencesRef);

// Defaults to 0.
WK_EXPORT void WKPreferencesSetJavaScriptRuntimeFlags(WKPreferencesRef preferences, WKJavaScriptRuntimeFlagSet javascriptRuntimeFlagSet);
WK_EXPORT WKJavaScriptRuntimeFlagSet WKPreferencesGetJavaScriptRuntimeFlags(WKPreferencesRef preferences);

// Defaults to true.
WK_EXPORT void WKPreferencesSetMetaRefreshEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetMetaRefreshEnabled(WKPreferencesRef preferences);

// Defaults to true.
WK_EXPORT void WKPreferencesSetHTTPEquivEnabled(WKPreferencesRef preferences, bool enabled);
WK_EXPORT bool WKPreferencesGetHTTPEquivEnabled(WKPreferencesRef preferences);

// Defaults to false.
WK_EXPORT void WKPreferencesSetResourceUsageOverlayVisible(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetResourceUsageOverlayVisible(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetMockCaptureDevicesEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetMockCaptureDevicesEnabled(WKPreferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetICECandidateFilteringEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetICECandidateFilteringEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetEnumeratingAllNetworkInterfacesEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetEnumeratingAllNetworkInterfacesEnabled(WKPreferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetMediaCaptureRequiresSecureConnection(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetMediaCaptureRequiresSecureConnection(WKPreferencesRef);

// Defaults to 1 minute on iOS, 10 minutes elsewhere
WK_EXPORT void WKPreferencesSetInactiveMediaCaptureSteamRepromptIntervalInMinutes(WKPreferencesRef, double);
WK_EXPORT double WKPreferencesGetInactiveMediaCaptureSteamRepromptIntervalInMinutes(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetDownloadAttributeEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetDownloadAttributeEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetSelectionPaintingWithoutSelectionGapsEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetSelectionPaintingWithoutSelectionGapsEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetAllowsPictureInPictureMediaPlayback(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetAllowsPictureInPictureMediaPlayback(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetAttachmentElementEnabled(WKPreferencesRef preferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetAttachmentElementEnabled(WKPreferencesRef preferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetIntersectionObserverEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetIntersectionObserverEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetMenuItemElementEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetMenuItemElementEnabled(WKPreferencesRef);
        
// Defaults to false
WK_EXPORT void WKPreferencesSetDataTransferItemsEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetDataTransferItemsEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetCustomPasteboardDataEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetCustomPasteboardDataEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetDialogElementEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetDialogElementEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetKeygenElementEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetKeygenElementEnabled(WKPreferencesRef);

// Defaults to false, true for iOS
WK_EXPORT void WKPreferencesSetWebShareEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetWebShareEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetPaintTimingEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetPaintTimingEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetFetchAPIKeepAliveEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetFetchAPIKeepAliveEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetIsNSURLSessionWebSocketEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetIsNSURLSessionWebSocketEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetWebRTCPlatformCodecsInGPUProcessEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetWebRTCPlatformCodecsInGPUProcessEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetIsAccessibilityIsolatedTreeEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetIsAccessibilityIsolatedTreeEnabled(WKPreferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetLargeImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetLargeImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetAnimatedImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetAnimatedImageAsyncDecodingEnabled(WKPreferencesRef preferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetShouldSuppressKeyboardInputDuringProvisionalNavigation(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetShouldSuppressKeyboardInputDuringProvisionalNavigation(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetLinkPreloadEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetLinkPreloadEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetMediaPreloadingEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetMediaPreloadingEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetExposeSpeakersEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetExposeSpeakersEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetWebAuthenticationEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetWebAuthenticationEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetWebAuthenticationModernEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetWebAuthenticationModernEnabled(WKPreferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetInvisibleMediaAutoplayPermitted(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetInvisibleMediaAutoplayPermitted(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetMediaUserGestureInheritsFromDocument(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetMediaUserGestureInheritsFromDocument(WKPreferencesRef);

// Defaults to an empty string
WK_EXPORT void WKPreferencesSetMediaContentTypesRequiringHardwareSupport(WKPreferencesRef, WKStringRef);
WK_EXPORT WKStringRef WKPreferencesCopyMediaContentTypesRequiringHardwareSupport(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetInspectorAdditionsEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetInspectorAdditionsEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetStorageAccessAPIEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetStorageAccessAPIEnabled(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetAccessibilityObjectModelEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetAccessibilityObjectModelEnabled(WKPreferencesRef);

// Defaults to true
WK_EXPORT void WKPreferencesSetAriaReflectionEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetAriaReflectionEnabled(WKPreferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetSyntheticEditingCommandsEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetSyntheticEditingCommandsEnabled(WKPreferencesRef);
    
// Defaults to false.
WK_EXPORT void WKPreferencesSetCSSOMViewScrollingAPIEnabled(WKPreferencesRef, bool);
WK_EXPORT bool WKPreferencesGetCSSOMViewScrollingAPIEnabled(WKPreferencesRef);

// Defaults to true.
WK_EXPORT void WKPreferencesSetShouldAllowUserInstalledFonts(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetShouldAllowUserInstalledFonts(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetAllowCrossOriginSubresourcesToAskForCredentials(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetAllowCrossOriginSubresourcesToAskForCredentials(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetServerTimingEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetServerTimingEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetColorFilterEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetColorFilterEnabled(WKPreferencesRef);

// Defaults to false.
WK_EXPORT void WKPreferencesSetPunchOutWhiteBackgroundsInDarkMode(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetPunchOutWhiteBackgroundsInDarkMode(WKPreferencesRef);

// Defaults to false
WK_EXPORT void WKPreferencesSetReferrerPolicyAttributeEnabled(WKPreferencesRef, bool flag);
WK_EXPORT bool WKPreferencesGetReferrerPolicyAttributeEnabled(WKPreferencesRef);


// The following are all deprecated and do nothing. They should be removed when possible.

WK_EXPORT void WKPreferencesSetNewBlockInsideInlineModelEnabled(WKPreferencesRef, bool) WK_C_API_DEPRECATED;
WK_EXPORT bool WKPreferencesGetNewBlockInsideInlineModelEnabled(WKPreferencesRef) WK_C_API_DEPRECATED;
WK_EXPORT void WKPreferencesSetLongMousePressEnabled(WKPreferencesRef, bool) WK_C_API_DEPRECATED;
WK_EXPORT bool WKPreferencesGetLongMousePressEnabled(WKPreferencesRef) WK_C_API_DEPRECATED;
WK_EXPORT void WKPreferencesSetAntialiasedFontDilationEnabled(WKPreferencesRef, bool) WK_C_API_DEPRECATED;
WK_EXPORT bool WKPreferencesGetAntialiasedFontDilationEnabled(WKPreferencesRef) WK_C_API_DEPRECATED;
WK_EXPORT void WKPreferencesSetApplicationChromeModeEnabled(WKPreferencesRef, bool) WK_C_API_DEPRECATED;
WK_EXPORT bool WKPreferencesGetApplicationChromeModeEnabled(WKPreferencesRef) WK_C_API_DEPRECATED;
WK_EXPORT void WKPreferencesSetInspectorUsesWebKitUserInterface(WKPreferencesRef, bool) WK_C_API_DEPRECATED;
WK_EXPORT bool WKPreferencesGetInspectorUsesWebKitUserInterface(WKPreferencesRef) WK_C_API_DEPRECATED;
WK_EXPORT void WKPreferencesSetHixie76WebSocketProtocolEnabled(WKPreferencesRef, bool) WK_C_API_DEPRECATED;
WK_EXPORT bool WKPreferencesGetHixie76WebSocketProtocolEnabled(WKPreferencesRef) WK_C_API_DEPRECATED;
WK_EXPORT void WKPreferencesSetFetchAPIEnabled(WKPreferencesRef, bool) WK_C_API_DEPRECATED;
WK_EXPORT bool WKPreferencesGetFetchAPIEnabled(WKPreferencesRef) WK_C_API_DEPRECATED;
WK_EXPORT void WKPreferencesSetIsSecureContextAttributeEnabled(WKPreferencesRef, bool) WK_C_API_DEPRECATED;
WK_EXPORT bool WKPreferencesGetIsSecureContextAttributeEnabled(WKPreferencesRef) WK_C_API_DEPRECATED;
WK_EXPORT void WKPreferencesSetUserTimingEnabled(WKPreferencesRef, bool) WK_C_API_DEPRECATED;
WK_EXPORT bool WKPreferencesGetUserTimingEnabled(WKPreferencesRef) WK_C_API_DEPRECATED;
WK_EXPORT void WKPreferencesSetResourceTimingEnabled(WKPreferencesRef, bool) WK_C_API_DEPRECATED;
WK_EXPORT bool WKPreferencesGetResourceTimingEnabled(WKPreferencesRef) WK_C_API_DEPRECATED;
WK_EXPORT void WKPreferencesSetSubpixelCSSOMElementMetricsEnabled(WKPreferencesRef, bool) WK_C_API_DEPRECATED;
WK_EXPORT bool WKPreferencesGetSubpixelCSSOMElementMetricsEnabled(WKPreferencesRef) WK_C_API_DEPRECATED;

#ifdef __cplusplus
}
#endif

#endif /* WKPreferencesPrivate_h */
