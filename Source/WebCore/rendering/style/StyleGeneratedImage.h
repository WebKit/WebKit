/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "StyleImage.h"

namespace WebCore {

class CSSValue;
class CSSImageGeneratorValue;

class StyleGeneratedImage final : public StyleImage {
public:
    static Ref<StyleGeneratedImage> create(Ref<CSSImageGeneratorValue>&& value)
    {
        return adoptRef(*new StyleGeneratedImage(WTFMove(value)));
    }

    CSSImageGeneratorValue& imageValue() { return m_imageGeneratorValue; }

private:
    bool operator==(const StyleImage& other) const final;

    WrappedImagePtr data() const final { return m_imageGeneratorValue.ptr(); }

    Ref<CSSValue> cssValue() const final;

    bool isPending() const override;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    FloatSize imageSize(const RenderElement*, float multiplier) const final;
    bool imageHasRelativeWidth() const final { return !m_fixedSize; }
    bool imageHasRelativeHeight() const final { return !m_fixedSize; }
    void computeIntrinsicDimensions(const RenderElement*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) final;
    bool usesImageContainerSize() const final { return !m_fixedSize; }
    void setContainerContextForRenderer(const RenderElement&, const FloatSize& containerSize, float) final { m_containerSize = containerSize; }
    void addClient(RenderElement&) final;
    void removeClient(RenderElement&) final;
    bool hasClient(RenderElement&) const final;
    RefPtr<Image> image(RenderElement*, const FloatSize&) const final;
    bool knownToBeOpaque(const RenderElement&) const final;

    explicit StyleGeneratedImage(Ref<CSSImageGeneratorValue>&&);
    
    Ref<CSSImageGeneratorValue> m_imageGeneratorValue;
    FloatSize m_containerSize;
    bool m_fixedSize;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleGeneratedImage, isGeneratedImage)
