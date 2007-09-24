/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
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
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "NotImplemented.h"
#include "RenderHTMLCanvas.h"
#include "Settings.h"
#include <wtf/MathExtras.h>

#if PLATFORM(QT)
#include <QPainter>
#include <QPixmap>
#include <QPainterPath>
#endif

namespace WebCore {

using namespace HTMLNames;

CanvasRenderingContext2D::CanvasRenderingContext2D(HTMLCanvasElement* canvas)
    : m_canvas(canvas)
    , m_stateStack(1)
{
}

void CanvasRenderingContext2D::reset()
{
    m_stateStack.resize(1);
    m_stateStack.first() = State();
}

CanvasRenderingContext2D::State::State()
    : m_strokeStyle(new CanvasStyle("black"))
    , m_fillStyle(new CanvasStyle("black"))
    , m_lineWidth(1)
    , m_lineCap(ButtCap)
    , m_lineJoin(MiterJoin)
    , m_miterLimit(10)
    , m_shadowBlur(0)
    , m_shadowColor("black")
    , m_globalAlpha(1)
    , m_globalComposite(CompositeSourceOver)
    , m_appliedStrokePattern(false)
    , m_appliedFillPattern(false)
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
    m_stateStack.removeLast();
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
    state().m_strokeStyle = style;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state().m_strokeStyle->applyStrokeColor(c);
    state().m_appliedStrokePattern = false;
}

CanvasStyle* CanvasRenderingContext2D::fillStyle() const
{
    return state().m_fillStyle.get();
}

void CanvasRenderingContext2D::setFillStyle(PassRefPtr<CanvasStyle> style)
{
    if (!style)
        return;
    state().m_fillStyle = style;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state().m_fillStyle->applyFillColor(c);
    state().m_appliedFillPattern = false;
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
    c->scale(FloatSize(sx, sy));
    state().m_path.transform(AffineTransform().scale(1.0/sx, 1.0/sy));
}

void CanvasRenderingContext2D::rotate(float angleInRadians)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->rotate(angleInRadians);
    state().m_path.transform(AffineTransform().rotate(-angleInRadians / piDouble * 180.0));
}

void CanvasRenderingContext2D::translate(float tx, float ty)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->translate(tx, ty);
    state().m_path.transform(AffineTransform().translate(-tx, -ty));
}

void CanvasRenderingContext2D::setStrokeColor(const String& color)
{
    setStrokeStyle(new CanvasStyle(color));
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel)
{
    setStrokeStyle(new CanvasStyle(grayLevel, 1));
}

void CanvasRenderingContext2D::setStrokeColor(const String& color, float alpha)
{
    setStrokeStyle(new CanvasStyle(color, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel, float alpha)
{
    setStrokeStyle(new CanvasStyle(grayLevel, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float r, float g, float b, float a)
{
    setStrokeStyle(new CanvasStyle(r, g, b, a));
}

void CanvasRenderingContext2D::setStrokeColor(float c, float m, float y, float k, float a)
{
    setStrokeStyle(new CanvasStyle(c, m, y, k, a));
}

void CanvasRenderingContext2D::setFillColor(const String& color)
{
    setFillStyle(new CanvasStyle(color));
}

void CanvasRenderingContext2D::setFillColor(float grayLevel)
{
    setFillStyle(new CanvasStyle(grayLevel, 1));
}

void CanvasRenderingContext2D::setFillColor(const String& color, float alpha)
{
    setFillStyle(new CanvasStyle(color, 1));
}

void CanvasRenderingContext2D::setFillColor(float grayLevel, float alpha)
{
    setFillStyle(new CanvasStyle(grayLevel, alpha));
}

void CanvasRenderingContext2D::setFillColor(float r, float g, float b, float a)
{
    setFillStyle(new CanvasStyle(r, g, b, a));
}

void CanvasRenderingContext2D::setFillColor(float c, float m, float y, float k, float a)
{
    setFillStyle(new CanvasStyle(c, m, y, k, a));
}

void CanvasRenderingContext2D::beginPath()
{
    state().m_path.clear();
}

void CanvasRenderingContext2D::closePath()
{
    state().m_path.closeSubpath();
}

void CanvasRenderingContext2D::moveTo(float x, float y)
{
    state().m_path.moveTo(FloatPoint(x, y));
}

void CanvasRenderingContext2D::lineTo(float x, float y)
{
    state().m_path.addLineTo(FloatPoint(x, y));
}

void CanvasRenderingContext2D::quadraticCurveTo(float cpx, float cpy, float x, float y)
{
    state().m_path.addQuadCurveTo(FloatPoint(cpx, cpy), FloatPoint(x, y));
}

void CanvasRenderingContext2D::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
    state().m_path.addBezierCurveTo(FloatPoint(cp1x, cp1y), FloatPoint(cp2x, cp2y), FloatPoint(x, y));
}

void CanvasRenderingContext2D::arcTo(float x0, float y0, float x1, float y1, float r, ExceptionCode& ec)
{
    ec = 0;
    if (!(r > 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    state().m_path.addArcTo(FloatPoint(x0, y0), FloatPoint(x1, y1), r);
}

void CanvasRenderingContext2D::arc(float x, float y, float r, float sa, float ea, bool clockwise, ExceptionCode& ec)
{
    ec = 0;
    if (!(r > 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    state().m_path.addArc(FloatPoint(x, y), r, sa, ea, clockwise);
}

void CanvasRenderingContext2D::rect(float x, float y, float width, float height, ExceptionCode& ec)
{
    ec = 0;
    if (!(width >= 0 && height >= 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    state().m_path.addRect(FloatRect(x, y, width, height));
}

void CanvasRenderingContext2D::clearPathForDashboardBackwardCompatibilityMode()
{
    if (m_canvas)
        if (Settings* settings = m_canvas->document()->settings())
            if (settings->usesDashboardBackwardCompatibilityMode())
                state().m_path.clear();
}

void CanvasRenderingContext2D::fill()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    CGContextBeginPath(c->platformContext());
    CGContextAddPath(c->platformContext(), state().m_path.platformPath());

    if (!state().m_path.isEmpty())
        willDraw(CGContextGetPathBoundingBox(c->platformContext()));

    if (state().m_fillStyle->gradient()) {
        // Shading works on the entire clip region, so convert the current path to a clip.
        c->save();
        CGContextClip(c->platformContext());
        CGContextDrawShading(c->platformContext(), state().m_fillStyle->gradient()->platformShading());        
        c->restore();
    } else {
        if (state().m_fillStyle->pattern())
            applyFillPattern();
        CGContextFillPath(c->platformContext());
    }
#elif PLATFORM(QT)
    QPainterPath* path = state().m_path.platformPath();
    QPainter* p = static_cast<QPainter*>(c->platformContext());
    willDraw(path->controlPointRect());
    if (state().m_fillStyle->gradient()) {
        p->fillPath(*path, QBrush(*(state().m_fillStyle->gradient()->platformShading())));
    } else {
        if (state().m_fillStyle->pattern())
            applyFillPattern();
        p->fillPath(*path, p->brush());
    }
#endif

    clearPathForDashboardBackwardCompatibilityMode();
}

void CanvasRenderingContext2D::stroke()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    CGContextBeginPath(c->platformContext());
    CGContextAddPath(c->platformContext(), state().m_path.platformPath());

    if (!state().m_path.isEmpty()) {
        float lineWidth = state().m_lineWidth;
        float inset = -lineWidth / 2;
        CGRect boundingRect = CGRectInset(CGContextGetPathBoundingBox(c->platformContext()), inset, inset);
        willDraw(boundingRect);
    }

    if (state().m_strokeStyle->gradient()) {
        // Shading works on the entire clip region, so convert the current path to a clip.
        c->save();
        CGContextReplacePathWithStrokedPath(c->platformContext());
        CGContextClip(c->platformContext());
        CGContextDrawShading(c->platformContext(), state().m_strokeStyle->gradient()->platformShading());        
        c->restore();
    } else {
        if (state().m_strokeStyle->pattern())
            applyStrokePattern();
        CGContextStrokePath(c->platformContext());
    }
#elif PLATFORM(QT)
    QPainterPath* path = state().m_path.platformPath();
    QPainter* p = static_cast<QPainter*>(c->platformContext());
    willDraw(path->controlPointRect());
    if (state().m_strokeStyle->gradient()) {
        p->save();
        p->setBrush(*(state().m_strokeStyle->gradient()->platformShading()));
        p->strokePath(*path, p->pen());
        p->restore();
    } else {
        if (state().m_strokeStyle->pattern())
            applyStrokePattern();
        p->strokePath(*path, p->pen());
    }
#endif

    clearPathForDashboardBackwardCompatibilityMode();
}

void CanvasRenderingContext2D::clip()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->clip(state().m_path);
    clearPathForDashboardBackwardCompatibilityMode();
}

void CanvasRenderingContext2D::clearRect(float x, float y, float width, float height, ExceptionCode& ec)
{
    ec = 0;
    if (!(width >= 0 && height >= 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    FloatRect rect(x, y, width, height);
    willDraw(rect);
    c->clearRect(rect);
}

void CanvasRenderingContext2D::fillRect(float x, float y, float width, float height, ExceptionCode& ec)
{
    ec = 0;

    if (!(width >= 0 && height >= 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    CGRect rect = CGRectMake(x, y, width, height);

    willDraw(rect);

    if (state().m_fillStyle->gradient()) {
        // Shading works on the entire clip region, so convert the rect to a clip.
        c->save();
        CGContextClipToRect(c->platformContext(), rect);
        CGContextDrawShading(c->platformContext(), state().m_fillStyle->gradient()->platformShading());        
        c->restore();
    } else {
        if (state().m_fillStyle->pattern())
            applyFillPattern();
        CGContextFillRect(c->platformContext(), rect);
    }
#elif PLATFORM(QT)
    QRectF rect(x, y, width, height);
    willDraw(rect);
    QPainter* p = static_cast<QPainter*>(c->platformContext());
    if (state().m_fillStyle->gradient()) {
        p->fillRect(rect, QBrush(*(state().m_fillStyle->gradient()->platformShading())));
    } else {
        if (state().m_fillStyle->pattern())
            applyFillPattern();
        p->fillRect(rect, p->brush());
    }
#endif
}

void CanvasRenderingContext2D::strokeRect(float x, float y, float width, float height, ExceptionCode& ec)
{
    strokeRect(x, y, width, height, state().m_lineWidth, ec);
}

void CanvasRenderingContext2D::strokeRect(float x, float y, float width, float height, float lineWidth, ExceptionCode& ec)
{
    ec = 0;

    if (!(width >= 0 && height >= 0 && lineWidth >= 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    FloatRect rect(x, y, width, height);

    FloatRect boundingRect = rect;
    boundingRect.inflate(lineWidth / 2);
    willDraw(boundingRect);

    // FIXME: No support for gradients!
    if (state().m_strokeStyle->pattern())
        applyStrokePattern();

    c->strokeRect(rect, lineWidth);
}

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
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    const CGFloat components[2] = { grayLevel, 1 };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceGray();
    CGColorRef color = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c->platformContext(), CGSizeMake(width, height), blur, color);
    CGColorRelease(color);
#endif
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color, float alpha)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = color;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    RGBA32 rgba = CSSParser::parseColor(color);
    const CGFloat components[4] = {
        ((rgba >> 16) & 0xFF) / 255.0f,
        ((rgba >> 8) & 0xFF) / 255.0f,
        (rgba & 0xFF) / 255.0f,
        alpha
    };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef shadowColor = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c->platformContext(), CGSizeMake(width, height), blur, shadowColor);
    CGColorRelease(shadowColor);
#endif
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel, float alpha)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    const CGFloat components[2] = { grayLevel, alpha };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceGray();
    CGColorRef color = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c->platformContext(), CGSizeMake(width, height), blur, color);
    CGColorRelease(color);
#endif
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float r, float g, float b, float a)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    const CGFloat components[4] = { r, g, b, a };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef shadowColor = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c->platformContext(), CGSizeMake(width, height), blur, shadowColor);
    CGColorRelease(shadowColor);
#endif
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float c, float m, float y, float k, float a)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    GraphicsContext* dc = drawingContext();
    if (!dc)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    const CGFloat components[5] = { c, m, y, k, a };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceCMYK();
    CGColorRef shadowColor = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(dc->platformContext(), CGSizeMake(width, height), blur, shadowColor);
    CGColorRelease(shadowColor);
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
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    RGBA32 rgba = state().m_shadowColor.isEmpty() ? 0 : CSSParser::parseColor(state().m_shadowColor);
    const CGFloat components[4] = {
        ((rgba >> 16) & 0xFF) / 255.0f,
        ((rgba >> 8) & 0xFF) / 255.0f,
        (rgba & 0xFF) / 255.0f,
        ((rgba >> 24) & 0xFF) / 255.0f
    };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef color = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c->platformContext(), state().m_shadowOffset, state().m_shadowBlur, color);
    CGColorRelease(color);
#endif
}

static IntSize size(HTMLImageElement* image)
{
    if (CachedImage* cachedImage = image->cachedImage())
        return cachedImage->imageSize();
    return IntSize();
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image, float x, float y)
{
    ASSERT(image);
    IntSize s = size(image);
    ExceptionCode ec;
    drawImage(image, x, y, s.width(), s.height(), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image,
    float x, float y, float width, float height, ExceptionCode& ec)
{
    ASSERT(image);
    IntSize s = size(image);
    drawImage(image, FloatRect(0, 0, s.width(), s.height()), FloatRect(x, y, width, height), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image, const FloatRect& srcRect, const FloatRect& dstRect,
    ExceptionCode& ec)
{
    ASSERT(image);

    ec = 0;

    FloatRect imageRect = FloatRect(FloatPoint(), size(image));
    if (!(imageRect.contains(srcRect) && srcRect.width() >= 0 && srcRect.height() >= 0 
            && dstRect.width() >= 0 && dstRect.height() >= 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (srcRect.isEmpty() || dstRect.isEmpty())
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    CachedImage* cachedImage = image->cachedImage();
    if (!cachedImage)
        return;

    FloatRect sourceRect = c->roundToDevicePixels(srcRect);
    FloatRect destRect = c->roundToDevicePixels(dstRect);
    willDraw(destRect);
    c->drawImage(cachedImage->image(), destRect, sourceRect, state().m_globalComposite);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* canvas, float x, float y)
{
    ASSERT(canvas);
    ExceptionCode ec;
    drawImage(canvas, x, y, canvas->width(), canvas->height(), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* canvas,
    float x, float y, float width, float height, ExceptionCode& ec)
{
    ASSERT(canvas);
    drawImage(canvas, FloatRect(0, 0, canvas->width(), canvas->height()), FloatRect(x, y, width, height), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* canvas, const FloatRect& srcRect,
    const FloatRect& dstRect, ExceptionCode& ec)
{
    ASSERT(canvas);

    ec = 0;

    FloatRect srcCanvasRect = FloatRect(FloatPoint(), canvas->size());
    if (!(srcCanvasRect.contains(srcRect) && srcRect.width() >= 0 && srcRect.height() >= 0 
            && dstRect.width() >= 0 && dstRect.height() >= 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (srcRect.isEmpty() || dstRect.isEmpty())
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
        
    FloatRect sourceRect = c->roundToDevicePixels(srcRect);
    FloatRect destRect = c->roundToDevicePixels(dstRect);
        
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    CGImageRef platformImage = canvas->createPlatformImage();
    if (!platformImage)
        return;

    willDraw(destRect);

    float iw = CGImageGetWidth(platformImage);
    float ih = CGImageGetHeight(platformImage);
    if (sourceRect.x() == 0 && sourceRect.y() == 0 && iw == sourceRect.width() && ih == sourceRect.height()) {
        // Fast path, yay!
        CGContextDrawImage(c->platformContext(), destRect, platformImage);
    } else {
        // Slow path, boo!
        // Create a new bitmap of the appropriate size and then draw that into our context.

        size_t csw = static_cast<size_t>(ceilf(sourceRect.width()));
        size_t csh = static_cast<size_t>(ceilf(sourceRect.height()));

        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        size_t bytesPerRow = csw * 4;
        void* buffer = fastMalloc(csh * bytesPerRow);

        CGContextRef clippedSourceContext = CGBitmapContextCreate(buffer, csw, csh,
            8, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast);
        CGColorSpaceRelease(colorSpace);
        CGContextTranslateCTM(clippedSourceContext, -sourceRect.x(), -sourceRect.y());
        CGContextDrawImage(clippedSourceContext, CGRectMake(0, 0, iw, ih), platformImage);

        CGImageRef clippedSourceImage = CGBitmapContextCreateImage(clippedSourceContext);
        CGContextRelease(clippedSourceContext);

        CGContextDrawImage(c->platformContext(), destRect, clippedSourceImage);
        CGImageRelease(clippedSourceImage);
        
        fastFree(buffer);
    }

    CGImageRelease(platformImage);
#elif PLATFORM(QT)
    QImage px = canvas->createPlatformImage();
    if (px.isNull())
        return;
    willDraw(dstRect);
    QPainter* painter = static_cast<QPainter*>(c->platformContext());
    painter->drawImage(dstRect, px, srcRect);
#endif
}

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

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    CompositeOperator op;
    if (!parseCompositeOperator(compositeOperation, op))
        op = CompositeSourceOver;

    FloatRect destRect = FloatRect(dx, dy, dw, dh);
    willDraw(destRect);
    c->drawImage(cachedImage->image(), destRect, FloatRect(sx, sy, sw, sh), op);
}

void CanvasRenderingContext2D::setAlpha(float alpha)
{
    setGlobalAlpha(alpha);
}

void CanvasRenderingContext2D::setCompositeOperation(const String& operation)
{
    setGlobalCompositeOperation(operation);
}

PassRefPtr<CanvasGradient> CanvasRenderingContext2D::createLinearGradient(float x0, float y0, float x1, float y1)
{
    return new CanvasGradient(FloatPoint(x0, y0), FloatPoint(x1, y1));
}

PassRefPtr<CanvasGradient> CanvasRenderingContext2D::createRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1)
{
    return new CanvasGradient(FloatPoint(x0, y0), r0, FloatPoint(x1, y1), r1);
}

PassRefPtr<CanvasPattern> CanvasRenderingContext2D::createPattern(HTMLImageElement* image,
    const String& repetitionType, ExceptionCode& ec)
{
    bool repeatX, repeatY;
    CanvasPattern::parseRepetitionType(repetitionType, repeatX, repeatY, ec);
    if (ec)
        return 0;
    return new CanvasPattern(image ? image->cachedImage() : 0, repeatX, repeatY);
}

PassRefPtr<CanvasPattern> CanvasRenderingContext2D::createPattern(HTMLCanvasElement* canvas,
    const String& repetitionType, ExceptionCode& ec)
{
    bool repeatX, repeatY;
    CanvasPattern::parseRepetitionType(repetitionType, repeatX, repeatY, ec);
    if (ec)
        return 0;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    CGImageRef image = canvas->createPlatformImage();
    if (!image)
        return 0;
    PassRefPtr<CanvasPattern> pattern = new CanvasPattern(image, repeatX, repeatY);
    CGImageRelease(image);
    return pattern;
#else
    notImplemented();
    return 0;
#endif
}

void CanvasRenderingContext2D::willDraw(const FloatRect& r)
{
    if (!m_canvas)
        return;
    m_canvas->willDraw(r);
}

GraphicsContext* CanvasRenderingContext2D::drawingContext() const
{
    if (!m_canvas)
        return 0;
    return m_canvas->drawingContext();
}

void CanvasRenderingContext2D::applyStrokePattern()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

#if PLATFORM(CG)
    // Check for case where the pattern is already set.
    CGAffineTransform m = CGContextGetCTM(c->platformContext());
    if (state().m_appliedStrokePattern
            && CGAffineTransformEqualToTransform(m, state().m_strokeStylePatternTransform))
        return;

    CanvasPattern* pattern = state().m_strokeStyle->pattern();
    if (!pattern)
        return;

    CGPatternRef platformPattern = pattern->createPattern(m);
    if (!platformPattern)
        return;

    CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(0);
    CGContextSetStrokeColorSpace(c->platformContext(), patternSpace);
    CGColorSpaceRelease(patternSpace);

    const CGFloat patternAlpha = 1;
    CGContextSetStrokePattern(c->platformContext(), platformPattern, &patternAlpha);
    CGPatternRelease(platformPattern);

    state().m_strokeStylePatternTransform = m;
#elif PLATFORM(QT)
    fprintf(stderr, "FIXME: CanvasRenderingContext2D::applyStrokePattern\n");
#endif
    state().m_appliedStrokePattern = true;
}

void CanvasRenderingContext2D::applyFillPattern()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

#if PLATFORM(CG)
    // Check for case where the pattern is already set.
    CGAffineTransform m = CGContextGetCTM(c->platformContext());
    if (state().m_appliedFillPattern
            && CGAffineTransformEqualToTransform(m, state().m_fillStylePatternTransform))
        return;

    CanvasPattern* pattern = state().m_fillStyle->pattern();
    if (!pattern)
        return;

    CGPatternRef platformPattern = pattern->createPattern(m);
    if (!platformPattern)
        return;

    CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(0);
    CGContextSetFillColorSpace(c->platformContext(), patternSpace);
    CGColorSpaceRelease(patternSpace);

    const CGFloat patternAlpha = 1;
    CGContextSetFillPattern(c->platformContext(), platformPattern, &patternAlpha);
    CGPatternRelease(platformPattern);

    state().m_fillStylePatternTransform = m;
#elif PLATFORM(QT)
    fprintf(stderr, "FIXME: CanvasRenderingContext2D::applyFillPattern\n");
#endif
    state().m_appliedFillPattern = true;
}

} // namespace WebCore
