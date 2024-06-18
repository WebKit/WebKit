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
#include "Length.h"
#include "StyleColor.h"
#include "StyleGeneratedImage.h"
#include <variant>

namespace WebCore {

template<typename Position>
struct StyleGradientImageStop {
    std::optional<StyleColor> color;
    Position position;

    bool operator==(const StyleGradientImageStop&) const = default;
};

using StyleGradientImageLengthStop = StyleGradientImageStop<std::optional<Length>>;
using StyleGradientImageAngularStop = StyleGradientImageStop<std::variant<std::monostate, AngleRaw, PercentRaw>>;

class StyleGradientImage final : public StyleGeneratedImage {
public:
    struct LinearData {
        CSSLinearGradientValue::Data data;
        CSSGradientRepeat repeating;
        Vector<StyleGradientImageLengthStop> stops;

        friend bool operator==(const LinearData&, const LinearData&) = default;
    };
    struct PrefixedLinearData {
        CSSPrefixedLinearGradientValue::Data data;
        CSSGradientRepeat repeating;
        Vector<StyleGradientImageLengthStop> stops;

        friend bool operator==(const PrefixedLinearData&, const PrefixedLinearData&) = default;
    };
    struct DeprecatedLinearData {
        CSSDeprecatedLinearGradientValue::Data data;
        Vector<StyleGradientImageLengthStop> stops;

        friend bool operator==(const DeprecatedLinearData&, const DeprecatedLinearData&) = default;
    };
    struct RadialData {
        CSSRadialGradientValue::Data data;
        CSSGradientRepeat repeating;
        Vector<StyleGradientImageLengthStop> stops;

        friend bool operator==(const RadialData&, const RadialData&) = default;
    };
    struct PrefixedRadialData {
        CSSPrefixedRadialGradientValue::Data data;
        CSSGradientRepeat repeating;
        Vector<StyleGradientImageLengthStop> stops;

        friend bool operator==(const PrefixedRadialData&, const PrefixedRadialData&) = default;
    };
    struct DeprecatedRadialData {
        CSSDeprecatedRadialGradientValue::Data data;
        Vector<StyleGradientImageLengthStop> stops;

        friend bool operator==(const DeprecatedRadialData&, const DeprecatedRadialData&) = default;
    };
    struct ConicData {
        CSSConicGradientValue::Data data;
        CSSGradientRepeat repeating;
        Vector<StyleGradientImageAngularStop> stops;

        friend bool operator==(const ConicData&, const ConicData&) = default;
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

    Ref<Gradient> createGradient(const LinearData&, const RenderElement&, const FloatSize&, const RenderStyle&) const;
    Ref<Gradient> createGradient(const PrefixedLinearData&, const RenderElement&, const FloatSize&, const RenderStyle&) const;
    Ref<Gradient> createGradient(const DeprecatedLinearData&, const RenderElement&, const FloatSize&, const RenderStyle&) const;
    Ref<Gradient> createGradient(const RadialData&, const RenderElement&, const FloatSize&, const RenderStyle&) const;
    Ref<Gradient> createGradient(const PrefixedRadialData&, const RenderElement&, const FloatSize&, const RenderStyle&) const;
    Ref<Gradient> createGradient(const DeprecatedRadialData&, const RenderElement&, const FloatSize&, const RenderStyle&) const;
    Ref<Gradient> createGradient(const ConicData&, const RenderElement&, const FloatSize&, const RenderStyle&) const;

    template<typename GradientAdapter, typename Stops> GradientColorStops computeStops(GradientAdapter&, const Stops&, const RenderStyle&, float maxLengthForRepeat, CSSGradientRepeat) const;
    template<typename GradientAdapter, typename Stops> GradientColorStops computeStopsForDeprecatedVariants(GradientAdapter&, const Stops&, const RenderStyle&) const;

    Data m_data;
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    bool m_knownCacheableBarringFilter { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleGradientImage, isGradientImage)
