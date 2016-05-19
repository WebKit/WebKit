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
#include "JSSVGPathSeg.h"

#include "JSDOMBinding.h"
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
#include "SVGPathSeg.h"
#include "SVGPathSegArc.h"
#include "SVGPathSegClosePath.h"
#include "SVGPathSegCurvetoCubic.h"
#include "SVGPathSegCurvetoCubicSmooth.h"
#include "SVGPathSegCurvetoQuadratic.h"
#include "SVGPathSegCurvetoQuadraticSmoothAbs.h"
#include "SVGPathSegCurvetoQuadraticSmoothRel.h"
#include "SVGPathSegLinetoAbs.h"
#include "SVGPathSegLinetoHorizontal.h"
#include "SVGPathSegLinetoRel.h"
#include "SVGPathSegLinetoVertical.h"
#include "SVGPathSegMovetoAbs.h"
#include "SVGPathSegMovetoRel.h"

using namespace JSC;

namespace WebCore {

JSValue toJSNewlyCreated(ExecState*, JSDOMGlobalObject* globalObject, Ref<SVGPathSeg>&& object)
{
    switch (object->pathSegType()) {
    case SVGPathSeg::PATHSEG_CLOSEPATH:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegClosePath, WTFMove(object));
    case SVGPathSeg::PATHSEG_MOVETO_ABS:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegMovetoAbs, WTFMove(object));
    case SVGPathSeg::PATHSEG_MOVETO_REL:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegMovetoRel, WTFMove(object));
    case SVGPathSeg::PATHSEG_LINETO_ABS:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegLinetoAbs, WTFMove(object));
    case SVGPathSeg::PATHSEG_LINETO_REL:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegLinetoRel, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegCurvetoCubicAbs, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegCurvetoCubicRel, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegCurvetoQuadraticAbs, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegCurvetoQuadraticRel, WTFMove(object));
    case SVGPathSeg::PATHSEG_ARC_ABS:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegArcAbs, WTFMove(object));
    case SVGPathSeg::PATHSEG_ARC_REL:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegArcRel, WTFMove(object));
    case SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegLinetoHorizontalAbs, WTFMove(object));
    case SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegLinetoHorizontalRel, WTFMove(object));
    case SVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegLinetoVerticalAbs, WTFMove(object));
    case SVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegLinetoVerticalRel, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegCurvetoCubicSmoothAbs, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegCurvetoCubicSmoothRel, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegCurvetoQuadraticSmoothAbs, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSegCurvetoQuadraticSmoothRel, WTFMove(object));
    case SVGPathSeg::PATHSEG_UNKNOWN:
    default:
        return CREATE_DOM_WRAPPER(globalObject, SVGPathSeg, WTFMove(object));
    }
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, SVGPathSeg& object)
{
    return wrap(state, globalObject, object);
}

}
