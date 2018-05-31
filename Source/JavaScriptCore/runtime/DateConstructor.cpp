/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2018 Apple Inc. All rights reserved.
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
#include "ObjectPrototype.h"
#include "JSCInlines.h"
#include <math.h>
#include <time.h>
#include <wtf/MathExtras.h>

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if HAVE(SYS_TIMEB_H)
#include <sys/timeb.h>
#endif

using namespace WTF;

namespace JSC {

EncodedJSValue JSC_HOST_CALL dateParse(ExecState*);
EncodedJSValue JSC_HOST_CALL dateUTC(ExecState*);

}

#include "DateConstructor.lut.h"

namespace JSC {

const ClassInfo DateConstructor::s_info = { "Function", &InternalFunction::s_info, &dateConstructorTable, nullptr, CREATE_METHOD_TABLE(DateConstructor) };

/* Source for DateConstructor.lut.h
@begin dateConstructorTable
  parse     dateParse   DontEnum|Function 1
  UTC       dateUTC     DontEnum|Function 7
  now       dateNow     DontEnum|Function 0
@end
*/

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(DateConstructor);

static EncodedJSValue JSC_HOST_CALL callDate(ExecState*);
static EncodedJSValue JSC_HOST_CALL constructWithDateConstructor(ExecState*);

DateConstructor::DateConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callDate, constructWithDateConstructor)
{
}

void DateConstructor::finishCreation(VM& vm, DatePrototype* datePrototype)
{
    Base::finishCreation(vm, "Date");
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, datePrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(7), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

static double millisecondsFromComponents(ExecState* exec, const ArgList& args, WTF::TimeType timeType)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double doubleArguments[7];
    for (int i = 0; i < 7; i++) {
        doubleArguments[i] = args.at(i).toNumber(exec);
        RETURN_IF_EXCEPTION(scope, 0);
    }

    int numArgs = args.size();

    if ((!std::isfinite(doubleArguments[0]) || (doubleArguments[0] > INT_MAX) || (doubleArguments[0] < INT_MIN))
        || (!std::isfinite(doubleArguments[1]) || (doubleArguments[1] > INT_MAX) || (doubleArguments[1] < INT_MIN))
        || (numArgs >= 3 && (!std::isfinite(doubleArguments[2]) || (doubleArguments[2] > INT_MAX) || (doubleArguments[2] < INT_MIN)))
        || (numArgs >= 4 && (!std::isfinite(doubleArguments[3]) || (doubleArguments[3] > INT_MAX) || (doubleArguments[3] < INT_MIN)))
        || (numArgs >= 5 && (!std::isfinite(doubleArguments[4]) || (doubleArguments[4] > INT_MAX) || (doubleArguments[4] < INT_MIN)))
        || (numArgs >= 6 && (!std::isfinite(doubleArguments[5]) || (doubleArguments[5] > INT_MAX) || (doubleArguments[5] < INT_MIN)))
        || (numArgs >= 7 && (!std::isfinite(doubleArguments[6]) || (doubleArguments[6] > INT_MAX) || (doubleArguments[6] < INT_MIN))))
        return PNaN;

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
    return gregorianDateTimeToMS(vm, t, ms, timeType);
}

// ECMA 15.9.3
JSObject* constructDate(ExecState* exec, JSGlobalObject* globalObject, JSValue newTarget, const ArgList& args)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    int numArgs = args.size();

    double value;

    if (numArgs == 0) // new Date() ECMA 15.9.3.3
        value = jsCurrentTime();
    else if (numArgs == 1) {
        JSValue arg0 = args.at(0);
        if (auto* dateInstance = jsDynamicCast<DateInstance*>(vm, arg0))
            value = dateInstance->internalNumber();
        else {
            JSValue primitive = arg0.toPrimitive(exec);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (primitive.isString()) {
                value = parseDate(exec, vm, asString(primitive)->value(exec));
                RETURN_IF_EXCEPTION(scope, nullptr);
            } else
                value = primitive.toNumber(exec);
        }
    } else
        value = millisecondsFromComponents(exec, args, WTF::LocalTime);
    RETURN_IF_EXCEPTION(scope, nullptr);

    Structure* dateStructure = InternalFunction::createSubclassStructure(exec, newTarget, globalObject->dateStructure());
    RETURN_IF_EXCEPTION(scope, nullptr);

    return DateInstance::create(vm, dateStructure, value);
}
    
static EncodedJSValue JSC_HOST_CALL constructWithDateConstructor(ExecState* exec)
{
    ArgList args(exec);
    return JSValue::encode(constructDate(exec, jsCast<InternalFunction*>(exec->jsCallee())->globalObject(exec->vm()), exec->newTarget(), args));
}

// ECMA 15.9.2
static EncodedJSValue JSC_HOST_CALL callDate(ExecState* exec)
{
    VM& vm = exec->vm();
    GregorianDateTime ts;
    msToGregorianDateTime(vm, WallTime::now().secondsSinceEpoch().milliseconds(), WTF::LocalTime, ts);
    return JSValue::encode(jsNontrivialString(&vm, formatDateTime(ts, DateTimeFormatDateAndTime, false)));
}

EncodedJSValue JSC_HOST_CALL dateParse(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    String dateStr = exec->argument(0).toWTFString(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    return JSValue::encode(jsNumber(parseDate(exec, vm, dateStr)));
}

EncodedJSValue JSC_HOST_CALL dateNow(ExecState*)
{
    return JSValue::encode(jsNumber(jsCurrentTime()));
}

EncodedJSValue JSC_HOST_CALL dateUTC(ExecState* exec) 
{
    double ms = millisecondsFromComponents(exec, ArgList(exec), WTF::UTCTime);
    return JSValue::encode(jsNumber(timeClip(ms)));
}

} // namespace JSC
