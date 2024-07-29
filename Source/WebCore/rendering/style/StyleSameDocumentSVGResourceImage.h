/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StyleImage.h"

namespace WebCore {

class CSSValue;
class CSSImageValue;
class CachedImage;
class Document;
class LegacyRenderSVGResourceContainer;
class RenderElement;
class RenderSVGResourceContainer;
class TreeScope;

class StyleSameDocumentSVGResourceImage final : public StyleImage {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<StyleSameDocumentSVGResourceImage> create(Ref<CSSImageValue>);
    virtual ~StyleSameDocumentSVGResourceImage();

    bool operator==(const StyleImage&) const final;
    bool equals(const StyleSameDocumentSVGResourceImage&) const;

    WrappedImagePtr data() const final { return this; }

    Ref<CSSValue> computedStyleValue(const RenderStyle&) const final;

    bool canRender(const RenderElement*, float multiplier) const final;
    bool isPending() const final;
    bool isLoaded(const RenderElement*) const;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    FloatSize imageSize(const RenderElement*, float multiplier, CachedImage::SizeType = CachedImage::UsedSize) const final;
    bool imageHasRelativeWidth() const final;
    bool imageHasRelativeHeight() const final;
    void computeIntrinsicDimensions(const RenderElement*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) final;
    bool usesImageContainerSize() const final;
    void setContainerContextForRenderer(const RenderElement&, const FloatSize&, float, const URL& = URL()) final;
    void addClient(RenderElement&) final;
    void removeClient(RenderElement&) final;
    bool hasClient(RenderElement&) const final;
    RefPtr<Image> image(const RenderElement*, const FloatSize&, bool isForFirstLine) const final;
    bool knownToBeOpaque(const RenderElement&) const final;

    URL reresolvedURL(const Document&) const;

    URL imageURL() const;

private:
    StyleSameDocumentSVGResourceImage(Ref<CSSImageValue>&&);

    LegacyRenderSVGResourceContainer* legacyRenderSVGResource(const RenderElement&) const;
    RenderSVGResourceContainer* renderSVGResource(const RenderElement&) const;
    bool isRenderSVGResource(const RenderElement*) const;

    Ref<CSSImageValue> m_cssValue;
    FloatSize m_containerSize;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleSameDocumentSVGResourceImage, isSameDocumentSVGResourceImage)
