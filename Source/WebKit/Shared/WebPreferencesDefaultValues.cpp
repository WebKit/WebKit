/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebPreferencesDefaultValues.h"

#include <WebCore/RuntimeApplicationChecks.h>

#if PLATFORM(COCOA)
#include "VersionChecks.h"
#include <pal/spi/cocoa/FeatureFlagsSPI.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebKit {

bool defaultPassiveTouchListenersAsDefaultOnDocument()
{
#if PLATFORM(IOS_FAMILY)
    return linkedOnOrAfter(WebKit::SDKVersion::FirstThatDefaultsToPassiveTouchListenersOnDocument);
#else
    return true;
#endif
}

bool defaultCSSOMViewScrollingAPIEnabled()
{
#if PLATFORM(IOS_FAMILY)
    if (WebCore::IOSApplication::isIMDb() && applicationSDKVersion() < DYLD_IOS_VERSION_13_0)
        return false;
#endif
    return true;
}

#if ENABLE(TEXT_AUTOSIZING) && !PLATFORM(IOS_FAMILY)

bool defaultTextAutosizingUsesIdempotentMode()
{
    return false;
}

#endif // ENABLE(TEXT_AUTOSIZING) && !PLATFORM(IOS_FAMILY)

bool defaultDisallowSyncXHRDuringPageDismissalEnabled()
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (CFPreferencesGetAppBooleanValue(CFSTR("allowDeprecatedSynchronousXMLHttpRequestDuringUnload"), CFSTR("com.apple.WebKit"), nullptr)) {
        WTFLogAlways("Allowing synchronous XHR during page unload due to managed preference");
        return false;
    }
#elif PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    if (allowsDeprecatedSynchronousXMLHttpRequestDuringUnload()) {
        WTFLogAlways("Allowing synchronous XHR during page unload due to managed preference");
        return false;
    }
#endif
    return true;
}

static bool defaultAsyncFrameAndOverflowScrollingEnabled()
{
#if PLATFORM(IOS_FAMILY)
    return true;
#endif

#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("async_frame_and_overflow_scrolling");
#endif

#if PLATFORM(MAC)
    return true;
#endif

    return false;
}

bool defaultAsyncFrameScrollingEnabled()
{
#if USE(NICOSIA)
    return true;
#endif

    return defaultAsyncFrameAndOverflowScrollingEnabled();
}

bool defaultAsyncOverflowScrollingEnabled()
{
    return defaultAsyncFrameAndOverflowScrollingEnabled();
}

#if ENABLE(GPU_PROCESS)

bool defaultUseGPUProcessForMedia()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("canvas_and_media_in_gpu_process");
#endif

    return false;
}

#endif // ENABLE(GPU_PROCESS)

bool defaultRenderCanvasInGPUProcessEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("canvas_and_media_in_gpu_process");
#endif

    return false;
}

#if ENABLE(MEDIA_STREAM)

bool defaultCaptureAudioInGPUProcessEnabled()
{
#if PLATFORM(MAC) && HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("webrtc_in_gpu_process");
#endif

#if PLATFORM(IOS_FAMILY) && HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("canvas_and_media_in_gpu_process");
#endif

    return false;
}

bool defaultCaptureAudioInUIProcessEnabled()
{
#if PLATFORM(IOS_FAMILY)
    return false;
#endif

#if PLATFORM(MAC)
    return !defaultCaptureAudioInGPUProcessEnabled();
#endif

    return false;
}

bool defaultCaptureVideoInGPUProcessEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("webrtc_in_gpu_process");
#endif

    return false;
}

#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(WEB_RTC)

bool defaultWebRTCCodecsInGPUProcess()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("webrtc_in_gpu_process");
#endif

    return false;
}

#endif // ENABLE(WEB_RTC)

#if ENABLE(WEBGPU)

bool defaultWebGPUEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("WebGPU");
#endif

    return false;
}

#endif // ENABLE(WEBGPU)

bool defaultInAppBrowserPrivacy()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("InAppBrowserPrivacy");
#endif

    return false;
}

#if HAVE(INCREMENTAL_PDF_APIS)
bool defaultIncrementalPDFEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("incremental_pdf");
#endif

    return false;
}
#endif

#if ENABLE(WEBXR)

bool defaultWebXREnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("WebXR");
#endif

    return false;
}

#endif // ENABLE(WEBXR)

#if ENABLE(VP9)
bool defaultVP9DecoderEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("vp9_decoder");
#endif

    return true;
}
#endif

#if ENABLE(VP9)
bool defaultVP9SWDecoderEnabledOnBattery()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("SW_vp9_decoder_on_battery");
#endif

    return false;
}
#endif

#if ENABLE(MEDIA_SOURCE) && ENABLE(VP9)
bool defaultWebMParserEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("webm_parser");
#endif

    return true;
}
#endif

#if ENABLE(WEB_RTC)
bool defaultWebRTCH264LowLatencyEncoderEnabled()
{
    return true;
}
#endif

} // namespace WebKit
