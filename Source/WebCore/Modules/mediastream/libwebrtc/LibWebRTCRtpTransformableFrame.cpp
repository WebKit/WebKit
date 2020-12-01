/*
 * Copyright (C) 2020 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LibWebRTCRtpTransformableFrame.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

ALLOW_UNUSED_PARAMETERS_BEGIN
ALLOW_DEPRECATED_DECLARATIONS_BEGIN

#include <webrtc/api/frame_transformer_interface.h>

ALLOW_DEPRECATED_DECLARATIONS_END
ALLOW_UNUSED_PARAMETERS_END

namespace WebCore {

LibWebRTCRtpTransformableFrame::LibWebRTCRtpTransformableFrame(std::unique_ptr<webrtc::TransformableFrameInterface>&& frame)
    : m_rtcFrame(WTFMove(frame))
{
}

LibWebRTCRtpTransformableFrame::~LibWebRTCRtpTransformableFrame()
{
}

std::unique_ptr<webrtc::TransformableFrameInterface> LibWebRTCRtpTransformableFrame::takeRTCFrame()
{
    return WTFMove(m_rtcFrame);
}

RTCRtpTransformableFrame::Data LibWebRTCRtpTransformableFrame::data() const
{
    if (!m_rtcFrame)
        return { nullptr, 0 };
    auto data = m_rtcFrame->GetData();
    return { data.begin(), data.size() };
}

void LibWebRTCRtpTransformableFrame::setData(Data data)
{
    if (m_rtcFrame)
        m_rtcFrame->SetData({ data.data, data.size });
}

bool LibWebRTCRtpTransformableFrame::isKeyFrame() const
{
    ASSERT(m_rtcFrame);
    auto* videoFrame = static_cast<webrtc::TransformableVideoFrameInterface*>(m_rtcFrame.get());
    return videoFrame && videoFrame->IsKeyFrame();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
