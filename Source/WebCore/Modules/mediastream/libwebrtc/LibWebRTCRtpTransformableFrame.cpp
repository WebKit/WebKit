/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "LibWebRTCUtils.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN

#include <webrtc/api/frame_transformer_factory.h>
#include <webrtc/api/frame_transformer_interface.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LibWebRTCRtpTransformableFrame);

LibWebRTCRtpTransformableFrame::LibWebRTCRtpTransformableFrame(std::unique_ptr<webrtc::TransformableFrameInterface>&& frame, bool isAudio)
    : m_rtcFrame(WTFMove(frame))
    , m_isAudio(isAudio)
{
}

LibWebRTCRtpTransformableFrame::~LibWebRTCRtpTransformableFrame()
{
}

std::unique_ptr<webrtc::TransformableFrameInterface> LibWebRTCRtpTransformableFrame::takeRTCFrame()
{
    return WTFMove(m_rtcFrame);
}

std::span<const uint8_t> LibWebRTCRtpTransformableFrame::data() const
{
    if (!m_rtcFrame)
        return { };
    auto data = m_rtcFrame->GetData();
    return unsafeMakeSpan(data.begin(), data.size());
}

void LibWebRTCRtpTransformableFrame::setData(std::span<const uint8_t> data)
{
    if (m_rtcFrame)
        m_rtcFrame->SetData({ data.data(), data.size() });
}

bool LibWebRTCRtpTransformableFrame::isKeyFrame() const
{
    ASSERT(m_rtcFrame);
    auto* videoFrame = static_cast<webrtc::TransformableVideoFrameInterface*>(m_rtcFrame.get());
    return videoFrame && videoFrame->IsKeyFrame();
}

uint64_t LibWebRTCRtpTransformableFrame::timestamp() const
{
    return m_rtcFrame ? m_rtcFrame->GetTimestamp() : 0;
}

RTCEncodedAudioFrameMetadata LibWebRTCRtpTransformableFrame::audioMetadata() const
{
    if (!m_rtcFrame)
        return { };

    Vector<uint32_t> cssrcs;
    std::optional<uint16_t> sequenceNumber;
    if (m_rtcFrame->GetDirection() == webrtc::TransformableFrameInterface::Direction::kReceiver) {
        auto* audioFrame = static_cast<webrtc::TransformableAudioFrameInterface*>(m_rtcFrame.get());
        auto contributingSources = audioFrame->GetContributingSources();
        cssrcs = Vector<uint32_t>(contributingSources.size(), [&](size_t cptr) {
            return contributingSources[cptr];
        });
        sequenceNumber = audioFrame->SequenceNumber();
    }
    return { m_rtcFrame->GetSsrc(), m_rtcFrame->GetPayloadType(), WTFMove(cssrcs), sequenceNumber, m_rtcFrame->GetTimestamp(), fromStdString(m_rtcFrame->GetMimeType()) };
}

RTCEncodedVideoFrameMetadata LibWebRTCRtpTransformableFrame::videoMetadata() const
{
    if (!m_rtcFrame)
        return { };
    auto* videoFrame = static_cast<webrtc::TransformableVideoFrameInterface*>(m_rtcFrame.get());
    auto metadata = videoFrame->Metadata();

    std::optional<int64_t> frameId;
    if (metadata.GetFrameId())
        frameId = *metadata.GetFrameId();

    Vector<int64_t> dependencies;
    for (auto value : metadata.GetFrameDependencies())
        dependencies.append(value);

    Vector<uint32_t> cssrcs;
    if (m_rtcFrame->GetDirection() == webrtc::TransformableFrameInterface::Direction::kReceiver) {
        auto rtcCssrcs = metadata.GetCsrcs();
        cssrcs = Vector<uint32_t>(rtcCssrcs.size(), [&](size_t cptr) {
            return rtcCssrcs[cptr];
        });
    }

    std::optional<int64_t> timestamp;
    if (auto presentationTimestamp = m_rtcFrame->GetPresentationTimestamp())
        timestamp = presentationTimestamp->us();
    return { frameId, WTFMove(dependencies), metadata.GetWidth(), metadata.GetHeight(), metadata.GetSpatialIndex(), metadata.GetTemporalIndex(), m_rtcFrame->GetSsrc(), m_rtcFrame->GetPayloadType(), WTFMove(cssrcs), timestamp, m_rtcFrame->GetTimestamp(), fromStdString(m_rtcFrame->GetMimeType()) };
}

Ref<RTCRtpTransformableFrame> LibWebRTCRtpTransformableFrame::clone()
{
    std::unique_ptr<webrtc::TransformableFrameInterface> rtcClone;
    if (m_isAudio)
        rtcClone = webrtc::CloneAudioFrame(static_cast<webrtc::TransformableAudioFrameInterface*>(m_rtcFrame.get()));
    else
        rtcClone = webrtc::CloneVideoFrame(static_cast<webrtc::TransformableVideoFrameInterface*>(m_rtcFrame.get()));
    return adoptRef(*new LibWebRTCRtpTransformableFrame(WTFMove(rtcClone), m_isAudio));
}

void LibWebRTCRtpTransformableFrame::setOptions(const RTCEncodedAudioFrameMetadata& metadata)
{
    ASSERT(m_isAudio);
    // FIXME: Support more metadata.
    if (metadata.rtpTimestamp)
        m_rtcFrame->SetRTPTimestamp(*metadata.rtpTimestamp);
}

void LibWebRTCRtpTransformableFrame::setOptions(const RTCEncodedVideoFrameMetadata& newMetadata)
{
    ASSERT(!m_isAudio);
    auto rtcMetadata = static_cast<webrtc::TransformableVideoFrameInterface*>(m_rtcFrame.get())->Metadata();

    if (newMetadata.frameId)
        rtcMetadata.SetFrameId(*newMetadata.frameId);
    if (newMetadata.dependencies)
        rtcMetadata.SetFrameDependencies({ newMetadata.dependencies->span().data(), newMetadata.dependencies->size() });
    if (newMetadata.width)
        rtcMetadata.SetWidth(*newMetadata.width);
    if (newMetadata.height)
        rtcMetadata.SetHeight(*newMetadata.height);
    if (newMetadata.spatialIndex)
        rtcMetadata.SetSpatialIndex(*newMetadata.spatialIndex);
    if (newMetadata.temporalIndex)
        rtcMetadata.SetTemporalIndex(*newMetadata.temporalIndex);
    if (newMetadata.synchronizationSource)
        rtcMetadata.SetSsrc(*newMetadata.synchronizationSource);
    // FIXME: newMetadata.payloadType
    if (newMetadata.contributingSources) {
        std::vector<uint32_t> csrcs(newMetadata.contributingSources->size());
        for (auto& csrc : *newMetadata.contributingSources)
            csrcs.push_back(csrc);
        rtcMetadata.SetCsrcs(WTFMove(csrcs));
    }
    if (newMetadata.rtpTimestamp)
        m_rtcFrame->SetRTPTimestamp(*newMetadata.rtpTimestamp);
    // FIXME: newMetadata.mimeType

    static_cast<webrtc::TransformableVideoFrameInterface*>(m_rtcFrame.get())->SetMetadata(WTFMove(rtcMetadata));
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
