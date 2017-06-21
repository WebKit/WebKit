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
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

// FIXME: Do clean-up in the includes

#include "config.h"
#include "RealtimeIncomingVideoSource.h"

#if USE(LIBWEBRTC)

#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "Logging.h"
#include "MediaSampleAVFObjC.h"
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <webrtc/sdk/objc/Framework/Classes/Video/corevideo_frame_buffer.h>
#include <wtf/MainThread.h>

#include "CoreMediaSoftLink.h"
#include "CoreVideoSoftLink.h"

namespace WebCore {

Ref<RealtimeIncomingVideoSource> RealtimeIncomingVideoSource::create(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& trackId)
{
    const OSType imageFormat = kCVPixelFormatType_32BGRA;
    RetainPtr<CFNumberRef> imageFormatNumber = adoptCF(CFNumberCreate(nullptr,  kCFNumberIntType,  &imageFormat));

    RetainPtr<CFMutableDictionaryRef> conformerOptions = adoptCF(CFDictionaryCreateMutable(0, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(conformerOptions.get(), kCVPixelBufferPixelFormatTypeKey, imageFormatNumber.get());
    PixelBufferConformerCV conformer(conformerOptions.get());

    auto source = adoptRef(*new RealtimeIncomingVideoSource(WTFMove(videoTrack), WTFMove(trackId), conformerOptions.get()));
    source->start();
    return source;
}

RealtimeIncomingVideoSource::RealtimeIncomingVideoSource(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& videoTrackId, CFMutableDictionaryRef conformerOptions)
    : RealtimeMediaSource(WTFMove(videoTrackId), RealtimeMediaSource::Type::Video, String())
    , m_videoTrack(WTFMove(videoTrack))
    , m_conformer(conformerOptions)
{
    m_currentSettings.setWidth(640);
    m_currentSettings.setHeight(480);
    notifyMutedChange(!m_videoTrack);
}

void RealtimeIncomingVideoSource::startProducingData()
{
    if (m_videoTrack)
        m_videoTrack->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

void RealtimeIncomingVideoSource::setSourceTrack(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& track)
{
    ASSERT(!m_videoTrack);
    ASSERT(track);

    m_videoTrack = WTFMove(track);
    notifyMutedChange(!m_videoTrack);
    if (isProducingData())
        m_videoTrack->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

void RealtimeIncomingVideoSource::stopProducingData()
{
    if (m_videoTrack)
        m_videoTrack->RemoveSink(this);
}

CVPixelBufferRef RealtimeIncomingVideoSource::pixelBufferFromVideoFrame(const webrtc::VideoFrame& frame)
{
    if (muted()) {
        if (!m_blackFrame || m_blackFrameWidth != frame.width() || m_blackFrameHeight != frame.height()) {
            CVPixelBufferRef pixelBuffer = nullptr;
            auto status = CVPixelBufferCreate(kCFAllocatorDefault, frame.width(), frame.height(), kCVPixelFormatType_420YpCbCr8Planar, nullptr, &pixelBuffer);
            ASSERT_UNUSED(status, status == noErr);

            m_blackFrame = pixelBuffer;
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

void RealtimeIncomingVideoSource::OnFrame(const webrtc::VideoFrame& frame)
{
    if (!isProducingData())
        return;

#if !RELEASE_LOG_DISABLED
    if (!(++m_numberOfFrames % 30))
        RELEASE_LOG(MediaStream, "RealtimeIncomingVideoSource::OnFrame %zu frame", m_numberOfFrames);
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
    if (ostatus != noErr) {
        LOG_ERROR("Failed to create the sample buffer: %d", static_cast<int>(ostatus));
        return;
    }
    CFRelease(formatDescription);

    auto sample = adoptCF(sampleBuffer);

    CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, true);
    for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray); ++i) {
        CFMutableDictionaryRef attachments = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(attachmentsArray, i);
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

    RefPtr<RealtimeIncomingVideoSource> protectedThis(this);
    callOnMainThread([protectedThis = WTFMove(protectedThis), sample = WTFMove(sample), width, height, rotation] {
        protectedThis->processNewSample(sample.get(), width, height, rotation);
    });
}

void RealtimeIncomingVideoSource::processNewSample(CMSampleBufferRef sample, unsigned width, unsigned height, MediaSample::VideoRotation rotation)
{
    m_buffer = sample;
    if (width != m_currentSettings.width() || height != m_currentSettings.height()) {
        m_currentSettings.setWidth(width);
        m_currentSettings.setHeight(height);
        settingsDidChange();
    }

    videoSampleAvailable(MediaSampleAVFObjC::create(sample, rotation));
}

const RealtimeMediaSourceCapabilities& RealtimeIncomingVideoSource::capabilities() const
{
    return RealtimeMediaSourceCapabilities::emptyCapabilities();
}

const RealtimeMediaSourceSettings& RealtimeIncomingVideoSource::settings() const
{
    return m_currentSettings;
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
