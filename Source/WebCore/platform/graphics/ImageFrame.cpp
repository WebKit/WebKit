/*
 * Copyright (C) 2016-2026 Apple Inc. All rights reserved.
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

ImageFrame::ImageFrame() = default;

ImageFrame::ImageFrame(Ref<NativeImage>&& nativeImage)
{
    m_size = nativeImage->size();
    m_hasAlpha = nativeImage->hasAlpha();

    m_destinations[DecodingDestination::Base].headroom = nativeImage->headroom();
    m_destinations[DecodingDestination::Base].nativeImage = WTF::move(nativeImage);
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

ImageFrame::ImageFrame(const ImageFrame&) = default;
ImageFrame& ImageFrame::operator=(const ImageFrame&) = default;

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

size_t ImageFrame::sizeInBytes() const
{
    CheckedSize sizeInBytes = 0;

    for (auto& destination : m_destinations)
        sizeInBytes += destination.sizeInBytes();

    return sizeInBytes;
}

size_t ImageFrame::clearImage(std::optional<DecodingDestination> decodingDestination)
{
    CheckedSize sizeInBytes = 0;

    if (!decodingDestination || *decodingDestination == DecodingDestination::Base)
        sizeInBytes += destination(DecodingDestination::Base).clear();

    if (!decodingDestination || *decodingDestination == DecodingDestination::BaseAndGainMap)
        sizeInBytes += destination(DecodingDestination::BaseAndGainMap).clear();

    if (!decodingDestination || *decodingDestination == DecodingDestination::ShouldDecodeToHDR)
        sizeInBytes += destination(DecodingDestination::ShouldDecodeToHDR).clear();

    return sizeInBytes;
}

size_t ImageFrame::clear()
{
    auto sizeInBytes = clearImage();
    *this = ImageFrame();
    return sizeInBytes;
}

bool ImageFrame::hasNativeImage(DecodingDestination decodingDestination, SubsamplingLevel subsamplingLevel) const
{
    return destination(decodingDestination).hasNativeImage() && subsamplingLevel >= m_subsamplingLevel;
}

bool ImageFrame::hasFullSizeNativeImage(DecodingDestination decodingDestination, SubsamplingLevel subsamplingLevel) const
{
    return destination(decodingDestination).hasFullSizeNativeImage() && subsamplingLevel >= m_subsamplingLevel;
}

std::optional<DecodingDestination> ImageFrame::compatibleDecodingDestinationWithOptions(const DecodingOptions& decodingOptions, SubsamplingLevel subsamplingLevel) const
{
    if (!isComplete() || subsamplingLevel < m_subsamplingLevel)
        return std::nullopt;

    for (auto& destination : m_destinations) {
        if (destination.hasDecodedNativeImageCompatibleWithOptions(decodingOptions))
            return destination.decodingOptions.decodingDestination();
    }

    return std::nullopt;
}

} // namespace WebCore
