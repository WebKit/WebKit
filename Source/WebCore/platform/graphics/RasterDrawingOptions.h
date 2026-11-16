/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/GraphicsTypes.h>
#include <WebCore/ImageOrientation.h>
#include <WebCore/ImageTypes.h>
#include <WebCore/PlatformDynamicRangeLimit.h>
#include <initializer_list>
#include <wtf/Forward.h>

namespace WebCore {

struct RasterDrawingOptions {
    template<typename Type> static constexpr bool isOptionType =
        std::is_same_v<Type, CompositeOperator>
        || std::is_same_v<Type, BlendMode>
        || std::is_same_v<Type, ImageOrientation>
        || std::is_same_v<Type, ImageOrientation::Orientation>
        || std::is_same_v<Type, InterpolationQuality>
#if ENABLE(AX_CUSTOM_COLOR_MODE)
        || std::is_same_v<Type, InvertContent>
#endif
#if USE(SKIA)
        || std::is_same_v<Type, StrictImageClamping>
#endif
        || std::is_same_v<Type, DrawsHDRContent>
        || std::is_same_v<Type, Headroom>
        || std::is_same_v<Type, PlatformDynamicRangeLimit>;

    template<typename T>
        requires isOptionType<std::decay_t<T>>
    RasterDrawingOptions(std::initializer_list<T> options)
    {
        for (auto& option : options)
            setOption(option);
    }

    template<typename T>
        requires isOptionType<std::decay_t<T>>
    explicit RasterDrawingOptions(T option)
    {
        setOption(option);
    }

    template<typename T, typename U, typename... Rest>
        requires isOptionType<std::decay_t<T>>
    RasterDrawingOptions(T first, U second, Rest... rest)
    {
        setOption(first);
        setOption(second);
        (setOption(rest), ...);
    }

    template<typename... Overrides>
    RasterDrawingOptions(const RasterDrawingOptions& other, Overrides... overrides)
        : RasterDrawingOptions(other)
    {
        (setOption(overrides), ...);
    }

    RasterDrawingOptions() = default;
    RasterDrawingOptions(const RasterDrawingOptions&) = default;
    RasterDrawingOptions(RasterDrawingOptions&&) = default;
    RasterDrawingOptions& operator=(const RasterDrawingOptions&) = default;
    RasterDrawingOptions& operator=(RasterDrawingOptions&&) = default;

    CompositeOperator compositeOperator() const { return m_compositeOperator; }
    BlendMode blendMode() const { return m_blendMode; }
    ImageOrientation orientation() const { return m_orientation; }
    InterpolationQuality interpolationQuality() const { return m_interpolationQuality; }
#if ENABLE(AX_CUSTOM_COLOR_MODE)
    InvertContent invertContent() const { return m_invertContent; }
#endif
#if USE(SKIA)
    StrictImageClamping strictImageClamping() const { return m_strictImageClamping; }
#endif
    DrawsHDRContent drawsHDRContent() const { return m_drawsHDRContent; }
    Headroom headroom() const { return m_headroom; }
    PlatformDynamicRangeLimit dynamicRangeLimit() const { return m_dynamicRangeLimit; }

private:
    void setOption(CompositeOperator compositeOperator) { m_compositeOperator = compositeOperator; }
    void setOption(BlendMode blendMode) { m_blendMode = blendMode; }
    void setOption(ImageOrientation orientation) { m_orientation = orientation.orientation(); }
    void setOption(ImageOrientation::Orientation orientation) { m_orientation = orientation; }
    void setOption(InterpolationQuality interpolationQuality) { m_interpolationQuality = interpolationQuality; }
#if ENABLE(AX_CUSTOM_COLOR_MODE)
    void setOption(InvertContent invertContent) { m_invertContent = invertContent; }
#endif
#if USE(SKIA)
    void setOption(StrictImageClamping strictImageClamping) { m_strictImageClamping = strictImageClamping; }
#endif
    void setOption(DrawsHDRContent drawsHDRContent) { m_drawsHDRContent = drawsHDRContent; }
    void setOption(Headroom headroom) { m_headroom = headroom; }
    void setOption(PlatformDynamicRangeLimit dynamicRangeLimit) { m_dynamicRangeLimit = dynamicRangeLimit; }

    CompositeOperator m_compositeOperator : 4 { CompositeOperator::SourceOver };
    BlendMode m_blendMode : 5 { BlendMode::Normal };
    ImageOrientation::Orientation m_orientation : 4 { ImageOrientation::Orientation::None };
    InterpolationQuality m_interpolationQuality : 4 { InterpolationQuality::Default };
#if ENABLE(AX_CUSTOM_COLOR_MODE)
    InvertContent m_invertContent : 2 { InvertContent::FromResource };
#endif
#if USE(SKIA)
    StrictImageClamping m_strictImageClamping : 1 { StrictImageClamping::Yes };
#endif
    DrawsHDRContent m_drawsHDRContent : 1 { DrawsHDRContent::No };
    Headroom m_headroom { Headroom::FromImage };
    PlatformDynamicRangeLimit m_dynamicRangeLimit { PlatformDynamicRangeLimit::initialValue() };
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, RasterDrawingOptions);

} // namespace WebCore
