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
#include "StyleColor.h"
#include "StyleGeneratedImage.h"
#include <variant>

namespace WebCore {

struct StyleGradientImageStop {
    std::optional<StyleColor> color;
    RefPtr<CSSPrimitiveValue> position;
};

class StyleGradientImage final : public StyleGeneratedImage {
public:
    using Stop = StyleGradientImageStop;

    struct LinearData {
        RefPtr<CSSPrimitiveValue> firstX;
        RefPtr<CSSPrimitiveValue> firstY;
        RefPtr<CSSPrimitiveValue> secondX;
        RefPtr<CSSPrimitiveValue> secondY;
        RefPtr<CSSPrimitiveValue> angle;
    };

    struct RadialData {
        RefPtr<CSSPrimitiveValue> firstX;
        RefPtr<CSSPrimitiveValue> firstY;
        RefPtr<CSSPrimitiveValue> secondX;
        RefPtr<CSSPrimitiveValue> secondY;
        RefPtr<CSSPrimitiveValue> firstRadius;
        RefPtr<CSSPrimitiveValue> secondRadius;
        RefPtr<CSSPrimitiveValue> shape;
        RefPtr<CSSPrimitiveValue> sizingBehavior;
        RefPtr<CSSPrimitiveValue> endHorizontalSize;
        RefPtr<CSSPrimitiveValue> endVerticalSize;
    };

    struct ConicData {
        RefPtr<CSSPrimitiveValue> firstX;
        RefPtr<CSSPrimitiveValue> firstY;
        RefPtr<CSSPrimitiveValue> secondX;
        RefPtr<CSSPrimitiveValue> secondY;
        RefPtr<CSSPrimitiveValue> angle;
    };

    using Data = std::variant<LinearData, RadialData, ConicData>;

    static Ref<StyleGradientImage> create(Data data, CSSGradientRepeat repeat, CSSGradientType gradientType, CSSGradientColorInterpolationMethod colorInterpolationMethod, Vector<StyleGradientImageStop> stops)
    {
        return adoptRef(*new StyleGradientImage(WTFMove(data), repeat, gradientType, colorInterpolationMethod, WTFMove(stops)));
    }
    virtual ~StyleGradientImage();

    bool operator==(const StyleImage&) const final;
    bool equals(const StyleGradientImage&) const;

    static constexpr bool isFixedSize = false;

private:
    explicit StyleGradientImage(Data&&, CSSGradientRepeat, CSSGradientType, CSSGradientColorInterpolationMethod, Vector<StyleGradientImageStop>&&);

    Ref<CSSValue> computedStyleValue(const RenderStyle&) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    RefPtr<Image> image(const RenderElement*, const FloatSize&) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    FloatSize fixedSize(const RenderElement&) const final;
    void didAddClient(RenderElement&) final { }
    void didRemoveClient(RenderElement&) final { }

    Ref<Gradient> createGradient(const LinearData&, const RenderElement&, const FloatSize&) const;
    Ref<Gradient> createGradient(const RadialData&, const RenderElement&, const FloatSize&) const;
    Ref<Gradient> createGradient(const ConicData&, const RenderElement&, const FloatSize&) const;

    template<typename GradientAdapter> GradientColorStops computeStops(GradientAdapter&, const CSSToLengthConversionData&, const RenderStyle&, float maxLengthForRepeat) const;

    Data m_data;
    CSSGradientRepeat m_repeat { CSSGradientRepeat::NonRepeating };
    CSSGradientType m_gradientType { CSSGradientType::CSSLinearGradient };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    Vector<StyleGradientImageStop> m_stops;
    bool m_knownCacheableBarringFilter { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleGradientImage, isGradientImage)
