/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
#include "CanvasRenderingContext2D.h"

#include "AffineTransform.h"
#include "CSSParser.h"
#include "CachedImage.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasStyle.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "ExceptionCode.h"
#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "KURL.h"
#include "Page.h"
#include "RenderHTMLCanvas.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StrokeStyleApplier.h"
#include "TextMetrics.h"
#include "HTMLVideoElement.h"
#include <stdio.h>
#include <wtf/ByteArray.h>
#include <wtf/MathExtras.h>
#include <wtf/OwnPtr.h>
#include <wtf/UnusedParam.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

static const char* const defaultFont = "10px sans-serif";


class CanvasStrokeStyleApplier : public StrokeStyleApplier {
public:
    CanvasStrokeStyleApplier(CanvasRenderingContext2D* canvasContext)
        : m_canvasContext(canvasContext)
    {
    }
    
    virtual void strokeStyle(GraphicsContext* c)
    {
        c->setStrokeThickness(m_canvasContext->lineWidth());
        c->setLineCap(m_canvasContext->getLineCap());
        c->setLineJoin(m_canvasContext->getLineJoin());
        c->setMiterLimit(m_canvasContext->miterLimit());
    }

private:
    CanvasRenderingContext2D* m_canvasContext;
};

CanvasRenderingContext2D::CanvasRenderingContext2D(HTMLCanvasElement* canvas, bool usesCSSCompatibilityParseMode, bool usesDashboardCompatibilityMode)
    : CanvasRenderingContext(canvas)
    , m_stateStack(1)
    , m_usesCSSCompatibilityParseMode(usesCSSCompatibilityParseMode)
#if ENABLE(DASHBOARD_SUPPORT)
    , m_usesDashboardCompatibilityMode(usesDashboardCompatibilityMode)
#endif
{
#if !ENABLE(DASHBOARD_SUPPORT)
   ASSERT_UNUSED(usesDashboardCompatibilityMode, !usesDashboardCompatibilityMode);
#endif

    // Make sure that even if the drawingContext() has a different default
    // thickness, it is in sync with the canvas thickness.
    setLineWidth(lineWidth());
}

CanvasRenderingContext2D::~CanvasRenderingContext2D()
{
}

void CanvasRenderingContext2D::reset()
{
    m_stateStack.resize(1);
    m_stateStack.first() = State();
}

CanvasRenderingContext2D::State::State()
    : m_strokeStyle(CanvasStyle::create("black"))
    , m_fillStyle(CanvasStyle::create("black"))
    , m_lineWidth(1)
    , m_lineCap(ButtCap)
    , m_lineJoin(MiterJoin)
    , m_miterLimit(10)
    , m_shadowBlur(0)
    , m_shadowColor("black")
    , m_globalAlpha(1)
    , m_globalComposite(CompositeSourceOver)
    , m_invertibleCTM(true)
    , m_textAlign(StartTextAlign)
    , m_textBaseline(AlphabeticTextBaseline)
    , m_unparsedFont(defaultFont)
    , m_realizedFont(false)
{
}

void CanvasRenderingContext2D::save()
{
    ASSERT(m_stateStack.size() >= 1);
    m_stateStack.append(state());
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->save();
}

void CanvasRenderingContext2D::restore()
{
    ASSERT(m_stateStack.size() >= 1);
    if (m_stateStack.size() <= 1)
        return;
    m_path.transform(state().m_transform);
    m_stateStack.removeLast();
    m_path.transform(state().m_transform.inverse());
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->restore();
}

CanvasStyle* CanvasRenderingContext2D::strokeStyle() const
{
    return state().m_strokeStyle.get();
}

void CanvasRenderingContext2D::setStrokeStyle(PassRefPtr<CanvasStyle> style)
{
    if (!style)
        return;

    if (canvas()->originClean()) {
        if (CanvasPattern* pattern = style->canvasPattern()) {
            if (!pattern->originClean())
                canvas()->setOriginTainted();
        }
    }

    state().m_strokeStyle = style;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state().m_strokeStyle->applyStrokeColor(c);
}

CanvasStyle* CanvasRenderingContext2D::fillStyle() const
{
    return state().m_fillStyle.get();
}

void CanvasRenderingContext2D::setFillStyle(PassRefPtr<CanvasStyle> style)
{
    if (!style)
        return;
 
    if (canvas()->originClean()) {
        if (CanvasPattern* pattern = style->canvasPattern()) {
            if (!pattern->originClean())
                canvas()->setOriginTainted();
        }
    }

    state().m_fillStyle = style;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state().m_fillStyle->applyFillColor(c);
}

float CanvasRenderingContext2D::lineWidth() const
{
    return state().m_lineWidth;
}

void CanvasRenderingContext2D::setLineWidth(float width)
{
    if (!(width > 0))
        return;
    state().m_lineWidth = width;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setStrokeThickness(width);
}

String CanvasRenderingContext2D::lineCap() const
{
    return lineCapName(state().m_lineCap);
}

void CanvasRenderingContext2D::setLineCap(const String& s)
{
    LineCap cap;
    if (!parseLineCap(s, cap))
        return;
    state().m_lineCap = cap;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setLineCap(cap);
}

String CanvasRenderingContext2D::lineJoin() const
{
    return lineJoinName(state().m_lineJoin);
}

void CanvasRenderingContext2D::setLineJoin(const String& s)
{
    LineJoin join;
    if (!parseLineJoin(s, join))
        return;
    state().m_lineJoin = join;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setLineJoin(join);
}

float CanvasRenderingContext2D::miterLimit() const
{
    return state().m_miterLimit;
}

void CanvasRenderingContext2D::setMiterLimit(float limit)
{
    if (!(limit > 0))
        return;
    state().m_miterLimit = limit;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setMiterLimit(limit);
}

float CanvasRenderingContext2D::shadowOffsetX() const
{
    return state().m_shadowOffset.width();
}

void CanvasRenderingContext2D::setShadowOffsetX(float x)
{
    state().m_shadowOffset.setWidth(x);
    applyShadow();
}

float CanvasRenderingContext2D::shadowOffsetY() const
{
    return state().m_shadowOffset.height();
}

void CanvasRenderingContext2D::setShadowOffsetY(float y)
{
    state().m_shadowOffset.setHeight(y);
    applyShadow();
}

float CanvasRenderingContext2D::shadowBlur() const
{
    return state().m_shadowBlur;
}

void CanvasRenderingContext2D::setShadowBlur(float blur)
{
    state().m_shadowBlur = blur;
    applyShadow();
}

String CanvasRenderingContext2D::shadowColor() const
{
    // FIXME: What should this return if you called setShadow with a non-string color?
    return state().m_shadowColor;
}

void CanvasRenderingContext2D::setShadowColor(const String& color)
{
    state().m_shadowColor = color;
    applyShadow();
}

float CanvasRenderingContext2D::globalAlpha() const
{
    return state().m_globalAlpha;
}

void CanvasRenderingContext2D::setGlobalAlpha(float alpha)
{
    if (!(alpha >= 0 && alpha <= 1))
        return;
    state().m_globalAlpha = alpha;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setAlpha(alpha);
}

String CanvasRenderingContext2D::globalCompositeOperation() const
{
    return compositeOperatorName(state().m_globalComposite);
}

void CanvasRenderingContext2D::setGlobalCompositeOperation(const String& operation)
{
    CompositeOperator op;
    if (!parseCompositeOperator(operation, op))
        return;
    state().m_globalComposite = op;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setCompositeOperation(op);
}

void CanvasRenderingContext2D::scale(float sx, float sy)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!isfinite(sx) | !isfinite(sy))
        return;

    AffineTransform newTransform = state().m_transform;
    newTransform.scaleNonUniform(sx, sy);
    if (!newTransform.isInvertible()) {
        state().m_invertibleCTM = false;
        return;
    }

    state().m_transform = newTransform;
    c->scale(FloatSize(sx, sy));
    m_path.transform(AffineTransform().scaleNonUniform(1.0 / sx, 1.0 / sy));
}

void CanvasRenderingContext2D::rotate(float angleInRadians)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!isfinite(angleInRadians))
        return;

    AffineTransform newTransform = state().m_transform;
    newTransform.rotate(angleInRadians / piDouble * 180.0);
    if (!newTransform.isInvertible()) {
        state().m_invertibleCTM = false;
        return;
    }

    state().m_transform = newTransform;
    c->rotate(angleInRadians);
    m_path.transform(AffineTransform().rotate(-angleInRadians / piDouble * 180.0));
}

void CanvasRenderingContext2D::translate(float tx, float ty)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!isfinite(tx) | !isfinite(ty))
        return;

    AffineTransform newTransform = state().m_transform;
    newTransform.translate(tx, ty);
    if (!newTransform.isInvertible()) {
        state().m_invertibleCTM = false;
        return;
    }

    state().m_transform = newTransform;
    c->translate(tx, ty);
    m_path.transform(AffineTransform().translate(-tx, -ty));
}

void CanvasRenderingContext2D::transform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!isfinite(m11) | !isfinite(m21) | !isfinite(dx) | 
        !isfinite(m12) | !isfinite(m22) | !isfinite(dy))
        return;

    AffineTransform transform(m11, m12, m21, m22, dx, dy);
    AffineTransform newTransform = transform * state().m_transform;
    if (!newTransform.isInvertible()) {
        state().m_invertibleCTM = false;
        return;
    }

    state().m_transform = newTransform;
    c->concatCTM(transform);
    m_path.transform(transform.inverse());
}

void CanvasRenderingContext2D::setTransform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    
    if (!isfinite(m11) | !isfinite(m21) | !isfinite(dx) | 
        !isfinite(m12) | !isfinite(m22) | !isfinite(dy))
        return;

    AffineTransform ctm = state().m_transform;
    if (!ctm.isInvertible())
        return;
    c->concatCTM(c->getCTM().inverse());
    c->concatCTM(canvas()->baseTransform());
    state().m_transform.multiply(ctm.inverse());
    m_path.transform(ctm);

    state().m_invertibleCTM = true;
    transform(m11, m12, m21, m22, dx, dy);
}

void CanvasRenderingContext2D::setStrokeColor(const String& color)
{
    setStrokeStyle(CanvasStyle::create(color));
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel)
{
    setStrokeStyle(CanvasStyle::create(grayLevel, 1));
}

void CanvasRenderingContext2D::setStrokeColor(const String& color, float alpha)
{
    setStrokeStyle(CanvasStyle::create(color, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel, float alpha)
{
    setStrokeStyle(CanvasStyle::create(grayLevel, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float r, float g, float b, float a)
{
    setStrokeStyle(CanvasStyle::create(r, g, b, a));
}

void CanvasRenderingContext2D::setStrokeColor(float c, float m, float y, float k, float a)
{
    setStrokeStyle(CanvasStyle::create(c, m, y, k, a));
}

void CanvasRenderingContext2D::setFillColor(const String& color)
{
    setFillStyle(CanvasStyle::create(color));
}

void CanvasRenderingContext2D::setFillColor(float grayLevel)
{
    setFillStyle(CanvasStyle::create(grayLevel, 1));
}

void CanvasRenderingContext2D::setFillColor(const String& color, float alpha)
{
    setFillStyle(CanvasStyle::create(color, alpha));
}

void CanvasRenderingContext2D::setFillColor(float grayLevel, float alpha)
{
    setFillStyle(CanvasStyle::create(grayLevel, alpha));
}

void CanvasRenderingContext2D::setFillColor(float r, float g, float b, float a)
{
    setFillStyle(CanvasStyle::create(r, g, b, a));
}

void CanvasRenderingContext2D::setFillColor(float c, float m, float y, float k, float a)
{
    setFillStyle(CanvasStyle::create(c, m, y, k, a));
}

void CanvasRenderingContext2D::beginPath()
{
    m_path.clear();
}

void CanvasRenderingContext2D::closePath()
{
    m_path.closeSubpath();
}

void CanvasRenderingContext2D::moveTo(float x, float y)
{
    if (!isfinite(x) | !isfinite(y))
        return;
    if (!state().m_invertibleCTM)
        return;
    m_path.moveTo(FloatPoint(x, y));
}

void CanvasRenderingContext2D::lineTo(float x, float y)
{
    if (!isfinite(x) | !isfinite(y))
        return;
    if (!state().m_invertibleCTM)
        return;
    if (!m_path.hasCurrentPoint())
        m_path.moveTo(FloatPoint(x, y));
    else
        m_path.addLineTo(FloatPoint(x, y));
}

void CanvasRenderingContext2D::quadraticCurveTo(float cpx, float cpy, float x, float y)
{
    if (!isfinite(cpx) | !isfinite(cpy) | !isfinite(x) | !isfinite(y))
        return;
    if (!state().m_invertibleCTM)
        return;
    if (!m_path.hasCurrentPoint())
        m_path.moveTo(FloatPoint(x, y));
    else
        m_path.addQuadCurveTo(FloatPoint(cpx, cpy), FloatPoint(x, y));
}

void CanvasRenderingContext2D::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
    if (!isfinite(cp1x) | !isfinite(cp1y) | !isfinite(cp2x) | !isfinite(cp2y) | !isfinite(x) | !isfinite(y))
        return;
    if (!state().m_invertibleCTM)
        return;
    if (!m_path.hasCurrentPoint())
        m_path.moveTo(FloatPoint(x, y));
    else
        m_path.addBezierCurveTo(FloatPoint(cp1x, cp1y), FloatPoint(cp2x, cp2y), FloatPoint(x, y));
}

void CanvasRenderingContext2D::arcTo(float x0, float y0, float x1, float y1, float r, ExceptionCode& ec)
{
    ec = 0;
    if (!isfinite(x0) | !isfinite(y0) | !isfinite(x1) | !isfinite(y1) | !isfinite(r))
        return;
    
    if (r < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    if (!state().m_invertibleCTM)
        return;
    m_path.addArcTo(FloatPoint(x0, y0), FloatPoint(x1, y1), r);
}

void CanvasRenderingContext2D::arc(float x, float y, float r, float sa, float ea, bool anticlockwise, ExceptionCode& ec)
{
    ec = 0;
    if (!isfinite(x) | !isfinite(y) | !isfinite(r) | !isfinite(sa) | !isfinite(ea))
        return;
    
    if (r < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    if (!state().m_invertibleCTM)
        return;
    m_path.addArc(FloatPoint(x, y), r, sa, ea, anticlockwise);
}
    
static bool validateRectForCanvas(float& x, float& y, float& width, float& height)
{
    if (!isfinite(x) | !isfinite(y) | !isfinite(width) | !isfinite(height))
        return false;

    if (!width && !height)
        return false;

    if (width < 0) {
        width = -width;
        x -= width;
    }
    
    if (height < 0) {
        height = -height;
        y -= height;
    }
    
    return true;
}

void CanvasRenderingContext2D::rect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;
    if (!state().m_invertibleCTM)
        return;
    m_path.addRect(FloatRect(x, y, width, height));
}

#if ENABLE(DASHBOARD_SUPPORT)
void CanvasRenderingContext2D::clearPathForDashboardBackwardCompatibilityMode()
{
    if (m_usesDashboardCompatibilityMode)
        m_path.clear();
}
#endif

void CanvasRenderingContext2D::fill()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!m_path.isEmpty()) {
        c->beginPath();
        c->addPath(m_path);
        willDraw(m_path.boundingRect());
        c->fillPath();
    }

#if ENABLE(DASHBOARD_SUPPORT)
    clearPathForDashboardBackwardCompatibilityMode();
#endif
}

void CanvasRenderingContext2D::stroke()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!m_path.isEmpty()) {
        c->beginPath();
        c->addPath(m_path);

        CanvasStrokeStyleApplier strokeApplier(this);
        FloatRect boundingRect = m_path.strokeBoundingRect(&strokeApplier);
        willDraw(boundingRect);

        c->strokePath();
    }

#if ENABLE(DASHBOARD_SUPPORT)
    clearPathForDashboardBackwardCompatibilityMode();
#endif
}

void CanvasRenderingContext2D::clip()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
    c->canvasClip(m_path);
#if ENABLE(DASHBOARD_SUPPORT)
    clearPathForDashboardBackwardCompatibilityMode();
#endif
}

bool CanvasRenderingContext2D::isPointInPath(const float x, const float y)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return false;
    if (!state().m_invertibleCTM)
        return false;

    FloatPoint point(x, y);
    AffineTransform ctm = state().m_transform;
    FloatPoint transformedPoint = ctm.inverse().mapPoint(point);
    return m_path.contains(transformedPoint);
}

void CanvasRenderingContext2D::clearRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
    FloatRect rect(x, y, width, height);
    willDraw(rect);
    c->clearRect(rect);
}

void CanvasRenderingContext2D::fillRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    FloatRect rect(x, y, width, height);
    willDraw(rect);

    c->save();
    c->fillRect(rect);
    c->restore();
}

void CanvasRenderingContext2D::strokeRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;
    strokeRect(x, y, width, height, state().m_lineWidth);
}

void CanvasRenderingContext2D::strokeRect(float x, float y, float width, float height, float lineWidth)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;
    
    if (!(lineWidth >= 0))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    FloatRect rect(x, y, width, height);

    FloatRect boundingRect = rect;
    boundingRect.inflate(lineWidth / 2);
    willDraw(boundingRect);

    c->strokeRect(rect, lineWidth);
}

#if PLATFORM(CG)
static inline CGSize adjustedShadowSize(CGFloat width, CGFloat height)
{
    // Work around <rdar://problem/5539388> by ensuring that shadow offsets will get truncated
    // to the desired integer.
    static const CGFloat extraShadowOffset = narrowPrecisionToCGFloat(1.0 / 128);
    if (width > 0)
        width += extraShadowOffset;
    else if (width < 0)
        width -= extraShadowOffset;

    if (height > 0)
        height += extraShadowOffset;
    else if (height < 0)
        height -= extraShadowOffset;

    return CGSizeMake(width, height);
}
#endif

void CanvasRenderingContext2D::setShadow(float width, float height, float blur)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";
    applyShadow();
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = color;
    applyShadow();
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    RGBA32 rgba = makeRGBA32FromFloats(grayLevel, grayLevel, grayLevel, 1.0f);
    c->setShadow(IntSize(width, -height), state().m_shadowBlur, Color(rgba), DeviceColorSpace);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color, float alpha)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = color;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    RGBA32 rgba = 0; // default is transparent black
    if (!state().m_shadowColor.isEmpty())
        CSSParser::parseColor(rgba, state().m_shadowColor);
    c->setShadow(IntSize(width, -height), state().m_shadowBlur, Color(colorWithOverrideAlpha(rgba, alpha)), DeviceColorSpace);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel, float alpha)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    RGBA32 rgba = makeRGBA32FromFloats(grayLevel, grayLevel, grayLevel, alpha);
    c->setShadow(IntSize(width, -height), state().m_shadowBlur, Color(rgba), DeviceColorSpace);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float r, float g, float b, float a)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    RGBA32 rgba = makeRGBA32FromFloats(r, g, b, a); // default is transparent black
    if (!state().m_shadowColor.isEmpty())
        CSSParser::parseColor(rgba, state().m_shadowColor);
    c->setShadow(IntSize(width, -height), state().m_shadowBlur, Color(rgba), DeviceColorSpace);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float c, float m, float y, float k, float a)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    GraphicsContext* dc = drawingContext();
    if (!dc)
        return;
#if PLATFORM(CG)
    const CGFloat components[5] = { c, m, y, k, a };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceCMYK();
    CGColorRef shadowColor = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(dc->platformContext(), adjustedShadowSize(width, -height), blur, shadowColor);
    CGColorRelease(shadowColor);
#else
    dc->setShadow(IntSize(width, -height), blur, Color(c, m, y, k, a), DeviceColorSpace);
#endif
}

void CanvasRenderingContext2D::clearShadow()
{
    state().m_shadowOffset = FloatSize();
    state().m_shadowBlur = 0;
    state().m_shadowColor = "";
    applyShadow();
}

void CanvasRenderingContext2D::applyShadow()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    RGBA32 rgba = 0; // default is transparent black
    if (!state().m_shadowColor.isEmpty())
        CSSParser::parseColor(rgba, state().m_shadowColor);
    float width = state().m_shadowOffset.width();
    float height = state().m_shadowOffset.height();
    c->setShadow(IntSize(width, -height), state().m_shadowBlur, Color(rgba), DeviceColorSpace);
}

static IntSize size(HTMLImageElement* image)
{
    if (CachedImage* cachedImage = image->cachedImage())
        return cachedImage->imageSize(1.0f); // FIXME: Not sure about this.
    return IntSize();
}

#if ENABLE(VIDEO)
static IntSize size(HTMLVideoElement* video)
{
    if (MediaPlayer* player = video->player())
        return player->naturalSize();
    return IntSize();
}
#endif

static inline FloatRect normalizeRect(const FloatRect& rect)
{
    return FloatRect(min(rect.x(), rect.right()),
        min(rect.y(), rect.bottom()),
        max(rect.width(), -rect.width()),
        max(rect.height(), -rect.height()));
}

void CanvasRenderingContext2D::checkOrigin(const KURL& url)
{
    if (canvas()->securityOrigin().taintsCanvas(url))
        canvas()->setOriginTainted();
}

void CanvasRenderingContext2D::checkOrigin(const String& url)
{
    checkOrigin(KURL(KURL(), url));
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image, float x, float y, ExceptionCode& ec)
{
    if (!image) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    IntSize s = size(image);
    drawImage(image, x, y, s.width(), s.height(), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image,
    float x, float y, float width, float height, ExceptionCode& ec)
{
    if (!image) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    IntSize s = size(image);
    drawImage(image, FloatRect(0, 0, s.width(), s.height()), FloatRect(x, y, width, height), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionCode& ec)
{
    if (!image) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    drawImage(image, FloatRect(sx, sy, sw, sh), FloatRect(dx, dy, dw, dh), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image, const FloatRect& srcRect, const FloatRect& dstRect,
    ExceptionCode& ec)
{
    if (!image) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    ec = 0;

    FloatRect imageRect = FloatRect(FloatPoint(), size(image));
    if (!imageRect.contains(normalizeRect(srcRect)) || srcRect.width() == 0 || srcRect.height() == 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!dstRect.width() || !dstRect.height())
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    CachedImage* cachedImage = image->cachedImage();
    if (!cachedImage)
        return;

    if (canvas()->originClean())
        checkOrigin(cachedImage->response().url());

    if (canvas()->originClean() && !cachedImage->image()->hasSingleSecurityOrigin())
        canvas()->setOriginTainted();

    FloatRect sourceRect = c->roundToDevicePixels(srcRect);
    FloatRect destRect = c->roundToDevicePixels(dstRect);
    willDraw(destRect);
    c->drawImage(cachedImage->image(), DeviceColorSpace, destRect, sourceRect, state().m_globalComposite);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* canvas, float x, float y, ExceptionCode& ec)
{
    if (!canvas) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    drawImage(canvas, x, y, canvas->width(), canvas->height(), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* canvas,
    float x, float y, float width, float height, ExceptionCode& ec)
{
    if (!canvas) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    drawImage(canvas, FloatRect(0, 0, canvas->width(), canvas->height()), FloatRect(x, y, width, height), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* canvas,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionCode& ec)
{
    drawImage(canvas, FloatRect(sx, sy, sw, sh), FloatRect(dx, dy, dw, dh), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* sourceCanvas, const FloatRect& srcRect,
    const FloatRect& dstRect, ExceptionCode& ec)
{
    if (!sourceCanvas) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    ec = 0;

    FloatRect srcCanvasRect = FloatRect(FloatPoint(), sourceCanvas->size());
    if (!srcCanvasRect.contains(normalizeRect(srcRect)) || srcRect.width() == 0 || srcRect.height() == 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!dstRect.width() || !dstRect.height())
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
        
    FloatRect sourceRect = c->roundToDevicePixels(srcRect);
    FloatRect destRect = c->roundToDevicePixels(dstRect);
        
    // FIXME: Do this through platform-independent GraphicsContext API.
    ImageBuffer* buffer = sourceCanvas->buffer();
    if (!buffer)
        return;

    if (!sourceCanvas->originClean())
        canvas()->setOriginTainted();

    c->drawImage(buffer->image(), DeviceColorSpace, destRect, sourceRect, state().m_globalComposite);
    willDraw(destRect); // This call comes after drawImage, since the buffer we draw into may be our own, and we need to make sure it is dirty.
                        // FIXME: Arguably willDraw should become didDraw and occur after drawing calls and not before them to avoid problems like this.
}

#if ENABLE(VIDEO)
void CanvasRenderingContext2D::drawImage(HTMLVideoElement* video, float x, float y, ExceptionCode& ec)
{
    if (!video) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    IntSize s = size(video);
    drawImage(video, x, y, s.width(), s.height(), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLVideoElement* video,
                                         float x, float y, float width, float height, ExceptionCode& ec)
{
    if (!video) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    IntSize s = size(video);
    drawImage(video, FloatRect(0, 0, s.width(), s.height()), FloatRect(x, y, width, height), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLVideoElement* video,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionCode& ec)
{
    drawImage(video, FloatRect(sx, sy, sw, sh), FloatRect(dx, dy, dw, dh), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLVideoElement* video, const FloatRect& srcRect, const FloatRect& dstRect,
                                         ExceptionCode& ec)
{
    if (!video) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    
    ec = 0;
    FloatRect videoRect = FloatRect(FloatPoint(), size(video));
    if (!videoRect.contains(normalizeRect(srcRect)) || srcRect.width() == 0 || srcRect.height() == 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    if (!dstRect.width() || !dstRect.height())
        return;
    
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (canvas()->originClean())
        checkOrigin(video->currentSrc());

    if (canvas()->originClean() && !video->hasSingleSecurityOrigin())
        canvas()->setOriginTainted();

    FloatRect sourceRect = c->roundToDevicePixels(srcRect);
    FloatRect destRect = c->roundToDevicePixels(dstRect);
    willDraw(destRect);

    c->save();
    c->clip(destRect);
    c->translate(destRect.x(), destRect.y());
    c->scale(FloatSize(destRect.width()/sourceRect.width(), destRect.height()/sourceRect.height()));
    c->translate(-sourceRect.x(), -sourceRect.y());
    video->paintCurrentFrameInContext(c, IntRect(IntPoint(), size(video)));
    c->restore();
}
#endif

// FIXME: Why isn't this just another overload of drawImage? Why have a different name?
void CanvasRenderingContext2D::drawImageFromRect(HTMLImageElement* image,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh,
    const String& compositeOperation)
{
    if (!image)
        return;

    CachedImage* cachedImage = image->cachedImage();
    if (!cachedImage)
        return;

    if (canvas()->originClean())
        checkOrigin(cachedImage->response().url());

    if (canvas()->originClean() && !cachedImage->image()->hasSingleSecurityOrigin())
        canvas()->setOriginTainted();

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    CompositeOperator op;
    if (!parseCompositeOperator(compositeOperation, op))
        op = CompositeSourceOver;

    FloatRect destRect = FloatRect(dx, dy, dw, dh);
    willDraw(destRect);
    c->drawImage(cachedImage->image(), DeviceColorSpace, destRect, FloatRect(sx, sy, sw, sh), op);
}

void CanvasRenderingContext2D::setAlpha(float alpha)
{
    setGlobalAlpha(alpha);
}

void CanvasRenderingContext2D::setCompositeOperation(const String& operation)
{
    setGlobalCompositeOperation(operation);
}

void CanvasRenderingContext2D::prepareGradientForDashboard(CanvasGradient* gradient) const
{
#if ENABLE(DASHBOARD_SUPPORT)
    if (m_usesDashboardCompatibilityMode)
        gradient->setDashboardCompatibilityMode();
#else
    UNUSED_PARAM(gradient);
#endif
}

PassRefPtr<CanvasGradient> CanvasRenderingContext2D::createLinearGradient(float x0, float y0, float x1, float y1, ExceptionCode& ec)
{
    if (!isfinite(x0) || !isfinite(y0) || !isfinite(x1) || !isfinite(y1)) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }

    PassRefPtr<CanvasGradient> gradient = CanvasGradient::create(FloatPoint(x0, y0), FloatPoint(x1, y1));
    prepareGradientForDashboard(gradient.get());
    return gradient;
}

PassRefPtr<CanvasGradient> CanvasRenderingContext2D::createRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1, ExceptionCode& ec)
{
    if (!isfinite(x0) || !isfinite(y0) || !isfinite(r0) || 
        !isfinite(x1) || !isfinite(y1) || !isfinite(r1)) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    PassRefPtr<CanvasGradient> gradient =  CanvasGradient::create(FloatPoint(x0, y0), r0, FloatPoint(x1, y1), r1);
    prepareGradientForDashboard(gradient.get());
    return gradient;
}

PassRefPtr<CanvasPattern> CanvasRenderingContext2D::createPattern(HTMLImageElement* image,
    const String& repetitionType, ExceptionCode& ec)
{
    if (!image) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    bool repeatX, repeatY;
    ec = 0;
    CanvasPattern::parseRepetitionType(repetitionType, repeatX, repeatY, ec);
    if (ec)
        return 0;

    if (!image->complete()) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    CachedImage* cachedImage = image->cachedImage();
    if (!cachedImage || !image->cachedImage()->image())
        return CanvasPattern::create(Image::nullImage(), repeatX, repeatY, true);

    bool originClean = !canvas()->securityOrigin().taintsCanvas(KURL(KURL(), cachedImage->response().url())) && cachedImage->image()->hasSingleSecurityOrigin();
    return CanvasPattern::create(cachedImage->image(), repeatX, repeatY, originClean);
}

PassRefPtr<CanvasPattern> CanvasRenderingContext2D::createPattern(HTMLCanvasElement* canvas,
    const String& repetitionType, ExceptionCode& ec)
{
    if (!canvas) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    if (!canvas->width() || !canvas->height()) {
        ec = INVALID_STATE_ERR;
        return 0;
    }
    
    bool repeatX, repeatY;
    ec = 0;
    CanvasPattern::parseRepetitionType(repetitionType, repeatX, repeatY, ec);
    if (ec)
        return 0;
    return CanvasPattern::create(canvas->buffer()->image(), repeatX, repeatY, canvas->originClean());
}

void CanvasRenderingContext2D::willDraw(const FloatRect& r, unsigned options)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    FloatRect dirtyRect = r;
    if (options & CanvasWillDrawApplyTransform) {
        AffineTransform ctm = state().m_transform;
        dirtyRect = ctm.mapRect(r);
    }
    
    if (options & CanvasWillDrawApplyShadow) {
        // The shadow gets applied after transformation
        FloatRect shadowRect(dirtyRect);
        shadowRect.move(state().m_shadowOffset);
        shadowRect.inflate(state().m_shadowBlur);
        dirtyRect.unite(shadowRect);
    }
    
    if (options & CanvasWillDrawApplyClip) {
        // FIXME: apply the current clip to the rectangle. Unfortunately we can't get the clip
        // back out of the GraphicsContext, so to take clip into account for incremental painting,
        // we'd have to keep the clip path around.
    }
    
    canvas()->willDraw(dirtyRect);
}

GraphicsContext* CanvasRenderingContext2D::drawingContext() const
{
    return canvas()->drawingContext();
}

static PassRefPtr<ImageData> createEmptyImageData(const IntSize& size)
{
    RefPtr<ImageData> data = ImageData::create(size.width(), size.height());
    memset(data->data()->data()->data(), 0, data->data()->data()->length());
    return data.get();
}

PassRefPtr<ImageData> CanvasRenderingContext2D::createImageData(float sw, float sh, ExceptionCode& ec) const
{
    ec = 0;
    if (!isfinite(sw) || !isfinite(sh)) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    FloatSize unscaledSize(sw, sh);
    IntSize scaledSize = canvas()->convertLogicalToDevice(unscaledSize);
    if (scaledSize.width() < 1)
        scaledSize.setWidth(1);
    if (scaledSize.height() < 1)
        scaledSize.setHeight(1);
    
    return createEmptyImageData(scaledSize);
}

PassRefPtr<ImageData> CanvasRenderingContext2D::getImageData(float sx, float sy, float sw, float sh, ExceptionCode& ec) const
{
    if (!canvas()->originClean()) {
        ec = SECURITY_ERR;
        return 0;
    }
    
    FloatRect unscaledRect(sx, sy, sw, sh);
    IntRect scaledRect = canvas()->convertLogicalToDevice(unscaledRect);
    if (scaledRect.width() < 1)
        scaledRect.setWidth(1);
    if (scaledRect.height() < 1)
        scaledRect.setHeight(1);
    ImageBuffer* buffer = canvas() ? canvas()->buffer() : 0;
    if (!buffer)
        return createEmptyImageData(scaledRect.size());
    return buffer->getUnmultipliedImageData(scaledRect);
}

void CanvasRenderingContext2D::putImageData(ImageData* data, float dx, float dy, ExceptionCode& ec)
{
    if (!data) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    putImageData(data, dx, dy, 0, 0, data->width(), data->height(), ec);
}

void CanvasRenderingContext2D::putImageData(ImageData* data, float dx, float dy, float dirtyX, float dirtyY, 
                                            float dirtyWidth, float dirtyHeight, ExceptionCode& ec)
{
    if (!data) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    if (!isfinite(dx) || !isfinite(dy) || !isfinite(dirtyX) || 
        !isfinite(dirtyY) || !isfinite(dirtyWidth) || !isfinite(dirtyHeight)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    ImageBuffer* buffer = canvas()->buffer();
    if (!buffer)
        return;

    if (dirtyWidth < 0) {
        dirtyX += dirtyWidth;
        dirtyWidth = -dirtyWidth;
    }

    if (dirtyHeight < 0) {
        dirtyY += dirtyHeight;
        dirtyHeight = -dirtyHeight;
    }

    FloatRect clipRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight);
    clipRect.intersect(IntRect(0, 0, data->width(), data->height()));
    IntSize destOffset(static_cast<int>(dx), static_cast<int>(dy));
    IntRect sourceRect = enclosingIntRect(clipRect);
    sourceRect.move(destOffset);
    sourceRect.intersect(IntRect(IntPoint(), buffer->size()));
    if (sourceRect.isEmpty())
        return;
    willDraw(sourceRect, 0);  // ignore transform, shadow and clip
    sourceRect.move(-destOffset);
    IntPoint destPoint(destOffset.width(), destOffset.height());
    
    buffer->putUnmultipliedImageData(data, sourceRect, destPoint);
}

String CanvasRenderingContext2D::font() const
{
    return state().m_unparsedFont;
}

void CanvasRenderingContext2D::setFont(const String& newFont)
{
    RefPtr<CSSMutableStyleDeclaration> tempDecl = CSSMutableStyleDeclaration::create();
    CSSParser parser(!m_usesCSSCompatibilityParseMode);
        
    String declarationText("font: ");
    declarationText += newFont;
    parser.parseDeclaration(tempDecl.get(), declarationText);
    if (!tempDecl->length())
        return;
            
    // The parse succeeded.
    state().m_unparsedFont = newFont;
    
    // Map the <canvas> font into the text style. If the font uses keywords like larger/smaller, these will work
    // relative to the canvas.
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    if (RenderStyle* computedStyle = canvas()->computedStyle())
        newStyle->setFontDescription(computedStyle->fontDescription());

    // Now map the font property into the style.
    CSSStyleSelector* styleSelector = canvas()->styleSelector();
    styleSelector->applyPropertyToStyle(CSSPropertyFont, tempDecl->getPropertyCSSValue(CSSPropertyFont).get(), newStyle.get());
    
    state().m_font = newStyle->font();
    state().m_font.update(styleSelector->fontSelector());
    state().m_realizedFont = true;
}

void CanvasRenderingContext2D::updateFont()
{
    if (!state().m_realizedFont)
        return;

    const Font& font = state().m_font;
    font.update(font.fontSelector());
}

String CanvasRenderingContext2D::textAlign() const
{
    return textAlignName(state().m_textAlign);
}

void CanvasRenderingContext2D::setTextAlign(const String& s)
{
    TextAlign align;
    if (!parseTextAlign(s, align))
        return;
    state().m_textAlign = align;
}
        
String CanvasRenderingContext2D::textBaseline() const
{
    return textBaselineName(state().m_textBaseline);
}

void CanvasRenderingContext2D::setTextBaseline(const String& s)
{
    TextBaseline baseline;
    if (!parseTextBaseline(s, baseline))
        return;
    state().m_textBaseline = baseline;
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y)
{
    drawTextInternal(text, x, y, true);
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y, float maxWidth)
{
    drawTextInternal(text, x, y, true, maxWidth, true);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y)
{
    drawTextInternal(text, x, y, false);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y, float maxWidth)
{
    drawTextInternal(text, x, y, false, maxWidth, true);
}

PassRefPtr<TextMetrics> CanvasRenderingContext2D::measureText(const String& text)
{
    RefPtr<TextMetrics> metrics = TextMetrics::create();
    metrics->setWidth(accessFont().width(TextRun(text.characters(), text.length())));
    return metrics;
}

void CanvasRenderingContext2D::drawTextInternal(const String& text, float x, float y, bool fill, float /*maxWidth*/, bool /*useMaxWidth*/)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
    
    const Font& font = accessFont();
    
    // FIXME: Handle maxWidth.
    // FIXME: Need to turn off font smoothing.

    RenderStyle* computedStyle = canvas()->computedStyle();
    bool rtl = computedStyle ? computedStyle->direction() == RTL : false;
    bool override = computedStyle ? computedStyle->unicodeBidi() == Override : false;

    unsigned length = text.length();
    const UChar* string = text.characters();
    TextRun textRun(string, length, 0, 0, 0, rtl, override, false, false);

    // Draw the item text at the correct point.
    FloatPoint location(x, y);
    switch (state().m_textBaseline) {
        case TopTextBaseline:
        case HangingTextBaseline:
            location.setY(y + font.ascent());
            break;
        case BottomTextBaseline:
        case IdeographicTextBaseline:
            location.setY(y - font.descent());
            break;
        case MiddleTextBaseline:
            location.setY(y - font.descent() + font.height() / 2);
            break;
        case AlphabeticTextBaseline:
        default:
             // Do nothing.
            break;
    }
    
    float width = font.width(TextRun(text, false, 0, 0, rtl, override));

    TextAlign align = state().m_textAlign;
    if (align == StartTextAlign)
         align = rtl ? RightTextAlign : LeftTextAlign;
    else if (align == EndTextAlign)
        align = rtl ? LeftTextAlign : RightTextAlign;
    
    switch (align) {
        case CenterTextAlign:
            location.setX(location.x() - width / 2);
            break;
        case RightTextAlign:
            location.setX(location.x() - width);
            break;
        default:
            break;
    }
    
    // The slop built in to this mask rect matches the heuristic used in FontCGWin.cpp for GDI text.
    FloatRect textRect = FloatRect(location.x() - font.height() / 2, location.y() - font.ascent() - font.lineGap(),
                                   width + font.height(), font.lineSpacing());
    if (!fill)
        textRect.inflate(c->strokeThickness() / 2);

    if (fill)
        canvas()->willDraw(textRect);
    else {
        // When stroking text, pointy miters can extend outside of textRect, so we
        // punt and dirty the whole canvas.
        canvas()->willDraw(FloatRect(0, 0, canvas()->width(), canvas()->height()));
    }
    
#if PLATFORM(CG)
    CanvasStyle* drawStyle = fill ? state().m_fillStyle.get() : state().m_strokeStyle.get();
    if (drawStyle->canvasGradient() || drawStyle->canvasPattern()) {
        // FIXME: The rect is not big enough for miters on stroked text.
        IntRect maskRect = enclosingIntRect(textRect);

        OwnPtr<ImageBuffer> maskImage = ImageBuffer::create(maskRect.size());

        GraphicsContext* maskImageContext = maskImage->context();

        if (fill)
            maskImageContext->setFillColor(Color::black, DeviceColorSpace);
        else {
            maskImageContext->setStrokeColor(Color::black, DeviceColorSpace);
            maskImageContext->setStrokeThickness(c->strokeThickness());
        }

        maskImageContext->setTextDrawingMode(fill ? cTextFill : cTextStroke);
        maskImageContext->translate(-maskRect.x(), -maskRect.y());
        
        maskImageContext->drawBidiText(font, textRun, location);
        
        c->save();
        c->clipToImageBuffer(maskRect, maskImage.get());
        drawStyle->applyFillColor(c);
        c->fillRect(maskRect);
        c->restore();

        return;
    }
#endif

    c->setTextDrawingMode(fill ? cTextFill : cTextStroke);
    c->drawBidiText(font, textRun, location);
}

const Font& CanvasRenderingContext2D::accessFont()
{
    canvas()->document()->updateStyleIfNeeded();

    if (!state().m_realizedFont)
        setFont(state().m_unparsedFont);
    return state().m_font;
}

} // namespace WebCore
