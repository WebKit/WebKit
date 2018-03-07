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

#include "VideoToolBoxEncoderFactory.h"
#include "encoder.h"

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/h264_profile_level_id.h"
#include "modules/video_coding/codecs/h264/include/h264.h"

namespace webrtc {

VideoToolboxVideoEncoderFactory::~VideoToolboxVideoEncoderFactory()
{
    for (H264VideoToolboxEncoder& encoder : m_encoders)
        encoder.ClearFactory();
    if (m_destructorObserver)
        m_destructorObserver->notifyOfEncoderFactoryDestruction(*this);
}

void VideoToolboxVideoEncoderFactory::Add(H264VideoToolboxEncoder& encoder)
{
    m_encoders.push_back(encoder);
}

void VideoToolboxVideoEncoderFactory::Remove(H264VideoToolboxEncoder& encoder)
{
    std::remove_if(m_encoders.begin(), m_encoders.end(), [&] (auto& item){
        return &encoder == &item.get();
    });
}

void VideoToolboxVideoEncoderFactory::SetActive(bool isActive)
{
    if (m_isActive == isActive)
        return;

    m_isActive = isActive;
    for (H264VideoToolboxEncoder& encoder : m_encoders)
        encoder.SetActive(isActive);
}

void VideoToolboxVideoEncoderFactory::setH264HardwareEncoderAllowed(bool allowed)
{
    m_h264HardwareEncoderAllowed = allowed;
}

static inline SdpVideoFormat CreateH264Format(H264::Profile profile, H264::Level level) {
    const rtc::Optional<std::string> profile_string =
    H264::ProfileLevelIdToString(H264::ProfileLevelId(profile, level));
    RTC_CHECK(profile_string);
    return SdpVideoFormat(cricket::kH264CodecName,
                          {{cricket::kH264FmtpProfileLevelId, *profile_string},
                              {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
                              {cricket::kH264FmtpPacketizationMode, "1"}});
}

std::vector<webrtc::SdpVideoFormat> VideoToolboxVideoEncoderFactory::GetSupportedFormats() const
{
    return {CreateH264Format(H264::kProfileBaseline, H264::kLevel3_1),
        CreateH264Format(H264::kProfileConstrainedBaseline, H264::kLevel3_1)};
}

auto VideoToolboxVideoEncoderFactory::QueryVideoEncoder(const webrtc::SdpVideoFormat&) const -> CodecInfo
{
    // We would like to use hardware accelerated or not, but we will only for sure when creating the compression session.
    return { true, false };
}

std::unique_ptr<webrtc::VideoEncoder> VideoToolboxVideoEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat& format)
{
    return std::make_unique<H264VideoToolboxEncoder>(format, *this);
}

}
