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

#if PLATFORM(IOS_FAMILY)

bool defaultPassiveTouchListenersAsDefaultOnDocument()
{
    static bool result = linkedOnOrAfter(WebKit::SDKVersion::FirstThatDefaultsToPassiveTouchListenersOnDocument);
    return result;
}

bool defaultCSSOMViewScrollingAPIEnabled()
{
    static bool result = WebCore::IOSApplication::isIMDb() && applicationSDKVersion() < DYLD_IOS_VERSION_13_0;
    return !result;
}

#endif

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)

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

#endif


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

bool defaultUseGPUProcessForCanvasRenderingEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("gpu_process_canvas_rendering");
#endif

    return false;
}

bool defaultUseGPUProcessForDOMRenderingEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("gpu_process_dom_rendering");
#endif

    return false;
}

bool defaultUseGPUProcessForMediaEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("gpu_process_media");
#endif

    return false;
}

bool defaultUseGPUProcessForWebGLEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("gpu_process_webgl");
#endif

    return false;
}

#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MEDIA_STREAM)

bool defaultCaptureAudioInGPUProcessEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
#if PLATFORM(MAC)
    return isFeatureFlagEnabled("gpu_process_webrtc");
#elif PLATFORM(IOS_FAMILY)
    return isFeatureFlagEnabled("canvas_and_media_in_gpu_process");
#endif
#endif

    return false;
}

bool defaultCaptureAudioInUIProcessEnabled()
{
#if PLATFORM(MAC)
    return !defaultCaptureAudioInGPUProcessEnabled();
#endif

    return false;
}

bool defaultCaptureVideoInGPUProcessEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("gpu_process_webrtc");
#endif

    return false;
}

bool defaultCaptureVideoInUIProcessEnabled()
{
#if PLATFORM(MAC)
    return !MacApplication::isSafari();
#endif

    return false;
}

#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(WEB_RTC)

bool defaultWebRTCCodecsInGPUProcess()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("gpu_process_webrtc");
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

bool defaultVP9SWDecoderEnabledOnBattery()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("SW_vp9_decoder_on_battery");
#endif

    return false;
}

#if ENABLE(MEDIA_SOURCE)

bool defaultWebMParserEnabled()
{
#if HAVE(SYSTEM_FEATURE_FLAGS)
    return isFeatureFlagEnabled("webm_parser");
#endif

    return true;
}

#endif // ENABLE(MEDIA_SOURCE)
#endif // ENABLE(VP9)

} // namespace WebKit
