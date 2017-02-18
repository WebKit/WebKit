/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageFrameCache.h"

#include "Image.h"
#include "ImageObserver.h"

#if USE(CG)
#include "ImageDecoderCG.h"
#elif USE(DIRECT2D)
#include "ImageDecoderDirect2D.h"
#include <WinCodec.h>
#else
#include "ImageDecoder.h"
#endif

#include <wtf/CheckedArithmetic.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebCore {

ImageFrameCache::ImageFrameCache(Image* image)
    : m_image(image)
{
}

ImageFrameCache::ImageFrameCache(NativeImagePtr&& nativeImage)
{
    m_frameCount = 1;
    m_isSizeAvailable = true;
    growFrames();

    setNativeImage(WTFMove(nativeImage));

    m_decodedSize = m_frames[0].frameBytes();

    // The assumption is the memory image will be displayed with the default
    // orientation. So set m_sizeRespectingOrientation to be the same as m_size.
    m_size = m_frames[0].size();
    m_sizeRespectingOrientation = m_size;
}

ImageFrameCache::~ImageFrameCache()
{
    ASSERT(!hasDecodingQueue());
}

void ImageFrameCache::destroyDecodedData(size_t frameCount, size_t excludeFrame)
{
    unsigned decodedSize = 0;

    ASSERT(frameCount <= m_frames.size());

    for (size_t index = 0; index < frameCount; ++index) {
        if (index == excludeFrame)
            continue;
        decodedSize += m_frames[index++].clearImage();
    }

    decodedSizeReset(decodedSize);
}

void ImageFrameCache::destroyIncompleteDecodedData()
{
    unsigned decodedSize = 0;
    
    for (auto& frame : m_frames) {
        if (!frame.hasMetadata() || frame.isComplete())
            continue;
        
        decodedSize += frame.clear();
    }

    decodedSizeDecreased(decodedSize);
}

void ImageFrameCache::decodedSizeChanged(long long decodedSize)
{
    if (!decodedSize || !m_image || !m_image->imageObserver())
        return;
    
    m_image->imageObserver()->decodedSizeChanged(m_image, decodedSize);
}

void ImageFrameCache::decodedSizeIncreased(unsigned decodedSize)
{
    if (!decodedSize)
        return;
    
    m_decodedSize += decodedSize;
    
    // The fully-decoded frame will subsume the partially decoded data used
    // to determine image properties.
    long long changeSize = static_cast<long long>(decodedSize) - m_decodedPropertiesSize;
    m_decodedPropertiesSize = 0;
    decodedSizeChanged(changeSize);
}

void ImageFrameCache::decodedSizeDecreased(unsigned decodedSize)
{
    if (!decodedSize)
        return;

    ASSERT(m_decodedSize >= decodedSize);
    m_decodedSize -= decodedSize;
    decodedSizeChanged(-static_cast<long long>(decodedSize));
}

void ImageFrameCache::decodedSizeReset(unsigned decodedSize)
{
    ASSERT(m_decodedSize >= decodedSize);
    m_decodedSize -= decodedSize;

    // Clearing the ImageSource destroys the extra decoded data used for
    // determining image properties.
    decodedSize += m_decodedPropertiesSize;
    m_decodedPropertiesSize = 0;
    decodedSizeChanged(-static_cast<long long>(decodedSize));
}

void ImageFrameCache::didDecodeProperties(unsigned decodedPropertiesSize)
{
    if (m_decodedSize)
        return;

    long long decodedSize = static_cast<long long>(decodedPropertiesSize) - m_decodedPropertiesSize;
    m_decodedPropertiesSize = decodedPropertiesSize;
    decodedSizeChanged(decodedSize);
}

void ImageFrameCache::growFrames()
{
    ASSERT(isSizeAvailable());
    ASSERT(m_frames.size() <= frameCount());
    m_frames.grow(frameCount());
}

void ImageFrameCache::setNativeImage(NativeImagePtr&& nativeImage)
{
    ASSERT(m_frames.size() == 1);
    ImageFrame& frame = m_frames[0];

    ASSERT(!isDecoderAvailable());

    frame.m_nativeImage = WTFMove(nativeImage);

    frame.m_decoding = ImageFrame::Decoding::Complete;
    frame.m_size = nativeImageSize(frame.m_nativeImage);
    frame.m_hasAlpha = nativeImageHasAlpha(frame.m_nativeImage);
}

void ImageFrameCache::setFrameNativeImageAtIndex(NativeImagePtr&& nativeImage, size_t index, SubsamplingLevel subsamplingLevel)
{
    ASSERT(index < m_frames.size());
    ImageFrame& frame = m_frames[index];

    ASSERT(isDecoderAvailable());

    frame.m_nativeImage = WTFMove(nativeImage);
    setFrameMetadataAtIndex(index, subsamplingLevel);
}

void ImageFrameCache::setFrameMetadataAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    ASSERT(index < m_frames.size());
    ImageFrame& frame = m_frames[index];

    ASSERT(isDecoderAvailable());
    frame.m_decoding = m_decoder->frameIsCompleteAtIndex(index) ? ImageFrame::Decoding::Complete : ImageFrame::Decoding::Partial;
    if (frame.hasMetadata())
        return;
    
    frame.m_subsamplingLevel = subsamplingLevel;
    frame.m_size = m_decoder->frameSizeAtIndex(index, subsamplingLevel);
    frame.m_orientation = m_decoder->frameOrientationAtIndex(index);
    frame.m_hasAlpha = m_decoder->frameHasAlphaAtIndex(index);
    
    if (repetitionCount())
        frame.m_duration = m_decoder->frameDurationAtIndex(index);
}

void ImageFrameCache::replaceFrameNativeImageAtIndex(NativeImagePtr&& nativeImage, size_t index, SubsamplingLevel subsamplingLevel)
{
    ASSERT(index < m_frames.size());
    ImageFrame& frame = m_frames[index];

    if (!frame.hasValidNativeImage(subsamplingLevel)) {
        // Clear the current image frame and update the observer with this clearance.
        unsigned decodedSize = frame.clear();
        decodedSizeDecreased(decodedSize);
    }

    // Do not cache the NativeImage if adding its frameByes to the MemoryCache will cause numerical overflow.
    size_t frameBytes = size().unclampedArea() * sizeof(RGBA32);
    if (!WTF::isInBounds<unsigned>(frameBytes + decodedSize()))
        return;

    // Copy the new image to the cache.
    setFrameNativeImageAtIndex(WTFMove(nativeImage), index, subsamplingLevel);

    // Update the observer with the new image frame bytes.
    decodedSizeIncreased(frame.frameBytes());
}

void ImageFrameCache::cacheFrameNativeImageAtIndex(NativeImagePtr&& nativeImage, size_t index, SubsamplingLevel subsamplingLevel)
{
    if (!isDecoderAvailable())
        return;

    ASSERT(index < m_frames.size());
    ASSERT(m_frames[index].isBeingDecoded());

    // Clean the old native image and set a new one
    replaceFrameNativeImageAtIndex(WTFMove(nativeImage), index, subsamplingLevel);

    // Notify the image with the readiness of the new frame NativeImage.
    if (m_image)
        m_image->newFrameNativeImageAvailableAtIndex(index);
}

Ref<WorkQueue> ImageFrameCache::decodingQueue()
{
    if (!m_decodingQueue)
        m_decodingQueue = WorkQueue::create("org.webkit.ImageDecoder", WorkQueue::Type::Serial, WorkQueue::QOS::UserInteractive);
    
    return *m_decodingQueue;
}

void ImageFrameCache::startAsyncDecodingQueue()
{
    if (hasDecodingQueue() || !isDecoderAvailable())
        return;

    m_frameRequestQueue.open();

    Ref<ImageFrameCache> protectedThis = Ref<ImageFrameCache>(*this);
    Ref<WorkQueue> protectedQueue = decodingQueue();

    // We need to protect this and m_decodingQueue from being deleted while we are in the decoding loop.
    decodingQueue()->dispatch([this, protectedThis = WTFMove(protectedThis), protectedQueue = WTFMove(protectedQueue)] {
        ImageFrameRequest frameRequest;

        while (m_frameRequestQueue.dequeue(frameRequest)) {
            // Get the frame NativeImage on the decoding thread.
            NativeImagePtr nativeImage = m_decoder->createFrameImageAtIndex(frameRequest.index, frameRequest.subsamplingLevel, DecodingMode::Immediate);

            // Update the cached frames on the main thread to avoid updating the MemoryCache from a different thread.
            callOnMainThread([this, protectedQueue = protectedQueue.copyRef(), nativeImage, frameRequest] () mutable {
                // The queue may be closed if after we got the frame NativeImage, stopAsyncDecodingQueue() was called
                if (protectedQueue.ptr() == m_decodingQueue)
                    cacheFrameNativeImageAtIndex(WTFMove(nativeImage), frameRequest.index, frameRequest.subsamplingLevel);
            });
        }
    });
}

bool ImageFrameCache::requestFrameAsyncDecodingAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    if (!isDecoderAvailable())
        return false;

    if (!hasDecodingQueue())
        startAsyncDecodingQueue();

    ASSERT(index < m_frames.size());
    ImageFrame& frame = m_frames[index];

    // We need to coalesce multiple requests for decoding the same ImageFrame while it
    // is still being decoded. This may happen if the image rectangle is repainted
    // multiple times while the ImageFrame has not finished decoding.
    if (frame.isBeingDecoded())
        return true;

    if (subsamplingLevel == SubsamplingLevel::Undefinded)
        subsamplingLevel = frame.subsamplingLevel();

    if (frame.hasValidNativeImage(subsamplingLevel))
        return false;

    frame.setDecoding(ImageFrame::Decoding::BeingDecoded);
    m_frameRequestQueue.enqueue({ index, subsamplingLevel });
    return true;
}

void ImageFrameCache::stopAsyncDecodingQueue()
{
    if (!hasDecodingQueue())
        return;
    
    m_frameRequestQueue.close();
    m_decodingQueue = nullptr;

    for (ImageFrame& frame : m_frames) {
        if (frame.isBeingDecoded())
            frame.clear();
    }
}

const ImageFrame& ImageFrameCache::frameAtIndex(size_t index, SubsamplingLevel subsamplingLevel, ImageFrame::Caching caching)
{
    ASSERT(index < m_frames.size());
    ImageFrame& frame = m_frames[index];
    if (!isDecoderAvailable() || frame.isBeingDecoded() || caching == ImageFrame::Caching::Empty)
        return frame;
    
    if (subsamplingLevel == SubsamplingLevel::Undefinded)
        subsamplingLevel = frame.subsamplingLevel();

    if (!frame.isComplete() && caching == ImageFrame::Caching::Metadata)
        setFrameMetadataAtIndex(index, subsamplingLevel);
    else if (!frame.hasValidNativeImage(subsamplingLevel) && caching == ImageFrame::Caching::MetadataAndImage)
        replaceFrameNativeImageAtIndex(m_decoder->createFrameImageAtIndex(index, subsamplingLevel), index, subsamplingLevel);

    return frame;
}

void ImageFrameCache::clearMetadata()
{
    m_frameCount = std::nullopt;
    m_singlePixelSolidColor = std::nullopt;
}

template<typename T, T (ImageDecoder::*functor)() const>
T ImageFrameCache::metadata(const T& defaultValue, std::optional<T>* cachedValue)
{
    if (cachedValue && *cachedValue)
        return cachedValue->value();

    if (!isDecoderAvailable() || !m_decoder->isSizeAvailable())
        return defaultValue;

    if (!cachedValue)
        return (m_decoder->*functor)();

    *cachedValue = (m_decoder->*functor)();
    didDecodeProperties(m_decoder->bytesDecodedToDetermineProperties());
    return cachedValue->value();
}

template<typename T, T (ImageFrame::*functor)() const>
T ImageFrameCache::frameMetadataAtIndex(size_t index, SubsamplingLevel subsamplingLevel, ImageFrame::Caching caching, std::optional<T>* cachedValue)
{
    if (cachedValue && *cachedValue)
        return cachedValue->value();
    
    const ImageFrame& frame = index < m_frames.size() ? frameAtIndex(index, subsamplingLevel, caching) : ImageFrame::defaultFrame();

    // Don't cache any unavailable frame metadata.
    if (!frame.hasMetadata() || !cachedValue)
        return (frame.*functor)();
    
    *cachedValue = (frame.*functor)();
    return cachedValue->value();
}

bool ImageFrameCache::isSizeAvailable()
{
    if (m_isSizeAvailable)
        return m_isSizeAvailable.value();
    
    if (!isDecoderAvailable() || !m_decoder->isSizeAvailable())
        return false;
    
    m_isSizeAvailable = true;
    didDecodeProperties(m_decoder->bytesDecodedToDetermineProperties());
    return true;
}

size_t ImageFrameCache::frameCount()
{
    return metadata<size_t, (&ImageDecoder::frameCount)>(m_frames.size(), &m_frameCount);
}

RepetitionCount ImageFrameCache::repetitionCount()
{
    return metadata<RepetitionCount, (&ImageDecoder::repetitionCount)>(RepetitionCountNone, &m_repetitionCount);
}

String ImageFrameCache::filenameExtension()
{
    return metadata<String, (&ImageDecoder::filenameExtension)>(String(), &m_filenameExtension);
}

std::optional<IntPoint> ImageFrameCache::hotSpot()
{
    return metadata<std::optional<IntPoint>, (&ImageDecoder::hotSpot)>(std::nullopt, &m_hotSpot);
}

IntSize ImageFrameCache::size()
{
    return frameMetadataAtIndex<IntSize, (&ImageFrame::size)>(0, SubsamplingLevel::Default, ImageFrame::Caching::Metadata, &m_size);
}

IntSize ImageFrameCache::sizeRespectingOrientation()
{
    return frameMetadataAtIndex<IntSize, (&ImageFrame::sizeRespectingOrientation)>(0, SubsamplingLevel::Default, ImageFrame::Caching::Metadata, &m_sizeRespectingOrientation);
}

Color ImageFrameCache::singlePixelSolidColor()
{
    return frameCount() == 1 ? frameMetadataAtIndex<Color, (&ImageFrame::singlePixelSolidColor)>(0, SubsamplingLevel::Undefinded, ImageFrame::Caching::MetadataAndImage, &m_singlePixelSolidColor) : Color();
}

bool ImageFrameCache::frameIsBeingDecodedAtIndex(size_t index)
{
    return frameMetadataAtIndex<bool, (&ImageFrame::isBeingDecoded)>(index);
}

bool ImageFrameCache::frameIsCompleteAtIndex(size_t index)
{
    return frameMetadataAtIndex<bool, (&ImageFrame::isComplete)>(index);
}

bool ImageFrameCache::frameHasAlphaAtIndex(size_t index)
{
    return frameMetadataAtIndex<bool, (&ImageFrame::hasAlpha)>(index);
}

bool ImageFrameCache::frameHasImageAtIndex(size_t index)
{
    return frameMetadataAtIndex<bool, (&ImageFrame::hasNativeImage)>(index);
}

bool ImageFrameCache::frameHasValidNativeImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return frameHasImageAtIndex(index) && subsamplingLevel >= frameSubsamplingLevelAtIndex(index);
}

SubsamplingLevel ImageFrameCache::frameSubsamplingLevelAtIndex(size_t index)
{
    return frameMetadataAtIndex<SubsamplingLevel, (&ImageFrame::subsamplingLevel)>(index);
}

IntSize ImageFrameCache::frameSizeAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return frameMetadataAtIndex<IntSize, (&ImageFrame::size)>(index, subsamplingLevel, ImageFrame::Caching::Metadata);
}

unsigned ImageFrameCache::frameBytesAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return frameMetadataAtIndex<unsigned, (&ImageFrame::frameBytes)>(index, subsamplingLevel, ImageFrame::Caching::Metadata);
}

float ImageFrameCache::frameDurationAtIndex(size_t index)
{
    return frameMetadataAtIndex<float, (&ImageFrame::duration)>(index, SubsamplingLevel::Undefinded, ImageFrame::Caching::Metadata);
}

ImageOrientation ImageFrameCache::frameOrientationAtIndex(size_t index)
{
    return frameMetadataAtIndex<ImageOrientation, (&ImageFrame::orientation)>(index, SubsamplingLevel::Undefinded, ImageFrame::Caching::Metadata);
}

NativeImagePtr ImageFrameCache::frameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return frameMetadataAtIndex<NativeImagePtr, (&ImageFrame::nativeImage)>(index, subsamplingLevel, ImageFrame::Caching::MetadataAndImage);
}

}
