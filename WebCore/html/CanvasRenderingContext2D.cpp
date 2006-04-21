/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasStyle.h"
#include "ExceptionCode.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HTMLNames.h"
#include "RenderHTMLCanvas.h"
#include "cssparser.h"

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
#if __APPLE__
    , m_platformContextStrokeStyleIsPattern(false)
    , m_platformContextFillStyleIsPattern(false)
#endif
{
}

void CanvasRenderingContext2D::save()
{
    ASSERT(m_stateStack.size() >= 1);
    m_stateStack.append(state());

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGContextSaveGState(c);
#endif
}

void CanvasRenderingContext2D::restore()
{
    ASSERT(m_stateStack.size() >= 1);
    if (m_stateStack.size() <= 1)
        return;
    m_stateStack.removeLast();

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGContextRestoreGState(c);
#endif
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

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    state().m_strokeStyle->applyStrokeColor(platformContext());
    state().m_platformContextStrokeStyleIsPattern = false;
#endif
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

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    state().m_fillStyle->applyFillColor(platformContext());
    state().m_platformContextFillStyleIsPattern = false;
#endif
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

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextSetLineWidth(platformContext(), width);
#endif
}

String CanvasRenderingContext2D::lineCap() const
{
    const char* const names[3] = { "butt", "round", "square" };
    return names[state().m_lineCap];
}

void CanvasRenderingContext2D::setLineCap(const String& s)
{
    Cap cap;
    if (s == "butt")
        cap = ButtCap;
    else if (s == "round")
        cap = RoundCap;
    else if (s == "square")
        cap = SquareCap;
    else
        return;

    state().m_lineCap = cap;

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    switch (cap) {
        case ButtCap:
            CGContextSetLineCap(platformContext(), kCGLineCapButt);
            break;
        case RoundCap:
            CGContextSetLineCap(platformContext(), kCGLineCapRound);
            break;
        case SquareCap:
            CGContextSetLineCap(platformContext(), kCGLineCapSquare);
            break;
    }
#endif
}

String CanvasRenderingContext2D::lineJoin() const
{
    const char* const names[3] = { "miter", "round", "bevel" };
    return names[state().m_lineJoin];
}

void CanvasRenderingContext2D::setLineJoin(const String& s)
{
    Join join;
    if (s == "miter")
        join = MiterJoin;
    else if (s == "round")
        join = RoundJoin;
    else if (s == "bevel")
        join = BevelJoin;
    else
        return;

    state().m_lineJoin = join;

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    switch (join) {
        case MiterJoin:
            CGContextSetLineJoin(platformContext(), kCGLineJoinMiter);
            break;
        case RoundJoin:
            CGContextSetLineJoin(platformContext(), kCGLineJoinRound);
            break;
        case BevelJoin:
            CGContextSetLineJoin(platformContext(), kCGLineJoinBevel);
            break;
    }
#endif
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

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextSetMiterLimit(platformContext(), limit);
#endif
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
    // FIXME: What should this return if you called setShadow with a non-string color.
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

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextSetAlpha(platformContext(), alpha);
#endif
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

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    WebCore::setCompositeOperation(platformContext(), op);
#endif
}

void CanvasRenderingContext2D::scale(float sx, float sy)
{
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextScaleCTM(platformContext(), sx, sy);
#endif
}

void CanvasRenderingContext2D::rotate(float angleInRadians)
{
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRotateCTM(platformContext(), angleInRadians);
#endif
}

void CanvasRenderingContext2D::translate(float tx, float ty)
{
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextTranslateCTM(platformContext(), tx, ty);
#endif
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
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGContextBeginPath(c);
#endif
}

void CanvasRenderingContext2D::closePath()
{
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGContextClosePath(c);
#endif
}

void CanvasRenderingContext2D::moveTo(float x, float y)
{
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGContextMoveToPoint(c, x, y);
#endif
}

void CanvasRenderingContext2D::lineTo(float x, float y)
{
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGContextAddLineToPoint(c, x, y);
#endif
}

void CanvasRenderingContext2D::quadraticCurveTo(float cpx, float cpy, float x, float y)
{
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGContextAddQuadCurveToPoint(c, cpx, cpy, x, y);
#endif
}

void CanvasRenderingContext2D::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGContextAddCurveToPoint(c, cp1x, cp1y, cp2x, cp2y, x, y);
#endif
}

void CanvasRenderingContext2D::arcTo(float x0, float y0, float x1, float y1, float radius, ExceptionCode& ec)
{
    ec = 0;

    if (!(radius > 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGContextAddArcToPoint(c, x0, y0, x1, y1, radius);
#endif
}

void CanvasRenderingContext2D::arc(float x, float y, float r, float sa, float ea, bool clockwise, ExceptionCode& ec)
{
    ec = 0;

    if (!(r > 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGContextAddArc(c, x, y, r, sa, ea, clockwise);
#endif
}

void CanvasRenderingContext2D::rect(float x, float y, float width, float height, ExceptionCode& ec)
{
    ec = 0;

    if (!(width > 0 && height > 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGContextAddRect(c, CGRectMake(x, y, width, height));
#endif
}

void CanvasRenderingContext2D::fill()
{
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;

    willDraw(CGContextGetPathBoundingBox(c));

    if (state().m_fillStyle->gradient()) {
        // Shading works on the entire clip region, so convert the current path to a clip.
        CGContextSaveGState(c);        
        CGContextClip(c);
        CGContextDrawShading(c, state().m_fillStyle->gradient()->platformShading());        
        CGContextRestoreGState(c);
    } else {
        if (state().m_fillStyle->pattern())
            applyFillPattern();
        CGContextFillPath(c);
    }
#endif
}

void CanvasRenderingContext2D::stroke()
{
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;

    // FIXME: Is this right for negative widths?
    float lineWidth = state().m_lineWidth;
    float inset = -lineWidth / 2;
    CGRect boundingRect = CGRectInset(CGContextGetPathBoundingBox(c), inset, inset);

    willDraw(boundingRect);

    if (state().m_strokeStyle->gradient()) {
        // Shading works on the entire clip region, so convert the current path to a clip.
        CGContextSaveGState(c);        
        CGContextReplacePathWithStrokedPath(c);
        CGContextClip(c);
        CGContextDrawShading(c, state().m_strokeStyle->gradient()->platformShading());        
        CGContextRestoreGState(c);
    } else {
        if (state().m_strokeStyle->pattern())
            applyStrokePattern();
        CGContextStrokePath(c);
    }
#endif
}

void CanvasRenderingContext2D::clip()
{
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGContextClip(c);
#endif
}

void CanvasRenderingContext2D::clearRect(float x, float y, float width, float height, ExceptionCode& ec)
{
    ec = 0;

    if (!(width > 0 && height > 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGRect rect = CGRectMake(x, y, width, height);

    willDraw(rect);

    CGContextClearRect(c, rect);
#endif
}

void CanvasRenderingContext2D::fillRect(float x, float y, float width, float height, ExceptionCode& ec)
{
    ec = 0;

    if (!(width > 0 && height > 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    CGRect rect = CGRectMake(x, y, width, height);

    willDraw(rect);

    if (state().m_fillStyle->gradient()) {
        // Shading works on the entire clip region, so convert the rect to a clip.
        CGContextSaveGState(c);        
        CGContextClipToRect(c, rect);
        CGContextDrawShading(c, state().m_fillStyle->gradient()->platformShading());        
        CGContextRestoreGState(c);
    } else {
        if (state().m_fillStyle->pattern())
            applyFillPattern();
        CGContextFillRect(c, rect);
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

    if (!(width > 0 && height > 0 && lineWidth > 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;

    // FIXME: Is this right for negative widths?
    float inset = -lineWidth / 2;
    CGRect rect = CGRectMake(x, y, width, height);
    CGRect boundingRect = CGRectInset(rect, inset, inset);

    willDraw(boundingRect);

    // FIXME: No support for gradients!
    if (state().m_strokeStyle->pattern())
        applyStrokePattern();
    CGContextStrokeRectWithWidth(c, rect, lineWidth);
#endif
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

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    const CGFloat components[2] = { grayLevel, 1 };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceGray();
    CGColorRef color = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c, CGSizeMake(width, height), blur, color);
    CGColorRelease(color);
#endif
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color, float alpha)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = color;

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    RGBA32 rgba = CSSParser::parseColor(color);
    const CGFloat components[4] = {
        ((rgba >> 16) & 0xFF) / 255.0,
        ((rgba >> 8) & 0xFF) / 255.0,
        (rgba & 0xFF) / 255.0,
        alpha
    };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef shadowColor = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c, CGSizeMake(width, height), blur, shadowColor);
    CGColorRelease(shadowColor);
#endif
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel, float alpha)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    const CGFloat components[2] = { grayLevel, alpha };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceGray();
    CGColorRef color = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c, CGSizeMake(width, height), blur, color);
    CGColorRelease(color);
#endif
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float r, float g, float b, float a)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    const CGFloat components[4] = { r, g, b, a };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef shadowColor = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c, CGSizeMake(width, height), blur, shadowColor);
    CGColorRelease(shadowColor);
#endif
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float c, float m, float y, float k, float a)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef dc = platformContext();
    if (!dc)
        return;
    const CGFloat components[5] = { c, m, y, k, a };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceCMYK();
    CGColorRef shadowColor = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(dc, CGSizeMake(width, height), blur, shadowColor);
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
    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;
    RGBA32 rgba = state().m_shadowColor.isEmpty() ? 0 : CSSParser::parseColor(state().m_shadowColor);
    const CGFloat components[4] = {
        ((rgba >> 16) & 0xFF) / 255.0,
        ((rgba >> 8) & 0xFF) / 255.0,
        (rgba & 0xFF) / 255.0,
        ((rgba >> 24) & 0xFF) / 255.0
    };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef color = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c, state().m_shadowOffset, state().m_shadowBlur, color);
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
    drawImage(image, 0, 0, s.width(), s.height(), x, y, width, height, ec);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionCode& ec)
{
    ASSERT(image);

    ec = 0;

    FloatRect imageRect = FloatRect(FloatPoint(), size(image));
    FloatRect sourceRect = FloatRect(sx, sy, sw, sh);

    if (!(imageRect.contains(sourceRect) && sw > 0 && sh > 0 && dw > 0 && dh > 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    CachedImage* cachedImage = image->cachedImage();
    if (!cachedImage)
        return;

    FloatRect destRect = FloatRect(dx, dy, dw, dh);
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
    drawImage(canvas, 0, 0, canvas->width(), canvas->height(), x, y, width, height, ec);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* canvas,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionCode& ec)
{
    ASSERT(canvas);

    ec = 0;

    if (!(sw > 0 && sh > 0 && dw > 0 && dh > 0)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!canvas)
        return;

    // FIXME: Do this through platform-independent GraphicsContext API.
#if __APPLE__
    CGContextRef c = platformContext();
    if (!c)
        return;

    CGImageRef platformImage = canvas->createPlatformImage();
    if (!platformImage)
        return;

    FloatRect destRect = FloatRect(dx, dy, dw, dh);
    willDraw(destRect);

    float iw = CGImageGetWidth(platformImage);
    float ih = CGImageGetHeight(platformImage);
    if (sx == 0 && sy == 0 && iw == sw && ih == sh) {
        // Fast path, yay!
        CGContextDrawImage(c, CGRectMake(dx, dy, dw, dh), platformImage);
    } else {
        // Slow path, boo!
        // Create a new bitmap of the appropriate size and then draw that into our context.

        size_t csw = static_cast<size_t>(ceilf(sw));
        size_t csh = static_cast<size_t>(ceilf(sh));

        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        size_t bytesPerRow = csw * 4;
        void* buffer = fastMalloc(csh * bytesPerRow);

        CGContextRef clippedSourceContext = CGBitmapContextCreate(buffer, csw, csh,
            8, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast);
        CGColorSpaceRelease(colorSpace);
        CGContextTranslateCTM(clippedSourceContext, -sx, -sy);
        CGContextDrawImage(clippedSourceContext, CGRectMake(0, 0, iw, ih), platformImage);

        CGImageRef clippedSourceImage = CGBitmapContextCreateImage(clippedSourceContext);
        CGContextRelease(clippedSourceContext);

        CGContextDrawImage(c, CGRectMake(dx, dy, dw, dh), clippedSourceImage);
        CGImageRelease(clippedSourceImage);
        
        fastFree(buffer);
    }

    CGImageRelease(platformImage);
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

    // FIXME: Do this through platform-independent GraphicsContext API.
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
#if __APPLE__
    CGImageRef image = canvas->createPlatformImage();
    if (!image)
        return 0;
    PassRefPtr<CanvasPattern> pattern = new CanvasPattern(image, repeatX, repeatY);
    CGImageRelease(image);
    return pattern;
#else
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

#if __APPLE__

CGContextRef CanvasRenderingContext2D::platformContext() const
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return 0;
    return c->platformContext();
}

void CanvasRenderingContext2D::applyStrokePattern()
{
    CGContextRef c = platformContext();
    if (!c)
        return;

    // Check for case where the pattern is already set.
    CGAffineTransform m = CGContextGetCTM(c);
    if (state().m_platformContextStrokeStyleIsPattern
            && CGAffineTransformEqualToTransform(m, state().m_strokeStylePatternTransform))
        return;

    CanvasPattern* pattern = state().m_strokeStyle->pattern();
    if (!pattern)
        return;

    CGPatternRef platformPattern = pattern->createPattern(m);
    if (!platformPattern)
        return;

    CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(0);
    CGContextSetStrokeColorSpace(c, patternSpace);
    CGColorSpaceRelease(patternSpace);

    const CGFloat patternAlpha = 1;
    CGContextSetStrokePattern(c, platformPattern, &patternAlpha);
    CGPatternRelease(platformPattern);

    state().m_platformContextStrokeStyleIsPattern = true;
    state().m_strokeStylePatternTransform = m;
}

void CanvasRenderingContext2D::applyFillPattern()
{
    CGContextRef c = platformContext();
    if (!c)
        return;

    // Check for case where the pattern is already set.
    CGAffineTransform m = CGContextGetCTM(c);
    if (state().m_platformContextFillStyleIsPattern
            && CGAffineTransformEqualToTransform(m, state().m_fillStylePatternTransform))
        return;

    CanvasPattern* pattern = state().m_fillStyle->pattern();
    if (!pattern)
        return;

    CGPatternRef platformPattern = pattern->createPattern(m);
    if (!platformPattern)
        return;

    CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(0);
    CGContextSetFillColorSpace(c, patternSpace);
    CGColorSpaceRelease(patternSpace);

    const CGFloat patternAlpha = 1;
    CGContextSetFillPattern(c, platformPattern, &patternAlpha);
    CGPatternRelease(platformPattern);

    state().m_platformContextFillStyleIsPattern = true;
    state().m_fillStylePatternTransform = m;
}

#endif

} // namespace WebCore
