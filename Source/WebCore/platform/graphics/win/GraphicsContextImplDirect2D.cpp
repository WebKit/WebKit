/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "GraphicsContextImplDirect2D.h"

#if USE(DIRECT2D)

#include "Direct2DOperations.h"
#include "FloatRoundedRect.h"
#include "Font.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "GraphicsContextPlatformPrivateDirect2D.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "PlatformContextDirect2D.h"

namespace WebCore {

GraphicsContext::GraphicsContextImplFactory GraphicsContextImplDirect2D::createFactory(PlatformContextDirect2D& platformContext)
{
    return GraphicsContext::GraphicsContextImplFactory(
        [&platformContext](GraphicsContext& context)
        {
            return makeUnique<GraphicsContextImplDirect2D>(context, platformContext);
        });
}

GraphicsContext::GraphicsContextImplFactory GraphicsContextImplDirect2D::createFactory(ID2D1RenderTarget* renderTarget)
{
    return GraphicsContext::GraphicsContextImplFactory(
        [renderTarget](GraphicsContext& context)
        {
            return makeUnique<GraphicsContextImplDirect2D>(context, renderTarget);
        });
}

GraphicsContextImplDirect2D::GraphicsContextImplDirect2D(GraphicsContext& context, PlatformContextDirect2D& platformContext)
    : GraphicsContextImpl(context, FloatRect { }, AffineTransform { })
    , m_platformContext(platformContext)
    , m_private(makeUnique<GraphicsContextPlatformPrivate>(m_platformContext, GraphicsContext::BitmapRenderingContextType::GPUMemory))
{
    m_platformContext.setGraphicsContextPrivate(m_private.get());
    m_private->syncContext(m_platformContext);
}

GraphicsContextImplDirect2D::GraphicsContextImplDirect2D(GraphicsContext& context, ID2D1RenderTarget* renderTarget)
    : GraphicsContextImpl(context, FloatRect { }, AffineTransform { })
    , m_ownedPlatformContext(makeUnique<PlatformContextDirect2D>(renderTarget))
    , m_platformContext(*m_ownedPlatformContext)
    , m_private(makeUnique<GraphicsContextPlatformPrivate>(m_platformContext, GraphicsContext::BitmapRenderingContextType::GPUMemory))
{
    m_platformContext.setGraphicsContextPrivate(m_private.get());
    m_private->syncContext(m_platformContext);
}

GraphicsContextImplDirect2D::~GraphicsContextImplDirect2D()
{
    m_platformContext.setGraphicsContextPrivate(nullptr);
}

bool GraphicsContextImplDirect2D::hasPlatformContext() const
{
    return true;
}

PlatformContextDirect2D* GraphicsContextImplDirect2D::platformContext() const
{
    return &m_platformContext;
}

void GraphicsContextImplDirect2D::updateState(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    if (flags & GraphicsContextState::StrokeThicknessChange)
        Direct2D::State::setStrokeThickness(m_platformContext, state.strokeThickness);

    if (flags & GraphicsContextState::StrokeStyleChange)
        Direct2D::State::setStrokeStyle(m_platformContext, state.strokeStyle);

    if (flags & GraphicsContextState::ShadowChange) {
        if (state.shadowsIgnoreTransforms) {
            // Meaning that this graphics context is associated with a CanvasRenderingContext
            // We flip the height since CG and HTML5 Canvas have opposite Y axis
            auto& mutableState = const_cast<GraphicsContextState&>(graphicsContext().state());
            auto& shadowOffset = state.shadowOffset;
            mutableState.shadowOffset = { shadowOffset.width(), -shadowOffset.height() };
        }
    }

    if (flags & GraphicsContextState::CompositeOperationChange)
        Direct2D::State::setCompositeOperation(m_platformContext, state.compositeOperator, state.blendMode);

    if (flags & GraphicsContextState::ShouldAntialiasChange)
        Direct2D::State::setShouldAntialias(m_platformContext, state.shouldAntialias);
}

void GraphicsContextImplDirect2D::clearShadow()
{
}

void GraphicsContextImplDirect2D::setLineCap(LineCap lineCap)
{
    Direct2D::setLineCap(m_platformContext, lineCap);
}

void GraphicsContextImplDirect2D::setLineDash(const DashArray& dashes, float dashOffset)
{
    Direct2D::setLineDash(m_platformContext, dashes, dashOffset);
}

void GraphicsContextImplDirect2D::setLineJoin(LineJoin lineJoin)
{
    Direct2D::setLineJoin(m_platformContext, lineJoin);
}

void GraphicsContextImplDirect2D::setMiterLimit(float miterLimit)
{
    Direct2D::setMiterLimit(m_platformContext, miterLimit);
}

void GraphicsContextImplDirect2D::fillRect(const FloatRect& rect)
{
    auto& state = graphicsContext().state();
    Direct2D::fillRect(m_platformContext, rect, Direct2D::FillSource(state, graphicsContext()), Direct2D::ShadowState(state));
}

void GraphicsContextImplDirect2D::fillRect(const FloatRect& rect, const Color& color)
{
    Direct2D::fillRect(m_platformContext, rect, color, Direct2D::ShadowState(graphicsContext().state()));
}

void GraphicsContextImplDirect2D::fillRect(const FloatRect& rect, Gradient& gradient)
{
    auto brush = gradient.createBrush(m_platformContext.renderTarget());
    if (!brush)
        return;

    Direct2D::fillRectWithGradient(m_platformContext, rect, brush.get());
}

void GraphicsContextImplDirect2D::fillRect(const FloatRect& rect, const Color& color, CompositeOperator compositeOperator, BlendMode blendMode)
{
    auto& state = graphicsContext().state();
    CompositeOperator previousOperator = state.compositeOperator;

    Direct2D::State::setCompositeOperation(m_platformContext, compositeOperator, blendMode);
    Direct2D::fillRect(m_platformContext, rect, color, Direct2D::ShadowState(state));
    Direct2D::State::setCompositeOperation(m_platformContext, previousOperator, BlendMode::Normal);
}

void GraphicsContextImplDirect2D::fillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
{
    auto& state = graphicsContext().state();

    CompositeOperator previousOperator = state.compositeOperator;
    Direct2D::State::setCompositeOperation(m_platformContext, previousOperator, blendMode);

    Direct2D::ShadowState shadowState(state);
    if (rect.isRounded())
        Direct2D::fillRoundedRect(m_platformContext, rect, color, shadowState);
    else
        Direct2D::fillRect(m_platformContext, rect.rect(), color, shadowState);

    Direct2D::State::setCompositeOperation(m_platformContext, previousOperator, BlendMode::Normal);
}

void GraphicsContextImplDirect2D::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    auto& state = graphicsContext().state();

    Direct2D::FillSource fillSource(state, graphicsContext());
    fillSource.brush = m_platformContext.brushWithColor(color);

    Direct2D::fillRectWithRoundedHole(m_platformContext, rect, roundedHoleRect, fillSource, Direct2D::ShadowState(graphicsContext().state()));
}

void GraphicsContextImplDirect2D::fillPath(const Path& path)
{
    auto& state = graphicsContext().state();
    Direct2D::fillPath(m_platformContext, path, Direct2D::FillSource(state, graphicsContext()), Direct2D::ShadowState(state));
}

void GraphicsContextImplDirect2D::fillEllipse(const FloatRect& rect)
{
    Path path;
    path.addEllipse(rect);
    fillPath(path);
}

void GraphicsContextImplDirect2D::strokeRect(const FloatRect& rect, float lineWidth)
{
    auto& state = graphicsContext().state();
    Direct2D::strokeRect(m_platformContext, rect, lineWidth, Direct2D::StrokeSource(state, graphicsContext()), Direct2D::ShadowState(state));
}

void GraphicsContextImplDirect2D::strokePath(const Path& path)
{
    auto& state = graphicsContext().state();
    Direct2D::strokePath(m_platformContext, path, Direct2D::StrokeSource(state, graphicsContext()), Direct2D::ShadowState(state));
}

void GraphicsContextImplDirect2D::strokeEllipse(const FloatRect& rect)
{
    Path path;
    path.addEllipse(rect);
    strokePath(path);
}

void GraphicsContextImplDirect2D::clearRect(const FloatRect& rect)
{
    Direct2D::clearRect(m_platformContext, rect);
}

void GraphicsContextImplDirect2D::drawGlyphs(const Font& font, const GlyphBuffer& glyphBuffer, unsigned from, unsigned numGlyphs, const FloatPoint& point, FontSmoothingMode fontSmoothing)
{
    UNUSED_PARAM(fontSmoothing);
    if (!font.platformData().size())
        return;

    auto xOffset = point.x();
    Vector<unsigned short> glyphs(numGlyphs);
    Vector<float> horizontalAdvances(numGlyphs);
    Vector<DWRITE_GLYPH_OFFSET> glyphOffsets(numGlyphs);

    for (unsigned i = 0; i < numGlyphs; ++i) {
        if (i + from >= glyphBuffer.advancesCount())
            break;

        auto advance = glyphBuffer.advances(i + from);
        if (!advance)
            continue;

        glyphs[i] = glyphBuffer.glyphAt(i + from);
        horizontalAdvances[i] = advance->width();
        glyphOffsets[i].advanceOffset = advance->width();
        glyphOffsets[i].ascenderOffset = advance->height();
    }

    double syntheticBoldOffset = font.syntheticBoldOffset();

    auto& state = graphicsContext().state();
    Direct2D::drawGlyphs(m_platformContext, Direct2D::FillSource(state, graphicsContext()), Direct2D::StrokeSource(state, graphicsContext()),
        Direct2D::ShadowState(state), point, font, syntheticBoldOffset, glyphs, horizontalAdvances, glyphOffsets, xOffset,
        state.textDrawingMode, state.strokeThickness, state.shadowOffset, state.shadowColor);
}

ImageDrawResult GraphicsContextImplDirect2D::drawImage(Image& image, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& imagePaintingOptions)
{
    return GraphicsContextImpl::drawImageImpl(graphicsContext(), image, destination, source, imagePaintingOptions);
}

ImageDrawResult GraphicsContextImplDirect2D::drawTiledImage(Image& image, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions& imagePaintingOptions)
{
    return GraphicsContextImpl::drawTiledImageImpl(graphicsContext(), image, destination, source, tileSize, spacing, imagePaintingOptions);
}

ImageDrawResult GraphicsContextImplDirect2D::drawTiledImage(Image& image, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions& imagePaintingOptions)
{
    return GraphicsContextImpl::drawTiledImageImpl(graphicsContext(), image, destination, source, tileScaleFactor, hRule, vRule, imagePaintingOptions);
}

void GraphicsContextImplDirect2D::drawNativeImage(const NativeImagePtr& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    auto& state = graphicsContext().state();
    Direct2D::drawNativeImage(m_platformContext, image.get(), imageSize, destRect, srcRect, options, state.alpha, Direct2D::ShadowState(state));
}

void GraphicsContextImplDirect2D::drawPattern(Image& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize&, const ImagePaintingOptions& options)
{
    auto* context = &graphicsContext();
    if (auto surface = image.nativeImageForCurrentFrame(context))
        Direct2D::drawPattern(m_platformContext, WTFMove(surface), IntSize(image.size()), destRect, tileRect, patternTransform, phase, options.compositeOperator(), options.blendMode());
}

void GraphicsContextImplDirect2D::drawRect(const FloatRect& rect, float borderThickness)
{
    auto& state = graphicsContext().state();
    Direct2D::drawRect(m_platformContext, rect, borderThickness, state.fillColor, state.strokeStyle, state.strokeColor);
}

void GraphicsContextImplDirect2D::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    auto& state = graphicsContext().state();
    Direct2D::drawLine(m_platformContext, point1, point2, state.strokeStyle, state.strokeColor, state.strokeThickness, state.shouldAntialias);
}

void GraphicsContextImplDirect2D::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleUnderlines)
{
    auto& state = graphicsContext().state();
    Direct2D::drawLinesForText(m_platformContext, point, thickness, widths, printing, doubleUnderlines, state.strokeColor);
}

void GraphicsContextImplDirect2D::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    Direct2D::drawDotsForDocumentMarker(m_platformContext, rect, style);
}

void GraphicsContextImplDirect2D::drawEllipse(const FloatRect& rect)
{
    auto& state = graphicsContext().state();
    Direct2D::fillEllipse(*platformContext(), rect, state.fillColor, state.strokeStyle, state.strokeColor, state.strokeThickness);
}

void GraphicsContextImplDirect2D::drawPath(const Path&)
{
}

void GraphicsContextImplDirect2D::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
    UNUSED_PARAM(offset);
    notImplemented();
}

void GraphicsContextImplDirect2D::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
    UNUSED_PARAM(offset);
    notImplemented();
}

void GraphicsContextImplDirect2D::save()
{
    Direct2D::save(m_platformContext);
}

void GraphicsContextImplDirect2D::restore()
{
    Direct2D::restore(m_platformContext);
}

void GraphicsContextImplDirect2D::translate(float x, float y)
{
    Direct2D::translate(m_platformContext, x, y);
}

void GraphicsContextImplDirect2D::rotate(float angleInRadians)
{
    Direct2D::rotate(m_platformContext, angleInRadians);
}

void GraphicsContextImplDirect2D::scale(const FloatSize& size)
{
    Direct2D::scale(m_platformContext, size);
}

void GraphicsContextImplDirect2D::concatCTM(const AffineTransform& transform)
{
    Direct2D::concatCTM(m_platformContext, transform);
}

void GraphicsContextImplDirect2D::setCTM(const AffineTransform& transform)
{
    Direct2D::State::setCTM(m_platformContext, transform);
}

AffineTransform GraphicsContextImplDirect2D::getCTM(GraphicsContext::IncludeDeviceScale)
{
    return Direct2D::State::getCTM(m_platformContext);
}

void GraphicsContextImplDirect2D::beginTransparencyLayer(float opacity)
{
    Direct2D::beginTransparencyLayer(m_platformContext, opacity);
}

void GraphicsContextImplDirect2D::endTransparencyLayer()
{
    Direct2D::endTransparencyLayer(m_platformContext);
}

void GraphicsContextImplDirect2D::clip(const FloatRect& rect)
{
    Direct2D::clip(m_platformContext, rect);
}

void GraphicsContextImplDirect2D::clipOut(const FloatRect& rect)
{
    Direct2D::clipOut(m_platformContext, rect);
}

void GraphicsContextImplDirect2D::clipOut(const Path& path)
{
    Direct2D::clipOut(m_platformContext, path);
}

void GraphicsContextImplDirect2D::clipPath(const Path& path, WindRule clipRule)
{
    Direct2D::clipPath(m_platformContext, path, clipRule);
}

IntRect GraphicsContextImplDirect2D::clipBounds()
{
    return Direct2D::State::getClipBounds(m_platformContext);
}

void GraphicsContextImplDirect2D::clipToImageBuffer(ImageBuffer& buffer, const FloatRect& destRect)
{
    RefPtr<Image> image = buffer.copyImage(DontCopyBackingStore);
    if (!image)
        return;

    auto* context = &graphicsContext();
    if (auto surface = image->nativeImageForCurrentFrame(context))
        notImplemented();
}

void GraphicsContextImplDirect2D::applyDeviceScaleFactor(float)
{
}

FloatRect GraphicsContextImplDirect2D::roundToDevicePixels(const FloatRect& rect, GraphicsContext::RoundingMode)
{
    return Direct2D::State::roundToDevicePixels(m_platformContext, rect);
}

void GraphicsContextImplDirect2D::clipToDrawingCommands(const FloatRect&, ColorSpace, Function<void(GraphicsContext&)>&&)
{
    // FIXME: Not implemented.
}

} // namespace WebCore

#endif // USE(DIRECT2D)
