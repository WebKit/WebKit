/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "DOMInternal.h"
#import "DOMSVGPathSeg.h"
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
#import <objc/objc-class.h>

@implementation DOMSVGPathSeg (WebCoreInternal)

- (WebCore::SVGPathSeg *)_SVGPathSeg
{
    return reinterpret_cast<WebCore::SVGPathSeg*>(_internal);
}

- (id)_initWithSVGPathSeg:(WebCore::SVGPathSeg *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal*>(impl);
    impl->ref();
    WebCore::addDOMWrapper(self, impl);
    return self;
}

+ (DOMSVGPathSeg *)_wrapSVGPathSeg:(WebCore::SVGPathSeg *)impl
{
    if (!impl)
        return nil;
    id cachedInstance;
    cachedInstance = WebCore::getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];

    Class wrapperClass = nil;
    switch (impl->pathSegType()) {
        case WebCore::SVGPathSeg::PATHSEG_UNKNOWN:
            wrapperClass = [DOMSVGPathSeg class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_CLOSEPATH:
            wrapperClass = [DOMSVGPathSegClosePath class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_MOVETO_ABS:
            wrapperClass = [DOMSVGPathSegMovetoAbs class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_MOVETO_REL:
            wrapperClass = [DOMSVGPathSegMovetoRel class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_LINETO_ABS:
            wrapperClass = [DOMSVGPathSegLinetoAbs class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_LINETO_REL:
            wrapperClass = [DOMSVGPathSegLinetoRel class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
            wrapperClass = [DOMSVGPathSegCurvetoCubicAbs class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:
            wrapperClass = [DOMSVGPathSegCurvetoCubicRel class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:
            wrapperClass = [DOMSVGPathSegCurvetoQuadraticAbs class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:
            wrapperClass = [DOMSVGPathSegCurvetoQuadraticRel class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_ARC_ABS:
            wrapperClass = [DOMSVGPathSegArcAbs class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_ARC_REL:
            wrapperClass = [DOMSVGPathSegArcRel class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:
            wrapperClass = [DOMSVGPathSegLinetoHorizontalAbs class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:
            wrapperClass = [DOMSVGPathSegLinetoHorizontalRel class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:
            wrapperClass = [DOMSVGPathSegLinetoVerticalAbs class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:
            wrapperClass = [DOMSVGPathSegLinetoVerticalRel class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
            wrapperClass = [DOMSVGPathSegCurvetoCubicSmoothAbs class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
            wrapperClass = [DOMSVGPathSegCurvetoCubicSmoothRel class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
            wrapperClass = [DOMSVGPathSegCurvetoQuadraticSmoothAbs class];
            break;
        case WebCore::SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
            wrapperClass = [DOMSVGPathSegCurvetoQuadraticSmoothRel class];
            break;
    }

    return [[[wrapperClass alloc] _initWithSVGPathSeg:impl] autorelease];
}

@end

#endif // ENABLE(SVG)
