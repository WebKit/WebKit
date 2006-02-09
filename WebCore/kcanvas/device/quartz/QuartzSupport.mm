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
#import "QuartzSupport.h"

#import "KCanvasMatrix.h"
#import "KCanvasResourcesQuartz.h"
#import "KRenderingFillPainter.h"
#import "KRenderingStrokePainter.h"
#import "KCanvasRenderingStyle.h"
#import "kxmlcore/Assertions.h"

#import <QuartzCore/CoreImage.h>

#import "SVGRenderStyle.h"

#ifndef NDEBUG
void debugDumpCGImageToFile(NSString *filename, CGImageRef image, int width, int height)
{
    NSImage *fileImage = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
    [fileImage lockFocus];
    CGContextRef fileImageContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    CGContextDrawImage(fileImageContext, CGRectMake(0, 0, width, height), image); 
    [fileImage unlockFocus];
    NSData *tiff = [fileImage TIFFRepresentation];
    [tiff writeToFile:filename atomically:YES];
    [fileImage release];
}

void debugDumpCGLayerToFile(NSString *filename, CGLayerRef layer, int width, int height)
{
    NSImage *fileImage = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
    [fileImage lockFocus];
    CGContextRef fileImageContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    CGContextDrawLayerAtPoint(fileImageContext, CGPointMake(0, 0), layer); 
    [fileImage unlockFocus];
    NSData *tiff = [fileImage TIFFRepresentation];
    [tiff writeToFile:filename atomically:YES];
    [fileImage release];
}

void debugDumpCIImageToFile(NSString *filename, CIImage *ciImage, int width, int height)
{
    NSImage *fileImage = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
    [fileImage lockFocus];
    CGContextRef fileImageContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    CIContext *ciContext = [CIContext contextWithCGContext:fileImageContext options:nil];
    [ciContext drawImage:ciImage atPoint:CGPointZero fromRect:CGRectMake(0, 0, width, height)];
    [fileImage unlockFocus];
    NSData *tiff = [fileImage TIFFRepresentation];
    [tiff writeToFile:filename atomically:YES];
    [fileImage release];
}
#endif

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
        float *lengths = (float *)malloc(dashCount * sizeof(float));
        for (unsigned int x = 0; x < dashCount; x++)
            lengths[x] = dashes[x];
        CGContextSetLineDash(context, strokePainter.dashOffset(), lengths, dashes.count());
        free(lengths);
    }
}

void applyStrokeStyleToContext(CGContextRef context, khtml::RenderStyle* renderStyle, const khtml::RenderObject* renderObject)
{
    KRenderingStrokePainter strokePainter = KSVG::KSVGPainterFactory::strokePainter(renderStyle, renderObject);
    applyStrokeStyleToContext(context, strokePainter);
}

void CGPathToCFStringApplierFunction(void *info, const CGPathElement *element)
{
    CFMutableStringRef string = (CFMutableStringRef)info;
    CFStringRef typeString = CFSTR("");
    CGPoint *points = element->points;
    switch(element->type) {
    case kCGPathElementMoveToPoint:
        CFStringAppendFormat(string, 0, CFSTR("M%.2f,%.2f"), points[0].x, points[0].y);
        break;
    case kCGPathElementAddLineToPoint:
        CFStringAppendFormat(string, 0, CFSTR("L%.2f,%.2f"), points[0].x, points[0].y);
        break;
    case kCGPathElementAddQuadCurveToPoint:
        CFStringAppendFormat(string, 0, CFSTR("Q%.2f,%.2f,%.2f,%.2f"),
                points[0].x, points[0].y, points[1].x, points[1].y);
        break;
    case kCGPathElementAddCurveToPoint:
        CFStringAppendFormat(string, 0, CFSTR("C%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"),
                points[0].x, points[0].y, points[1].x, points[1].y,
                points[2].x, points[2].y);
        break;
    case kCGPathElementCloseSubpath:
        typeString = CFSTR("X"); break;
    }
}

CFStringRef CFStringFromCGPath(CGPathRef path)
{
    if (!path)
        return 0;

    CFMutableStringRef string = CFStringCreateMutable(NULL, 0);
    CGPathApply(path, string, CGPathToCFStringApplierFunction);

    return string;
}

#endif // SVG_SUPPORT

