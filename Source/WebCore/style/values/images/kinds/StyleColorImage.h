/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "StyleColor.h"
#include "StyleGeneratedImage.h"

namespace WebCore {
namespace Style {

class ColorImage final : public GeneratedImage {
public:
    static Ref<ColorImage> create(Color&& color)
    {
        return adoptRef(*new ColorImage(WTF::move(color)));
    }
    virtual ~ColorImage();

    bool operator==(const Image&) const final;
    bool equals(const ColorImage&) const;

    static constexpr bool isFixedSize = false;

private:
    explicit ColorImage(Color&&);

    Ref<CSSValue> computedStyleValue(const Style::ComputedStyle&) const final;
    Ref<DeprecatedCSSOMValue> computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle&, CSSStyleDeclaration&) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    RefPtr<WebCore::Image> image(const RenderElement*, const FloatSize&, const GraphicsContext&, bool isForFirstLine) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    FloatSize fixedSize(const RenderElement&) const final;
    void didAddClient(RenderElement&) final { }
    void didRemoveClient(RenderElement&) final { }

    Color m_color;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(ColorImage, isColorImage)
