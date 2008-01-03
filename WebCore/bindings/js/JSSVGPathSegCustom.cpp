/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG)
#include "JSSVGPathSeg.h"
#include "JSSVGPathSegArcAbs.h"
#include "JSSVGPathSegArcRel.h"
#include "JSSVGPathSegClosePath.h"
#include "JSSVGPathSegCurvetoCubicAbs.h"
#include "JSSVGPathSegCurvetoCubicRel.h"
#include "JSSVGPathSegCurvetoCubicSmoothAbs.h"
#include "JSSVGPathSegCurvetoCubicSmoothRel.h"
#include "JSSVGPathSegCurvetoQuadraticAbs.h"
#include "JSSVGPathSegCurvetoQuadraticRel.h"
#include "JSSVGPathSegCurvetoQuadraticSmoothAbs.h"
#include "JSSVGPathSegCurvetoQuadraticSmoothRel.h"
#include "JSSVGPathSegLinetoAbs.h"
#include "JSSVGPathSegLinetoRel.h"
#include "JSSVGPathSegLinetoHorizontalAbs.h"
#include "JSSVGPathSegLinetoHorizontalRel.h"
#include "JSSVGPathSegLinetoVerticalAbs.h"
#include "JSSVGPathSegLinetoVerticalRel.h"
#include "JSSVGPathSegMovetoAbs.h"
#include "JSSVGPathSegMovetoRel.h"

#include "kjs_binding.h"

#include "SVGPathSeg.h"
#include "SVGPathSegArc.h"
#include "SVGPathSegClosePath.h"
#include "SVGPathSegCurvetoCubic.h"
#include "SVGPathSegCurvetoCubicSmooth.h"
#include "SVGPathSegCurvetoQuadratic.h"
#include "SVGPathSegCurvetoQuadraticSmooth.h"
#include "SVGPathSegLineto.h"
#include "SVGPathSegLinetoHorizontal.h"
#include "SVGPathSegLinetoVertical.h"
#include "SVGPathSegMoveto.h"

using namespace KJS;

namespace WebCore {

JSValue* toJS(ExecState* exec, SVGPathSeg* obj, SVGElement* context)
{
    if (!obj)
        return jsNull();
    
    switch (obj->pathSegType()) {
    case SVGPathSeg::PATHSEG_CLOSEPATH:
        return cacheSVGDOMObject<SVGPathSegClosePath, JSSVGPathSegClosePath, JSSVGPathSegClosePathPrototype>(exec, static_cast<SVGPathSegClosePath*>(obj), context);
    case SVGPathSeg::PATHSEG_MOVETO_ABS:
        return cacheSVGDOMObject<SVGPathSegMovetoAbs, JSSVGPathSegMovetoAbs, JSSVGPathSegMovetoAbsPrototype>(exec, static_cast<SVGPathSegMovetoAbs*>(obj), context);
    case SVGPathSeg::PATHSEG_MOVETO_REL:
        return cacheSVGDOMObject<SVGPathSegMovetoRel, JSSVGPathSegMovetoRel, JSSVGPathSegMovetoRelPrototype>(exec, static_cast<SVGPathSegMovetoRel*>(obj), context);
    case SVGPathSeg::PATHSEG_LINETO_ABS:
        return cacheSVGDOMObject<SVGPathSegLinetoAbs, JSSVGPathSegLinetoAbs, JSSVGPathSegLinetoAbsPrototype>(exec, static_cast<SVGPathSegLinetoAbs*>(obj), context);
    case SVGPathSeg::PATHSEG_LINETO_REL:
        return cacheSVGDOMObject<SVGPathSegLinetoRel, JSSVGPathSegLinetoRel, JSSVGPathSegLinetoRelPrototype>(exec, static_cast<SVGPathSegLinetoRel*>(obj), context);
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
        return cacheSVGDOMObject<SVGPathSegCurvetoCubicAbs, JSSVGPathSegCurvetoCubicAbs, JSSVGPathSegCurvetoCubicAbsPrototype>(exec, static_cast<SVGPathSegCurvetoCubicAbs*>(obj), context);
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:
        return cacheSVGDOMObject<SVGPathSegCurvetoCubicRel, JSSVGPathSegCurvetoCubicRel, JSSVGPathSegCurvetoCubicRelPrototype>(exec, static_cast<SVGPathSegCurvetoCubicRel*>(obj), context);
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:
        return cacheSVGDOMObject<SVGPathSegCurvetoQuadraticAbs, JSSVGPathSegCurvetoQuadraticAbs, JSSVGPathSegCurvetoQuadraticAbsPrototype>(exec, static_cast<SVGPathSegCurvetoQuadraticAbs*>(obj), context);
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:
        return cacheSVGDOMObject<SVGPathSegCurvetoQuadraticRel, JSSVGPathSegCurvetoQuadraticRel, JSSVGPathSegCurvetoQuadraticRelPrototype>(exec, static_cast<SVGPathSegCurvetoQuadraticRel*>(obj), context);
    case SVGPathSeg::PATHSEG_ARC_ABS:
        return cacheSVGDOMObject<SVGPathSegArcAbs, JSSVGPathSegArcAbs, JSSVGPathSegArcAbsPrototype>(exec, static_cast<SVGPathSegArcAbs*>(obj), context);
    case SVGPathSeg::PATHSEG_ARC_REL:
        return cacheSVGDOMObject<SVGPathSegArcRel, JSSVGPathSegArcRel, JSSVGPathSegArcRelPrototype>(exec, static_cast<SVGPathSegArcRel*>(obj), context);
    case SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:
        return cacheSVGDOMObject<SVGPathSegLinetoHorizontalAbs, JSSVGPathSegLinetoHorizontalAbs, JSSVGPathSegLinetoHorizontalAbsPrototype>(exec, static_cast<SVGPathSegLinetoHorizontalAbs*>(obj), context);
    case SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:
        return cacheSVGDOMObject<SVGPathSegLinetoHorizontalRel, JSSVGPathSegLinetoHorizontalRel, JSSVGPathSegLinetoHorizontalRelPrototype>(exec, static_cast<SVGPathSegLinetoHorizontalRel*>(obj), context);
    case SVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:
        return cacheSVGDOMObject<SVGPathSegLinetoVerticalAbs, JSSVGPathSegLinetoVerticalAbs, JSSVGPathSegLinetoVerticalAbsPrototype>(exec, static_cast<SVGPathSegLinetoVerticalAbs*>(obj), context);
    case SVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:
        return cacheSVGDOMObject<SVGPathSegLinetoVerticalRel, JSSVGPathSegLinetoVerticalRel, JSSVGPathSegLinetoVerticalRelPrototype>(exec, static_cast<SVGPathSegLinetoVerticalRel*>(obj), context);
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
        return cacheSVGDOMObject<SVGPathSegCurvetoCubicSmoothAbs, JSSVGPathSegCurvetoCubicSmoothAbs, JSSVGPathSegCurvetoCubicSmoothAbsPrototype>(exec, static_cast<SVGPathSegCurvetoCubicSmoothAbs*>(obj), context);
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
        return cacheSVGDOMObject<SVGPathSegCurvetoCubicSmoothRel, JSSVGPathSegCurvetoCubicSmoothRel, JSSVGPathSegCurvetoCubicSmoothRelPrototype>(exec, static_cast<SVGPathSegCurvetoCubicSmoothRel*>(obj), context);
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
        return cacheSVGDOMObject<SVGPathSegCurvetoQuadraticSmoothAbs, JSSVGPathSegCurvetoQuadraticSmoothAbs, JSSVGPathSegCurvetoQuadraticSmoothAbsPrototype>(exec, static_cast<SVGPathSegCurvetoQuadraticSmoothAbs*>(obj), context);
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
        return cacheSVGDOMObject<SVGPathSegCurvetoQuadraticSmoothRel, JSSVGPathSegCurvetoQuadraticSmoothRel, JSSVGPathSegCurvetoQuadraticSmoothRelPrototype>(exec, static_cast<SVGPathSegCurvetoQuadraticSmoothRel*>(obj), context);
    case SVGPathSeg::PATHSEG_UNKNOWN:
    default:
        return cacheSVGDOMObject<SVGPathSeg, JSSVGPathSeg, JSSVGPathSegPrototype>(exec, obj, context);
    }
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
