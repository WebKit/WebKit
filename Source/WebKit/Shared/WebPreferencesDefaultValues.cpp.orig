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
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <WebCore/VersionChecks.h>
#include <pal/spi/cocoa/FeatureFlagsSPI.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
#import <wtf/cocoa/Entitlements.h>
#endif

namespace WebKit {

#if !PLATFORM(COCOA)
bool isFeatureFlagEnabled(const char*, bool defaultValue)
{
    return defaultValue;
}
#endif

#if PLATFORM(IOS_FAMILY)

bool defaultPassiveTouchListenersAsDefaultOnDocument()
{
    static bool result = linkedOnOrAfter(WebCore::SDKVersion::FirstThatDefaultsToPassiveTouchListenersOnDocument);
    return result;
}

bool defaultCSSOMViewScrollingAPIEnabled()
{
    static bool result = WebCore::IOSApplication::isIMDb() && applicationSDKVersion() < DYLD_IOS_VERSION_13_0;
    return !result;
}

#endif

#if PLATFORM(MAC)

bool defaultPassiveWheelListenersAsDefaultOnDocument()
{
    static bool result = linkedOnOrAfter(WebCore::SDKVersion::FirstThatDefaultsToPassiveWheelListenersOnDocument);
    return result;
}

bool defaultWheelEventGesturesBecomeNonBlocking()
{
    static bool result = linkedOnOrAfter(WebCore::SDKVersion::FirstThatAllowsWheelEventGesturesToBecomeNonBlocking);
    return result;
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
#elif PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
    if (allowsDeprecatedSynchronousXMLHttpRequestDuringUnload()) {
        WTFLogAlways("Allowing synchronous XHR during page unload due to managed preference");
        return false;
    }
#endif
    return true;
}

#endif

#if PLATFORM(MAC)

bool defaultAppleMailPaginationQuirkEnabled()
{
    return WebCore::MacApplication::isAppleMail();
}

#endif

static bool defaultAsyncFrameAndOverflowScrollingEnabled()
{
#if PLATFORM(IOS_FAMILY)
    return true;
#endif

#if PLATFORM(MAC)
    bool defaultValue = true;
#else
    bool defaultValue = false;
#endif

    return isFeatureFlagEnabled("async_frame_and_overflow_scrolling", defaultValue);
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

bool defaultOfflineWebApplicationCacheEnabled()
{
#if PLATFORM(COCOA)
    static bool newSDK = linkedOnOrAfter(WebCore::SDKVersion::FirstWithApplicationCacheDisabledByDefault);
    return !newSDK;
#else
    // FIXME: Other platforms should consider turning this off.
    // ApplicationCache is on its way to being removed from WebKit.
    return true;
#endif
}

#if ENABLE(GPU_PROCESS)

bool defaultUseGPUProcessForCanvasRenderingEnabled()
{
#if ENABLE(GPU_PROCESS_BY_DEFAULT)
    bool defaultValue = true;
#else
    bool defaultValue = false;
#endif

    return isFeatureFlagEnabled("gpu_process_canvas_rendering", defaultValue);
}

bool defaultUseGPUProcessForDOMRenderingEnabled()
{
    return isFeatureFlagEnabled("gpu_process_dom_rendering", false);
}

bool defaultUseGPUProcessForMediaEnabled()
{
#if ENABLE(GPU_PROCESS_BY_DEFAULT)
    bool defaultValue = true;
#else
    bool defaultValue = false;
#endif

    return isFeatureFlagEnabled("gpu_process_media", defaultValue);
}

bool defaultUseGPUProcessForWebGLEnabled()
{
    return isFeatureFlagEnabled("gpu_process_webgl", false);
}

#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MEDIA_STREAM)

bool defaultCaptureAudioInGPUProcessEnabled()
{
#if PLATFORM(MAC)
    // FIXME: Enable GPU process audio capture when <rdar://problem/29448368> is fixed.
    if (!WebCore::MacApplication::isSafari())
        return false;
#endif

#if ENABLE(GPU_PROCESS_BY_DEFAULT)
    bool defaultValue = true;
#else
    bool defaultValue = false;
#endif

#if PLATFORM(MAC)
    return isFeatureFlagEnabled("gpu_process_webrtc", defaultValue);
#elif PLATFORM(IOS_FAMILY)
    return isFeatureFlagEnabled("gpu_process_media", defaultValue);
#else
    return defaultValue;
#endif
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
#if ENABLE(GPU_PROCESS_BY_DEFAULT)
    bool defaultValue = true;
#else
    bool defaultValue = false;
#endif

    return isFeatureFlagEnabled("gpu_process_webrtc", defaultValue);
}

#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(WEB_RTC)

bool defaultWebRTCCodecsInGPUProcess()
{
#if ENABLE(GPU_PROCESS_BY_DEFAULT)
    bool defaultValue = true;
#else
    bool defaultValue = false;
#endif

    return isFeatureFlagEnabled("gpu_process_webrtc", defaultValue);
}

#endif // ENABLE(WEB_RTC)

#if HAVE(INCREMENTAL_PDF_APIS)
bool defaultIncrementalPDFEnabled()
{
#if PLATFORM(MAC)
    bool defaultValue = true;
#else
    bool defaultValue = false;
#endif

    return isFeatureFlagEnabled("incremental_pdf", defaultValue);
}
#endif

#if ENABLE(WEBXR)

bool defaultWebXREnabled()
{
#if HAVE(WEBXR_INTERNALS)
    return true;
#else
    return false;
#endif
}

#endif // ENABLE(WEBXR)

#if ENABLE(WEBM_FORMAT_READER)

bool defaultWebMFormatReaderEnabled()
{
#if PLATFORM(MAC)
    bool defaultValue = true;
#else
    bool defaultValue = false;
#endif

    return isFeatureFlagEnabled("webm_format_reader", defaultValue);
}

#endif // ENABLE(WEBM_FORMAT_READER)

#if ENABLE(VP9)

bool defaultVP8DecoderEnabled()
{
    return isFeatureFlagEnabled("vp8_decoder", true);
}

bool defaultVP9DecoderEnabled()
{
    return isFeatureFlagEnabled("vp9_decoder", true);
}

bool defaultVP9SWDecoderEnabledOnBattery()
{
    return isFeatureFlagEnabled("sw_vp9_decoder_on_battery", false);
}
#endif // ENABLE(VP9)

#if ENABLE(MEDIA_SOURCE)

bool defaultWebMParserEnabled()
{
    return isFeatureFlagEnabled("webm_parser", true);
}

bool defaultWebMWebAudioEnabled()
{
    return isFeatureFlagEnabled("webm_webaudio", false);
}

#endif // ENABLE(MEDIA_SOURCE)

#if ENABLE(MEDIA_SESSION_COORDINATOR)
bool defaultMediaSessionCoordinatorEnabled()
{
    static dispatch_once_t onceToken;
    static bool enabled { false };
    dispatch_once(&onceToken, ^{
        enabled = WTF::processHasEntitlement("com.apple.developer.group-session.urlactivity");
    });
    return enabled;
}
#endif

} // namespace WebKit
