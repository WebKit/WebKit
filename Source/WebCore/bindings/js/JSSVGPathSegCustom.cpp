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

namespace WebCore {
using namespace JSC;

JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<SVGPathSeg>&& object)
{
    switch (object->pathSegType()) {
    case SVGPathSeg::PATHSEG_CLOSEPATH:
        return createWrapper<SVGPathSegClosePath>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_MOVETO_ABS:
        return createWrapper<SVGPathSegMovetoAbs>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_MOVETO_REL:
        return createWrapper<SVGPathSegMovetoRel>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_LINETO_ABS:
        return createWrapper<SVGPathSegLinetoAbs>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_LINETO_REL:
        return createWrapper<SVGPathSegLinetoRel>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
        return createWrapper<SVGPathSegCurvetoCubicAbs>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:
        return createWrapper<SVGPathSegCurvetoCubicRel>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:
        return createWrapper<SVGPathSegCurvetoQuadraticAbs>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:
        return createWrapper<SVGPathSegCurvetoQuadraticRel>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_ARC_ABS:
        return createWrapper<SVGPathSegArcAbs>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_ARC_REL:
        return createWrapper<SVGPathSegArcRel>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:
        return createWrapper<SVGPathSegLinetoHorizontalAbs>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:
        return createWrapper<SVGPathSegLinetoHorizontalRel>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:
        return createWrapper<SVGPathSegLinetoVerticalAbs>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:
        return createWrapper<SVGPathSegLinetoVerticalRel>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
        return createWrapper<SVGPathSegCurvetoCubicSmoothAbs>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
        return createWrapper<SVGPathSegCurvetoCubicSmoothRel>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
        return createWrapper<SVGPathSegCurvetoQuadraticSmoothAbs>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
        return createWrapper<SVGPathSegCurvetoQuadraticSmoothRel>(globalObject, WTFMove(object));
    case SVGPathSeg::PATHSEG_UNKNOWN:
    default:
        return createWrapper<SVGPathSeg>(globalObject, WTFMove(object));
    }
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, SVGPathSeg& object)
{
    return wrap(lexicalGlobalObject, globalObject, object);
}

}
