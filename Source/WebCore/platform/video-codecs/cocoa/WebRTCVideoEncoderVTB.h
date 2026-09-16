/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if USE(AVFOUNDATION)

#include "VideoEncoderVTB.h"
#include <WebCore/VideoEncoderScalabilityMode.h>
#include <WebCore/WebRTCVideoEncoder.h>
#include <memory>
#include <wtf/BlockPtr.h>

namespace WebCore {

class WebRTCVideoEncoderBitrateAdjuster;

class WebRTCVideoEncoderVTB : public WebRTCVideoEncoder {
public:
    ~WebRTCVideoEncoderVTB();

protected:
    WebRTCVideoEncoderVTB(bool useAnnexB, VideoEncoderScalabilityMode, WebRTCVideoEncoderCallback&&, WebRTCVideoEncoderDescriptionCallback&&, WebRTCVideoEncoderErrorCallback&&);

    uint16_t width() const { return m_width; }
    uint16_t height() const { return m_height; }
    bool useAnnexB() const { return m_useAnnexB; }
    bool needsToSendDescription() const { return m_needsToSendDescription; }
    void setNeedsToSendDescription(bool value) { m_needsToSendDescription = value; }

    void notifyEncodedFrame(std::span<const uint8_t>, const WebRTCVideoEncoderFrameInfo&);
    void notifyDescription(std::span<const uint8_t>);
    void notifyError(bool isFrameDropped);

    virtual CMVideoCodecType codecType() const = 0;
    // Converts the raw VTCompressionSession output into wire bytes and reports it via notifyEncodedFrame()/notifyDescription().
    // Returns false on failure, in which case the base class reports an encode error.
    virtual bool convertAndNotify(RetainPtr<CMSampleBufferRef>&&, WebRTCVideoEncoderFrameInfo&&) = 0;
    // Called once the compression session is (re)created, after the base class has set its own properties.
    // Overridden by leaf classes needing codec-specific session properties (e.g. H264's profile/level).
    virtual void configureAdditionalProperties() { }
    void setProperty(CFStringRef, CFTypeRef);

private:
    void setLowLatency(bool) final;
    void initialize(uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate) final;
    void encodeFrame(CVPixelBufferRef, int64_t timeStampNs, int64_t timeStamp, std::optional<uint64_t> duration, VideoFrame::Rotation, bool isKeyframeRequired) final;
    void setRates(uint32_t bitRate, uint32_t frameRate) final;
    void flush() final;

    bool resetCompressionSession();
    void configureCompressionSession();
    void setEncoderBitrateBps(uint32_t);

    WebRTCVideoEncoderCallback m_callback;
    WebRTCVideoEncoderDescriptionCallback m_descriptionCallback;
    WebRTCVideoEncoderErrorCallback m_errorCallback;
    RefPtr<VideoEncoderVTB> m_encoder;
    uint16_t m_width { 0 };
    uint16_t m_height { 0 };
    unsigned m_targetBitrateBps { 0 };
    unsigned m_encoderBitrateBps { 0 };
    bool m_useAnnexB { true };
    bool m_isLowLatencyEnabled { true };
    bool m_needsToSendDescription { false };
    const std::unique_ptr<WebRTCVideoEncoderBitrateAdjuster> m_bitrateAdjuster;
    VideoEncoderScalabilityMode m_scalabilityMode { VideoEncoderScalabilityMode::L1T1 };
};

}

#endif
