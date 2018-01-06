/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RealtimeIncomingVideoSourceCocoa.h"

#if USE(LIBWEBRTC)

#include "Logging.h"
#include "MediaSampleAVFObjC.h"
#include <webrtc/sdk/objc/Framework/Classes/Video/corevideo_frame_buffer.h>
#include <wtf/cf/TypeCastsCF.h>

#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

namespace WebCore {
using namespace PAL;

Ref<RealtimeIncomingVideoSource> RealtimeIncomingVideoSource::create(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& trackId)
{
    auto source = RealtimeIncomingVideoSourceCocoa::create(WTFMove(videoTrack), WTFMove(trackId));
    source->start();
    return WTFMove(source);
}

Ref<RealtimeIncomingVideoSourceCocoa> RealtimeIncomingVideoSourceCocoa::create(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& trackId)
{
    return adoptRef(*new RealtimeIncomingVideoSourceCocoa(WTFMove(videoTrack), WTFMove(trackId)));
}

RealtimeIncomingVideoSourceCocoa::RealtimeIncomingVideoSourceCocoa(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& videoTrackId)
    : RealtimeIncomingVideoSource(WTFMove(videoTrack), WTFMove(videoTrackId))
{
}

CVPixelBufferRef RealtimeIncomingVideoSourceCocoa::pixelBufferFromVideoFrame(const webrtc::VideoFrame& frame)
{
    if (muted()) {
        if (!m_blackFrame || m_blackFrameWidth != frame.width() || m_blackFrameHeight != frame.height()) {
            CVPixelBufferRef pixelBuffer = nullptr;
            auto status = CVPixelBufferCreate(kCFAllocatorDefault, frame.width(), frame.height(), kCVPixelFormatType_420YpCbCr8Planar, nullptr, &pixelBuffer);
            ASSERT_UNUSED(status, status == noErr);

            m_blackFrame = adoptCF(pixelBuffer);
            m_blackFrameWidth = frame.width();
            m_blackFrameHeight = frame.height();

            status = CVPixelBufferLockBaseAddress(pixelBuffer, 0);
            ASSERT(status == noErr);
            void* data = CVPixelBufferGetBaseAddress(pixelBuffer);
            size_t yLength = frame.width() * frame.height();
            memset(data, 0, yLength);
            memset(static_cast<uint8_t*>(data) + yLength, 128, yLength / 2);

            status = CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
            ASSERT(!status);
        }
        return m_blackFrame.get();
    }
    auto buffer = frame.video_frame_buffer();
    ASSERT(buffer->type() == webrtc::VideoFrameBuffer::Type::kNative);
    return static_cast<webrtc::CoreVideoFrameBuffer&>(*buffer).pixel_buffer();
}

void RealtimeIncomingVideoSourceCocoa::OnFrame(const webrtc::VideoFrame& frame)
{
    if (!isProducingData())
        return;

#if !RELEASE_LOG_DISABLED
    if (!(++m_numberOfFrames % 30))
        RELEASE_LOG(MediaStream, "RealtimeIncomingVideoSourceCocoa::OnFrame %zu frame", m_numberOfFrames);
#endif

    auto pixelBuffer = pixelBufferFromVideoFrame(frame);

    // FIXME: Convert timing information from VideoFrame to CMSampleTimingInfo.
    // For the moment, we will pretend that frames should be rendered asap.
    CMSampleTimingInfo timingInfo;
    timingInfo.presentationTimeStamp = kCMTimeInvalid;
    timingInfo.decodeTimeStamp = kCMTimeInvalid;
    timingInfo.duration = kCMTimeInvalid;

    CMVideoFormatDescriptionRef formatDescription;
    OSStatus ostatus = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)pixelBuffer, &formatDescription);
    if (ostatus != noErr) {
        LOG_ERROR("Failed to initialize CMVideoFormatDescription: %d", static_cast<int>(ostatus));
        return;
    }

    CMSampleBufferRef sampleBuffer;
    ostatus = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)pixelBuffer, formatDescription, &timingInfo, &sampleBuffer);
    CFRelease(formatDescription);
    if (ostatus != noErr) {
        LOG_ERROR("Failed to create the sample buffer: %d", static_cast<int>(ostatus));
        return;
    }

    auto sample = adoptCF(sampleBuffer);

    CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, true);
    for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray); ++i) {
        CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
        CFDictionarySetValue(attachments, kCMSampleAttachmentKey_DisplayImmediately, kCFBooleanTrue);
    }

    unsigned width = frame.width();
    unsigned height = frame.height();

    MediaSample::VideoRotation rotation;
    switch (frame.rotation()) {
    case webrtc::kVideoRotation_0:
        rotation = MediaSample::VideoRotation::None;
        break;
    case webrtc::kVideoRotation_180:
        rotation = MediaSample::VideoRotation::UpsideDown;
        break;
    case webrtc::kVideoRotation_90:
        rotation = MediaSample::VideoRotation::Right;
        std::swap(width, height);
        break;
    case webrtc::kVideoRotation_270:
        rotation = MediaSample::VideoRotation::Left;
        std::swap(width, height);
        break;
    }

    RefPtr<RealtimeIncomingVideoSourceCocoa> protectedThis(this);
    callOnMainThread([protectedThis = makeRef(*this), sample = WTFMove(sample), width, height, rotation] {
        protectedThis->processNewSample(sample.get(), width, height, rotation);
    });
}

void RealtimeIncomingVideoSourceCocoa::processNewSample(CMSampleBufferRef sample, unsigned width, unsigned height, MediaSample::VideoRotation rotation)
{
    m_buffer = sample;
    if (width != m_currentSettings.width() || height != m_currentSettings.height()) {
        m_currentSettings.setWidth(width);
        m_currentSettings.setHeight(height);
        settingsDidChange();
    }

    videoSampleAvailable(MediaSampleAVFObjC::create(sample, rotation));
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
