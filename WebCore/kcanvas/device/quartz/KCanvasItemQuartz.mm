/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *           (C) 2006 Alexander Kellett <lypanov@kde.org>
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

#import <wtf/Assertions.h>

#import "kcanvas/RenderPath.h"
#import "KCanvasRenderingStyle.h"
#import "KRenderingFillPainter.h"
#import "KRenderingStrokePainter.h"

#import "KRenderingDeviceQuartz.h"
#import "KCanvasFilterQuartz.h"
#import "KCanvasResourcesQuartz.h"
#import "KCanvasMaskerQuartz.h"
#import "QuartzSupport.h"

#import "SVGRenderStyle.h"
#import "SVGStyledElement.h"
#import "KCanvasRenderingStyle.h"


namespace WebCore {

FloatRect RenderPath::strokeBBox() const
{
    if (KSVGPainterFactory::isStroked(style())) {
        KRenderingStrokePainter strokePainter = KSVGPainterFactory::strokePainter(style(), this);
        return strokeBoundingBox(path(), strokePainter);
    }

    return path().boundingRect();
}


bool RenderPath::strokeContains(const FloatPoint& point, bool requiresStroke) const
{
    if (path().isEmpty())
        return false;

    if (requiresStroke && !KSVGPainterFactory::strokePaintServer(style(), this))
        return false;

    CGMutablePathRef cgPath = path().platformPath();
    return pathContainsPoint(cgPath, point, kCGPathStroke);
}

}

#endif // SVG_SUPPORT
