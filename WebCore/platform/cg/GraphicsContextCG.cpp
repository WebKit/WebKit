/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "IntPointArray.h"
#include "KRenderingDeviceQuartz.h"
#include "Path.h"

using namespace std;

namespace WebCore {

// NSColor, NSBezierPath, and NSGraphicsContext
// calls in this file are all exception-safe, so we don't block
// exceptions for those.

void GraphicsContext::savePlatformState()
{
    CGContextSaveGState(platformContext());
}

void GraphicsContext::restorePlatformState()
{
    CGContextRestoreGState(platformContext());
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    CGContextRef context = platformContext();

    if (fillColor().alpha()) {
        CGContextSetFillColorWithColor(context, cgColor(fillColor()));
        CGContextFillRect(context, rect);
    }

    if (pen().style() != Pen::Pen::NoPen) {
        CGContextSetFillColorWithColor(context, cgColor(pen().color()));
        CGRect rects[4] = {
            FloatRect(rect.x(), rect.y(), rect.width(), 1),
            FloatRect(rect.x(), rect.bottom() - 1, rect.width(), 1),
            FloatRect(rect.x(), rect.y() + 1, 1, rect.height() - 2),
            FloatRect(rect.right() - 1, rect.y() + 1, 1, rect.height() - 2)
        };
        CGContextFillRects(context, rects, 4);
    }
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    Pen::PenStyle penStyle = pen().style();
    if (penStyle == Pen::Pen::NoPen)
        return;
    float width = pen().width();
    if (width < 1)
        width = 1;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());
    
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == Pen::DotLine || penStyle == Pen::DashLine) {
        if (isVerticalLine) {
            p1.move(0, width);
            p2.move(0, -width);
        } else {
            p1.move(width, 0);
            p2.move(-width, 0);
        }
    }
    
    if (((int)width) % 2) {
        if (isVerticalLine) {
            // We're a vertical line.  Adjust our x.
            p1.move(0.5, 0);
            p2.move(0.5, 0);
        } else {
            // We're a horizontal line. Adjust our y.
            p1.move(0, 0.5);
            p2.move(0, 0.5);
        }
    }
    
    int patWidth = 0;
    switch (penStyle) {
        case Pen::NoPen:
        case Pen::SolidLine:
            break;
        case Pen::DotLine:
            patWidth = (int)width;
            break;
        case Pen::DashLine:
            patWidth = 3 * (int)width;
            break;
    }

    CGContextRef context = platformContext();

    CGContextSaveGState(context);

    CGColorRef colorRef = cgColor(pen().color());
    CGContextSetFillColorWithColor(context, colorRef);
    CGContextSetStrokeColorWithColor(context, colorRef);

    CGContextSetShouldAntialias(context, false);
    
    if (patWidth) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        if (isVerticalLine) {
            CGContextFillRect(context, FloatRect(p1.x() - width / 2, p1.y() - width, width, width));
            CGContextFillRect(context, FloatRect(p2.x() - width / 2, p2.y(), width, width));
        } else {
            CGContextFillRect(context, FloatRect(p1.x() - width, p1.y() - width / 2, width, width));
            CGContextFillRect(context, FloatRect(p2.x(), p2.y() - width / 2, width, width));
        }

        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance = (isVerticalLine ? (point2.y() - point1.y()) : (point2.x() - point1.x())) - 2*(int)width;
        int remainder = distance % patWidth;
        int coverage = distance - remainder;
        int numSegments = coverage / patWidth;

        float patternOffset = 0;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0;
        else {
            bool evenNumberOfSegments = numSegments % 2 == 0;
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder / 2;
                } else
                    patternOffset = patWidth / 2;
            } else {
                if (remainder)
                    patternOffset = (patWidth - remainder)/2;
            }
        }
        
        const CGFloat dottedLine[2] = { patWidth, patWidth };
        CGContextSetLineDash(context, patternOffset, dottedLine, 2);
    }
    
    CGContextSetLineWidth(context, width);

    CGContextBeginPath(context);
    CGContextMoveToPoint(context, p1.x(), p1.y());
    CGContextAddLineToPoint(context, p2.x(), p2.y());

    CGContextStrokePath(context);

    CGContextRestoreGState(context);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    // FIXME: CG added CGContextAddEllipseinRect in Tiger, so we should be able to quite easily draw an ellipse.
    // This code can only handle circles, not ellipses. But khtml only
    // uses it for circles.
    ASSERT(rect.width() == rect.height());

    if (paintingDisabled())
        return;
        
    CGContextRef context = platformContext();
    CGContextBeginPath(context);
    float r = (float)rect.width() / 2;
    CGContextAddArc(context, rect.x() + r, rect.y() + r, r, 0, 2*M_PI, true);
    CGContextClosePath(context);

    if (fillColor().alpha()) {
        CGContextSetFillColorWithColor(context, cgColor(fillColor()));
        if (pen().style() != Pen::NoPen) {
            // stroke and fill
            CGContextSetStrokeColorWithColor(context, cgColor(pen().color()));
            unsigned penWidth = pen().width();
            if (penWidth == 0) 
                penWidth++;
            CGContextSetLineWidth(context, penWidth);
            CGContextDrawPath(context, kCGPathFillStroke);
        } else
            CGContextFillPath(context);
    } else if (pen().style() != Pen::NoPen) {
        CGContextSetStrokeColorWithColor(context, cgColor(pen().color()));
        unsigned penWidth = pen().width();
        if (penWidth == 0) 
            penWidth++;
        CGContextSetLineWidth(context, penWidth);
        CGContextStrokePath(context);
    }
}


void GraphicsContext::drawArc(int x, int y, int w, int h, int a, int alen)
{ 
    // Only supports arc on circles.  That's all khtml needs.
    ASSERT(w == h);

    if (paintingDisabled())
        return;
    
    if (pen().style() != Pen::NoPen) {
        CGContextRef context = platformContext();
        CGContextBeginPath(context);
        
        float r = (float)w / 2;
        float fa = (float)a / 16;
        float falen =  fa + (float)alen / 16;
        CGContextAddArc(context, x + r, y + r, r, -fa * M_PI/180, -falen * M_PI/180, true);
        
        CGContextSetStrokeColorWithColor(context, cgColor(pen().color()));
        CGContextSetLineWidth(context, pen().width());
        CGContextStrokePath(context);
    }
}

void GraphicsContext::drawConvexPolygon(const IntPointArray& points)
{
    if (paintingDisabled())
        return;

    int npoints = points.size();
    if (npoints <= 1)
        return;

    CGContextRef context = platformContext();

    CGContextSaveGState(context);

    CGContextSetShouldAntialias(context, false);
    
    CGContextBeginPath(context);
    CGContextMoveToPoint(context, points[0].x(), points[0].y());
    for (int i = 1; i < npoints; i++)
        CGContextAddLineToPoint(context, points[i].x(), points[i].y());
    CGContextClosePath(context);

    if (fillColor().alpha()) {
        CGContextSetFillColorWithColor(context, cgColor(fillColor()));
        CGContextEOFillPath(context);
    }

    if (pen().style() != Pen::NoPen) {
        CGContextSetStrokeColorWithColor(context, cgColor(pen().color()));
        CGContextSetLineWidth(context, pen().width());
        CGContextStrokePath(context);
    }

    CGContextRestoreGState(context);
}

void GraphicsContext::fillRect(const IntRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;
    if (color.alpha()) {
        CGContextRef context = platformContext();
        CGContextSetFillColorWithColor(context, cgColor(color));
        CGContextFillRect(context, rect);
    }
}

void GraphicsContext::addClip(const IntRect& rect)
{
    if (paintingDisabled())
        return;
    CGContextClipToRect(platformContext(), rect);
}

void GraphicsContext::addRoundedRectClip(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
    const IntSize& bottomLeft, const IntSize& bottomRight)
{
    if (paintingDisabled())
        return;

    // Need sufficient width and height to contain these curves.  Sanity check our top/bottom
    // values and our width/height values to make sure the curves can all fit.
    int requiredWidth = max(topLeft.width() + topRight.width(), bottomLeft.width() + bottomRight.width());
    if (requiredWidth > rect.width())
        return;
    int requiredHeight = max(topLeft.height() + bottomLeft.height(), topRight.height() + bottomRight.height());
    if (requiredHeight > rect.height())
        return;
 
    // Clip to our rect.
    addClip(rect);

    // OK, the curves can fit.
    CGContextRef context = platformContext();
    
    // Add the four ellipses to the path.  Technically this really isn't good enough, since we could end up
    // not clipping the other 3/4 of the ellipse we don't care about.  We're relying on the fact that for
    // normal use cases these ellipses won't overlap one another (or when they do the curvature of one will
    // be subsumed by the other).
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.y(), topLeft.width() * 2, topLeft.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.right() - topRight.width() * 2, rect.y(),
                                                  topRight.width() * 2, topRight.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.bottom() - bottomLeft.height() * 2,
                                                  bottomLeft.width() * 2, bottomLeft.height() * 2));
    CGContextAddEllipseInRect(context, CGRectMake(rect.right() - bottomRight.width() * 2,
                                                  rect.bottom() - bottomRight.height() * 2,
                                                  bottomRight.width() * 2, bottomRight.height() * 2));
    
    // Now add five rects (one for each edge rect in between the rounded corners and one for the interior).
    CGContextAddRect(context, CGRectMake(rect.x() + topLeft.width(), rect.y(),
                                         rect.width() - topLeft.width() - topRight.width(),
                                         max(topLeft.height(), topRight.height())));
    CGContextAddRect(context, CGRectMake(rect.x() + bottomLeft.width(), 
                                         rect.bottom() - max(bottomLeft.height(), bottomRight.height()),
                                         rect.width() - bottomLeft.width() - bottomRight.width(),
                                         max(bottomLeft.height(), bottomRight.height())));
    CGContextAddRect(context, CGRectMake(rect.x(), rect.y() + topLeft.height(),
                                         max(topLeft.width(), bottomLeft.width()), rect.height() - topLeft.height() - bottomLeft.height()));
    CGContextAddRect(context, CGRectMake(rect.right() - max(topRight.width(), bottomRight.width()),
                                         rect.y() + topRight.height(),
                                         max(topRight.width(), bottomRight.width()), rect.height() - topRight.height() - bottomRight.height()));
    CGContextAddRect(context, CGRectMake(rect.x() + max(topLeft.width(), bottomLeft.width()),
                                         rect.y() + max(topLeft.height(), topRight.height()),
                                         rect.width() - max(topLeft.width(), bottomLeft.width()) - max(topRight.width(), bottomRight.width()),
                                         rect.height() - max(topLeft.height(), topRight.height()) - max(bottomLeft.height(), bottomRight.height())));
    CGContextClip(context);
}

#if SVG_SUPPORT
KRenderingDeviceContext* GraphicsContext::createRenderingDeviceContext()
{
    return new KRenderingDeviceContextQuartz(platformContext());
}
#endif

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    CGContextSaveGState(context);
    CGContextSetAlpha(context, opacity);
    CGContextBeginTransparencyLayer(context, 0);
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    CGContextEndTransparencyLayer(context);
    CGContextRestoreGState(context);
}

void GraphicsContext::setShadow(const IntSize& size, int blur, const Color& color)
{
    if (paintingDisabled())
        return;
    // Check for an invalid color, as this means that the color was not set for the shadow
    // and we should therefore just use the default shadow color.
    CGContextRef context = platformContext();
    if (!color.isValid())
        CGContextSetShadow(context, CGSizeMake(size.width(), -size.height()), blur); // y is flipped.
    else {
        CGColorRef colorCG = cgColor(color);
        CGContextSetShadowWithColor(context,
                                    CGSizeMake(size.width(), -size.height()), // y is flipped.
                                    blur, 
                                    colorCG);
        CGColorRelease(colorCG);
    }
}

void GraphicsContext::clearShadow()
{
    if (paintingDisabled())
        return;
    CGContextSetShadowWithColor(platformContext(), CGSizeZero, 0, NULL);
}

void GraphicsContext::setLineWidth(float width)
{
    if (paintingDisabled())
        return;
    CGContextSetLineWidth(platformContext(), width);
}

void GraphicsContext::setMiterLimit(float limit)
{
    if (paintingDisabled())
        return;
    CGContextSetMiterLimit(platformContext(), limit);
}

void GraphicsContext::setAlpha(float alpha)
{
    if (paintingDisabled())
        return;
    CGContextSetAlpha(platformContext(), alpha);
}

void GraphicsContext::clearRect(const FloatRect& r)
{
    if (paintingDisabled())
        return;
    CGContextClearRect(platformContext(), r);
}

void GraphicsContext::strokeRect(const FloatRect& r, float lineWidth)
{
    if (paintingDisabled())
        return;
    CGContextStrokeRectWithWidth(platformContext(), r, lineWidth);
}

void GraphicsContext::setLineCap(LineCap cap)
{
    if (paintingDisabled())
        return;
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
}

void GraphicsContext::setLineJoin(LineJoin join)
{
    if (paintingDisabled())
        return;
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
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    CGContextBeginPath(context);
    CGContextAddPath(context, path.platformPath());
    CGContextClip(context);
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;
    CGContextScaleCTM(platformContext(), size.width(), size.height());
}

void GraphicsContext::rotate(float angle)
{
    if (paintingDisabled())
        return;
    CGContextRotateCTM(platformContext(), angle);
}

void GraphicsContext::translate(const FloatSize& size)
{
    if (paintingDisabled())
        return;
    CGContextTranslateCTM(platformContext(), size.width(), size.height());
}

}
