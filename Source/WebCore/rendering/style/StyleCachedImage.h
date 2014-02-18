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
    static PassRefPtr<StyleCachedImage> create(CachedImage* image) { return adoptRef(new StyleCachedImage(image)); }
    virtual ~StyleCachedImage();

    virtual CachedImage* cachedImage() const override { return m_image.get(); }

private:
    virtual WrappedImagePtr data() const override { return m_image.get(); }

    virtual PassRefPtr<CSSValue> cssValue() const override;
    
    virtual bool canRender(const RenderObject*, float multiplier) const override;
    virtual bool isLoaded() const override;
    virtual bool errorOccurred() const override;
    virtual LayoutSize imageSize(const RenderElement*, float multiplier) const override;
    virtual bool imageHasRelativeWidth() const override;
    virtual bool imageHasRelativeHeight() const override;
    virtual void computeIntrinsicDimensions(const RenderElement*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) override;
    virtual bool usesImageContainerSize() const override;
    virtual void setContainerSizeForRenderer(const RenderElement*, const IntSize&, float) override;
    virtual void addClient(RenderElement*) override;
    virtual void removeClient(RenderElement*) override;
    virtual PassRefPtr<Image> image(RenderElement*, const IntSize&) const override;
    virtual bool knownToBeOpaque(const RenderElement*) const override;

    explicit StyleCachedImage(CachedImage*);

    CachedResourceHandle<CachedImage> m_image;
};

STYLE_IMAGE_TYPE_CASTS(StyleCachedImage, StyleImage, isCachedImage)

}
#endif
