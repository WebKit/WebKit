/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
#include "JSCInlines.h"
#include "JSDateMath.h"

namespace JSC {

JSC_DECLARE_HOST_FUNCTION(dateParse);
JSC_DECLARE_HOST_FUNCTION(dateUTC);
JSC_DECLARE_HOST_FUNCTION(dateNow);

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

static JSC_DECLARE_HOST_FUNCTION(callDate);
static JSC_DECLARE_HOST_FUNCTION(constructWithDateConstructor);

DateConstructor::DateConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callDate, constructWithDateConstructor)
{
}

void DateConstructor::finishCreation(VM& vm, DatePrototype* datePrototype)
{
    Base::finishCreation(vm, 7, vm.propertyNames->Date.string(), PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, datePrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

static double millisecondsFromComponents(JSGlobalObject* globalObject, const ArgList& args, WTF::TimeType timeType)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Initialize doubleArguments with default values.
    double doubleArguments[7] {
        0, 0, 1, 0, 0, 0, 0
    };
    unsigned numberOfUsedArguments = std::max(std::min<unsigned>(7U, args.size()), 1U);
    for (unsigned i = 0; i < numberOfUsedArguments; ++i) {
        doubleArguments[i] = args.at(i).toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, 0);
    }
    for (unsigned i = 0; i < numberOfUsedArguments; ++i) {
        if (!std::isfinite(doubleArguments[i]) || (doubleArguments[i] > INT_MAX) || (doubleArguments[i] < INT_MIN))
            return PNaN;
    }

    GregorianDateTime t;
    int year = JSC::toInt32(doubleArguments[0]);
    t.setYear((year >= 0 && year <= 99) ? (year + 1900) : year);
    t.setMonth(JSC::toInt32(doubleArguments[1]));
    t.setMonthDay(JSC::toInt32(doubleArguments[2]));
    t.setHour(JSC::toInt32(doubleArguments[3]));
    t.setMinute(JSC::toInt32(doubleArguments[4]));
    t.setSecond(JSC::toInt32(doubleArguments[5]));
    t.setIsDST(-1);
    return gregorianDateTimeToMS(vm.dateCache, t, doubleArguments[6], timeType);
}

// ECMA 15.9.3
JSObject* constructDate(JSGlobalObject* globalObject, JSValue newTarget, const ArgList& args)
{
    VM& vm = globalObject->vm();
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
            JSValue primitive = arg0.toPrimitive(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (primitive.isString()) {
                String primitiveString = asString(primitive)->value(globalObject);
                RETURN_IF_EXCEPTION(scope, nullptr);
                value = parseDate(globalObject, vm, primitiveString);
                RETURN_IF_EXCEPTION(scope, nullptr);
            } else
                value = primitive.toNumber(globalObject);
        }
    } else
        value = millisecondsFromComponents(globalObject, args, WTF::LocalTime);
    RETURN_IF_EXCEPTION(scope, nullptr);

    Structure* dateStructure = !newTarget || newTarget == globalObject->dateConstructor()
        ? globalObject->dateStructure()
        : InternalFunction::createSubclassStructure(globalObject, asObject(newTarget), getFunctionRealm(vm, asObject(newTarget))->dateStructure());
    RETURN_IF_EXCEPTION(scope, nullptr);

    return DateInstance::create(vm, dateStructure, value);
}
    
JSC_DEFINE_HOST_FUNCTION(constructWithDateConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ArgList args(callFrame);
    return JSValue::encode(constructDate(globalObject, callFrame->newTarget(), args));
}

// ECMA 15.9.2
JSC_DEFINE_HOST_FUNCTION(callDate, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    GregorianDateTime ts;
    msToGregorianDateTime(vm.dateCache, WallTime::now().secondsSinceEpoch().milliseconds(), WTF::LocalTime, ts);
    return JSValue::encode(jsNontrivialString(vm, formatDateTime(ts, DateTimeFormatDateAndTime, false)));
}

JSC_DEFINE_HOST_FUNCTION(dateParse, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    String dateStr = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(jsNumber(parseDate(globalObject, vm, dateStr))));
}

JSValue dateNowImpl()
{
    return jsNumber(jsCurrentTime());
}

JSC_DEFINE_HOST_FUNCTION(dateNow, (JSGlobalObject*, CallFrame*))
{
    return JSValue::encode(jsNumber(jsCurrentTime()));
}

JSC_DEFINE_HOST_FUNCTION(dateUTC, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    double ms = millisecondsFromComponents(globalObject, ArgList(callFrame), WTF::UTCTime);
    return JSValue::encode(jsNumber(timeClip(ms)));
}

} // namespace JSC
