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

class MultiImage : public Image {
    WTF_MAKE_TZONE_ALLOCATED(MultiImage);
public:
    virtual ~MultiImage();

protected:
    MultiImage(Type);

    bool equals(const MultiImage& other) const;

    virtual ImageWithScale selectBestFitImage(const Document&) = 0;
    const CachedImage* cachedImage() const final;

private:
    WrappedImagePtr data() const final;

    bool canRender(const RenderElement&, float multiplier) const final;
    bool isPending() const final { return m_isPending; }
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    bool isLoaded(const RenderElement&) const final;
    bool isSVGImage() const final;
    bool hasNothingToDraw(const RenderElement&) const final;
    bool errorOccurred() const final;
    NaturalDimensions naturalDimensions(const RenderElement&, const ImageSizingContext&) const final;
    WTF::String accessibilityDescription() const final;
    bool isAnimated() const final;
    void stopAnimation() final;
    void resetAnimation() final;
    ImageDrawingExtras drawingExtrasForRenderer(const RenderElement&, const WTF::URL& = WTF::URL()) const override;
    void addClient(RenderElement&) final;
    void removeClient(RenderElement&) final;
    bool hasClient(RenderElement&) const final;
    ImageDrawResult draw(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions, bool isForFirstLine) const final;
    ImageDrawResult drawAsPattern(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions, bool isForFirstLine) const final;
    ImageDrawResult drawTiled(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions, bool isForFirstLine) const final;
    ImageDrawResult drawNinePiece(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const NinePieceGeometry&, ImagePaintingOptions) const final;
    bool currentFrameIsComplete() const final;
    float imageScaleFactor() const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    bool isOriginClean(Document&) const final;
    Vector<Ref<const CachedImage>, 1> cachedImages() const final;
    bool hasDecodedImage() const final;
    RefPtr<NativeImage> nativeImage(const RenderElement&, ConcreteObjectSize, const ColorSpace&) const final;
    std::optional<IntPoint> hotSpot() const final;
    const Image* selectedImage() const final { return m_selectedImage.get(); }
    Image* selectedImage() final { return m_selectedImage.get(); }

    RefPtr<Image> m_selectedImage;
    bool m_isPending { true };
};

} // namespace Style
} // namespace WebCore
