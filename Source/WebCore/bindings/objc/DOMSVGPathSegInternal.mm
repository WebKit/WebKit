/*
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#if ENABLE(SVG)

#import "DOMSVGPathSegInternal.h"

#import "DOMSVGPathSegArcAbs.h"
#import "DOMSVGPathSegArcRel.h"
#import "DOMSVGPathSegClosePath.h"
#import "DOMSVGPathSegCurvetoCubicAbs.h"
#import "DOMSVGPathSegCurvetoCubicRel.h"
#import "DOMSVGPathSegCurvetoCubicSmoothAbs.h"
#import "DOMSVGPathSegCurvetoCubicSmoothRel.h"
#import "DOMSVGPathSegCurvetoQuadraticAbs.h"
#import "DOMSVGPathSegCurvetoQuadraticRel.h"
#import "DOMSVGPathSegCurvetoQuadraticSmoothAbs.h"
#import "DOMSVGPathSegCurvetoQuadraticSmoothRel.h"
#import "DOMSVGPathSegLinetoAbs.h"
#import "DOMSVGPathSegLinetoHorizontalAbs.h"
#import "DOMSVGPathSegLinetoHorizontalRel.h"
#import "DOMSVGPathSegLinetoRel.h"
#import "DOMSVGPathSegLinetoVerticalAbs.h"
#import "DOMSVGPathSegLinetoVerticalRel.h"
#import "DOMSVGPathSegList.h"
#import "DOMSVGPathSegMovetoAbs.h"
#import "DOMSVGPathSegMovetoRel.h"
#import "SVGPathSeg.h"

Class kitClass(WebCore::SVGPathSeg* impl)
{
    switch (impl->pathSegType()) {
        case WebCore::SVGPathSeg::PATHSEG_UNKNOWN:
            return [DOMSVGPathSeg class];
        case WebCore::SVGPathSeg::PATHSEG_CLOSEPATH:
            return [DOMSVGPathSegClosePath class];
        case WebCore::SVGPathSeg::PATHSEG_MOVETO_ABS:
            return [DOMSVGPathSegMovetoAbs class];
        case WebCore::SVGPathSeg::PATHSEG_MOVETO_REL:
            return [DOMSVGPathSegMovetoRel class];
        case WebCore::SVGPathSeg::PATHSEG_LINETO_ABS:
            return [DOMSVGPathSegLinetoAbs class];
        case WebCore::SVGPathSeg::PATHSEG_LINETO_REL:
            return [DOMSVGPathSegLinetoRel class];
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
            return [DOMSVGPathSegCurvetoCubicAbs class];
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:
            return [DOMSVGPathSegCurvetoCubicRel class];
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:
            return [DOMSVGPathSegCurvetoQuadraticAbs class];
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:
            return [DOMSVGPathSegCurvetoQuadraticRel class];
        case WebCore::SVGPathSeg::PATHSEG_ARC_ABS:
            return [DOMSVGPathSegArcAbs class];
        case WebCore::SVGPathSeg::PATHSEG_ARC_REL:
            return [DOMSVGPathSegArcRel class];
        case WebCore::SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:
            return [DOMSVGPathSegLinetoHorizontalAbs class];
        case WebCore::SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:
            return [DOMSVGPathSegLinetoHorizontalRel class];
        case WebCore::SVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:
            return [DOMSVGPathSegLinetoVerticalAbs class];
        case WebCore::SVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:
            return [DOMSVGPathSegLinetoVerticalRel class];
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
            return [DOMSVGPathSegCurvetoCubicSmoothAbs class];
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
            return [DOMSVGPathSegCurvetoCubicSmoothRel class];
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
            return [DOMSVGPathSegCurvetoQuadraticSmoothAbs class];
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
            return [DOMSVGPathSegCurvetoQuadraticSmoothRel class];
    }
    ASSERT_NOT_REACHED();
    return nil;
}

#endif // ENABLE(SVG)
