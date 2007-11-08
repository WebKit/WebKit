// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007 Apple Inc. All Rights Reserved.
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
  abs           &MathProtoFuncAbs::create      DontEnum|Function 1
  acos          &MathProtoFuncACos::create     DontEnum|Function 1
  asin          &MathProtoFuncASin::create     DontEnum|Function 1
  atan          &MathProtoFuncATan::create     DontEnum|Function 1
  atan2         &MathProtoFuncATan2::create    DontEnum|Function 2
  ceil          &MathProtoFuncCeil::create     DontEnum|Function 1
  cos           &MathProtoFuncCos::create      DontEnum|Function 1
  exp           &MathProtoFuncExp::create      DontEnum|Function 1
  floor         &MathProtoFuncFloor::create    DontEnum|Function 1
  log           &MathProtoFuncLog::create      DontEnum|Function 1
  max           &MathProtoFuncMax::create      DontEnum|Function 2
  min           &MathProtoFuncMin::create      DontEnum|Function 2
  pow           &MathProtoFuncPow::create      DontEnum|Function 2
  random        &MathProtoFuncRandom::create   DontEnum|Function 0 
  round         &MathProtoFuncRound::create    DontEnum|Function 1
  sin           &MathProtoFuncSin::create      DontEnum|Function 1
  sqrt          &MathProtoFuncSqrt::create     DontEnum|Function 1
  tan           &MathProtoFuncTan::create      DontEnum|Function 1
@end
*/

MathObjectImp::MathObjectImp(ExecState * /*exec*/,
                             ObjectPrototype *objProto)
  : JSObject(objProto)
{
}

// ECMA 15.8

bool MathObjectImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot &slot)
{
  return getStaticPropertySlot<MathObjectImp, JSObject>(exec, &mathTable, this, propertyName, slot);
}

JSValue *MathObjectImp::getValueProperty(ExecState *, int token) const
{
  double d = -42; // ;)
  switch (token) {
  case Euler:
    d = exp(1.0);
    break;
  case Ln2:
    d = log(2.0);
    break;
  case Ln10:
    d = log(10.0);
    break;
  case Log2E:
    d = 1.0/log(2.0);
    break;
  case Log10E:
    d = 1.0/log(10.0);
    break;
  case Pi:
    d = piDouble;
    break;
  case Sqrt1_2:
    d = sqrt(0.5);
    break;
  case Sqrt2:
    d = sqrt(2.0);
    break;
  default:
    ASSERT(0);
  }

  return jsNumber(d);
}

// ------------------------------ Functions --------------------------------

JSValue* MathProtoFuncAbs::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    return signbit(arg) ? jsNumber(-arg) : jsNumber(arg);
}

JSValue* MathProtoFuncACos::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    return jsNumber(acos(arg));
}

JSValue* MathProtoFuncASin::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    return jsNumber(asin(arg));
}

JSValue* MathProtoFuncATan::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    return jsNumber(atan(arg));
}

JSValue* MathProtoFuncATan2::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    double arg2 = args[1]->toNumber(exec);
    return jsNumber(atan2(arg, arg2));
}

JSValue* MathProtoFuncCeil::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    if (signbit(arg) && arg > -1.0)
        return jsNumber(-0.0);
    return jsNumber(ceil(arg));
}

JSValue* MathProtoFuncCos::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    return jsNumber(cos(arg));
}

JSValue* MathProtoFuncExp::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    return jsNumber(exp(arg));
}

JSValue* MathProtoFuncFloor::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    if (signbit(arg) && arg == 0.0)
        return jsNumber(-0.0);
    return jsNumber(floor(arg));
}

JSValue* MathProtoFuncLog::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    return jsNumber(log(arg));
}

JSValue* MathProtoFuncMax::callAsFunction(ExecState* exec, JSObject*, const List& args)
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

JSValue* MathProtoFuncMin::callAsFunction(ExecState* exec, JSObject*, const List& args)
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

JSValue* MathProtoFuncPow::callAsFunction(ExecState* exec, JSObject*, const List& args)
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

JSValue* MathProtoFuncRandom::callAsFunction(ExecState*, JSObject*, const List&)
{
    if (!didInitRandom) {
        wtf_random_init();
        didInitRandom = true;
    }
    return jsNumber(wtf_random());
}

JSValue* MathProtoFuncRound::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    if (signbit(arg) && arg >= -0.5)
         return jsNumber(-0.0);
    return jsNumber(floor(arg + 0.5));
}

JSValue* MathProtoFuncSin::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    return jsNumber(sin(arg));
}

JSValue* MathProtoFuncSqrt::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    return jsNumber(sqrt(arg));
}

JSValue* MathProtoFuncTan::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    double arg = args[0]->toNumber(exec);
    return jsNumber(tan(arg));
}

} // namespace KJS
