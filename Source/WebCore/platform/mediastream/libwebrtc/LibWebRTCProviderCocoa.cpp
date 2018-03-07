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
#include <webrtc/media/engine/webrtcvideodecoderfactory.h>
#include <webrtc/media/engine/webrtcvideoencoderfactory.h>
#include <wtf/darwin/WeakLinking.h>
#endif

namespace WebCore {

UniqueRef<LibWebRTCProvider> LibWebRTCProvider::create()
{
#if USE(LIBWEBRTC) && PLATFORM(COCOA)
    return makeUniqueRef<LibWebRTCProviderCocoa>();
#else
    return makeUniqueRef<LibWebRTCProvider>();
#endif
}

#if USE(LIBWEBRTC)

LibWebRTCProviderCocoa::~LibWebRTCProviderCocoa()
{
    if (m_encoderFactory)
        m_encoderFactory->ClearDestructorObserver();
    if (m_decoderFactory)
        m_decoderFactory->ClearDestructorObserver();
}

void LibWebRTCProviderCocoa::setH264HardwareEncoderAllowed(bool allowed)
{
    m_h264HardwareEncoderAllowed = allowed;
#if PLATFORM(MAC)
    if (m_encoderFactory)
        m_encoderFactory->setH264HardwareEncoderAllowed(allowed);
#endif
}

std::unique_ptr<webrtc::VideoDecoderFactory> LibWebRTCProviderCocoa::createDecoderFactory()
{
    ASSERT(!m_decoderFactory);
    auto decoderFactory = std::make_unique<webrtc::VideoToolboxVideoDecoderFactory>(this);
    m_decoderFactory = decoderFactory.get();

    return WTFMove(decoderFactory);
}

std::unique_ptr<webrtc::VideoEncoderFactory> LibWebRTCProviderCocoa::createEncoderFactory()
{
    ASSERT(!m_encoderFactory);
    auto encoderFactory = std::make_unique<webrtc::VideoToolboxVideoEncoderFactory>(this);
    m_encoderFactory = encoderFactory.get();

#if PLATFORM(MAC)
    m_encoderFactory->setH264HardwareEncoderAllowed(m_h264HardwareEncoderAllowed);
#endif

    return WTFMove(encoderFactory);
}

void LibWebRTCProviderCocoa::setActive(bool value)
{
    if (m_decoderFactory)
        m_decoderFactory->SetActive(value);
    if (m_encoderFactory)
        m_encoderFactory->SetActive(value);
}

#endif // USE(LIBWEBRTC)

bool LibWebRTCProvider::webRTCAvailable()
{
#if USE(LIBWEBRTC)
    return !isNullFunctionPointer(rtc::LogMessage::LogToDebug);
#else
    return true;
#endif
}

} // namespace WebCore
