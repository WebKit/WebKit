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

#define Webxr_layers_feature_status Unstable

#if defined(ENABLE_WEBXR_WEBGPU) && ENABLE_WEBXR_WEBGPU && PLATFORM(VISION)
#define Webgpu_webxr_feature_status Stable
#else
#define Webgpu_webxr_feature_status Unstable
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
#if ENABLE(TEXT_AUTOSIZING)
bool defaultTextAutosizingUsesIdempotentMode();
#endif
#endif

#if ENABLE(FULLSCREEN_API)
bool defaultVideoFullscreenRequiresElementFullscreen();
#endif

#if PLATFORM(MAC)
bool defaultScrollAnimatorEnabled();
bool defaultPassiveWheelListenersAsDefaultOnDocument();
bool defaultWheelEventGesturesBecomeNonBlocking();
bool defaultAppleMailPaginationQuirkEnabled();
#endif

#if ENABLE(MEDIA_STREAM)
bool defaultCaptureAudioInGPUProcessEnabled();
bool defaultManageCaptureStatusBarInGPUProcessEnabled();
double defaultInactiveMediaCaptureStreamRepromptWithoutUserGestureIntervalInMinutes();
#endif

#if ENABLE(MEDIA_SOURCE) && PLATFORM(IOS_FAMILY)
bool defaultMediaSourceEnabled();
#endif

#if ENABLE(MEDIA_SOURCE)
bool defaultManagedMediaSourceEnabled();
bool defaultMediaSourcePrefersDecompressionSession();
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
bool defaultManagedMediaSourceNeedsAirPlay();
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
bool defaultGamepadVibrationActuatorEnabled();
#endif

#if ENABLE(WEB_AUTHN)
bool defaultDigitalCredentialsEnabled();
#endif

#if PLATFORM(IOS_FAMILY)
bool defaultAutomaticLiveResizeEnabled();
bool defaultVisuallyContiguousBidiTextSelectionEnabled();
bool defaultBidiContentAwarePasteEnabled();
#endif

bool defaultRunningBoardThrottlingEnabled();
bool defaultShouldDropNearSuspendedAssertionAfterDelay();
bool defaultShouldTakeNearSuspendedAssertion();
bool defaultShowModalDialogEnabled();
bool defaultLinearMediaPlayerEnabled();

bool defaultShouldEnableScreenOrientationAPI();
bool defaultPopoverAttributeEnabled();
bool defaultUseGPUProcessForDOMRenderingEnabled();

#if USE(LIBWEBRTC)
bool defaultPeerConnectionEnabledAvailable();
#endif
bool defaultWebRTCSocketsServiceClassEnabled();

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
bool defaultBuiltInNotificationsEnabled();
#endif

#if ENABLE(DEVICE_ORIENTATION)
bool defaultDeviceOrientationPermissionAPIEnabled();
#endif

#if ENABLE(REQUIRES_PAGE_VISIBILITY_FOR_NOW_PLAYING)
bool defaultRequiresPageVisibilityForVideoToBeNowPlaying();
#endif

bool defaultCookieStoreAPIEnabled();

bool defaultContentInsetBackgroundFillEnabled();
bool defaultTopContentInsetBackgroundCanChangeAfterScrolling();

#if ENABLE(SCREEN_TIME)
bool defaultScreenTimeEnabled();
#endif

#if ENABLE(CONTENT_EXTENSIONS)
bool defaultIFrameResourceMonitoringEnabled();
#endif

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
bool defaultPreferSpatialAudioExperience();
#endif

bool defaultRTCEncodedStreamsQuirkEnabled();

bool defaultMutationEventsEnabled();

bool defaultTrustedTypesEnabled();

bool defaultGetBoundingClientRectZoomedEnabled();
bool defaultFacebookLiveRecordingQuirkEnabled();

#if HAVE(MATERIAL_HOSTING)
bool defaultHostedBlurMaterialInMediaControlsEnabled();
#endif

bool defaultIOSurfaceLosslessCompressionEnabled();

#if ENABLE(UNIFIED_PDF)
bool defaultUnifiedPDFEnabled();
#endif

bool defaultScrollbarColorEnabled();

bool defaultAllowMultipleCommitLayerTreePending();

} // namespace WebKit
