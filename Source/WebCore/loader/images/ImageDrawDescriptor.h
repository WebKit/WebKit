/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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

#include <WebCore/ConcreteObjectSize.h>
#include <WebCore/DecodingOptions.h>
#include <WebCore/GraphicsTypes.h>
#include <WebCore/ImageOrientation.h>
#include <WebCore/ImageRenditionOptions.h>
#include <WebCore/ImageTypes.h>
#include <WebCore/RasterDrawingOptions.h>
#include <wtf/Forward.h>

namespace WebCore {

// Everything a caller tells an Image about a draw, other than where to draw.
struct ImageDrawDescriptor {
    template<typename Type> static constexpr bool isOptionType =
        std::is_same_v<Type, ConcreteObjectSize>
        || std::is_same_v<Type, ImageRenditionOptions>
        || std::is_same_v<Type, CompositeOperator>
        || std::is_same_v<Type, BlendMode>
        || std::is_same_v<Type, DecodingMode>
        || std::is_same_v<Type, ImageOrientation>
        || std::is_same_v<Type, ImageOrientation::Orientation>
        || std::is_same_v<Type, InterpolationQuality>
        || std::is_same_v<Type, AllowImageSubsampling>
        || std::is_same_v<Type, DrawLuminanceMask>
#if ENABLE(AX_CUSTOM_COLOR_MODE)
        || std::is_same_v<Type, InvertContent>
#endif
#if USE(SKIA)
        || std::is_same_v<Type, StrictImageClamping>
#endif
        || std::is_same_v<Type, ShowDebugBackground>
        || std::is_same_v<Type, IgnoreRootPreserveAspectRatio>
        || std::is_same_v<Type, DrawsHDRContent>
        || std::is_same_v<Type, AllowAcceleratedApplyGainMap>
        || std::is_same_v<Type, Headroom>
        || std::is_same_v<Type, PlatformDynamicRangeLimit>;

    template<typename... Options>
        requires (isOptionType<std::decay_t<Options>> && ...)
    explicit ImageDrawDescriptor(ConcreteObjectSize concreteObjectSize, Options... options)
        : concreteObjectSize(concreteObjectSize)
    {
        (setOption(options), ...);
    }

    template<typename... Overrides>
        requires (isOptionType<std::decay_t<Overrides>> && ...)
    ImageDrawDescriptor(const ImageDrawDescriptor& other, Overrides... overrides)
        : ImageDrawDescriptor(other)
    {
        (setOption(overrides), ...);
    }

    ImageDrawDescriptor(const ImageDrawDescriptor&) = default;
    ImageDrawDescriptor(ImageDrawDescriptor&&) = default;
    ImageDrawDescriptor& operator=(const ImageDrawDescriptor&) = default;
    ImageDrawDescriptor& operator=(ImageDrawDescriptor&&) = default;

    WEBCORE_EXPORT RasterDrawingOptions rasterDrawingOptions() const;

    ConcreteObjectSize concreteObjectSize;
    ImageRenditionOptions rendition;

    CompositeOperator compositeOperator { CompositeOperator::SourceOver };
    BlendMode blendMode { BlendMode::Normal };
    ImageOrientation orientation { ImageOrientation::Orientation::FromImage };
#if USE(SKIA)
    StrictImageClamping strictImageClamping { StrictImageClamping::Yes };
#endif

    DecodingMode decodingMode { DecodingMode::Synchronous };
    AllowImageSubsampling allowImageSubsampling { AllowImageSubsampling::No };
    AllowAcceleratedApplyGainMap allowAcceleratedApplyGainMap { AllowAcceleratedApplyGainMap::No };
    ShowDebugBackground showDebugBackground { ShowDebugBackground::No };

    DrawLuminanceMask drawLuminanceMask { DrawLuminanceMask::No };

private:
    void setOption(ConcreteObjectSize size) { concreteObjectSize = size; }
    void setOption(const ImageRenditionOptions& option) { rendition = option; }
    void setOption(CompositeOperator option) { compositeOperator = option; }
    void setOption(BlendMode option) { blendMode = option; }
    void setOption(DecodingMode option) { decodingMode = option; }
    void setOption(ImageOrientation option) { orientation = option; }
    void setOption(ImageOrientation::Orientation option) { orientation = option; }
    void setOption(InterpolationQuality option) { rendition.interpolationQuality = option; }
    void setOption(AllowImageSubsampling option) { allowImageSubsampling = option; }
    void setOption(DrawLuminanceMask option) { drawLuminanceMask = option; }
#if ENABLE(AX_CUSTOM_COLOR_MODE)
    void setOption(InvertContent option) { rendition.invertContent = option; }
#endif
#if USE(SKIA)
    void setOption(StrictImageClamping option) { strictImageClamping = option; }
#endif
    void setOption(ShowDebugBackground option) { showDebugBackground = option; }
    void setOption(IgnoreRootPreserveAspectRatio option) { rendition.ignoreRootPreserveAspectRatio = option; }
    void setOption(DrawsHDRContent option) { rendition.drawsHDRContent = option; }
    void setOption(AllowAcceleratedApplyGainMap option) { allowAcceleratedApplyGainMap = option; }
    void setOption(Headroom option) { rendition.headroom = option; }
    void setOption(PlatformDynamicRangeLimit option) { rendition.dynamicRangeLimit = option; }
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const ImageDrawDescriptor&);

} // namespace WebCore
