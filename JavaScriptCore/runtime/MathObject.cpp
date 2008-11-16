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
#include "Operations.h"
#include <time.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(MathObject);

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

MathObject::MathObject(ExecState* exec, PassRefPtr<Structure> structure)
    : JSObject(structure)
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
    const HashEntry* entry = ExecState::mathTable(exec)->entry(exec, propertyName);

    if (!entry)
        return JSObject::getOwnPropertySlot(exec, propertyName, slot);

    ASSERT(entry->attributes() & Function);
    setUpStaticFunctionSlot(exec, entry, this, propertyName, slot);
    return true;
}

// ------------------------------ Functions --------------------------------

JSValue* mathProtoFuncAbs(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, fabs(args.at(exec, 0)->toNumber(exec)));
}

JSValue* mathProtoFuncACos(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, acos(args.at(exec, 0)->toNumber(exec)));
}

JSValue* mathProtoFuncASin(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, asin(args.at(exec, 0)->toNumber(exec)));
}

JSValue* mathProtoFuncATan(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, atan(args.at(exec, 0)->toNumber(exec)));
}

JSValue* mathProtoFuncATan2(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, atan2(args.at(exec, 0)->toNumber(exec), args.at(exec, 1)->toNumber(exec)));
}

JSValue* mathProtoFuncCeil(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, ceil(args.at(exec, 0)->toNumber(exec)));
}

JSValue* mathProtoFuncCos(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, cos(args.at(exec, 0)->toNumber(exec)));
}

JSValue* mathProtoFuncExp(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, exp(args.at(exec, 0)->toNumber(exec)));
}

JSValue* mathProtoFuncFloor(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, floor(args.at(exec, 0)->toNumber(exec)));
}

JSValue* mathProtoFuncLog(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, log(args.at(exec, 0)->toNumber(exec)));
}

JSValue* mathProtoFuncMax(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    unsigned argsCount = args.size();
    double result = -Inf;
    for (unsigned k = 0; k < argsCount; ++k) {
        double val = args.at(exec, k)->toNumber(exec);
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
        double val = args.at(exec, k)->toNumber(exec);
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

    double arg = args.at(exec, 0)->toNumber(exec);
    double arg2 = args.at(exec, 1)->toNumber(exec);

    if (isnan(arg2))
        return jsNaN(exec);
    if (isinf(arg2) && fabs(arg) == 1)
        return jsNaN(exec);
    return jsNumber(exec, pow(arg, arg2));
}

JSValue* mathProtoFuncRandom(ExecState* exec, JSObject*, JSValue*, const ArgList&)
{
#if !ENABLE(JSC_MULTIPLE_THREADS)
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
    double arg = args.at(exec, 0)->toNumber(exec);
    if (signbit(arg) && arg >= -0.5)
         return jsNumber(exec, -0.0);
    return jsNumber(exec, floor(arg + 0.5));
}

JSValue* mathProtoFuncSin(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, sin(args.at(exec, 0)->toNumber(exec)));
}

JSValue* mathProtoFuncSqrt(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, sqrt(args.at(exec, 0)->toNumber(exec)));
}

JSValue* mathProtoFuncTan(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return jsNumber(exec, tan(args.at(exec, 0)->toNumber(exec)));
}

} // namespace JSC
