/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKPreferencesInternal.h"

#import "APIArray.h"
#import "Logging.h"
#import "WKNSArray.h"
#import "WebPreferences.h"
#import "_WKExperimentalFeatureInternal.h"
#import "_WKInternalDebugFeatureInternal.h"
#import <WebCore/SecurityOrigin.h>
#import <WebCore/Settings.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/RetainPtr.h>

@implementation WKPreferences

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebPreferences>(self, String(), "WebKit"_s, "WebKitDebug"_s);
    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKPreferences.class, self))
        return;

    _preferences->~WebPreferences();

    [super dealloc];
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

// FIXME: We currently only encode/decode API preferences. We should consider whether we should
// encode/decode SPI preferences as well.

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeDouble:self.minimumFontSize forKey:@"minimumFontSize"];
    [coder encodeBool:self.javaScriptCanOpenWindowsAutomatically forKey:@"javaScriptCanOpenWindowsAutomatically"];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [coder encodeBool:self.javaScriptEnabled forKey:@"javaScriptEnabled"];
ALLOW_DEPRECATED_DECLARATIONS_END
    
    [coder encodeBool:self.shouldPrintBackgrounds forKey:@"shouldPrintBackgrounds"];

#if PLATFORM(MAC)
    [coder encodeBool:self.tabFocusesLinks forKey:@"tabFocusesLinks"];
#endif
    [coder encodeBool:self.textInteractionEnabled forKey:@"textInteractionEnabled"];
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [self init]))
        return nil;

    self.minimumFontSize = [coder decodeDoubleForKey:@"minimumFontSize"];
    self.javaScriptCanOpenWindowsAutomatically = [coder decodeBoolForKey:@"javaScriptCanOpenWindowsAutomatically"];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    self.javaScriptEnabled = [coder decodeBoolForKey:@"javaScriptEnabled"];
ALLOW_DEPRECATED_DECLARATIONS_END
    
    self.shouldPrintBackgrounds = [coder decodeBoolForKey:@"shouldPrintBackgrounds"];

#if PLATFORM(MAC)
    self.tabFocusesLinks = [coder decodeBoolForKey:@"tabFocusesLinks"];
#endif
    if ([coder containsValueForKey:@"textInteractionEnabled"])
        self.textInteractionEnabled = [coder decodeBoolForKey:@"textInteractionEnabled"];

    return self;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [wrapper(_preferences->copy()) retain];
}

- (CGFloat)minimumFontSize
{
    return _preferences->minimumFontSize();
}

- (void)setMinimumFontSize:(CGFloat)minimumFontSize
{
    _preferences->setMinimumFontSize(minimumFontSize);
}

- (void)setFraudulentWebsiteWarningEnabled:(BOOL)enabled
{
    _preferences->setSafeBrowsingEnabled(enabled);
}

- (BOOL)isFraudulentWebsiteWarningEnabled
{
    return _preferences->safeBrowsingEnabled();
}

- (BOOL)javaScriptCanOpenWindowsAutomatically
{
    return _preferences->javaScriptCanOpenWindowsAutomatically();
}

- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)javaScriptCanOpenWindowsAutomatically
{
    _preferences->setJavaScriptCanOpenWindowsAutomatically(javaScriptCanOpenWindowsAutomatically);
}

- (void)setShouldPrintBackgrounds:(BOOL)enabled
{
    _preferences->setShouldPrintBackgrounds(enabled);
}

- (BOOL)shouldPrintBackgrounds
{
    return _preferences->shouldPrintBackgrounds();
}

- (BOOL)isTextInteractionEnabled
{
    return _preferences->textInteractionEnabled();
}

- (void)setTextInteractionEnabled:(BOOL)textInteractionEnabled
{
    _preferences->setTextInteractionEnabled(textInteractionEnabled);
}

- (BOOL)isSiteSpecificQuirksModeEnabled
{
    return _preferences->needsSiteSpecificQuirks();
}

- (void)setSiteSpecificQuirksModeEnabled:(BOOL)enabled
{
    _preferences->setNeedsSiteSpecificQuirks(enabled);
}

- (BOOL)isElementFullscreenEnabled
{
    return _preferences->fullScreenEnabled();
}

- (void)setElementFullscreenEnabled:(BOOL)elementFullscreenEnabled
{
    _preferences->setFullScreenEnabled(elementFullscreenEnabled);
}

#pragma mark OS X-specific methods

#if PLATFORM(MAC)

- (BOOL)tabFocusesLinks
{
    return _preferences->tabsToLinks();
}

- (void)setTabFocusesLinks:(BOOL)tabFocusesLinks
{
    _preferences->setTabsToLinks(tabFocusesLinks);
}

#endif

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_preferences;
}

@end

@implementation WKPreferences (WKPrivate)

- (BOOL)_telephoneNumberDetectionIsEnabled
{
    return _preferences->telephoneNumberParsingEnabled();
}

- (void)_setTelephoneNumberDetectionIsEnabled:(BOOL)telephoneNumberDetectionIsEnabled
{
    _preferences->setTelephoneNumberParsingEnabled(telephoneNumberDetectionIsEnabled);
}

static WebCore::StorageBlockingPolicy toStorageBlockingPolicy(_WKStorageBlockingPolicy policy)
{
    switch (policy) {
    case _WKStorageBlockingPolicyAllowAll:
        return WebCore::StorageBlockingPolicy::AllowAll;
    case _WKStorageBlockingPolicyBlockThirdParty:
        return WebCore::StorageBlockingPolicy::BlockThirdParty;
    case _WKStorageBlockingPolicyBlockAll:
        return WebCore::StorageBlockingPolicy::BlockAll;
    }

    ASSERT_NOT_REACHED();
    return WebCore::StorageBlockingPolicy::AllowAll;
}

static _WKStorageBlockingPolicy toAPI(WebCore::StorageBlockingPolicy policy)
{
    switch (policy) {
    case WebCore::StorageBlockingPolicy::AllowAll:
        return _WKStorageBlockingPolicyAllowAll;
    case WebCore::StorageBlockingPolicy::BlockThirdParty:
        return _WKStorageBlockingPolicyBlockThirdParty;
    case WebCore::StorageBlockingPolicy::BlockAll:
        return _WKStorageBlockingPolicyBlockAll;
    }

    ASSERT_NOT_REACHED();
    return _WKStorageBlockingPolicyAllowAll;
}

- (_WKStorageBlockingPolicy)_storageBlockingPolicy
{
    return toAPI(static_cast<WebCore::StorageBlockingPolicy>(_preferences->storageBlockingPolicy()));
}

- (void)_setStorageBlockingPolicy:(_WKStorageBlockingPolicy)policy
{
    _preferences->setStorageBlockingPolicy(static_cast<uint32_t>(toStorageBlockingPolicy(policy)));
}

- (BOOL)_offlineApplicationCacheIsEnabled
{
    return _preferences->offlineWebApplicationCacheEnabled();
}

- (void)_setOfflineApplicationCacheIsEnabled:(BOOL)offlineApplicationCacheIsEnabled
{
    _preferences->setOfflineWebApplicationCacheEnabled(offlineApplicationCacheIsEnabled);
}

- (BOOL)_fullScreenEnabled
{
    return _preferences->fullScreenEnabled();
}

- (void)_setFullScreenEnabled:(BOOL)fullScreenEnabled
{
    _preferences->setFullScreenEnabled(fullScreenEnabled);
}

- (BOOL)_allowsPictureInPictureMediaPlayback
{
    return _preferences->allowsPictureInPictureMediaPlayback();
}

- (void)_setAllowsPictureInPictureMediaPlayback:(BOOL)allowed
{
    _preferences->setAllowsPictureInPictureMediaPlayback(allowed);
}

- (BOOL)_compositingBordersVisible
{
    return _preferences->compositingBordersVisible();
}

- (void)_setCompositingBordersVisible:(BOOL)compositingBordersVisible
{
    _preferences->setCompositingBordersVisible(compositingBordersVisible);
}

- (BOOL)_compositingRepaintCountersVisible
{
    return _preferences->compositingRepaintCountersVisible();
}

- (void)_setCompositingRepaintCountersVisible:(BOOL)repaintCountersVisible
{
    _preferences->setCompositingRepaintCountersVisible(repaintCountersVisible);
}

- (BOOL)_tiledScrollingIndicatorVisible
{
    return _preferences->tiledScrollingIndicatorVisible();
}

- (void)_setTiledScrollingIndicatorVisible:(BOOL)tiledScrollingIndicatorVisible
{
    _preferences->setTiledScrollingIndicatorVisible(tiledScrollingIndicatorVisible);
}

- (BOOL)_resourceUsageOverlayVisible
{
    return _preferences->resourceUsageOverlayVisible();
}

- (void)_setResourceUsageOverlayVisible:(BOOL)resourceUsageOverlayVisible
{
    _preferences->setResourceUsageOverlayVisible(resourceUsageOverlayVisible);
}

- (_WKDebugOverlayRegions)_visibleDebugOverlayRegions
{
    return _preferences->visibleDebugOverlayRegions();
}

- (void)_setVisibleDebugOverlayRegions:(_WKDebugOverlayRegions)regionFlags
{
    _preferences->setVisibleDebugOverlayRegions(regionFlags);
}

- (BOOL)_legacyLineLayoutVisualCoverageEnabled
{
    return _preferences->legacyLineLayoutVisualCoverageEnabled();
}

- (void)_setLegacyLineLayoutVisualCoverageEnabled:(BOOL)legacyLineLayoutVisualCoverageEnabled
{
    _preferences->setLegacyLineLayoutVisualCoverageEnabled(legacyLineLayoutVisualCoverageEnabled);
}

- (BOOL)_contentChangeObserverEnabled
{
    return _preferences->contentChangeObserverEnabled();
}

- (void)_setContentChangeObserverEnabled:(BOOL)contentChangeObserverEnabled
{
    _preferences->setContentChangeObserverEnabled(contentChangeObserverEnabled);
}

- (BOOL)_acceleratedDrawingEnabled
{
    return _preferences->acceleratedDrawingEnabled();
}

- (void)_setAcceleratedDrawingEnabled:(BOOL)acceleratedDrawingEnabled
{
    _preferences->setAcceleratedDrawingEnabled(acceleratedDrawingEnabled);
}

- (BOOL)_displayListDrawingEnabled
{
    return _preferences->displayListDrawingEnabled();
}

- (void)_setDisplayListDrawingEnabled:(BOOL)displayListDrawingEnabled
{
    _preferences->setDisplayListDrawingEnabled(displayListDrawingEnabled);
}

- (BOOL)_largeImageAsyncDecodingEnabled
{
    return _preferences->largeImageAsyncDecodingEnabled();
}

- (void)_setLargeImageAsyncDecodingEnabled:(BOOL)_largeImageAsyncDecodingEnabled
{
    _preferences->setLargeImageAsyncDecodingEnabled(_largeImageAsyncDecodingEnabled);
}

- (BOOL)_needsInAppBrowserPrivacyQuirks
{
    return _preferences->needsInAppBrowserPrivacyQuirks();
}

- (void)_setNeedsInAppBrowserPrivacyQuirks:(BOOL)enabled
{
    _preferences->setNeedsInAppBrowserPrivacyQuirks(enabled);
}

- (BOOL)_animatedImageAsyncDecodingEnabled
{
    return _preferences->animatedImageAsyncDecodingEnabled();
}

- (void)_setAnimatedImageAsyncDecodingEnabled:(BOOL)_animatedImageAsyncDecodingEnabled
{
    _preferences->setAnimatedImageAsyncDecodingEnabled(_animatedImageAsyncDecodingEnabled);
}

- (BOOL)_textAutosizingEnabled
{
    return _preferences->textAutosizingEnabled();
}

- (void)_setTextAutosizingEnabled:(BOOL)enabled
{
    _preferences->setTextAutosizingEnabled(enabled);
}

- (BOOL)_subpixelAntialiasedLayerTextEnabled
{
    return _preferences->subpixelAntialiasedLayerTextEnabled();
}

- (void)_setSubpixelAntialiasedLayerTextEnabled:(BOOL)enabled
{
    _preferences->setSubpixelAntialiasedLayerTextEnabled(enabled);
}

- (BOOL)_developerExtrasEnabled
{
    return _preferences->developerExtrasEnabled();
}

- (void)_setDeveloperExtrasEnabled:(BOOL)developerExtrasEnabled
{
    _preferences->setDeveloperExtrasEnabled(developerExtrasEnabled);
}

- (BOOL)_logsPageMessagesToSystemConsoleEnabled
{
    return _preferences->logsPageMessagesToSystemConsoleEnabled();
}

- (void)_setLogsPageMessagesToSystemConsoleEnabled:(BOOL)logsPageMessagesToSystemConsoleEnabled
{
    _preferences->setLogsPageMessagesToSystemConsoleEnabled(logsPageMessagesToSystemConsoleEnabled);
}

- (BOOL)_hiddenPageDOMTimerThrottlingEnabled
{
    return _preferences->hiddenPageDOMTimerThrottlingEnabled();
}

- (void)_setHiddenPageDOMTimerThrottlingEnabled:(BOOL)hiddenPageDOMTimerThrottlingEnabled
{
    _preferences->setHiddenPageDOMTimerThrottlingEnabled(hiddenPageDOMTimerThrottlingEnabled);
}

- (BOOL)_hiddenPageDOMTimerThrottlingAutoIncreases
{
    return _preferences->hiddenPageDOMTimerThrottlingAutoIncreases();
}

- (void)_setHiddenPageDOMTimerThrottlingAutoIncreases:(BOOL)hiddenPageDOMTimerThrottlingAutoIncreases
{
    _preferences->setHiddenPageDOMTimerThrottlingAutoIncreases(hiddenPageDOMTimerThrottlingAutoIncreases);
}

- (BOOL)_pageVisibilityBasedProcessSuppressionEnabled
{
    return _preferences->pageVisibilityBasedProcessSuppressionEnabled();
}

- (void)_setPageVisibilityBasedProcessSuppressionEnabled:(BOOL)pageVisibilityBasedProcessSuppressionEnabled
{
    _preferences->setPageVisibilityBasedProcessSuppressionEnabled(pageVisibilityBasedProcessSuppressionEnabled);
}

- (BOOL)_allowFileAccessFromFileURLs
{
    return _preferences->allowFileAccessFromFileURLs();
}

- (void)_setAllowFileAccessFromFileURLs:(BOOL)allowFileAccessFromFileURLs
{
    _preferences->setAllowFileAccessFromFileURLs(allowFileAccessFromFileURLs);
}

- (_WKJavaScriptRuntimeFlags)_javaScriptRuntimeFlags
{
    return _preferences->javaScriptRuntimeFlags();
}

- (void)_setJavaScriptRuntimeFlags:(_WKJavaScriptRuntimeFlags)javaScriptRuntimeFlags
{
    _preferences->setJavaScriptRuntimeFlags(javaScriptRuntimeFlags);
}

- (BOOL)_isStandalone
{
    return _preferences->standalone();
}

- (void)_setStandalone:(BOOL)standalone
{
    _preferences->setStandalone(standalone);
}

- (BOOL)_diagnosticLoggingEnabled
{
    return _preferences->diagnosticLoggingEnabled();
}

- (void)_setDiagnosticLoggingEnabled:(BOOL)diagnosticLoggingEnabled
{
    _preferences->setDiagnosticLoggingEnabled(diagnosticLoggingEnabled);
}

- (NSUInteger)_defaultFontSize
{
    return _preferences->defaultFontSize();
}

- (void)_setDefaultFontSize:(NSUInteger)defaultFontSize
{
    _preferences->setDefaultFontSize(defaultFontSize);
}

- (NSUInteger)_defaultFixedPitchFontSize
{
    return _preferences->defaultFixedFontSize();
}

- (void)_setDefaultFixedPitchFontSize:(NSUInteger)defaultFixedPitchFontSize
{
    _preferences->setDefaultFixedFontSize(defaultFixedPitchFontSize);
}

- (NSString *)_fixedPitchFontFamily
{
    return _preferences->fixedFontFamily();
}

- (void)_setFixedPitchFontFamily:(NSString *)fixedPitchFontFamily
{
    _preferences->setFixedFontFamily(fixedPitchFontFamily);
}

+ (NSArray<_WKInternalDebugFeature *> *)_internalDebugFeatures
{
    auto features = WebKit::WebPreferences::internalDebugFeatures();
    return wrapper(API::Array::create(WTFMove(features)));
}

- (BOOL)_isEnabledForInternalDebugFeature:(_WKInternalDebugFeature *)feature
{
    return _preferences->isFeatureEnabled(*feature->_internalDebugFeature);
}

- (void)_setEnabled:(BOOL)value forInternalDebugFeature:(_WKInternalDebugFeature *)feature
{
    _preferences->setFeatureEnabled(*feature->_internalDebugFeature, value);
}

+ (NSArray<_WKExperimentalFeature *> *)_experimentalFeatures
{
    auto features = WebKit::WebPreferences::experimentalFeatures();
    return wrapper(API::Array::create(WTFMove(features)));
}

// FIXME: Remove this once Safari has adopted the new API.
- (BOOL)_isEnabledForFeature:(_WKExperimentalFeature *)feature
{
    return [self _isEnabledForExperimentalFeature:feature];
}

// FIXME: Remove this once Safari has adopted the new API.
- (void)_setEnabled:(BOOL)value forFeature:(_WKExperimentalFeature *)feature
{
    [self _setEnabled:value forExperimentalFeature:feature];
}

- (BOOL)_isEnabledForExperimentalFeature:(_WKExperimentalFeature *)feature
{
    return _preferences->isFeatureEnabled(*feature->_experimentalFeature);
}

- (void)_setEnabled:(BOOL)value forExperimentalFeature:(_WKExperimentalFeature *)feature
{
    _preferences->setFeatureEnabled(*feature->_experimentalFeature, value);
}

- (BOOL)_applePayCapabilityDisclosureAllowed
{
#if ENABLE(APPLE_PAY)
    return _preferences->applePayCapabilityDisclosureAllowed();
#else
    return NO;
#endif
}

- (void)_setApplePayCapabilityDisclosureAllowed:(BOOL)applePayCapabilityDisclosureAllowed
{
#if ENABLE(APPLE_PAY)
    _preferences->setApplePayCapabilityDisclosureAllowed(applePayCapabilityDisclosureAllowed);
#endif
}

- (BOOL)_shouldSuppressKeyboardInputDuringProvisionalNavigation
{
    return _preferences->shouldSuppressTextInputFromEditingDuringProvisionalNavigation();
}

- (void)_setShouldSuppressKeyboardInputDuringProvisionalNavigation:(BOOL)shouldSuppress
{
    _preferences->setShouldSuppressTextInputFromEditingDuringProvisionalNavigation(shouldSuppress);
}

- (BOOL)_loadsImagesAutomatically
{
    return _preferences->loadsImagesAutomatically();
}

- (void)_setLoadsImagesAutomatically:(BOOL)loadsImagesAutomatically
{
    _preferences->setLoadsImagesAutomatically(loadsImagesAutomatically);
}

- (BOOL)_peerConnectionEnabled
{
    return _preferences->peerConnectionEnabled();
}

- (void)_setPeerConnectionEnabled:(BOOL)enabled
{
    _preferences->setPeerConnectionEnabled(enabled);
}

- (BOOL)_mediaDevicesEnabled
{
    return _preferences->mediaDevicesEnabled();
}

- (void)_setMediaDevicesEnabled:(BOOL)enabled
{
    _preferences->setMediaDevicesEnabled(enabled);
}

- (BOOL)_getUserMediaRequiresFocus
{
    return _preferences->getUserMediaRequiresFocus();
}

- (void)_setGetUserMediaRequiresFocus:(BOOL)enabled
{
    _preferences->setGetUserMediaRequiresFocus(enabled);
}

- (BOOL)_screenCaptureEnabled
{
    return _preferences->screenCaptureEnabled();
}

- (void)_setScreenCaptureEnabled:(BOOL)enabled
{
    _preferences->setScreenCaptureEnabled(enabled);
}

- (BOOL)_mockCaptureDevicesEnabled
{
    return _preferences->mockCaptureDevicesEnabled();
}

- (void)_setMockCaptureDevicesEnabled:(BOOL)enabled
{
    _preferences->setMockCaptureDevicesEnabled(enabled);
}

- (BOOL)_mockCaptureDevicesPromptEnabled
{
    return _preferences->mockCaptureDevicesPromptEnabled();
}

- (void)_setMockCaptureDevicesPromptEnabled:(BOOL)enabled
{
    _preferences->setMockCaptureDevicesPromptEnabled(enabled);
}

- (BOOL)_mediaCaptureRequiresSecureConnection
{
    return _preferences->mediaCaptureRequiresSecureConnection();
}

- (void)_setMediaCaptureRequiresSecureConnection:(BOOL)requiresSecureConnection
{
    _preferences->setMediaCaptureRequiresSecureConnection(requiresSecureConnection);
}

- (double)_inactiveMediaCaptureSteamRepromptIntervalInMinutes
{
    return _preferences->inactiveMediaCaptureSteamRepromptIntervalInMinutes();
}

- (void)_setInactiveMediaCaptureSteamRepromptIntervalInMinutes:(double)interval
{
    _preferences->setInactiveMediaCaptureSteamRepromptIntervalInMinutes(interval);
}

- (BOOL)_interruptAudioOnPageVisibilityChangeEnabled
{
    return _preferences->interruptAudioOnPageVisibilityChangeEnabled();
}

- (void)_setInterruptAudioOnPageVisibilityChangeEnabled:(BOOL)enabled
{
    _preferences->setInterruptAudioOnPageVisibilityChangeEnabled(enabled);
}

- (BOOL)_enumeratingAllNetworkInterfacesEnabled
{
    return _preferences->enumeratingAllNetworkInterfacesEnabled();
}

- (void)_setEnumeratingAllNetworkInterfacesEnabled:(BOOL)enabled
{
    _preferences->setEnumeratingAllNetworkInterfacesEnabled(enabled);
}

- (BOOL)_iceCandidateFilteringEnabled
{
    return _preferences->iceCandidateFilteringEnabled();
}

- (void)_setICECandidateFilteringEnabled:(BOOL)enabled
{
    _preferences->setICECandidateFilteringEnabled(enabled);
}

- (BOOL)_webRTCLegacyAPIEnabled
{
    return NO;
}

- (void)_setWebRTCLegacyAPIEnabled:(BOOL)enabled
{
}

- (void)_setJavaScriptCanAccessClipboard:(BOOL)javaScriptCanAccessClipboard
{
    _preferences->setJavaScriptCanAccessClipboard(javaScriptCanAccessClipboard);
}

- (BOOL)_shouldAllowUserInstalledFonts
{
    return _preferences->shouldAllowUserInstalledFonts();
}

- (void)_setShouldAllowUserInstalledFonts:(BOOL)_shouldAllowUserInstalledFonts
{
    _preferences->setShouldAllowUserInstalledFonts(_shouldAllowUserInstalledFonts);
}

- (BOOL)_shouldAllowDesignSystemUIFonts
{
    // These fonts are always enabled. This function only exists for binary compatibility.
    return YES;
}

- (void)_setShouldAllowDesignSystemUIFonts:(BOOL)_shouldAllowDesignSystemUIFonts
{
    // These fonts are always enabled. This function only exists for binary compatibility.
}

static _WKEditableLinkBehavior toAPI(WebCore::EditableLinkBehavior behavior)
{
    switch (behavior) {
    case WebCore::EditableLinkBehavior::Default:
        return _WKEditableLinkBehaviorDefault;
    case WebCore::EditableLinkBehavior::AlwaysLive:
        return _WKEditableLinkBehaviorAlwaysLive;
    case WebCore::EditableLinkBehavior::OnlyLiveWithShiftKey:
        return _WKEditableLinkBehaviorOnlyLiveWithShiftKey;
    case WebCore::EditableLinkBehavior::LiveWhenNotFocused:
        return _WKEditableLinkBehaviorLiveWhenNotFocused;
    case WebCore::EditableLinkBehavior::NeverLive:
        return _WKEditableLinkBehaviorNeverLive;
    }
    
    ASSERT_NOT_REACHED();
    return _WKEditableLinkBehaviorNeverLive;
}

static WebCore::EditableLinkBehavior toEditableLinkBehavior(_WKEditableLinkBehavior wkBehavior)
{
    switch (wkBehavior) {
    case _WKEditableLinkBehaviorDefault:
        return WebCore::EditableLinkBehavior::Default;
    case _WKEditableLinkBehaviorAlwaysLive:
        return WebCore::EditableLinkBehavior::AlwaysLive;
    case _WKEditableLinkBehaviorOnlyLiveWithShiftKey:
        return WebCore::EditableLinkBehavior::OnlyLiveWithShiftKey;
    case _WKEditableLinkBehaviorLiveWhenNotFocused:
        return WebCore::EditableLinkBehavior::LiveWhenNotFocused;
    case _WKEditableLinkBehaviorNeverLive:
        return WebCore::EditableLinkBehavior::NeverLive;
    }
    
    ASSERT_NOT_REACHED();
    return WebCore::EditableLinkBehavior::NeverLive;
}

- (_WKEditableLinkBehavior)_editableLinkBehavior
{
    return toAPI(static_cast<WebCore::EditableLinkBehavior>(_preferences->editableLinkBehavior()));
}

- (void)_setEditableLinkBehavior:(_WKEditableLinkBehavior)editableLinkBehavior
{
    _preferences->setEditableLinkBehavior(static_cast<uint32_t>(toEditableLinkBehavior(editableLinkBehavior)));
}

- (void)_setAVFoundationEnabled:(BOOL)enabled
{
    _preferences->setAVFoundationEnabled(enabled);
}

- (BOOL)_avFoundationEnabled
{
    return _preferences->isAVFoundationEnabled();
}

- (void)_setColorFilterEnabled:(BOOL)enabled
{
    _preferences->setColorFilterEnabled(enabled);
}

- (BOOL)_colorFilterEnabled
{
    return _preferences->colorFilterEnabled();
}

- (void)_setPunchOutWhiteBackgroundsInDarkMode:(BOOL)punches
{
    _preferences->setPunchOutWhiteBackgroundsInDarkMode(punches);
}

- (BOOL)_punchOutWhiteBackgroundsInDarkMode
{
    return _preferences->punchOutWhiteBackgroundsInDarkMode();
}

- (void)_setLowPowerVideoAudioBufferSizeEnabled:(BOOL)enabled
{
    _preferences->setLowPowerVideoAudioBufferSizeEnabled(enabled);
}

- (BOOL)_lowPowerVideoAudioBufferSizeEnabled
{
    return _preferences->lowPowerVideoAudioBufferSizeEnabled();
}

- (void)_setShouldIgnoreMetaViewport:(BOOL)ignoreMetaViewport
{
    return _preferences->setShouldIgnoreMetaViewport(ignoreMetaViewport);
}

- (BOOL)_shouldIgnoreMetaViewport
{
    return _preferences->shouldIgnoreMetaViewport();
}

- (void)_setNeedsSiteSpecificQuirks:(BOOL)enabled
{
    _preferences->setNeedsSiteSpecificQuirks(enabled);
}

- (BOOL)_needsSiteSpecificQuirks
{
    return _preferences->needsSiteSpecificQuirks();
}

- (void)_setItpDebugModeEnabled:(BOOL)enabled
{
    _preferences->setItpDebugModeEnabled(enabled);
}

- (BOOL)_itpDebugModeEnabled
{
    return _preferences->itpDebugModeEnabled();
}

- (void)_setMediaSourceEnabled:(BOOL)enabled
{
    _preferences->setMediaSourceEnabled(enabled);
}

- (BOOL)_mediaSourceEnabled
{
    return _preferences->mediaSourceEnabled();
}

- (BOOL)_secureContextChecksEnabled
{
    return _preferences->secureContextChecksEnabled();
}

- (void)_setSecureContextChecksEnabled:(BOOL)enabled
{
    _preferences->setSecureContextChecksEnabled(enabled);
}

- (void)_setWebAudioEnabled:(BOOL)enabled
{
    _preferences->setWebAudioEnabled(enabled);
}

- (BOOL)_webAudioEnabled
{
    return _preferences->webAudioEnabled();
}

- (void)_setAcceleratedCompositingEnabled:(BOOL)enabled
{
    _preferences->setAcceleratedCompositingEnabled(enabled);
}

- (BOOL)_acceleratedCompositingEnabled
{
    return _preferences->acceleratedCompositingEnabled();
}

- (BOOL)_remotePlaybackEnabled
{
    return _preferences->remotePlaybackEnabled();
}

- (void)_setRemotePlaybackEnabled:(BOOL)enabled
{
    _preferences->setRemotePlaybackEnabled(enabled);
}

- (BOOL)_serviceWorkerEntitlementDisabledForTesting
{
    return _preferences->serviceWorkerEntitlementDisabledForTesting();
}

- (void)_setServiceWorkerEntitlementDisabledForTesting:(BOOL)disable
{
    _preferences->setServiceWorkerEntitlementDisabledForTesting(disable);
}

#if PLATFORM(MAC)
- (void)_setCanvasUsesAcceleratedDrawing:(BOOL)enabled
{
    _preferences->setCanvasUsesAcceleratedDrawing(enabled);
}

- (BOOL)_canvasUsesAcceleratedDrawing
{
    return _preferences->canvasUsesAcceleratedDrawing();
}

- (void)_setDefaultTextEncodingName:(NSString *)name
{
    _preferences->setDefaultTextEncodingName(name);
}

- (NSString *)_defaultTextEncodingName
{
    return _preferences->defaultTextEncodingName();
}

- (void)_setAuthorAndUserStylesEnabled:(BOOL)enabled
{
    _preferences->setAuthorAndUserStylesEnabled(enabled);
}

- (BOOL)_authorAndUserStylesEnabled
{
    return _preferences->authorAndUserStylesEnabled();
}

- (void)_setDOMTimersThrottlingEnabled:(BOOL)enabled
{
    _preferences->setDOMTimersThrottlingEnabled(enabled);
}

- (BOOL)_domTimersThrottlingEnabled
{
    return _preferences->domTimersThrottlingEnabled();
}

- (void)_setWebArchiveTestingModeEnabled:(BOOL)enabled
{
    _preferences->setWebArchiveTestingModeEnabled(enabled);
}

- (BOOL)_webArchiveTestingModeEnabled
{
    return _preferences->webArchiveTestingModeEnabled();
}

- (void)_setLocalFileContentSniffingEnabled:(BOOL)enabled
{
    _preferences->setLocalFileContentSniffingEnabled(enabled);
}

- (BOOL)_localFileContentSniffingEnabled
{
    return _preferences->localFileContentSniffingEnabled();
}

- (void)_setUsesPageCache:(BOOL)enabled
{
    _preferences->setUsesBackForwardCache(enabled);
}

- (BOOL)_usesPageCache
{
    return _preferences->usesBackForwardCache();
}

- (void)_setPageCacheSupportsPlugins:(BOOL)enabled
{
}

- (BOOL)_pageCacheSupportsPlugins
{
    return NO;
}

- (void)_setShouldPrintBackgrounds:(BOOL)enabled
{
    self.shouldPrintBackgrounds = enabled;
}

- (BOOL)_shouldPrintBackgrounds
{
    return self.shouldPrintBackgrounds;
}

- (void)_setWebSecurityEnabled:(BOOL)enabled
{
    _preferences->setWebSecurityEnabled(enabled);
}

- (BOOL)_webSecurityEnabled
{
    return _preferences->webSecurityEnabled();
}

- (void)_setUniversalAccessFromFileURLsAllowed:(BOOL)enabled
{
    _preferences->setAllowUniversalAccessFromFileURLs(enabled);
}

- (BOOL)_universalAccessFromFileURLsAllowed
{
    return _preferences->allowUniversalAccessFromFileURLs();
}

- (void)_setTopNavigationToDataURLsAllowed:(BOOL)enabled
{
    _preferences->setAllowTopNavigationToDataURLs(enabled);
}

- (BOOL)_topNavigationToDataURLsAllowed
{
    return _preferences->allowTopNavigationToDataURLs();
}

- (void)_setSuppressesIncrementalRendering:(BOOL)enabled
{
    _preferences->setSuppressesIncrementalRendering(enabled);
}

- (BOOL)_suppressesIncrementalRendering
{
    return _preferences->suppressesIncrementalRendering();
}

- (void)_setAsynchronousPluginInitializationEnabled:(BOOL)enabled
{
}

- (BOOL)_asynchronousPluginInitializationEnabled
{
    return NO;
}

- (void)_setArtificialPluginInitializationDelayEnabled:(BOOL)enabled
{
}

- (BOOL)_artificialPluginInitializationDelayEnabled
{
    return NO;
}

- (void)_setExperimentalPlugInSandboxProfilesEnabled:(BOOL)enabled
{
}

- (BOOL)_experimentalPlugInSandboxProfilesEnabled
{
    return NO;
}

- (void)_setCookieEnabled:(BOOL)enabled
{
    _preferences->setCookieEnabled(enabled);
}

- (BOOL)_cookieEnabled
{
    return _preferences->cookieEnabled();
}

- (void)_setPlugInSnapshottingEnabled:(BOOL)enabled
{
}

- (BOOL)_plugInSnapshottingEnabled
{
    return NO;
}

- (void)_setViewGestureDebuggingEnabled:(BOOL)enabled
{
    _preferences->setViewGestureDebuggingEnabled(enabled);
}

- (BOOL)_viewGestureDebuggingEnabled
{
    return _preferences->viewGestureDebuggingEnabled();
}

- (void)_setStandardFontFamily:(NSString *)family
{
    _preferences->setStandardFontFamily(family);
}

- (NSString *)_standardFontFamily
{
    return _preferences->standardFontFamily();
}

- (void)_setBackspaceKeyNavigationEnabled:(BOOL)enabled
{
    _preferences->setBackspaceKeyNavigationEnabled(enabled);
}

- (BOOL)_backspaceKeyNavigationEnabled
{
    return _preferences->backspaceKeyNavigationEnabled();
}

- (void)_setWebGLEnabled:(BOOL)enabled
{
    _preferences->setWebGLEnabled(enabled);
}

- (BOOL)_webGLEnabled
{
    return _preferences->webGLEnabled();
}

- (void)_setAllowsInlineMediaPlayback:(BOOL)enabled
{
    _preferences->setAllowsInlineMediaPlayback(enabled);
}

- (BOOL)_allowsInlineMediaPlayback
{
    return _preferences->allowsInlineMediaPlayback();
}

- (void)_setApplePayEnabled:(BOOL)enabled
{
    _preferences->setApplePayEnabled(enabled);
}

- (BOOL)_applePayEnabled
{
    return _preferences->applePayEnabled();
}

- (void)_setDNSPrefetchingEnabled:(BOOL)enabled
{
    _preferences->setDNSPrefetchingEnabled(enabled);
}

- (BOOL)_dnsPrefetchingEnabled
{
    return _preferences->dnsPrefetchingEnabled();
}

- (void)_setInlineMediaPlaybackRequiresPlaysInlineAttribute:(BOOL)enabled
{
    _preferences->setInlineMediaPlaybackRequiresPlaysInlineAttribute(enabled);
}

- (BOOL)_inlineMediaPlaybackRequiresPlaysInlineAttribute
{
    return _preferences->inlineMediaPlaybackRequiresPlaysInlineAttribute();
}

- (void)_setInvisibleMediaAutoplayNotPermitted:(BOOL)enabled
{
    _preferences->setInvisibleAutoplayNotPermitted(enabled);
}

- (BOOL)_invisibleMediaAutoplayNotPermitted
{
    return _preferences->invisibleAutoplayNotPermitted();
}

- (void)_setLegacyEncryptedMediaAPIEnabled:(BOOL)enabled
{
    _preferences->setLegacyEncryptedMediaAPIEnabled(enabled);
}

- (BOOL)_legacyEncryptedMediaAPIEnabled
{
    return _preferences->legacyEncryptedMediaAPIEnabled();
}

- (void)_setMainContentUserGestureOverrideEnabled:(BOOL)enabled
{
    _preferences->setMainContentUserGestureOverrideEnabled(enabled);
}

- (BOOL)_mainContentUserGestureOverrideEnabled
{
    return _preferences->mainContentUserGestureOverrideEnabled();
}

- (void)_setMediaStreamEnabled:(BOOL)enabled
{
    _preferences->setMediaStreamEnabled(enabled);
}

- (BOOL)_mediaStreamEnabled
{
    return _preferences->mediaStreamEnabled();
}

- (void)_setNeedsStorageAccessFromFileURLsQuirk:(BOOL)enabled
{
    _preferences->setNeedsStorageAccessFromFileURLsQuirk(enabled);
}

- (BOOL)_needsStorageAccessFromFileURLsQuirk
{
    return _preferences->needsStorageAccessFromFileURLsQuirk();
}

- (void)_setPDFPluginEnabled:(BOOL)enabled
{
    _preferences->setPDFPluginEnabled(enabled);
}

- (BOOL)_pdfPluginEnabled
{
    return _preferences->pdfPluginEnabled();
}

- (void)_setRequiresUserGestureForAudioPlayback:(BOOL)enabled
{
    _preferences->setRequiresUserGestureForAudioPlayback(enabled);
}

- (BOOL)_requiresUserGestureForAudioPlayback
{
    return _preferences->requiresUserGestureForAudioPlayback();
}

- (void)_setRequiresUserGestureForVideoPlayback:(BOOL)enabled
{
    _preferences->setRequiresUserGestureForVideoPlayback(enabled);
}

- (BOOL)_requiresUserGestureForVideoPlayback
{
    return _preferences->requiresUserGestureForVideoPlayback();
}

- (void)_setServiceControlsEnabled:(BOOL)enabled
{
    _preferences->setServiceControlsEnabled(enabled);
}

- (BOOL)_serviceControlsEnabled
{
    return _preferences->serviceControlsEnabled();
}

- (void)_setShowsToolTipOverTruncatedText:(BOOL)enabled
{
    _preferences->setShowsToolTipOverTruncatedText(enabled);
}

- (BOOL)_showsToolTipOverTruncatedText
{
    return _preferences->showsToolTipOverTruncatedText();
}

- (void)_setTextAreasAreResizable:(BOOL)enabled
{
    _preferences->setTextAreasAreResizable(enabled);
}

- (BOOL)_textAreasAreResizable
{
    return _preferences->textAreasAreResizable();
}

- (void)_setUseGiantTiles:(BOOL)enabled
{
    _preferences->setUseGiantTiles(enabled);
}

- (BOOL)_useGiantTiles
{
    return _preferences->useGiantTiles();
}

- (void)_setWantsBalancedSetDefersLoadingBehavior:(BOOL)enabled
{
    _preferences->setWantsBalancedSetDefersLoadingBehavior(enabled);
}

- (BOOL)_wantsBalancedSetDefersLoadingBehavior
{
    return _preferences->wantsBalancedSetDefersLoadingBehavior();
}

- (void)_setAggressiveTileRetentionEnabled:(BOOL)enabled
{
    _preferences->setAggressiveTileRetentionEnabled(enabled);
}

- (BOOL)_aggressiveTileRetentionEnabled
{
    return _preferences->aggressiveTileRetentionEnabled();
}

- (void)_setAppNapEnabled:(BOOL)enabled
{
    _preferences->setPageVisibilityBasedProcessSuppressionEnabled(enabled);
}

- (BOOL)_appNapEnabled
{
    return _preferences->pageVisibilityBasedProcessSuppressionEnabled();
}

#endif // PLATFORM(MAC)

- (BOOL)_javaScriptCanAccessClipboard
{
    return _preferences->javaScriptCanAccessClipboard();
}

- (void)_setDOMPasteAllowed:(BOOL)domPasteAllowed
{
    _preferences->setDOMPasteAllowed(domPasteAllowed);
}

- (BOOL)_domPasteAllowed
{
    return _preferences->domPasteAllowed();
}

- (void)_setShouldEnableTextAutosizingBoost:(BOOL)shouldEnableTextAutosizingBoost
{
#if ENABLE(TEXT_AUTOSIZING)
    _preferences->setShouldEnableTextAutosizingBoost(shouldEnableTextAutosizingBoost);
#endif
}

- (BOOL)_shouldEnableTextAutosizingBoost
{
#if ENABLE(TEXT_AUTOSIZING)
    return _preferences->shouldEnableTextAutosizingBoost();
#else
    return NO;
#endif
}

- (BOOL)_isSafeBrowsingEnabled
{
    return _preferences->safeBrowsingEnabled();
}

- (void)_setSafeBrowsingEnabled:(BOOL)enabled
{
    _preferences->setSafeBrowsingEnabled(enabled);
}

- (void)_setVideoQualityIncludesDisplayCompositingEnabled:(BOOL)videoQualityIncludesDisplayCompositingEnabled
{
    _preferences->setVideoQualityIncludesDisplayCompositingEnabled(videoQualityIncludesDisplayCompositingEnabled);
}

- (BOOL)_videoQualityIncludesDisplayCompositingEnabled
{
    return _preferences->videoQualityIncludesDisplayCompositingEnabled();
}

- (void)_setDeviceOrientationEventEnabled:(BOOL)enabled
{
#if ENABLE(DEVICE_ORIENTATION)
    _preferences->setDeviceOrientationEventEnabled(enabled);
#endif
}

- (BOOL)_deviceOrientationEventEnabled
{
#if ENABLE(DEVICE_ORIENTATION)
    return _preferences->deviceOrientationEventEnabled();
#else
    return false;
#endif
}

- (void)_setAccessibilityIsolatedTreeEnabled:(BOOL)accessibilityIsolatedTreeEnabled
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    _preferences->setIsAccessibilityIsolatedTreeEnabled(accessibilityIsolatedTreeEnabled);
#endif
}

- (BOOL)_accessibilityIsolatedTreeEnabled
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    return _preferences->isAccessibilityIsolatedTreeEnabled();
#else
    return false;
#endif
}

- (BOOL)_speechRecognitionEnabled
{
    return _preferences->speechRecognitionEnabled();
}

- (void)_setSpeechRecognitionEnabled:(BOOL)speechRecognitionEnabled
{
    _preferences->setSpeechRecognitionEnabled(speechRecognitionEnabled);
}

- (BOOL)_privateClickMeasurementEnabled
{
    return _preferences->privateClickMeasurementEnabled();
}

- (void)_setPrivateClickMeasurementEnabled:(BOOL)privateClickMeasurementEnabled
{
    _preferences->setPrivateClickMeasurementEnabled(privateClickMeasurementEnabled);
}

- (_WKPitchCorrectionAlgorithm)_pitchCorrectionAlgorithm
{
    return static_cast<_WKPitchCorrectionAlgorithm>(_preferences->pitchCorrectionAlgorithm());
}

- (void)_setPitchCorrectionAlgorithm:(_WKPitchCorrectionAlgorithm)pitchCorrectionAlgorithm
{
    _preferences->setPitchCorrectionAlgorithm(pitchCorrectionAlgorithm);
}

- (BOOL)_mediaSessionEnabled
{
    return _preferences->mediaSessionEnabled();
}

- (void)_setMediaSessionEnabled:(BOOL)mediaSessionEnabled
{
    _preferences->setMediaSessionEnabled(mediaSessionEnabled);
}

- (BOOL)_isExtensibleSSOEnabled
{
#if HAVE(APP_SSO)
    return _preferences->isExtensibleSSOEnabled();
#else
    return false;
#endif
}

- (void)_setExtensibleSSOEnabled:(BOOL)extensibleSSOEnabled
{
#if HAVE(APP_SSO)
    _preferences->setExtensibleSSOEnabled(extensibleSSOEnabled);
#endif
}

- (BOOL)_requiresPageVisibilityToPlayAudio
{
    return _preferences->requiresPageVisibilityToPlayAudio();
}

- (void)_setRequiresPageVisibilityToPlayAudio:(BOOL)requiresVisibility
{
    _preferences->setRequiresPageVisibilityToPlayAudio(requiresVisibility);
}

- (BOOL)_fileSystemAccessEnabled
{
    return _preferences->fileSystemAccessEnabled();
}

- (void)_setFileSystemAccessEnabled:(BOOL)fileSystemAccessEnabled
{
    _preferences->setFileSystemAccessEnabled(fileSystemAccessEnabled);
}

- (BOOL)_storageAPIEnabled
{
    return _preferences->storageAPIEnabled();
}

- (void)_setStorageAPIEnabled:(BOOL)storageAPIEnabled
{
    _preferences->setStorageAPIEnabled(storageAPIEnabled);
}

- (BOOL)_accessHandleEnabled
{
    return _preferences->accessHandleEnabled();
}

- (void)_setAccessHandleEnabled:(BOOL)accessHandleEnabled
{
    _preferences->setAccessHandleEnabled(accessHandleEnabled);
}

- (void)_setNotificationsEnabled:(BOOL)enabled
{
    _preferences->setNotificationsEnabled(enabled);
}

- (BOOL)_notificationsEnabled
{
    return _preferences->notificationsEnabled();
}

- (BOOL)_pushAPIEnabled
{
    return _preferences->pushAPIEnabled();
}

- (void)_setPushAPIEnabled:(BOOL)pushAPIEnabled
{
    _preferences->setPushAPIEnabled(pushAPIEnabled);
}

- (void)_setModelDocumentEnabled:(BOOL)enabled
{
    _preferences->setModelDocumentEnabled(enabled);
}

- (BOOL)_modelDocumentEnabled
{
    return _preferences->modelDocumentEnabled();
}

- (void)_setRequiresFullscreenToLockScreenOrientation:(BOOL)enabled
{
    _preferences->setFullscreenRequirementForScreenOrientationLockingEnabled(enabled);
}

- (BOOL)_requiresFullscreenToLockScreenOrientation
{
    return _preferences->fullscreenRequirementForScreenOrientationLockingEnabled();
}

- (void)_setInteractionRegionMinimumCornerRadius:(double)radius
{
    _preferences->setInteractionRegionMinimumCornerRadius(radius);
}

- (double)_interactionRegionMinimumCornerRadius
{
    return _preferences->interactionRegionMinimumCornerRadius();
}

- (void)_setInteractionRegionInlinePadding:(double)padding
{
    _preferences->setInteractionRegionInlinePadding(padding);
}

- (double)_interactionRegionInlinePadding
{
    return _preferences->interactionRegionInlinePadding();
}

- (void)_setMediaPreferredFullscreenWidth:(double)width
{
    _preferences->setMediaPreferredFullscreenWidth(width);
}

- (double)_mediaPreferredFullscreenWidth
{
    return _preferences->mediaPreferredFullscreenWidth();
}

@end

@implementation WKPreferences (WKDeprecated)

#if !TARGET_OS_IPHONE

- (BOOL)javaEnabled
{
    return NO;
}

- (void)setJavaEnabled:(BOOL)javaEnabled
{
}

- (BOOL)plugInsEnabled
{
    return NO;
}

- (void)setPlugInsEnabled:(BOOL)plugInsEnabled
{
    if (plugInsEnabled)
        RELEASE_LOG_FAULT(Plugins, "Application attempted to enable NPAPI plug ins, which are no longer supported");
}

#endif

- (BOOL)javaScriptEnabled
{
    return _preferences->javaScriptEnabled();
}

- (void)setJavaScriptEnabled:(BOOL)javaScriptEnabled
{
    _preferences->setJavaScriptEnabled(javaScriptEnabled);
}

@end

@implementation WKPreferences (WKPrivateDeprecated)

- (void)_setRequestAnimationFrameEnabled:(BOOL)enabled
{
}

- (BOOL)_requestAnimationFrameEnabled
{
    return YES;
}

#if !TARGET_OS_IPHONE

- (void)_setSubpixelCSSOMElementMetricsEnabled:(BOOL)enabled
{
}

- (BOOL)_subpixelCSSOMElementMetricsEnabled
{
    return NO;
}

- (void)_setWebArchiveDebugModeEnabled:(BOOL)enabled
{
}

- (BOOL)_webArchiveDebugModeEnabled
{
    return NO;
}

#endif

#if PLATFORM(MAC)
- (void)_setJavaEnabledForLocalFiles:(BOOL)enabled
{
}

- (BOOL)_javaEnabledForLocalFiles
{
    return NO;
}
#endif

@end
