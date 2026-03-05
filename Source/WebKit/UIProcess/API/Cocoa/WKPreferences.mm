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
    return [wrapper(protect(*_preferences)->copy()) retain];
}

- (CGFloat)minimumFontSize
{
    return protect(*_preferences)->minimumFontSize();
}

- (void)setMinimumFontSize:(CGFloat)minimumFontSize
{
    protect(*_preferences)->setMinimumFontSize(minimumFontSize);
}

- (void)setFraudulentWebsiteWarningEnabled:(BOOL)enabled
{
    protect(*_preferences)->setSafeBrowsingEnabled(enabled);
}

- (BOOL)isFraudulentWebsiteWarningEnabled
{
    return protect(*_preferences)->safeBrowsingEnabled();
}

- (BOOL)javaScriptCanOpenWindowsAutomatically
{
    return protect(*_preferences)->javaScriptCanOpenWindowsAutomatically();
}

- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)javaScriptCanOpenWindowsAutomatically
{
    protect(*_preferences)->setJavaScriptCanOpenWindowsAutomatically(javaScriptCanOpenWindowsAutomatically);
}

- (void)setShouldPrintBackgrounds:(BOOL)enabled
{
    protect(*_preferences)->setShouldPrintBackgrounds(enabled);
}

- (BOOL)shouldPrintBackgrounds
{
    return protect(*_preferences)->shouldPrintBackgrounds();
}

- (BOOL)isTextInteractionEnabled
{
    return protect(*_preferences)->textInteractionEnabled();
}

- (void)setTextInteractionEnabled:(BOOL)textInteractionEnabled
{
    protect(*_preferences)->setTextInteractionEnabled(textInteractionEnabled);
}

- (BOOL)isSiteSpecificQuirksModeEnabled
{
    return protect(*_preferences)->needsSiteSpecificQuirks();
}

- (void)setSiteSpecificQuirksModeEnabled:(BOOL)enabled
{
    protect(*_preferences)->setNeedsSiteSpecificQuirks(enabled);
}

- (BOOL)isElementFullscreenEnabled
{
    return protect(*_preferences)->fullScreenEnabled();
}

- (void)setElementFullscreenEnabled:(BOOL)elementFullscreenEnabled
{
    protect(*_preferences)->setFullScreenEnabled(elementFullscreenEnabled);
}

- (void)setInactiveSchedulingPolicy:(WKInactiveSchedulingPolicy)policy
{
    switch (policy) {
    case WKInactiveSchedulingPolicySuspend:
        protect(*_preferences)->setShouldTakeNearSuspendedAssertions(false);
        protect(*_preferences)->setBackgroundWebContentRunningBoardThrottlingEnabled(true);
        protect(*_preferences)->setShouldDropNearSuspendedAssertionAfterDelay(WebKit::defaultShouldDropNearSuspendedAssertionAfterDelay());
        break;
    case WKInactiveSchedulingPolicyThrottle:
        protect(*_preferences)->setShouldTakeNearSuspendedAssertions(true);
        protect(*_preferences)->setBackgroundWebContentRunningBoardThrottlingEnabled(true);
        protect(*_preferences)->setShouldDropNearSuspendedAssertionAfterDelay(false);
        break;
    case WKInactiveSchedulingPolicyNone:
        protect(*_preferences)->setShouldTakeNearSuspendedAssertions(true);
        protect(*_preferences)->setBackgroundWebContentRunningBoardThrottlingEnabled(false);
        protect(*_preferences)->setShouldDropNearSuspendedAssertionAfterDelay(false);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

- (WKInactiveSchedulingPolicy)inactiveSchedulingPolicy
{
    return protect(*_preferences)->backgroundWebContentRunningBoardThrottlingEnabled() ? (protect(*_preferences)->shouldTakeNearSuspendedAssertions() ? WKInactiveSchedulingPolicyThrottle : WKInactiveSchedulingPolicySuspend) : WKInactiveSchedulingPolicyNone;
}

- (BOOL)tabFocusesLinks
{
    return protect(*_preferences)->tabsToLinks();
}

- (void)setTabFocusesLinks:(BOOL)tabFocusesLinks
{
    protect(*_preferences)->setTabsToLinks(tabFocusesLinks);
}

- (BOOL)_useSystemAppearance
{
    return protect(*_preferences)->useSystemAppearance();
}

- (void)_setUseSystemAppearance:(BOOL)useSystemAppearance
{
    protect(*_preferences)->setUseSystemAppearance(useSystemAppearance);
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
    protect(*_preferences)->setOverlayRegionsEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

- (BOOL)isLookToScrollEnabled
{
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    return protect(*_preferences)->overlayRegionsEnabled();
#else
    return NO;
#endif
}
#endif

@end

@implementation WKPreferences (WKPrivate)

- (BOOL)_telephoneNumberDetectionIsEnabled
{
    return protect(*_preferences)->telephoneNumberParsingEnabled();
}

- (void)_setTelephoneNumberDetectionIsEnabled:(BOOL)telephoneNumberDetectionIsEnabled
{
    protect(*_preferences)->setTelephoneNumberParsingEnabled(telephoneNumberDetectionIsEnabled);
}

static WebCore::StorageBlockingPolicy NODELETE toStorageBlockingPolicy(_WKStorageBlockingPolicy policy)
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

static _WKStorageBlockingPolicy NODELETE toAPI(WebCore::StorageBlockingPolicy policy)
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
    return toAPI(static_cast<WebCore::StorageBlockingPolicy>(protect(*_preferences)->storageBlockingPolicy()));
}

- (void)_setStorageBlockingPolicy:(_WKStorageBlockingPolicy)policy
{
    protect(*_preferences)->setStorageBlockingPolicy(static_cast<uint32_t>(toStorageBlockingPolicy(policy)));
}

- (BOOL)_fullScreenEnabled
{
    return protect(*_preferences)->fullScreenEnabled();
}

- (void)_setFullScreenEnabled:(BOOL)fullScreenEnabled
{
    protect(*_preferences)->setFullScreenEnabled(fullScreenEnabled);
}

- (BOOL)_allowsPictureInPictureMediaPlayback
{
    return protect(*_preferences)->allowsPictureInPictureMediaPlayback();
}

- (void)_setAllowsPictureInPictureMediaPlayback:(BOOL)allowed
{
    protect(*_preferences)->setAllowsPictureInPictureMediaPlayback(allowed);
}

- (BOOL)_compositingBordersVisible
{
    return protect(*_preferences)->compositingBordersVisible();
}

- (void)_setCompositingBordersVisible:(BOOL)compositingBordersVisible
{
    protect(*_preferences)->setCompositingBordersVisible(compositingBordersVisible);
}

- (BOOL)_compositingRepaintCountersVisible
{
    return protect(*_preferences)->compositingRepaintCountersVisible();
}

- (void)_setCompositingRepaintCountersVisible:(BOOL)repaintCountersVisible
{
    protect(*_preferences)->setCompositingRepaintCountersVisible(repaintCountersVisible);
}

- (BOOL)_tiledScrollingIndicatorVisible
{
    return protect(*_preferences)->tiledScrollingIndicatorVisible();
}

- (void)_setTiledScrollingIndicatorVisible:(BOOL)tiledScrollingIndicatorVisible
{
    protect(*_preferences)->setTiledScrollingIndicatorVisible(tiledScrollingIndicatorVisible);
}

- (BOOL)_resourceUsageOverlayVisible
{
    return protect(*_preferences)->resourceUsageOverlayVisible();
}

- (void)_setResourceUsageOverlayVisible:(BOOL)resourceUsageOverlayVisible
{
    protect(*_preferences)->setResourceUsageOverlayVisible(resourceUsageOverlayVisible);
}

- (_WKDebugOverlayRegions)_visibleDebugOverlayRegions
{
    return protect(*_preferences)->visibleDebugOverlayRegions();
}

- (void)_setVisibleDebugOverlayRegions:(_WKDebugOverlayRegions)regionFlags
{
    protect(*_preferences)->setVisibleDebugOverlayRegions(regionFlags);
}

- (BOOL)_legacyLineLayoutVisualCoverageEnabled
{
    return protect(*_preferences)->legacyLineLayoutVisualCoverageEnabled();
}

- (void)_setLegacyLineLayoutVisualCoverageEnabled:(BOOL)legacyLineLayoutVisualCoverageEnabled
{
    protect(*_preferences)->setLegacyLineLayoutVisualCoverageEnabled(legacyLineLayoutVisualCoverageEnabled);
}

- (BOOL)_contentChangeObserverEnabled
{
    return protect(*_preferences)->contentChangeObserverEnabled();
}

- (void)_setContentChangeObserverEnabled:(BOOL)contentChangeObserverEnabled
{
    protect(*_preferences)->setContentChangeObserverEnabled(contentChangeObserverEnabled);
}

- (BOOL)_acceleratedDrawingEnabled
{
    return protect(*_preferences)->acceleratedDrawingEnabled();
}

- (void)_setAcceleratedDrawingEnabled:(BOOL)acceleratedDrawingEnabled
{
    protect(*_preferences)->setAcceleratedDrawingEnabled(acceleratedDrawingEnabled);
}

- (BOOL)_largeImageAsyncDecodingEnabled
{
    return protect(*_preferences)->largeImageAsyncDecodingEnabled();
}

- (void)_setLargeImageAsyncDecodingEnabled:(BOOL)_largeImageAsyncDecodingEnabled
{
    protect(*_preferences)->setLargeImageAsyncDecodingEnabled(_largeImageAsyncDecodingEnabled);
}

- (BOOL)_needsInAppBrowserPrivacyQuirks
{
    return protect(*_preferences)->needsInAppBrowserPrivacyQuirks();
}

- (void)_setNeedsInAppBrowserPrivacyQuirks:(BOOL)enabled
{
    protect(*_preferences)->setNeedsInAppBrowserPrivacyQuirks(enabled);
}

- (BOOL)_animatedImageAsyncDecodingEnabled
{
    return protect(*_preferences)->animatedImageAsyncDecodingEnabled();
}

- (void)_setAnimatedImageAsyncDecodingEnabled:(BOOL)_animatedImageAsyncDecodingEnabled
{
    protect(*_preferences)->setAnimatedImageAsyncDecodingEnabled(_animatedImageAsyncDecodingEnabled);
}

- (BOOL)_textAutosizingEnabled
{
    return protect(*_preferences)->textAutosizingEnabled();
}

- (void)_setTextAutosizingEnabled:(BOOL)enabled
{
    protect(*_preferences)->setTextAutosizingEnabled(enabled);
}

- (BOOL)_useUIProcessForBackForwardItemLoading
{
    return protect(*_preferences)->useUIProcessForBackForwardItemLoading();
}

- (void)_setUseUIProcessForBackForwardItemLoading:(BOOL)flag
{
    protect(*_preferences)->setUseUIProcessForBackForwardItemLoading(flag);
}

- (BOOL)_developerExtrasEnabled
{
    return protect(*_preferences)->developerExtrasEnabled();
}

- (void)_setDeveloperExtrasEnabled:(BOOL)developerExtrasEnabled
{
    protect(*_preferences)->setDeveloperExtrasEnabled(developerExtrasEnabled);
}

- (BOOL)_logsPageMessagesToSystemConsoleEnabled
{
    return protect(*_preferences)->logsPageMessagesToSystemConsoleEnabled();
}

- (void)_setLogsPageMessagesToSystemConsoleEnabled:(BOOL)logsPageMessagesToSystemConsoleEnabled
{
    protect(*_preferences)->setLogsPageMessagesToSystemConsoleEnabled(logsPageMessagesToSystemConsoleEnabled);
}

- (BOOL)_hiddenPageDOMTimerThrottlingEnabled
{
    return protect(*_preferences)->hiddenPageDOMTimerThrottlingEnabled();
}

- (void)_setHiddenPageDOMTimerThrottlingEnabled:(BOOL)hiddenPageDOMTimerThrottlingEnabled
{
    protect(*_preferences)->setHiddenPageDOMTimerThrottlingEnabled(hiddenPageDOMTimerThrottlingEnabled);
}

- (BOOL)_hiddenPageDOMTimerThrottlingAutoIncreases
{
    return protect(*_preferences)->hiddenPageDOMTimerThrottlingAutoIncreases();
}

- (void)_setHiddenPageDOMTimerThrottlingAutoIncreases:(BOOL)hiddenPageDOMTimerThrottlingAutoIncreases
{
    protect(*_preferences)->setHiddenPageDOMTimerThrottlingAutoIncreases(hiddenPageDOMTimerThrottlingAutoIncreases);
}

- (BOOL)_pageVisibilityBasedProcessSuppressionEnabled
{
    return protect(*_preferences)->pageVisibilityBasedProcessSuppressionEnabled();
}

- (void)_setPageVisibilityBasedProcessSuppressionEnabled:(BOOL)pageVisibilityBasedProcessSuppressionEnabled
{
    protect(*_preferences)->setPageVisibilityBasedProcessSuppressionEnabled(pageVisibilityBasedProcessSuppressionEnabled);
}

- (BOOL)_allowFileAccessFromFileURLs
{
    return protect(*_preferences)->allowFileAccessFromFileURLs();
}

- (void)_setAllowFileAccessFromFileURLs:(BOOL)allowFileAccessFromFileURLs
{
    protect(*_preferences)->setAllowFileAccessFromFileURLs(allowFileAccessFromFileURLs);
}

- (_WKJavaScriptRuntimeFlags)_javaScriptRuntimeFlags
{
    return protect(*_preferences)->javaScriptRuntimeFlags();
}

- (void)_setJavaScriptRuntimeFlags:(_WKJavaScriptRuntimeFlags)javaScriptRuntimeFlags
{
    protect(*_preferences)->setJavaScriptRuntimeFlags(javaScriptRuntimeFlags);
}

- (BOOL)_isStandalone
{
    return protect(*_preferences)->standalone();
}

- (void)_setStandalone:(BOOL)standalone
{
    protect(*_preferences)->setStandalone(standalone);
}

- (BOOL)_diagnosticLoggingEnabled
{
    return protect(*_preferences)->diagnosticLoggingEnabled();
}

- (void)_setDiagnosticLoggingEnabled:(BOOL)diagnosticLoggingEnabled
{
    protect(*_preferences)->setDiagnosticLoggingEnabled(diagnosticLoggingEnabled);
}

- (NSUInteger)_defaultFontSize
{
    return protect(*_preferences)->defaultFontSize();
}

- (void)_setDefaultFontSize:(NSUInteger)defaultFontSize
{
    protect(*_preferences)->setDefaultFontSize(defaultFontSize);
}

- (NSUInteger)_defaultFixedPitchFontSize
{
    return protect(*_preferences)->defaultFixedFontSize();
}

- (void)_setDefaultFixedPitchFontSize:(NSUInteger)defaultFixedPitchFontSize
{
    protect(*_preferences)->setDefaultFixedFontSize(defaultFixedPitchFontSize);
}

- (NSString *)_fixedPitchFontFamily
{
    return protect(*_preferences)->fixedFontFamily().createNSString().autorelease();
}

- (void)_setFixedPitchFontFamily:(NSString *)fixedPitchFontFamily
{
    protect(*_preferences)->setFixedFontFamily(fixedPitchFontFamily);
}

+ (NSArray<_WKFeature *> *)_features
{
    auto features = WebKit::WebPreferences::features();
    return wrapper(API::Array::create(WTF::move(features))).autorelease();
}

+ (NSArray<_WKFeature *> *)_internalDebugFeatures
{
    auto features = WebKit::WebPreferences::internalDebugFeatures();
    return wrapper(API::Array::create(WTF::move(features))).autorelease();
}

- (BOOL)_isEnabledForInternalDebugFeature:(_WKFeature *)feature
{
    return protect(*_preferences)->isFeatureEnabled(Ref { *feature->_wrappedFeature });
}

- (void)_setEnabled:(BOOL)value forInternalDebugFeature:(_WKFeature *)feature
{
    protect(*_preferences)->setFeatureEnabled(Ref { *feature->_wrappedFeature }, value);
}

+ (NSArray<_WKExperimentalFeature *> *)_experimentalFeatures
{
    auto features = WebKit::WebPreferences::experimentalFeatures();
    return wrapper(API::Array::create(WTF::move(features))).autorelease();
}

- (BOOL)_isEnabledForFeature:(_WKFeature *)feature
{
    return protect(*_preferences)->isFeatureEnabled(Ref { *feature->_wrappedFeature });
}

- (void)_setEnabled:(BOOL)value forFeature:(_WKFeature *)feature
{
    protect(*_preferences)->setFeatureEnabled(Ref { *feature->_wrappedFeature }, value);
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
    protect(*_preferences)->disableRichJavaScriptFeatures();
}

- (void)_disableMediaPlaybackRelatedFeatures
{
    protect(*_preferences)->disableMediaPlaybackRelatedFeatures();
}

- (BOOL)_applePayCapabilityDisclosureAllowed
{
#if ENABLE(APPLE_PAY)
    return protect(*_preferences)->applePayCapabilityDisclosureAllowed();
#else
    return NO;
#endif
}

- (void)_setApplePayCapabilityDisclosureAllowed:(BOOL)applePayCapabilityDisclosureAllowed
{
#if ENABLE(APPLE_PAY)
    protect(*_preferences)->setApplePayCapabilityDisclosureAllowed(applePayCapabilityDisclosureAllowed);
#endif
}

- (BOOL)_shouldSuppressKeyboardInputDuringProvisionalNavigation
{
    return protect(*_preferences)->shouldSuppressTextInputFromEditingDuringProvisionalNavigation();
}

- (void)_setShouldSuppressKeyboardInputDuringProvisionalNavigation:(BOOL)shouldSuppress
{
    protect(*_preferences)->setShouldSuppressTextInputFromEditingDuringProvisionalNavigation(shouldSuppress);
}

- (BOOL)_loadsImagesAutomatically
{
    return protect(*_preferences)->loadsImagesAutomatically();
}

- (void)_setLoadsImagesAutomatically:(BOOL)loadsImagesAutomatically
{
    protect(*_preferences)->setLoadsImagesAutomatically(loadsImagesAutomatically);
}

- (BOOL)_peerConnectionEnabled
{
    return protect(*_preferences)->peerConnectionEnabled();
}

- (void)_setPeerConnectionEnabled:(BOOL)enabled
{
    protect(*_preferences)->setPeerConnectionEnabled(enabled);
}

- (BOOL)_mediaDevicesEnabled
{
    return protect(*_preferences)->mediaDevicesEnabled();
}

- (void)_setMediaDevicesEnabled:(BOOL)enabled
{
    protect(*_preferences)->setMediaDevicesEnabled(enabled);
}

- (BOOL)_getUserMediaRequiresFocus
{
    return protect(*_preferences)->getUserMediaRequiresFocus();
}

- (void)_setGetUserMediaRequiresFocus:(BOOL)enabled
{
    protect(*_preferences)->setGetUserMediaRequiresFocus(enabled);
}

- (BOOL)_screenCaptureEnabled
{
    return protect(*_preferences)->screenCaptureEnabled();
}

- (void)_setScreenCaptureEnabled:(BOOL)enabled
{
    protect(*_preferences)->setScreenCaptureEnabled(enabled);
}

- (BOOL)_mockCaptureDevicesEnabled
{
    return protect(*_preferences)->mockCaptureDevicesEnabled();
}

- (void)_setMockCaptureDevicesEnabled:(BOOL)enabled
{
    protect(*_preferences)->setMockCaptureDevicesEnabled(enabled);
}

- (BOOL)_mockCaptureDevicesPromptEnabled
{
    return protect(*_preferences)->mockCaptureDevicesPromptEnabled();
}

- (void)_setMockCaptureDevicesPromptEnabled:(BOOL)enabled
{
    protect(*_preferences)->setMockCaptureDevicesPromptEnabled(enabled);
}

- (BOOL)_mediaCaptureRequiresSecureConnection
{
    return protect(*_preferences)->mediaCaptureRequiresSecureConnection();
}

- (void)_setMediaCaptureRequiresSecureConnection:(BOOL)requiresSecureConnection
{
    protect(*_preferences)->setMediaCaptureRequiresSecureConnection(requiresSecureConnection);
}

- (double)_inactiveMediaCaptureStreamRepromptIntervalInMinutes
{
    return protect(*_preferences)->inactiveMediaCaptureStreamRepromptIntervalInMinutes();
}

- (void)_setInactiveMediaCaptureStreamRepromptIntervalInMinutes:(double)interval
{
    protect(*_preferences)->setInactiveMediaCaptureStreamRepromptIntervalInMinutes(interval);
}

- (double)_inactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes
{
    return protect(*_preferences)->inactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes();
}

- (void)_setInactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes:(double)interval
{
    protect(*_preferences)->setInactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes(interval);
}

- (BOOL)_interruptAudioOnPageVisibilityChangeEnabled
{
    return protect(*_preferences)->interruptAudioOnPageVisibilityChangeEnabled();
}

- (void)_setInterruptAudioOnPageVisibilityChangeEnabled:(BOOL)enabled
{
    protect(*_preferences)->setInterruptAudioOnPageVisibilityChangeEnabled(enabled);
}

- (BOOL)_enumeratingAllNetworkInterfacesEnabled
{
    return protect(*_preferences)->enumeratingAllNetworkInterfacesEnabled();
}

- (void)_setEnumeratingAllNetworkInterfacesEnabled:(BOOL)enabled
{
    protect(*_preferences)->setEnumeratingAllNetworkInterfacesEnabled(enabled);
}

- (BOOL)_iceCandidateFilteringEnabled
{
    return protect(*_preferences)->iceCandidateFilteringEnabled();
}

- (void)_setICECandidateFilteringEnabled:(BOOL)enabled
{
    protect(*_preferences)->setICECandidateFilteringEnabled(enabled);
}

- (void)_setJavaScriptCanAccessClipboard:(BOOL)javaScriptCanAccessClipboard
{
    protect(*_preferences)->setJavaScriptCanAccessClipboard(javaScriptCanAccessClipboard);
}

- (BOOL)_shouldAllowUserInstalledFonts
{
    return protect(*_preferences)->shouldAllowUserInstalledFonts();
}

- (void)_setShouldAllowUserInstalledFonts:(BOOL)_shouldAllowUserInstalledFonts
{
    protect(*_preferences)->setShouldAllowUserInstalledFonts(_shouldAllowUserInstalledFonts);
}

static _WKEditableLinkBehavior NODELETE toAPI(WebCore::EditableLinkBehavior behavior)
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

static WebCore::EditableLinkBehavior NODELETE toEditableLinkBehavior(_WKEditableLinkBehavior wkBehavior)
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
    return toAPI(static_cast<WebCore::EditableLinkBehavior>(protect(*_preferences)->editableLinkBehavior()));
}

- (void)_setEditableLinkBehavior:(_WKEditableLinkBehavior)editableLinkBehavior
{
    protect(*_preferences)->setEditableLinkBehavior(static_cast<uint32_t>(toEditableLinkBehavior(editableLinkBehavior)));
}

- (void)_setAVFoundationEnabled:(BOOL)enabled
{
    protect(*_preferences)->setAVFoundationEnabled(enabled);
}

- (BOOL)_avFoundationEnabled
{
    return protect(*_preferences)->isAVFoundationEnabled();
}

- (void)_setTextExtractionEnabled:(BOOL)enabled
{
    protect(*_preferences)->setTextExtractionEnabled(enabled);
}

- (BOOL)_textExtractionEnabled
{
    return protect(*_preferences)->textExtractionEnabled();
}

- (void)_setColorFilterEnabled:(BOOL)enabled
{
    protect(*_preferences)->setColorFilterEnabled(enabled);
}

- (BOOL)_colorFilterEnabled
{
    return protect(*_preferences)->colorFilterEnabled();
}

- (void)_setPunchOutWhiteBackgroundsInDarkMode:(BOOL)punches
{
    protect(*_preferences)->setPunchOutWhiteBackgroundsInDarkMode(punches);
}

- (BOOL)_punchOutWhiteBackgroundsInDarkMode
{
    return protect(*_preferences)->punchOutWhiteBackgroundsInDarkMode();
}

- (void)_setLowPowerVideoAudioBufferSizeEnabled:(BOOL)enabled
{
    protect(*_preferences)->setLowPowerVideoAudioBufferSizeEnabled(enabled);
}

- (BOOL)_lowPowerVideoAudioBufferSizeEnabled
{
    return protect(*_preferences)->lowPowerVideoAudioBufferSizeEnabled();
}

- (void)_setShouldIgnoreMetaViewport:(BOOL)ignoreMetaViewport
{
    return protect(*_preferences)->setShouldIgnoreMetaViewport(ignoreMetaViewport);
}

- (BOOL)_shouldIgnoreMetaViewport
{
    return protect(*_preferences)->shouldIgnoreMetaViewport();
}

- (void)_setNeedsSiteSpecificQuirks:(BOOL)enabled
{
    protect(*_preferences)->setNeedsSiteSpecificQuirks(enabled);
}

- (BOOL)_needsSiteSpecificQuirks
{
    return protect(*_preferences)->needsSiteSpecificQuirks();
}

- (void)_setItpDebugModeEnabled:(BOOL)enabled
{
    protect(*_preferences)->setItpDebugModeEnabled(enabled);
}

- (BOOL)_itpDebugModeEnabled
{
    return protect(*_preferences)->itpDebugModeEnabled();
}

- (void)_setMediaSourceEnabled:(BOOL)enabled
{
    protect(*_preferences)->setMediaSourceEnabled(enabled);
}

- (BOOL)_mediaSourceEnabled
{
    return protect(*_preferences)->mediaSourceEnabled();
}

- (void)_setManagedMediaSourceEnabled:(BOOL)enabled
{
    protect(*_preferences)->setManagedMediaSourceEnabled(enabled);
}

- (BOOL)_managedMediaSourceEnabled
{
    return protect(*_preferences)->managedMediaSourceEnabled();
}

- (void)_setManagedMediaSourceLowThreshold:(double)threshold
{
    protect(*_preferences)->setManagedMediaSourceLowThreshold(threshold);
}

- (double)_managedMediaSourceLowThreshold
{
    return protect(*_preferences)->managedMediaSourceLowThreshold();
}

- (void)_setManagedMediaSourceHighThreshold:(double)threshold
{
    protect(*_preferences)->setManagedMediaSourceHighThreshold(threshold);
}

- (double)_managedMediaSourceHighThreshold
{
    return protect(*_preferences)->managedMediaSourceHighThreshold();
}

- (BOOL)_secureContextChecksEnabled
{
    return protect(*_preferences)->secureContextChecksEnabled();
}

- (void)_setSecureContextChecksEnabled:(BOOL)enabled
{
    protect(*_preferences)->setSecureContextChecksEnabled(enabled);
}

- (void)_setWebAudioEnabled:(BOOL)enabled
{
    protect(*_preferences)->setWebAudioEnabled(enabled);
}

- (BOOL)_webAudioEnabled
{
    return protect(*_preferences)->webAudioEnabled();
}

- (void)_setAcceleratedCompositingEnabled:(BOOL)enabled
{
    protect(*_preferences)->setAcceleratedCompositingEnabled(enabled);
}

- (BOOL)_acceleratedCompositingEnabled
{
    return protect(*_preferences)->acceleratedCompositingEnabled();
}

- (BOOL)_remotePlaybackEnabled
{
    return protect(*_preferences)->remotePlaybackEnabled();
}

- (void)_setRemotePlaybackEnabled:(BOOL)enabled
{
    protect(*_preferences)->setRemotePlaybackEnabled(enabled);
}

- (BOOL)_serviceWorkerEntitlementDisabledForTesting
{
    return protect(*_preferences)->serviceWorkerEntitlementDisabledForTesting();
}

- (void)_setServiceWorkerEntitlementDisabledForTesting:(BOOL)disable
{
    protect(*_preferences)->setServiceWorkerEntitlementDisabledForTesting(disable);
}

#if PLATFORM(MAC)
- (void)_setCanvasUsesAcceleratedDrawing:(BOOL)enabled
{
    protect(*_preferences)->setCanvasUsesAcceleratedDrawing(enabled);
}

- (BOOL)_canvasUsesAcceleratedDrawing
{
    return protect(*_preferences)->canvasUsesAcceleratedDrawing();
}

- (void)_setDefaultTextEncodingName:(NSString *)name
{
    protect(*_preferences)->setDefaultTextEncodingName(name);
}

- (NSString *)_defaultTextEncodingName
{
    return protect(*_preferences)->defaultTextEncodingName().createNSString().autorelease();
}

- (void)_setAuthorAndUserStylesEnabled:(BOOL)enabled
{
    protect(*_preferences)->setAuthorAndUserStylesEnabled(enabled);
}

- (BOOL)_authorAndUserStylesEnabled
{
    return protect(*_preferences)->authorAndUserStylesEnabled();
}

- (void)_setDOMTimersThrottlingEnabled:(BOOL)enabled
{
    protect(*_preferences)->setDOMTimersThrottlingEnabled(enabled);
}

- (BOOL)_domTimersThrottlingEnabled
{
    return protect(*_preferences)->domTimersThrottlingEnabled();
}

- (void)_setWebArchiveDebugModeEnabled:(BOOL)enabled
{
    protect(*_preferences)->setWebArchiveDebugModeEnabled(enabled);
}

- (BOOL)_webArchiveDebugModeEnabled
{
    return protect(*_preferences)->webArchiveDebugModeEnabled();
}

- (void)_setLocalFileContentSniffingEnabled:(BOOL)enabled
{
    protect(*_preferences)->setLocalFileContentSniffingEnabled(enabled);
}

- (BOOL)_localFileContentSniffingEnabled
{
    return protect(*_preferences)->localFileContentSniffingEnabled();
}

- (void)_setUsesPageCache:(BOOL)enabled
{
    protect(*_preferences)->setUsesBackForwardCache(enabled);
}

- (BOOL)_usesPageCache
{
    return protect(*_preferences)->usesBackForwardCache();
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
    protect(*_preferences)->setWebSecurityEnabled(enabled);
}

- (BOOL)_webSecurityEnabled
{
    return protect(*_preferences)->webSecurityEnabled();
}

- (void)_setUniversalAccessFromFileURLsAllowed:(BOOL)enabled
{
    protect(*_preferences)->setAllowUniversalAccessFromFileURLs(enabled);
}

- (BOOL)_universalAccessFromFileURLsAllowed
{
    return protect(*_preferences)->allowUniversalAccessFromFileURLs();
}

- (void)_setTopNavigationToDataURLsAllowed:(BOOL)enabled
{
    protect(*_preferences)->setAllowTopNavigationToDataURLs(enabled);
}

- (BOOL)_topNavigationToDataURLsAllowed
{
    return protect(*_preferences)->allowTopNavigationToDataURLs();
}

- (void)_setSuppressesIncrementalRendering:(BOOL)enabled
{
    protect(*_preferences)->setSuppressesIncrementalRendering(enabled);
}

- (BOOL)_suppressesIncrementalRendering
{
    return protect(*_preferences)->suppressesIncrementalRendering();
}

- (void)_setCookieEnabled:(BOOL)enabled
{
    protect(*_preferences)->setCookieEnabled(enabled);
}

- (BOOL)_cookieEnabled
{
    return protect(*_preferences)->cookieEnabled();
}

- (void)_setViewGestureDebuggingEnabled:(BOOL)enabled
{
    protect(*_preferences)->setViewGestureDebuggingEnabled(enabled);
}

- (BOOL)_viewGestureDebuggingEnabled
{
    return protect(*_preferences)->viewGestureDebuggingEnabled();
}

- (void)_setStandardFontFamily:(NSString *)family
{
    protect(*_preferences)->setStandardFontFamily(family);
}

- (NSString *)_standardFontFamily
{
    return protect(*_preferences)->standardFontFamily().createNSString().autorelease();
}

- (void)_setBackspaceKeyNavigationEnabled:(BOOL)enabled
{
    protect(*_preferences)->setBackspaceKeyNavigationEnabled(enabled);
}

- (BOOL)_backspaceKeyNavigationEnabled
{
    return protect(*_preferences)->backspaceKeyNavigationEnabled();
}

- (void)_setWebGLEnabled:(BOOL)enabled
{
    protect(*_preferences)->setWebGLEnabled(enabled);
}

- (BOOL)_webGLEnabled
{
    return protect(*_preferences)->webGLEnabled();
}

- (void)_setAllowsInlineMediaPlayback:(BOOL)enabled
{
    protect(*_preferences)->setAllowsInlineMediaPlayback(enabled);
}

- (BOOL)_allowsInlineMediaPlayback
{
    return protect(*_preferences)->allowsInlineMediaPlayback();
}

- (void)_setApplePayEnabled:(BOOL)enabled
{
    protect(*_preferences)->setApplePayEnabled(enabled);
}

- (BOOL)_applePayEnabled
{
    return protect(*_preferences)->applePayEnabled();
}

- (void)_setInlineMediaPlaybackRequiresPlaysInlineAttribute:(BOOL)enabled
{
    protect(*_preferences)->setInlineMediaPlaybackRequiresPlaysInlineAttribute(enabled);
}

- (BOOL)_inlineMediaPlaybackRequiresPlaysInlineAttribute
{
    return protect(*_preferences)->inlineMediaPlaybackRequiresPlaysInlineAttribute();
}

- (void)_setInvisibleMediaAutoplayNotPermitted:(BOOL)enabled
{
    protect(*_preferences)->setInvisibleAutoplayNotPermitted(enabled);
}

- (BOOL)_invisibleMediaAutoplayNotPermitted
{
    return protect(*_preferences)->invisibleAutoplayNotPermitted();
}

- (void)_setLegacyEncryptedMediaAPIEnabled:(BOOL)enabled
{
    protect(*_preferences)->setLegacyEncryptedMediaAPIEnabled(enabled);
}

- (BOOL)_legacyEncryptedMediaAPIEnabled
{
    return protect(*_preferences)->legacyEncryptedMediaAPIEnabled();
}

- (void)_setMainContentUserGestureOverrideEnabled:(BOOL)enabled
{
    protect(*_preferences)->setMainContentUserGestureOverrideEnabled(enabled);
}

- (BOOL)_mainContentUserGestureOverrideEnabled
{
    return protect(*_preferences)->mainContentUserGestureOverrideEnabled();
}

- (void)_setNeedsStorageAccessFromFileURLsQuirk:(BOOL)enabled
{
    protect(*_preferences)->setNeedsStorageAccessFromFileURLsQuirk(enabled);
}

- (BOOL)_needsStorageAccessFromFileURLsQuirk
{
    return protect(*_preferences)->needsStorageAccessFromFileURLsQuirk();
}

- (void)_setPDFPluginEnabled:(BOOL)enabled
{
    protect(*_preferences)->setPDFPluginEnabled(enabled);
}

- (BOOL)_pdfPluginEnabled
{
    return protect(*_preferences)->pdfPluginEnabled();
}

- (void)_setRequiresUserGestureForAudioPlayback:(BOOL)enabled
{
    protect(*_preferences)->setRequiresUserGestureForAudioPlayback(enabled);
}

- (BOOL)_requiresUserGestureForAudioPlayback
{
    return protect(*_preferences)->requiresUserGestureForAudioPlayback();
}

- (void)_setRequiresUserGestureForVideoPlayback:(BOOL)enabled
{
    protect(*_preferences)->setRequiresUserGestureForVideoPlayback(enabled);
}

- (BOOL)_requiresUserGestureForVideoPlayback
{
    return protect(*_preferences)->requiresUserGestureForVideoPlayback();
}

- (void)_setServiceControlsEnabled:(BOOL)enabled
{
    protect(*_preferences)->setServiceControlsEnabled(enabled);
}

- (BOOL)_serviceControlsEnabled
{
    return protect(*_preferences)->serviceControlsEnabled();
}

- (void)_setShowsToolTipOverTruncatedText:(BOOL)enabled
{
    protect(*_preferences)->setShowsToolTipOverTruncatedText(enabled);
}

- (BOOL)_showsToolTipOverTruncatedText
{
    return protect(*_preferences)->showsToolTipOverTruncatedText();
}

- (void)_setTextAreasAreResizable:(BOOL)enabled
{
    protect(*_preferences)->setTextAreasAreResizable(enabled);
}

- (BOOL)_textAreasAreResizable
{
    return protect(*_preferences)->textAreasAreResizable();
}

- (void)_setUseGiantTiles:(BOOL)enabled
{
    protect(*_preferences)->setUseGiantTiles(enabled);
}

- (BOOL)_useGiantTiles
{
    return protect(*_preferences)->useGiantTiles();
}

- (void)_setWantsBalancedSetDefersLoadingBehavior:(BOOL)enabled
{
    protect(*_preferences)->setWantsBalancedSetDefersLoadingBehavior(enabled);
}

- (BOOL)_wantsBalancedSetDefersLoadingBehavior
{
    return protect(*_preferences)->wantsBalancedSetDefersLoadingBehavior();
}

- (void)_setAggressiveTileRetentionEnabled:(BOOL)enabled
{
    protect(*_preferences)->setAggressiveTileRetentionEnabled(enabled);
}

- (BOOL)_aggressiveTileRetentionEnabled
{
    return protect(*_preferences)->aggressiveTileRetentionEnabled();
}

- (void)_setAppNapEnabled:(BOOL)enabled
{
    protect(*_preferences)->setPageVisibilityBasedProcessSuppressionEnabled(enabled);
}

- (BOOL)_appNapEnabled
{
    return protect(*_preferences)->pageVisibilityBasedProcessSuppressionEnabled();
}

#endif // PLATFORM(MAC)

- (BOOL)_javaScriptCanAccessClipboard
{
    return protect(*_preferences)->javaScriptCanAccessClipboard();
}

- (void)_setDOMPasteAllowed:(BOOL)domPasteAllowed
{
    protect(*_preferences)->setDOMPasteAllowed(domPasteAllowed);
}

- (BOOL)_domPasteAllowed
{
    return protect(*_preferences)->domPasteAllowed();
}

- (void)_setShouldEnableTextAutosizingBoost:(BOOL)shouldEnableTextAutosizingBoost
{
#if ENABLE(TEXT_AUTOSIZING)
    protect(*_preferences)->setShouldEnableTextAutosizingBoost(shouldEnableTextAutosizingBoost);
#endif
}

- (BOOL)_shouldEnableTextAutosizingBoost
{
#if ENABLE(TEXT_AUTOSIZING)
    return protect(*_preferences)->shouldEnableTextAutosizingBoost();
#else
    return NO;
#endif
}

- (BOOL)_isSafeBrowsingEnabled
{
    return protect(*_preferences)->safeBrowsingEnabled();
}

- (void)_setSafeBrowsingEnabled:(BOOL)enabled
{
    protect(*_preferences)->setSafeBrowsingEnabled(enabled);
}

- (void)_setVideoQualityIncludesDisplayCompositingEnabled:(BOOL)videoQualityIncludesDisplayCompositingEnabled
{
    protect(*_preferences)->setVideoQualityIncludesDisplayCompositingEnabled(videoQualityIncludesDisplayCompositingEnabled);
}

- (BOOL)_videoQualityIncludesDisplayCompositingEnabled
{
    return protect(*_preferences)->videoQualityIncludesDisplayCompositingEnabled();
}

- (void)_setDeviceOrientationEventEnabled:(BOOL)enabled
{
#if ENABLE(DEVICE_ORIENTATION)
    protect(*_preferences)->setDeviceOrientationEventEnabled(enabled);
#endif
}

- (BOOL)_deviceOrientationEventEnabled
{
#if ENABLE(DEVICE_ORIENTATION)
    return protect(*_preferences)->deviceOrientationEventEnabled();
#else
    return false;
#endif
}

- (void)_setAccessibilityIsolatedTreeEnabled:(BOOL)accessibilityIsolatedTreeEnabled
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    protect(*_preferences)->setIsAccessibilityIsolatedTreeEnabled(accessibilityIsolatedTreeEnabled);
#endif
}

- (BOOL)_accessibilityIsolatedTreeEnabled
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    return protect(*_preferences)->isAccessibilityIsolatedTreeEnabled();
#else
    return false;
#endif
}

- (BOOL)_speechRecognitionEnabled
{
    return protect(*_preferences)->speechRecognitionEnabled();
}

- (void)_setSpeechRecognitionEnabled:(BOOL)speechRecognitionEnabled
{
    protect(*_preferences)->setSpeechRecognitionEnabled(speechRecognitionEnabled);
}

- (BOOL)_privateClickMeasurementEnabled
{
    return protect(*_preferences)->privateClickMeasurementEnabled();
}

- (void)_setPrivateClickMeasurementEnabled:(BOOL)privateClickMeasurementEnabled
{
    protect(*_preferences)->setPrivateClickMeasurementEnabled(privateClickMeasurementEnabled);
}

- (BOOL)_privateClickMeasurementDebugModeEnabled
{
    return protect(*_preferences)->privateClickMeasurementDebugModeEnabled();
}

- (void)_setPrivateClickMeasurementDebugModeEnabled:(BOOL)enabled
{
    protect(*_preferences)->setPrivateClickMeasurementDebugModeEnabled(enabled);
}

- (_WKPitchCorrectionAlgorithm)_pitchCorrectionAlgorithm
{
    return static_cast<_WKPitchCorrectionAlgorithm>(protect(*_preferences)->pitchCorrectionAlgorithm());
}

- (void)_setPitchCorrectionAlgorithm:(_WKPitchCorrectionAlgorithm)pitchCorrectionAlgorithm
{
    protect(*_preferences)->setPitchCorrectionAlgorithm(pitchCorrectionAlgorithm);
}

- (BOOL)_mediaSessionEnabled
{
    return protect(*_preferences)->mediaSessionEnabled();
}

- (void)_setMediaSessionEnabled:(BOOL)mediaSessionEnabled
{
    protect(*_preferences)->setMediaSessionEnabled(mediaSessionEnabled);
}

- (BOOL)_isExtensibleSSOEnabled
{
#if HAVE(APP_SSO)
    return protect(*_preferences)->isExtensibleSSOEnabled();
#else
    return false;
#endif
}

- (void)_setExtensibleSSOEnabled:(BOOL)extensibleSSOEnabled
{
#if HAVE(APP_SSO)
    protect(*_preferences)->setExtensibleSSOEnabled(extensibleSSOEnabled);
#endif
}

- (BOOL)_requiresPageVisibilityToPlayAudio
{
    return protect(*_preferences)->requiresPageVisibilityToPlayAudio();
}

- (void)_setRequiresPageVisibilityToPlayAudio:(BOOL)requiresVisibility
{
    protect(*_preferences)->setRequiresPageVisibilityToPlayAudio(requiresVisibility);
}

- (BOOL)_fileSystemAccessEnabled
{
    return protect(*_preferences)->fileSystemEnabled();
}

- (void)_setFileSystemAccessEnabled:(BOOL)fileSystemAccessEnabled
{
    protect(*_preferences)->setFileSystemEnabled(fileSystemAccessEnabled);
}

- (BOOL)_storageAPIEnabled
{
    return protect(*_preferences)->storageAPIEnabled();
}

- (void)_setStorageAPIEnabled:(BOOL)storageAPIEnabled
{
    protect(*_preferences)->setStorageAPIEnabled(storageAPIEnabled);
}

- (BOOL)_accessHandleEnabled
{
    return protect(*_preferences)->accessHandleEnabled();
}

- (void)_setAccessHandleEnabled:(BOOL)accessHandleEnabled
{
    protect(*_preferences)->setAccessHandleEnabled(accessHandleEnabled);
}

- (void)_setNotificationsEnabled:(BOOL)enabled
{
    protect(*_preferences)->setNotificationsEnabled(enabled);
}

- (BOOL)_notificationsEnabled
{
    return protect(*_preferences)->notificationsEnabled();
}

- (void)_setNotificationEventEnabled:(BOOL)enabled
{
    protect(*_preferences)->setNotificationEventEnabled(enabled);
}

- (BOOL)_notificationEventEnabled
{
    return protect(*_preferences)->notificationEventEnabled();
}

- (BOOL)_pushAPIEnabled
{
    return protect(*_preferences)->pushAPIEnabled();
}

- (void)_setPushAPIEnabled:(BOOL)pushAPIEnabled
{
    protect(*_preferences)->setPushAPIEnabled(pushAPIEnabled);
}

- (void)_setModelDocumentEnabled:(BOOL)enabled
{
    protect(*_preferences)->setModelDocumentEnabled(enabled);
}

- (BOOL)_modelDocumentEnabled
{
    return protect(*_preferences)->modelDocumentEnabled();
}

- (void)_setRequiresFullscreenToLockScreenOrientation:(BOOL)enabled
{
    protect(*_preferences)->setFullscreenRequirementForScreenOrientationLockingEnabled(enabled);
}

- (BOOL)_requiresFullscreenToLockScreenOrientation
{
    return protect(*_preferences)->fullscreenRequirementForScreenOrientationLockingEnabled();
}

- (void)_setInteractionRegionMinimumCornerRadius:(double)radius
{
    protect(*_preferences)->setInteractionRegionMinimumCornerRadius(radius);
}

- (double)_interactionRegionMinimumCornerRadius
{
    return protect(*_preferences)->interactionRegionMinimumCornerRadius();
}

- (void)_setInteractionRegionInlinePadding:(double)padding
{
    protect(*_preferences)->setInteractionRegionInlinePadding(padding);
}

- (double)_interactionRegionInlinePadding
{
    return protect(*_preferences)->interactionRegionInlinePadding();
}

- (void)_setMediaPreferredFullscreenWidth:(double)width
{
    protect(*_preferences)->setMediaPreferredFullscreenWidth(width);
}

- (double)_mediaPreferredFullscreenWidth
{
    return protect(*_preferences)->mediaPreferredFullscreenWidth();
}

- (void)_setAppBadgeEnabled:(BOOL)enabled
{
    protect(*_preferences)->setAppBadgeEnabled(enabled);
}

- (BOOL)_appBadgeEnabled
{
    return protect(*_preferences)->appBadgeEnabled();
}

- (void)_setVerifyWindowOpenUserGestureFromUIProcess:(BOOL)enabled
{
    protect(*_preferences)->setVerifyWindowOpenUserGestureFromUIProcess(enabled);
}

- (BOOL)_verifyWindowOpenUserGestureFromUIProcess
{
    return protect(*_preferences)->verifyWindowOpenUserGestureFromUIProcess();
}

- (BOOL)_mediaCapabilityGrantsEnabled
{
    return protect(*_preferences)->mediaCapabilityGrantsEnabled();
}

- (void)_setMediaCapabilityGrantsEnabled:(BOOL)mediaCapabilityGrantsEnabled
{
    protect(*_preferences)->setMediaCapabilityGrantsEnabled(mediaCapabilityGrantsEnabled);
}

- (void)_setAllowPrivacySensitiveOperationsInNonPersistentDataStores:(BOOL)allowPrivacySensitiveOperationsInNonPersistentDataStores
{
    protect(*_preferences)->setAllowPrivacySensitiveOperationsInNonPersistentDataStores(allowPrivacySensitiveOperationsInNonPersistentDataStores);
}

- (BOOL)_allowPrivacySensitiveOperationsInNonPersistentDataStores
{
    return protect(*_preferences)->allowPrivacySensitiveOperationsInNonPersistentDataStores();
}

- (void)_setVideoFullscreenRequiresElementFullscreen:(BOOL)videoFullscreenRequiresElementFullscreen
{
    protect(*_preferences)->setVideoFullscreenRequiresElementFullscreen(videoFullscreenRequiresElementFullscreen);
}

- (BOOL)_videoFullscreenRequiresElementFullscreen
{
    return protect(*_preferences)->videoFullscreenRequiresElementFullscreen();
}

- (void)_setCSSTransformStyleSeparatedEnabled:(BOOL)enabled
{
    protect(*_preferences)->setCSSTransformStyleSeparatedEnabled(enabled);
}

- (BOOL)_cssTransformStyleSeparatedEnabled
{
    return protect(*_preferences)->cssTransformStyleSeparatedEnabled();
}

- (void)_setOverlayRegionsEnabled:(BOOL)enabled
{
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    protect(*_preferences)->setOverlayRegionsEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

- (BOOL)_overlayRegionsEnabled
{
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    return protect(*_preferences)->overlayRegionsEnabled();
#else
    return NO;
#endif
}

- (void)_setModelElementEnabled:(BOOL)enabled
{
    protect(*_preferences)->setModelElementEnabled(enabled);
}

- (BOOL)_modelProcessEnabled
{
    return protect(*_preferences)->modelProcessEnabled();
}

- (void)_setModelProcessEnabled:(BOOL)enabled
{
    protect(*_preferences)->setModelProcessEnabled(enabled);
}

- (BOOL)_modelElementEnabled
{
    return protect(*_preferences)->modelElementEnabled();
}

- (void)_setModelNoPortalAttributeEnabled:(BOOL)enabled
{
    protect(*_preferences)->setModelNoPortalAttributeEnabled(enabled);
}

- (BOOL)_modelNoPortalAttributeEnabled
{
    return protect(*_preferences)->modelNoPortalAttributeEnabled();
}

- (void)_setUpdateSceneGeometryEnabled:(BOOL)enabled
{
    protect(*_preferences)->setUpdateSceneGeometryEnabled(enabled);
}

- (BOOL)_updateSceneGeometryEnabled
{
    return protect(*_preferences)->updateSceneGeometryEnabled();
}

- (void)_setRequiresPageVisibilityForVideoToBeNowPlayingForTesting:(BOOL)enabled
{
#if ENABLE(REQUIRES_PAGE_VISIBILITY_FOR_NOW_PLAYING)
    protect(*_preferences)->setRequiresPageVisibilityForVideoToBeNowPlaying(enabled);
#endif
}

- (BOOL)_requiresPageVisibilityForVideoToBeNowPlayingForTesting
{
#if ENABLE(REQUIRES_PAGE_VISIBILITY_FOR_NOW_PLAYING)
    return protect(*_preferences)->requiresPageVisibilityForVideoToBeNowPlaying();
#else
    return NO;
#endif
}

- (BOOL)_siteIsolationEnabled
{
    return protect(*_preferences)->siteIsolationEnabled();
}

- (void)_setSiteIsolationEnabled:(BOOL)enabled
{
    protect(*_preferences)->setSiteIsolationEnabled(enabled);
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
    return protect(*_preferences)->javaScriptEnabled();
}

- (void)setJavaScriptEnabled:(BOOL)javaScriptEnabled
{
    protect(*_preferences)->setJavaScriptEnabled(javaScriptEnabled);
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
