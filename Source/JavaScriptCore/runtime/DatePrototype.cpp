/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 *  Copyright (C) 2008, 2009 Torch Mobile, Inc. All rights reserved.
 *  Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
#include "DatePrototype.h"

#include "DateConversion.h"
#include "DateInstance.h"
#include "Error.h"
#include "IntegrityInlines.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSDateMath.h"
#include "JSGlobalObject.h"
#include "JSObject.h"
#include "JSString.h"
#include "TemporalInstant.h"
#include <wtf/Assertions.h>

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetDate);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetDay);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetFullYear);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetHours);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetMilliSeconds);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetMinutes);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetMonth);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetSeconds);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetTimezoneOffset);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetUTCDate);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetUTCDay);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetUTCFullYear);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetUTCHours);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetUTCMilliseconds);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetUTCMinutes);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetUTCMonth);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetUTCSeconds);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncGetYear);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetDate);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetFullYear);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetHours);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetMilliSeconds);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetMinutes);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetMonth);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetSeconds);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetTime);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetUTCDate);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetUTCFullYear);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetUTCHours);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetUTCMilliseconds);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetUTCMinutes);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetUTCMonth);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetUTCSeconds);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncSetYear);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncToDateString);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncToPrimitiveSymbol);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncToString);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncToTimeString);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncToUTCString);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncToISOString);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(dateProtoFuncToTemporalInstant);

}

#include "DatePrototype.lut.h"

namespace JSC {

static EncodedJSValue formateDateInstance(JSGlobalObject* globalObject, CallFrame* callFrame, DateTimeFormat format, bool asUTCVariant)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto& cache = vm.dateCache;

    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    Integrity::auditStructureID(thisDateObj->structureID());
    const GregorianDateTime* gregorianDateTime = asUTCVariant
        ? thisDateObj->gregorianDateTimeUTC(cache)
        : thisDateObj->gregorianDateTime(cache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNontrivialString(vm, String("Invalid Date"_s)));

    return JSValue::encode(jsNontrivialString(vm, formatDateTime(*gregorianDateTime, format, asUTCVariant, cache)));
}


static void applyToNumberToOtherwiseIgnoredArguments(JSGlobalObject* globalObject, CallFrame* callFrame, unsigned maxArgs)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned numArgs = std::min<unsigned>(callFrame->argumentCount(), maxArgs);
    for (unsigned index = 0; index < numArgs; ++index) {
        callFrame->uncheckedArgument(index).toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

// Converts a list of arguments sent to a Date member function into milliseconds, updating
// ms (representing milliseconds) and t (representing the rest of the date structure) appropriately.
//
// Format of member function: f([hour,] [min,] [sec,] [ms])
static bool fillStructuresUsingTimeArgs(JSGlobalObject* globalObject, CallFrame* callFrame, unsigned maxArgs, double* ms, GregorianDateTime* t)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double milliseconds = 0;
    unsigned idx = 0;
    unsigned numArgs = std::min<unsigned>(callFrame->argumentCount(), maxArgs);

    // hours
    if (maxArgs >= 4 && idx < numArgs) {
        t->setHour(0);
        double hours = callFrame->uncheckedArgument(idx++).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        milliseconds += hours * msPerHour;
    }

    // minutes
    if (maxArgs >= 3 && idx < numArgs) {
        t->setMinute(0);
        double minutes = callFrame->uncheckedArgument(idx++).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        milliseconds += minutes * msPerMinute;
    }

    // seconds
    if (maxArgs >= 2 && idx < numArgs) {
        t->setSecond(0);
        double seconds = callFrame->uncheckedArgument(idx++).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        milliseconds += seconds * msPerSecond;
    }

    // milliseconds
    if (idx < numArgs) {
        double millis = callFrame->uncheckedArgument(idx).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        milliseconds += millis;
    } else
        milliseconds += *ms;

    *ms = milliseconds;
    return std::isfinite(milliseconds);
}

// Converts a list of arguments sent to a Date member function into years, months, and milliseconds, updating
// ms (representing milliseconds) and t (representing the rest of the date structure) appropriately.
//
// Format of member function: f([years,] [months,] [days])
static bool fillStructuresUsingDateArgs(JSGlobalObject* globalObject, CallFrame* callFrame, unsigned maxArgs, double *ms, GregorianDateTime *t)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool ok = true;
    unsigned idx = 0;
    unsigned numArgs = std::min<unsigned>(callFrame->argumentCount(), maxArgs);
  
    // years
    if (maxArgs >= 3 && idx < numArgs) {
        double years = callFrame->uncheckedArgument(idx++).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        ok = ok && std::isfinite(years);
        t->setYear(toInt32(years));
    }
    // months
    if (maxArgs >= 2 && idx < numArgs) {
        double months = callFrame->uncheckedArgument(idx++).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        ok = ok && std::isfinite(months);
        t->setMonth(toInt32(months));
    }
    // days
    if (idx < numArgs) {
        double days = callFrame->uncheckedArgument(idx++).toIntegerPreserveNaN(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        ok = ok && std::isfinite(days);
        t->setMonthDay(0);
        *ms += days * msPerDay;
    }
    
    return ok;
}

const ClassInfo DatePrototype::s_info = { "Object"_s, &Base::s_info, &datePrototypeTable, nullptr, CREATE_METHOD_TABLE(DatePrototype) };

/* Source for DatePrototype.lut.h
@begin datePrototypeTable
  toString              dateProtoFuncToString                DontEnum|Function       0
  toISOString           dateProtoFuncToISOString             DontEnum|Function       0
  toDateString          dateProtoFuncToDateString            DontEnum|Function       0
  toTimeString          dateProtoFuncToTimeString            DontEnum|Function       0
  toLocaleString        JSBuiltin                            DontEnum|Function       0
  toLocaleDateString    JSBuiltin                            DontEnum|Function       0
  toLocaleTimeString    JSBuiltin                            DontEnum|Function       0
  valueOf               dateProtoFuncGetTime                 DontEnum|Function       0  DatePrototypeGetTimeIntrinsic
  getTime               dateProtoFuncGetTime                 DontEnum|Function       0  DatePrototypeGetTimeIntrinsic
  getFullYear           dateProtoFuncGetFullYear             DontEnum|Function       0  DatePrototypeGetFullYearIntrinsic
  getUTCFullYear        dateProtoFuncGetUTCFullYear          DontEnum|Function       0  DatePrototypeGetUTCFullYearIntrinsic
  getMonth              dateProtoFuncGetMonth                DontEnum|Function       0  DatePrototypeGetMonthIntrinsic
  getUTCMonth           dateProtoFuncGetUTCMonth             DontEnum|Function       0  DatePrototypeGetUTCMonthIntrinsic
  getDate               dateProtoFuncGetDate                 DontEnum|Function       0  DatePrototypeGetDateIntrinsic
  getUTCDate            dateProtoFuncGetUTCDate              DontEnum|Function       0  DatePrototypeGetUTCDateIntrinsic
  getDay                dateProtoFuncGetDay                  DontEnum|Function       0  DatePrototypeGetDayIntrinsic
  getUTCDay             dateProtoFuncGetUTCDay               DontEnum|Function       0  DatePrototypeGetUTCDayIntrinsic
  getHours              dateProtoFuncGetHours                DontEnum|Function       0  DatePrototypeGetHoursIntrinsic
  getUTCHours           dateProtoFuncGetUTCHours             DontEnum|Function       0  DatePrototypeGetUTCHoursIntrinsic
  getMinutes            dateProtoFuncGetMinutes              DontEnum|Function       0  DatePrototypeGetMinutesIntrinsic
  getUTCMinutes         dateProtoFuncGetUTCMinutes           DontEnum|Function       0  DatePrototypeGetUTCMinutesIntrinsic
  getSeconds            dateProtoFuncGetSeconds              DontEnum|Function       0  DatePrototypeGetSecondsIntrinsic
  getUTCSeconds         dateProtoFuncGetUTCSeconds           DontEnum|Function       0  DatePrototypeGetUTCSecondsIntrinsic
  getMilliseconds       dateProtoFuncGetMilliSeconds         DontEnum|Function       0  DatePrototypeGetMillisecondsIntrinsic
  getUTCMilliseconds    dateProtoFuncGetUTCMilliseconds      DontEnum|Function       0  DatePrototypeGetUTCMillisecondsIntrinsic
  getTimezoneOffset     dateProtoFuncGetTimezoneOffset       DontEnum|Function       0  DatePrototypeGetTimezoneOffsetIntrinsic
  getYear               dateProtoFuncGetYear                 DontEnum|Function       0  DatePrototypeGetYearIntrinsic
  setTime               dateProtoFuncSetTime                 DontEnum|Function       1
  setMilliseconds       dateProtoFuncSetMilliSeconds         DontEnum|Function       1
  setUTCMilliseconds    dateProtoFuncSetUTCMilliseconds      DontEnum|Function       1
  setSeconds            dateProtoFuncSetSeconds              DontEnum|Function       2
  setUTCSeconds         dateProtoFuncSetUTCSeconds           DontEnum|Function       2
  setMinutes            dateProtoFuncSetMinutes              DontEnum|Function       3
  setUTCMinutes         dateProtoFuncSetUTCMinutes           DontEnum|Function       3
  setHours              dateProtoFuncSetHours                DontEnum|Function       4
  setUTCHours           dateProtoFuncSetUTCHours             DontEnum|Function       4
  setDate               dateProtoFuncSetDate                 DontEnum|Function       1
  setUTCDate            dateProtoFuncSetUTCDate              DontEnum|Function       1
  setMonth              dateProtoFuncSetMonth                DontEnum|Function       2
  setUTCMonth           dateProtoFuncSetUTCMonth             DontEnum|Function       2
  setFullYear           dateProtoFuncSetFullYear             DontEnum|Function       3
  setUTCFullYear        dateProtoFuncSetUTCFullYear          DontEnum|Function       3
  setYear               dateProtoFuncSetYear                 DontEnum|Function       1
  toJSON                dateProtoFuncToJSON                  DontEnum|Function       1
@end
*/

// ECMA 15.9.4

DatePrototype::DatePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void DatePrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    Identifier toUTCStringName = Identifier::fromString(vm, "toUTCString"_s);
    JSFunction* toUTCStringFunction = JSFunction::create(vm, globalObject, 0, toUTCStringName.string(), dateProtoFuncToUTCString, ImplementationVisibility::Public);
    putDirectWithoutTransition(vm, toUTCStringName, toUTCStringFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "toGMTString"_s), toUTCStringFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* toPrimitiveFunction = JSFunction::create(vm, globalObject, 1, "[Symbol.toPrimitive]"_s, dateProtoFuncToPrimitiveSymbol, ImplementationVisibility::Public);
    putDirectWithoutTransition(vm, vm.propertyNames->toPrimitiveSymbol, toPrimitiveFunction, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);

    if (Options::useTemporal()) {
        Identifier toTemporalInstantName = Identifier::fromString(vm, "toTemporalInstant"_s);
        JSFunction* toTemporalInstantFunction = JSFunction::create(vm, globalObject, 0, toTemporalInstantName.string(), dateProtoFuncToTemporalInstant, ImplementationVisibility::Public);
        putDirectWithoutTransition(vm, toTemporalInstantName, toTemporalInstantFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    }
    // The constructor will be added later, after DateConstructor has been built.
}

// Functions

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    const bool asUTCVariant = false;
    return formateDateInstance(globalObject, callFrame, DateTimeFormatDateAndTime, asUTCVariant);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncToUTCString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    const bool asUTCVariant = true;
    return formateDateInstance(globalObject, callFrame, DateTimeFormatDateAndTime, asUTCVariant);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncToISOString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    Integrity::auditStructureID(thisDateObj->structureID());
    if (!std::isfinite(thisDateObj->internalNumber()))
        return throwVMError(globalObject, scope, createRangeError(globalObject, "Invalid Date"_s));

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNontrivialString(vm, String("Invalid Date"_s)));
    // Maximum amount of space we need in buffer: 7 (max. digits in year) + 2 * 5 (2 characters each for month, day, hour, minute, second) + 4 (. + 3 digits for milliseconds)
    // 6 for formatting and one for null termination = 28. We add one extra character to allow us to force null termination.
    char buffer[28];
    // If the year is outside the bounds of 0 and 9999 inclusive we want to use the extended year format (ES 15.9.1.15.1).
    int ms = static_cast<int>(fmod(thisDateObj->internalNumber(), msPerSecond));
    if (ms < 0)
        ms += msPerSecond;

    int charactersWritten;
    if (gregorianDateTime->year() > 9999 || gregorianDateTime->year() < 0)
        charactersWritten = snprintf(buffer, sizeof(buffer), "%+07d-%02d-%02dT%02d:%02d:%02d.%03dZ", gregorianDateTime->year(), gregorianDateTime->month() + 1, gregorianDateTime->monthDay(), gregorianDateTime->hour(), gregorianDateTime->minute(), gregorianDateTime->second(), ms);
    else
        charactersWritten = snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", gregorianDateTime->year(), gregorianDateTime->month() + 1, gregorianDateTime->monthDay(), gregorianDateTime->hour(), gregorianDateTime->minute(), gregorianDateTime->second(), ms);

    ASSERT(charactersWritten > 0 && static_cast<unsigned>(charactersWritten) < sizeof(buffer));
    if (static_cast<unsigned>(charactersWritten) >= sizeof(buffer))
        return JSValue::encode(jsEmptyString(vm));

    return JSValue::encode(jsNontrivialString(vm, String(buffer, charactersWritten)));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncToDateString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    const bool asUTCVariant = false;
    return formateDateInstance(globalObject, callFrame, DateTimeFormatDate, asUTCVariant);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncToTimeString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    const bool asUTCVariant = false;
    return formateDateInstance(globalObject, callFrame, DateTimeFormatTime, asUTCVariant);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncToPrimitiveSymbol, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Date.prototype[Symbol.toPrimitive] expected |this| to be an object."_s);
    JSObject* thisObject = jsCast<JSObject*>(thisValue);
    Integrity::auditStructureID(thisObject->structureID());

    if (!callFrame->argumentCount())
        return throwVMTypeError(globalObject, scope, "Date.prototype[Symbol.toPrimitive] expected a first argument."_s);

    JSValue hintValue = callFrame->uncheckedArgument(0);
    PreferredPrimitiveType type = toPreferredPrimitiveType(globalObject, hintValue);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (type == NoPreference)
        type = PreferString;

    RELEASE_AND_RETURN(scope, JSValue::encode(thisObject->ordinaryToPrimitive(globalObject, type)));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    return JSValue::encode(jsNumber(thisDateObj->internalNumber()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetFullYear, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->year()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetUTCFullYear, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->year()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetMonth, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->month()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetUTCMonth, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->month()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->monthDay()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetUTCDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->monthDay()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetDay, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->weekDay()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetUTCDay, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->weekDay()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetHours, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->hour()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetUTCHours, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->hour()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetMinutes, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->minute()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetUTCMinutes, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->minute()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetSeconds, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->second()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetUTCSeconds, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTimeUTC(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(gregorianDateTime->second()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetMilliSeconds, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    double milli = thisDateObj->internalNumber();
    if (std::isnan(milli))
        return JSValue::encode(jsNaN());

    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;
    // Since timeClip makes internalNumber integer milliseconds, this result is always int32_t.
    return JSValue::encode(jsNumber(static_cast<int32_t>(ms)));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetUTCMilliseconds, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    double milli = thisDateObj->internalNumber();
    if (std::isnan(milli))
        return JSValue::encode(jsNaN());

    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;
    // Since timeClip makes internalNumber integer milliseconds, this result is always int32_t.
    return JSValue::encode(jsNumber(static_cast<int32_t>(ms)));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetTimezoneOffset, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());
    return JSValue::encode(jsNumber(-gregorianDateTime->utcOffsetInMinute()));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    double milli = timeClip(callFrame->argument(0).toNumber(globalObject));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    thisDateObj->setInternalNumber(milli);
    return JSValue::encode(jsNumber(milli));
}

static EncodedJSValue setNewValueFromTimeArgs(JSGlobalObject* globalObject, CallFrame* callFrame, unsigned numArgsToUse, WTF::TimeType inputTimeType)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto& cache = vm.dateCache;

    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    double milli = thisDateObj->internalNumber();

    if (!callFrame->argumentCount() || std::isnan(milli)) {
        applyToNumberToOtherwiseIgnoredArguments(globalObject, callFrame, numArgsToUse);
        RETURN_IF_EXCEPTION(scope, { });
        thisDateObj->setInternalNumber(PNaN);
        return JSValue::encode(jsNaN());
    }
     
    double secs = floor(milli / msPerSecond);
    double ms = milli - secs * msPerSecond;

    const GregorianDateTime* other = inputTimeType == WTF::UTCTime
        ? thisDateObj->gregorianDateTimeUTC(cache)
        : thisDateObj->gregorianDateTime(cache);
    if (!other) {
        applyToNumberToOtherwiseIgnoredArguments(globalObject, callFrame, numArgsToUse);
        RETURN_IF_EXCEPTION(scope, { });
        return JSValue::encode(jsNaN());
    }

    GregorianDateTime gregorianDateTime(*other);
    bool success = fillStructuresUsingTimeArgs(globalObject, callFrame, numArgsToUse, &ms, &gregorianDateTime);
    RETURN_IF_EXCEPTION(scope, { });
    if (!success) {
        thisDateObj->setInternalNumber(PNaN);
        return JSValue::encode(jsNaN());
    } 

    double newUTCDate = cache.gregorianDateTimeToMS(gregorianDateTime, ms, inputTimeType);
    double result = timeClip(newUTCDate);
    thisDateObj->setInternalNumber(result);
    return JSValue::encode(jsNumber(result));
}

static EncodedJSValue setNewValueFromDateArgs(JSGlobalObject* globalObject, CallFrame* callFrame, unsigned numArgsToUse, WTF::TimeType inputTimeType)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto& cache = vm.dateCache;

    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    if (!callFrame->argumentCount()) {
        applyToNumberToOtherwiseIgnoredArguments(globalObject, callFrame, numArgsToUse);
        RETURN_IF_EXCEPTION(scope, { });
        thisDateObj->setInternalNumber(PNaN);
        return JSValue::encode(jsNaN());
    }

    double milli = thisDateObj->internalNumber();
    double ms = 0; 

    GregorianDateTime gregorianDateTime; 
    if (numArgsToUse == 3 && std::isnan(milli)) 
        cache.msToGregorianDateTime(0, WTF::UTCTime, gregorianDateTime);
    else { 
        ms = milli - floor(milli / msPerSecond) * msPerSecond; 
        const GregorianDateTime* other = inputTimeType == WTF::UTCTime
            ? thisDateObj->gregorianDateTimeUTC(cache)
            : thisDateObj->gregorianDateTime(cache);
        if (!other) {
            applyToNumberToOtherwiseIgnoredArguments(globalObject, callFrame, numArgsToUse);
            RETURN_IF_EXCEPTION(scope, { });
            return JSValue::encode(jsNaN());
        }
        gregorianDateTime = *other;
    }
    
    bool success = fillStructuresUsingDateArgs(globalObject, callFrame, numArgsToUse, &ms, &gregorianDateTime);
    RETURN_IF_EXCEPTION(scope, { });
    if (!success) {
        thisDateObj->setInternalNumber(PNaN);
        return JSValue::encode(jsNaN());
    } 

    double newUTCDate = cache.gregorianDateTimeToMS(gregorianDateTime, ms, inputTimeType);
    double result = timeClip(newUTCDate);
    thisDateObj->setInternalNumber(result);
    return JSValue::encode(jsNumber(result));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetMilliSeconds, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 1, WTF::LocalTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetUTCMilliseconds, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 1, WTF::UTCTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetSeconds, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 2, WTF::LocalTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetUTCSeconds, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 2, WTF::UTCTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetMinutes, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 3, WTF::LocalTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetUTCMinutes, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 3, WTF::UTCTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetHours, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 4, WTF::LocalTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetUTCHours, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromTimeArgs(globalObject, callFrame, 4, WTF::UTCTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromDateArgs(globalObject, callFrame, 1, WTF::LocalTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetUTCDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromDateArgs(globalObject, callFrame, 1, WTF::UTCTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetMonth, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromDateArgs(globalObject, callFrame, 2, WTF::LocalTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetUTCMonth, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromDateArgs(globalObject, callFrame, 2, WTF::UTCTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetFullYear, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromDateArgs(globalObject, callFrame, 3, WTF::LocalTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetUTCFullYear, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setNewValueFromDateArgs(globalObject, callFrame, 3, WTF::UTCTime);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncSetYear, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto& cache = vm.dateCache;

    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    if (!callFrame->argumentCount()) { 
        thisDateObj->setInternalNumber(PNaN);
        return JSValue::encode(jsNaN());
    }

    double milli = thisDateObj->internalNumber();
    double ms = 0;

    GregorianDateTime gregorianDateTime;
    if (std::isnan(milli))
        // Based on ECMA 262 B.2.5 (setYear)
        // the time must be reset to +0 if it is NaN.
        cache.msToGregorianDateTime(0, WTF::UTCTime, gregorianDateTime);
    else {
        double secs = floor(milli / msPerSecond);
        ms = milli - secs * msPerSecond;
        if (const GregorianDateTime* other = thisDateObj->gregorianDateTime(cache))
            gregorianDateTime = *other;
    }

    double year = callFrame->argument(0).toIntegerPreserveNaN(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (!std::isfinite(year)) {
        thisDateObj->setInternalNumber(PNaN);
        return JSValue::encode(jsNaN());
    }

    gregorianDateTime.setYear(toInt32((year >= 0 && year <= 99) ? (year + 1900) : year));
    double timeInMilliseconds = cache.gregorianDateTimeToMS(gregorianDateTime, ms, WTF::LocalTime);
    double result = timeClip(timeInMilliseconds);
    thisDateObj->setInternalNumber(result);
    return JSValue::encode(jsNumber(result));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncGetYear, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    const GregorianDateTime* gregorianDateTime = thisDateObj->gregorianDateTime(vm.dateCache);
    if (!gregorianDateTime)
        return JSValue::encode(jsNaN());

    // NOTE: IE returns the full year even in getYear.
    return JSValue::encode(jsNumber(gregorianDateTime->year() - 1900));
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    JSObject* object = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue timeValue = object->toPrimitive(globalObject, PreferNumber);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (timeValue.isNumber() && !std::isfinite(timeValue.asNumber()))
        return JSValue::encode(jsNull());

    JSValue toISOValue = object->get(globalObject, vm.propertyNames->toISOString);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    auto callData = JSC::getCallData(toISOValue);
    if (callData.type == CallData::Type::None)
        return throwVMTypeError(globalObject, scope, "toISOString is not a function"_s);

    JSValue result = call(globalObject, asObject(toISOValue), callData, object, *vm.emptyList);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(dateProtoFuncToTemporalInstant, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    auto* thisDateObj = jsDynamicCast<DateInstance*>(thisValue);
    if (UNLIKELY(!thisDateObj))
        return throwVMTypeError(globalObject, scope);

    double epochMilliseconds = thisDateObj->internalNumber();
    if (!isInteger(epochMilliseconds)) 
        return throwVMError(globalObject, scope, createRangeError(globalObject, "Invalid integer number of Epoch Millseconds"_s));

    ASSERT(epochMilliseconds >= std::numeric_limits<int64_t>::min() && epochMilliseconds <= static_cast<double>(std::numeric_limits<int64_t>::max()));
    ISO8601::ExactTime exactTime = ISO8601::ExactTime::fromEpochMilliseconds(epochMilliseconds);
    return JSValue::encode(TemporalInstant::create(vm, globalObject->instantStructure(), exactTime));
}

} // namespace JSC
