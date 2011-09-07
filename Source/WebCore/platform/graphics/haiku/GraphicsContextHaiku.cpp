/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "AffineTransform.h"
#include "Color.h"
#include "Font.h"
#include "FontData.h"
#include "NotImplemented.h"
#include "Path.h"
#include <wtf/text/CString.h>
#include <GraphicsDefs.h>
#include <Region.h>
#include <View.h>
#include <Window.h>
#include <stdio.h>


namespace WebCore {

class GraphicsContextPlatformPrivate {
public:
    GraphicsContextPlatformPrivate(BView* view);
    ~GraphicsContextPlatformPrivate();

    BView* m_view;
};

GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(BView* view)
    : m_view(view)
{
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
}

void GraphicsContext::platformInit(PlatformGraphicsContext* context)
{
    m_data = new GraphicsContextPlatformPrivate(context);
    setPaintingDisabled(!context);
}

void GraphicsContext::platformDestroy()
{
    delete m_data;
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    return m_data->m_view;
}

void GraphicsContext::savePlatformState()
{
    m_data->m_view->PushState();
}

void GraphicsContext::restorePlatformState()
{
    m_data->m_view->PopState();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    m_data->m_view->FillRect(rect);
    if (strokeStyle() != NoStroke)
        m_data->m_view->StrokeRect(rect, getHaikuStrokeStyle());
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    if (strokeStyle() == NoStroke)
        return;

    m_data->m_view->StrokeLine(point1, point2, getHaikuStrokeStyle());
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    m_data->m_view->FillEllipse(rect);
    if (strokeStyle() != NoStroke)
        m_data->m_view->StrokeEllipse(rect, getHaikuStrokeStyle());
}

void GraphicsContext::strokeArc(const IntRect& rect, int startAngle, int angleSpan)
{
    if (paintingDisabled())
        return;

    m_data->m_view->StrokeArc(rect, startAngle, angleSpan, getHaikuStrokeStyle());
}

void GraphicsContext::strokePath(const Path&)
{
    notImplemented();
}

void GraphicsContext::drawConvexPolygon(size_t pointsLength, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    BPoint bPoints[pointsLength];
    for (size_t i = 0; i < pointsLength; i++)
        bPoints[i] = points[i];

    m_data->m_view->FillPolygon(bPoints, pointsLength);
    if (strokeStyle() != NoStroke)
        // Stroke with low color
        m_data->m_view->StrokePolygon(bPoints, pointsLength, true, getHaikuStrokeStyle());
}

void GraphicsContext::clipConvexPolygon(size_t numPoints, const FloatPoint* points, bool antialiased)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;
    
    // FIXME: IMPLEMENT!!
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    rgb_color oldColor = m_data->m_view->HighColor();
    m_data->m_view->SetHighColor(color);
    m_data->m_view->FillRect(rect);
    m_data->m_view->SetHighColor(oldColor);
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled() || !color.alpha())
        return;

    notImplemented();
    // FIXME: A simple implementation could just use FillRoundRect if all
    // the sizes are the same, or even if they are not. Otherwise several
    // FillRect and FillArc calls are needed.
}

void GraphicsContext::fillPath(const Path&)
{
    notImplemented();
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    BRegion region(rect);
    m_data->m_view->ConstrainClippingRegion(&region);
}

void GraphicsContext::drawFocusRing(const Path& path, int width, int offset, const Color& color)
{
    // FIXME: implement
}

void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects, int /* width */, int /* offset */, const Color& color)
{
    if (paintingDisabled())
        return;

    unsigned rectCount = rects.size();

    // FIXME: maybe we should implement this with BShape?

    if (rects.size() > 1) {
        BRegion    region;
        for (int i = 0; i < rectCount; ++i)
            region.Include(BRect(rects[i]));

        m_data->m_view->SetHighColor(color);
        m_data->m_view->StrokeRect(region.Frame(), B_MIXED_COLORS);
    }
}

void GraphicsContext::drawLineForText(const IntPoint& origin, int width, bool printing)
{
    if (paintingDisabled())
        return;

    IntPoint endPoint = origin + IntSize(width, 0);
    drawLine(origin, endPoint);
}

void GraphicsContext::drawLineForTextChecking(const IntPoint&, int width, TextCheckingLineStyle)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect)
{
    notImplemented();
    return rect;
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::strokeRect(const FloatRect& rect, float width)
{
    if (paintingDisabled())
        return;

    float oldSize = m_data->m_view->PenSize();
    m_data->m_view->SetPenSize(width);
    m_data->m_view->StrokeRect(rect, getHaikuStrokeStyle());
    m_data->m_view->SetPenSize(oldSize);
}

void GraphicsContext::setLineCap(LineCap lineCap)
{
    if (paintingDisabled())
        return;

    cap_mode mode = B_BUTT_CAP;
    switch (lineCap) {
    case RoundCap:
        mode = B_ROUND_CAP;
        break;
    case SquareCap:
        mode = B_SQUARE_CAP;
        break;
    case ButtCap:
    default:
        break;
    }

    m_data->m_view->SetLineMode(mode, m_data->m_view->LineJoinMode(), m_data->m_view->LineMiterLimit());
}

void GraphicsContext::setLineJoin(LineJoin lineJoin)
{
    if (paintingDisabled())
        return;

    join_mode mode = B_MITER_JOIN;
    switch (lineJoin) {
    case RoundJoin:
        mode = B_ROUND_JOIN;
        break;
    case BevelJoin:
        mode = B_BEVEL_JOIN;
        break;
    case MiterJoin:
    default:
        break;
    }

    m_data->m_view->SetLineMode(m_data->m_view->LineCapMode(), mode, m_data->m_view->LineMiterLimit());
}

void GraphicsContext::setMiterLimit(float limit)
{
    if (paintingDisabled())
        return;

    m_data->m_view->SetLineMode(m_data->m_view->LineCapMode(), m_data->m_view->LineJoinMode(), limit);
}

void GraphicsContext::setAlpha(float opacity)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::setPlatformCompositeOperation(CompositeOperator op)
{
    if (paintingDisabled())
        return;

    drawing_mode mode = B_OP_COPY;
    switch (op) {
    case CompositeClear:
    case CompositeCopy:
        // Use the default above
        break;
    case CompositeSourceOver:
        mode = B_OP_OVER;
        break;
    default:
        printf("GraphicsContext::setPlatformCompositeOperation: Unsupported composite operation %s\n",
                compositeOperatorName(op).utf8().data());
    }
    m_data->m_view->SetDrawingMode(mode);
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    m_data->m_view->ConstrainClippingRegion(path.platformPath());
}

void GraphicsContext::canvasClip(const Path& path)
{
    clip(path);
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::clipToImageBuffer(const FloatRect&, const ImageBuffer*)
{
    notImplemented();
}

AffineTransform GraphicsContext::getCTM() const
{
    notImplemented();
    return AffineTransform();
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::rotate(float radians)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::clipOut(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::concatCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::setCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::setPlatformShouldAntialias(bool enable)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality)
{
}

InterpolationQuality GraphicsContext::imageInterpolationQuality() const
{
    notImplemented();
    return InterpolationDefault;
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    notImplemented();
}

void GraphicsContext::setPlatformFont(const Font& font)
{
    m_data->m_view->SetFont(font.primaryFont()->platformData().font());
}

void GraphicsContext::setPlatformStrokeColor(const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    m_data->m_view->SetHighColor(color);
}

pattern GraphicsContext::getHaikuStrokeStyle()
{
    switch (strokeStyle()) {
    case SolidStroke:
        return B_SOLID_HIGH;
        break;
    case DottedStroke:
        return B_MIXED_COLORS;
        break;
    case DashedStroke:
        // FIXME: use a better dashed stroke!
        notImplemented();
        return B_MIXED_COLORS;
        break;
    default:
        return B_SOLID_LOW;
        break;
    }
}

void GraphicsContext::setPlatformStrokeStyle(StrokeStyle strokeStyle)
{
    // FIXME: see getHaikuStrokeStyle.
    notImplemented();
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    if (paintingDisabled())
        return;

    m_data->m_view->SetPenSize(thickness);
}

void GraphicsContext::setPlatformFillColor(const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    m_data->m_view->SetHighColor(color);
}

void GraphicsContext::clearPlatformShadow()
{
    notImplemented();
}

void GraphicsContext::setPlatformShadow(FloatSize const&, float, Color const&, ColorSpace)
{
    notImplemented();
}

} // namespace WebCore

