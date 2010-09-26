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

#include "Lookup.h"
#include "ObjectPrototype.h"
#include "Operations.h"
#include <time.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/RandomNumber.h>
#include <wtf/RandomNumberSeed.h>

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(MathObject);

static EncodedJSValue JSC_HOST_CALL mathProtoFuncAbs(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncACos(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncASin(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncATan(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncATan2(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncCeil(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncCos(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncExp(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncFloor(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncLog(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncMax(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncMin(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncPow(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncRandom(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncRound(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncSin(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncSqrt(ExecState*);
static EncodedJSValue JSC_HOST_CALL mathProtoFuncTan(ExecState*);

}

#include "MathObject.lut.h"

namespace JSC {

// ------------------------------ MathObject --------------------------------

const ClassInfo MathObject::info = { "Math", 0, 0, ExecState::mathTable };

/* Source for MathObject.lut.h
@begin mathTable
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

MathObject::MathObject(ExecState* exec, JSGlobalObject* globalObject, NonNullPassRefPtr<Structure> structure)
    : JSObjectWithGlobalObject(globalObject, structure)
{
    putDirectWithoutTransition(Identifier(exec, "E"), jsNumber(exec, exp(1.0)), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(Identifier(exec, "LN2"), jsNumber(exec, log(2.0)), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(Identifier(exec, "LN10"), jsNumber(exec, log(10.0)), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(Identifier(exec, "LOG2E"), jsNumber(exec, 1.0 / log(2.0)), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(Identifier(exec, "LOG10E"), jsNumber(exec, 1.0 / log(10.0)), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(Identifier(exec, "PI"), jsNumber(exec, piDouble), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(Identifier(exec, "SQRT1_2"), jsNumber(exec, sqrt(0.5)), DontDelete | DontEnum | ReadOnly);
    putDirectWithoutTransition(Identifier(exec, "SQRT2"), jsNumber(exec, sqrt(2.0)), DontDelete | DontEnum | ReadOnly);
}

// ECMA 15.8

bool MathObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot &slot)
{
    return getStaticFunctionSlot<JSObject>(exec, ExecState::mathTable(exec), this, propertyName, slot);
}

bool MathObject::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    return getStaticFunctionDescriptor<JSObject>(exec, ExecState::mathTable(exec), this, propertyName, descriptor);
}

// ------------------------------ Functions --------------------------------

EncodedJSValue JSC_HOST_CALL mathProtoFuncAbs(ExecState* exec)
{
    return JSValue::encode(jsNumber(exec, fabs(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncACos(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(exec, acos(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncASin(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(exec, asin(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncATan(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(exec, atan(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncATan2(ExecState* exec)
{
    double arg0 = exec->argument(0).toNumber(exec);
    double arg1 = exec->argument(1).toNumber(exec);
    return JSValue::encode(jsDoubleNumber(exec, atan2(arg0, arg1)));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncCeil(ExecState* exec)
{
    return JSValue::encode(jsNumber(exec, ceil(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncCos(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(exec, cos(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncExp(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(exec, exp(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncFloor(ExecState* exec)
{
    return JSValue::encode(jsNumber(exec, floor(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncLog(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(exec, log(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncMax(ExecState* exec)
{
    unsigned argsCount = exec->argumentCount();
    double result = -Inf;
    for (unsigned k = 0; k < argsCount; ++k) {
        double val = exec->argument(k).toNumber(exec);
        if (isnan(val)) {
            result = NaN;
            break;
        }
        if (val > result || (val == 0 && result == 0 && !signbit(val)))
            result = val;
    }
    return JSValue::encode(jsNumber(exec, result));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncMin(ExecState* exec)
{
    unsigned argsCount = exec->argumentCount();
    double result = +Inf;
    for (unsigned k = 0; k < argsCount; ++k) {
        double val = exec->argument(k).toNumber(exec);
        if (isnan(val)) {
            result = NaN;
            break;
        }
        if (val < result || (val == 0 && result == 0 && signbit(val)))
            result = val;
    }
    return JSValue::encode(jsNumber(exec, result));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncPow(ExecState* exec)
{
    // ECMA 15.8.2.1.13

    double arg = exec->argument(0).toNumber(exec);
    double arg2 = exec->argument(1).toNumber(exec);

    if (isnan(arg2))
        return JSValue::encode(jsNaN(exec));
    if (isinf(arg2) && fabs(arg) == 1)
        return JSValue::encode(jsNaN(exec));
    return JSValue::encode(jsNumber(exec, pow(arg, arg2)));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncRandom(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(exec, exec->lexicalGlobalObject()->weakRandomNumber()));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncRound(ExecState* exec)
{
    double arg = exec->argument(0).toNumber(exec);
    double integer = ceil(arg);
    return JSValue::encode(jsNumber(exec, integer - (integer - arg > 0.5)));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncSin(ExecState* exec)
{
    return JSValue::encode(exec->globalData().cachedSin(exec, exec->argument(0).toNumber(exec)));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncSqrt(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(exec, sqrt(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncTan(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(exec, tan(exec->argument(0).toNumber(exec))));
}

} // namespace JSC
