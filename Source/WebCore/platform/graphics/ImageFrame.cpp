/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

ImageFrame::ImageFrame(Ref<NativeImage>&& nativeImage)
{
    m_size = nativeImage->size();
    m_hasAlpha = nativeImage->hasAlpha();

    m_source.headroom = nativeImage->headroom();
    m_source.nativeImage = WTFMove(nativeImage);
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
    m_densityCorrectedSize = other.m_densityCorrectedSize;
    m_subsamplingLevel = other.m_subsamplingLevel;

    m_orientation = other.m_orientation;
    m_duration = other.m_duration;
    m_hasAlpha = other.m_hasAlpha;

    m_source = other.m_source;
    m_hdrSource = other.m_hdrSource;
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

unsigned ImageFrame::clearSourceImage(ShouldDecodeToHDR shouldDecodeToHDR)
{
    auto& source = this->source(shouldDecodeToHDR);
    if (!source.hasNativeImage())
        return 0;

    source.clear();
    return sizeInBytes();
}

unsigned ImageFrame::clearImage(std::optional<ShouldDecodeToHDR> shouldDecodeToHDR)
{

    unsigned frameBytes = 0;
    if (!shouldDecodeToHDR || *shouldDecodeToHDR == ShouldDecodeToHDR::No)
        frameBytes += clearSourceImage(ShouldDecodeToHDR::No);

    if (!shouldDecodeToHDR || *shouldDecodeToHDR == ShouldDecodeToHDR::Yes)
        frameBytes += clearSourceImage(ShouldDecodeToHDR::Yes);

    return frameBytes;
}

unsigned ImageFrame::clear()
{
    unsigned frameBytes = clearImage();
    *this = ImageFrame();
    return frameBytes;
}

bool ImageFrame::hasNativeImage(ShouldDecodeToHDR shouldDecodeToHDR, SubsamplingLevel subsamplingLevel) const
{
    return source(shouldDecodeToHDR).hasNativeImage() && subsamplingLevel >= m_subsamplingLevel;
}

bool ImageFrame::hasFullSizeNativeImage(ShouldDecodeToHDR shouldDecodeToHDR, SubsamplingLevel subsamplingLevel) const
{
    return source(shouldDecodeToHDR).hasFullSizeNativeImage() && subsamplingLevel >= m_subsamplingLevel;
}

bool ImageFrame::hasDecodedNativeImageCompatibleWithOptions(const DecodingOptions& decodingOptions, SubsamplingLevel subsamplingLevel) const
{
    return isComplete() && source(decodingOptions.shouldDecodeToHDR()).hasDecodedNativeImageCompatibleWithOptions(decodingOptions) && subsamplingLevel >= m_subsamplingLevel;
}

} // namespace WebCore
