/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "DateConstructor.h"

#include "DateConversion.h"
#include "DateInstance.h"
#include "DatePrototype.h"
#include "JSDateMath.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "JSStringBuilder.h"
#include "ObjectPrototype.h"
#include "JSCInlines.h"
#include <math.h>
#include <time.h>
#include <wtf/MathExtras.h>

#if ENABLE(WEB_REPLAY)
#include "InputCursor.h"
#include "JSReplayInputs.h"
#endif

#if OS(WINCE)
extern "C" time_t time(time_t* timer); // Provided by libce.
#endif

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if HAVE(SYS_TIMEB_H)
#include <sys/timeb.h>
#endif

using namespace WTF;

namespace JSC {

static EncodedJSValue JSC_HOST_CALL dateParse(ExecState*);
static EncodedJSValue JSC_HOST_CALL dateNow(ExecState*);
static EncodedJSValue JSC_HOST_CALL dateUTC(ExecState*);

}

#include "DateConstructor.lut.h"

namespace JSC {

const ClassInfo DateConstructor::s_info = { "Function", &InternalFunction::s_info, 0, ExecState::dateConstructorTable, CREATE_METHOD_TABLE(DateConstructor) };

/* Source for DateConstructor.lut.h
@begin dateConstructorTable
  parse     dateParse   DontEnum|Function 1
  UTC       dateUTC     DontEnum|Function 7
  now       dateNow     DontEnum|Function 0
@end
*/

#if ENABLE(WEB_REPLAY)
static double deterministicCurrentTime(JSGlobalObject* globalObject)
{
    double currentTime = jsCurrentTime();
    InputCursor& cursor = globalObject->inputCursor();
    if (cursor.isCapturing())
        cursor.appendInput<GetCurrentTime>(currentTime);

    if (cursor.isReplaying()) {
        if (GetCurrentTime* input = cursor.fetchInput<GetCurrentTime>())
            currentTime = input->currentTime();
    }
    return currentTime;
}
#endif

#if ENABLE(WEB_REPLAY)
#define NORMAL_OR_DETERMINISTIC_FUNCTION(a, b) (b)
#else
#define NORMAL_OR_DETERMINISTIC_FUNCTION(a, b) (a)
#endif

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(DateConstructor);

DateConstructor::DateConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void DateConstructor::finishCreation(VM& vm, DatePrototype* datePrototype)
{
    Base::finishCreation(vm, datePrototype->classInfo()->className);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, datePrototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(7), ReadOnly | DontEnum | DontDelete);
}

bool DateConstructor::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot &slot)
{
    return getStaticFunctionSlot<InternalFunction>(exec, ExecState::dateConstructorTable(exec->vm()), jsCast<DateConstructor*>(object), propertyName, slot);
}

// ECMA 15.9.3
JSObject* constructDate(ExecState* exec, JSGlobalObject* globalObject, const ArgList& args)
{
    VM& vm = exec->vm();
    int numArgs = args.size();

    double value;

    if (numArgs == 0) // new Date() ECMA 15.9.3.3
        value = NORMAL_OR_DETERMINISTIC_FUNCTION(jsCurrentTime(), deterministicCurrentTime(globalObject));
    else if (numArgs == 1) {
        if (args.at(0).inherits(DateInstance::info()))
            value = asDateInstance(args.at(0))->internalNumber();
        else {
            JSValue primitive = args.at(0).toPrimitive(exec);
            if (primitive.isString())
                value = parseDate(vm, primitive.getString(exec));
            else
                value = primitive.toNumber(exec);
        }
    } else {
        double doubleArguments[7] = {
            args.at(0).toNumber(exec), 
            args.at(1).toNumber(exec), 
            args.at(2).toNumber(exec), 
            args.at(3).toNumber(exec), 
            args.at(4).toNumber(exec), 
            args.at(5).toNumber(exec), 
            args.at(6).toNumber(exec)
        };
        if ((!std::isfinite(doubleArguments[0]) || (doubleArguments[0] > INT_MAX) || (doubleArguments[0] < INT_MIN))
            || (!std::isfinite(doubleArguments[1]) || (doubleArguments[1] > INT_MAX) || (doubleArguments[1] < INT_MIN))
            || (numArgs >= 3 && (!std::isfinite(doubleArguments[2]) || (doubleArguments[2] > INT_MAX) || (doubleArguments[2] < INT_MIN)))
            || (numArgs >= 4 && (!std::isfinite(doubleArguments[3]) || (doubleArguments[3] > INT_MAX) || (doubleArguments[3] < INT_MIN)))
            || (numArgs >= 5 && (!std::isfinite(doubleArguments[4]) || (doubleArguments[4] > INT_MAX) || (doubleArguments[4] < INT_MIN)))
            || (numArgs >= 6 && (!std::isfinite(doubleArguments[5]) || (doubleArguments[5] > INT_MAX) || (doubleArguments[5] < INT_MIN)))
            || (numArgs >= 7 && (!std::isfinite(doubleArguments[6]) || (doubleArguments[6] > INT_MAX) || (doubleArguments[6] < INT_MIN))))
            value = QNaN;
        else {
            GregorianDateTime t;
            int year = JSC::toInt32(doubleArguments[0]);
            t.setYear((year >= 0 && year <= 99) ? (year + 1900) : year);
            t.setMonth(JSC::toInt32(doubleArguments[1]));
            t.setMonthDay((numArgs >= 3) ? JSC::toInt32(doubleArguments[2]) : 1);
            t.setHour(JSC::toInt32(doubleArguments[3]));
            t.setMinute(JSC::toInt32(doubleArguments[4]));
            t.setSecond(JSC::toInt32(doubleArguments[5]));
            t.setIsDST(-1);
            double ms = (numArgs >= 7) ? doubleArguments[6] : 0;
            value = gregorianDateTimeToMS(vm, t, ms, false);
        }
    }

    return DateInstance::create(vm, globalObject->dateStructure(), value);
}
    
static EncodedJSValue JSC_HOST_CALL constructWithDateConstructor(ExecState* exec)
{
    ArgList args(exec);
    return JSValue::encode(constructDate(exec, asInternalFunction(exec->callee())->globalObject(), args));
}

ConstructType DateConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructWithDateConstructor;
    return ConstructTypeHost;
}

// ECMA 15.9.2
static EncodedJSValue JSC_HOST_CALL callDate(ExecState* exec)
{
    VM& vm = exec->vm();
    GregorianDateTime ts;
    msToGregorianDateTime(vm, currentTimeMS(), false, ts);
    return JSValue::encode(jsNontrivialString(&vm, formatDateTime(ts, DateTimeFormatDateAndTime, false)));
}

CallType DateConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callDate;
    return CallTypeHost;
}

static EncodedJSValue JSC_HOST_CALL dateParse(ExecState* exec)
{
    return JSValue::encode(jsNumber(parseDate(exec->vm(), exec->argument(0).toString(exec)->value(exec))));
}

static EncodedJSValue JSC_HOST_CALL dateNow(ExecState* exec)
{
#if !ENABLE(WEB_REPLAY)
    UNUSED_PARAM(exec);
#endif

    return JSValue::encode(jsNumber(NORMAL_OR_DETERMINISTIC_FUNCTION(jsCurrentTime(), deterministicCurrentTime(exec->lexicalGlobalObject()))));
}

static EncodedJSValue JSC_HOST_CALL dateUTC(ExecState* exec) 
{
    double doubleArguments[7] = {
        exec->argument(0).toNumber(exec), 
        exec->argument(1).toNumber(exec), 
        exec->argument(2).toNumber(exec), 
        exec->argument(3).toNumber(exec), 
        exec->argument(4).toNumber(exec), 
        exec->argument(5).toNumber(exec), 
        exec->argument(6).toNumber(exec)
    };
    int n = exec->argumentCount();
    if ((std::isnan(doubleArguments[0]) || (doubleArguments[0] > INT_MAX) || (doubleArguments[0] < INT_MIN))
        || (std::isnan(doubleArguments[1]) || (doubleArguments[1] > INT_MAX) || (doubleArguments[1] < INT_MIN))
        || (n >= 3 && (std::isnan(doubleArguments[2]) || (doubleArguments[2] > INT_MAX) || (doubleArguments[2] < INT_MIN)))
        || (n >= 4 && (std::isnan(doubleArguments[3]) || (doubleArguments[3] > INT_MAX) || (doubleArguments[3] < INT_MIN)))
        || (n >= 5 && (std::isnan(doubleArguments[4]) || (doubleArguments[4] > INT_MAX) || (doubleArguments[4] < INT_MIN)))
        || (n >= 6 && (std::isnan(doubleArguments[5]) || (doubleArguments[5] > INT_MAX) || (doubleArguments[5] < INT_MIN)))
        || (n >= 7 && (std::isnan(doubleArguments[6]) || (doubleArguments[6] > INT_MAX) || (doubleArguments[6] < INT_MIN))))
        return JSValue::encode(jsNaN());

    GregorianDateTime t;
    int year = JSC::toInt32(doubleArguments[0]);
    t.setYear((year >= 0 && year <= 99) ? (year + 1900) : year);
    t.setMonth(JSC::toInt32(doubleArguments[1]));
    t.setMonthDay((n >= 3) ? JSC::toInt32(doubleArguments[2]) : 1);
    t.setHour(JSC::toInt32(doubleArguments[3]));
    t.setMinute(JSC::toInt32(doubleArguments[4]));
    t.setSecond(JSC::toInt32(doubleArguments[5]));
    double ms = (n >= 7) ? doubleArguments[6] : 0;
    return JSValue::encode(jsNumber(timeClip(gregorianDateTimeToMS(exec->vm(), t, ms, true))));
}

} // namespace JSC
