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

#if ENABLE(SVG)
#include "RenderPath.h"

#include <ApplicationServices/ApplicationServices.h>
#include "CgSupport.h"
#include "GraphicsContext.h"
#include "SVGPaintServer.h"
#include "SVGRenderStyle.h"
#include "SVGStyledElement.h"
#include <wtf/Assertions.h>

namespace WebCore {

FloatRect RenderPath::strokeBBox() const
{
    if (style()->svgStyle()->hasStroke())
        return strokeBoundingBox(path(), style(), this);

    return path().boundingRect();
}


bool RenderPath::strokeContains(const FloatPoint& point, bool requiresStroke) const
{
    if (path().isEmpty())
        return false;

    if (requiresStroke && !SVGPaintServer::strokePaintServer(style(), this))
        return false;

    CGMutablePathRef cgPath = path().platformPath();
    
    CGContextRef context = scratchContext();
    CGContextSaveGState(context);
    
    CGContextBeginPath(context);
    CGContextAddPath(context, cgPath);

    GraphicsContext gc(context);
    applyStrokeStyleToContext(&gc, style(), this);

    bool hitSuccess = CGContextPathContainsPoint(context, point, kCGPathStroke);
    CGContextRestoreGState(context);
    
    return hitSuccess;
}

}

#endif // ENABLE(SVG)
