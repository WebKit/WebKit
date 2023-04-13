/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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

#include "DecodingOptions.h"
#include "GraphicsTypes.h"
#include "ImageOrientation.h"
#include <wtf/ArgumentCoder.h>

namespace WebCore {

struct ImagePaintingOptions {
    template<typename Type> static constexpr bool isOptionType =
        std::is_same_v<Type, CompositeOperator>
        || std::is_same_v<Type, BlendMode>
        || std::is_same_v<Type, DecodingMode>
        || std::is_same_v<Type, ImageOrientation>
        || std::is_same_v<Type, ImageOrientation::Orientation>
        || std::is_same_v<Type, InterpolationQuality>;

    template<typename First, typename... Rest, typename = std::enable_if_t<isOptionType<std::decay_t<First>>>>
    ImagePaintingOptions(First first, Rest... rest)
    {
        setOption(first, rest...);
    }

    template<typename... Overrides>
    ImagePaintingOptions(const ImagePaintingOptions& other, Overrides... overrides)
        : ImagePaintingOptions(other)
    {
        setOption(overrides...);
    }

    ImagePaintingOptions() = default;
    ImagePaintingOptions(const ImagePaintingOptions&) = default;
    ImagePaintingOptions& operator=(const ImagePaintingOptions&) = default;

    CompositeOperator compositeOperator() const { return m_compositeOperator; }
    BlendMode blendMode() const { return m_blendMode; }
    DecodingMode decodingMode() const { return m_decodingMode; }
    ImageOrientation orientation() const { return m_orientation; }
    InterpolationQuality interpolationQuality() const { return m_interpolationQuality; }

private:
    friend struct IPC::ArgumentCoder<ImagePaintingOptions, void>;
    template <typename First, typename... Rest>
    void setOption(First first, Rest... rest)
    {
        setOption(first);
        setOption(rest...);
    }

    void setOption(CompositeOperator compositeOperator) { m_compositeOperator = compositeOperator; }
    void setOption(BlendMode blendMode) { m_blendMode = blendMode; }
    void setOption(DecodingMode decodingMode) { m_decodingMode = decodingMode; }
    void setOption(ImageOrientation orientation) { m_orientation = orientation; }
    void setOption(ImageOrientation::Orientation orientation) { m_orientation = orientation; }
    void setOption(InterpolationQuality interpolationQuality) { m_interpolationQuality = interpolationQuality; }

    CompositeOperator m_compositeOperator { CompositeOperator::SourceOver };
    BlendMode m_blendMode { BlendMode::Normal };
    DecodingMode m_decodingMode { DecodingMode::Synchronous };
    ImageOrientation m_orientation { ImageOrientation::Orientation::None };
    InterpolationQuality m_interpolationQuality { InterpolationQuality::Default };
};

}
