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

#if SVG_SUPPORT

#import "KCanvasRenderingStyle.h" // for all the CAP_BUTT contstants, etc.

namespace WebCore {
    class IntRect;
    class RenderStyle;
    class RenderObject;
}

#ifndef NDEBUG
void debugDumpCGImageToFile(NSString *filename, CGImageRef image, int width, int height);
void debugDumpCGLayerToFile(NSString *filename, CGLayerRef layer, int width, int height);
void debugDumpCIImageToFile(NSString *filename, CIImage *ciImage, int width, int height);
#endif

CFStringRef CFStringFromCGPath(CGPathRef path);
CFStringRef CFStringFromCGAffineTransform(CGAffineTransform t);
CGAffineTransform CGAffineTransformMakeMapBetweenRects(CGRect source, CGRect dest);

void applyStrokeStyleToContext(CGContextRef, WebCore::RenderStyle*, const WebCore::RenderObject*);

static inline CGLineCap CGLineCapFromKC(KCCapStyle cap)
{
    if (cap == CAP_BUTT)
        return kCGLineCapButt;
    else if (cap == CAP_ROUND)
        return kCGLineCapRound;
    else if (cap == CAP_SQUARE)
        return kCGLineCapSquare;
    
    return kCGLineCapButt;
}

static inline CGLineJoin CGLineJoinFromKC(KCJoinStyle join)
{
    if (join == JOIN_MITER)
        return kCGLineJoinMiter;
    else if (join == JOIN_ROUND)
        return kCGLineJoinRound;
    else if (join == JOIN_BEVEL)
        return kCGLineJoinBevel;
    
    return kCGLineJoinMiter;
}

static inline CGPoint CGPointSubtractPoints(CGPoint a, CGPoint b)
{
    return CGPointMake(a.x - b.x, a.y - b.y);
}

#endif // SVG_SUPPORT
