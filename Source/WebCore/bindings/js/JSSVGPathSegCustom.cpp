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
    case SVGPathSegType::ClosePath:
        return createWrapper<SVGPathSegClosePath>(globalObject, WTFMove(object));
    case SVGPathSegType::MoveToAbs:
        return createWrapper<SVGPathSegMovetoAbs>(globalObject, WTFMove(object));
    case SVGPathSegType::MoveToRel:
        return createWrapper<SVGPathSegMovetoRel>(globalObject, WTFMove(object));
    case SVGPathSegType::LineToAbs:
        return createWrapper<SVGPathSegLinetoAbs>(globalObject, WTFMove(object));
    case SVGPathSegType::LineToRel:
        return createWrapper<SVGPathSegLinetoRel>(globalObject, WTFMove(object));
    case SVGPathSegType::CurveToCubicAbs:
        return createWrapper<SVGPathSegCurvetoCubicAbs>(globalObject, WTFMove(object));
    case SVGPathSegType::CurveToCubicRel:
        return createWrapper<SVGPathSegCurvetoCubicRel>(globalObject, WTFMove(object));
    case SVGPathSegType::CurveToQuadraticAbs:
        return createWrapper<SVGPathSegCurvetoQuadraticAbs>(globalObject, WTFMove(object));
    case SVGPathSegType::CurveToQuadraticRel:
        return createWrapper<SVGPathSegCurvetoQuadraticRel>(globalObject, WTFMove(object));
    case SVGPathSegType::ArcAbs:
        return createWrapper<SVGPathSegArcAbs>(globalObject, WTFMove(object));
    case SVGPathSegType::ArcRel:
        return createWrapper<SVGPathSegArcRel>(globalObject, WTFMove(object));
    case SVGPathSegType::LineToHorizontalAbs:
        return createWrapper<SVGPathSegLinetoHorizontalAbs>(globalObject, WTFMove(object));
    case SVGPathSegType::LineToHorizontalRel:
        return createWrapper<SVGPathSegLinetoHorizontalRel>(globalObject, WTFMove(object));
    case SVGPathSegType::LineToVerticalAbs:
        return createWrapper<SVGPathSegLinetoVerticalAbs>(globalObject, WTFMove(object));
    case SVGPathSegType::LineToVerticalRel:
        return createWrapper<SVGPathSegLinetoVerticalRel>(globalObject, WTFMove(object));
    case SVGPathSegType::CurveToCubicSmoothAbs:
        return createWrapper<SVGPathSegCurvetoCubicSmoothAbs>(globalObject, WTFMove(object));
    case SVGPathSegType::CurveToCubicSmoothRel:
        return createWrapper<SVGPathSegCurvetoCubicSmoothRel>(globalObject, WTFMove(object));
    case SVGPathSegType::CurveToQuadraticSmoothAbs:
        return createWrapper<SVGPathSegCurvetoQuadraticSmoothAbs>(globalObject, WTFMove(object));
    case SVGPathSegType::CurveToQuadraticSmoothRel:
        return createWrapper<SVGPathSegCurvetoQuadraticSmoothRel>(globalObject, WTFMove(object));
    case SVGPathSegType::Unknown:
    default:
        return createWrapper<SVGPathSeg>(globalObject, WTFMove(object));
    }
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, SVGPathSeg& object)
{
    return wrap(lexicalGlobalObject, globalObject, object);
}

}
