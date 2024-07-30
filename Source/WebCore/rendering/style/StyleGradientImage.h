/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "CSSGradientValue.h"
#include "LengthSize.h"
#include "StyleColor.h"
#include "StyleGeneratedImage.h"
#include <variant>

namespace WebCore {

template<typename Position> struct StyleGradientImageStop {
    std::optional<StyleColor> color;
    Position position;

    bool operator==(const StyleGradientImageStop&) const = default;
};

using StyleGradientImageLengthStop = StyleGradientImageStop<std::optional<Length>>;
using StyleGradientImageAngularStop = StyleGradientImageStop<std::variant<std::monostate, AngleRaw, PercentRaw>>;


// MARK: StyleGradientPosition

struct StyleGradientPosition {
    struct Coordinate {
        Length length;

        bool operator==(const Coordinate&) const = default;
    };

    Coordinate x;
    Coordinate y;

    bool operator==(const StyleGradientPosition&) const = default;
};

// MARK: StyleGradientDeprecatedPoint

struct StyleGradientDeprecatedPoint {
    struct Coordinate {
        std::variant<NumberRaw, PercentRaw> value;

        bool operator==(const Coordinate&) const = default;
    };

    Coordinate x;
    Coordinate y;

    bool operator==(const StyleGradientDeprecatedPoint&) const = default;
};

class StyleGradientImage final : public StyleGeneratedImage {
public:
    struct LinearData {
        using Horizontal = CSSLinearGradientValue::Horizontal;
        using Vertical = CSSLinearGradientValue::Vertical;
        using GradientLine = std::variant<std::monostate, AngleRaw, Horizontal, Vertical, std::pair<Horizontal, Vertical>>;

        GradientLine gradientLine;
        CSSGradientRepeat repeating;
        Vector<StyleGradientImageLengthStop> stops;

        bool operator==(const LinearData&) const = default;
    };
    struct PrefixedLinearData {
        using Horizontal = CSSPrefixedLinearGradientValue::Horizontal;
        using Vertical = CSSPrefixedLinearGradientValue::Vertical;
        using GradientLine = std::variant<std::monostate, AngleRaw, Horizontal, Vertical, std::pair<Horizontal, Vertical>>;

        GradientLine gradientLine;
        CSSGradientRepeat repeating;
        Vector<StyleGradientImageLengthStop> stops;

        bool operator==(const PrefixedLinearData&) const = default;
    };
    struct DeprecatedLinearData {
        StyleGradientDeprecatedPoint first;
        StyleGradientDeprecatedPoint second;
        Vector<StyleGradientImageLengthStop> stops;

        bool operator==(const DeprecatedLinearData&) const = default;
    };
    struct RadialData {
        using ShapeKeyword = CSSRadialGradientValue::ShapeKeyword;
        using ExtentKeyword = CSSRadialGradientValue::ExtentKeyword;

        struct Shape {
            ShapeKeyword shape;
            std::optional<StyleGradientPosition> position;
            bool operator==(const Shape&) const = default;
        };
        struct Extent {
            ExtentKeyword extent;
            std::optional<StyleGradientPosition> position;
            bool operator==(const Extent&) const = default;
        };
        struct Length {
            WebCore::Length length; // <length [0,∞]>
            std::optional<StyleGradientPosition> position;
            bool operator==(const Length&) const = default;
        };
        struct CircleOfLength {
            WebCore::Length length; // <length [0,∞]>
            std::optional<StyleGradientPosition> position;
            bool operator==(const CircleOfLength&) const = default;
        };
        struct CircleOfExtent {
            ExtentKeyword extent;
            std::optional<StyleGradientPosition> position;
            bool operator==(const CircleOfExtent&) const = default;
        };
        struct Size {
            LengthSize size; // <length-percentage [0,∞]>, <length-percentage [0,∞]>
            std::optional<StyleGradientPosition> position;
            bool operator==(const Size&) const = default;
        };
        struct EllipseOfSize {
            LengthSize size; // <length-percentage [0,∞]>, <length-percentage [0,∞]>
            std::optional<StyleGradientPosition> position;
            bool operator==(const EllipseOfSize&) const = default;
        };
        struct EllipseOfExtent {
            ExtentKeyword extent;
            std::optional<StyleGradientPosition> position;
            bool operator==(const EllipseOfExtent&) const = default;
        };
        using GradientBox = std::variant<std::monostate, Shape, Extent, Length, Size, CircleOfLength, CircleOfExtent, EllipseOfSize, EllipseOfExtent, StyleGradientPosition>;

        GradientBox gradientBox;
        CSSGradientRepeat repeating;
        Vector<StyleGradientImageLengthStop> stops;

        bool operator==(const RadialData&) const = default;
    };
    struct PrefixedRadialData {
        using ShapeKeyword = CSSPrefixedRadialGradientValue::ShapeKeyword;
        using ExtentKeyword = CSSPrefixedRadialGradientValue::ExtentKeyword;
        using ShapeAndExtent = CSSPrefixedRadialGradientValue::ShapeAndExtent;
        struct MeasuredSize {
            LengthSize size; // <length-percentage [0,∞]>, <length-percentage [0,∞]>
            bool operator==(const MeasuredSize&) const = default;
        };
        using GradientBox = std::variant<std::monostate, ShapeKeyword, ExtentKeyword, ShapeAndExtent, MeasuredSize>;

        GradientBox gradientBox;
        std::optional<StyleGradientPosition> position;
        CSSGradientRepeat repeating;
        Vector<StyleGradientImageLengthStop> stops;

        bool operator==(const PrefixedRadialData&) const = default;
    };
    struct DeprecatedRadialData {
        StyleGradientDeprecatedPoint first;
        StyleGradientDeprecatedPoint second;
        float firstRadius;
        float secondRadius;
        Vector<StyleGradientImageLengthStop> stops;

        bool operator==(const DeprecatedRadialData&) const = default;
    };
    struct ConicData {
        std::optional<AngleRaw> angle;
        std::optional<StyleGradientPosition> position;
        CSSGradientRepeat repeating;
        Vector<StyleGradientImageAngularStop> stops;

        bool operator==(const ConicData&) const = default;
    };

    using Data = std::variant<LinearData, DeprecatedLinearData, PrefixedLinearData, RadialData, DeprecatedRadialData, PrefixedRadialData, ConicData>;

    static Ref<StyleGradientImage> create(Data data, CSSGradientColorInterpolationMethod colorInterpolationMethod)
    {
        return adoptRef(*new StyleGradientImage(WTFMove(data), colorInterpolationMethod));
    }
    virtual ~StyleGradientImage();

    bool operator==(const StyleImage&) const final;
    bool equals(const StyleGradientImage&) const;

    static constexpr bool isFixedSize = false;

private:
    explicit StyleGradientImage(Data&&, CSSGradientColorInterpolationMethod);

    Ref<CSSValue> computedStyleValue(const RenderStyle&) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    RefPtr<Image> image(const RenderElement*, const FloatSize&, bool isForFirstLine) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    FloatSize fixedSize(const RenderElement&) const final;
    void didAddClient(RenderElement&) final { }
    void didRemoveClient(RenderElement&) final { }

    Ref<Gradient> createGradient(const LinearData&, const FloatSize&, const RenderStyle&) const;
    Ref<Gradient> createGradient(const PrefixedLinearData&, const FloatSize&, const RenderStyle&) const;
    Ref<Gradient> createGradient(const DeprecatedLinearData&, const FloatSize&, const RenderStyle&) const;
    Ref<Gradient> createGradient(const RadialData&, const FloatSize&, const RenderStyle&) const;
    Ref<Gradient> createGradient(const PrefixedRadialData&, const FloatSize&, const RenderStyle&) const;
    Ref<Gradient> createGradient(const DeprecatedRadialData&, const FloatSize&, const RenderStyle&) const;
    Ref<Gradient> createGradient(const ConicData&, const FloatSize&, const RenderStyle&) const;

    template<typename GradientAdapter, typename Stops> GradientColorStops computeStops(GradientAdapter&, const Stops&, const RenderStyle&, float maxLengthForRepeat, CSSGradientRepeat) const;
    template<typename GradientAdapter, typename Stops> GradientColorStops computeStopsForDeprecatedVariants(GradientAdapter&, const Stops&, const RenderStyle&) const;

    Data m_data;
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    bool m_knownCacheableBarringFilter { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleGradientImage, isGradientImage)
