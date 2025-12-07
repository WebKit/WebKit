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
#import "WKTextExtractionUtilities.h"
#import "WebPreferences.h"
#import "_WKFeatureInternal.h"
#import <WebCore/SecurityOrigin.h>
#import <WebCore/Settings.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/RetainPtr.h>

static Ref<WebKit::WebPreferences> protectedPreferences(WKPreferences *preferences)
{
    return *preferences->_preferences;
}

@implementation WKPreferences

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

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

    SUPPRESS_UNRETAINED_ARG _preferences->~WebPreferences();

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

    [coder encodeBool:self.tabFocusesLinks forKey:@"tabFocusesLinks"];
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

    self.tabFocusesLinks = [coder decodeBoolForKey:@"tabFocusesLinks"];
    if ([coder containsValueForKey:@"textInteractionEnabled"])
        self.textInteractionEnabled = [coder decodeBoolForKey:@"textInteractionEnabled"];

    return self;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [wrapper(protectedPreferences(self)->copy()) retain];
}

- (CGFloat)minimumFontSize
{
    return protectedPreferences(self)->minimumFontSize();
}

- (void)setMinimumFontSize:(CGFloat)minimumFontSize
{
    protectedPreferences(self)->setMinimumFontSize(minimumFontSize);
}

- (void)setFraudulentWebsiteWarningEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setSafeBrowsingEnabled(enabled);
}

- (BOOL)isFraudulentWebsiteWarningEnabled
{
    return protectedPreferences(self)->safeBrowsingEnabled();
}

- (BOOL)javaScriptCanOpenWindowsAutomatically
{
    return protectedPreferences(self)->javaScriptCanOpenWindowsAutomatically();
}

- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)javaScriptCanOpenWindowsAutomatically
{
    protectedPreferences(self)->setJavaScriptCanOpenWindowsAutomatically(javaScriptCanOpenWindowsAutomatically);
}

- (void)setShouldPrintBackgrounds:(BOOL)enabled
{
    protectedPreferences(self)->setShouldPrintBackgrounds(enabled);
}

- (BOOL)shouldPrintBackgrounds
{
    return protectedPreferences(self)->shouldPrintBackgrounds();
}

- (BOOL)isTextInteractionEnabled
{
    return protectedPreferences(self)->textInteractionEnabled();
}

- (void)setTextInteractionEnabled:(BOOL)textInteractionEnabled
{
    protectedPreferences(self)->setTextInteractionEnabled(textInteractionEnabled);
}

- (BOOL)isSiteSpecificQuirksModeEnabled
{
    return protectedPreferences(self)->needsSiteSpecificQuirks();
}

- (void)setSiteSpecificQuirksModeEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setNeedsSiteSpecificQuirks(enabled);
}

- (BOOL)isElementFullscreenEnabled
{
    return protectedPreferences(self)->fullScreenEnabled();
}

- (void)setElementFullscreenEnabled:(BOOL)elementFullscreenEnabled
{
    protectedPreferences(self)->setFullScreenEnabled(elementFullscreenEnabled);
}

- (void)setInactiveSchedulingPolicy:(WKInactiveSchedulingPolicy)policy
{
    switch (policy) {
    case WKInactiveSchedulingPolicySuspend:
        protectedPreferences(self)->setShouldTakeNearSuspendedAssertions(false);
        protectedPreferences(self)->setBackgroundWebContentRunningBoardThrottlingEnabled(true);
        protectedPreferences(self)->setShouldDropNearSuspendedAssertionAfterDelay(WebKit::defaultShouldDropNearSuspendedAssertionAfterDelay());
        break;
    case WKInactiveSchedulingPolicyThrottle:
        protectedPreferences(self)->setShouldTakeNearSuspendedAssertions(true);
        protectedPreferences(self)->setBackgroundWebContentRunningBoardThrottlingEnabled(true);
        protectedPreferences(self)->setShouldDropNearSuspendedAssertionAfterDelay(false);
        break;
    case WKInactiveSchedulingPolicyNone:
        protectedPreferences(self)->setShouldTakeNearSuspendedAssertions(true);
        protectedPreferences(self)->setBackgroundWebContentRunningBoardThrottlingEnabled(false);
        protectedPreferences(self)->setShouldDropNearSuspendedAssertionAfterDelay(false);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

- (WKInactiveSchedulingPolicy)inactiveSchedulingPolicy
{
    return protectedPreferences(self)->backgroundWebContentRunningBoardThrottlingEnabled() ? (protectedPreferences(self)->shouldTakeNearSuspendedAssertions() ? WKInactiveSchedulingPolicyThrottle : WKInactiveSchedulingPolicySuspend) : WKInactiveSchedulingPolicyNone;
}

- (BOOL)tabFocusesLinks
{
    return protectedPreferences(self)->tabsToLinks();
}

- (void)setTabFocusesLinks:(BOOL)tabFocusesLinks
{
    protectedPreferences(self)->setTabsToLinks(tabFocusesLinks);
}

- (BOOL)_useSystemAppearance
{
    return protectedPreferences(self)->useSystemAppearance();
}

- (void)_setUseSystemAppearance:(BOOL)useSystemAppearance
{
    protectedPreferences(self)->setUseSystemAppearance(useSystemAppearance);
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_preferences;
}

#if PLATFORM(VISION)
- (void)setIsLookToScrollEnabled:(BOOL)enabled
{
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    protectedPreferences(self)->setOverlayRegionsEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

- (BOOL)isLookToScrollEnabled
{
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    return protectedPreferences(self)->overlayRegionsEnabled();
#else
    return NO;
#endif
}
#endif

@end

@implementation WKPreferences (WKPrivate)

- (BOOL)_telephoneNumberDetectionIsEnabled
{
    return protectedPreferences(self)->telephoneNumberParsingEnabled();
}

- (void)_setTelephoneNumberDetectionIsEnabled:(BOOL)telephoneNumberDetectionIsEnabled
{
    protectedPreferences(self)->setTelephoneNumberParsingEnabled(telephoneNumberDetectionIsEnabled);
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
    return toAPI(static_cast<WebCore::StorageBlockingPolicy>(protectedPreferences(self)->storageBlockingPolicy()));
}

- (void)_setStorageBlockingPolicy:(_WKStorageBlockingPolicy)policy
{
    protectedPreferences(self)->setStorageBlockingPolicy(static_cast<uint32_t>(toStorageBlockingPolicy(policy)));
}

- (BOOL)_fullScreenEnabled
{
    return protectedPreferences(self)->fullScreenEnabled();
}

- (void)_setFullScreenEnabled:(BOOL)fullScreenEnabled
{
    protectedPreferences(self)->setFullScreenEnabled(fullScreenEnabled);
}

- (BOOL)_allowsPictureInPictureMediaPlayback
{
    return protectedPreferences(self)->allowsPictureInPictureMediaPlayback();
}

- (void)_setAllowsPictureInPictureMediaPlayback:(BOOL)allowed
{
    protectedPreferences(self)->setAllowsPictureInPictureMediaPlayback(allowed);
}

- (BOOL)_compositingBordersVisible
{
    return protectedPreferences(self)->compositingBordersVisible();
}

- (void)_setCompositingBordersVisible:(BOOL)compositingBordersVisible
{
    protectedPreferences(self)->setCompositingBordersVisible(compositingBordersVisible);
}

- (BOOL)_compositingRepaintCountersVisible
{
    return protectedPreferences(self)->compositingRepaintCountersVisible();
}

- (void)_setCompositingRepaintCountersVisible:(BOOL)repaintCountersVisible
{
    protectedPreferences(self)->setCompositingRepaintCountersVisible(repaintCountersVisible);
}

- (BOOL)_tiledScrollingIndicatorVisible
{
    return protectedPreferences(self)->tiledScrollingIndicatorVisible();
}

- (void)_setTiledScrollingIndicatorVisible:(BOOL)tiledScrollingIndicatorVisible
{
    protectedPreferences(self)->setTiledScrollingIndicatorVisible(tiledScrollingIndicatorVisible);
}

- (BOOL)_resourceUsageOverlayVisible
{
    return protectedPreferences(self)->resourceUsageOverlayVisible();
}

- (void)_setResourceUsageOverlayVisible:(BOOL)resourceUsageOverlayVisible
{
    protectedPreferences(self)->setResourceUsageOverlayVisible(resourceUsageOverlayVisible);
}

- (_WKDebugOverlayRegions)_visibleDebugOverlayRegions
{
    return protectedPreferences(self)->visibleDebugOverlayRegions();
}

- (void)_setVisibleDebugOverlayRegions:(_WKDebugOverlayRegions)regionFlags
{
    protectedPreferences(self)->setVisibleDebugOverlayRegions(regionFlags);
}

- (BOOL)_legacyLineLayoutVisualCoverageEnabled
{
    return protectedPreferences(self)->legacyLineLayoutVisualCoverageEnabled();
}

- (void)_setLegacyLineLayoutVisualCoverageEnabled:(BOOL)legacyLineLayoutVisualCoverageEnabled
{
    protectedPreferences(self)->setLegacyLineLayoutVisualCoverageEnabled(legacyLineLayoutVisualCoverageEnabled);
}

- (BOOL)_contentChangeObserverEnabled
{
    return protectedPreferences(self)->contentChangeObserverEnabled();
}

- (void)_setContentChangeObserverEnabled:(BOOL)contentChangeObserverEnabled
{
    protectedPreferences(self)->setContentChangeObserverEnabled(contentChangeObserverEnabled);
}

- (BOOL)_acceleratedDrawingEnabled
{
    return protectedPreferences(self)->acceleratedDrawingEnabled();
}

- (void)_setAcceleratedDrawingEnabled:(BOOL)acceleratedDrawingEnabled
{
    protectedPreferences(self)->setAcceleratedDrawingEnabled(acceleratedDrawingEnabled);
}

- (BOOL)_largeImageAsyncDecodingEnabled
{
    return protectedPreferences(self)->largeImageAsyncDecodingEnabled();
}

- (void)_setLargeImageAsyncDecodingEnabled:(BOOL)_largeImageAsyncDecodingEnabled
{
    protectedPreferences(self)->setLargeImageAsyncDecodingEnabled(_largeImageAsyncDecodingEnabled);
}

- (BOOL)_needsInAppBrowserPrivacyQuirks
{
    return protectedPreferences(self)->needsInAppBrowserPrivacyQuirks();
}

- (void)_setNeedsInAppBrowserPrivacyQuirks:(BOOL)enabled
{
    protectedPreferences(self)->setNeedsInAppBrowserPrivacyQuirks(enabled);
}

- (BOOL)_animatedImageAsyncDecodingEnabled
{
    return protectedPreferences(self)->animatedImageAsyncDecodingEnabled();
}

- (void)_setAnimatedImageAsyncDecodingEnabled:(BOOL)_animatedImageAsyncDecodingEnabled
{
    protectedPreferences(self)->setAnimatedImageAsyncDecodingEnabled(_animatedImageAsyncDecodingEnabled);
}

- (BOOL)_textAutosizingEnabled
{
    return protectedPreferences(self)->textAutosizingEnabled();
}

- (void)_setTextAutosizingEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setTextAutosizingEnabled(enabled);
}

- (BOOL)_developerExtrasEnabled
{
    return protectedPreferences(self)->developerExtrasEnabled();
}

- (void)_setDeveloperExtrasEnabled:(BOOL)developerExtrasEnabled
{
    protectedPreferences(self)->setDeveloperExtrasEnabled(developerExtrasEnabled);
}

- (BOOL)_logsPageMessagesToSystemConsoleEnabled
{
    return protectedPreferences(self)->logsPageMessagesToSystemConsoleEnabled();
}

- (void)_setLogsPageMessagesToSystemConsoleEnabled:(BOOL)logsPageMessagesToSystemConsoleEnabled
{
    protectedPreferences(self)->setLogsPageMessagesToSystemConsoleEnabled(logsPageMessagesToSystemConsoleEnabled);
}

- (BOOL)_hiddenPageDOMTimerThrottlingEnabled
{
    return protectedPreferences(self)->hiddenPageDOMTimerThrottlingEnabled();
}

- (void)_setHiddenPageDOMTimerThrottlingEnabled:(BOOL)hiddenPageDOMTimerThrottlingEnabled
{
    protectedPreferences(self)->setHiddenPageDOMTimerThrottlingEnabled(hiddenPageDOMTimerThrottlingEnabled);
}

- (BOOL)_hiddenPageDOMTimerThrottlingAutoIncreases
{
    return protectedPreferences(self)->hiddenPageDOMTimerThrottlingAutoIncreases();
}

- (void)_setHiddenPageDOMTimerThrottlingAutoIncreases:(BOOL)hiddenPageDOMTimerThrottlingAutoIncreases
{
    protectedPreferences(self)->setHiddenPageDOMTimerThrottlingAutoIncreases(hiddenPageDOMTimerThrottlingAutoIncreases);
}

- (BOOL)_pageVisibilityBasedProcessSuppressionEnabled
{
    return protectedPreferences(self)->pageVisibilityBasedProcessSuppressionEnabled();
}

- (void)_setPageVisibilityBasedProcessSuppressionEnabled:(BOOL)pageVisibilityBasedProcessSuppressionEnabled
{
    protectedPreferences(self)->setPageVisibilityBasedProcessSuppressionEnabled(pageVisibilityBasedProcessSuppressionEnabled);
}

- (BOOL)_allowFileAccessFromFileURLs
{
    return protectedPreferences(self)->allowFileAccessFromFileURLs();
}

- (void)_setAllowFileAccessFromFileURLs:(BOOL)allowFileAccessFromFileURLs
{
    protectedPreferences(self)->setAllowFileAccessFromFileURLs(allowFileAccessFromFileURLs);
}

- (_WKJavaScriptRuntimeFlags)_javaScriptRuntimeFlags
{
    return protectedPreferences(self)->javaScriptRuntimeFlags();
}

- (void)_setJavaScriptRuntimeFlags:(_WKJavaScriptRuntimeFlags)javaScriptRuntimeFlags
{
    protectedPreferences(self)->setJavaScriptRuntimeFlags(javaScriptRuntimeFlags);
}

- (BOOL)_isStandalone
{
    return protectedPreferences(self)->standalone();
}

- (void)_setStandalone:(BOOL)standalone
{
    protectedPreferences(self)->setStandalone(standalone);
}

- (BOOL)_diagnosticLoggingEnabled
{
    return protectedPreferences(self)->diagnosticLoggingEnabled();
}

- (void)_setDiagnosticLoggingEnabled:(BOOL)diagnosticLoggingEnabled
{
    protectedPreferences(self)->setDiagnosticLoggingEnabled(diagnosticLoggingEnabled);
}

- (NSUInteger)_defaultFontSize
{
    return protectedPreferences(self)->defaultFontSize();
}

- (void)_setDefaultFontSize:(NSUInteger)defaultFontSize
{
    protectedPreferences(self)->setDefaultFontSize(defaultFontSize);
}

- (NSUInteger)_defaultFixedPitchFontSize
{
    return protectedPreferences(self)->defaultFixedFontSize();
}

- (void)_setDefaultFixedPitchFontSize:(NSUInteger)defaultFixedPitchFontSize
{
    protectedPreferences(self)->setDefaultFixedFontSize(defaultFixedPitchFontSize);
}

- (NSString *)_fixedPitchFontFamily
{
    return protectedPreferences(self)->fixedFontFamily().createNSString().autorelease();
}

- (void)_setFixedPitchFontFamily:(NSString *)fixedPitchFontFamily
{
    protectedPreferences(self)->setFixedFontFamily(fixedPitchFontFamily);
}

+ (NSArray<_WKFeature *> *)_features
{
    auto features = WebKit::WebPreferences::features();
    return wrapper(API::Array::create(WTFMove(features))).autorelease();
}

+ (NSArray<_WKFeature *> *)_internalDebugFeatures
{
    auto features = WebKit::WebPreferences::internalDebugFeatures();
    return wrapper(API::Array::create(WTFMove(features))).autorelease();
}

- (BOOL)_isEnabledForInternalDebugFeature:(_WKFeature *)feature
{
    return protectedPreferences(self)->isFeatureEnabled(Ref { *feature->_wrappedFeature });
}

- (void)_setEnabled:(BOOL)value forInternalDebugFeature:(_WKFeature *)feature
{
    protectedPreferences(self)->setFeatureEnabled(Ref { *feature->_wrappedFeature }, value);
}

+ (NSArray<_WKExperimentalFeature *> *)_experimentalFeatures
{
    auto features = WebKit::WebPreferences::experimentalFeatures();
    return wrapper(API::Array::create(WTFMove(features))).autorelease();
}

- (BOOL)_isEnabledForFeature:(_WKFeature *)feature
{
    return protectedPreferences(self)->isFeatureEnabled(Ref { *feature->_wrappedFeature });
}

- (void)_setEnabled:(BOOL)value forFeature:(_WKFeature *)feature
{
    protectedPreferences(self)->setFeatureEnabled(Ref { *feature->_wrappedFeature }, value);
}

- (BOOL)_isEnabledForExperimentalFeature:(_WKFeature *)feature
{
    return [self _isEnabledForFeature:feature];
}

- (void)_setEnabled:(BOOL)value forExperimentalFeature:(_WKFeature *)feature
{
    [self _setEnabled:value forFeature:feature];
}

- (void)_disableRichJavaScriptFeatures
{
    protectedPreferences(self)->disableRichJavaScriptFeatures();
}

- (void)_disableMediaPlaybackRelatedFeatures
{
    protectedPreferences(self)->disableMediaPlaybackRelatedFeatures();
}

- (BOOL)_applePayCapabilityDisclosureAllowed
{
#if ENABLE(APPLE_PAY)
    return protectedPreferences(self)->applePayCapabilityDisclosureAllowed();
#else
    return NO;
#endif
}

- (void)_setApplePayCapabilityDisclosureAllowed:(BOOL)applePayCapabilityDisclosureAllowed
{
#if ENABLE(APPLE_PAY)
    protectedPreferences(self)->setApplePayCapabilityDisclosureAllowed(applePayCapabilityDisclosureAllowed);
#endif
}

- (BOOL)_shouldSuppressKeyboardInputDuringProvisionalNavigation
{
    return protectedPreferences(self)->shouldSuppressTextInputFromEditingDuringProvisionalNavigation();
}

- (void)_setShouldSuppressKeyboardInputDuringProvisionalNavigation:(BOOL)shouldSuppress
{
    protectedPreferences(self)->setShouldSuppressTextInputFromEditingDuringProvisionalNavigation(shouldSuppress);
}

- (BOOL)_loadsImagesAutomatically
{
    return protectedPreferences(self)->loadsImagesAutomatically();
}

- (void)_setLoadsImagesAutomatically:(BOOL)loadsImagesAutomatically
{
    protectedPreferences(self)->setLoadsImagesAutomatically(loadsImagesAutomatically);
}

- (BOOL)_peerConnectionEnabled
{
    return protectedPreferences(self)->peerConnectionEnabled();
}

- (void)_setPeerConnectionEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setPeerConnectionEnabled(enabled);
}

- (BOOL)_mediaDevicesEnabled
{
    return protectedPreferences(self)->mediaDevicesEnabled();
}

- (void)_setMediaDevicesEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setMediaDevicesEnabled(enabled);
}

- (BOOL)_getUserMediaRequiresFocus
{
    return protectedPreferences(self)->getUserMediaRequiresFocus();
}

- (void)_setGetUserMediaRequiresFocus:(BOOL)enabled
{
    protectedPreferences(self)->setGetUserMediaRequiresFocus(enabled);
}

- (BOOL)_screenCaptureEnabled
{
    return protectedPreferences(self)->screenCaptureEnabled();
}

- (void)_setScreenCaptureEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setScreenCaptureEnabled(enabled);
}

- (BOOL)_mockCaptureDevicesEnabled
{
    return protectedPreferences(self)->mockCaptureDevicesEnabled();
}

- (void)_setMockCaptureDevicesEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setMockCaptureDevicesEnabled(enabled);
}

- (BOOL)_mockCaptureDevicesPromptEnabled
{
    return protectedPreferences(self)->mockCaptureDevicesPromptEnabled();
}

- (void)_setMockCaptureDevicesPromptEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setMockCaptureDevicesPromptEnabled(enabled);
}

- (BOOL)_mediaCaptureRequiresSecureConnection
{
    return protectedPreferences(self)->mediaCaptureRequiresSecureConnection();
}

- (void)_setMediaCaptureRequiresSecureConnection:(BOOL)requiresSecureConnection
{
    protectedPreferences(self)->setMediaCaptureRequiresSecureConnection(requiresSecureConnection);
}

- (double)_inactiveMediaCaptureStreamRepromptIntervalInMinutes
{
    return protectedPreferences(self)->inactiveMediaCaptureStreamRepromptIntervalInMinutes();
}

- (void)_setInactiveMediaCaptureStreamRepromptIntervalInMinutes:(double)interval
{
    protectedPreferences(self)->setInactiveMediaCaptureStreamRepromptIntervalInMinutes(interval);
}

- (double)_inactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes
{
    return protectedPreferences(self)->inactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes();
}

- (void)_setInactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes:(double)interval
{
    protectedPreferences(self)->setInactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes(interval);
}

- (BOOL)_interruptAudioOnPageVisibilityChangeEnabled
{
    return protectedPreferences(self)->interruptAudioOnPageVisibilityChangeEnabled();
}

- (void)_setInterruptAudioOnPageVisibilityChangeEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setInterruptAudioOnPageVisibilityChangeEnabled(enabled);
}

- (BOOL)_enumeratingAllNetworkInterfacesEnabled
{
    return protectedPreferences(self)->enumeratingAllNetworkInterfacesEnabled();
}

- (void)_setEnumeratingAllNetworkInterfacesEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setEnumeratingAllNetworkInterfacesEnabled(enabled);
}

- (BOOL)_iceCandidateFilteringEnabled
{
    return protectedPreferences(self)->iceCandidateFilteringEnabled();
}

- (void)_setICECandidateFilteringEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setICECandidateFilteringEnabled(enabled);
}

- (void)_setJavaScriptCanAccessClipboard:(BOOL)javaScriptCanAccessClipboard
{
    protectedPreferences(self)->setJavaScriptCanAccessClipboard(javaScriptCanAccessClipboard);
}

- (BOOL)_shouldAllowUserInstalledFonts
{
    return protectedPreferences(self)->shouldAllowUserInstalledFonts();
}

- (void)_setShouldAllowUserInstalledFonts:(BOOL)_shouldAllowUserInstalledFonts
{
    protectedPreferences(self)->setShouldAllowUserInstalledFonts(_shouldAllowUserInstalledFonts);
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
    return toAPI(static_cast<WebCore::EditableLinkBehavior>(protectedPreferences(self)->editableLinkBehavior()));
}

- (void)_setEditableLinkBehavior:(_WKEditableLinkBehavior)editableLinkBehavior
{
    protectedPreferences(self)->setEditableLinkBehavior(static_cast<uint32_t>(toEditableLinkBehavior(editableLinkBehavior)));
}

- (void)_setAVFoundationEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setAVFoundationEnabled(enabled);
}

- (BOOL)_avFoundationEnabled
{
    return protectedPreferences(self)->isAVFoundationEnabled();
}

- (void)_setTextExtractionEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setTextExtractionEnabled(enabled);
}

- (BOOL)_textExtractionEnabled
{
    return protectedPreferences(self)->textExtractionEnabled();
}

- (void)_setColorFilterEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setColorFilterEnabled(enabled);
}

- (BOOL)_colorFilterEnabled
{
    return protectedPreferences(self)->colorFilterEnabled();
}

- (void)_setPunchOutWhiteBackgroundsInDarkMode:(BOOL)punches
{
    protectedPreferences(self)->setPunchOutWhiteBackgroundsInDarkMode(punches);
}

- (BOOL)_punchOutWhiteBackgroundsInDarkMode
{
    return protectedPreferences(self)->punchOutWhiteBackgroundsInDarkMode();
}

- (void)_setLowPowerVideoAudioBufferSizeEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setLowPowerVideoAudioBufferSizeEnabled(enabled);
}

- (BOOL)_lowPowerVideoAudioBufferSizeEnabled
{
    return protectedPreferences(self)->lowPowerVideoAudioBufferSizeEnabled();
}

- (void)_setShouldIgnoreMetaViewport:(BOOL)ignoreMetaViewport
{
    return protectedPreferences(self)->setShouldIgnoreMetaViewport(ignoreMetaViewport);
}

- (BOOL)_shouldIgnoreMetaViewport
{
    return protectedPreferences(self)->shouldIgnoreMetaViewport();
}

- (void)_setNeedsSiteSpecificQuirks:(BOOL)enabled
{
    protectedPreferences(self)->setNeedsSiteSpecificQuirks(enabled);
}

- (BOOL)_needsSiteSpecificQuirks
{
    return protectedPreferences(self)->needsSiteSpecificQuirks();
}

- (void)_setItpDebugModeEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setItpDebugModeEnabled(enabled);
}

- (BOOL)_itpDebugModeEnabled
{
    return protectedPreferences(self)->itpDebugModeEnabled();
}

- (void)_setMediaSourceEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setMediaSourceEnabled(enabled);
}

- (BOOL)_mediaSourceEnabled
{
    return protectedPreferences(self)->mediaSourceEnabled();
}

- (void)_setManagedMediaSourceEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setManagedMediaSourceEnabled(enabled);
}

- (BOOL)_managedMediaSourceEnabled
{
    return protectedPreferences(self)->managedMediaSourceEnabled();
}

- (void)_setManagedMediaSourceLowThreshold:(double)threshold
{
    protectedPreferences(self)->setManagedMediaSourceLowThreshold(threshold);
}

- (double)_managedMediaSourceLowThreshold
{
    return protectedPreferences(self)->managedMediaSourceLowThreshold();
}

- (void)_setManagedMediaSourceHighThreshold:(double)threshold
{
    protectedPreferences(self)->setManagedMediaSourceHighThreshold(threshold);
}

- (double)_managedMediaSourceHighThreshold
{
    return protectedPreferences(self)->managedMediaSourceHighThreshold();
}

- (BOOL)_secureContextChecksEnabled
{
    return protectedPreferences(self)->secureContextChecksEnabled();
}

- (void)_setSecureContextChecksEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setSecureContextChecksEnabled(enabled);
}

- (void)_setWebAudioEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setWebAudioEnabled(enabled);
}

- (BOOL)_webAudioEnabled
{
    return protectedPreferences(self)->webAudioEnabled();
}

- (void)_setAcceleratedCompositingEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setAcceleratedCompositingEnabled(enabled);
}

- (BOOL)_acceleratedCompositingEnabled
{
    return protectedPreferences(self)->acceleratedCompositingEnabled();
}

- (BOOL)_remotePlaybackEnabled
{
    return protectedPreferences(self)->remotePlaybackEnabled();
}

- (void)_setRemotePlaybackEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setRemotePlaybackEnabled(enabled);
}

- (BOOL)_serviceWorkerEntitlementDisabledForTesting
{
    return protectedPreferences(self)->serviceWorkerEntitlementDisabledForTesting();
}

- (void)_setServiceWorkerEntitlementDisabledForTesting:(BOOL)disable
{
    protectedPreferences(self)->setServiceWorkerEntitlementDisabledForTesting(disable);
}

#if PLATFORM(MAC)
- (void)_setCanvasUsesAcceleratedDrawing:(BOOL)enabled
{
    protectedPreferences(self)->setCanvasUsesAcceleratedDrawing(enabled);
}

- (BOOL)_canvasUsesAcceleratedDrawing
{
    return protectedPreferences(self)->canvasUsesAcceleratedDrawing();
}

- (void)_setDefaultTextEncodingName:(NSString *)name
{
    protectedPreferences(self)->setDefaultTextEncodingName(name);
}

- (NSString *)_defaultTextEncodingName
{
    return protectedPreferences(self)->defaultTextEncodingName().createNSString().autorelease();
}

- (void)_setAuthorAndUserStylesEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setAuthorAndUserStylesEnabled(enabled);
}

- (BOOL)_authorAndUserStylesEnabled
{
    return protectedPreferences(self)->authorAndUserStylesEnabled();
}

- (void)_setDOMTimersThrottlingEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setDOMTimersThrottlingEnabled(enabled);
}

- (BOOL)_domTimersThrottlingEnabled
{
    return protectedPreferences(self)->domTimersThrottlingEnabled();
}

- (void)_setWebArchiveDebugModeEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setWebArchiveDebugModeEnabled(enabled);
}

- (BOOL)_webArchiveDebugModeEnabled
{
    return protectedPreferences(self)->webArchiveDebugModeEnabled();
}

- (void)_setLocalFileContentSniffingEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setLocalFileContentSniffingEnabled(enabled);
}

- (BOOL)_localFileContentSniffingEnabled
{
    return protectedPreferences(self)->localFileContentSniffingEnabled();
}

- (void)_setUsesPageCache:(BOOL)enabled
{
    protectedPreferences(self)->setUsesBackForwardCache(enabled);
}

- (BOOL)_usesPageCache
{
    return protectedPreferences(self)->usesBackForwardCache();
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
    protectedPreferences(self)->setWebSecurityEnabled(enabled);
}

- (BOOL)_webSecurityEnabled
{
    return protectedPreferences(self)->webSecurityEnabled();
}

- (void)_setUniversalAccessFromFileURLsAllowed:(BOOL)enabled
{
    protectedPreferences(self)->setAllowUniversalAccessFromFileURLs(enabled);
}

- (BOOL)_universalAccessFromFileURLsAllowed
{
    return protectedPreferences(self)->allowUniversalAccessFromFileURLs();
}

- (void)_setTopNavigationToDataURLsAllowed:(BOOL)enabled
{
    protectedPreferences(self)->setAllowTopNavigationToDataURLs(enabled);
}

- (BOOL)_topNavigationToDataURLsAllowed
{
    return protectedPreferences(self)->allowTopNavigationToDataURLs();
}

- (void)_setSuppressesIncrementalRendering:(BOOL)enabled
{
    protectedPreferences(self)->setSuppressesIncrementalRendering(enabled);
}

- (BOOL)_suppressesIncrementalRendering
{
    return protectedPreferences(self)->suppressesIncrementalRendering();
}

- (void)_setCookieEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setCookieEnabled(enabled);
}

- (BOOL)_cookieEnabled
{
    return protectedPreferences(self)->cookieEnabled();
}

- (void)_setViewGestureDebuggingEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setViewGestureDebuggingEnabled(enabled);
}

- (BOOL)_viewGestureDebuggingEnabled
{
    return protectedPreferences(self)->viewGestureDebuggingEnabled();
}

- (void)_setStandardFontFamily:(NSString *)family
{
    protectedPreferences(self)->setStandardFontFamily(family);
}

- (NSString *)_standardFontFamily
{
    return protectedPreferences(self)->standardFontFamily().createNSString().autorelease();
}

- (void)_setBackspaceKeyNavigationEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setBackspaceKeyNavigationEnabled(enabled);
}

- (BOOL)_backspaceKeyNavigationEnabled
{
    return protectedPreferences(self)->backspaceKeyNavigationEnabled();
}

- (void)_setWebGLEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setWebGLEnabled(enabled);
}

- (BOOL)_webGLEnabled
{
    return protectedPreferences(self)->webGLEnabled();
}

- (void)_setAllowsInlineMediaPlayback:(BOOL)enabled
{
    protectedPreferences(self)->setAllowsInlineMediaPlayback(enabled);
}

- (BOOL)_allowsInlineMediaPlayback
{
    return protectedPreferences(self)->allowsInlineMediaPlayback();
}

- (void)_setApplePayEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setApplePayEnabled(enabled);
}

- (BOOL)_applePayEnabled
{
    return protectedPreferences(self)->applePayEnabled();
}

- (void)_setInlineMediaPlaybackRequiresPlaysInlineAttribute:(BOOL)enabled
{
    protectedPreferences(self)->setInlineMediaPlaybackRequiresPlaysInlineAttribute(enabled);
}

- (BOOL)_inlineMediaPlaybackRequiresPlaysInlineAttribute
{
    return protectedPreferences(self)->inlineMediaPlaybackRequiresPlaysInlineAttribute();
}

- (void)_setInvisibleMediaAutoplayNotPermitted:(BOOL)enabled
{
    protectedPreferences(self)->setInvisibleAutoplayNotPermitted(enabled);
}

- (BOOL)_invisibleMediaAutoplayNotPermitted
{
    return protectedPreferences(self)->invisibleAutoplayNotPermitted();
}

- (void)_setLegacyEncryptedMediaAPIEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setLegacyEncryptedMediaAPIEnabled(enabled);
}

- (BOOL)_legacyEncryptedMediaAPIEnabled
{
    return protectedPreferences(self)->legacyEncryptedMediaAPIEnabled();
}

- (void)_setMainContentUserGestureOverrideEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setMainContentUserGestureOverrideEnabled(enabled);
}

- (BOOL)_mainContentUserGestureOverrideEnabled
{
    return protectedPreferences(self)->mainContentUserGestureOverrideEnabled();
}

- (void)_setNeedsStorageAccessFromFileURLsQuirk:(BOOL)enabled
{
    protectedPreferences(self)->setNeedsStorageAccessFromFileURLsQuirk(enabled);
}

- (BOOL)_needsStorageAccessFromFileURLsQuirk
{
    return protectedPreferences(self)->needsStorageAccessFromFileURLsQuirk();
}

- (void)_setPDFPluginEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setPDFPluginEnabled(enabled);
}

- (BOOL)_pdfPluginEnabled
{
    return protectedPreferences(self)->pdfPluginEnabled();
}

- (void)_setRequiresUserGestureForAudioPlayback:(BOOL)enabled
{
    protectedPreferences(self)->setRequiresUserGestureForAudioPlayback(enabled);
}

- (BOOL)_requiresUserGestureForAudioPlayback
{
    return protectedPreferences(self)->requiresUserGestureForAudioPlayback();
}

- (void)_setRequiresUserGestureForVideoPlayback:(BOOL)enabled
{
    protectedPreferences(self)->setRequiresUserGestureForVideoPlayback(enabled);
}

- (BOOL)_requiresUserGestureForVideoPlayback
{
    return protectedPreferences(self)->requiresUserGestureForVideoPlayback();
}

- (void)_setServiceControlsEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setServiceControlsEnabled(enabled);
}

- (BOOL)_serviceControlsEnabled
{
    return protectedPreferences(self)->serviceControlsEnabled();
}

- (void)_setShowsToolTipOverTruncatedText:(BOOL)enabled
{
    protectedPreferences(self)->setShowsToolTipOverTruncatedText(enabled);
}

- (BOOL)_showsToolTipOverTruncatedText
{
    return protectedPreferences(self)->showsToolTipOverTruncatedText();
}

- (void)_setTextAreasAreResizable:(BOOL)enabled
{
    protectedPreferences(self)->setTextAreasAreResizable(enabled);
}

- (BOOL)_textAreasAreResizable
{
    return protectedPreferences(self)->textAreasAreResizable();
}

- (void)_setUseGiantTiles:(BOOL)enabled
{
    protectedPreferences(self)->setUseGiantTiles(enabled);
}

- (BOOL)_useGiantTiles
{
    return protectedPreferences(self)->useGiantTiles();
}

- (void)_setWantsBalancedSetDefersLoadingBehavior:(BOOL)enabled
{
    protectedPreferences(self)->setWantsBalancedSetDefersLoadingBehavior(enabled);
}

- (BOOL)_wantsBalancedSetDefersLoadingBehavior
{
    return protectedPreferences(self)->wantsBalancedSetDefersLoadingBehavior();
}

- (void)_setAggressiveTileRetentionEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setAggressiveTileRetentionEnabled(enabled);
}

- (BOOL)_aggressiveTileRetentionEnabled
{
    return protectedPreferences(self)->aggressiveTileRetentionEnabled();
}

- (void)_setAppNapEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setPageVisibilityBasedProcessSuppressionEnabled(enabled);
}

- (BOOL)_appNapEnabled
{
    return protectedPreferences(self)->pageVisibilityBasedProcessSuppressionEnabled();
}

#endif // PLATFORM(MAC)

- (BOOL)_javaScriptCanAccessClipboard
{
    return protectedPreferences(self)->javaScriptCanAccessClipboard();
}

- (void)_setDOMPasteAllowed:(BOOL)domPasteAllowed
{
    protectedPreferences(self)->setDOMPasteAllowed(domPasteAllowed);
}

- (BOOL)_domPasteAllowed
{
    return protectedPreferences(self)->domPasteAllowed();
}

- (void)_setShouldEnableTextAutosizingBoost:(BOOL)shouldEnableTextAutosizingBoost
{
#if ENABLE(TEXT_AUTOSIZING)
    protectedPreferences(self)->setShouldEnableTextAutosizingBoost(shouldEnableTextAutosizingBoost);
#endif
}

- (BOOL)_shouldEnableTextAutosizingBoost
{
#if ENABLE(TEXT_AUTOSIZING)
    return protectedPreferences(self)->shouldEnableTextAutosizingBoost();
#else
    return NO;
#endif
}

- (BOOL)_isSafeBrowsingEnabled
{
    return protectedPreferences(self)->safeBrowsingEnabled();
}

- (void)_setSafeBrowsingEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setSafeBrowsingEnabled(enabled);
}

- (void)_setVideoQualityIncludesDisplayCompositingEnabled:(BOOL)videoQualityIncludesDisplayCompositingEnabled
{
    protectedPreferences(self)->setVideoQualityIncludesDisplayCompositingEnabled(videoQualityIncludesDisplayCompositingEnabled);
}

- (BOOL)_videoQualityIncludesDisplayCompositingEnabled
{
    return protectedPreferences(self)->videoQualityIncludesDisplayCompositingEnabled();
}

- (void)_setDeviceOrientationEventEnabled:(BOOL)enabled
{
#if ENABLE(DEVICE_ORIENTATION)
    protectedPreferences(self)->setDeviceOrientationEventEnabled(enabled);
#endif
}

- (BOOL)_deviceOrientationEventEnabled
{
#if ENABLE(DEVICE_ORIENTATION)
    return protectedPreferences(self)->deviceOrientationEventEnabled();
#else
    return false;
#endif
}

- (void)_setAccessibilityIsolatedTreeEnabled:(BOOL)accessibilityIsolatedTreeEnabled
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    protectedPreferences(self)->setIsAccessibilityIsolatedTreeEnabled(accessibilityIsolatedTreeEnabled);
#endif
}

- (BOOL)_accessibilityIsolatedTreeEnabled
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    return protectedPreferences(self)->isAccessibilityIsolatedTreeEnabled();
#else
    return false;
#endif
}

- (BOOL)_speechRecognitionEnabled
{
    return protectedPreferences(self)->speechRecognitionEnabled();
}

- (void)_setSpeechRecognitionEnabled:(BOOL)speechRecognitionEnabled
{
    protectedPreferences(self)->setSpeechRecognitionEnabled(speechRecognitionEnabled);
}

- (BOOL)_privateClickMeasurementEnabled
{
    return protectedPreferences(self)->privateClickMeasurementEnabled();
}

- (void)_setPrivateClickMeasurementEnabled:(BOOL)privateClickMeasurementEnabled
{
    protectedPreferences(self)->setPrivateClickMeasurementEnabled(privateClickMeasurementEnabled);
}

- (BOOL)_privateClickMeasurementDebugModeEnabled
{
    return protectedPreferences(self)->privateClickMeasurementDebugModeEnabled();
}

- (void)_setPrivateClickMeasurementDebugModeEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setPrivateClickMeasurementDebugModeEnabled(enabled);
}

- (_WKPitchCorrectionAlgorithm)_pitchCorrectionAlgorithm
{
    return static_cast<_WKPitchCorrectionAlgorithm>(protectedPreferences(self)->pitchCorrectionAlgorithm());
}

- (void)_setPitchCorrectionAlgorithm:(_WKPitchCorrectionAlgorithm)pitchCorrectionAlgorithm
{
    protectedPreferences(self)->setPitchCorrectionAlgorithm(pitchCorrectionAlgorithm);
}

- (BOOL)_mediaSessionEnabled
{
    return protectedPreferences(self)->mediaSessionEnabled();
}

- (void)_setMediaSessionEnabled:(BOOL)mediaSessionEnabled
{
    protectedPreferences(self)->setMediaSessionEnabled(mediaSessionEnabled);
}

- (BOOL)_isExtensibleSSOEnabled
{
#if HAVE(APP_SSO)
    return protectedPreferences(self)->isExtensibleSSOEnabled();
#else
    return false;
#endif
}

- (void)_setExtensibleSSOEnabled:(BOOL)extensibleSSOEnabled
{
#if HAVE(APP_SSO)
    protectedPreferences(self)->setExtensibleSSOEnabled(extensibleSSOEnabled);
#endif
}

- (BOOL)_requiresPageVisibilityToPlayAudio
{
    return protectedPreferences(self)->requiresPageVisibilityToPlayAudio();
}

- (void)_setRequiresPageVisibilityToPlayAudio:(BOOL)requiresVisibility
{
    protectedPreferences(self)->setRequiresPageVisibilityToPlayAudio(requiresVisibility);
}

- (BOOL)_fileSystemAccessEnabled
{
    return protectedPreferences(self)->fileSystemEnabled();
}

- (void)_setFileSystemAccessEnabled:(BOOL)fileSystemAccessEnabled
{
    protectedPreferences(self)->setFileSystemEnabled(fileSystemAccessEnabled);
}

- (BOOL)_storageAPIEnabled
{
    return protectedPreferences(self)->storageAPIEnabled();
}

- (void)_setStorageAPIEnabled:(BOOL)storageAPIEnabled
{
    protectedPreferences(self)->setStorageAPIEnabled(storageAPIEnabled);
}

- (BOOL)_accessHandleEnabled
{
    return protectedPreferences(self)->accessHandleEnabled();
}

- (void)_setAccessHandleEnabled:(BOOL)accessHandleEnabled
{
    protectedPreferences(self)->setAccessHandleEnabled(accessHandleEnabled);
}

- (void)_setNotificationsEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setNotificationsEnabled(enabled);
}

- (BOOL)_notificationsEnabled
{
    return protectedPreferences(self)->notificationsEnabled();
}

- (void)_setNotificationEventEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setNotificationEventEnabled(enabled);
}

- (BOOL)_notificationEventEnabled
{
    return protectedPreferences(self)->notificationEventEnabled();
}

- (BOOL)_pushAPIEnabled
{
    return protectedPreferences(self)->pushAPIEnabled();
}

- (void)_setPushAPIEnabled:(BOOL)pushAPIEnabled
{
    protectedPreferences(self)->setPushAPIEnabled(pushAPIEnabled);
}

- (void)_setModelDocumentEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setModelDocumentEnabled(enabled);
}

- (BOOL)_modelDocumentEnabled
{
    return protectedPreferences(self)->modelDocumentEnabled();
}

- (void)_setRequiresFullscreenToLockScreenOrientation:(BOOL)enabled
{
    protectedPreferences(self)->setFullscreenRequirementForScreenOrientationLockingEnabled(enabled);
}

- (BOOL)_requiresFullscreenToLockScreenOrientation
{
    return protectedPreferences(self)->fullscreenRequirementForScreenOrientationLockingEnabled();
}

- (void)_setInteractionRegionMinimumCornerRadius:(double)radius
{
    protectedPreferences(self)->setInteractionRegionMinimumCornerRadius(radius);
}

- (double)_interactionRegionMinimumCornerRadius
{
    return protectedPreferences(self)->interactionRegionMinimumCornerRadius();
}

- (void)_setInteractionRegionInlinePadding:(double)padding
{
    protectedPreferences(self)->setInteractionRegionInlinePadding(padding);
}

- (double)_interactionRegionInlinePadding
{
    return protectedPreferences(self)->interactionRegionInlinePadding();
}

- (void)_setMediaPreferredFullscreenWidth:(double)width
{
    protectedPreferences(self)->setMediaPreferredFullscreenWidth(width);
}

- (double)_mediaPreferredFullscreenWidth
{
    return protectedPreferences(self)->mediaPreferredFullscreenWidth();
}

- (void)_setAppBadgeEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setAppBadgeEnabled(enabled);
}

- (BOOL)_appBadgeEnabled
{
    return protectedPreferences(self)->appBadgeEnabled();
}

- (void)_setVerifyWindowOpenUserGestureFromUIProcess:(BOOL)enabled
{
    protectedPreferences(self)->setVerifyWindowOpenUserGestureFromUIProcess(enabled);
}

- (BOOL)_verifyWindowOpenUserGestureFromUIProcess
{
    return protectedPreferences(self)->verifyWindowOpenUserGestureFromUIProcess();
}

- (BOOL)_mediaCapabilityGrantsEnabled
{
    return protectedPreferences(self)->mediaCapabilityGrantsEnabled();
}

- (void)_setMediaCapabilityGrantsEnabled:(BOOL)mediaCapabilityGrantsEnabled
{
    protectedPreferences(self)->setMediaCapabilityGrantsEnabled(mediaCapabilityGrantsEnabled);
}

- (void)_setAllowPrivacySensitiveOperationsInNonPersistentDataStores:(BOOL)allowPrivacySensitiveOperationsInNonPersistentDataStores
{
    protectedPreferences(self)->setAllowPrivacySensitiveOperationsInNonPersistentDataStores(allowPrivacySensitiveOperationsInNonPersistentDataStores);
}

- (BOOL)_allowPrivacySensitiveOperationsInNonPersistentDataStores
{
    return protectedPreferences(self)->allowPrivacySensitiveOperationsInNonPersistentDataStores();
}

- (void)_setVideoFullscreenRequiresElementFullscreen:(BOOL)videoFullscreenRequiresElementFullscreen
{
    protectedPreferences(self)->setVideoFullscreenRequiresElementFullscreen(videoFullscreenRequiresElementFullscreen);
}

- (BOOL)_videoFullscreenRequiresElementFullscreen
{
    return protectedPreferences(self)->videoFullscreenRequiresElementFullscreen();
}

- (void)_setCSSTransformStyleSeparatedEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setCSSTransformStyleSeparatedEnabled(enabled);
}

- (BOOL)_cssTransformStyleSeparatedEnabled
{
    return protectedPreferences(self)->cssTransformStyleSeparatedEnabled();
}

- (void)_setOverlayRegionsEnabled:(BOOL)enabled
{
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    protectedPreferences(self)->setOverlayRegionsEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

- (BOOL)_overlayRegionsEnabled
{
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    return protectedPreferences(self)->overlayRegionsEnabled();
#else
    return NO;
#endif
}

- (void)_setModelElementEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setModelElementEnabled(enabled);
}

- (BOOL)_modelProcessEnabled
{
    return protectedPreferences(self)->modelProcessEnabled();
}

- (void)_setModelProcessEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setModelProcessEnabled(enabled);
}

- (BOOL)_modelElementEnabled
{
    return protectedPreferences(self)->modelElementEnabled();
}

- (void)_setModelNoPortalAttributeEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setModelNoPortalAttributeEnabled(enabled);
}

- (BOOL)_modelNoPortalAttributeEnabled
{
    return protectedPreferences(self)->modelNoPortalAttributeEnabled();
}

- (void)_setUpdateSceneGeometryEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setUpdateSceneGeometryEnabled(enabled);
}

- (BOOL)_updateSceneGeometryEnabled
{
    return protectedPreferences(self)->updateSceneGeometryEnabled();
}

- (void)_setRequiresPageVisibilityForVideoToBeNowPlayingForTesting:(BOOL)enabled
{
#if ENABLE(REQUIRES_PAGE_VISIBILITY_FOR_NOW_PLAYING)
    protectedPreferences(self)->setRequiresPageVisibilityForVideoToBeNowPlaying(enabled);
#endif
}

- (BOOL)_requiresPageVisibilityForVideoToBeNowPlayingForTesting
{
#if ENABLE(REQUIRES_PAGE_VISIBILITY_FOR_NOW_PLAYING)
    return protectedPreferences(self)->requiresPageVisibilityForVideoToBeNowPlaying();
#else
    return NO;
#endif
}

- (BOOL)_siteIsolationEnabled
{
    return protectedPreferences(self)->siteIsolationEnabled();
}

- (void)_setSiteIsolationEnabled:(BOOL)enabled
{
    protectedPreferences(self)->setSiteIsolationEnabled(enabled);
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
        RELEASE_LOG_FAULT(Plugins, "Application attempted to enable NPAPI plugins, which are no longer supported");
}

#endif

- (BOOL)javaScriptEnabled
{
    return protectedPreferences(self)->javaScriptEnabled();
}

- (void)setJavaScriptEnabled:(BOOL)javaScriptEnabled
{
    protectedPreferences(self)->setJavaScriptEnabled(javaScriptEnabled);
}

@end

@implementation WKPreferences (WKPrivateDeprecated)

- (void)_setDNSPrefetchingEnabled:(BOOL)enabled
{
}

- (BOOL)_dnsPrefetchingEnabled
{
    return NO;
}

- (BOOL)_shouldAllowDesignSystemUIFonts
{
    return YES;
}

- (void)_setShouldAllowDesignSystemUIFonts:(BOOL)_shouldAllowDesignSystemUIFonts
{
}

- (void)_setRequestAnimationFrameEnabled:(BOOL)enabled
{
}

- (BOOL)_requestAnimationFrameEnabled
{
    return YES;
}

- (BOOL)_subpixelAntialiasedLayerTextEnabled
{
    return NO;
}

- (void)_setSubpixelAntialiasedLayerTextEnabled:(BOOL)enabled
{
}

#if !TARGET_OS_IPHONE

- (void)_setPageCacheSupportsPlugins:(BOOL)enabled
{
}

- (BOOL)_pageCacheSupportsPlugins
{
    return NO;
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

- (void)_setPlugInSnapshottingEnabled:(BOOL)enabled
{
}

- (BOOL)_plugInSnapshottingEnabled
{
    return NO;
}

- (void)_setSubpixelCSSOMElementMetricsEnabled:(BOOL)enabled
{
}

- (BOOL)_subpixelCSSOMElementMetricsEnabled
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

- (BOOL)_displayListDrawingEnabled
{
    return NO;
}

- (void)_setDisplayListDrawingEnabled:(BOOL)displayListDrawingEnabled
{
}

- (BOOL)_offlineApplicationCacheIsEnabled
{
    return NO;
}

- (void)_setOfflineApplicationCacheIsEnabled:(BOOL)offlineApplicationCacheIsEnabled
{
}

- (void)_setMediaStreamEnabled:(BOOL)enabled
{
}

- (BOOL)_mediaStreamEnabled
{
    return YES;
}

- (void)_setClientBadgeEnabled:(BOOL)enabled
{
}

- (BOOL)_clientBadgeEnabled
{
    return NO;
}

+ (void)_forceSiteIsolationAlwaysOnForTesting
{
    WebKit::WebPreferences::forceSiteIsolationAlwaysOnForTesting();
}

@end
