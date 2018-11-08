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
#include "GraphicsContext.h"

#if USE(CAIRO)

#include "AffineTransform.h"
#include "CairoOperations.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "GraphicsContextImpl.h"
#include "GraphicsContextPlatformPrivateCairo.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "PlatformContextCairo.h"
#include "RefPtrCairo.h"

#if PLATFORM(WIN)
#include <cairo-win32.h>
#endif


namespace WebCore {

void GraphicsContext::platformInit(PlatformContextCairo* platformContext)
{
    if (!platformContext)
        return;

    m_data = new GraphicsContextPlatformPrivate(*platformContext);
    m_data->platformContext.setGraphicsContextPrivate(m_data);
    m_data->syncContext(platformContext->cr());
}

void GraphicsContext::platformDestroy()
{
    if (m_data) {
        m_data->platformContext.setGraphicsContextPrivate(nullptr);
        delete m_data;
    }
}

AffineTransform GraphicsContext::getCTM(IncludeDeviceScale includeScale) const
{
    if (paintingDisabled())
        return AffineTransform();

    if (m_impl)
        return m_impl->getCTM(includeScale);

    ASSERT(hasPlatformContext());
    return Cairo::State::getCTM(*platformContext());
}

PlatformContextCairo* GraphicsContext::platformContext() const
{
    if (m_impl)
        return m_impl->platformContext();
    return &m_data->platformContext;
}

void GraphicsContext::savePlatformState()
{
    ASSERT(hasPlatformContext());
    Cairo::save(*platformContext());
}

void GraphicsContext::restorePlatformState()
{
    ASSERT(hasPlatformContext());
    Cairo::restore(*platformContext());
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const FloatRect& rect, float borderThickness)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawRect(rect, borderThickness);
        return;
    }

    ASSERT(!rect.isEmpty());
    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Cairo::drawRect(*platformContext(), rect, borderThickness, state.fillColor, state.strokeStyle, state.strokeColor);
}

void GraphicsContext::drawNativeImage(const NativeImagePtr& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator compositeOperator, BlendMode blendMode, ImageOrientation orientation)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawNativeImage(image, imageSize, destRect, srcRect, compositeOperator, blendMode, orientation);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Cairo::drawNativeImage(*platformContext(), image.get(), destRect, srcRect, compositeOperator, blendMode, orientation, state.imageInterpolationQuality, state.alpha, Cairo::ShadowState(state));
}

// This is only used to draw borders, so we should not draw shadows.
void GraphicsContext::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    if (paintingDisabled())
        return;

    if (strokeStyle() == NoStroke)
        return;

    if (m_impl) {
        m_impl->drawLine(point1, point2);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Cairo::drawLine(*platformContext(), point1, point2, state.strokeStyle, state.strokeColor, state.strokeThickness, state.shouldAntialias);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawEllipse(rect);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Cairo::drawEllipse(*platformContext(), rect, state.fillColor, state.strokeStyle, state.strokeColor, state.strokeThickness);
}

void GraphicsContext::fillPath(const Path& path)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    if (m_impl) {
        m_impl->fillPath(path);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Cairo::fillPath(*platformContext(), path, Cairo::FillSource(state), Cairo::ShadowState(state));
}

void GraphicsContext::strokePath(const Path& path)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    if (m_impl) {
        m_impl->strokePath(path);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Cairo::strokePath(*platformContext(), path, Cairo::StrokeSource(state), Cairo::ShadowState(state));
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->fillRect(rect);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Cairo::fillRect(*platformContext(), rect, Cairo::FillSource(state), Cairo::ShadowState(state));
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->fillRect(rect, color);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::fillRect(*platformContext(), rect, color, Cairo::ShadowState(state()));
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clip(rect);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::clip(*platformContext(), rect);
}

void GraphicsContext::clipPath(const Path& path, WindRule clipRule)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipPath(path, clipRule);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::clipPath(*platformContext(), path, clipRule);
}

void GraphicsContext::clipToImageBuffer(ImageBuffer& buffer, const FloatRect& destRect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipToImageBuffer(buffer, destRect);
        return;
    }

    RefPtr<Image> image = buffer.copyImage(DontCopyBackingStore);
    if (!image)
        return;

    ASSERT(hasPlatformContext());
    if (auto surface = image->nativeImageForCurrentFrame())
        Cairo::clipToImageBuffer(*platformContext(), surface.get(), destRect);
}

IntRect GraphicsContext::clipBounds() const
{
    if (paintingDisabled())
        return IntRect();

    if (m_impl)
        return m_impl->clipBounds();

    ASSERT(hasPlatformContext());
    return Cairo::State::getClipBounds(*platformContext());
}

void GraphicsContext::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawFocusRing(path, width, offset, color);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::drawFocusRing(*platformContext(), path, width, color);
}

void GraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawFocusRing(rects, width, offset, color);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::drawFocusRing(*platformContext(), rects, width, color);
}

void GraphicsContext::drawLineForText(const FloatRect& rect, bool printing, bool doubleUnderlines, StrokeStyle)
{
    drawLinesForText(rect.location(), rect.height(), DashArray { 0, rect.width() }, printing, doubleUnderlines);
}

void GraphicsContext::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleUnderlines, StrokeStyle)
{
    if (paintingDisabled())
        return;

    if (widths.isEmpty())
        return;

    if (m_impl) {
        m_impl->drawLinesForText(point, thickness, widths, printing, doubleUnderlines);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::drawLinesForText(*platformContext(), point, thickness, widths, printing, doubleUnderlines, m_state.strokeColor);
}

void GraphicsContext::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawDotsForDocumentMarker(rect, style);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::drawDotsForDocumentMarker(*platformContext(), rect, style);
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect, RoundingMode roundingMode)
{
    if (paintingDisabled())
        return rect;

    if (m_impl)
        return m_impl->roundToDevicePixels(rect, roundingMode);

    return Cairo::State::roundToDevicePixels(*platformContext(), rect);
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->translate(x, y);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::translate(*platformContext(), x, y);
}

void GraphicsContext::setPlatformFillColor(const Color&)
{
    // Cairo contexts can't hold separate fill and stroke colors
    // so we set them just before we actually fill or stroke
}

void GraphicsContext::setPlatformStrokeColor(const Color&)
{
    // Cairo contexts can't hold separate fill and stroke colors
    // so we set them just before we actually fill or stroke
}

void GraphicsContext::setPlatformStrokeThickness(float strokeThickness)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());
    Cairo::State::setStrokeThickness(*platformContext(), strokeThickness);
}

void GraphicsContext::setPlatformStrokeStyle(StrokeStyle strokeStyle)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());
    Cairo::State::setStrokeStyle(*platformContext(), strokeStyle);
}

void GraphicsContext::setURLForRect(const URL&, const FloatRect&)
{
    notImplemented();
}

void GraphicsContext::concatCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->concatCTM(transform);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::concatCTM(*platformContext(), transform);
}

void GraphicsContext::setCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setCTM(transform);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::State::setCTM(*platformContext(), transform);
}

void GraphicsContext::setPlatformShadow(const FloatSize& offset, float, const Color&)
{
    if (m_state.shadowsIgnoreTransforms) {
        // Meaning that this graphics context is associated with a CanvasRenderingContext
        // We flip the height since CG and HTML5 Canvas have opposite Y axis
        m_state.shadowOffset = { offset.width(), -offset.height() };
    }
}

void GraphicsContext::clearPlatformShadow()
{
}

void GraphicsContext::beginPlatformTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());
    Cairo::beginTransparencyLayer(*platformContext(), opacity);
}

void GraphicsContext::endPlatformTransparencyLayer()
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());
    Cairo::endTransparencyLayer(*platformContext());
}

bool GraphicsContext::supportsTransparencyLayers()
{
    return true;
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clearRect(rect);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::clearRect(*platformContext(), rect);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->strokeRect(rect, lineWidth);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Cairo::strokeRect(*platformContext(), rect, lineWidth, Cairo::StrokeSource(state), Cairo::ShadowState(state));
}

void GraphicsContext::setLineCap(LineCap lineCap)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setLineCap(lineCap);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::setLineCap(*platformContext(), lineCap);
}

void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setLineDash(dashes, dashOffset);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::setLineDash(*platformContext(), dashes, dashOffset);
}

void GraphicsContext::setLineJoin(LineJoin lineJoin)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setLineJoin(lineJoin);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::setLineJoin(*platformContext(), lineJoin);
}

void GraphicsContext::setMiterLimit(float miter)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        // Maybe this should be part of the state.
        m_impl->setMiterLimit(miter);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::setMiterLimit(*platformContext(), miter);
}

void GraphicsContext::setPlatformAlpha(float)
{
}

void GraphicsContext::setPlatformCompositeOperation(CompositeOperator compositeOperator, BlendMode blendMode)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());
    Cairo::State::setCompositeOperation(*platformContext(), compositeOperator, blendMode);
}

void GraphicsContext::canvasClip(const Path& path, WindRule windRule)
{
    clipPath(path, windRule);
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipOut(path);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::clipOut(*platformContext(), path);
}

void GraphicsContext::rotate(float radians)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->rotate(radians);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::rotate(*platformContext(), radians);
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->scale(size);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::scale(*platformContext(), size);
}

void GraphicsContext::clipOut(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipOut(rect);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::clipOut(*platformContext(), rect);
}

void GraphicsContext::platformFillRoundedRect(const FloatRoundedRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());
    Cairo::fillRoundedRect(*platformContext(), rect, color, Cairo::ShadowState(state()));
}

void GraphicsContext::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    if (paintingDisabled() || !color.isValid())
        return;

    if (m_impl) {
        m_impl->fillRectWithRoundedHole(rect, roundedHoleRect, color);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Cairo::fillRectWithRoundedHole(*platformContext(), rect, roundedHoleRect, Cairo::FillSource(state), Cairo::ShadowState(state));
}

void GraphicsContext::drawPattern(Image& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator compositeOperator, BlendMode blendMode)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawPattern(image, destRect, tileRect, patternTransform, phase, spacing, compositeOperator, blendMode);
        return;
    }

    ASSERT(hasPlatformContext());
    if (auto surface = image.nativeImageForCurrentFrame())
        Cairo::drawPattern(*platformContext(), surface.get(), IntSize(image.size()), destRect, tileRect, patternTransform, phase, compositeOperator, blendMode);
}

void GraphicsContext::setPlatformShouldAntialias(bool enable)
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());
    Cairo::State::setShouldAntialias(*platformContext(), enable);
}

void GraphicsContext::setPlatformImageInterpolationQuality(InterpolationQuality)
{
}

bool GraphicsContext::isAcceleratedContext() const
{
    if (!hasPlatformContext())
        return false;

    return Cairo::State::isAcceleratedContext(*platformContext());
}

} // namespace WebCore

#endif // USE(CAIRO)
