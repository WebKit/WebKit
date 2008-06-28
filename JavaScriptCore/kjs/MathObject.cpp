/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008 Apple Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "MathObject.h"

#include "ObjectPrototype.h"
#include "operations.h"
#include <time.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>

namespace KJS {

static JSValue* mathProtoFuncAbs(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncACos(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncASin(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncATan(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncATan2(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncCeil(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncCos(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncExp(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncFloor(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncLog(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncMax(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncMin(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncPow(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncRandom(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncRound(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncSin(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncSqrt(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* mathProtoFuncTan(ExecState*, JSObject*, JSValue*, const ArgList&);

}

#include "MathObject.lut.h"

namespace KJS {

// ------------------------------ MathObject --------------------------------

const ClassInfo MathObject::info = { "Math", 0, 0, ExecState::mathTable };

/* Source for MathObject.lut.h
@begin mathTable
  E             MathObject::Euler           DontEnum|DontDelete|ReadOnly
  LN2           MathObject::Ln2             DontEnum|DontDelete|ReadOnly
  LN10          MathObject::Ln10            DontEnum|DontDelete|ReadOnly
  LOG2E         MathObject::Log2E           DontEnum|DontDelete|ReadOnly
  LOG10E        MathObject::Log10E          DontEnum|DontDelete|ReadOnly
  PI            MathObject::Pi              DontEnum|DontDelete|ReadOnly
  SQRT1_2       MathObject::Sqrt1_2         DontEnum|DontDelete|ReadOnly
  SQRT2         MathObject::Sqrt2           DontEnum|DontDelete|ReadOnly
  abs           mathProtoFuncAbs               DontEnum|Function 1
  acos          mathProtoFuncACos              DontEnum|Function 1
  asin          mathProtoFuncASin              DontEnum|Function 1
  atan          mathProtoFuncATan              DontEnum|Function 1
  atan2         mathProtoFuncATan2             DontEnum|Function 2
  ceil          mathProtoFuncCeil              DontEnum|Function 1
  cos           mathProtoFuncCos               DontEnum|Function 1
  exp           mathProtoFuncExp               DontEnum|Function 1
  floor         mathProtoFuncFloor             DontEnum|Function 1
  log           mathProtoFuncLog               DontEnum|Function 1
  max           mathProtoFuncMax               DontEnum|Function 2
  min           mathProtoFuncMin               DontEnum|Function 2
  pow           mathProtoFuncPow               DontEnum|Function 2
  random        mathProtoFuncRandom            DontEnum|Function 0 
  round         mathProtoFuncRound             DontEnum|Function 1
  sin           mathProtoFuncSin               DontEnum|Function 1
  sqrt          mathProtoFuncSqrt              DontEnum|Function 1
  tan           mathProtoFuncTan               DontEnum|Function 1
@end
*/

MathObject::MathObject(ExecState*, ObjectPrototype* objectPrototype)
    : JSObject(objectPrototype)
{
}

// ECMA 15.8

bool MathObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot &slot)
{
    return getStaticPropertySlot<MathObject, JSObject>(exec, ExecState::mathTable(exec), this, propertyName, slot);
}

JSValue* MathObject::getValueProperty(ExecState* exec, int token) const
{
    switch (token) {
    case Euler:
        return jsNumber(exec, exp(1.0));
    case Ln2:
        return jsNumber(exec, log(2.0));
    case Ln10:
        return jsNumber(exec, log(10.0));
    case Log2E:
        return jsNumber(exec, 1.0 / log(2.0));
    case Log10E:
        return jsNumber(exec, 1.0 / log(10.0));
    case Pi:
        return jsNumber(exec, piDouble);
    case Sqrt1_2:
        return jsNumber(exec, sqrt(0.5));
    case Sqrt2:
        return jsNumber(exec, sqrt(2.0));
    }

    ASSERT_NOT_REACHED();
    return 0;
}

// ------------------------------ Functions --------------------------------

JSValue* mathProtoFuncAbs(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    double arg = args[0]->toNumber(exec);
    return signbit(arg) ? jsNumber(exec, -arg) : jsNumber(exec, arg);
}

JSValue* mathProtoFuncACos(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, acos(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncASin(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, asin(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncATan(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, atan(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncATan2(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, atan2(args[0]->toNumber(exec), args[1]->toNumber(exec)));
}

JSValue* mathProtoFuncCeil(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    double arg = args[0]->toNumber(exec);
    if (signbit(arg) && arg > -1.0)
        return jsNumber(exec, -0.0);
    return jsNumber(exec, ceil(arg));
}

JSValue* mathProtoFuncCos(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, cos(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncExp(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, exp(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncFloor(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    double arg = args[0]->toNumber(exec);
    if (signbit(arg) && arg == 0.0)
        return jsNumber(exec, -0.0);
    return jsNumber(exec, floor(arg));
}

JSValue* mathProtoFuncLog(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, log(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncMax(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    unsigned argsCount = args.size();
    double result = -Inf;
    for (unsigned k = 0; k < argsCount; ++k) {
        double val = args[k]->toNumber(exec);
        if (isnan(val)) {
            result = NaN;
            break;
        }
        if (val > result || (val == 0 && result == 0 && !signbit(val)))
            result = val;
    }
    return jsNumber(exec, result);
}

JSValue* mathProtoFuncMin(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    unsigned argsCount = args.size();
    double result = +Inf;
    for (unsigned k = 0; k < argsCount; ++k) {
        double val = args[k]->toNumber(exec);
        if (isnan(val)) {
            result = NaN;
            break;
        }
        if (val < result || (val == 0 && result == 0 && signbit(val)))
            result = val;
    }
    return jsNumber(exec, result);
}

JSValue* mathProtoFuncPow(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    // ECMA 15.8.2.1.13

    double arg = args[0]->toNumber(exec);
    double arg2 = args[1]->toNumber(exec);

    if (isnan(arg2))
        return jsNaN(exec);
    if (isinf(arg2) && fabs(arg) == 1)
        return jsNaN(exec);
    return jsNumber(exec, pow(arg, arg2));
}

JSValue* mathProtoFuncRandom(ExecState* exec, JSObject*, JSValue*, const ArgList&)
{
#if !USE(MULTIPLE_THREADS)
    static bool didInitRandom;
    if (!didInitRandom) {
        wtf_random_init();
        didInitRandom = true;
    }
#endif

    return jsNumber(exec, wtf_random());
}

JSValue* mathProtoFuncRound(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    double arg = args[0]->toNumber(exec);
    if (signbit(arg) && arg >= -0.5)
         return jsNumber(exec, -0.0);
    return jsNumber(exec, floor(arg + 0.5));
}

JSValue* mathProtoFuncSin(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, sin(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncSqrt(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, sqrt(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncTan(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, tan(args[0]->toNumber(exec)));
}

} // namespace KJS
