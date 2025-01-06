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

#pragma once

#include "Color.h"
#include "DestinationColorSpace.h"
#include "ImageOrientation.h"
#include "ImageTypes.h"
#include "IntPoint.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class BitmapImageSource;
class GraphicsContext;
class ImageDecoder;
class ImageFrame;
class NativeImage;

class BitmapImageDescriptor {
public:
    BitmapImageDescriptor(BitmapImageSource&);

    void clear() { m_cachedFlags = { }; }

    EncodedDataStatus encodedDataStatus() const;
    IntSize size(ImageOrientation = ImageOrientation::Orientation::FromImage) const;
    IntSize sourceSize(ImageOrientation = ImageOrientation::Orientation::FromImage) const;
    std::optional<IntSize> densityCorrectedSize() const;
    ImageOrientation orientation() const;
    unsigned primaryFrameIndex() const;
    unsigned frameCount() const;
    RepetitionCount repetitionCount() const;
    DestinationColorSpace colorSpace() const;
    std::optional<Color> singlePixelSolidColor() const;

    String uti() const;
    String filenameExtension() const;
    String accessibilityDescription() const;
    std::optional<IntPoint> hotSpot() const;
    SubsamplingLevel maximumSubsamplingLevel() const;
    SubsamplingLevel subsamplingLevelForScaleFactor(GraphicsContext&, const FloatSize& scaleFactor, AllowImageSubsampling) const;

#if ENABLE(QUICKLOOK_FULLSCREEN)
    bool shouldUseQuickLookForFullscreen() const;
#endif

#if ENABLE(SPATIAL_IMAGE_DETECTION)
    bool isSpatial() const;
#endif

    void dump(TextStream&) const;

private:
    enum class CachedFlag : uint16_t {
        EncodedDataStatus           = 1 << 0,
        Size                        = 1 << 1,
        DensityCorrectedSize        = 1 << 2,
        Orientation                 = 1 << 3,
        PrimaryFrameIndex           = 1 << 4,
        FrameCount                  = 1 << 5,
        RepetitionCount             = 1 << 6,
        ColorSpace                  = 1 << 7,
        SinglePixelSolidColor       = 1 << 8,

        UTI                         = 1 << 9,
        FilenameExtension           = 1 << 10,
        AccessibilityDescription    = 1 << 11,
        HotSpot                     = 1 << 12,
        MaximumSubsamplingLevel     = 1 << 13,
    };

    template<typename MetadataType>
    MetadataType imageMetadata(MetadataType& cachedValue, const MetadataType& defaultValue, CachedFlag, MetadataType (ImageDecoder::*functor)() const) const;

    template<typename MetadataType>
    MetadataType primaryImageFrameMetadata(MetadataType& cachedValue, CachedFlag, MetadataType (ImageFrame::*functor)() const, const std::optional<SubsamplingLevel>& = std::nullopt) const;

    template<typename MetadataType>
    MetadataType primaryNativeImageMetadata(MetadataType& cachedValue, const MetadataType& defaultValue, CachedFlag, MetadataType (NativeImage::*functor)() const) const;

    mutable OptionSet<CachedFlag> m_cachedFlags;

    mutable EncodedDataStatus m_encodedDataStatus { EncodedDataStatus::Unknown };
    mutable IntSize m_size;
    mutable std::optional<IntSize> m_densityCorrectedSize;
    mutable ImageOrientation m_orientation;
    mutable size_t m_primaryFrameIndex { 0 };
    mutable size_t m_frameCount { 0 };
    mutable RepetitionCount m_repetitionCount { RepetitionCountNone };
    mutable DestinationColorSpace m_colorSpace { DestinationColorSpace::SRGB() };
    mutable std::optional<Color> m_singlePixelSolidColor;

    mutable String m_uti;
    mutable String m_filenameExtension;
    mutable String m_accessibilityDescription;
    mutable std::optional<IntPoint> m_hotSpot;
    mutable SubsamplingLevel m_maximumSubsamplingLevel { SubsamplingLevel::Default };

    BitmapImageSource& m_source;
};

} // namespace WebCore
