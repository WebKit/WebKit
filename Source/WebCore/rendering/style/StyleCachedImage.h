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

class StyleCachedImage FINAL : public StyleImage, private CachedImageClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<StyleCachedImage> create(CachedImage* image) { return adoptRef(new StyleCachedImage(image)); }
    virtual ~StyleCachedImage();

    virtual CachedImage* cachedImage() const OVERRIDE { return m_image.get(); }

private:
    virtual WrappedImagePtr data() const OVERRIDE { return m_image.get(); }

    virtual PassRefPtr<CSSValue> cssValue() const OVERRIDE;
    
    virtual bool canRender(const RenderObject*, float multiplier) const OVERRIDE;
    virtual bool isLoaded() const OVERRIDE;
    virtual bool errorOccurred() const OVERRIDE;
    virtual LayoutSize imageSize(const RenderElement*, float multiplier) const OVERRIDE;
    virtual bool imageHasRelativeWidth() const OVERRIDE;
    virtual bool imageHasRelativeHeight() const OVERRIDE;
    virtual void computeIntrinsicDimensions(const RenderElement*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) OVERRIDE;
    virtual bool usesImageContainerSize() const OVERRIDE;
    virtual void setContainerSizeForRenderer(const RenderElement*, const IntSize&, float) OVERRIDE;
    virtual void addClient(RenderElement*) OVERRIDE;
    virtual void removeClient(RenderElement*) OVERRIDE;
    virtual PassRefPtr<Image> image(RenderElement*, const IntSize&) const OVERRIDE;
    virtual bool knownToBeOpaque(const RenderElement*) const OVERRIDE;

    explicit StyleCachedImage(CachedImage*);

    CachedResourceHandle<CachedImage> m_image;
};

}
#endif
