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

void ImageFrameCache::destroyDecodedData(bool destroyAll, size_t count)
{
    if (destroyAll)
        count = m_frames.size();
    
    unsigned decodedSize = 0;
    for (size_t i = 0; i <  count; ++i)
        decodedSize += m_frames[i].clearImage();

    decodedSizeReset(decodedSize);
}

bool ImageFrameCache::destroyDecodedDataIfNecessary(bool destroyAll, size_t count)
{
    unsigned decodedSize = 0;
    for (auto& frame : m_frames)
        decodedSize += frame.frameBytes();
    
    if (decodedSize < LargeAnimationCutoff)
        return false;
    
    destroyDecodedData(destroyAll, count);
    return true;
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

void ImageFrameCache::setFrameNativeImage(NativeImagePtr&& nativeImage, size_t index, SubsamplingLevel subsamplingLevel)
{
    ASSERT(index < m_frames.size());
    ImageFrame& frame = m_frames[index];

    ASSERT(isDecoderAvailable());

    frame.m_nativeImage = WTFMove(nativeImage);
    setFrameMetadata(index, subsamplingLevel);
}

void ImageFrameCache::setFrameMetadata(size_t index, SubsamplingLevel subsamplingLevel)
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

const ImageFrame& ImageFrameCache::frameAtIndex(size_t index, SubsamplingLevel subsamplingLevel, ImageFrame::Caching caching)
{
    ASSERT(index < m_frames.size());
    ImageFrame& frame = m_frames[index];
    if (!isDecoderAvailable() || caching == ImageFrame::Caching::Empty)
        return frame;
    
    if (subsamplingLevel == SubsamplingLevel::Undefinded)
        subsamplingLevel = frame.subsamplingLevel();
    
    if (frame.hasInvalidNativeImage(subsamplingLevel)) {
        unsigned decodedSize = frame.clear();
        decodedSizeDecreased(decodedSize);
    }
    
    if (!frame.isComplete() && caching == ImageFrame::Caching::Metadata)
        setFrameMetadata(index, subsamplingLevel);
    
    if (!frame.hasNativeImage() && caching == ImageFrame::Caching::MetadataAndImage) {
        size_t frameBytes = size().unclampedArea() * sizeof(RGBA32);

        // Do not create the NativeImage if adding its frameByes to the MemoryCache will cause numerical overflow.
        if (WTF::isInBounds<unsigned>(frameBytes + decodedSize())) {
            setFrameNativeImage(m_decoder->createFrameImageAtIndex(index, subsamplingLevel), index, subsamplingLevel);
            decodedSizeIncreased(frame.frameBytes());
        }
    }
    
    return frame;
}

void ImageFrameCache::clearMetadata()
{
    m_frameCount = Nullopt;
    m_singlePixelSolidColor = Nullopt;
    m_maximumSubsamplingLevel = Nullopt;
}

template<typename T, T (ImageDecoder::*functor)() const>
T ImageFrameCache::metadata(const T& defaultValue, Optional<T>* cachedValue)
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
T ImageFrameCache::frameMetadataAtIndex(size_t index, SubsamplingLevel subsamplingLevel, ImageFrame::Caching caching, Optional<T>* cachedValue)
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

Optional<IntPoint> ImageFrameCache::hotSpot()
{
    return metadata<Optional<IntPoint>, (&ImageDecoder::hotSpot)>(Nullopt, &m_hotSpot);
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

bool ImageFrameCache::frameHasInvalidNativeImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return frameHasImageAtIndex(index) && subsamplingLevel < frameSubsamplingLevelAtIndex(index);
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
