/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#include "CSSMutableStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "CachedImage.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasStyle.h"
#include "ExceptionCode.h"
#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "KURL.h"
#include "Page.h"
#include "RenderHTMLCanvas.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StrokeStyleApplier.h"
#include "TextMetrics.h"

#if ENABLE(ACCELERATED_2D_CANVAS)
#include "Chrome.h"
#include "ChromeClient.h"
#include "DrawingBuffer.h"
#include "FrameView.h"
#include "GraphicsContext3D.h"
#include "SharedGraphicsContext3D.h"
#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayer.h"
#endif
#endif

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
#if ENABLE(ACCELERATED_2D_CANVAS)
    , m_context3D(0)
#endif
{
#if !ENABLE(DASHBOARD_SUPPORT)
    ASSERT_UNUSED(usesDashboardCompatibilityMode, !usesDashboardCompatibilityMode);
#endif

    // Make sure that even if the drawingContext() has a different default
    // thickness, it is in sync with the canvas thickness.
    setLineWidth(lineWidth());

#if ENABLE(ACCELERATED_2D_CANVAS)
    Page* p = canvas->document()->page();
    if (!p)
        return;
    if (!p->settings()->accelerated2dCanvasEnabled())
        return;
    m_context3D = p->chrome()->client()->getSharedGraphicsContext3D();
    if (m_context3D) {
        if (GraphicsContext* c = drawingContext()) {
            m_drawingBuffer = DrawingBuffer::create(m_context3D.get(), IntSize(canvas->width(), canvas->height()));
            c->setSharedGraphicsContext3D(m_context3D.get(), m_drawingBuffer.get(), IntSize(canvas->width(), canvas->height()));
        }
    }
#endif
}

CanvasRenderingContext2D::~CanvasRenderingContext2D()
{
}

bool CanvasRenderingContext2D::isAccelerated() const
{
#if ENABLE(ACCELERATED_2D_CANVAS)
    return m_context3D;
#else
    return false;
#endif
}

bool CanvasRenderingContext2D::paintsIntoCanvasBuffer() const
{
#if ENABLE(ACCELERATED_2D_CANVAS)
    if (m_context3D)
        return m_context3D->paintsIntoCanvasBuffer();
#endif
    return true;
}


void CanvasRenderingContext2D::reset()
{
    m_stateStack.resize(1);
    m_stateStack.first() = State();
    m_path.clear();
#if ENABLE(ACCELERATED_2D_CANVAS)
    if (m_context3D) {
        if (GraphicsContext* c = drawingContext()) {
            m_drawingBuffer->reset(IntSize(canvas()->width(), canvas()->height()));
            c->setSharedGraphicsContext3D(m_context3D.get(), m_drawingBuffer.get(), IntSize(canvas()->width(), canvas()->height()));
        }
    }
#endif
}

CanvasRenderingContext2D::State::State()
    : m_strokeStyle(CanvasStyle::createFromRGBA(Color::black))
    , m_fillStyle(CanvasStyle::createFromRGBA(Color::black))
    , m_lineWidth(1)
    , m_lineCap(ButtCap)
    , m_lineJoin(MiterJoin)
    , m_miterLimit(10)
    , m_shadowBlur(0)
    , m_shadowColor(Color::transparent)
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

void CanvasRenderingContext2D::setAllAttributesToDefault()
{
    state().m_globalAlpha = 1;
    state().m_shadowOffset = FloatSize();
    state().m_shadowBlur = 0;
    state().m_shadowColor = Color::transparent;
    state().m_globalComposite = CompositeSourceOver;

    GraphicsContext* context = drawingContext();
    if (!context)
        return;

    context->setShadow(FloatSize(), 0, Color::transparent, DeviceColorSpace);
    context->setAlpha(1);
    context->setCompositeOperation(CompositeSourceOver);
}

CanvasStyle* CanvasRenderingContext2D::strokeStyle() const
{
    return state().m_strokeStyle.get();
}

void CanvasRenderingContext2D::setStrokeStyle(PassRefPtr<CanvasStyle> style)
{
    if (!style)
        return;

    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentColor(*style))
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
    state().m_unparsedStrokeColor = String();
}

CanvasStyle* CanvasRenderingContext2D::fillStyle() const
{
    return state().m_fillStyle.get();
}

void CanvasRenderingContext2D::setFillStyle(PassRefPtr<CanvasStyle> style)
{
    if (!style)
        return;

    if (state().m_fillStyle && state().m_fillStyle->isEquivalentColor(*style))
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
    state().m_unparsedFillColor = String();
}

float CanvasRenderingContext2D::lineWidth() const
{
    return state().m_lineWidth;
}

void CanvasRenderingContext2D::setLineWidth(float width)
{
    if (!(isfinite(width) && width > 0))
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
    if (!(isfinite(limit) && limit > 0))
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
    if (!isfinite(x))
        return;
    state().m_shadowOffset.setWidth(x);
    applyShadow();
}

float CanvasRenderingContext2D::shadowOffsetY() const
{
    return state().m_shadowOffset.height();
}

void CanvasRenderingContext2D::setShadowOffsetY(float y)
{
    if (!isfinite(y))
        return;
    state().m_shadowOffset.setHeight(y);
    applyShadow();
}

float CanvasRenderingContext2D::shadowBlur() const
{
    return state().m_shadowBlur;
}

void CanvasRenderingContext2D::setShadowBlur(float blur)
{
    if (!(isfinite(blur) && blur >= 0))
        return;
    state().m_shadowBlur = blur;
    applyShadow();
}

String CanvasRenderingContext2D::shadowColor() const
{
    return Color(state().m_shadowColor).serialized();
}

void CanvasRenderingContext2D::setShadowColor(const String& color)
{
    if (!CSSParser::parseColor(state().m_shadowColor, color))
        return;

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
#if ENABLE(ACCELERATED_2D_CANVAS)
    if (isAccelerated() && op != CompositeSourceOver) {
        c->setSharedGraphicsContext3D(0, 0, IntSize());
        m_drawingBuffer.clear();
        m_context3D.clear();
        // Mark as needing a style recalc so our compositing layer can be removed.
        canvas()->setNeedsStyleRecalc(SyntheticStyleChange);
    }
#endif
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

    if (!isfinite(m11) | !isfinite(m21) | !isfinite(dx) | !isfinite(m12) | !isfinite(m22) | !isfinite(dy))
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

    if (!isfinite(m11) | !isfinite(m21) | !isfinite(dx) | !isfinite(m12) | !isfinite(m22) | !isfinite(dy))
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
    if (color == state().m_unparsedStrokeColor)
        return;
    setStrokeStyle(CanvasStyle::createFromString(color));
    state().m_unparsedStrokeColor = color;
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel)
{
    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentRGBA(grayLevel, grayLevel, grayLevel, 1.0f))
        return;
    setStrokeStyle(CanvasStyle::createFromGrayLevelWithAlpha(grayLevel, 1.0f));
}

void CanvasRenderingContext2D::setStrokeColor(const String& color, float alpha)
{
    setStrokeStyle(CanvasStyle::createFromStringWithOverrideAlpha(color, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel, float alpha)
{
    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentRGBA(grayLevel, grayLevel, grayLevel, alpha))
        return;
    setStrokeStyle(CanvasStyle::createFromGrayLevelWithAlpha(grayLevel, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float r, float g, float b, float a)
{
    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentRGBA(r, g, b, a))
        return;
    setStrokeStyle(CanvasStyle::createFromRGBAChannels(r, g, b, a));
}

void CanvasRenderingContext2D::setStrokeColor(float c, float m, float y, float k, float a)
{
    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentCMYKA(c, m, y, k, a))
        return;
    setStrokeStyle(CanvasStyle::createFromCMYKAChannels(c, m, y, k, a));
}

void CanvasRenderingContext2D::setFillColor(const String& color)
{
    if (color == state().m_unparsedFillColor)
        return;
    setFillStyle(CanvasStyle::createFromString(color));
    state().m_unparsedFillColor = color;
}

void CanvasRenderingContext2D::setFillColor(float grayLevel)
{
    if (state().m_fillStyle && state().m_fillStyle->isEquivalentRGBA(grayLevel, grayLevel, grayLevel, 1.0f))
        return;
    setFillStyle(CanvasStyle::createFromGrayLevelWithAlpha(grayLevel, 1.0f));
}

void CanvasRenderingContext2D::setFillColor(const String& color, float alpha)
{
    setFillStyle(CanvasStyle::createFromStringWithOverrideAlpha(color, alpha));
}

void CanvasRenderingContext2D::setFillColor(float grayLevel, float alpha)
{
    if (state().m_fillStyle && state().m_fillStyle->isEquivalentRGBA(grayLevel, grayLevel, grayLevel, alpha))
        return;
    setFillStyle(CanvasStyle::createFromGrayLevelWithAlpha(grayLevel, alpha));
}

void CanvasRenderingContext2D::setFillColor(float r, float g, float b, float a)
{
    if (state().m_fillStyle && state().m_fillStyle->isEquivalentRGBA(r, g, b, a))
        return;
    setFillStyle(CanvasStyle::createFromRGBAChannels(r, g, b, a));
}

void CanvasRenderingContext2D::setFillColor(float c, float m, float y, float k, float a)
{
    if (state().m_fillStyle && state().m_fillStyle->isEquivalentCMYKA(c, m, y, k, a))
        return;
    setFillStyle(CanvasStyle::createFromCMYKAChannels(c, m, y, k, a));
}

void CanvasRenderingContext2D::beginPath()
{
    m_path.clear();
}

void CanvasRenderingContext2D::closePath()
{
    if (m_path.isEmpty())
        return;

    FloatRect boundRect = m_path.boundingRect();
    if (boundRect.width() || boundRect.height())
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

    FloatPoint p1 = FloatPoint(x, y);
    if (!m_path.hasCurrentPoint())
        m_path.moveTo(p1);
    else if (p1 != m_path.currentPoint())
        m_path.addLineTo(FloatPoint(x, y));
}

void CanvasRenderingContext2D::quadraticCurveTo(float cpx, float cpy, float x, float y)
{
    if (!isfinite(cpx) | !isfinite(cpy) | !isfinite(x) | !isfinite(y))
        return;
    if (!state().m_invertibleCTM)
        return;
    if (!m_path.hasCurrentPoint())
        m_path.moveTo(FloatPoint(cpx, cpy));

    FloatPoint p1 = FloatPoint(x, y);
    if (p1 != m_path.currentPoint())
        m_path.addQuadCurveTo(FloatPoint(cpx, cpy), p1);
}

void CanvasRenderingContext2D::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
    if (!isfinite(cp1x) | !isfinite(cp1y) | !isfinite(cp2x) | !isfinite(cp2y) | !isfinite(x) | !isfinite(y))
        return;
    if (!state().m_invertibleCTM)
        return;
    if (!m_path.hasCurrentPoint())
        m_path.moveTo(FloatPoint(cp1x, cp1y));

    FloatPoint p1 = FloatPoint(x, y);
    if (p1 != m_path.currentPoint())
        m_path.addBezierCurveTo(FloatPoint(cp1x, cp1y), FloatPoint(cp2x, cp2y), p1);
}

void CanvasRenderingContext2D::arcTo(float x1, float y1, float x2, float y2, float r, ExceptionCode& ec)
{
    ec = 0;
    if (!isfinite(x1) | !isfinite(y1) | !isfinite(x2) | !isfinite(y2) | !isfinite(r))
        return;

    if (r < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!state().m_invertibleCTM)
        return;

    FloatPoint p1 = FloatPoint(x1, y1);
    FloatPoint p2 = FloatPoint(x2, y2);

    if (!m_path.hasCurrentPoint())
        m_path.moveTo(p1);
    else if (p1 == m_path.currentPoint() || p1 == p2 || !r)
        lineTo(x1, y1);
    else
        m_path.addArcTo(p1, p2, r);
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

    if (sa == ea)
        return;

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
    if (!state().m_invertibleCTM)
        return;

    if (!isfinite(x) || !isfinite(y) || !isfinite(width) || !isfinite(height))
        return;

    if (!width && !height) {
        m_path.moveTo(FloatPoint(x, y));
        return;
    }

    if (width < 0) {
        width = -width;
        x -= width;
    }

    if (height < 0) {
        height = -height;
        y -= height;
    }

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
        c->fillPath();
        didDraw(m_path.boundingRect());
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

#if PLATFORM(QT)
        // Fast approximation of the stroke's bounding rect.
        // This yields a slightly oversized rect but is very fast
        // compared to Path::strokeBoundingRect().
        FloatRect boundingRect = m_path.platformPath().controlPointRect();
        boundingRect.inflate(state().m_miterLimit + state().m_lineWidth);
#else
        CanvasStrokeStyleApplier strokeApplier(this);
        FloatRect boundingRect = m_path.strokeBoundingRect(&strokeApplier);
#endif
        c->strokePath();
        didDraw(boundingRect);
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
    GraphicsContext* context = drawingContext();
    if (!context)
        return;
    if (!state().m_invertibleCTM)
        return;
    FloatRect rect(x, y, width, height);

    save();
    setAllAttributesToDefault();
    context->clearRect(rect);
    didDraw(rect);
    restore();
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

    // from the HTML5 Canvas spec:
    // If x0 = x1 and y0 = y1, then the linear gradient must paint nothing
    // If x0 = x1 and y0 = y1 and r0 = r1, then the radial gradient must paint nothing
    Gradient* gradient = c->fillGradient();
    if (gradient && gradient->isZeroSize())
        return;

    FloatRect rect(x, y, width, height);

    c->fillRect(rect);
    didDraw(rect);
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

    c->strokeRect(rect, lineWidth);
    didDraw(boundingRect);
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
    state().m_shadowColor = Color::transparent;
    applyShadow();
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color)
{
    if (!CSSParser::parseColor(state().m_shadowColor, color))
        return;

    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    applyShadow();
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = makeRGBA32FromFloats(grayLevel, grayLevel, grayLevel, 1.0f);

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    c->setShadow(IntSize(width, -height), state().m_shadowBlur, state().m_shadowColor, DeviceColorSpace);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color, float alpha)
{
    RGBA32 rgba;

    if (!CSSParser::parseColor(rgba, color))
        return;

    state().m_shadowColor = colorWithOverrideAlpha(rgba, alpha);
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    c->setShadow(IntSize(width, -height), state().m_shadowBlur, state().m_shadowColor, DeviceColorSpace);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel, float alpha)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = makeRGBA32FromFloats(grayLevel, grayLevel, grayLevel, alpha);

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    c->setShadow(IntSize(width, -height), state().m_shadowBlur, state().m_shadowColor, DeviceColorSpace);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float r, float g, float b, float a)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = makeRGBA32FromFloats(r, g, b, a);

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    c->setShadow(IntSize(width, -height), state().m_shadowBlur, state().m_shadowColor, DeviceColorSpace);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float c, float m, float y, float k, float a)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = makeRGBAFromCMYKA(c, m, y, k, a);

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
    dc->setShadow(IntSize(width, -height), blur, state().m_shadowColor, DeviceColorSpace);
#endif
}

void CanvasRenderingContext2D::clearShadow()
{
    state().m_shadowOffset = FloatSize();
    state().m_shadowBlur = 0;
    state().m_shadowColor = Color::transparent;
    applyShadow();
}

void CanvasRenderingContext2D::applyShadow()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    float width = state().m_shadowOffset.width();
    float height = state().m_shadowOffset.height();
    c->setShadow(FloatSize(width, -height), state().m_shadowBlur, state().m_shadowColor, DeviceColorSpace);
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
    if (m_cleanOrigins.contains(url.string()))
        return;

    if (canvas()->securityOrigin().taintsCanvas(url))
        canvas()->setOriginTainted();
    else
        m_cleanOrigins.add(url.string());
}

void CanvasRenderingContext2D::checkOrigin(const String& url)
{
    if (m_cleanOrigins.contains(url))
        return;

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

    if (!isfinite(dstRect.x()) || !isfinite(dstRect.y()) || !isfinite(dstRect.width()) || !isfinite(dstRect.height())
        || !isfinite(srcRect.x()) || !isfinite(srcRect.y()) || !isfinite(srcRect.width()) || !isfinite(srcRect.height()))
        return;

    if (!dstRect.width() || !dstRect.height())
        return;

    if (!image->complete())
        return;

    FloatRect normalizedSrcRect = normalizeRect(srcRect);
    FloatRect normalizedDstRect = normalizeRect(dstRect);

    FloatRect imageRect = FloatRect(FloatPoint(), size(image));
    if (!imageRect.contains(normalizedSrcRect) || !srcRect.width() || !srcRect.height()) {
        ec = INDEX_SIZE_ERR;
        return;
    }

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

    FloatRect sourceRect = c->roundToDevicePixels(normalizedSrcRect);
    FloatRect destRect = c->roundToDevicePixels(normalizedDstRect);
    c->drawImage(cachedImage->image(), DeviceColorSpace, destRect, sourceRect, state().m_globalComposite);
    didDraw(destRect);
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

    FloatRect srcCanvasRect = FloatRect(FloatPoint(), sourceCanvas->size());

    if (!srcCanvasRect.width() || !srcCanvasRect.height()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!srcCanvasRect.contains(normalizeRect(srcRect)) || !srcRect.width() || !srcRect.height()) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    ec = 0;

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

#if ENABLE(ACCELERATED_2D_CANVAS)
    // If we're drawing from one accelerated canvas 2d to another, avoid calling sourceCanvas->makeRenderingResultsAvailable()
    // as that will do a readback to software.
    CanvasRenderingContext* sourceContext = sourceCanvas->renderingContext();
    // FIXME: Implement an accelerated path for drawing from a WebGL canvas to a 2d canvas when possible.
    if (!isAccelerated() || !sourceContext || !sourceContext->isAccelerated() || !sourceContext->is2d())
        sourceCanvas->makeRenderingResultsAvailable();
#else
    sourceCanvas->makeRenderingResultsAvailable();
#endif

    c->drawImageBuffer(buffer, DeviceColorSpace, destRect, sourceRect, state().m_globalComposite);
    didDraw(destRect);
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

    if (video->readyState() == HTMLMediaElement::HAVE_NOTHING || video->readyState() == HTMLMediaElement::HAVE_METADATA)
        return;

    FloatRect videoRect = FloatRect(FloatPoint(), size(video));
    if (!videoRect.contains(normalizeRect(srcRect)) || !srcRect.width() || !srcRect.height()) {
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

    c->save();
    c->clip(destRect);
    c->translate(destRect.x(), destRect.y());
    c->scale(FloatSize(destRect.width() / sourceRect.width(), destRect.height() / sourceRect.height()));
    c->translate(-sourceRect.x(), -sourceRect.y());
    video->paintCurrentFrameInContext(c, IntRect(IntPoint(), size(video)));
    c->restore();
    didDraw(destRect);
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
    c->drawImage(cachedImage->image(), DeviceColorSpace, destRect, FloatRect(sx, sy, sw, sh), op);
    didDraw(destRect);
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
    if (!isfinite(x0) || !isfinite(y0) || !isfinite(r0) || !isfinite(x1) || !isfinite(y1) || !isfinite(r1)) {
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

    if (!image->complete())
        return 0;

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
    return CanvasPattern::create(canvas->copiedImage(), repeatX, repeatY, canvas->originClean());
}

void CanvasRenderingContext2D::didDraw(const FloatRect& r, unsigned options)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    FloatRect dirtyRect = r;
    if (options & CanvasDidDrawApplyTransform) {
        AffineTransform ctm = state().m_transform;
        dirtyRect = ctm.mapRect(r);
    }

    if (options & CanvasDidDrawApplyShadow && alphaChannel(state().m_shadowColor)) {
        // The shadow gets applied after transformation
        FloatRect shadowRect(dirtyRect);
        shadowRect.move(state().m_shadowOffset);
        shadowRect.inflate(state().m_shadowBlur);
        dirtyRect.unite(shadowRect);
    }

    if (options & CanvasDidDrawApplyClip) {
        // FIXME: apply the current clip to the rectangle. Unfortunately we can't get the clip
        // back out of the GraphicsContext, so to take clip into account for incremental painting,
        // we'd have to keep the clip path around.
    }

#if ENABLE(ACCELERATED_2D_CANVAS)
    if (isAccelerated())
        drawingContext()->markDirtyRect(enclosingIntRect(dirtyRect));
#endif
#if ENABLE(ACCELERATED_2D_CANVAS) && USE(ACCELERATED_COMPOSITING)
    // If we are drawing to hardware and we have a composited layer, just call rendererContentChanged().
    RenderBox* renderBox = canvas()->renderBox();
    if (isAccelerated() && renderBox && renderBox->hasLayer() && renderBox->layer()->hasAcceleratedCompositing())
        renderBox->layer()->rendererContentChanged();
    else
#endif
        canvas()->didDraw(dirtyRect);
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

PassRefPtr<ImageData> CanvasRenderingContext2D::createImageData(PassRefPtr<ImageData> imageData, ExceptionCode& ec) const
{
    if (!imageData) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }

    IntSize size(imageData->width(), imageData->height());
    return createEmptyImageData(size);
}

PassRefPtr<ImageData> CanvasRenderingContext2D::createImageData(float sw, float sh, ExceptionCode& ec) const
{
    ec = 0;
    if (!sw || !sh) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }
    if (!isfinite(sw) || !isfinite(sh)) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }

    FloatSize unscaledSize(fabs(sw), fabs(sh));
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
    if (!sw || !sh) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }
    if (!isfinite(sx) || !isfinite(sy) || !isfinite(sw) || !isfinite(sh)) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }

    FloatRect unscaledRect(sx, sy, sw, sh);
    IntRect scaledRect = canvas()->convertLogicalToDevice(unscaledRect);
    if (scaledRect.width() < 1)
        scaledRect.setWidth(1);
    if (scaledRect.height() < 1)
        scaledRect.setHeight(1);
    ImageBuffer* buffer = canvas()->buffer();
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
    if (!isfinite(dx) || !isfinite(dy) || !isfinite(dirtyX) || !isfinite(dirtyY) || !isfinite(dirtyWidth) || !isfinite(dirtyHeight)) {
        ec = NOT_SUPPORTED_ERR;
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
    sourceRect.move(-destOffset);
    IntPoint destPoint(destOffset.width(), destOffset.height());

    buffer->putUnmultipliedImageData(data, sourceRect, destPoint);
    didDraw(sourceRect, CanvasDidDrawApplyNone); // ignore transform, shadow and clip
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
    if (RenderStyle* computedStyle = canvas()->computedStyle()) {
        newStyle->setFontDescription(computedStyle->fontDescription());
        newStyle->font().update(newStyle->font().fontSelector());
    }

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

#if PLATFORM(QT)
    // We always use complex text shaping since it can't be turned off for QPainterPath::addText().
    Font::CodePath oldCodePath = Font::codePath();
    Font::setCodePath(Font::Complex);
#endif

    metrics->setWidth(accessFont().width(TextRun(text.characters(), text.length())));

#if PLATFORM(QT)
    Font::setCodePath(oldCodePath);
#endif

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
        c->clipToImageBuffer(maskImage.get(), maskRect);
        drawStyle->applyFillColor(c);
        c->fillRect(maskRect);
        c->restore();

        return;
    }
#endif

    c->setTextDrawingMode(fill ? cTextFill : cTextStroke);

#if PLATFORM(QT)
    // We always use complex text shaping since it can't be turned off for QPainterPath::addText().
    Font::CodePath oldCodePath = Font::codePath();
    Font::setCodePath(Font::Complex);
#endif

    c->drawBidiText(font, textRun, location);

    if (fill)
        didDraw(textRect);
    else {
        // When stroking text, pointy miters can extend outside of textRect, so we
        // punt and dirty the whole canvas.
        didDraw(FloatRect(0, 0, canvas()->width(), canvas()->height()));
    }

#if PLATFORM(QT)
    Font::setCodePath(oldCodePath);
#endif
}

const Font& CanvasRenderingContext2D::accessFont()
{
    if (!state().m_realizedFont)
        setFont(state().m_unparsedFont);
    return state().m_font;
}

void CanvasRenderingContext2D::paintRenderingResultsToCanvas()
{
#if ENABLE(ACCELERATED_2D_CANVAS)
    if (GraphicsContext* c = drawingContext())
        c->syncSoftwareCanvas();
#endif
}

#if ENABLE(ACCELERATED_2D_CANVAS) && USE(ACCELERATED_COMPOSITING)
PlatformLayer* CanvasRenderingContext2D::platformLayer() const
{
    return m_drawingBuffer->platformLayer();
}
#endif

} // namespace WebCore
