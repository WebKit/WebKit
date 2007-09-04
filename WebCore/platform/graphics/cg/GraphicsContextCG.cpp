/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#define _USE_MATH_DEFINES 1
#include "config.h"
#include "GraphicsContext.h"

#if PLATFORM(CG)

#include "AffineTransform.h"
#include "KURL.h"
#include "Path.h"
#include <wtf/MathExtras.h>

#include <GraphicsContextPlatformPrivate.h> // FIXME: Temporary.

using namespace std;

namespace WebCore {

static void setCGFillColor(CGContextRef context, const Color& color)
{
    CGFloat red, green, blue, alpha;
    color.getRGBA(red, green, blue, alpha);
    CGContextSetRGBFillColor(context, red, green, blue, alpha);
}

static void setCGStrokeColor(CGContextRef context, const Color& color)
{
    CGFloat red, green, blue, alpha;
    color.getRGBA(red, green, blue, alpha);
    CGContextSetRGBStrokeColor(context, red, green, blue, alpha);
}

GraphicsContext::GraphicsContext(CGContextRef cgContext)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate(cgContext))
{
    setPaintingDisabled(!cgContext);
    if (cgContext) {
        // Make sure the context starts in sync with our state.
        setPlatformFillColor(fillColor());
        setPlatformStrokeColor(strokeColor());
    }
}

GraphicsContext::~GraphicsContext()
{
    destroyGraphicsContextPrivate(m_common);
    delete m_data;
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

CGContextRef GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    ASSERT(m_data->m_cgContext);
    return m_data->m_cgContext;
}

void GraphicsContext::savePlatformState()
{
    // Note: Do not use this function within this class implementation, since we want to avoid the extra
    // save of the secondary context (in GraphicsContextPlatformPrivate.h).
    CGContextSaveGState(platformContext());
    m_data->save();
}

void GraphicsContext::restorePlatformState()
{
    // Note: Do not use this function within this class implementation, since we want to avoid the extra
    // restore of the secondary context (in GraphicsContextPlatformPrivate.h).
    CGContextRestoreGState(platformContext());
    m_data->restore();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    CGContextRef context = platformContext();

    if (fillColor().alpha())
        CGContextFillRect(context, rect);

    if (strokeStyle() != NoStroke && strokeColor().alpha()) {
        // We do a fill of four rects to simulate the stroke of a border.
        Color oldFillColor = fillColor();
        if (oldFillColor != strokeColor())
            setCGFillColor(context, strokeColor());
        CGRect rects[4] = {
            FloatRect(rect.x(), rect.y(), rect.width(), 1),
            FloatRect(rect.x(), rect.bottom() - 1, rect.width(), 1),
            FloatRect(rect.x(), rect.y() + 1, 1, rect.height() - 2),
            FloatRect(rect.right() - 1, rect.y() + 1, 1, rect.height() - 2)
        };
        CGContextFillRects(context, rects, 4);
        if (oldFillColor != strokeColor())
            setCGFillColor(context, oldFillColor);
    }
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    if (strokeStyle() == NoStroke || !strokeColor().alpha())
        return;

    float width = strokeThickness();

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());
    
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (strokeStyle() == DottedStroke || strokeStyle() == DashedStroke) {
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
            p1.move(0.5f, 0.0f);
            p2.move(0.5f, 0.0f);
        } else {
            // We're a horizontal line. Adjust our y.
            p1.move(0.0f, 0.5f);
            p2.move(0.0f, 0.5f);
        }
    }
    
    int patWidth = 0;
    switch (strokeStyle()) {
        case NoStroke:
        case SolidStroke:
            break;
        case DottedStroke:
            patWidth = (int)width;
            break;
        case DashedStroke:
            patWidth = 3 * (int)width;
            break;
    }

    CGContextRef context = platformContext();
    CGContextSaveGState(context);

    CGContextSetShouldAntialias(context, false);

    if (patWidth) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        setCGFillColor(context, strokeColor());  // The save/restore make it safe to mutate the fill color here without setting it back to the old color.
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

        float patternOffset = 0.0f;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0f;
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
    CGContextAddArc(context, rect.x() + r, rect.y() + r, r, 0.0f, 2.0f * piFloat, 0);
    CGContextClosePath(context);

    if (fillColor().alpha()) {
        if (strokeStyle() != NoStroke)
            // stroke and fill
            CGContextDrawPath(context, kCGPathFillStroke);
        else
            CGContextFillPath(context);
    } else if (strokeStyle() != NoStroke)
        CGContextStrokePath(context);
}


void GraphicsContext::strokeArc(const IntRect& rect, int startAngle, int angleSpan)
{ 
    if (paintingDisabled() || strokeStyle() == NoStroke || strokeThickness() <= 0.0f || !strokeColor().alpha())
        return;
    
    CGContextRef context = platformContext();
    CGContextSaveGState(context);
    CGContextBeginPath(context);
    CGContextSetShouldAntialias(context, false);
    
    int x = rect.x();
    int y = rect.y();
    float w = (float)rect.width();
    float h = (float)rect.height();
    float scaleFactor = h / w;
    float reverseScaleFactor = w / h;
    
    if (w != h)
        scale(FloatSize(1, scaleFactor));
    
    float hRadius = w / 2;
    float vRadius = h / 2;
    float fa = startAngle;
    float falen =  fa + angleSpan;
    float start = -fa * piFloat / 180.0f;
    float end = -falen * piFloat / 180.0f;
    CGContextAddArc(context, x + hRadius, (y + vRadius) * reverseScaleFactor, hRadius, start, end, true);

    if (w != h)
        scale(FloatSize(1, reverseScaleFactor));
    
    
    float width = strokeThickness();
    int patWidth = 0;
    
    switch (strokeStyle()) {
        case DottedStroke:
            patWidth = (int)(width / 2);
            break;
        case DashedStroke:
            patWidth = 3 * (int)(width / 2);
            break;
        default:
            break;
    }

    CGContextSaveGState(context);
    
    if (patWidth) {
        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance;
        if (hRadius == vRadius)
            distance = static_cast<int>((piFloat * hRadius) / 2.0f);
        else // We are elliptical and will have to estimate the distance
            distance = static_cast<int>((piFloat * sqrtf((hRadius * hRadius + vRadius * vRadius) / 2.0f)) / 2.0f);
        
        int remainder = distance % patWidth;
        int coverage = distance - remainder;
        int numSegments = coverage / patWidth;

        float patternOffset = 0.0f;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0f;
        else {
            bool evenNumberOfSegments = numSegments % 2 == 0;
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder / 2.0f;
                } else
                    patternOffset = patWidth / 2.0f;
            } else {
                if (remainder)
                    patternOffset = (patWidth - remainder) / 2.0f;
            }
        }
    
        const CGFloat dottedLine[2] = { patWidth, patWidth };
        CGContextSetLineDash(context, patternOffset, dottedLine, 2);
    }

    CGContextStrokePath(context);
    CGContextRestoreGState(context);
    
    CGContextRestoreGState(context);
}

void GraphicsContext::drawConvexPolygon(size_t npoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled() || !fillColor().alpha() && (strokeThickness() <= 0 || strokeStyle() == NoStroke))
        return;

    if (npoints <= 1)
        return;

    CGContextRef context = platformContext();

    CGContextSaveGState(context);

    CGContextSetShouldAntialias(context, shouldAntialias);
    
    CGContextBeginPath(context);
    CGContextMoveToPoint(context, points[0].x(), points[0].y());
    for (size_t i = 1; i < npoints; i++)
        CGContextAddLineToPoint(context, points[i].x(), points[i].y());
    CGContextClosePath(context);

    if (fillColor().alpha()) {
        if (strokeStyle() != NoStroke)
            CGContextDrawPath(context, kCGPathEOFillStroke);
        else
            CGContextEOFillPath(context);
    } else
        CGContextStrokePath(context);

    CGContextRestoreGState(context);
}

void GraphicsContext::fillRect(const IntRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;
    if (color.alpha()) {
        CGContextRef context = platformContext();
        Color oldFillColor = fillColor();
        if (oldFillColor != color)
            setCGFillColor(context, color);
        CGContextFillRect(context, rect);
        if (oldFillColor != color)
            setCGFillColor(context, oldFillColor);
    }
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;
    if (color.alpha()) {
        CGContextRef context = platformContext();
        Color oldFillColor = fillColor();
        if (oldFillColor != color)
            setCGFillColor(context, color);
        CGContextFillRect(context, rect);
        if (oldFillColor != color)
            setCGFillColor(context, oldFillColor);
    }
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color)
{
    if (paintingDisabled() || !color.alpha())
        return;

    CGContextRef context = platformContext();
    Color oldFillColor = fillColor();
    if (oldFillColor != color)
        setCGFillColor(context, color);

    addPath(Path::createRoundedRectangle(rect, topLeft, topRight, bottomLeft, bottomRight));
    CGContextFillPath(context);

    if (oldFillColor != color)
        setCGFillColor(context, oldFillColor);
}


void GraphicsContext::clip(const IntRect& rect)
{
    if (paintingDisabled())
        return;
    CGContextClipToRect(platformContext(), rect);
    m_data->clip(rect);
}

void GraphicsContext::clipOut(const IntRect& rect)
{
    if (paintingDisabled())
        return;
        
    CGRect rects[2] = { CGContextGetClipBoundingBox(platformContext()), rect };
    CGContextBeginPath(platformContext());
    CGContextAddRects(platformContext(), rects, 2);
    CGContextEOClip(platformContext());
}

void GraphicsContext::clipOutEllipseInRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;
        
    CGContextBeginPath(platformContext());
    CGContextAddRect(platformContext(), CGContextGetClipBoundingBox(platformContext()));
    CGContextAddEllipseInRect(platformContext(), rect);
    CGContextEOClip(platformContext());
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    if (paintingDisabled())
        return;

    clip(rect);
    CGContextRef context = platformContext();
    
    // Add outer ellipse
    CGContextAddEllipseInRect(context, CGRectMake(rect.x(), rect.y(), rect.width(), rect.height()));
    // Add inner ellipse.
    CGContextAddEllipseInRect(context, CGRectMake(rect.x() + thickness, rect.y() + thickness,
        rect.width() - (thickness * 2), rect.height() - (thickness * 2)));
    
    CGContextEOClip(context);
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    CGContextSaveGState(context);
    CGContextSetAlpha(context, opacity);
    CGContextBeginTransparencyLayer(context, 0);
    m_data->beginTransparencyLayer();
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    CGContextEndTransparencyLayer(context);
    CGContextRestoreGState(context);
    m_data->endTransparencyLayer();
}

void GraphicsContext::setShadow(const IntSize& size, int blur, const Color& color)
{
    // Extreme "blur" values can make text drawing crash or take crazy long times, so clamp
    blur = min(blur, 1000);

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
    CGContextSetShadowWithColor(platformContext(), CGSizeZero, 0, 0);
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
 
void GraphicsContext::beginPath()
{
    CGContextBeginPath(platformContext());
}

void GraphicsContext::addPath(const Path& path)
{
    CGContextAddPath(platformContext(), path.platformPath());
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;
    CGContextRef context = platformContext();
    CGContextBeginPath(context);
    CGContextAddPath(context, path.platformPath());
    CGContextClip(context);
    m_data->clip(path);
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;
        
    CGContextBeginPath(platformContext());
    CGContextAddRect(platformContext(), CGContextGetClipBoundingBox(platformContext()));
    CGContextAddPath(platformContext(), path.platformPath());
    CGContextEOClip(platformContext());
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;
    CGContextScaleCTM(platformContext(), size.width(), size.height());
    m_data->scale(size);
}

void GraphicsContext::rotate(float angle)
{
    if (paintingDisabled())
        return;
    CGContextRotateCTM(platformContext(), angle);
    m_data->rotate(angle);
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;
    CGContextTranslateCTM(platformContext(), x, y);
    m_data->translate(x, y);
}

void GraphicsContext::concatCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;
    CGContextConcatCTM(platformContext(), transform);
    m_data->concatCTM(transform);
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect)
{
    // It is not enough just to round to pixels in device space. The rotation part of the 
    // affine transform matrix to device space can mess with this conversion if we have a
    // rotating image like the hands of the world clock widget. We just need the scale, so 
    // we get the affine transform matrix and extract the scale.
    CGAffineTransform deviceMatrix = CGContextGetUserSpaceToDeviceSpaceTransform(platformContext());
    if (CGAffineTransformIsIdentity(deviceMatrix))
        return rect;

    float deviceScaleX = sqrtf(deviceMatrix.a * deviceMatrix.a + deviceMatrix.b * deviceMatrix.b);
    float deviceScaleY = sqrtf(deviceMatrix.c * deviceMatrix.c + deviceMatrix.d * deviceMatrix.d);

    CGPoint deviceOrigin = CGPointMake(rect.x() * deviceScaleX, rect.y() * deviceScaleY);
    CGPoint deviceLowerRight = CGPointMake((rect.x() + rect.width()) * deviceScaleX,
        (rect.y() + rect.height()) * deviceScaleY);

    deviceOrigin.x = roundf(deviceOrigin.x);
    deviceOrigin.y = roundf(deviceOrigin.y);
    deviceLowerRight.x = roundf(deviceLowerRight.x);
    deviceLowerRight.y = roundf(deviceLowerRight.y);
    
    // Don't let the height or width round to 0 unless either was originally 0
    if (deviceOrigin.y == deviceLowerRight.y && rect.height() != 0)
        deviceLowerRight.y += 1;
    if (deviceOrigin.x == deviceLowerRight.x && rect.width() != 0)
        deviceLowerRight.x += 1;

    FloatPoint roundedOrigin = FloatPoint(deviceOrigin.x / deviceScaleX, deviceOrigin.y / deviceScaleY);
    FloatPoint roundedLowerRight = FloatPoint(deviceLowerRight.x / deviceScaleX, deviceLowerRight.y / deviceScaleY);
    return FloatRect(roundedOrigin, roundedLowerRight - roundedOrigin);
}

void GraphicsContext::drawLineForText(const IntPoint& point, int width, bool printing)
{
    if (paintingDisabled())
        return;

    if (width <= 0)
        return;

    CGContextSaveGState(platformContext());

    float x = point.x();
    float y = point.y();
    float lineLength = width;

    // Use a minimum thickness of 0.5 in user space.
    // See http://bugs.webkit.org/show_bug.cgi?id=4255 for details of why 0.5 is the right minimum thickness to use.
    float thickness = max(strokeThickness(), 0.5f);

    if (!printing) {
        // On screen, use a minimum thickness of 1.0 in user space (later rounded to an integral number in device space).
        float adjustedThickness = max(thickness, 1.0f);

        // FIXME: This should be done a better way.
        // We try to round all parameters to integer boundaries in device space. If rounding pixels in device space
        // makes our thickness more than double, then there must be a shrinking-scale factor and rounding to pixels
        // in device space will make the underlines too thick.
        CGRect lineRect = roundToDevicePixels(FloatRect(x, y, lineLength, adjustedThickness));
        if (lineRect.size.height < thickness * 2.0) {
            x = lineRect.origin.x;
            y = lineRect.origin.y;
            lineLength = lineRect.size.width;
            thickness = lineRect.size.height;
            CGContextSetShouldAntialias(platformContext(), false);
        }
    }
    
    if (fillColor() != strokeColor())
        setCGFillColor(platformContext(), strokeColor());
    CGContextFillRect(platformContext(), CGRectMake(x, y, lineLength, thickness));

    CGContextRestoreGState(platformContext());
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    if (paintingDisabled())
        return;
        
    CFURLRef urlRef = link.createCFURL();
    if (urlRef) {
        CGContextRef context = platformContext();
        
        // Get the bounding box to handle clipping.
        CGRect box = CGContextGetClipBoundingBox(context);

        IntRect intBox((int)box.origin.x, (int)box.origin.y, (int)box.size.width, (int)box.size.height);
        IntRect rect = destRect;
        rect.intersect(intBox);

        CGPDFContextSetURLForRect(context, urlRef,
            CGRectApplyAffineTransform(rect, CGContextGetCTM(context)));

        CFRelease(urlRef);
    }
}

void GraphicsContext::setUseLowQualityImageInterpolation(bool lowQualityMode)
{
    if (paintingDisabled())
        return;
        
    CGContextSetInterpolationQuality(platformContext(), lowQualityMode ? kCGInterpolationNone : kCGInterpolationDefault);
}

bool GraphicsContext::useLowQualityImageInterpolation() const
{
    if (paintingDisabled())
        return false;
    
    return CGContextGetInterpolationQuality(platformContext());
}

void GraphicsContext::setPlatformTextDrawingMode(int mode)
{
    if (paintingDisabled())
        return;

    // Wow, wish CG had used bits here.
    CGContextRef context = platformContext();
    switch (mode) {
        case cTextInvisible: // Invisible
            CGContextSetTextDrawingMode(context, kCGTextInvisible);
            break;
        case cTextFill: // Fill
            CGContextSetTextDrawingMode(context, kCGTextFill);
            break;
        case cTextStroke: // Stroke
            CGContextSetTextDrawingMode(context, kCGTextStroke);
            break;
        case 3: // Fill | Stroke
            CGContextSetTextDrawingMode(context, kCGTextFillStroke);
            break;
        case cTextClip: // Clip
            CGContextSetTextDrawingMode(context, kCGTextClip);
            break;
        case 5: // Fill | Clip
            CGContextSetTextDrawingMode(context, kCGTextFillClip);
            break;
        case 6: // Stroke | Clip
            CGContextSetTextDrawingMode(context, kCGTextStrokeClip);
            break;
        case 7: // Fill | Stroke | Clip
            CGContextSetTextDrawingMode(context, kCGTextFillStrokeClip);
            break;
        default:
            break;
    }
}

void GraphicsContext::setPlatformStrokeColor(const Color& color)
{
    if (paintingDisabled())
        return;
    setCGStrokeColor(platformContext(), color);
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    if (paintingDisabled())
        return;
    CGContextSetLineWidth(platformContext(), thickness);
}

void GraphicsContext::setPlatformFillColor(const Color& color)
{
    if (paintingDisabled())
        return;
    setCGFillColor(platformContext(), color);
}

}

#endif // PLATFORM(CG)
