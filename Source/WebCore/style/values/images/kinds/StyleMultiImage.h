/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Noam Rosenthal (noam@webkit.org)
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleInvalidImage.h"
#include "StylePrimitiveNumeric.h"
#include "StyleString.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class Document;

namespace Style {

struct ImageWithScale {
    RefPtr<Image> image { InvalidImage::create() };
    Style::Resolution<> scaleFactor { 1 };
    std::optional<FunctionNotation<CSSValueType, Style::String>> mimeType;

    bool operator==(const ImageWithScale& other) const
    {
        return image == other.image && scaleFactor == other.scaleFactor;
    }
};

class MultiImage : public Image, private ImageClient {
    WTF_MAKE_TZONE_ALLOCATED(MultiImage);
public:
    virtual ~MultiImage();

    // ImageClient.
    void ref() const final { Image::ref(); }
    void deref() const final { Image::deref(); }

protected:
    MultiImage(Type);

    bool equals(const MultiImage& other) const;

    virtual ImageWithScale selectBestFitImage(const Document&) = 0;
    WebCore::CachedImage* cachedImage() const final;

private:
    WrappedImagePtr data() const final;

    bool canRender(const RenderElement*, float multiplier) const final;
    bool isPending() const final { return m_isPending; }
    bool isLoading() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    bool isLoaded(const RenderElement*) const final;
    bool errorOccurred() const final;
    FloatSize imageSize(const RenderElement*, float multiplier, ImageSizeType = ImageSizeType::Used) const final;
    bool imageHasRelativeWidth() const final;
    bool imageHasRelativeHeight() const final;
    void computeIntrinsicDimensions(const RenderElement*, float& intrinsicWidth, float& intrinsicHeight, FloatSize& intrinsicRatio) final;
    bool usesImageContainerSize() const final;
    void registerContainerContext(const ImageContainerContextKey&, ImageContainerContext&&);
    RefPtr<WebCore::Image> image(const RenderElement*, const FloatSize&, const GraphicsContext& destinationContext, bool isForFirstLine) const final;
    bool currentFrameIsComplete(const RenderElement&) const final;
    float imageScaleFactor() const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    const Image* selectedImage() const final { return m_selectedImage.get(); }
    Image* selectedImage() final { return m_selectedImage.get(); }
    bool contains(const Image&) const final;
    void didAddClient(ImageClient&) final { }
    void didRemoveClient(ImageClient&) final { }

    // ImageClient.
    void imageChanged(const Image&, const IntRect*) const final;
    void notifyFinished(const CachedImage&) const final;
    bool allowsAnimation(const CachedImage&) const final;
    bool canDestroyDecodedData(const CachedImage&) const final;
    bool useSystemDarkAppearance(const CachedImage&) const final;
    VisibleInViewportState imageFrameAvailable(const CachedImage&, ImageAnimatingState, const IntRect*) const final;
    VisibleInViewportState imageVisibleInViewport(const CachedImage&, const Document&) const final;
    void didRemoveCachedImageClient(const CachedImage&) const final;
    void imageContentChanged(const CachedImage&) const final;
    void scheduleRenderingUpdateForImage(const CachedImage&) const final;
    bool isRendererClient() const final;

    RefPtr<Image> m_selectedImage;
    bool m_isPending { true };
};

} // namespace Style
} // namespace WebCore
