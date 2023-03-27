/*
 * Copyright (C) 2016-2023 Apple Inc.  All rights reserved.
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

ImageFrame::ImageFrame(ImageFrame&& other)
    : m_decodingStatus(other.m_decodingStatus)
    , m_size(other.m_size)
    , m_nativeImage(WTFMove(other.m_nativeImage))
    , m_cachedDecodedImage(WTFMove(other.m_cachedDecodedImage))
    , m_subsamplingLevel(other.m_subsamplingLevel)
    , m_decodingOptions(other.m_decodingOptions)
    , m_orientation(other.m_orientation)
    , m_densityCorrectedSize(other.m_densityCorrectedSize)
    , m_duration(other.m_duration)
    , m_hasAlpha(other.m_hasAlpha)
{
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

unsigned ImageFrame::clearImageAndCacheDecodedData()
{
    if (!hasNativeImage())
        return 0;

    ASSERT(!m_cachedDecodedImage);

    m_cachedDecodedImage = m_nativeImage->decodedImage().createCachedDecodedImage();
    if (!m_cachedDecodedImage)
        return 0;

    unsigned frameBytes = this->frameBytes();

    m_nativeImage->clearSubimages();
    m_nativeImage = nullptr;

    return frameBytes;
}

unsigned ImageFrame::clearImage()
{
    m_decodingOptions = DecodingOptions();

    unsigned frameBytes = 0;

    if (m_nativeImage) {
        frameBytes = this->frameBytes();
        m_nativeImage->clearSubimages();
        m_nativeImage = nullptr;
    }

    m_cachedDecodedImage.reset();

    return frameBytes;
}

unsigned ImageFrame::clear()
{
    unsigned frameBytes = clearImage();

    m_decodingStatus = DecodingStatus::Invalid;
    m_size = { };
    m_nativeImage = nullptr;
    m_cachedDecodedImage = nullptr;
    m_subsamplingLevel = SubsamplingLevel::Default;
    m_decodingOptions = { };
    m_orientation = ImageOrientation::Orientation::None;
    m_densityCorrectedSize = std::nullopt;
    m_duration = { };
    m_hasAlpha = true;

    return frameBytes;
}

IntSize ImageFrame::size() const
{
    return m_size;
}
    
bool ImageFrame::hasNativeImage(const std::optional<SubsamplingLevel>& subsamplingLevel) const
{
    return m_nativeImage && (!subsamplingLevel || *subsamplingLevel >= m_subsamplingLevel);
}

bool ImageFrame::hasFullSizeNativeImage(const std::optional<SubsamplingLevel>& subsamplingLevel) const
{
    return hasNativeImage(subsamplingLevel) && m_decodingOptions.hasFullSize();
}

bool ImageFrame::hasDecodedNativeImageCompatibleWithOptions(const std::optional<SubsamplingLevel>& subsamplingLevel, const DecodingOptions& decodingOptions) const
{
    return hasNativeImage(subsamplingLevel) && m_decodingOptions.isCompatibleWith(decodingOptions);
}

bool ImageFrame::hasCachedDecodedImage(const std::optional<SubsamplingLevel>& subsamplingLevel) const
{
    return m_cachedDecodedImage && (!subsamplingLevel || *subsamplingLevel >= m_subsamplingLevel);
}

bool ImageFrame::hasCachedDecodedImageCompatibleWithOptions(const std::optional<SubsamplingLevel>& subsamplingLevel, const DecodingOptions& decodingOptions) const
{
    return hasCachedDecodedImage(subsamplingLevel) && m_decodingOptions.isCompatibleWith(decodingOptions);
}

Color ImageFrame::singlePixelSolidColor() const
{
    if (!hasNativeImage() || m_size != IntSize(1, 1))
        return Color();

    return m_nativeImage->singlePixelSolidColor();
}

}
