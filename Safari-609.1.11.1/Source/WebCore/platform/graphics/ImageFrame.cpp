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
#include "ImageFrame.h"

#include <wtf/NeverDestroyed.h>

namespace WebCore {

ImageFrame::ImageFrame()
{
}

ImageFrame::~ImageFrame()
{
    clearImage();
}

const ImageFrame& ImageFrame::defaultFrame()
{
    static NeverDestroyed<ImageFrame> sharedInstance;
    return sharedInstance;
}

ImageFrame& ImageFrame::operator=(const ImageFrame& other)
{
    if (this == &other)
        return *this;

    m_decodingStatus = other.m_decodingStatus;
    m_size = other.m_size;

    m_nativeImage = other.m_nativeImage;
    m_subsamplingLevel = other.m_subsamplingLevel;
    m_decodingOptions = other.m_decodingOptions;

    m_orientation = other.m_orientation;
    m_duration = other.m_duration;
    m_hasAlpha = other.m_hasAlpha;
    return *this;
}

void ImageFrame::setDecodingStatus(DecodingStatus decodingStatus)
{
    ASSERT(decodingStatus != DecodingStatus::Decoding);
    m_decodingStatus = decodingStatus;
}

DecodingStatus ImageFrame::decodingStatus() const
{
    ASSERT(m_decodingStatus != DecodingStatus::Decoding);
    return m_decodingStatus;
}

unsigned ImageFrame::clearImage()
{
    if (!hasNativeImage())
        return 0;

    unsigned frameBytes = this->frameBytes();

    clearNativeImageSubimages(m_nativeImage);
    m_nativeImage = nullptr;
    m_decodingOptions = DecodingOptions();

    return frameBytes;
}

unsigned ImageFrame::clear()
{
    unsigned frameBytes = clearImage();
    *this = ImageFrame();
    return frameBytes;
}

IntSize ImageFrame::size() const
{
    return m_size;
}
    
bool ImageFrame::hasNativeImage(const Optional<SubsamplingLevel>& subsamplingLevel) const
{
    return m_nativeImage && (!subsamplingLevel || *subsamplingLevel >= m_subsamplingLevel);
}

bool ImageFrame::hasFullSizeNativeImage(const Optional<SubsamplingLevel>& subsamplingLevel) const
{
    return hasNativeImage(subsamplingLevel) && (m_decodingOptions.isSynchronous() || m_decodingOptions.hasFullSize());
}

bool ImageFrame::hasDecodedNativeImageCompatibleWithOptions(const Optional<SubsamplingLevel>& subsamplingLevel, const DecodingOptions& decodingOptions) const
{
    return hasNativeImage(subsamplingLevel) && m_decodingOptions.isAsynchronousCompatibleWith(decodingOptions);
}

Color ImageFrame::singlePixelSolidColor() const
{
    if (!hasNativeImage() || m_size != IntSize(1, 1))
        return Color();

    return nativeImageSinglePixelSolidColor(m_nativeImage);
}

}
