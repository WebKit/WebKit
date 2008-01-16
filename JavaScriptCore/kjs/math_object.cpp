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
#include "math_object.h"
#include "math_object.lut.h"

#include "operations.h"
#include <time.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>

namespace KJS {

// ------------------------------ MathObjectImp --------------------------------

const ClassInfo MathObjectImp::info = { "Math", 0, &mathTable };

/* Source for math_object.lut.h
@begin mathTable 21
  E             MathObjectImp::Euler           DontEnum|DontDelete|ReadOnly
  LN2           MathObjectImp::Ln2             DontEnum|DontDelete|ReadOnly
  LN10          MathObjectImp::Ln10            DontEnum|DontDelete|ReadOnly
  LOG2E         MathObjectImp::Log2E           DontEnum|DontDelete|ReadOnly
  LOG10E        MathObjectImp::Log10E          DontEnum|DontDelete|ReadOnly
  PI            MathObjectImp::Pi              DontEnum|DontDelete|ReadOnly
  SQRT1_2       MathObjectImp::Sqrt1_2         DontEnum|DontDelete|ReadOnly
  SQRT2         MathObjectImp::Sqrt2           DontEnum|DontDelete|ReadOnly
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

MathObjectImp::MathObjectImp(ExecState*, ObjectPrototype* objectPrototype)
    : JSObject(objectPrototype)
{
}

// ECMA 15.8

bool MathObjectImp::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot &slot)
{
    return getStaticPropertySlot<MathObjectImp, JSObject>(exec, &mathTable, this, propertyName, slot);
}

JSValue* MathObjectImp::getValueProperty(ExecState*, int token) const
{
    switch (token) {
    case Euler:
        return jsNumber(exp(1.0));
    case Ln2:
        return jsNumber(log(2.0));
    case Ln10:
        return jsNumber(log(10.0));
    case Log2E:
        return jsNumber(1.0 / log(2.0));
    case Log10E:
        return jsNumber(1.0 / log(10.0));
    case Pi:
        return jsNumber(piDouble);
    case Sqrt1_2:
        return jsNumber(sqrt(0.5));
    case Sqrt2:
        return jsNumber(sqrt(2.0));
    }

    ASSERT_NOT_REACHED();
    return 0;
}

// ------------------------------ Functions --------------------------------

JSValue* mathProtoFuncAbs(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    return signbit(arg) ? jsNumber(-arg) : jsNumber(arg);
}

JSValue* mathProtoFuncACos(ExecState* exec, JSObject*, const List& args)
{
    return jsNumber(acos(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncASin(ExecState* exec, JSObject*, const List& args)
{
    return jsNumber(asin(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncATan(ExecState* exec, JSObject*, const List& args)
{
    return jsNumber(atan(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncATan2(ExecState* exec, JSObject*, const List& args)
{
    return jsNumber(atan2(args[0]->toNumber(exec), args[1]->toNumber(exec)));
}

JSValue* mathProtoFuncCeil(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    if (signbit(arg) && arg > -1.0)
        return jsNumber(-0.0);
    return jsNumber(ceil(arg));
}

JSValue* mathProtoFuncCos(ExecState* exec, JSObject*, const List& args)
{
    return jsNumber(cos(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncExp(ExecState* exec, JSObject*, const List& args)
{
    return jsNumber(exp(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncFloor(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    if (signbit(arg) && arg == 0.0)
        return jsNumber(-0.0);
    return jsNumber(floor(arg));
}

JSValue* mathProtoFuncLog(ExecState* exec, JSObject*, const List& args)
{
    return jsNumber(log(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncMax(ExecState* exec, JSObject*, const List& args)
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
    return jsNumber(result);
}

JSValue* mathProtoFuncMin(ExecState* exec, JSObject*, const List& args)
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
    return jsNumber(result);
}

JSValue* mathProtoFuncPow(ExecState* exec, JSObject*, const List& args)
{
    // ECMA 15.8.2.1.13

    double arg = args[0]->toNumber(exec);
    double arg2 = args[1]->toNumber(exec);

    if (isnan(arg2))
        return jsNumber(NaN);
    if (isinf(arg2) && fabs(arg) == 1)
        return jsNumber(NaN);
    return jsNumber(pow(arg, arg2));
}

static bool didInitRandom;

JSValue* mathProtoFuncRandom(ExecState*, JSObject*, const List&)
{
    if (!didInitRandom) {
        wtf_random_init();
        didInitRandom = true;
    }
    return jsNumber(wtf_random());
}

JSValue* mathProtoFuncRound(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    if (signbit(arg) && arg >= -0.5)
         return jsNumber(-0.0);
    return jsNumber(floor(arg + 0.5));
}

JSValue* mathProtoFuncSin(ExecState* exec, JSObject*, const List& args)
{
    return jsNumber(sin(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncSqrt(ExecState* exec, JSObject*, const List& args)
{
    return jsNumber(sqrt(args[0]->toNumber(exec)));
}

JSValue* mathProtoFuncTan(ExecState* exec, JSObject*, const List& args)
{
    return jsNumber(tan(args[0]->toNumber(exec)));
}

} // namespace KJS
