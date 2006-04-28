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

#import "config.h"
#import "GraphicsContext.h"

#import "BlockExceptions.h"
#import "FloatRect.h"
#import "Font.h"
#import "FoundationExtras.h"
#import "IntPointArray.h"
#import "KRenderingDeviceQuartz.h"
#import "Path.h"
#import "WebCoreSystemInterface.h"

// FIXME: A lot more of this should use CoreGraphics instead of AppKit.
// FIXME: A lot more of this should move into GraphicsContextCG.cpp.

using namespace std;

namespace WebCore {

// NSColor, NSBezierPath, and NSGraphicsContext
// calls in this file are all exception-safe, so we don't block
// exceptions for those.

class GraphicsContextPlatformPrivate {
public:
    GraphicsContextPlatformPrivate(CGContextRef);
    ~GraphicsContextPlatformPrivate();

    CGContextRef m_cgContext;
    IntRect m_focusRingClip; // Work around CG bug in focus ring clipping.
};

// A fillRect helper to work around the fact that NSRectFill uses copy mode, not source over.
static inline void fillRectSourceOver(const FloatRect& rect, const Color& col)
{
    [nsColor(col) set];
    NSRectFillUsingOperation(rect, NSCompositeSourceOver);
}

GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(CGContextRef cgContext)
{
    m_cgContext = cgContext;
    CGContextRetain(m_cgContext);
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
    CGContextRelease(m_cgContext);
}

GraphicsContext::GraphicsContext(CGContextRef cgContext)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate(cgContext))
{
    setPaintingDisabled(!cgContext);
}

GraphicsContext::~GraphicsContext()
{
    destroyGraphicsContextPrivate(m_common);
    delete m_data;
}

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

    // FIXME: This function currently works only when the current context is the current NSGraphicsContext.
    ASSERT(platformContext() == [[NSGraphicsContext currentContext] graphicsPort]);

    if (fillColor() & 0xFF000000)
        fillRectSourceOver(rect, fillColor());

    if (pen().style() != Pen::Pen::NoPen) {
        setColorFromPen();
        NSFrameRect(rect);
    }
}

void GraphicsContext::setColorFromFillColor()
{
    // FIXME: This function currently works only when the current context is the current NSGraphicsContext.
    ASSERT(platformContext() == [[NSGraphicsContext currentContext] graphicsPort]);

    [nsColor(fillColor()) set];
}
  
void GraphicsContext::setColorFromPen()
{
    // FIXME: This function currently works only when the current context is the current NSGraphicsContext.
    ASSERT(platformContext() == [[NSGraphicsContext currentContext] graphicsPort]);

    [nsColor(pen().color()) set];
}
  
// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    // FIXME: This function currently works only when the current context is the current NSGraphicsContext.
    ASSERT(platformContext() == [[NSGraphicsContext currentContext] graphicsPort]);

    Pen::PenStyle penStyle = pen().style();
    if (penStyle == Pen::Pen::NoPen)
        return;
    float width = pen().width();
    if (width < 1)
        width = 1;

    NSPoint p1 = point1;
    NSPoint p2 = point2;
    bool isVerticalLine = (p1.x == p2.x);
    
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == Pen::DotLine || penStyle == Pen::DashLine) {
        if (isVerticalLine) {
            p1.y += width;
            p2.y -= width;
        } else {
            p1.x += width;
            p2.x -= width;
        }
    }
    
    if (((int)width)%2) {
        if (isVerticalLine) {
            // We're a vertical line.  Adjust our x.
            p1.x += 0.5;
            p2.x += 0.5;
        } else {
            // We're a horizontal line. Adjust our y.
            p1.y += 0.5;
            p2.y += 0.5;
        }
    }
    
    NSBezierPath *path = [[NSBezierPath alloc] init];
    [path setLineWidth:width];

    int patWidth = 0;
    switch (penStyle) {
    case Pen::NoPen:
    case Pen::SolidLine:
        break;
    case Pen::DotLine:
        patWidth = (int)width;
        break;
    case Pen::DashLine:
        patWidth = 3*(int)width;
        break;
    }

    CGContextSaveGState(platformContext());

    setColorFromPen();

    CGContextSetShouldAntialias(platformContext(), false);
    
    if (patWidth) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        const Color& penColor = pen().color();
        if (isVerticalLine) {
            fillRectSourceOver(FloatRect(p1.x-width/2, p1.y-width, width, width), penColor);
            fillRectSourceOver(FloatRect(p2.x-width/2, p2.y, width, width), penColor);
        } else {
            fillRectSourceOver(FloatRect(p1.x-width, p1.y-width/2, width, width), penColor);
            fillRectSourceOver(FloatRect(p2.x, p2.y-width/2, width, width), penColor);
        }
        
        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance = (isVerticalLine ? (point2.y() - point1.y()) : (point2.x() - point1.x())) - 2*(int)width;
        int remainder = distance%patWidth;
        int coverage = distance-remainder;
        int numSegments = coverage/patWidth;

        float patternOffset = 0;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0;
        else {
            bool evenNumberOfSegments = numSegments%2 == 0;
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder/2;
                }
                else
                    patternOffset = patWidth/2;
            }
            else if (!evenNumberOfSegments) {
                if (remainder)
                    patternOffset = (patWidth - remainder)/2;
            }
        }
        
        const CGFloat dottedLine[2] = { patWidth, patWidth };
        [path setLineDash:dottedLine count:2 phase:patternOffset];
    }
    
    [path moveToPoint:p1];
    [path lineToPoint:p2];

    [path stroke];
    
    [path release];

    CGContextRestoreGState(platformContext());
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

    if (fillColor() & 0xFF000000) {
        setColorFromFillColor();
        if (pen().style() != Pen::NoPen) {
            // stroke and fill
            setColorFromPen();
            unsigned penWidth = pen().width();
            if (penWidth == 0) 
                penWidth++;
            CGContextSetLineWidth(context, penWidth);
            CGContextDrawPath(context, kCGPathFillStroke);
        } else
            CGContextFillPath(context);
    }
    if (pen().style() != Pen::NoPen) {
        setColorFromPen();
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
        
        setColorFromPen();
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

    if (fillColor() & 0xFF000000) {
        setColorFromFillColor();
        CGContextEOFillPath(context);
    }

    if (pen().style() != Pen::NoPen) {
        setColorFromPen();
        CGContextSetLineWidth(context, pen().width());
        CGContextStrokePath(context);
    }

    CGContextRestoreGState(context);
}

static int getBlendedColorComponent(int c, int a)
{
    // We use white.
    float alpha = (float)(a) / 255;
    int whiteBlend = 255 - a;
    c -= whiteBlend;
    return (int)(c/alpha);
}

Color GraphicsContext::selectedTextBackgroundColor() const
{
    NSColor *color = usesInactiveTextBackgroundColor() ? [NSColor secondarySelectedControlColor] : [NSColor selectedTextBackgroundColor];
    // this needs to always use device colorspace so it can de-calibrate the color for
    // Color to possibly recalibrate it
    color = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    
    Color col = Color((int)(255 * [color redComponent]), (int)(255 * [color greenComponent]), (int)(255 * [color blueComponent]));
    
    // Attempt to make the selection 60% transparent.  We do this by applying a standard blend and then
    // seeing if the resultant color is still within the 0-255 range.
    int alpha = 153;
    int red = getBlendedColorComponent(col.red(), alpha);
    int green = getBlendedColorComponent(col.green(), alpha);
    int blue = getBlendedColorComponent(col.blue(), alpha);
    if (red >= 0 && red <= 255 && green >= 0 && green <= 255 && blue >= 0 && blue <= 255)
        return Color(red, green, blue, alpha);
    return col;
}

void GraphicsContext::fillRect(const IntRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    // FIXME: This function currently works only when the current context is the current NSGraphicsContext.
    ASSERT(platformContext() == [[NSGraphicsContext currentContext] graphicsPort]);

    if (color.alpha())
        fillRectSourceOver(rect, color.rgb());
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

void GraphicsContext::setFocusRingClip(const IntRect& r)
{
    // This method only exists to work around bugs in Mac focus ring clipping.
    m_data->m_focusRingClip = r;
}

void GraphicsContext::clearFocusRingClip()
{
    // This method only exists to work around bugs in Mac focus ring clipping.
    m_data->m_focusRingClip = IntRect();
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
    CGContextRef context = platformContext();
    CGContextSetShadowWithColor(context, CGSizeZero, 0, NULL);
}

void GraphicsContext::drawFocusRing(const Color& color)
{
    if (paintingDisabled())
        return;

    int radius = (focusRingWidth() - 1) / 2;
    int offset = radius + focusRingOffset();
    CGColorRef colorRef = color.isValid() ? cgColor(color) : 0;

    CGMutablePathRef focusRingPath = CGPathCreateMutable();
    const Vector<IntRect>& rects = focusRingRects();
    unsigned rectCount = rects.size();
    for (unsigned i = 0; i < rectCount; i++)
        CGPathAddRect(focusRingPath, 0, CGRectInset(rects[i], -offset, -offset));

    CGContextRef context = platformContext();

    // FIXME: This works only inside a NSView's drawRect method. The view must be
    // focused and this context must be the current NSGraphicsContext.
    ASSERT(context == [[NSGraphicsContext currentContext] graphicsPort]);
    NSView* view = [NSView focusView];
    ASSERT(view);

    const NSRect* drawRects;
    int count;
    [view getRectsBeingDrawn:&drawRects count:&count];

    // We have to pass in our own clip rectangles here because a bug in CG
    // seems to inflate the clip (thus allowing the focus ring to paint
    // slightly outside the clip).
    NSRect transformedClipRect = [view convertRect:m_data->m_focusRingClip toView:nil];
    for (int i = 0; i < count; ++i) {
        NSRect transformedRect = [view convertRect:drawRects[i] toView:nil];
        NSRect rectToUse = NSIntersectionRect(transformedRect, transformedClipRect);
        if (!NSIsEmptyRect(rectToUse)) {
            CGContextBeginPath(context);
            CGContextAddPath(context, focusRingPath);
            wkDrawFocusRing(context, *(CGRect *)&rectToUse, colorRef, radius);
        }
    }

    CGColorRelease(colorRef);

    CGPathRelease(focusRingPath);
}

CGContextRef GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    ASSERT(m_data->m_cgContext);
    return m_data->m_cgContext;
}

void GraphicsContext::setLineWidth(float width)
{
    CGContextSetLineWidth(platformContext(), width);
}

void GraphicsContext::setMiterLimit(float limit)
{
    CGContextSetMiterLimit(platformContext(), limit);
}

void GraphicsContext::setAlpha(float alpha)
{
    CGContextSetAlpha(platformContext(), alpha);
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    [[NSGraphicsContext graphicsContextWithGraphicsPort:platformContext() flipped:YES]
        setCompositingOperation:(NSCompositingOperation)op];
    [pool release];
}

void GraphicsContext::clearRect(const FloatRect& r)
{
    CGContextClearRect(platformContext(), r);
}

void GraphicsContext::strokeRect(const FloatRect& r, float lineWidth)
{
    CGContextStrokeRectWithWidth(platformContext(), r, lineWidth);
}

void GraphicsContext::setLineCap(LineCap cap)
{
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
    CGContextBeginPath(platformContext());
    CGContextAddPath(platformContext(), path.platformPath());
    CGContextClip(platformContext());
}

}
