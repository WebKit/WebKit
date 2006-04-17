/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#if SVG_SUPPORT
#include "KCanvasPathQuartz.h"
#include "QuartzSupport.h"

namespace WebCore {

KCanvasPathQuartz::KCanvasPathQuartz()
{
    m_cgPath = CGPathCreateMutable();
}

KCanvasPathQuartz::~KCanvasPathQuartz()
{
    CGPathRelease(m_cgPath);
}

bool KCanvasPathQuartz::isEmpty() const
{
    return CGPathIsEmpty(m_cgPath);
}

void KCanvasPathQuartz::moveTo(float x, float y)
{
    CGPathMoveToPoint(m_cgPath, 0, x, y);
}

void KCanvasPathQuartz::lineTo(float x, float y)
{
    CGPathAddLineToPoint(m_cgPath, 0, x, y);
}

void KCanvasPathQuartz::curveTo(float x1, float y1, float x2, float y2, float x3, float y3)
{
    CGPathAddCurveToPoint(m_cgPath, 0, x1, y1, x2, y2, x3, y3);
}

void KCanvasPathQuartz::closeSubpath()
{
    CGPathCloseSubpath(m_cgPath);
}

FloatRect KCanvasPathQuartz::boundingBox()
{
    return CGPathGetBoundingBox(m_cgPath);
}

CGContextRef scratchContext()
{
    static CGContextRef scratch = 0;
    if (!scratch) {
        CFMutableDataRef empty = CFDataCreateMutable(NULL, 0);
        CGDataConsumerRef consumer = CGDataConsumerCreateWithCFData(empty);
        scratch = CGPDFContextCreate(consumer, NULL, NULL);
        CGDataConsumerRelease(consumer);
        CFRelease(empty);

        CGFloat black[4] = {0, 0, 0, 1};
        CGContextSetFillColor(scratch, black);
        CGContextSetStrokeColor(scratch, black);
    }
    return scratch;
}

FloatRect KCanvasPathQuartz::strokeBoundingBox(const KRenderingStrokePainter& strokePainter)
{
    // the bbox might grow if the path is stroked.
    // and CGPathGetBoundingBox doesn't support that, so we'll have
    // to make an alternative call...

    // FIXME: since this is mainly used to decide what to repaint,
    // perhaps it would be sufficien to just outset the fill bbox by
    // the stroke width - that should be way cheaper and simpler than
    // what we do here.

    CGContextRef context = scratchContext();
    CGContextSaveGState(context);

    CGContextBeginPath(context);
    CGContextAddPath(context, m_cgPath);
    applyStrokeStyleToContext(context, strokePainter);
    CGContextReplacePathWithStrokedPath(context);
    CGRect box = CGContextGetPathBoundingBox(context);
        
    CGContextRestoreGState(context);

    return FloatRect(box);
}

static bool pathContainsPoint(CGMutablePathRef cgPath, const FloatPoint& point, CGPathDrawingMode drawMode)
{
   CGContextRef context = scratchContext();
   CGContextSaveGState(context);
   
   CGContextBeginPath(context);
   CGContextAddPath(context, cgPath);
   bool hitSuccess = CGContextPathContainsPoint(context, point, drawMode);

   CGContextRestoreGState(context);

   return hitSuccess;
}

bool KCanvasPathQuartz::containsPoint(const FloatPoint& point, KCWindRule fillRule)
{
    return pathContainsPoint(m_cgPath, point, fillRule == RULE_EVENODD ? kCGPathEOFill : kCGPathFill);
}

bool KCanvasPathQuartz::strokeContainsPoint(const FloatPoint& point)
{
    return pathContainsPoint(m_cgPath, point, kCGPathStroke);
}

}

#endif // SVG_SUPPORT

