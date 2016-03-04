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

class CachedImage;

class StyleCachedImage final : public StyleImage, private CachedImageClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<StyleCachedImage> create(CachedImage* image) { return adoptRef(*new StyleCachedImage(image)); }
    virtual ~StyleCachedImage();

    CachedImage* cachedImage() const override { return m_image.get(); }

private:
    WrappedImagePtr data() const override { return m_image.get(); }

    PassRefPtr<CSSValue> cssValue() const override;
    
    bool canRender(const RenderObject*, float multiplier) const override;
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
    bool knownToBeOpaque(const RenderElement*) const override;

    explicit StyleCachedImage(CachedImage*);

    CachedResourceHandle<CachedImage> m_image;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleCachedImage, isCachedImage)

#endif // StyleCachedImage_h
