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

GraphicsContext::GraphicsContext(cairo_t* cr)
{
    if (!cr)
        return;

    m_data = new GraphicsContextPlatformPrivate(std::make_unique<PlatformContextCairo>(cr));
    m_data->platformContext.setGraphicsContextPrivate(m_data);
}

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

AffineTransform GraphicsContext::getCTM(IncludeDeviceScale) const
{
    if (paintingDisabled())
        return AffineTransform();

    if (m_impl) {
        WTFLogAlways("GraphicsContext::getCTM() is not yet compatible with recording contexts.");
        return AffineTransform();
    }

    ASSERT(hasPlatformContext());
    return Cairo::State::getCTM(*platformContext());
}

PlatformContextCairo* GraphicsContext::platformContext() const
{
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

    Cairo::State::setShadowValues(*platformContext(), FloatSize { m_state.shadowBlur, m_state.shadowBlur },
        m_state.shadowOffset, m_state.shadowColor, m_state.shadowsIgnoreTransforms);
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
    Cairo::drawRect(*platformContext(), rect, borderThickness, state());
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
    Cairo::drawNativeImage(*platformContext(), image.get(), destRect, srcRect, compositeOperator, blendMode, orientation, *this);
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
    Cairo::drawLine(*platformContext(), point1, point2, state());
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
    Cairo::drawEllipse(*platformContext(), rect, state());
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
    Cairo::fillPath(*platformContext(), path, Cairo::FillSource(state()), *this);
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
    Cairo::strokePath(*platformContext(), path, Cairo::StrokeSource(state()), *this);
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
    Cairo::fillRect(*platformContext(), rect, Cairo::FillSource(state()), *this);
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
    Cairo::fillRect(*platformContext(), rect, color, hasShadow(), *this);
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

    if (m_impl) {
        WTFLogAlways("Getting the clip bounds not yet supported with display lists");
        return IntRect(-2048, -2048, 4096, 4096); // FIXME: display lists.
    }

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

void GraphicsContext::drawLineForText(const FloatPoint& origin, float width, bool printing, bool doubleUnderlines, StrokeStyle)
{
    drawLinesForText(origin, DashArray { width, 0 }, printing, doubleUnderlines);
}

void GraphicsContext::drawLinesForText(const FloatPoint& point, const DashArray& widths, bool printing, bool doubleUnderlines, StrokeStyle)
{
    if (paintingDisabled())
        return;

    if (widths.isEmpty())
        return;

    if (m_impl) {
        m_impl->drawLinesForText(point, widths, printing, doubleUnderlines, strokeThickness());
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::drawLinesForText(*platformContext(), point, widths, printing, doubleUnderlines, m_state.strokeColor, m_state.strokeThickness);
}

void GraphicsContext::updateDocumentMarkerResources()
{
    // Unnecessary, since our document markers don't use resources.
}

void GraphicsContext::drawLineForDocumentMarker(const FloatPoint& origin, float width, DocumentMarkerLineStyle style)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawLineForDocumentMarker(origin, width, style);
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::drawLineForDocumentMarker(*platformContext(), origin, width, style);
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect, RoundingMode)
{
    if (paintingDisabled())
        return rect;

    if (m_impl) {
        WTFLogAlways("GraphicsContext::roundToDevicePixels() is not yet compatible with recording contexts.");
        return rect;
    }

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
        WTFLogAlways("GraphicsContext::setCTM() is not compatible with recording contexts.");
        return;
    }

    ASSERT(hasPlatformContext());
    Cairo::State::setCTM(*platformContext(), transform);
}

void GraphicsContext::setPlatformShadow(const FloatSize& offset, float blur, const Color& color)
{
    if (paintingDisabled())
        return;

    FloatSize adjustedOffset = offset;
    if (m_state.shadowsIgnoreTransforms) {
        // Meaning that this graphics context is associated with a CanvasRenderingContext
        // We flip the height since CG and HTML5 Canvas have opposite Y axis
        adjustedOffset.setHeight(-offset.height());
        m_state.shadowOffset = adjustedOffset;
    }

    ASSERT(hasPlatformContext());
    Cairo::State::setShadowValues(*platformContext(), FloatSize { blur, blur },
        adjustedOffset, color, m_state.shadowsIgnoreTransforms);
}

void GraphicsContext::clearPlatformShadow()
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());
    Cairo::State::clearShadow(*platformContext());
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
    Cairo::strokeRect(*platformContext(), rect, lineWidth, Cairo::StrokeSource(state()), *this);
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

void GraphicsContext::setPlatformAlpha(float alpha)
{
    Cairo::State::setGlobalAlpha(*platformContext(), alpha);
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
    Cairo::fillRoundedRect(*platformContext(), rect, color, hasShadow(), *this);
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
    Cairo::fillRectWithRoundedHole(*platformContext(), rect, roundedHoleRect, Cairo::FillSource(state), Cairo::ShadowBlurUsage(state), *this);
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

void GraphicsContext::setPlatformImageInterpolationQuality(InterpolationQuality quality)
{
    ASSERT(hasPlatformContext());
    Cairo::State::setImageInterpolationQuality(*platformContext(), quality);
}

bool GraphicsContext::isAcceleratedContext() const
{
    if (!hasPlatformContext())
        return false;

    return Cairo::State::isAcceleratedContext(*platformContext());
}

} // namespace WebCore

#endif // USE(CAIRO)
