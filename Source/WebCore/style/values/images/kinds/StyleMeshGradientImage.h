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

#include "StyleGeneratedImage.h"
#include "StyleGeneratedImageCache.h"
#include "StyleMeshGradient.h"

namespace WebCore {

class MeshGradient;

namespace Style {

class MeshGradientImage final : public GeneratedImage {
public:
    using Gradient = FunctionNotation<CSSValueID::MeshGradient, MeshGradient>;

    static Ref<MeshGradientImage> create(Gradient gradient)
    {
        return adoptRef(*new MeshGradientImage(WTF::move(gradient)));
    }
    virtual ~MeshGradientImage();

    bool operator==(const Image&) const final;
    bool equals(const MeshGradientImage&) const;

    const Gradient& gradient() const LIFETIME_BOUND { return m_gradient; }

private:
    explicit MeshGradientImage(Gradient&&);

    Ref<CSSValue> computedStyleValue(const Style::ComputedStyle&) const final;
    Ref<DeprecatedCSSOMValue> computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle&, CSSStyleDeclaration&) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    ImageDrawResult draw(GraphicsContext&, const RenderElement&, const ImageDrawDescriptor&, const FloatRect& destination, const FloatRect& source, bool isForFirstLine) const final;
    ImageDrawResult drawAsPattern(GraphicsContext&, const RenderElement&, const ImageDrawDescriptor&, const FloatRect& destination, const FloatRect& tile, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, bool isForFirstLine) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    bool containsCurrentColor() const final;
    void didAddClient(RenderElement&) final { }
    void didRemoveClient(RenderElement&) final { }

    using CachedGradient = GeneratedImageCacheEntry<WebCore::MeshGradient>;
    Ref<WebCore::MeshGradient> gradientForSize(const RenderElement&, FloatSize, bool isForFirstLine, CachedGradient*&) const;

    Gradient m_gradient;
    bool m_containsCurrentColor { false };
    mutable GeneratedImageCache<WebCore::MeshGradient> m_gradients;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(MeshGradientImage, isMeshGradientImage)
