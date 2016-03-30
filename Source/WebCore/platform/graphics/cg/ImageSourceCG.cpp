/*
 * Copyright (C) 2004, 2005, 2006, 2008, 2016 Apple Inc. All rights reserved.
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
#include "ImageSourceCG.h"

#include "ImageDecoderCG.h"
#include "ImageOrientation.h"
#include "SharedBuffer.h"

namespace WebCore {

ImageSource::ImageSource(ImageSource::AlphaOption, ImageSource::GammaAndColorProfileOption)
{
    // FIXME: AlphaOption and GammaAndColorProfileOption are ignored.
}

ImageSource::~ImageSource()
{
    clear(true);
}

void ImageSource::clear(bool destroyAllFrames, size_t, SharedBuffer* data, bool allDataReceived)
{
    // Recent versions of ImageIO discard previously decoded image frames if the client
    // application no longer holds references to them, so there's no need to throw away
    // the decoder unless we're explicitly asked to destroy all of the frames.
    if (!destroyAllFrames)
        return;

    m_decoder = nullptr;
    
    if (data)
        setData(data, allDataReceived);
}
    
void ImageSource::ensureDecoderIsCreated(SharedBuffer*)
{
    if (initialized())
        return;
    m_decoder = ImageDecoder::create();
}

void ImageSource::setData(SharedBuffer* data, bool allDataReceived)
{
    if (!data)
        return;
    
    ensureDecoderIsCreated(data);
    
    if (!initialized()) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    m_decoder->setData(data, allDataReceived);
}

String ImageSource::filenameExtension() const
{
    return initialized() ? m_decoder->filenameExtension() : String();
}
    
SubsamplingLevel ImageSource::calculateMaximumSubsamplingLevel() const
{
    if (!m_allowSubsampling || !allowSubsamplingOfFrameAtIndex(0))
        return 0;

    // Values chosen to be appropriate for iOS.
    const int cMaximumImageAreaBeforeSubsampling = 5 * 1024 * 1024;
    const SubsamplingLevel maxSubsamplingLevel = 3;

    SubsamplingLevel currentLevel = 0;
    for ( ; currentLevel <= maxSubsamplingLevel; ++currentLevel) {
        IntSize frameSize = frameSizeAtIndex(0, currentLevel);
        if (frameSize.area() < cMaximumImageAreaBeforeSubsampling)
            break;
    }

    return currentLevel;
}
    
SubsamplingLevel ImageSource::maximumSubsamplingLevel() const
{
#if PLATFORM(IOS)
    return calculateMaximumSubsamplingLevel();
#endif
    return 0;
}

SubsamplingLevel ImageSource::subsamplingLevelForScale(float scale) const
{
    return ImageDecoder::subsamplingLevelForScale(scale, maximumSubsamplingLevel());
}

bool ImageSource::isSizeAvailable()
{
    return initialized() && m_decoder->isSizeAvailable();
}

bool ImageSource::allowSubsamplingOfFrameAtIndex(size_t index) const
{
    return initialized() && m_decoder->allowSubsamplingOfFrameAtIndex(index);
}

IntSize ImageSource::frameSizeAtIndex(size_t index, SubsamplingLevel subsamplingLevel, RespectImageOrientationEnum shouldRespectImageOrientation) const
{
    if (!initialized())
        return { };
    
    IntSize size = m_decoder->frameSizeAtIndex(index, subsamplingLevel);
    ImageOrientation orientation = m_decoder->orientationAtIndex(index);
    
    return shouldRespectImageOrientation == RespectImageOrientation && orientation.usesWidthAsHeight() ? size.transposedSize() : size;
}

ImageOrientation ImageSource::orientationAtIndex(size_t index) const
{
    return initialized() ? m_decoder->orientationAtIndex(index) : ImageOrientation();
}

IntSize ImageSource::size() const
{
    return frameSizeAtIndex(0, 0);
}
    
IntSize ImageSource::sizeRespectingOrientation() const
{
    return frameSizeAtIndex(0, 0, RespectImageOrientation);
}

bool ImageSource::getHotSpot(IntPoint& hotSpot) const
{
    return initialized() && m_decoder->hotSpot(hotSpot);
}

size_t ImageSource::bytesDecodedToDetermineProperties() const
{
    return ImageDecoder::bytesDecodedToDetermineProperties();
}
    
int ImageSource::repetitionCount()
{
    return initialized() ? m_decoder->repetitionCount() : cAnimationLoopOnce;
}

size_t ImageSource::frameCount() const
{
    return initialized() ? m_decoder->frameCount() : 0;
}

RetainPtr<CGImageRef> ImageSource::createFrameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return initialized() ? m_decoder->createFrameImageAtIndex(index, subsamplingLevel) : nullptr;
}

bool ImageSource::frameIsCompleteAtIndex(size_t index)
{
    return initialized() && m_decoder->frameIsCompleteAtIndex(index);
}

float ImageSource::frameDurationAtIndex(size_t index)
{
    return initialized() ? m_decoder->frameDurationAtIndex(index) : 0;
}

bool ImageSource::frameHasAlphaAtIndex(size_t index)
{
    return !initialized() || m_decoder->frameHasAlphaAtIndex(index);
}

unsigned ImageSource::frameBytesAtIndex(size_t index, SubsamplingLevel subsamplingLevel) const
{
    IntSize frameSize = frameSizeAtIndex(index, subsamplingLevel);
    return frameSize.area() * 4;
}
    
void ImageSource::dump(TextStream& ts) const
{
    if (m_allowSubsampling)
        ts.dumpProperty("allow-subsampling", m_allowSubsampling);
    
    ImageOrientation orientation = orientationAtIndex(0);
    if (orientation != OriginTopLeft)
        ts.dumpProperty("orientation", orientation);
}

}

#endif // USE(CG)
