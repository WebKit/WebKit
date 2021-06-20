/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 * Copyright (C) 2015 Julian Harnath <julian.harnath@rwth-aachen.de>
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
#include "GraphicsContextHaiku.h"

#include "AffineTransform.h"
#include "Color.h"
#include "DisplayListRecorder.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "NotImplemented.h"
#include "Path.h"
#include "TransformationMatrix.h"

#include <wtf/text/CString.h>
#include <Bitmap.h>
#include <GraphicsDefs.h>
#include <Picture.h>
#include <Region.h>
#include <Shape.h>
#include <Window.h>
#include <stdio.h>

namespace WebCore {


GraphicsContextHaiku::GraphicsContextHaiku(BView* view)
    : m_view(view)
    , m_strokeStyle(B_SOLID_HIGH)
{
}

GraphicsContextHaiku::~GraphicsContextHaiku()
{
}

// Draws a filled rectangle with a stroked border.
void GraphicsContextHaiku::drawRect(const FloatRect& rect, float borderThickness)
{
    if (m_state.fillPattern)
        notImplemented();
    else if (m_state.fillGradient) {
        m_state.fillGradient->fill(*this, rect);
    } else
        m_view->FillRect(rect);

    // TODO: Support gradients
    if (strokeStyle() != NoStroke && borderThickness > 0.0f && strokeColor().isVisible())
    {
        m_view->SetPenSize(borderThickness);
        m_view->StrokeRect(rect, m_strokeStyle);
    }
}

void GraphicsContextHaiku::drawNativeImage(NativeImage& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    m_view->PushState();
    setCompositeOperation(options.compositeOperator());

    // Test using example site at
    // http://www.meyerweb.com/eric/css/edge/complexspiral/demo.html
    m_view->SetDrawingMode(B_OP_ALPHA);

    uint32 flags = 0;

    // TODO handle more things from options (blend mode, etc)
    if (options.interpolationQuality() > InterpolationQuality::Low)
        flags |= B_FILTER_BITMAP_BILINEAR;

    m_view->DrawBitmapAsync(image.platformImage().get(), BRect(srcRect), BRect(destRect), flags);

    m_view->PopState();
}

// This is only used to draw borders.
void GraphicsContextHaiku::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    if (strokeStyle() == NoStroke || !strokeColor().isVisible())
        return;
    m_view->StrokeLine(point1, point2, m_strokeStyle);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContextHaiku::drawEllipse(const FloatRect& rect)
{
    if (m_state.fillPattern || m_state.fillGradient || fillColor().isVisible()) {
//        TODO: What's this shadow business?
        if (m_state.fillPattern)
            notImplemented();
        else if (m_state.fillGradient) {
            const BGradient& gradient = m_state.fillGradient->getHaikuGradient();
            m_view->FillEllipse(rect, gradient);
        } else
            m_view->FillEllipse(rect);
    }

    // TODO: Support gradients
    if (strokeStyle() != NoStroke && strokeThickness() > 0.0f && strokeColor().isVisible())
        m_view->StrokeEllipse(rect, m_strokeStyle);
}

void GraphicsContextHaiku::strokeRect(const FloatRect& rect, float width)
{
    if (strokeStyle() == NoStroke || width <= 0.0f || !strokeColor().isVisible())
        return;

    float oldSize = m_view->PenSize();
    m_view->SetPenSize(width);
    // TODO stroke the shadow
    m_view->StrokeRect(rect, m_strokeStyle);
    m_view->SetPenSize(oldSize);
}

void GraphicsContextHaiku::strokePath(const Path& path)
{
    m_view->MovePenTo(B_ORIGIN);

    // TODO: stroke the shadow (cf shadowAndStrokeCurrentCairoPath)

    if (m_state.strokePattern)
        notImplemented();
    else if (m_state.strokeGradient) {
        notImplemented();
//      BGradient* gradient = m_state.strokeGradient->platformGradient();
//      m_view->StrokeShape(shape(), *gradient);
    } else if (strokeColor().isVisible()) {
        drawing_mode mode = m_view->DrawingMode();
        if (m_view->HighColor().alpha < 255)
            m_view->SetDrawingMode(B_OP_ALPHA);

        m_view->StrokeShape(path.platformPath(), m_strokeStyle);
        m_view->SetDrawingMode(mode);
    }
}

void GraphicsContextHaiku::fillRect(const FloatRect& rect, const Color& color)
{
    rgb_color previousColor = m_view->HighColor();

#if 0
    // FIXME needs support for Composite SourceIn.
    if (hasShadow()) {
        shadowBlur().drawRectShadow(this, rect, RoundedRect::Radii());
    }
#endif

    //setPlatformFillColor(color, ColorSpaceDeviceRGB);
    m_view->SetHighColor(color);
    m_view->FillRect(rect);

    m_view->SetHighColor(previousColor);
}

void GraphicsContextHaiku::fillRect(const FloatRect& rect)
{
    // TODO fill the shadow
    m_view->FillRect(rect);
}

void GraphicsContextHaiku::fillRoundedRectImpl(const FloatRoundedRect& roundRect, const Color& color)
{
    if (!color.isVisible())
        return;

    const FloatRect& rect = roundRect.rect();
    const FloatSize& topLeft = roundRect.radii().topLeft();
    const FloatSize& topRight = roundRect.radii().topRight();
    const FloatSize& bottomLeft = roundRect.radii().bottomLeft();
    const FloatSize& bottomRight = roundRect.radii().bottomRight();

#if 0
    // FIXME needs support for Composite SourceIn.
    if (hasShadow())
        shadowBlur().drawRectShadow(this, rect, FloatRoundedRect::Radii(topLeft, topRight, bottomLeft, bottomRight));
#endif

    BPoint points[3];
    const float kRadiusBezierScale = 1.0f - 0.5522847498f; //  1 - (sqrt(2) - 1) * 4 / 3

    BShape shape;
    shape.MoveTo(BPoint(rect.x() + topLeft.width(), rect.y()));
    shape.LineTo(BPoint(rect.maxX() - topRight.width(), rect.y()));
    points[0].x = rect.maxX() - kRadiusBezierScale * topRight.width();
    points[0].y = rect.y();
    points[1].x = rect.maxX();
    points[1].y = rect.y() + kRadiusBezierScale * topRight.height();
    points[2].x = rect.maxX();
    points[2].y = rect.y() + topRight.height();
    shape.BezierTo(points);
    shape.LineTo(BPoint(rect.maxX(), rect.maxY() - bottomRight.height()));
    points[0].x = rect.maxX();
    points[0].y = rect.maxY() - kRadiusBezierScale * bottomRight.height();
    points[1].x = rect.maxX() - kRadiusBezierScale * bottomRight.width();
    points[1].y = rect.maxY();
    points[2].x = rect.maxX() - bottomRight.width();
    points[2].y = rect.maxY();
    shape.BezierTo(points);
    shape.LineTo(BPoint(rect.x() + bottomLeft.width(), rect.maxY()));
    points[0].x = rect.x() + kRadiusBezierScale * bottomLeft.width();
    points[0].y = rect.maxY();
    points[1].x = rect.x();
    points[1].y = rect.maxY() - kRadiusBezierScale * bottomRight.height();
    points[2].x = rect.x();
    points[2].y = rect.maxY() - bottomRight.height();
    shape.BezierTo(points);
    shape.LineTo(BPoint(rect.x(), rect.y() + topLeft.height()));
    points[0].x = rect.x();
    points[0].y = rect.y() + kRadiusBezierScale * topLeft.height();
    points[1].x = rect.x() + kRadiusBezierScale * topLeft.width();
    points[1].y = rect.y();
    points[2].x = rect.x() + topLeft.width();
    points[2].y = rect.y();
    shape.BezierTo(points);
    shape.Close();

    rgb_color oldColor = m_view->HighColor();
    m_view->SetHighColor(color);
    m_view->MovePenTo(B_ORIGIN);
    m_view->FillShape(&shape);

    m_view->SetHighColor(oldColor);
}

void GraphicsContextHaiku::fillPath(const Path& path)
{
    m_view->SetFillRule(fillRule() == WindRule::NonZero ? B_NONZERO : B_EVEN_ODD);
    m_view->MovePenTo(B_ORIGIN);

    // TODO: Render the shadow (cf shadowAndFillCurrentCairoPath)
    drawing_mode mode = m_view->DrawingMode();

    if (m_state.fillPattern)
        notImplemented();
    else if (m_state.fillGradient) {
        m_view->SetDrawingMode(B_OP_ALPHA);
        const BGradient& gradient = m_state.fillGradient->getHaikuGradient();
        m_view->FillShape(path.platformPath(), gradient);
    } else {
        if (m_view->HighColor().alpha < 255)
            m_view->SetDrawingMode(B_OP_ALPHA);

        m_view->FillShape(path.platformPath());
    }

    m_view->SetDrawingMode(mode);
}

void GraphicsContextHaiku::clip(const FloatRect& rect)
{
    m_view->ClipToRect(rect);
}

void GraphicsContextHaiku::clipPath(const Path& path, WindRule windRule)
{
    m_view->SetFillRule(windRule == WindRule::EvenOdd ? B_EVEN_ODD : B_NONZERO);
    m_view->ClipToShape(path.platformPath());
    // TODO: reset wind rule
}


void GraphicsContextHaiku::drawPattern(NativeImage& image, const WebCore::FloatSize& size, const FloatRect& destRect,
    const FloatRect& tileRect, const AffineTransform&,
    const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions&)
{
    if (!image.platformImage()->IsValid()) // If the image hasn't fully loaded.
        return;

    // Figure out if the image has any alpha transparency, we can use faster drawing if not
    bool hasAlpha = false;

    uint8* bits = reinterpret_cast<uint8*>(image.platformImage()->Bits());
    uint32 width = image.platformImage()->Bounds().IntegerWidth() + 1;
    uint32 height = image.platformImage()->Bounds().IntegerHeight() + 1;

    uint32 bytesPerRow = image.platformImage()->BytesPerRow();
    for (uint32 y = 0; y < height && !hasAlpha; y++) {
        uint8* p = bits;
        for (uint32 x = 0; x < width && !hasAlpha; x++) {
            hasAlpha = p[3] < 255;
            p += 4;
        }
        bits += bytesPerRow;
    }

    m_view->PushState();
    if (hasAlpha)
        m_view->SetDrawingMode(B_OP_ALPHA);
    else
        m_view->SetDrawingMode(B_OP_COPY);

    clip(enclosingIntRect(destRect));
    float phaseOffsetX = destRect.x() - phase.x();
    float phaseOffsetY = destRect.y() - phase.y();
    // x mod w, y mod h
    phaseOffsetX -= std::trunc(phaseOffsetX / tileRect.width()) * tileRect.width();
    phaseOffsetY -= std::trunc(phaseOffsetY / tileRect.height()) * tileRect.height();
    m_view->DrawTiledBitmapAsync(
        image.platformImage().get(), destRect, BPoint(phaseOffsetX, phaseOffsetY));
    m_view->PopState();
}


void GraphicsContextHaiku::clipOut(const Path& path)
{
    if (path.isEmpty())
        return;

    m_view->ClipToInverseShape(path.platformPath());
}

void GraphicsContextHaiku::clipOut(const FloatRect& rect)
{
    m_view->ClipToInverseRect(rect);
}

void GraphicsContextHaiku::drawFocusRing(const Path& path, float width, float /*offset*/, const Color& color)
{
    if (width <= 0 || !color.isVisible())
        return;

    // GTK forces this to 2, we use 1. A focus ring several pixels thick doesn't
    // look good.
    width = 1;

    m_view->PushState();
    m_view->SetHighColor(color);
    m_view->SetPenSize(width);
    m_view->StrokeShape(path.platformPath(), B_SOLID_HIGH);
    m_view->PopState();
}

void GraphicsContextHaiku::drawFocusRing(const Vector<FloatRect>& rects, float width, float /* offset */, const Color& color)
{
    if (width <= 0 || !color.isVisible())
        return;

    unsigned rectCount = rects.size();
    if (rectCount <= 0)
        return;

    m_view->PushState();

    // GTK forces this to 2, we use 1. A focus ring several pixels thick doesn't
    // look good.
    // FIXME this still draws a focus ring that looks not so good on "details"
    // elements. Maybe we should disable that somewhere.
    width = 1;

    m_view->SetHighColor(color);
    m_view->SetPenSize(width);
    // FIXME: maybe we should implement this with BShape?
    for (unsigned i = 0; i < rectCount; ++i)
        m_view->StrokeRect(rects[i], B_SOLID_HIGH);

    m_view->PopState();
}

void GraphicsContextHaiku::drawLinesForText(const FloatPoint& point, float, const DashArray& widths, bool printing, bool doubleUnderlines, WebCore::StrokeStyle)
{
    if (widths.size() <= 0)
        return;

    // TODO would be faster to use BeginLineArray/EndLineArray here
    // TODO in Cairo, these are not lines, but filled rectangle? Whats the thickness?
    for (size_t i = 0; i < widths.size(); i += 2)
    {
        drawLine(
			FloatPoint(point.x() + widths[i], point.y()),
			FloatPoint(point.x() + widths[i+1], point.y()));
    }
}

void GraphicsContextHaiku::drawDotsForDocumentMarker(WebCore::FloatRect const&,
	WebCore::DocumentMarkerLineStyle)
{
	notImplemented();
}

FloatRect GraphicsContextHaiku::roundToDevicePixels(const FloatRect& rect, RoundingMode /* mode */)
{
    FloatRect rounded(rect);
    rounded.setX(roundf(rect.x()));
    rounded.setY(roundf(rect.y()));
    rounded.setWidth(roundf(rect.width()));
    rounded.setHeight(roundf(rect.height()));
    return rounded;
}

/* Used by canvas.clearRect. Must clear the given rectangle with transparent black. */
void GraphicsContextHaiku::clearRect(const FloatRect& rect)
{
    m_view->PushState();
    m_view->SetHighColor(0, 0, 0, 0);
    m_view->SetDrawingMode(B_OP_COPY);
    m_view->FillRect(rect);
    m_view->PopState();
}

void GraphicsContextHaiku::setLineCap(LineCap lineCap)
{
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

    m_view->SetLineMode(mode, m_view->LineJoinMode(), m_view->LineMiterLimit());
}

void GraphicsContextHaiku::setLineDash(const DashArray& /*dashes*/, float /*dashOffset*/)
{
    // TODO this is used to draw dashed strokes in SVG, but we need app_server support
    notImplemented();
}

void GraphicsContextHaiku::setLineJoin(LineJoin lineJoin)
{
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

    m_view->SetLineMode(m_view->LineCapMode(), mode, m_view->LineMiterLimit());
}

void GraphicsContextHaiku::setMiterLimit(float limit)
{
    m_view->SetLineMode(m_view->LineCapMode(), m_view->LineJoinMode(), limit);
}

AffineTransform GraphicsContextHaiku::getCTM(IncludeDeviceScale) const
{
    BAffineTransform t = m_view->Transform();
    	// TODO: we actually need to use the combined transform here?
    AffineTransform matrix(t.sx, t.shy, t.shx, t.sy, t.tx, t.ty);
    return matrix;
}

void GraphicsContextHaiku::translate(float x, float y)
{
    if (x == 0.f && y == 0.f)
        return;

    m_view->TranslateBy(x, y);
}

void GraphicsContextHaiku::rotate(float radians)
{
    if (radians == 0.f)
        return;

    m_view->RotateBy(radians);
}

void GraphicsContextHaiku::scale(const FloatSize& size)
{
    m_view->ScaleBy(size.width(), size.height());
}

void GraphicsContextHaiku::concatCTM(const AffineTransform& transform)
{
    BAffineTransform current = m_view->Transform();
    current.PreMultiply(transform);
    m_view->SetTransform(current);
}

void GraphicsContextHaiku::setCTM(const AffineTransform& transform)
{
    m_view->SetTransform(transform);
}

void GraphicsContextHaiku::updateState(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
#if 0
        StrokeGradientChange                    = 1 << 0,
        StrokePatternChange                     = 1 << 1,
        FillGradientChange // 
        FillPatternChange                       = 1 << 3,
#endif
    if (flags & GraphicsContextState::StrokeThicknessChange)
        m_view->SetPenSize(state.strokeThickness);
    if (flags & GraphicsContextState::StrokeColorChange)
        m_view->SetHighColor(state.strokeColor);
    if (flags & GraphicsContextState::StrokeStyleChange) {
        switch (strokeStyle()) {
            case DoubleStroke:
            case WavyStroke:
                notImplemented();
                m_strokeStyle = B_SOLID_HIGH;
                break;
            case SolidStroke:
                m_strokeStyle = B_SOLID_HIGH;
                break;
            case DottedStroke:
                m_strokeStyle = B_MIXED_COLORS;
                break;
            case DashedStroke:
                // FIXME: use a better dashed stroke!
                notImplemented();
                m_strokeStyle = B_MIXED_COLORS;
                break;
            case NoStroke:
                m_strokeStyle = B_SOLID_LOW;
                break;
        }
    }
    if (flags & GraphicsContextState::FillColorChange)
        m_view->SetLowColor(state.fillColor);
    if (flags & GraphicsContextState::FillRuleChange)
        m_view->SetFillRule(fillRule() == WindRule::NonZero ? B_NONZERO : B_EVEN_ODD);
#if 0
        ShadowChange                            = 1 << 9,
        ShadowsIgnoreTransformsChange           = 1 << 10,
        AlphaChange                             = 1 << 11,
#endif
    if (flags & GraphicsContextState::CompositeOperationChange) {
        drawing_mode mode = B_OP_ALPHA;
        alpha_function blending_mode = B_ALPHA_COMPOSITE;
        switch (compositeOperation()) {
            case CompositeOperator::SourceOver:
                blending_mode = B_ALPHA_COMPOSITE_SOURCE_OVER;
                break;
            case CompositeOperator::PlusLighter:
                blending_mode = B_ALPHA_COMPOSITE_LIGHTEN;
                break;
            case CompositeOperator::Difference:
                blending_mode = B_ALPHA_COMPOSITE_DIFFERENCE;
                break;
            case CompositeOperator::PlusDarker:
                blending_mode = B_ALPHA_COMPOSITE_DARKEN;
                break;
            case CompositeOperator::Clear:
                blending_mode = B_ALPHA_COMPOSITE_CLEAR;
                break;
            case CompositeOperator::DestinationOut:
                blending_mode = B_ALPHA_COMPOSITE_DESTINATION_OUT;
                break;
            case CompositeOperator::SourceAtop:
                blending_mode = B_ALPHA_COMPOSITE_SOURCE_ATOP;
                break;
            case CompositeOperator::SourceIn:
                blending_mode = B_ALPHA_COMPOSITE_SOURCE_IN;
                break;
            case CompositeOperator::SourceOut:
                blending_mode = B_ALPHA_COMPOSITE_SOURCE_OUT;
                break;
            case CompositeOperator::DestinationOver:
                blending_mode = B_ALPHA_COMPOSITE_DESTINATION_OVER;
                break;
            case CompositeOperator::DestinationAtop:
                blending_mode = B_ALPHA_COMPOSITE_DESTINATION_ATOP;
                break;
            case CompositeOperator::DestinationIn:
                blending_mode = B_ALPHA_COMPOSITE_DESTINATION_IN;
                break;
            case CompositeOperator::XOR:
                blending_mode = B_ALPHA_COMPOSITE_XOR;
                break;
            case CompositeOperator::Copy:
                mode = B_OP_COPY;
                break;
            default:
                fprintf(stderr, "GraphicsContext::setCompositeOperation: Unsupported composite operation %s\n",
                        compositeOperatorName(compositeOperation(), blendModeOperation()).utf8().data());
        }
        m_view->SetDrawingMode(mode);

        if (mode == B_OP_ALPHA)
            m_view->SetBlendingMode(B_PIXEL_ALPHA, blending_mode);
    }
#if 0
        BlendModeChange                         = 1 << 13,
        TextDrawingModeChange                   = 1 << 14,
        ShouldAntialiasChange                   = 1 << 15,
        ShouldSmoothFontsChange                 = 1 << 16,
        ShouldSubpixelQuantizeFontsChange       = 1 << 17,
        DrawLuminanceMaskChange                 = 1 << 18,
        ImageInterpolationQualityChange         = 1 << 19,
        UseDarkAppearanceChange                 = 1 << 20,
#endif
}

#if ENABLE(3D_RENDERING) && USE(TEXTURE_MAPPER)
TransformationMatrix GraphicsContextHaiku::get3DTransform() const
{
    // FIXME: Can we approximate the transformation better than this?
    return getCTM().toTransformationMatrix();
}

void GraphicsContextHaiku::concat3DTransform(const TransformationMatrix& transform)
{
    concatCTM(transform.toAffineTransform());
}

void GraphicsContextHaiku::set3DTransform(const TransformationMatrix& transform)
{
    setCTM(transform.toAffineTransform());
}
#endif

} // namespace WebCore

