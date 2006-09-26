/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *               2006 Rob Buis <buis@kde.org>
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
#ifdef SVG_SUPPORT
#import "QuartzSupport.h"

#import "GraphicsContext.h"
#import "KCanvasResourcesQuartz.h"
#import "KRenderingFillPainter.h"
#import "KRenderingStrokePainter.h"
#import "KCanvasRenderingStyle.h"
#import "wtf/Assertions.h"

#import <QuartzCore/CoreImage.h>

#import "SVGRenderStyle.h"

namespace WebCore {

CFStringRef CFStringFromCGAffineTransform(CGAffineTransform t)
{
    return CFStringCreateWithFormat(0, 0, CFSTR("a: %f b: %f c: %f d: %f tx: %f ty: %f"), t.a, t.b, t.c, t.d, t.tx, t.ty);
}

CGAffineTransform CGAffineTransformMakeMapBetweenRects(CGRect source, CGRect dest)
{
    CGAffineTransform transform = CGAffineTransformMakeTranslation(dest.origin.x - source.origin.x, dest.origin.y - source.origin.y);
    transform = CGAffineTransformScale(transform, dest.size.width/source.size.width, dest.size.height/source.size.height);
    return transform;
}

void applyStrokeStyleToContext(CGContextRef context, const KRenderingStrokePainter& strokePainter)
{

    /* Shouldn't all these be in the stroke painter? */
    CGContextSetLineWidth(context, strokePainter.strokeWidth());

    KCCapStyle capStyle = strokePainter.strokeCapStyle();
    CGContextSetLineCap(context, CGLineCapFromKC(capStyle));

    KCJoinStyle joinStyle = strokePainter.strokeJoinStyle();
    CGContextSetLineJoin(context, CGLineJoinFromKC(joinStyle));

    CGContextSetMiterLimit(context, strokePainter.strokeMiterLimit());

    KCDashArray dashes = strokePainter.dashArray();
    if (dashes.count()) {
        size_t dashCount = dashes.count();
        CGFloat *lengths = (CGFloat *)malloc(dashCount * sizeof(CGFloat));
        for (unsigned x = 0; x < dashCount; x++)
            lengths[x] = dashes[x];
        CGContextSetLineDash(context, strokePainter.dashOffset(), lengths, dashes.count());
        free(lengths);
    }
}

void applyStrokeStyleToContext(CGContextRef context, RenderStyle* renderStyle, const RenderObject* renderObject)
{
    KRenderingStrokePainter strokePainter = KSVGPainterFactory::strokePainter(renderStyle, renderObject);
    applyStrokeStyleToContext(context, strokePainter);
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

FloatRect strokeBoundingBox(const Path& path, const KRenderingStrokePainter& strokePainter)
 {
    // the bbox might grow if the path is stroked.
    // and CGPathGetBoundingBox doesn't support that, so we'll have
    // to make an alternative call...
 
    // FIXME: since this is mainly used to decide what to repaint,
    // perhaps it would be sufficien to just outset the fill bbox by
    // the stroke width - that should be way cheaper and simpler than
    // what we do here.
 
    CGPathRef cgPath = path.platformPath();

    CGContextRef context = scratchContext();
    CGContextSaveGState(context);

    CGContextBeginPath(context);
    CGContextAddPath(context, cgPath);
    applyStrokeStyleToContext(context, strokePainter);
    CGContextReplacePathWithStrokedPath(context);
    CGRect box = CGContextGetPathBoundingBox(context);
        
    CGContextRestoreGState(context);

    return FloatRect(box);
}

}

#endif // SVG_SUPPORT

