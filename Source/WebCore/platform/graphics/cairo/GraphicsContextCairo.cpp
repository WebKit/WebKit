/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Brent Fulgham <bfulgham@webkit.org>
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012, Intel Corporation
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
#include "GraphicsContextCairo.h"

#if USE(CAIRO)

#include "AffineTransform.h"
#include "CairoOperations.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "RefPtrCairo.h"

#if PLATFORM(WIN)
#include <cairo-win32.h>
#endif

#if PLATFORM(WPE) || PLATFORM(GTK)
#include "ThemeAdwaita.h"
#endif

namespace WebCore {

// Encapsulates the additional painting state information we store for each
// pushed graphics state.
class GraphicsContextCairo::CairoState {
public:
    CairoState() = default;

    struct {
        RefPtr<cairo_pattern_t> pattern;
        cairo_matrix_t matrix;
    } m_mask;
};

GraphicsContextCairo::GraphicsContextCairo(RefPtr<cairo_t>&& context)
    : m_cr(WTFMove(context))
{
    m_cairoStateStack.append(CairoState());
    m_cairoState = &m_cairoStateStack.last();
}

GraphicsContextCairo::GraphicsContextCairo(cairo_surface_t* surface)
    : GraphicsContextCairo(adoptRef(cairo_create(surface)))
{
}

GraphicsContextCairo::~GraphicsContextCairo() = default;

bool GraphicsContextCairo::hasPlatformContext() const
{
    return true;
}

AffineTransform GraphicsContextCairo::getCTM(IncludeDeviceScale includeScale) const
{
    UNUSED_PARAM(includeScale);
    return Cairo::State::getCTM(*platformContext());
}

GraphicsContextCairo* GraphicsContextCairo::platformContext() const
{
    return const_cast<GraphicsContextCairo*>(this);
}

void GraphicsContextCairo::save()
{
    GraphicsContext::save();

    m_cairoStateStack.append(CairoState());
    m_cairoState = &m_cairoStateStack.last();

    cairo_save(m_cr.get());
}

void GraphicsContextCairo::restore()
{
    if (!stackSize())
        return;

    GraphicsContext::restore();

    if (m_cairoStateStack.isEmpty())
        return;

    if (m_cairoState->m_mask.pattern) {
        cairo_pop_group_to_source(m_cr.get());

        cairo_matrix_t matrix;
        cairo_get_matrix(m_cr.get(), &matrix);
        cairo_set_matrix(m_cr.get(), &m_cairoState->m_mask.matrix);
        cairo_mask(m_cr.get(), m_cairoState->m_mask.pattern.get());
        cairo_set_matrix(m_cr.get(), &matrix);
    }

    m_cairoStateStack.removeLast();
    ASSERT(!m_cairoStateStack.isEmpty());
    m_cairoState = &m_cairoStateStack.last();

    cairo_restore(m_cr.get());
}

// Draws a filled rectangle with a stroked border.
void GraphicsContextCairo::drawRect(const FloatRect& rect, float borderThickness)
{
    ASSERT(!rect.isEmpty());
    auto& state = this->state();
    Cairo::drawRect(*this, rect, borderThickness, state.fillColor, state.strokeStyle, state.strokeColor);
}

void GraphicsContextCairo::drawNativeImage(NativeImage& nativeImage, const FloatSize&, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    auto& state = this->state();
    Cairo::drawPlatformImage(*this, nativeImage.platformImage().get(), destRect, srcRect, { options, state.imageInterpolationQuality }, state.alpha, Cairo::ShadowState(state));
}

// This is only used to draw borders, so we should not draw shadows.
void GraphicsContextCairo::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    if (strokeStyle() == NoStroke)
        return;

    auto& state = this->state();
    Cairo::drawLine(*this, point1, point2, state.strokeStyle, state.strokeColor, state.strokeThickness, state.shouldAntialias);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContextCairo::drawEllipse(const FloatRect& rect)
{
    auto& state = this->state();
    Cairo::drawEllipse(*this, rect, state.fillColor, state.strokeStyle, state.strokeColor, state.strokeThickness);
}

void GraphicsContextCairo::fillPath(const Path& path)
{
    if (path.isEmpty())
        return;

    auto& state = this->state();
    Cairo::fillPath(*this, path, Cairo::FillSource(state), Cairo::ShadowState(state));
}

void GraphicsContextCairo::strokePath(const Path& path)
{
    if (path.isEmpty())
        return;

    auto& state = this->state();
    Cairo::strokePath(*this, path, Cairo::StrokeSource(state), Cairo::ShadowState(state));
}

void GraphicsContextCairo::fillRect(const FloatRect& rect)
{
    auto& state = this->state();
    Cairo::fillRect(*this, rect, Cairo::FillSource(state), Cairo::ShadowState(state));
}

void GraphicsContextCairo::fillRect(const FloatRect& rect, const Color& color)
{
    Cairo::fillRect(*this, rect, color, Cairo::ShadowState(state()));
}

void GraphicsContextCairo::fillRect(const FloatRect& rect, Gradient& gradient)
{
    auto& state = this->state();
    auto pattern = gradient.createPattern(1.0, state.fillGradientSpaceTransform);
    if (!pattern)
        return;

    save();
    Cairo::fillRect(*this, rect, pattern.get());
    restore();
}

void GraphicsContextCairo::fillRect(const FloatRect& rect, const Color& color, CompositeOperator compositeOperator, BlendMode blendMode)
{
    auto& state = this->state();
    CompositeOperator previousOperator = state.compositeOperator;

    Cairo::State::setCompositeOperation(*this, compositeOperator, blendMode);
    Cairo::fillRect(*this, rect, color, Cairo::ShadowState(state));
    Cairo::State::setCompositeOperation(*this, previousOperator, BlendMode::Normal);
}

void GraphicsContextCairo::clip(const FloatRect& rect)
{
    Cairo::clip(*this, rect);
}

void GraphicsContextCairo::clipPath(const Path& path, WindRule clipRule)
{
    Cairo::clipPath(*this, path, clipRule);
}

IntRect GraphicsContextCairo::clipBounds() const
{
    return Cairo::State::getClipBounds(*platformContext());
}

void GraphicsContextCairo::clipToImageBuffer(ImageBuffer& buffer, const FloatRect& destRect)
{
    if (auto nativeImage = buffer.copyNativeImage(DontCopyBackingStore))
        Cairo::clipToImageBuffer(*this, nativeImage->platformImage().get(), destRect);
}

void GraphicsContextCairo::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
#if PLATFORM(WPE) || PLATFORM(GTK)
    ThemeAdwaita::paintFocus(*this, path, color);
    UNUSED_PARAM(width);
    UNUSED_PARAM(offset);
    return;
#else
    Cairo::drawFocusRing(*this, path, width, color);
#endif
}

void GraphicsContextCairo::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
#if PLATFORM(WPE) || PLATFORM(GTK)
    ThemeAdwaita::paintFocus(*this, rects, color);
    UNUSED_PARAM(width);
    UNUSED_PARAM(offset);
    return;
#else
    Cairo::drawFocusRing(*this, rects, width, color);
#endif
}

void GraphicsContextCairo::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleUnderlines, StrokeStyle)
{
    if (widths.isEmpty())
        return;
    Cairo::drawLinesForText(*this, point, thickness, widths, printing, doubleUnderlines, m_state.strokeColor);
}

void GraphicsContextCairo::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    Cairo::drawDotsForDocumentMarker(*this, rect, style);
}

FloatRect GraphicsContextCairo::roundToDevicePixels(const FloatRect& rect, RoundingMode roundingMode)
{
    UNUSED_PARAM(roundingMode);
    return Cairo::State::roundToDevicePixels(*this, rect);
}

void GraphicsContextCairo::translate(float x, float y)
{
    Cairo::translate(*this, x, y);
}

void GraphicsContextCairo::updateState(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    if (flags & GraphicsContextState::StrokeThicknessChange)
        Cairo::State::setStrokeThickness(*this, state.strokeThickness);

    if (flags & GraphicsContextState::StrokeStyleChange)
        Cairo::State::setStrokeStyle(*this, state.strokeStyle);

    if (flags & GraphicsContextState::ShadowChange) {
        if (state.shadowsIgnoreTransforms) {
            // Meaning that this graphics context is associated with a CanvasRenderingContext
            // We flip the height since CG and HTML5 Canvas have opposite Y axis
            auto& shadowOffset = state.shadowOffset;
            m_state.shadowOffset = { shadowOffset.width(), -shadowOffset.height() };
        }
    }

    if (flags & GraphicsContextState::CompositeOperationChange)
        Cairo::State::setCompositeOperation(*this, state.compositeOperator, state.blendMode);

    if (flags & GraphicsContextState::ShouldAntialiasChange)
        Cairo::State::setShouldAntialias(*this, state.shouldAntialias);
}

void GraphicsContextCairo::concatCTM(const AffineTransform& transform)
{
    Cairo::concatCTM(*this, transform);
}

void GraphicsContextCairo::setCTM(const AffineTransform& transform)
{
    Cairo::State::setCTM(*this, transform);
}

void GraphicsContextCairo::beginTransparencyLayer(float opacity)
{
    GraphicsContext::beginTransparencyLayer(opacity);
    Cairo::beginTransparencyLayer(*this, opacity);
}

void GraphicsContextCairo::endTransparencyLayer()
{
    GraphicsContext::endTransparencyLayer();
    Cairo::endTransparencyLayer(*this);
}

void GraphicsContextCairo::clearRect(const FloatRect& rect)
{
    Cairo::clearRect(*this, rect);
}

void GraphicsContextCairo::strokeRect(const FloatRect& rect, float lineWidth)
{
    auto& state = this->state();
    Cairo::strokeRect(*this, rect, lineWidth, Cairo::StrokeSource(state), Cairo::ShadowState(state));
}

void GraphicsContextCairo::setLineCap(LineCap lineCap)
{
    Cairo::setLineCap(*this, lineCap);
}

void GraphicsContextCairo::setLineDash(const DashArray& dashes, float dashOffset)
{
    Cairo::setLineDash(*this, dashes, dashOffset);
}

void GraphicsContextCairo::setLineJoin(LineJoin lineJoin)
{
    Cairo::setLineJoin(*this, lineJoin);
}

void GraphicsContextCairo::setMiterLimit(float miter)
{
    Cairo::setMiterLimit(*this, miter);
}

void GraphicsContextCairo::clipOut(const Path& path)
{
    Cairo::clipOut(*this, path);
}

void GraphicsContextCairo::rotate(float radians)
{
    Cairo::rotate(*this, radians);
}

void GraphicsContextCairo::scale(const FloatSize& size)
{
    Cairo::scale(*this, size);
}

void GraphicsContextCairo::clipOut(const FloatRect& rect)
{
    Cairo::clipOut(*this, rect);
}

void GraphicsContextCairo::fillRoundedRectImpl(const FloatRoundedRect& rect, const Color& color)
{
    Cairo::fillRoundedRect(*this, rect, color, Cairo::ShadowState(state()));
}

void GraphicsContextCairo::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    if (!color.isValid())
        return;

    auto& state = this->state();
    Cairo::fillRectWithRoundedHole(*this, rect, roundedHoleRect, Cairo::FillSource(state), Cairo::ShadowState(state));
}

void GraphicsContextCairo::drawPattern(NativeImage& nativeImage, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    if (!patternTransform.isInvertible())
        return;

    UNUSED_PARAM(spacing);
    Cairo::drawPattern(*this, nativeImage.platformImage().get(), IntSize(imageSize), destRect, tileRect, patternTransform, phase, options);
}

RenderingMode GraphicsContextCairo::renderingMode() const
{
    return Cairo::State::isAcceleratedContext(*platformContext()) ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;
}

void GraphicsContextCairo::drawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& point, FontSmoothingMode fontSmoothing)
{
    if (!font.platformData().size())
        return;

    auto xOffset = point.x();
    Vector<cairo_glyph_t> cairoGlyphs(numGlyphs);
    {
        auto yOffset = point.y();
        for (size_t i = 0; i < numGlyphs; ++i) {
            cairoGlyphs[i] = { glyphs[i], xOffset, yOffset };
            xOffset += advances[i].width();
            yOffset += advances[i].height();
        }
    }

    cairo_scaled_font_t* scaledFont = font.platformData().scaledFont();
    double syntheticBoldOffset = font.syntheticBoldOffset();

    if (!font.allowsAntialiasing())
        fontSmoothing = FontSmoothingMode::NoSmoothing;

    auto& state = this->state();
    Cairo::drawGlyphs(*this, Cairo::FillSource(state), Cairo::StrokeSource(state),
        Cairo::ShadowState(state), point, scaledFont, syntheticBoldOffset, cairoGlyphs, xOffset,
        state.textDrawingMode, state.strokeThickness, state.shadowOffset, state.shadowColor,
        fontSmoothing);
}

cairo_t* GraphicsContextCairo::cr() const
{
    return m_cr.get();
}

Vector<float>& GraphicsContextCairo::layers()
{
    return m_layers;
}

void GraphicsContextCairo::pushImageMask(cairo_surface_t* surface, const FloatRect& rect)
{
    // We must call savePlatformState at least once before we can use image masking,
    // since we actually apply the mask in restorePlatformState.
    ASSERT(!m_cairoStateStack.isEmpty());
    m_cairoState->m_mask.pattern = adoptRef(cairo_pattern_create_for_surface(surface));
    cairo_get_matrix(m_cr.get(), &m_cairoState->m_mask.matrix);

    cairo_matrix_t matrix;
    cairo_matrix_init_translate(&matrix, -rect.x(), -rect.y());
    cairo_pattern_set_matrix(m_cairoState->m_mask.pattern.get(), &matrix);

    cairo_push_group(m_cr.get());
}

} // namespace WebCore

#endif // USE(CAIRO)
