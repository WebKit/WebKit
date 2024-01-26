/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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
#include "BitmapImageMetadata.h"

#include "ImageDecoder.h"
#include "ImageSource.h"

namespace WebCore {

BitmapImageMetadata::BitmapImageMetadata(ImageSource& source)
    : m_source(source)
{
}

template<typename MetadataType>
MetadataType BitmapImageMetadata::imageMetadata(MetadataType& cachedValue, const MetadataType& defaultValue, CachedFlag cachedFlag, MetadataType (ImageDecoder::*functor)() const) const
{
    if (m_cachedFlags.contains(cachedFlag))
        return cachedValue;

    auto decoder = source().m_decoder;
    if (!decoder || !decoder->isSizeAvailable())
        return defaultValue;

    cachedValue = (*decoder.*functor)();
    m_cachedFlags.add(cachedFlag);
    source().didDecodeProperties(decoder->bytesDecodedToDetermineProperties());
    return cachedValue;
}

template<typename MetadataType>
MetadataType BitmapImageMetadata::primaryNativeImageMetadata(MetadataType& cachedValue, const MetadataType& defaultValue, CachedFlag cachedFlag, MetadataType (NativeImage::*functor)() const) const
{
    if (m_cachedFlags.contains(cachedFlag))
        return cachedValue;

    auto nativeImage = const_cast<ImageSource&>(source()).frameImageAtIndexCacheIfNeeded(primaryFrameIndex());
    if (!nativeImage)
        return defaultValue;

    cachedValue = (*nativeImage.*functor)();
    m_cachedFlags.add(cachedFlag);
    return cachedValue;
}

template<typename MetadataType>
MetadataType BitmapImageMetadata::primaryImageFrameMetadata(MetadataType& cachedValue, CachedFlag cachedFlag, MetadataType (ImageFrame::*functor)() const) const
{
    if (m_cachedFlags.contains(cachedFlag))
        return cachedValue;

    auto& frame = const_cast<ImageSource&>(source()).frameAtIndexCacheIfNeeded(primaryFrameIndex(), ImageFrame::Caching::Metadata);

    // Don't cache any unavailable frame Metadata.
    if (!frame.hasMetadata())
        return (frame.*functor)();

    cachedValue = (frame.*functor)();
    m_cachedFlags.add(cachedFlag);
    return cachedValue;
}

EncodedDataStatus BitmapImageMetadata::encodedDataStatus() const
{
    return imageMetadata(m_encodedDataStatus, EncodedDataStatus::Unknown, CachedFlag::EncodedDataStatus, &ImageDecoder::encodedDataStatus);
}

IntSize BitmapImageMetadata::size() const
{
    return primaryImageFrameMetadata(m_size, CachedFlag::Size, &ImageFrame::size);
}

std::optional<IntSize> BitmapImageMetadata::densityCorrectedSize() const
{
    return primaryImageFrameMetadata(m_densityCorrectedSize, CachedFlag::DensityCorrectedSize, &ImageFrame::densityCorrectedSize);
}

ImageOrientation BitmapImageMetadata::orientation() const
{
    return primaryImageFrameMetadata(m_orientation, CachedFlag::Orientation, &ImageFrame::orientation);
}

unsigned BitmapImageMetadata::primaryFrameIndex() const
{
    return imageMetadata(m_primaryFrameIndex, std::size_t(0), CachedFlag::PrimaryFrameIndex, &ImageDecoder::primaryFrameIndex);
}

unsigned BitmapImageMetadata::frameCount() const
{
    return imageMetadata(m_frameCount, std::size_t(0), CachedFlag::FrameCount, &ImageDecoder::frameCount);
}

RepetitionCount BitmapImageMetadata::repetitionCount() const
{
    return imageMetadata(m_repetitionCount, static_cast<RepetitionCount>(RepetitionCountNone), CachedFlag::RepetitionCount, &ImageDecoder::repetitionCount);
}

DestinationColorSpace BitmapImageMetadata::colorSpace() const
{
    return primaryNativeImageMetadata(m_colorSpace, DestinationColorSpace::SRGB(), CachedFlag::ColorSpace, &NativeImage::colorSpace);
}

std::optional<Color> BitmapImageMetadata::singlePixelSolidColor() const
{
    return primaryNativeImageMetadata(m_singlePixelSolidColor, std::optional<Color>(), CachedFlag::SinglePixelSolidColor, &NativeImage::singlePixelSolidColor);
}

String BitmapImageMetadata::uti() const
{
#if USE(CG)
    return imageMetadata(m_uti, String(), CachedFlag::UTI, &ImageDecoder::uti);
#else
    return String();
#endif
}

String BitmapImageMetadata::filenameExtension() const
{
    return imageMetadata(m_filenameExtension, String(), CachedFlag::FilenameExtension, &ImageDecoder::filenameExtension);
}

String BitmapImageMetadata::accessibilityDescription() const
{
    return imageMetadata(m_accessibilityDescription, String(), CachedFlag::AccessibilityDescription, &ImageDecoder::accessibilityDescription);
}

std::optional<IntPoint> BitmapImageMetadata::hotSpot() const
{
    return imageMetadata(m_hotSpot, std::optional<IntPoint>(), CachedFlag::HotSpot, &ImageDecoder::hotSpot);
}

SubsamplingLevel BitmapImageMetadata::maximumSubsamplingLevel() const
{
    if (m_cachedFlags.contains(CachedFlag::MaximumSubsamplingLevel))
        return m_maximumSubsamplingLevel;

    auto decoder = source().m_decoder;
    if (!decoder)
        return SubsamplingLevel::Default;

    // FIXME: this value was chosen to be appropriate for iOS since the image
    // subsampling is only enabled by default on iOS. Choose a different value
    // if image subsampling is enabled on other platform.
    static constexpr int maximumImageAreaBeforeSubsampling = 5 * 1024 * 1024;
    auto level = SubsamplingLevel::First;

    for (; level < SubsamplingLevel::Last; ++level) {
        if (source().frameSizeAtIndex(0, level).area() < maximumImageAreaBeforeSubsampling)
            break;
    }

    m_maximumSubsamplingLevel = level;
    m_cachedFlags.add(CachedFlag::MaximumSubsamplingLevel);
    return m_maximumSubsamplingLevel;
}

void BitmapImageMetadata::dump(TextStream& ts) const
{
    ts.dumpProperty("size", size());
    ts.dumpProperty("density-corrected-size", densityCorrectedSize());
    ts.dumpProperty("primary-frame-index", primaryFrameIndex());
    ts.dumpProperty("frame-count", frameCount());
    ts.dumpProperty("repetition-count", repetitionCount());

    ts.dumpProperty("uti", uti());
    ts.dumpProperty("filename-extension", filenameExtension());
    ts.dumpProperty("accessibility-description", accessibilityDescription());
}

} // namespace WebCore
