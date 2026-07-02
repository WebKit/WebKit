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

#pragma once

#include <WebCore/Color.h>
#include <WebCore/DecodingOptions.h>
#include <WebCore/ImageOrientation.h>
#include <WebCore/ImageTypes.h>
#include <WebCore/IntSize.h>
#include <WebCore/NativeImage.h>
#include <wtf/EnumeratedArray.h>
#include <wtf/Seconds.h>

namespace WebCore {

class ImageFrame {
    friend class BitmapImageSource;
    friend class ImageDecoder;
    friend class ImageDecoderCG;
public:
    enum class Caching { Metadata, MetadataAndImage };

    ImageFrame();
    ImageFrame(Ref<NativeImage>&&);
    ImageFrame(const ImageFrame&);

    ~ImageFrame();

    static const ImageFrame& NODELETE defaultFrame();

    ImageFrame& operator=(const ImageFrame&);

    size_t clearImage(std::optional<DecodingDestination> = std::nullopt);
    size_t clear();

    void NODELETE setDecodingStatus(DecodingStatus);
    DecodingStatus NODELETE decodingStatus() const;

    bool isInvalid() const { return m_decodingStatus == DecodingStatus::Invalid; }
    bool isPartial() const { return m_decodingStatus == DecodingStatus::Partial; }
    bool isComplete() const { return m_decodingStatus == DecodingStatus::Complete; }

    void setSize(const IntSize& size) { m_size = size; }
    IntSize size() const { return m_size; }

    size_t sizeInBytes() const;

    void setDensity(const FloatSize& density) { m_density = density; }
    FloatSize density() const { return m_density; }

    void setDensityCorrectedSize(const IntSize& size) { m_densityCorrectedSize = size; }
    std::optional<IntSize> densityCorrectedSize() const { return m_densityCorrectedSize; }

    SubsamplingLevel subsamplingLevel() const { return m_subsamplingLevel; }

    RefPtr<NativeImage> nativeImage(std::optional<DecodingDestination> decodingDestination) const { return destination(decodingDestination).nativeImage; }
    DecodingOptions decodingOptions(std::optional<DecodingDestination> decodingDestination) const { return destination(decodingDestination).decodingOptions; }
    Headroom headroom(std::optional<DecodingDestination> decodingDestination) const { return destination(decodingDestination).headroom; }

    void setOrientation(ImageOrientation orientation) { m_orientation = orientation; };
    ImageOrientation orientation() const { return m_orientation; }

    void setDuration(const Seconds& duration) { m_duration = duration; }
    Seconds duration() const { return m_duration; }

    void setHasAlpha(bool hasAlpha) { m_hasAlpha = hasAlpha; }
    bool hasAlpha() const { return !hasMetadata() || m_hasAlpha; }

    bool hasNativeImage(DecodingDestination decodingDestination) const { return m_destinations[decodingDestination].hasNativeImage(); }
    bool NODELETE hasNativeImage(DecodingDestination, SubsamplingLevel) const;
    bool NODELETE hasFullSizeNativeImage(DecodingDestination, SubsamplingLevel) const;
    std::optional<DecodingDestination> compatibleDecodingDestinationWithOptions(const DecodingOptions&, SubsamplingLevel) const;
    bool hasMetadata() const { return !size().isEmpty(); }

private:
    struct Destination {
        RefPtr<NativeImage> nativeImage;
        DecodingOptions decodingOptions { DecodingMode::Auto };
        Headroom headroom { Headroom::None };

        bool hasNativeImage() const { return nativeImage; }

        bool hasFullSizeNativeImage() const
        {
            return hasNativeImage() && decodingOptions.hasFullSize();
        }

        bool hasDecodedNativeImageCompatibleWithOptions(const DecodingOptions& decodingOptions) const
        {
            return hasNativeImage() && this->decodingOptions.isCompatibleWith(decodingOptions);
        }

        size_t sizeInBytes() const
        {
            if (RefPtr nativeImage = this->nativeImage)
                return nativeImage->sizeInBytes();
            return 0;
        }

        size_t clear()
        {
            if (RefPtr nativeImage = std::exchange(this->nativeImage, nullptr)) {
                nativeImage->clearSubimages();
                decodingOptions = DecodingOptions();
                headroom = Headroom::None;
                return nativeImage->sizeInBytes();
            }
            return 0;
        }
    };

    DecodingDestination decodingDestinationIfExists() const
    {
        if (m_destinations[DecodingDestination::ShouldDecodeToHDR].hasNativeImage())
            return DecodingDestination::ShouldDecodeToHDR;
        if (m_destinations[DecodingDestination::BaseAndGainMap].hasNativeImage())
            return DecodingDestination::BaseAndGainMap;
        return DecodingDestination::Base;
    }

    Destination& destination(std::optional<DecodingDestination> decodingDestination)
    {
        return m_destinations[decodingDestination.value_or(decodingDestinationIfExists())];
    }

    const Destination& destination(std::optional<DecodingDestination> decodingDestination) const
    {
        return m_destinations[decodingDestination.value_or(decodingDestinationIfExists())];
    }

    DecodingStatus m_decodingStatus { DecodingStatus::Invalid };

    IntSize m_size;
    FloatSize m_density;
    std::optional<IntSize> m_densityCorrectedSize;

    SubsamplingLevel m_subsamplingLevel { SubsamplingLevel::Default };

    ImageOrientation m_orientation { ImageOrientation::Orientation::None };
    Seconds m_duration;
    bool m_hasAlpha { true };

    EnumeratedArray<DecodingDestination, Destination, DecodingDestination::ShouldDecodeToHDR> m_destinations;
};

} // namespace WebCore
