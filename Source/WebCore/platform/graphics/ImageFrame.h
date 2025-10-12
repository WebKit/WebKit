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

#pragma once

#include <WebCore/Color.h>
#include <WebCore/DecodingOptions.h>
#include <WebCore/ImageOrientation.h>
#include <WebCore/ImageTypes.h>
#include <WebCore/IntSize.h>
#include <WebCore/NativeImage.h>
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
    ImageFrame(const ImageFrame& other) { operator=(other); }

    ~ImageFrame();

    static const ImageFrame& defaultFrame();

    ImageFrame& operator=(const ImageFrame& other);

    unsigned clearSourceImage(ShouldDecodeToHDR);
    unsigned clearImage(std::optional<ShouldDecodeToHDR> = std::nullopt);
    unsigned clear();

    void setDecodingStatus(DecodingStatus);
    DecodingStatus decodingStatus() const;

    bool isInvalid() const { return m_decodingStatus == DecodingStatus::Invalid; }
    bool isPartial() const { return m_decodingStatus == DecodingStatus::Partial; }
    bool isComplete() const { return m_decodingStatus == DecodingStatus::Complete; }

    void setSize(const IntSize& size) { m_size = size; }
    IntSize size() const { return m_size; }

    unsigned sizeInBytes() const { return (size().area() * sizeof(uint32_t)).value(); }

    void setDensityCorrectedSize(const IntSize& size) { m_densityCorrectedSize = size; }
    std::optional<IntSize> densityCorrectedSize() const { return m_densityCorrectedSize; }

    SubsamplingLevel subsamplingLevel() const { return m_subsamplingLevel; }

    RefPtr<NativeImage> nativeImage(std::optional<ShouldDecodeToHDR> shouldDecodeToHDR) const { return source(shouldDecodeToHDR).nativeImage; }
    DecodingOptions decodingOptions(std::optional<ShouldDecodeToHDR> shouldDecodeToHDR) const { return source(shouldDecodeToHDR).decodingOptions; }
    Headroom headroom(std::optional<ShouldDecodeToHDR> shouldDecodeToHDR) const { return source(shouldDecodeToHDR).headroom; }

    void setOrientation(ImageOrientation orientation) { m_orientation = orientation; };
    ImageOrientation orientation() const { return m_orientation; }

    void setDuration(const Seconds& duration) { m_duration = duration; }
    Seconds duration() const { return m_duration; }

    void setHasAlpha(bool hasAlpha) { m_hasAlpha = hasAlpha; }
    bool hasAlpha() const { return !hasMetadata() || m_hasAlpha; }

    bool hasNativeImage(ShouldDecodeToHDR shouldDecodeToHDR) const { return source(shouldDecodeToHDR).hasNativeImage(); }
    bool hasNativeImage(ShouldDecodeToHDR, SubsamplingLevel) const;
    bool hasFullSizeNativeImage(ShouldDecodeToHDR, SubsamplingLevel) const;
    bool hasDecodedNativeImageCompatibleWithOptions(const DecodingOptions&, SubsamplingLevel) const;
    bool hasMetadata() const { return !size().isEmpty(); }

private:
    struct Source {
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

        void clear()
        {
            if (!nativeImage)
                return;

            nativeImage->clearSubimages();
            nativeImage = nullptr;
            decodingOptions = DecodingOptions();
            headroom = Headroom::None;
        }
    };

    ShouldDecodeToHDR shouldDecodeToHDRIfExists() const
    {
        return hasNativeImage(ShouldDecodeToHDR::Yes) ? ShouldDecodeToHDR::Yes : ShouldDecodeToHDR::No;
    }

    Source& source(std::optional<ShouldDecodeToHDR> shouldDecodeToHDR)
    {
        if (shouldDecodeToHDR)
            return *shouldDecodeToHDR == ShouldDecodeToHDR::No ? m_source : m_hdrSource;

        return shouldDecodeToHDRIfExists() == ShouldDecodeToHDR::No ? m_source : m_hdrSource;
    }

    const Source& source(std::optional<ShouldDecodeToHDR> shouldDecodeToHDR) const
    {
        if (shouldDecodeToHDR)
            return *shouldDecodeToHDR == ShouldDecodeToHDR::No ? m_source : m_hdrSource;

        return shouldDecodeToHDRIfExists() == ShouldDecodeToHDR::No ? m_source : m_hdrSource;
    }

    DecodingStatus m_decodingStatus { DecodingStatus::Invalid };

    IntSize m_size;
    std::optional<IntSize> m_densityCorrectedSize;

    SubsamplingLevel m_subsamplingLevel { SubsamplingLevel::Default };

    ImageOrientation m_orientation { ImageOrientation::Orientation::None };
    Seconds m_duration;
    bool m_hasAlpha { true };

    Source m_source;
    Source m_hdrSource;
};

} // namespace WebCore
