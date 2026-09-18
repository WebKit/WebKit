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

#include "GPUVideoEncoder.h"
#include "VideoEncoderVTBSession.h"
#include <memory>
#include <wtf/BlockPtr.h>

namespace WebCore {

class GPUVideoEncoderBitrateAdjuster;

class GPUVideoEncoderVTB : public GPUVideoEncoder {
public:
    ~GPUVideoEncoderVTB();

protected:
    GPUVideoEncoderVTB(bool useAnnexB, GPUVideoEncoderCallback&&, GPUVideoEncoderDescriptionCallback&&, GPUVideoEncoderErrorCallback&&);

    uint16_t width() const { return m_width; }
    uint16_t height() const { return m_height; }
    bool useAnnexB() const { return m_useAnnexB; }
    bool needsToSendDescription() const { return m_needsToSendDescription; }
    void setNeedsToSendDescription(bool value) { m_needsToSendDescription = value; }

    void notifyEncodedFrame(std::span<const uint8_t>, const GPUVideoEncoderFrameInfo&);
    void notifyDescription(std::span<const uint8_t>);
    void notifyError();
    void notifyFrameDropped();

    virtual CMVideoCodecType codecType() const = 0;
    virtual bool convertAndNotify(RetainPtr<CMSampleBufferRef>&&, GPUVideoEncoderFrameInfo&&) = 0;

private:
    void setLowLatency(bool) final;
    void initialize(uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate) final;
    void encodeFrame(CVPixelBufferRef, int64_t timeStampNs, int64_t timeStamp, std::optional<uint64_t> duration, VideoFrame::Rotation, bool isKeyframeRequired) final;
    void setRates(uint32_t bitRate, uint32_t frameRate) final;
    void flush() final;

    bool resetCompressionSession();
    void configureCompressionSession();
    void setEncoderBitrateBps(uint32_t);

    GPUVideoEncoderCallback m_callback;
    GPUVideoEncoderDescriptionCallback m_descriptionCallback;
    GPUVideoEncoderErrorCallback m_errorCallback;
    RefPtr<VideoEncoderVTBSession> m_encoder;
    uint16_t m_width { 0 };
    uint16_t m_height { 0 };
    unsigned m_targetBitrateBps { 0 };
    unsigned m_encoderBitrateBps { 0 };
    bool m_useAnnexB { true };
    bool m_isLowLatencyEnabled { true };
    bool m_needsToSendDescription { false };
    const UniqueRef<GPUVideoEncoderBitrateAdjuster> m_bitrateAdjuster;
};

}

#endif
