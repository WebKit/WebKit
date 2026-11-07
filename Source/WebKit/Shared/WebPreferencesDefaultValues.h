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

#pragma once

#include <wtf/Forward.h>

#if PLATFORM(IOS_FAMILY)
#define EXPERIMENTAL_FULLSCREEN_API_HIDDEN false
#else
#define EXPERIMENTAL_FULLSCREEN_API_HIDDEN true
#endif

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=269475 - this should not be needed
#if defined(ENABLE_WEBGPU_BY_DEFAULT) && ENABLE_WEBGPU_BY_DEFAULT
#define Webgpu_feature_status Stable
#else
#define Webgpu_feature_status Unstable
#endif

#if defined(ENABLE_WEBGPU_BY_DEFAULT) && ENABLE_WEBGPU_BY_DEFAULT && defined(HAVE_SUPPORT_HDR_DISPLAY) && HAVE_SUPPORT_HDR_DISPLAY
#define Webgpuhdr_feature_status Stable
#else
#define Webgpuhdr_feature_status Preview
#endif

#if defined(ENABLE_SUPPORT_HDR_DISPLAY_BY_DEFAULT) && ENABLE_SUPPORT_HDR_DISPLAY_BY_DEFAULT && defined(HAVE_SUPPORT_HDR_DISPLAY) && HAVE_SUPPORT_HDR_DISPLAY
#define Supporthdrdisplay_feature_status Stable
#else
#define Supporthdrdisplay_feature_status Testable
#endif

#if defined(ENABLE_WEBXR_LAYERS) && ENABLE_WEBXR_LAYERS && USE(OPENXR)
#define Webxr_layers_feature_status Testable
#else
#define Webxr_layers_feature_status Unstable
#endif

#if defined(ENABLE_WEBXR_WEBGPU) && ENABLE_WEBXR_WEBGPU && PLATFORM(VISION)
#define Webgpu_webxr_feature_status Stable
#elif defined(ENABLE_WEBXR_WEBGPU) && ENABLE_WEBXR_WEBGPU
#define Webgpu_webxr_feature_status Testable
#else
#define Webgpu_webxr_feature_status Unstable
#endif

#if defined(ENABLE_WEBXR) && ENABLE_WEBXR && PLATFORM(MAC)
#define Webxr_feature_status Preview
#elif defined(ENABLE_WEBXR) && ENABLE_WEBXR
#define Webxr_feature_status Stable
#else
#define Webxr_feature_status Unstable
#endif

#if defined(ENABLE_UNPREFIXED_BACKDROP_FILTER) && ENABLE_UNPREFIXED_BACKDROP_FILTER
#define Backdropfilter_feature_status Stable
#else
#define Backdropfilter_feature_status Testable
#endif

#if defined(ENABLE_MODEL_ELEMENT) && ENABLE_MODEL_ELEMENT && (PLATFORM(VISION) || ENABLE(GPU_PROCESS_MODEL))
#define Modelelement_feature_status Stable
#else
#define Modelelement_feature_status Testable
#endif

namespace WebKit {

#if HAVE(LIQUID_GLASS)
bool isLiquidGlassEnabled();
void setLiquidGlassEnabled(bool);
#endif

#if PLATFORM(IOS_FAMILY)
bool defaultPassiveTouchListenersAsDefaultOnDocument();
bool defaultShouldPrintBackgrounds();
bool defaultUseAsyncUIKitInteractions();
bool defaultWriteRichTextDataWhenCopyingOrDragging();
bool defaultTextAutosizingUsesIdempotentMode();
#endif

#if ENABLE(FULLSCREEN_API)
NODELETE bool defaultVideoFullscreenRequiresElementFullscreen();
#endif

#if PLATFORM(MAC)
bool defaultScrollAnimatorEnabled();
bool defaultPassiveWheelListenersAsDefaultOnDocument();
bool defaultWheelEventGesturesBecomeNonBlocking();
bool defaultAppleMailPaginationQuirkEnabled();
bool defaultUseAppKitGestures();
bool defaultTextInputClientSelectionUpdatesEnabled();
#endif

#if ENABLE(MEDIA_STREAM)
NODELETE bool defaultCaptureAudioInGPUProcessEnabled();
NODELETE bool defaultManageCaptureStatusBarInGPUProcessEnabled();
NODELETE double defaultInactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes();
#endif

#if ENABLE(MEDIA_SOURCE) && PLATFORM(IOS_FAMILY)
bool defaultMediaSourceEnabled();
#endif

#if ENABLE(MEDIA_SOURCE)
NODELETE bool defaultManagedMediaSourceEnabled();
NODELETE bool defaultMediaSourcePrefersDecompressionSession();
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
NODELETE bool defaultManagedMediaSourceNeedsAirPlay();
#endif
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
bool defaultMediaSessionCoordinatorEnabled();
#endif

#if ENABLE(IMAGE_ANALYSIS)
bool defaultTextRecognitionInVideosEnabled();
bool defaultVisualTranslationEnabled();
bool defaultRemoveBackgroundEnabled();
#endif

#if ENABLE(GAMEPAD)
NODELETE bool defaultGamepadVibrationActuatorEnabled();
#endif

#if ENABLE(WEB_AUTHN)
NODELETE bool defaultDigitalCredentialsEnabled();
#endif

#if ENABLE(APPLE_PAY) && PLATFORM(APPLETV)
bool defaultApplePayEnabled();
#endif

#if HAVE(AVEXPERIENCECONTROLLER)
bool defaultAVExperienceControllerFullscreenEnabled();
#endif

#if PLATFORM(IOS_FAMILY)
bool defaultAutomaticLiveResizeEnabled();
bool defaultAutomaticallyAdjustsViewScaleUsingMinimumEffectiveDeviceWidth();
double defaultLayoutViewportHeightExpansionFactor();
bool defaultVisuallyContiguousBidiTextSelectionEnabled();
bool defaultBidiContentAwarePasteEnabled();
#endif

#if PLATFORM(COCOA)
bool defaultExtendedProofreadingEnabled();
#endif

bool defaultRunningBoardThrottlingEnabled();
bool defaultShouldDropNearSuspendedAssertionAfterDelay();
bool defaultShouldTakeNearSuspendedAssertion();
bool defaultShowModalDialogEnabled();
NODELETE bool defaultLinearMediaPlayerEnabled();

NODELETE bool defaultShouldEnableScreenOrientationAPI();
bool defaultPopoverAttributeEnabled();
bool defaultUseGPUProcessForDOMRenderingEnabled();
NODELETE unsigned defaultMaximumNestedInlineFormattingContextCount();

#if USE(LIBWEBRTC)
bool defaultPeerConnectionEnabledAvailable();
#endif
NODELETE bool defaultWebRTCSocketsServiceClassEnabled();

#if ENABLE(HORIZONTAL_BANNER_VIEW_OVERLAYS)
NODELETE bool defaultHorizontalBannerViewOverlaysEnabled();
#endif

#if ENABLE(DEVICE_ORIENTATION)
NODELETE bool defaultDeviceOrientationPermissionAPIEnabled();
#endif

#if PLATFORM(APPLETV)
bool defaultRequiresPageVisibilityForVideoToBeNowPlaying();
#endif

NODELETE bool defaultRequiresUserGestureToStartAudiblePlaybackWhenHidden();

NODELETE bool defaultCookieStoreAPIEnabled();

bool defaultContentInsetBackgroundFillEnabled();
NODELETE bool defaultTopContentInsetBackgroundCanChangeAfterScrolling();

#if ENABLE(SCREEN_TIME)
bool defaultScreenTimeEnabled();
#endif

#if ENABLE(CONTENT_EXTENSIONS)
bool defaultIFrameResourceMonitoringEnabled();
#endif

bool defaultSearchInputResultsAttributeEnabled();

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
bool defaultPreferSpatialAudioExperience();
#endif

NODELETE bool defaultRTCEncodedStreamsQuirkEnabled();

bool defaultMutationEventsEnabled();

#if PLATFORM(COCOA)
bool isSafariOrWebApp();
#endif

bool defaultTrustedTypesEnabled();

NODELETE bool defaultGetBoundingClientRectZoomedEnabled();
NODELETE bool defaultFacebookLiveRecordingQuirkEnabled();
bool defaultFontFaceSetConstructorEnabled();

#if HAVE(MATERIAL_HOSTING)
bool defaultHostedBlurMaterialInMediaControlsEnabled();
#endif

bool defaultIOSurfaceLosslessCompressionEnabled();

#if ENABLE(UNIFIED_PDF)
bool defaultUnifiedPDFEnabled();
#endif

NODELETE bool defaultScrollbarColorEnabled();

NODELETE bool defaultAllowMultipleCommitLayerTreePending();

#if ENABLE(VIDEO)
bool defaultCaptionDisplaySettingsEnabled();
#endif

#if ENABLE(MEDIA_STREAM)
NODELETE bool defaultShouldEnableScreenCapture();
#endif

#if ENABLE(CONTENT_CHANGE_OBSERVER)
bool defaultContentChangeObserverEnabled();
#endif

#if HAVE(WEBCONTENTRESTRICTIONS_ASK_TO)
NODELETE bool defaultWebContentRestrictionsAskToEnabled();
#endif

bool defaultWebTransportEnabled();

} // namespace WebKit
