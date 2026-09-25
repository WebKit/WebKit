/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include <WebCore/CSSValue.h>
#include <WebCore/CachedImage.h>
#include <WebCore/ColorSpace.h>
#include <WebCore/FloatSize.h>
#include <WebCore/Image.h>
#include <WebCore/RenderObject.h>
#include <WebCore/StyleImageDrawingExtras.h>
#include <WebCore/StyleURL.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/ScopedLambda.h>
#include <wtf/TypeCasts.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class CachedImage;
class CachedResourceLoader;
class CSSStyleDeclaration;
class CSSValue;
class CSSValuePool;
class Document;
class GraphicsContext;
class NativeImage;
class RenderElement;
struct ImagePaintingOptions;
struct NinePieceGeometry;
struct ResourceLoaderOptions;

namespace ObjectSizeNegotiation { class ImageSizingContext; }
using ObjectSizeNegotiation::ImageSizingContext;

namespace Style {

class CachedImage;
class ComputedStyle;

class Image : public RefCountedAndCanMakeWeakPtr<Image> {
public:
    virtual ~Image() = default;

    virtual bool operator==(const Image&) const = 0;

    // Computed Style representation.
    virtual Ref<CSSValue> computedStyleValue(const Style::ComputedStyle&) const = 0;
    virtual Ref<DeprecatedCSSOMValue> computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle&, CSSStyleDeclaration&) const = 0;

    // Opaque representation.
    virtual WrappedImagePtr data() const = 0;

    // Loading.
    virtual bool isPending() const = 0;
    virtual void load(CachedResourceLoader&, const ResourceLoaderOptions&) = 0;
    virtual bool isLoaded(const RenderElement&) const { return true; }
    virtual bool errorOccurred() const { return false; }

    virtual bool hasDecodedImage() const { return true; }
    virtual bool hasImage() const { return false; }

    virtual bool usesDataProtocol() const { return false; }

    virtual URL url() const { return { }; }

    virtual bool isOriginClean(Document&) const { return true; }

    virtual bool isSVGImage() const { return false; }

    virtual WTF::String accessibilityDescription() const { return { }; }

    bool hasHDRContent() const;

    // Clients.
    virtual void addClient(RenderElement&) = 0;
    virtual void removeClient(RenderElement&) = 0;
    virtual bool hasClient(RenderElement&) const = 0;

    // Size / scale.

    virtual NaturalDimensions naturalDimensions(const RenderElement&, const ImageSizingContext&) const = 0;

    virtual float imageScaleFactor() const { return 1; }

    virtual bool hasNothingToDraw(const RenderElement&) const { return false; }

    virtual ImageDrawResult draw(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions = { }, bool isForFirstLine = false) const = 0;

    virtual ImageDrawResult drawAsPattern(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions, bool isForFirstLine) const = 0;

    virtual ImageDrawResult drawTiled(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions = { }, bool isForFirstLine = false) const;

    virtual ImageDrawResult drawNinePiece(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const NinePieceGeometry&, ImagePaintingOptions) const;

    virtual RefPtr<NativeImage> nativeImage(const RenderElement&, ConcreteObjectSize, const ColorSpace& = ColorSpace::SRGB()) const;

    virtual bool isAnimated() const { return false; }
    virtual void stopAnimation() { }
    virtual void resetAnimation() { }

    virtual const CachedImage* cachedImage() const { return nullptr; }

    virtual Vector<Ref<const CachedImage>, 1> cachedImages() const;

    // Multiple Image selection.
    virtual Image* selectedImage() { return this; }
    virtual const Image* selectedImage() const { return this; }

    // Rendering.
    virtual bool currentFrameIsComplete() const { return true; }
    virtual bool canRender(const RenderElement&, float /*multiplier*/) const { return true; }
    virtual bool knownToBeOpaque(const RenderElement&) const = 0;

    virtual std::optional<IntPoint> hotSpot() const { return std::nullopt; }

    ImageDrawingExtras drawingExtras(const RenderElement& renderer) const { return drawingExtrasForRenderer(renderer); }

    // Derived type.
    ALWAYS_INLINE bool isCachedImage() const { return m_type == Type::CachedImage; }
    ALWAYS_INLINE bool isCursorImage() const { return m_type == Type::CursorImage; }
    ALWAYS_INLINE bool isImageSet() const { return m_type == Type::ImageSet; }
    ALWAYS_INLINE bool isGeneratedImage() const { return isFilterImage() || isCanvasImage() || isCrossfadeImage() || isGradientImage() || isNamedImage() || isColorImage() || isPaintImage() || isInvalidImage(); }
    ALWAYS_INLINE bool isFilterImage() const { return m_type == Type::FilterImage; }
    ALWAYS_INLINE bool isCanvasImage() const { return m_type == Type::CanvasImage; }
    ALWAYS_INLINE bool isCrossfadeImage() const { return m_type == Type::CrossfadeImage; }
    ALWAYS_INLINE bool isGradientImage() const { return m_type == Type::GradientImage; }
    ALWAYS_INLINE bool isNamedImage() const { return m_type == Type::NamedImage; }
    ALWAYS_INLINE bool isColorImage() const { return m_type == Type::ColorImage; }
    ALWAYS_INLINE bool isPaintImage() const { return m_type == Type::PaintImage; }
    ALWAYS_INLINE bool isInvalidImage() const { return m_type == Type::InvalidImage; }

    bool hasCachedImage() const { return m_type == Type::CachedImage || selectedImage()->isCachedImage(); }

protected:
    friend class MultiImage;

    virtual ImageDrawingExtras drawingExtrasForRenderer(const RenderElement&, const WTF::URL& = WTF::URL()) const { return { }; }

    enum class Type : uint8_t {
        CachedImage,
        CursorImage,
        ImageSet,
        FilterImage,
        CanvasImage,
        CrossfadeImage,
        GradientImage,
        NamedImage,
        ColorImage,
        InvalidImage,
        PaintImage,
    };

    Image(Type type)
        : m_type { type }
    {
    }

    using DestinationPaint = ImageDrawResult(GraphicsContext&);
    template<typename Paint>
    static ImageDrawResult drawIntoDestination(GraphicsContext& context, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, NOESCAPE Paint&& paint)
    {
        return drawIntoDestinationImpl(context, destination, source, options, paint);
    }

    using TiledDraw = ImageDrawResult(GraphicsContext&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source);
    using TiledDrawPattern = ImageDrawResult(GraphicsContext&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing);

    template<typename Draw, typename DrawPattern>
    static ImageDrawResult drawTiledUsing(GraphicsContext& context, NaturalDimensions naturalDimensions, NOESCAPE Draw&& draw, NOESCAPE DrawPattern&& drawPattern, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions options)
    {
        return drawTiledUsingImpl(context, naturalDimensions, draw, drawPattern, concreteObjectSize, destination, phase, tileSize, spacing, options);
    }

    template<typename DrawPattern>
    static ImageDrawResult drawTiledUsing(GraphicsContext& context, NOESCAPE DrawPattern&& drawPattern, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, WebCore::Image::TileRule horizontalRule, WebCore::Image::TileRule verticalRule)
    {
        return drawTiledUsingImpl(context, drawPattern, concreteObjectSize, destination, source, tileScaleFactor, horizontalRule, verticalRule);
    }

    template<typename Draw, typename DrawPattern>
    static ImageDrawResult drawNinePieceUsing(GraphicsContext& context, NOESCAPE Draw&& draw, NOESCAPE DrawPattern&& drawPattern, ConcreteObjectSize concreteObjectSize, const NinePieceGeometry& geometry)
    {
        return drawNinePieceUsingImpl(context, draw, drawPattern, concreteObjectSize, geometry);
    }

    ImageDrawResult drawResolved(GraphicsContext&, const RenderElement&, WebCore::Image&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions) const;

private:
    static ImageDrawResult drawIntoDestinationImpl(GraphicsContext&, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions, const ScopedLambda<DestinationPaint>&);
    static ImageDrawResult drawTiledUsingImpl(GraphicsContext&, NaturalDimensions, const ScopedLambda<TiledDraw>&, const ScopedLambda<TiledDrawPattern>&, ConcreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions);
    static ImageDrawResult drawTiledUsingImpl(GraphicsContext&, const ScopedLambda<TiledDrawPattern>&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, WebCore::Image::TileRule horizontalRule, WebCore::Image::TileRule verticalRule);
    static ImageDrawResult drawNinePieceUsingImpl(GraphicsContext&, const ScopedLambda<TiledDraw>&, const ScopedLambda<TiledDrawPattern>&, ConcreteObjectSize, const NinePieceGeometry&);

protected:
    Type m_type;
};

ConcreteObjectSize negotiate(const Image&, const RenderElement&, const ImageSizingContext&);

} // namespace Style
} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(ToClassName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Style::ToClassName) \
    static bool isType(const WebCore::Style::Image& image) { return image.predicate(); } \
SPECIALIZE_TYPE_TRAITS_END()
