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
#include "WebRTCVideoDecoderVTB.h"

#if USE(LIBWEBRTC)

#import "MediaReorderQueue.h"
#import <WebCore/CMUtilities.h>
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

    auto ioSurfaceValue = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    int64_t nv12type = shouldUseFullRange(format) ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    auto pixelFormat = adoptCF(CFNumberCreate(nullptr, kCFNumberLongType, &nv12type));
    CFTypeRef values[attributesSize] = { kCFBooleanTrue, ioSurfaceValue.get(), pixelFormat.get() };
    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, attributesSize, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

class WebRTCVideoDecoderVTBQueue : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<WebRTCVideoDecoderVTBQueue> {
public:
    static Ref<WebRTCVideoDecoderVTBQueue> create(uint8_t reorderSize) { return adoptRef(*new WebRTCVideoDecoderVTBQueue(reorderSize)); }
    void setReorderSize(uint8_t);
    uint8_t reorderSize() const;

    struct Buffer {
        RetainPtr<CVPixelBufferRef> frame;
        int64_t timeStamp { 0 };
    };
    void add(Buffer&&, WebRTCVideoDecoderCallback);
    void flush(WebRTCVideoDecoderCallback);

private:
    explicit WebRTCVideoDecoderVTBQueue(uint8_t reorderSize)
        : m_queue(reorderSize)
    {
    }

    struct BufferComparator {
        bool operator()(const Buffer& a, const Buffer& b) const { return a.timeStamp <= b.timeStamp; }
    };

    mutable Lock m_lock;
    MediaReorderQueue<Buffer, BufferComparator> m_queue WTF_GUARDED_BY_LOCK(m_lock);
};

WebRTCVideoDecoderVTB::WebRTCVideoDecoderVTB(WebRTCVideoDecoderCallback callback)
    : m_callback(makeBlockPtr(callback))
{
}

WebRTCVideoDecoderVTB::~WebRTCVideoDecoderVTB() = default;

static VideoDecoderVTB::CallbackMultiImage createMultiImageCallback(WebRTCVideoDecoderCallback callback, RefPtr<WebRTCVideoDecoderVTBQueue>&& queue, uint8_t reorderSize)
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

int32_t WebRTCVideoDecoderVTB::decodeFrameInternal(int64_t timeStamp, std::span<const uint8_t> data)
{
    if (!m_format)
        return 0;

    RetainPtr sample = sampleBufferFromVideoData(data, m_format.get());
    if (!sample)
        return -1;

    if (!m_decoder || !protect(m_decoder)->canAccept(m_format.get())) {
        m_decoder = VideoDecoderVTB::create(m_format.get(), createPixelBufferAttributes(m_format.get()).get());
        if (!m_decoder)
            return -1;
    }

    PAL::CMSampleBufferSetOutputPresentationTimeStamp(sample.get(), PAL::CMTimeMake(timeStamp, 1));
    VTDecodeInfoFlags decodeInfoFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
    protect(m_decoder)->decodeMultiImageFrame(sample.get(), decodeInfoFlags, createMultiImageCallback(m_callback.get(), m_queue.get(), m_reorderSize));
    return 0;
}

void WebRTCVideoDecoderVTB::setVideoFormat(RetainPtr<CMVideoFormatDescriptionRef>&& format, uint8_t reorderSize)
{
    m_format = WTF::move(format);
    m_reorderSize = reorderSize;
    if (reorderSize && !m_queue)
        m_queue = WebRTCVideoDecoderVTBQueue::create(reorderSize);
}

void WebRTCVideoDecoderVTB::flush()
{
    protect(m_decoder)->flush();
    if (RefPtr queue = m_queue)
        queue->flush(m_callback.get());
}

void WebRTCVideoDecoderVTB::setFormat(std::span<const uint8_t>, uint16_t width, uint16_t height)
{
    setFrameSize(width, height);
}

void WebRTCVideoDecoderVTB::setFrameSize(uint16_t width, uint16_t height)
{
    m_width = width;
    m_height = height;
}

uint8_t WebRTCVideoDecoderVTBQueue::reorderSize() const
{
    Locker lock(m_lock);
    return m_queue.reorderSize();
}

void WebRTCVideoDecoderVTBQueue::setReorderSize(uint8_t size)
{
    Locker lock(m_lock);
    m_queue.setReorderSize(size);
}

void WebRTCVideoDecoderVTBQueue::add(Buffer&& buffer, WebRTCVideoDecoderCallback callback)
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

void WebRTCVideoDecoderVTBQueue::flush(WebRTCVideoDecoderCallback callback)
{
    Locker lock(m_lock);

    while (!m_queue.isEmpty()) {
        auto buffer = m_queue.takeFirst();
        callback(buffer.frame.get(), buffer.timeStamp, 0, true);
    }
}

}

#endif //  USE(LIBWEBRTC)
