/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IntlDateTimeFormatPrototype.h"

#include "BuiltinNames.h"
#include "DateConstructor.h"
#include "IntlDateTimeFormatInlines.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "TemporalInstant.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalZonedDateTime.h"
#include <wtf/DateMath.h>

namespace JSC {

static JSC_DECLARE_CUSTOM_GETTER(intlDateTimeFormatPrototypeGetterFormat);
static JSC_DECLARE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatRange);
static JSC_DECLARE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatRangeToParts);
static JSC_DECLARE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatToParts);
static JSC_DECLARE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncResolvedOptions);
static JSC_DECLARE_HOST_FUNCTION(intlDateTimeFormatFuncFormatDateTime);

}

#include "IntlDateTimeFormatPrototype.lut.h"

namespace JSC {

const ClassInfo IntlDateTimeFormatPrototype::s_info = { "Intl.DateTimeFormat"_s, &Base::s_info, &dateTimeFormatPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlDateTimeFormatPrototype) };

/* Source for IntlDateTimeFormatPrototype.lut.h
@begin dateTimeFormatPrototypeTable
  format                intlDateTimeFormatPrototypeGetterFormat              DontEnum|ReadOnly|CustomAccessor
  formatRange           intlDateTimeFormatPrototypeFuncFormatRange           DontEnum|Function 2
  formatRangeToParts    intlDateTimeFormatPrototypeFuncFormatRangeToParts    DontEnum|Function 2
  formatToParts         intlDateTimeFormatPrototypeFuncFormatToParts         DontEnum|Function 1
  resolvedOptions       intlDateTimeFormatPrototypeFuncResolvedOptions       DontEnum|Function 0
@end
*/

IntlDateTimeFormatPrototype* IntlDateTimeFormatPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    IntlDateTimeFormatPrototype* object = new (NotNull, allocateCell<IntlDateTimeFormatPrototype>(vm)) IntlDateTimeFormatPrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* IntlDateTimeFormatPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlDateTimeFormatPrototype::IntlDateTimeFormatPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlDateTimeFormatPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    UNUSED_PARAM(globalObject);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

struct DateTimeValueRecord {
    double value;
    IntlDateTimeFormat::TemporalFieldKind kind;
};

static bool isTemporalObject(JSValue x)
{
    return dynamicDowncast<TemporalInstant>(x)
        || dynamicDowncast<TemporalPlainDate>(x)
        || dynamicDowncast<TemporalPlainDateTime>(x)
        || dynamicDowncast<TemporalPlainTime>(x)
        || dynamicDowncast<TemporalPlainYearMonth>(x)
        || dynamicDowncast<TemporalPlainMonthDay>(x)
        || dynamicDowncast<TemporalZonedDateTime>(x);
}

static bool sameTemporalType(JSValue x, JSValue y)
{
    if (!isTemporalObject(x) || !isTemporalObject(y))
        return false;
    if (dynamicDowncast<TemporalInstant>(x) && !dynamicDowncast<TemporalInstant>(y))
        return false;
    if (dynamicDowncast<TemporalPlainDate>(x) && !dynamicDowncast<TemporalPlainDate>(y))
        return false;
    if (dynamicDowncast<TemporalPlainDateTime>(x) && !dynamicDowncast<TemporalPlainDateTime>(y))
        return false;
    if (dynamicDowncast<TemporalPlainTime>(x) && !dynamicDowncast<TemporalPlainTime>(y))
        return false;
    if (dynamicDowncast<TemporalPlainYearMonth>(x) && !dynamicDowncast<TemporalPlainYearMonth>(y))
        return false;
    if (dynamicDowncast<TemporalPlainMonthDay>(x) && !dynamicDowncast<TemporalPlainMonthDay>(y))
        return false;
    if (dynamicDowncast<TemporalZonedDateTime>(x) && !dynamicDowncast<TemporalZonedDateTime>(y))
        return false;
    return true;
}

static String getTemporalCalendarId(JSValue x)
{
    if (auto* pd = dynamicDowncast<TemporalPlainDate>(x))
        return pd->calendarId();
    if (auto* pdt = dynamicDowncast<TemporalPlainDateTime>(x))
        return pdt->calendarId();
    if (auto* pym = dynamicDowncast<TemporalPlainYearMonth>(x))
        return pym->calendarId();
    if (auto* pmd = dynamicDowncast<TemporalPlainMonthDay>(x))
        return pmd->calendarId();
    return String();
}

static bool temporalCalendarMatchesICUCalendar(StringView temporalId, const String& icuCalId)
{
    return IntlDateTimeFormat::calendarMatchesICU(temporalId, icuCalId);
}

// HandleDateTimeValue ( dateTimeFormat, x )
// https://tc39.es/proposal-temporal/#sec-temporal-handledatetimevalue
static DateTimeValueRecord handleDateTimeValue(JSGlobalObject* globalObject, const IntlDateTimeFormat* dtf, JSValue x)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    using Kind = IntlDateTimeFormat::TemporalFieldKind;
    using Style = IntlDateTimeFormat::DateTimeStyle;

    if (x.isUndefined()) {
        double now = dateNowImpl().toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        return { now, IntlDateTimeFormat::TemporalFieldKind::None };
    }

    if (auto* instant = dynamicDowncast<TemporalInstant>(x))
        return { static_cast<double>(instant->exactTime().epochMilliseconds()), IntlDateTimeFormat::TemporalFieldKind::Instant };

    if (dynamicDowncast<TemporalZonedDateTime>(x)) {
        throwTypeError(globalObject, scope, "Temporal.ZonedDateTime is not supported in Intl.DateTimeFormat; use toLocaleString() or convert to PlainDateTime first"_s);
        return { };
    }

    // Helpers for style validation per V8/spec.
    auto rejectTimeOnlyStyle = [&]() -> bool {
        if (dtf && dtf->dateStyle() == Style::None && dtf->timeStyle() != Style::None) {
            throwTypeError(globalObject, scope, "Cannot format a date-only Temporal object with a time-only DateTimeFormat (timeStyle without dateStyle)"_s);
            return true;
        }
        return false;
    };
    auto rejectDateOnlyStyle = [&]() -> bool {
        if (dtf && dtf->dateStyle() != Style::None && dtf->timeStyle() == Style::None) {
            throwTypeError(globalObject, scope, "Cannot format a time-only Temporal object with a date-only DateTimeFormat (dateStyle without timeStyle)"_s);
            return true;
        }
        return false;
    };

    auto validateCalendar = [&](StringView calendarId, Kind kind) -> bool {
        if (!dtf || calendarId == "iso8601"_s)
            return false;
        if ((kind == Kind::PlainDate || kind == Kind::PlainDateTime) && calendarId == "iso8601"_s)
            return false;
        if (!temporalCalendarMatchesICUCalendar(calendarId, dtf->ensureCalendar())) {
            throwRangeError(globalObject, scope, "Temporal object's calendar does not match DateTimeFormat calendar"_s);
            return true;
        }
        return false;
    };

    if (auto* plainDate = dynamicDowncast<TemporalPlainDate>(x)) {
        if (rejectTimeOnlyStyle())
            return { };
        if (validateCalendar(plainDate->calendarId(), Kind::PlainDate))
            return { };
        auto et = ISO8601::ExactTime::fromISOPartsAndOffset(
            plainDate->year(), plainDate->month(), plainDate->day(), 12, 0, 0, 0, 0, 0, 0);
        return { static_cast<double>(et.epochMilliseconds()), Kind::PlainDate };
    }

    if (auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(x)) {
        if (validateCalendar(plainDateTime->calendarId(), Kind::PlainDateTime))
            return { };
        auto d = plainDateTime->plainDate();
        auto t = plainDateTime->plainTime();
        auto et = ISO8601::ExactTime::fromISOPartsAndOffset(
            d.year(), d.month(), d.day(), t.hour(), t.minute(), t.second(),
            t.millisecond(), t.microsecond(), t.nanosecond(), 0);
        return { static_cast<double>(et.epochMilliseconds()), Kind::PlainDateTime };
    }

    if (auto* plainTime = dynamicDowncast<TemporalPlainTime>(x)) {
        if (rejectDateOnlyStyle())
            return { };
        auto t = plainTime->plainTime();
        auto et = ISO8601::ExactTime::fromISOPartsAndOffset(
            1970, 1, 1, t.hour(), t.minute(), t.second(),
            t.millisecond(), t.microsecond(), t.nanosecond(), 0);
        return { static_cast<double>(et.epochMilliseconds()), Kind::PlainTime };
    }

    if (auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(x)) {
        if (rejectTimeOnlyStyle())
            return { };
        if (validateCalendar(yearMonth->calendarId(), Kind::PlainYearMonth))
            return { };
        auto et = ISO8601::ExactTime::fromISOPartsAndOffset(
            yearMonth->year(), yearMonth->month(), 1, 12, 0, 0, 0, 0, 0, 0);
        return { static_cast<double>(et.epochMilliseconds()), Kind::PlainYearMonth };
    }

    if (auto* monthDay = dynamicDowncast<TemporalPlainMonthDay>(x)) {
        if (rejectTimeOnlyStyle())
            return { };
        if (validateCalendar(monthDay->calendarId(), Kind::PlainMonthDay))
            return { };
        auto md = monthDay->plainMonthDay();
        auto& d = md.isoPlainDate();
        auto et = ISO8601::ExactTime::fromISOPartsAndOffset(
            d.year(), d.month(), d.day(), 12, 0, 0, 0, 0, 0, 0);
        return { static_cast<double>(et.epochMilliseconds()), Kind::PlainMonthDay };
    }

    double clipped = WTF::timeClip(x.toNumber(globalObject));
    RETURN_IF_EXCEPTION(scope, { });
    return { clipped, IntlDateTimeFormat::TemporalFieldKind::None };
}

JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatFuncFormatDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 12.1.7 DateTime Format Functions (ECMA-402)
    // https://tc39.github.io/ecma402/#sec-formatdatetime

    IntlDateTimeFormat* format = dynamicDowncast<IntlDateTimeFormat>(callFrame->thisValue());
    if (!format)
        [[unlikely]] return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.format called on value that's not a DateTimeFormat"_s));

    JSValue date = callFrame->argument(0);
    auto record = handleDateTimeValue(globalObject, format, date);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(format->format(globalObject, record.value, record.kind)));
}

JSC_DEFINE_CUSTOM_GETTER(intlDateTimeFormatPrototypeGetterFormat, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 12.3.3 Intl.DateTimeFormat.prototype.format (ECMA-402 2.0)
    // 1. Let dtf be this DateTimeFormat object.
    auto* dtf = IntlDateTimeFormat::unwrapForOldFunctions(globalObject, JSValue::decode(thisValue));
    RETURN_IF_EXCEPTION(scope, { });
    // 2. ReturnIfAbrupt(dtf).
    if (!dtf)
        [[unlikely]] return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.format called on value that's not a DateTimeFormat"_s));

    JSBoundFunction* boundFormat = dtf->boundFormat();
    // 3. If the [[boundFormat]] internal slot of this DateTimeFormat object is undefined,
    if (!boundFormat) {
        JSGlobalObject* globalObject = dtf->realm();
        // a. Let F be a new built-in function object as defined in 12.3.4.
        // b. The value of F’s length property is 1. (Note: F’s length property was 0 in ECMA-402 1.0)
        JSFunction* targetObject = JSFunction::create(vm, globalObject, 1, "format"_s, intlDateTimeFormatFuncFormatDateTime, ImplementationVisibility::Public);
        // c. Let bf be BoundFunctionCreate(F, «this value»).
        boundFormat = JSBoundFunction::create(vm, globalObject, targetObject, dtf, { }, 1, jsEmptyString(vm), makeSource("format"_s, SourceOrigin(), SourceTaintedOrigin::Untainted));
        RETURN_IF_EXCEPTION(scope, { });
        boundFormat->reifyLazyPropertyIfNeeded<>(vm, globalObject, vm.propertyNames->name);
        RETURN_IF_EXCEPTION(scope, { });
        boundFormat->putDirect(vm, vm.propertyNames->name, jsEmptyString(vm), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
        // d. Set dtf.[[boundFormat]] to bf.
        dtf->setBoundFormat(vm, boundFormat);
    }
    // 4. Return dtf.[[boundFormat]].
    return JSValue::encode(boundFormat);
}

JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatToParts, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 15.4 Intl.DateTimeFormat.prototype.formatToParts (ECMA-402 4.0)
    // https://tc39.github.io/ecma402/#sec-Intl.DateTimeFormat.prototype.formatToParts

    // Do not use unwrapForOldFunctions.
    auto* dateTimeFormat = dynamicDowncast<IntlDateTimeFormat>(callFrame->thisValue());
    if (!dateTimeFormat)
        [[unlikely]] return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.formatToParts called on value that's not a DateTimeFormat"_s));

    JSValue date = callFrame->argument(0);
    auto record = handleDateTimeValue(globalObject, dateTimeFormat, date);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->formatToParts(globalObject, record.value, record.kind)));
}

// http://tc39.es/proposal-intl-DateTimeFormat-formatRange/#sec-intl.datetimeformat.prototype.formatRange
JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatRange, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Do not use unwrapForOldFunctions.
    auto* dateTimeFormat = dynamicDowncast<IntlDateTimeFormat>(callFrame->thisValue());
    if (!dateTimeFormat)
        [[unlikely]] return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.formatRange called on value that's not a DateTimeFormat"_s));

    JSValue startDateValue = callFrame->argument(0);
    JSValue endDateValue = callFrame->argument(1);

    if (startDateValue.isUndefined() || endDateValue.isUndefined())
        return throwVMTypeError(globalObject, scope, "startDate or endDate is undefined"_s);

    // Spec: ToDateTimeFormattable converts non-Temporal values to numbers first.
    if (!isTemporalObject(startDateValue)) {
        startDateValue = jsNumber(startDateValue.toNumber(globalObject));
        RETURN_IF_EXCEPTION(scope, { });
    }
    if (!isTemporalObject(endDateValue)) {
        endDateValue = jsNumber(endDateValue.toNumber(globalObject));
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (isTemporalObject(startDateValue) || isTemporalObject(endDateValue)) {
        if (!sameTemporalType(startDateValue, endDateValue))
            return throwVMTypeError(globalObject, scope, "formatRange requires both arguments to be the same Temporal type"_s);
        auto startCal = getTemporalCalendarId(startDateValue);
        auto endCal = getTemporalCalendarId(endDateValue);
        if (!startCal.isNull() && !endCal.isNull() && startCal != endCal)
            return throwVMRangeError(globalObject, scope, "formatRange requires both arguments to have the same calendar"_s);
    }

    auto startRecord = handleDateTimeValue(globalObject, dateTimeFormat, startDateValue);
    RETURN_IF_EXCEPTION(scope, { });
    auto endRecord = handleDateTimeValue(globalObject, dateTimeFormat, endDateValue);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->formatRange(globalObject, startRecord.value, endRecord.value, startRecord.kind)));
}

// http://tc39.es/proposal-intl-DateTimeFormat-formatRange/#sec-intl.datetimeformat.prototype.formatRangeToParts
JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatRangeToParts, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Do not use unwrapForOldFunctions.
    auto* dateTimeFormat = dynamicDowncast<IntlDateTimeFormat>(callFrame->thisValue());
    if (!dateTimeFormat)
        [[unlikely]] return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.formatRangeToParts called on value that's not a DateTimeFormat"_s));

    JSValue startDateValue = callFrame->argument(0);
    JSValue endDateValue = callFrame->argument(1);

    if (startDateValue.isUndefined() || endDateValue.isUndefined())
        return throwVMTypeError(globalObject, scope, "startDate or endDate is undefined"_s);

    if (!isTemporalObject(startDateValue)) {
        startDateValue = jsNumber(startDateValue.toNumber(globalObject));
        RETURN_IF_EXCEPTION(scope, { });
    }
    if (!isTemporalObject(endDateValue)) {
        endDateValue = jsNumber(endDateValue.toNumber(globalObject));
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (isTemporalObject(startDateValue) || isTemporalObject(endDateValue)) {
        if (!sameTemporalType(startDateValue, endDateValue))
            return throwVMTypeError(globalObject, scope, "formatRangeToParts requires both arguments to be the same Temporal type"_s);
        auto startCal = getTemporalCalendarId(startDateValue);
        auto endCal = getTemporalCalendarId(endDateValue);
        if (!startCal.isNull() && !endCal.isNull() && startCal != endCal)
            return throwVMRangeError(globalObject, scope, "formatRangeToParts requires both arguments to have the same calendar"_s);
    }

    auto startRecord = handleDateTimeValue(globalObject, dateTimeFormat, startDateValue);
    RETURN_IF_EXCEPTION(scope, { });
    auto endRecord = handleDateTimeValue(globalObject, dateTimeFormat, endDateValue);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->formatRangeToParts(globalObject, startRecord.value, endRecord.value, startRecord.kind)));
}

JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncResolvedOptions, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 12.3.5 Intl.DateTimeFormat.prototype.resolvedOptions() (ECMA-402 2.0)

    auto* dateTimeFormat = IntlDateTimeFormat::unwrapForOldFunctions(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });
    if (!dateTimeFormat)
        [[unlikely]] return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.resolvedOptions called on value that's not a DateTimeFormat"_s));

    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->resolvedOptions(globalObject)));
}

} // namespace JSC
