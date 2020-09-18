/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "LibWebRTCProviderCocoa.h"

#if USE(LIBWEBRTC)

#include "RuntimeEnabledFeatures.h"

ALLOW_UNUSED_PARAMETERS_BEGIN
#include <webrtc/sdk/WebKit/WebKitUtilities.h>
ALLOW_UNUSED_PARAMETERS_END
#include <webrtc/sdk/WebKit/WebKitVP9Decoder.h>
#include <wtf/MainThread.h>
#include <wtf/darwin/WeakLinking.h>

WTF_WEAK_LINK_FORCE_IMPORT(webrtc::setApplicationStatus);

namespace WebCore {

UniqueRef<LibWebRTCProvider> LibWebRTCProvider::create()
{
    return makeUniqueRef<LibWebRTCProviderCocoa>();
}

void LibWebRTCProvider::setH264HardwareEncoderAllowed(bool allowed)
{
    webrtc::setH264HardwareEncoderAllowed(allowed);
}

LibWebRTCProviderCocoa::~LibWebRTCProviderCocoa()
{
}

std::unique_ptr<webrtc::VideoDecoderFactory> LibWebRTCProviderCocoa::createDecoderFactory()
{
    ASSERT(isMainThread());

    return webrtc::createWebKitDecoderFactory(isSupportingH265() ? webrtc::WebKitH265::On : webrtc::WebKitH265::Off, isSupportingVP9() ? webrtc::WebKitVP9::On : webrtc::WebKitVP9::Off);
}

std::unique_ptr<webrtc::VideoEncoderFactory> LibWebRTCProviderCocoa::createEncoderFactory()
{
    ASSERT(isMainThread());

    webrtc::setH264LowLatencyEncoderEnabled(RuntimeEnabledFeatures::sharedFeatures().webRTCH264LowLatencyEncoderEnabled());
    return webrtc::createWebKitEncoderFactory(isSupportingH265() ? webrtc::WebKitH265::On : webrtc::WebKitH265::Off, isSupportingVP9() ? webrtc::WebKitVP9::On : webrtc::WebKitVP9::Off);
}

void LibWebRTCProviderCocoa::setActive(bool value)
{
    webrtc::setApplicationStatus(value);
}

bool LibWebRTCProvider::webRTCAvailable()
{
#if PLATFORM(IOS)
    return true;
#else
    return !!webrtc::setApplicationStatus;
#endif
}

void LibWebRTCProvider::registerWebKitVP9Decoder()
{
    if (webRTCAvailable())
        webrtc::registerWebKitVP9Decoder();
}
} // namespace WebCore

#endif // USE(LIBWEBRTC)
