/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Noam Rosenthal (noam@webkit.org)
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

class Document;

struct ImageWithScale {
    RefPtr<StyleImage> image;
    float scaleFactor { 1 };
};

inline bool operator==(const ImageWithScale& a, const ImageWithScale& b)
{
    return a.image == b.image && a.scaleFactor == b.scaleFactor;
}

inline bool operator!=(const ImageWithScale& a, const ImageWithScale& b)
{
    return !(a == b);
}

class StyleMultiImage : public StyleImage {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~StyleMultiImage();

protected:
    StyleMultiImage(Type);

    bool equals(const StyleMultiImage& other) const;

    virtual ImageWithScale selectBestFitImage(const Document&) = 0;
    CachedImage* cachedImage() const final;

private:
    WrappedImagePtr data() const final;

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
    void setContainerContextForRenderer(const RenderElement&, const FloatSize&, float);
    void addClient(RenderElement&) final;
    void removeClient(RenderElement&) final;
    bool hasClient(RenderElement&) const final;
    RefPtr<Image> image(const RenderElement*, const FloatSize&) const final;
    float imageScaleFactor() const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    const StyleImage* selectedImage() const final { return m_selectedImage.get(); }
    StyleImage* selectedImage() final { return m_selectedImage.get(); }

    RefPtr<StyleImage> m_selectedImage;
    bool m_isPending { true };
};

} // namespace WebCore
