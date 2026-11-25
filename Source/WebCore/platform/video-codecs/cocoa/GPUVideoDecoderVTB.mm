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
#import <WebCore/PlanarImageConverterCV.h>
#import <wtf/ThreadSafeWeakPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cf/VectorCF.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GPUVideoDecoderVTB);

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

static PlanarImageConverter::ChromaSubsampling chromaSubsamplingFromFormat(CMVideoFormatDescriptionRef format)
{
    using enum PlanarImageConverter::ChromaSubsampling;
    RetainPtr atoms = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(format, PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
    if (!atoms)
        return HorizontalAndVertical;
    RetainPtr hvcC = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(atoms, CFSTR("hvcC")));
    if (!hvcC)
        return HorizontalAndVertical;
    // ISO/IEC 14496-15 8.3.3.1: chroma_format_idc is in byte 16 of the 23-byte fixed part of the HEVCDecoderConfigurationRecord.
    auto record = span(hvcC);
    if (record.size() < 23)
        return HorizontalAndVertical;
    switch (record[16] & 0x3) {
    case 2:
        return Horizontal;
    case 3:
        return None;
    default:
        return HorizontalAndVertical;
    }
}

static RetainPtr<CFDictionaryRef> createPixelBufferAttributes(CMVideoFormatDescriptionRef format)
{
    auto sampleFormat = PlanarImageConverter::pixelBufferSampleFormat({ static_cast<uint8_t>(bitDepthFromFormat(format)), false });
    if (!sampleFormat)
        return nullptr;
    auto pixelFormatType = PlanarImageConverter::pixelFormat(chromaSubsamplingFromFormat(format), *sampleFormat, shouldUseFullRange(format));
    if (!pixelFormatType)
        return nullptr;

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
    int64_t pixelFormatValue = *pixelFormatType;
    RetainPtr pixelFormat = adoptCF(CFNumberCreate(nullptr, kCFNumberLongType, &pixelFormatValue));
    CFTypeRef values[attributesSize] = { kCFBooleanTrue, ioSurfaceValue.get(), pixelFormat.get() };
    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, attributesSize, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

class GPUVideoDecoderVTBQueue : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GPUVideoDecoderVTBQueue> {
public:
    NODELETE static Ref<GPUVideoDecoderVTBQueue> create(uint8_t reorderQueueMaxSize) { return adoptRef(*new GPUVideoDecoderVTBQueue(reorderQueueMaxSize)); }
    NODELETE void setReorderQueueMaxSize(uint8_t);
    NODELETE uint8_t reorderQueueMaxSize() const;

    struct Buffer {
        RetainPtr<CVPixelBufferRef> frame;
        int64_t timeStamp { 0 };
    };
    void add(Buffer&&, GPUVideoDecoderCallback);
    void flush(GPUVideoDecoderCallback);

private:
    explicit GPUVideoDecoderVTBQueue(uint8_t reorderQueueMaxSize)
        : m_queue(reorderQueueMaxSize)
    {
    }

    struct BufferComparator {
        NODELETE bool operator()(const Buffer& a, const Buffer& b) const { return a.timeStamp <= b.timeStamp; }
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

static VideoDecoderVTBSession::CallbackMultiImage createMultiImageCallback(GPUVideoDecoderCallback callback, RefPtr<GPUVideoDecoderVTBQueue>&& queue, uint8_t reorderQueueMaxSize)
{
    return makeBlockPtr([callback = makeBlockPtr(callback), queue = WTF::move(queue), reorderQueueMaxSize](OSStatus, VTDecodeInfoFlags, CVImageBufferRef pixelBuffer, CMTaggedBufferGroupRef, CMTime presentationTime, CMTime) mutable {
        UNUSED_PARAM(reorderQueueMaxSize);
        if (!pixelBuffer) {
            callback(nil, 0, 0, false);
            return;
        }

        if (!queue) {
            callback((CVPixelBufferRef)pixelBuffer, presentationTime.value, 0, false);
            return;
        }

        if (reorderQueueMaxSize != queue->reorderQueueMaxSize()) {
            queue->flush(callback.get());
            queue->setReorderQueueMaxSize(reorderQueueMaxSize);
        }

        if (!reorderQueueMaxSize) {
            callback((CVPixelBufferRef)pixelBuffer, presentationTime.value, 0, false);
            return;
        }
        queue->add({ (CVPixelBufferRef)pixelBuffer, presentationTime.value }, callback.get());
    });
}

bool GPUVideoDecoderVTB::ensureDecoder()
{
    assertIsCurrent(queue());

    if (!m_format)
        return false;

    if (m_decoder && protect(m_decoder)->canAccept(m_format.get()))
        return true;

    if (RefPtr decoder = std::exchange(m_decoder, { }))
        decoder->flush();
    RetainPtr attributes = createPixelBufferAttributes(m_format);
    if (!attributes)
        return false;
    m_decoder = VideoDecoderVTBSession::create(m_format, attributes);
    return !!m_decoder;
}

int32_t GPUVideoDecoderVTB::decodeFrame(int64_t timeStamp, std::span<const uint8_t> data)
{
    assertIsCurrent(queue());

    if (!m_format)
        return 0;

    RetainPtr sample = sampleBufferFromVideoData(data, m_format.get());
    if (!sample)
        return -1;

    if (!ensureDecoder())
        return -1;

    PAL::CMSampleBufferSetOutputPresentationTimeStamp(sample.get(), PAL::CMTimeMake(timeStamp, 1));
    VTDecodeInfoFlags decodeInfoFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
    protect(m_decoder)->decodeMultiImageFrame(sample.get(), decodeInfoFlags, createMultiImageCallback(m_callback.get(), m_queue.get(), m_reorderQueueMaxSize));
    return 0;
}

bool GPUVideoDecoderVTB::setVideoInfo(Ref<VideoInfo>&& videoInfo)
{
    assertIsCurrent(queue());

    updateFormat(videoInfo);
    m_reorderQueueMaxSize = videoInfo->reorderQueueMaxSize().value_or(0);
    m_videoInfo = WTF::move(videoInfo);
    if (m_reorderQueueMaxSize && !m_queue)
        lazyInitialize(m_queue, GPUVideoDecoderVTBQueue::create(m_reorderQueueMaxSize));

    return ensureDecoder();
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

uint8_t GPUVideoDecoderVTBQueue::reorderQueueMaxSize() const
{
    Locker lock(m_lock);
    return m_queue.reorderSize();
}

void GPUVideoDecoderVTBQueue::setReorderQueueMaxSize(uint8_t size)
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
