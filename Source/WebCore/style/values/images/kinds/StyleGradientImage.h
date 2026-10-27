/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "FloatSizeHash.h"
#include "StyleGeneratedImage.h"
#include "StyleGradient.h"
#include <wtf/HashMap.h>

namespace WebCore {
namespace Style {

struct AppleColorFilter;

class GradientImage final : public GeneratedImage {
public:
    static Ref<GradientImage> create(Gradient gradient)
    {
        return adoptRef(*new GradientImage(WTF::move(gradient)));
    }
    virtual ~GradientImage();

    bool operator==(const Image&) const final;
    bool equals(const GradientImage&) const;

    const Gradient& gradient() const LIFETIME_BOUND { return m_gradient; }

    struct CachedGradientForTesting {
        float width { 0 };
        float height { 0 };
        bool hasColorFilter { false };
        uint64_t patternBufferIdentifier { 0 };
    };
    WEBCORE_EXPORT Vector<CachedGradientForTesting> cachedGradientsForTesting() const;

private:
    explicit GradientImage(Gradient&&);

    Ref<CSSValue> computedStyleValue(const Style::ComputedStyle&) const final;
    Ref<DeprecatedCSSOMValue> computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle&, CSSStyleDeclaration&) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    ImageDrawResult draw(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source, ImageDrawingOptions, bool isForFirstLine) const final;
    ImageDrawResult drawAsPattern(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions, bool isForFirstLine) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    bool containsCurrentColor() const final;
    void didAddClient(RenderElement&) final { }
    void didRemoveClient(RenderElement&) final { }

    // The platform gradient for one size, and the buffer a tiled draw rasterized it into.
    class CachedGradient;
    CachedGradient* cachedGradient(FloatSize, const AppleColorFilter&);
    void evictCachedGradient(CachedGradient&);
    Ref<WebCore::Gradient> gradientForSize(const RenderElement&, FloatSize, bool isForFirstLine, CachedGradient*&) const;

    Gradient m_gradient;
    bool m_stopsAreCacheable { false };
    HashMap<FloatSize, Vector<std::unique_ptr<CachedGradient>, 1>> m_gradients;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(GradientImage, isGradientImage)
