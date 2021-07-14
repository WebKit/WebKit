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

#include "CachedResourceHandle.h"
#include "StyleImage.h"

namespace WebCore {

class CSSValue;
class CSSImageValue;
class CachedImage;
class Document;

class StyleCachedImage final : public StyleImage {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<StyleCachedImage> create(CSSImageValue& cssValue, float scaleFactor = 1);
    virtual ~StyleCachedImage();

    bool operator==(const StyleImage& other) const final;

    CachedImage* cachedImage() const final;

    WrappedImagePtr data() const final { return m_cachedImage.get(); }

    Ref<CSSValue> cssValue() const final;
    
    bool canRender(const RenderElement*, float multiplier) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    bool isLoaded() const final;
    bool errorOccurred() const final;
    FloatSize imageSize(const RenderElement*, float multiplier) const final;
    bool imageHasRelativeWidth() const final;
    bool imageHasRelativeHeight() const final;
    void computeIntrinsicDimensions(const RenderElement*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) final;
    bool usesImageContainerSize() const final;
    void setContainerContextForRenderer(const RenderElement&, const FloatSize&, float) final;
    void addClient(RenderElement&) final;
    void removeClient(RenderElement&) final;
    bool hasClient(RenderElement&) const final;
    bool hasImage() const final;
    RefPtr<Image> image(RenderElement*, const FloatSize&) const final;
    float imageScaleFactor() const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    bool usesDataProtocol() const final;

private:
    StyleCachedImage(CSSImageValue&, float);
    URL imageURL();

    Ref<CSSImageValue> m_cssValue;
    bool m_isPending { true };
    mutable float m_scaleFactor { 1 };
    mutable CachedResourceHandle<CachedImage> m_cachedImage;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleCachedImage, isCachedImage)
