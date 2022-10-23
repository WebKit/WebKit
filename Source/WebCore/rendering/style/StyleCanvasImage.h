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

#include "CanvasBase.h"
#include "StyleGeneratedImage.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class HTMLCanvasElement;

class StyleCanvasImage final : public StyleGeneratedImage, public CanvasObserver {
public:
    static Ref<StyleCanvasImage> create(String name)
    {
        return adoptRef(*new StyleCanvasImage(WTFMove(name)));
    }
    virtual ~StyleCanvasImage();

    bool operator==(const StyleImage&) const final;
    bool equals(const StyleCanvasImage&) const;
    
    static constexpr bool isFixedSize = true;

private:
    explicit StyleCanvasImage(String&&);

    Ref<CSSValue> computedStyleValue(const RenderStyle&) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    RefPtr<Image> image(const RenderElement*, const FloatSize&) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    FloatSize fixedSize(const RenderElement&) const final;
    void didAddClient(RenderElement&) final;
    void didRemoveClient(RenderElement&) final;

    // CanvasObserver.
    bool isStyleCanvasImage() const final { return true; }
    void canvasChanged(CanvasBase&, const std::optional<FloatRect>& changedRect) final;
    void canvasResized(CanvasBase&) final;
    void canvasDestroyed(CanvasBase&) final;

    HTMLCanvasElement* element(Document&) const;

    // The name of the canvas.
    String m_name;
    // The document supplies the element and owns it.
    mutable HTMLCanvasElement* m_element;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleCanvasImage)
    static bool isType(const WebCore::StyleImage& image) { return image.isCanvasImage(); }
    static bool isType(const WebCore::CanvasObserver& canvasObserver) { return canvasObserver.isStyleCanvasImage(); }
SPECIALIZE_TYPE_TRAITS_END()

