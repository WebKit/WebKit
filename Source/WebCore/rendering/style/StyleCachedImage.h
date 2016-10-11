/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef StyleCachedImage_h
#define StyleCachedImage_h

#include "CachedImageClient.h"
#include "CachedResourceHandle.h"
#include "StyleImage.h"

namespace WebCore {

class CSSValue;
class CachedImage;
class Document;

class StyleCachedImage final : public StyleImage {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<StyleCachedImage> create(CSSValue& cssValue) { return adoptRef(*new StyleCachedImage(cssValue)); }
    virtual ~StyleCachedImage();

    bool operator==(const StyleImage& other) const override;

    CachedImage* cachedImage() const override;

    WrappedImagePtr data() const override { return m_cachedImage.get(); }

    PassRefPtr<CSSValue> cssValue() const override;
    
    bool canRender(const RenderObject*, float multiplier) const override;
    bool isPending() const override;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) override;
    bool isLoaded() const override;
    bool errorOccurred() const override;
    FloatSize imageSize(const RenderElement*, float multiplier) const override;
    bool imageHasRelativeWidth() const override;
    bool imageHasRelativeHeight() const override;
    void computeIntrinsicDimensions(const RenderElement*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) override;
    bool usesImageContainerSize() const override;
    void setContainerSizeForRenderer(const RenderElement*, const FloatSize&, float) override;
    void addClient(RenderElement*) override;
    void removeClient(RenderElement*) override;
    RefPtr<Image> image(RenderElement*, const FloatSize&) const override;
    float imageScaleFactor() const override;
    bool knownToBeOpaque(const RenderElement*) const override;

private:
    StyleCachedImage(CSSValue&);

    Ref<CSSValue> m_cssValue;
    bool m_isPending { true };
    mutable float m_scaleFactor { 1 };
    mutable CachedResourceHandle<CachedImage> m_cachedImage;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleCachedImage, isCachedImage)

#endif // StyleCachedImage_h
