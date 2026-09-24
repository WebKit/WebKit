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
#include <CoreMedia/CMFormatDescription.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class GPUVideoEncoderBitrateAdjuster;
class VideoEncoderVTBSession;

class GPUVideoEncoderVTB : public GPUVideoEncoder {
public:
    ~GPUVideoEncoderVTB();

protected:
    GPUVideoEncoderVTB(CreationInfo&&, GPUVideoEncoderCallback&&, GPUVideoEncoderDescriptionCallback&&, GPUVideoEncoderErrorCallback&&);

    bool useAnnexB() const { return m_creationInfo.useAnnexB; }
    bool needsToSendDescription() const { return m_needsToSendDescription; }
    void setNeedsToSendDescription(bool value) { m_needsToSendDescription = value; }

    void notifyEncodedFrame(std::span<const uint8_t>, const GPUVideoEncoderFrameInfo&);
    void notifyDescriptionIfNeeded(CMSampleBufferRef, CFStringRef boxName, const PlatformVideoColorSpace&);
    void notifyError();
    void notifyFrameDropped();

    virtual bool convertAndNotify(RetainPtr<CMSampleBufferRef>&&, GPUVideoEncoderFrameInfo&&, const PlatformVideoColorSpace&) = 0;
    virtual void configureAdditionalProperties() { }
    void setProperty(CFStringRef, CFTypeRef);

    WorkQueue& queue() { return m_creationInfo.queue; }

    static std::optional<Vector<uint8_t>> toVector(CMSampleBufferRef);

private:
    void initialize(uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate) final;
    void encodeFrame(CVPixelBufferRef, int64_t timeStampNs, int64_t timeStamp, std::optional<uint64_t> duration, VideoFrame::Rotation, bool isKeyframeRequired) final;
    void setRates(uint32_t bitRate, uint32_t frameRate) final;
    void flush() final;

    bool resetCompressionSession();
    void configureCompressionSession();
    void setEncoderBitrateBps(uint32_t);
    CMVideoCodecType codecType() const;
    void notifyDescription(std::span<const uint8_t>, const PlatformVideoColorSpace&);

    const CreationInfo m_creationInfo;
    const GPUVideoEncoderCallback m_callback;
    const GPUVideoEncoderDescriptionCallback m_descriptionCallback;
    const GPUVideoEncoderErrorCallback m_errorCallback;
    const UniqueRef<GPUVideoEncoderBitrateAdjuster> m_bitrateAdjuster;

    std::atomic<bool> m_needsToSendDescription { true };

    RefPtr<VideoEncoderVTBSession> m_encoder WTF_GUARDED_BY_CAPABILITY(queue());
    uint16_t m_width WTF_GUARDED_BY_CAPABILITY(queue()) { 0 };
    uint16_t m_height WTF_GUARDED_BY_CAPABILITY(queue()) { 0 };
    unsigned m_targetBitrateBps WTF_GUARDED_BY_CAPABILITY(queue()) { 0 };
    PlatformVideoColorSpace m_colorSpace WTF_GUARDED_BY_CAPABILITY(queue());
};

}

#endif
