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
#define Webgpu_feature_status Preview
#endif

#if defined(ENABLE_WEBXR_WEBGPU_BY_DEFAULT) && ENABLE_WEBXR_WEBGPU_BY_DEFAULT && PLATFORM(VISION)
#define Webxr_layers_feature_status Stable
#else
#define Webxr_layers_feature_status Unstable
#endif

#if defined(ENABLE_WEBXR_WEBGPU_BY_DEFAULT) && ENABLE_WEBXR_WEBGPU_BY_DEFAULT && PLATFORM(VISION)
#define Webgpu_webxr_feature_status Stable
#else
#define Webgpu_webxr_feature_status Unstable
#endif

#if defined(ENABLE_UNIFIED_PDF_AS_PREVIEW) && ENABLE_UNIFIED_PDF_AS_PREVIEW
#define Unifiedpdf_feature_status Preview
#else
#define Unifiedpdf_feature_status Internal
#endif

#if defined(ENABLE_UNPREFIXED_BACKDROP_FILTER) && ENABLE_UNPREFIXED_BACKDROP_FILTER
#define Backdropfilter_feature_status Stable
#else
#define Backdropfilter_feature_status Testable
#endif

namespace WebKit {

#if PLATFORM(IOS_FAMILY)
bool defaultPassiveTouchListenersAsDefaultOnDocument();
bool defaultCSSOMViewScrollingAPIEnabled();
bool defaultShouldPrintBackgrounds();
bool defaultAlternateFormControlDesignEnabled();
bool defaultVideoFullscreenRequiresElementFullscreen();
bool defaultUseAsyncUIKitInteractions();
bool defaultWriteRichTextDataWhenCopyingOrDragging();
#if ENABLE(TEXT_AUTOSIZING)
bool defaultTextAutosizingUsesIdempotentMode();
#endif
#endif

#if PLATFORM(MAC)
bool defaultScrollAnimatorEnabled();
bool defaultPassiveWheelListenersAsDefaultOnDocument();
bool defaultWheelEventGesturesBecomeNonBlocking();
#endif

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
bool defaultDisallowSyncXHRDuringPageDismissalEnabled();
#endif

#if PLATFORM(MAC)
bool defaultAppleMailPaginationQuirkEnabled();
#endif

#if !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
bool allowsDeprecatedSynchronousXMLHttpRequestDuringUnload();
#endif

#if ENABLE(MEDIA_STREAM)
bool defaultCaptureAudioInGPUProcessEnabled();
bool defaultCaptureAudioInUIProcessEnabled();
bool defaultManageCaptureStatusBarInGPUProcessEnabled();
#endif

#if ENABLE(MEDIA_SOURCE) && PLATFORM(IOS_FAMILY)
bool defaultMediaSourceEnabled();
#endif

#if ENABLE(MEDIA_SOURCE)
bool defaultManagedMediaSourceEnabled();
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

#if PLATFORM(IOS_FAMILY)
bool defaultAutomaticLiveResizeEnabled();
#endif

bool defaultRunningBoardThrottlingEnabled();
bool defaultShouldDropNearSuspendedAssertionAfterDelay();
bool defaultShouldTakeNearSuspendedAssertion();
bool defaultShowModalDialogEnabled();
bool defaultLinearMediaPlayerEnabled();
bool defaultLiveRangeSelectionEnabled();

bool defaultShouldEnableScreenOrientationAPI();
bool defaultPopoverAttributeEnabled();
bool defaultUseGPUProcessForDOMRenderingEnabled();
bool defaultSearchInputIncrementalAttributeAndSearchEventEnabled();

#if HAVE(SC_CONTENT_SHARING_PICKER)
bool defaultUseSCContentSharingPicker();
#endif

#if USE(LIBWEBRTC)
bool defaultPeerConnectionEnabledAvailable();
#endif

} // namespace WebKit
