/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *               2005 Alexander Kellett <lypanov@kde.org>
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
#include "SVGResourceClipper.h"

#include "GraphicsContext.h"
#include "CgSupport.h"

namespace WebCore {

void SVGResourceClipper::applyClip(GraphicsContext* context, const FloatRect& boundingBox) const
{
    CGContextRef cgContext = context->platformContext();
    if (m_clipData.clipData().size() < 1)
        return;

    bool heterogenousClipRules = false;
    WindRule clipRule = m_clipData.clipData()[0].windRule;

    context->beginPath();

    CGAffineTransform bboxTransform = CGAffineTransformMakeMapBetweenRects(CGRectMake(0,0,1,1), CGRect(boundingBox));

    for (unsigned x = 0; x < m_clipData.clipData().size(); x++) {
        ClipData data = m_clipData.clipData()[x];
        if (data.windRule != clipRule)
            heterogenousClipRules = true;
        
        CGPathRef clipPath = data.path.platformPath();

        if (data.bboxUnits) {
            CGMutablePathRef transformedPath = CGPathCreateMutable();
            CGPathAddPath(transformedPath, &bboxTransform, clipPath);
            CGContextAddPath(cgContext, transformedPath);
            CGPathRelease(transformedPath);
        } else
            CGContextAddPath(cgContext, clipPath);
    }

    if (m_clipData.clipData().size()) {
        // FIXME!
        // We don't currently allow for heterogenous clip rules.
        // we would have to detect such, draw to a mask, and then clip
        // to that mask                
        if (!CGContextIsPathEmpty(cgContext)) {
            if (clipRule == RULE_EVENODD)
                CGContextEOClip(cgContext);
            else
                CGContextClip(cgContext);
        }
    }
}

} // namespace WebCore

#endif // ENABLE(SVG)
