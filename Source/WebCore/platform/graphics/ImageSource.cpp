/*
 * Copyright (C) 2006, 2010, 2011, 2012, 2014, 2016 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
 * Copyright (C) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc
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
#include "ImageSource.h"

#if USE(CG)
#include "ImageDecoderCG.h"
#else
#include "ImageDecoder.h"
#endif

#include "ImageOrientation.h"

namespace WebCore {

ImageSource::ImageSource(const NativeImagePtr&)
{
}
    
ImageSource::ImageSource(ImageSource::AlphaOption alphaOption, ImageSource::GammaAndColorProfileOption gammaAndColorProfileOption)
    : m_needsUpdateMetadata(true)
    , m_maximumSubsamplingLevel(Nullopt)
    , m_alphaOption(alphaOption)
    , m_gammaAndColorProfileOption(gammaAndColorProfileOption)
{
}

ImageSource::~ImageSource()
{
}

void ImageSource::clearFrameBufferCache(size_t clearBeforeFrame)
{
    if (!initialized())
        return;
    m_decoder->clearFrameBufferCache(clearBeforeFrame);
}

void ImageSource::clear(bool destroyAllFrames, size_t clearBeforeFrame, SharedBuffer* data, bool allDataReceived)
{
    // There's no need to throw away the decoder unless we're explicitly asked
    // to destroy all of the frames.
    if (!destroyAllFrames) {
        clearFrameBufferCache(clearBeforeFrame);
        return;
    }

    m_decoder = nullptr;

    if (data)
        setData(data, allDataReceived);
}

void ImageSource::setData(SharedBuffer* data, bool allDataReceived)
{
    if (!data)
        return;

    if (!initialized()) {
        m_decoder = ImageDecoder::create(*data, m_alphaOption, m_gammaAndColorProfileOption);
        if (!m_decoder)
            return;
    }

    m_decoder->setData(*data, allDataReceived);
}

SubsamplingLevel ImageSource::calculateMaximumSubsamplingLevel() const
{
    if (!m_allowSubsampling || !frameAllowSubsamplingAtIndex(0))
        return SubsamplingLevel::Default;
    
    // FIXME: this value was chosen to be appropriate for iOS since the image
    // subsampling is only enabled by default on iOS. Choose a different value
    // if image subsampling is enabled on other platform.
    const int maximumImageAreaBeforeSubsampling = 5 * 1024 * 1024;

    for (SubsamplingLevel level = SubsamplingLevel::First; level <= SubsamplingLevel::Last; ++level) {
        if (frameSizeAtIndex(0, level).area() < maximumImageAreaBeforeSubsampling)
            return level;
    }
    
    return SubsamplingLevel::Last;
}

void ImageSource::updateMetadata()
{
    if (!(m_needsUpdateMetadata && isSizeAvailable()))
        return;

    m_frameCount = m_decoder->frameCount();
    if (!m_maximumSubsamplingLevel)
        m_maximumSubsamplingLevel = calculateMaximumSubsamplingLevel();

    m_needsUpdateMetadata = false;
}
    
SubsamplingLevel ImageSource::subsamplingLevelForScale(float scale)
{
    if (!(scale > 0 && scale <= 1))
        return SubsamplingLevel::Default;

    updateMetadata();
    if (!m_maximumSubsamplingLevel)
        return SubsamplingLevel::Default;

    int result = std::ceil(std::log2(1 / scale));
    return static_cast<SubsamplingLevel>(std::min(result, static_cast<int>(m_maximumSubsamplingLevel.value())));
}

size_t ImageSource::bytesDecodedToDetermineProperties()
{
    return ImageDecoder::bytesDecodedToDetermineProperties();
}

bool ImageSource::isSizeAvailable() const
{
    return initialized() && m_decoder->isSizeAvailable();
}

IntSize ImageSource::size() const
{
    return frameSizeAtIndex(0, SubsamplingLevel::Default);
}

IntSize ImageSource::sizeRespectingOrientation() const
{
    return frameSizeAtIndex(0, SubsamplingLevel::Default, RespectImageOrientation);
}

size_t ImageSource::frameCount()
{
    updateMetadata();
    return m_frameCount;
}

RepetitionCount ImageSource::repetitionCount()
{
    return initialized() ? m_decoder->repetitionCount() : RepetitionCountNone;
}

String ImageSource::filenameExtension() const
{
    return initialized() ? m_decoder->filenameExtension() : String();
}

Optional<IntPoint> ImageSource::hotSpot() const
{
    return initialized() ? m_decoder->hotSpot() : Nullopt;
}

bool ImageSource::frameIsCompleteAtIndex(size_t index)
{
    return initialized() && m_decoder->frameIsCompleteAtIndex(index);
}

bool ImageSource::frameHasAlphaAtIndex(size_t index)
{
    return !initialized() || m_decoder->frameHasAlphaAtIndex(index);
}

bool ImageSource::frameAllowSubsamplingAtIndex(size_t index) const
{
    return initialized() && m_decoder->frameAllowSubsamplingAtIndex(index);
}

IntSize ImageSource::frameSizeAtIndex(size_t index, SubsamplingLevel subsamplingLevel, RespectImageOrientationEnum shouldRespectImageOrientation) const
{
    if (!initialized())
        return { };

    IntSize size = m_decoder->frameSizeAtIndex(index, subsamplingLevel);
    if (shouldRespectImageOrientation != RespectImageOrientation)
        return size;

    return frameOrientationAtIndex(index).usesWidthAsHeight() ? size.transposedSize() : size;
}

unsigned ImageSource::frameBytesAtIndex(size_t index, SubsamplingLevel subsamplingLevel) const
{
    return frameSizeAtIndex(index, subsamplingLevel).area() * 4;
}

float ImageSource::frameDurationAtIndex(size_t index)
{
    return initialized() ? m_decoder->frameDurationAtIndex(index) : 0;
}

ImageOrientation ImageSource::frameOrientationAtIndex(size_t index) const
{
    return initialized() ? m_decoder->frameOrientationAtIndex(index) : ImageOrientation();
}
    
NativeImagePtr ImageSource::createFrameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return initialized() ? m_decoder->createFrameImageAtIndex(index, subsamplingLevel) : nullptr;
}

void ImageSource::dump(TextStream& ts) const
{
    if (m_allowSubsampling)
        ts.dumpProperty("allow-subsampling", m_allowSubsampling);
    
    ImageOrientation orientation = frameOrientationAtIndex(0);
    if (orientation != OriginTopLeft)
        ts.dumpProperty("orientation", orientation);
}

}
