/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "GPUVideoDecoderVTB.h"

#if USE(LIBWEBRTC)

#import "MediaReorderQueue.h"
#import <WebCore/CMUtilities.h>
#import <wtf/ThreadSafeWeakPtr.h>
#import <wtf/cf/TypeCastsCF.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

static bool shouldUseFullRange(CMVideoFormatDescriptionRef format)
{
    RetainPtr fullRange = dynamic_cf_cast<CFBooleanRef>(PAL::CMFormatDescriptionGetExtension(format, PAL::kCMFormatDescriptionExtension_FullRangeVideo));
    return fullRange && CFBooleanGetValue(fullRange.get());
}

static int bitDepthFromFormat(CMVideoFormatDescriptionRef format)
{
    int bitDepth = 8;
    if (RetainPtr bitsPerComponent = dynamic_cf_cast<CFNumberRef>(PAL::CMFormatDescriptionGetExtension(format, PAL::kCMFormatDescriptionExtension_BitsPerComponent)))
        CFNumberGetValue(bitsPerComponent.get(), kCFNumberIntType, &bitDepth);
    return bitDepth;
}

static RetainPtr<CFDictionaryRef> createPixelBufferAttributes(CMVideoFormatDescriptionRef format)
{
    static size_t const attributesSize = 3;
    CFTypeRef keys[attributesSize] = {
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        WebCore::kCVPixelBufferOpenGLCompatibilityKey,
#elif PLATFORM(IOS_FAMILY)
        WebCore::kCVPixelBufferExtendedPixelsRightKey,
#endif
        WebCore::kCVPixelBufferIOSurfacePropertiesKey,
        WebCore::kCVPixelBufferPixelFormatTypeKey
    };

    RetainPtr ioSurfaceValue = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    bool isFullRange = shouldUseFullRange(format);
    int bitDepth = bitDepthFromFormat(format);
    ASSERT(bitDepth == 8 || bitDepth == 10);

    int64_t pixelFormatType;
    if (bitDepth > 8)
        pixelFormatType = isFullRange ? kCVPixelFormatType_420YpCbCr10BiPlanarFullRange : kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
    else
        pixelFormatType = isFullRange ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    RetainPtr pixelFormat = adoptCF(CFNumberCreate(nullptr, kCFNumberLongType, &pixelFormatType));
    CFTypeRef values[attributesSize] = { kCFBooleanTrue, ioSurfaceValue.get(), pixelFormat.get() };
    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, attributesSize, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

class GPUVideoDecoderVTBQueue : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GPUVideoDecoderVTBQueue> {
public:
    static Ref<GPUVideoDecoderVTBQueue> create(uint8_t reorderSize) { return adoptRef(*new GPUVideoDecoderVTBQueue(reorderSize)); }
    void setReorderSize(uint8_t);
    uint8_t reorderSize() const;

    struct Buffer {
        RetainPtr<CVPixelBufferRef> frame;
        int64_t timeStamp { 0 };
    };
    void add(Buffer&&, GPUVideoDecoderCallback);
    void flush(GPUVideoDecoderCallback);

private:
    explicit GPUVideoDecoderVTBQueue(uint8_t reorderSize)
        : m_queue(reorderSize)
    {
    }

    struct BufferComparator {
        bool operator()(const Buffer& a, const Buffer& b) const { return a.timeStamp <= b.timeStamp; }
    };

    mutable Lock m_lock;
    MediaReorderQueue<Buffer, BufferComparator> m_queue WTF_GUARDED_BY_LOCK(m_lock);
};

GPUVideoDecoderVTB::GPUVideoDecoderVTB(GPUVideoDecoderCallback callback, Ref<WorkQueue>&& queue, std::optional<PlatformVideoColorSpace>&& colorSpaceOverride)
    : GPUVideoDecoder(WTF::move(colorSpaceOverride))
    , m_workQueue(WTF::move(queue))
    , m_callback(makeBlockPtr(callback))
{
    assertIsCurrent(this->queue());
}

GPUVideoDecoderVTB::~GPUVideoDecoderVTB() = default;

static VideoDecoderVTBSession::CallbackMultiImage createMultiImageCallback(GPUVideoDecoderCallback callback, RefPtr<GPUVideoDecoderVTBQueue>&& queue, uint8_t reorderSize)
{
    return makeBlockPtr([callback = makeBlockPtr(callback), queue = WTF::move(queue), reorderSize](OSStatus, VTDecodeInfoFlags, CVImageBufferRef pixelBuffer, CMTaggedBufferGroupRef, CMTime presentationTime, CMTime) mutable {
        UNUSED_PARAM(reorderSize);
        if (!pixelBuffer) {
            callback(nil, 0, 0, false);
            return;
        }

        if (!queue) {
            callback((CVPixelBufferRef)pixelBuffer, presentationTime.value, 0, false);
            return;
        }

        if (reorderSize != queue->reorderSize()) {
            queue->flush(callback.get());
            queue->setReorderSize(reorderSize);
        }

        if (!reorderSize) {
            callback((CVPixelBufferRef)pixelBuffer, presentationTime.value, 0, false);
            return;
        }
        queue->add({ (CVPixelBufferRef)pixelBuffer, presentationTime.value }, callback.get());
    });
}

int32_t GPUVideoDecoderVTB::decodeFrameInternal(int64_t timeStamp, std::span<const uint8_t> data)
{
    assertIsCurrent(queue());

    if (!m_format)
        return 0;

    RetainPtr sample = sampleBufferFromVideoData(data, m_format.get());
    if (!sample)
        return -1;

    if (!m_decoder || !protect(m_decoder)->canAccept(m_format.get())) {
        if (RefPtr decoder = std::exchange(m_decoder, { }))
            decoder->flush();
        m_decoder = VideoDecoderVTBSession::create(m_format.get(), createPixelBufferAttributes(m_format.get()).get());
        if (!m_decoder)
            return -1;
    }

    PAL::CMSampleBufferSetOutputPresentationTimeStamp(sample.get(), PAL::CMTimeMake(timeStamp, 1));
    VTDecodeInfoFlags decodeInfoFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
    protect(m_decoder)->decodeMultiImageFrame(sample.get(), decodeInfoFlags, createMultiImageCallback(m_callback.get(), m_queue.get(), m_reorderSize));
    return 0;
}

void GPUVideoDecoderVTB::setVideoInfo(Ref<VideoInfo>&& videoInfo, uint8_t reorderSize)
{
    assertIsCurrent(queue());

    updateFormat(videoInfo);
    m_videoInfo = WTF::move(videoInfo);
    m_reorderSize = reorderSize;
    if (reorderSize && !m_queue)
        m_queue = GPUVideoDecoderVTBQueue::create(reorderSize);
}

void GPUVideoDecoderVTB::colorSpaceOverrideChanged()
{
    assertIsCurrent(queue());

    if (RefPtr videoInfo = m_videoInfo)
        updateFormat(*videoInfo);
}

void GPUVideoDecoderVTB::updateFormat(const VideoInfo& videoInfo)
{
    assertIsCurrent(queue());

    auto colorSpaceOverride = this->colorSpaceOverride();
    if (!colorSpaceOverride) {
        m_format = createFormatDescriptionFromTrackInfo(videoInfo);
        return;
    }

    auto data = videoInfo.toVideoInfoData();
    overrideVideoColorSpaceAsNeeded(data.second.colorSpace, colorSpaceOverride);
    Ref updatedVideoInfo = VideoInfo::create(WTF::move(data));
    m_format = createFormatDescriptionFromTrackInfo(updatedVideoInfo);
}

void GPUVideoDecoderVTB::flush()
{
    assertIsCurrent(queue());

    if (RefPtr decoder = m_decoder)
        decoder->flush();
    if (RefPtr queue = m_queue)
        queue->flush(m_callback.get());
}

void GPUVideoDecoderVTB::setFormat(std::span<const uint8_t>, uint16_t width, uint16_t height)
{
    assertIsCurrent(queue());

    setFrameSize(width, height);
}

void GPUVideoDecoderVTB::setFrameSize(uint16_t width, uint16_t height)
{
    assertIsCurrent(queue());

    m_width = width;
    m_height = height;
}

uint8_t GPUVideoDecoderVTBQueue::reorderSize() const
{
    Locker lock(m_lock);
    return m_queue.reorderSize();
}

void GPUVideoDecoderVTBQueue::setReorderSize(uint8_t size)
{
    Locker lock(m_lock);
    m_queue.setReorderSize(size);
}

void GPUVideoDecoderVTBQueue::add(Buffer&& buffer, GPUVideoDecoderCallback callback)
{
    Locker lock(m_lock);

    m_queue.append(WTF::move(buffer));

    bool hasCalledCallback = false;
    bool moreFramesAvailable;
    while (auto buffer = m_queue.takeIfAvailable(moreFramesAvailable)) {
        hasCalledCallback = true;
        callback(buffer->frame.get(), buffer->timeStamp, 0, moreFramesAvailable);
    }

    if (!hasCalledCallback)
        callback(nil, 0, 0, true);
}

void GPUVideoDecoderVTBQueue::flush(GPUVideoDecoderCallback callback)
{
    Locker lock(m_lock);

    while (!m_queue.isEmpty()) {
        auto buffer = m_queue.takeFirst();
        callback(buffer.frame.get(), buffer.timeStamp, 0, true);
    }
}

}

#endif //  USE(LIBWEBRTC)
