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
#include "MediaSampleAVFObjC.h"
#include <webrtc/common_video/include/corevideo_frame_buffer.h>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
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
    source->startProducingData();
    return source;
}

RealtimeIncomingVideoSource::RealtimeIncomingVideoSource(rtc::scoped_refptr<webrtc::VideoTrackInterface>&& videoTrack, String&& videoTrackId, CFMutableDictionaryRef conformerOptions)
    : RealtimeMediaSource(WTFMove(videoTrackId), RealtimeMediaSource::Type::Video, String())
    , m_videoTrack(WTFMove(videoTrack))
    , m_conformer(conformerOptions)
{
    m_muted = !m_videoTrack;
    m_currentSettings.setWidth(640);
    m_currentSettings.setHeight(480);
}

void RealtimeIncomingVideoSource::startProducingData()
{
    if (m_isProducingData)
        return;

    m_isProducingData = true;
    if (m_videoTrack)
        m_videoTrack->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

void RealtimeIncomingVideoSource::stopProducingData()
{
    if (!m_isProducingData)
        return;

    m_isProducingData = false;
    if (m_videoTrack)
        m_videoTrack->RemoveSink(this);
}

void RealtimeIncomingVideoSource::OnFrame(const webrtc::VideoFrame& frame)
{
    if (!m_isProducingData)
        return;

    auto buffer = frame.video_frame_buffer();
    CVPixelBufferRef pixelBuffer = static_cast<CVPixelBufferRef>(buffer->native_handle());

    // FIXME: Convert timing information from VideoFrame to CMSampleTimingInfo.
    // For the moment, we will pretend that frames should be rendered asap.
    CMSampleTimingInfo timingInfo;
    timingInfo.presentationTimeStamp = kCMTimeInvalid;
    timingInfo.decodeTimeStamp = kCMTimeInvalid;
    timingInfo.duration = kCMTimeInvalid;

    CMVideoFormatDescriptionRef formatDescription;
    OSStatus ostatus = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)pixelBuffer, &formatDescription);
    if (ostatus != noErr) {
        LOG_ERROR("Failed to initialize CMVideoFormatDescription: %d", ostatus);
        return;
    }

    CMSampleBufferRef sampleBuffer;
    ostatus = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)pixelBuffer, formatDescription, &timingInfo, &sampleBuffer);
    if (ostatus != noErr) {
        LOG_ERROR("Failed to create the sample buffer: %d", ostatus);
        return;
    }
    CFRelease(formatDescription);

    RetainPtr<CMSampleBufferRef> sample = sampleBuffer;

    CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, true);
    for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray); ++i) {
        CFMutableDictionaryRef attachments = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(attachmentsArray, i);
        CFDictionarySetValue(attachments, kCMSampleAttachmentKey_DisplayImmediately, kCFBooleanTrue);
    }

    unsigned width = frame.width();
    unsigned height = frame.height();
    RefPtr<RealtimeIncomingVideoSource> protectedThis(this);
    callOnMainThread([protectedThis = WTFMove(protectedThis), sample = WTFMove(sample), width, height] {
        protectedThis->processNewSample(sample.get(), width, height);
    });
}

void RealtimeIncomingVideoSource::processNewSample(CMSampleBufferRef sample, unsigned width, unsigned height)
{
    m_buffer = sample;
    if (width != m_currentSettings.width() || height != m_currentSettings.height()) {
        m_currentSettings.setWidth(width);
        m_currentSettings.setHeight(height);
        settingsDidChange();
    }

    videoSampleAvailable(MediaSampleAVFObjC::create(sample));
}

static inline void drawImage(ImageBuffer& imageBuffer, CGImageRef image, const FloatRect& rect)
{
    auto& context = imageBuffer.context();
    GraphicsContextStateSaver stateSaver(context);
    context.translate(rect.x(), rect.y() + rect.height());
    context.scale(FloatSize(1, -1));
    context.setImageInterpolationQuality(InterpolationLow);
    IntRect paintRect(IntPoint(0, 0), IntSize(rect.width(), rect.height()));
    CGContextDrawImage(context.platformContext(), CGRectMake(0, 0, paintRect.width(), paintRect.height()), image);
}

RefPtr<Image> RealtimeIncomingVideoSource::currentFrameImage()
{
    if (!m_buffer)
        return nullptr;

    FloatRect rect(0, 0, m_currentSettings.width(), m_currentSettings.height());
    auto imageBuffer = ImageBuffer::create(rect.size(), Unaccelerated);

    auto pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(m_buffer.get()));
    drawImage(*imageBuffer, m_conformer.createImageFromPixelBuffer(pixelBuffer).get(), rect);

    return ImageBuffer::sinkIntoImage(WTFMove(imageBuffer));
}

void RealtimeIncomingVideoSource::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled())
        return;

    if (!m_buffer)
        return;

    // FIXME: Can we optimize here the painting?
    FloatRect fullRect(0, 0, m_currentSettings.width(), m_currentSettings.height());
    auto imageBuffer = ImageBuffer::create(fullRect.size(), Unaccelerated);

    auto pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(m_buffer.get()));
    drawImage(*imageBuffer, m_conformer.createImageFromPixelBuffer(pixelBuffer).get(), fullRect);

    GraphicsContextStateSaver stateSaver(context);
    context.setImageInterpolationQuality(InterpolationLow);
    IntRect paintRect(IntPoint(0, 0), IntSize(rect.width(), rect.height()));
    context.drawImage(*imageBuffer->copyImage(DontCopyBackingStore), rect);
}

RefPtr<RealtimeMediaSourceCapabilities> RealtimeIncomingVideoSource::capabilities() const
{
    return m_capabilities;
}

const RealtimeMediaSourceSettings& RealtimeIncomingVideoSource::settings() const
{
    return m_currentSettings;
}

RealtimeMediaSourceSupportedConstraints& RealtimeIncomingVideoSource::supportedConstraints()
{
    return m_supportedConstraints;
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
