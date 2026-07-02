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

using DateTimeValueRecord = IntlDateTimeFormat::DateTimeValueRecord;


bool IntlDateTimeFormat::isTemporalObject(JSValue x)
{
    return dynamicDowncast<TemporalInstant>(x)
        || dynamicDowncast<TemporalPlainDate>(x)
        || dynamicDowncast<TemporalPlainDateTime>(x)
        || dynamicDowncast<TemporalPlainTime>(x)
        || dynamicDowncast<TemporalPlainYearMonth>(x)
        || dynamicDowncast<TemporalPlainMonthDay>(x)
        || dynamicDowncast<TemporalZonedDateTime>(x);
}

// https://tc39.es/proposal-temporal/#sec-temporal-sametemporaltype
bool IntlDateTimeFormat::sameTemporalType(JSValue x, JSValue y)
{
    if (!isTemporalObject(x) || !isTemporalObject(y))
        return false;
#define CHECK(T)                                                    \
    if ((bool)dynamicDowncast<T>(x) != (bool)dynamicDowncast<T>(y)) \
        return false;
    CHECK(TemporalInstant)
    CHECK(TemporalPlainDate)
    CHECK(TemporalPlainDateTime)
    CHECK(TemporalPlainTime)
    CHECK(TemporalPlainYearMonth)
    CHECK(TemporalPlainMonthDay)
    CHECK(TemporalZonedDateTime)
#undef CHECK
    return true;
}

// HandleDateTimeValue ( dateTimeFormat, x )
// https://tc39.es/proposal-temporal/#sec-temporal-handledatetimevalue
IntlDateTimeFormat::DateTimeValueRecord IntlDateTimeFormat::handleDateTimeValue(JSGlobalObject* globalObject, const IntlDateTimeFormat* dtf, JSValue x)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    using Kind = IntlDateTimeFormat::TemporalFieldKind;

    // If date is undefined, let x be !Call(%Date.now%, undefined).
    // (#sec-datetime-format-functions step 3, #sec-Intl.DateTimeFormat.prototype.formatToParts step 3)
    if (x.isUndefined()) {
        double now = dateNowImpl().toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        return { now, Kind::None };
    }

    // iso8601Exempt: per spec, PlainDate and PlainDateTime allow iso8601 calendar to match any DTF
    // calendar (HandleDateTimeTemporalDate/DateTime step 1: "not either dtf.[[Calendar]] or 'iso8601'").
    // PlainYearMonth and PlainMonthDay require exact calendar match — iso8601 is NOT exempt
    // (HandleDateTimeTemporalYearMonth/MonthDay step 1: "not equal to dtf.[[Calendar]]").
    auto validateCalendar = [&](CalendarID calendarId, Kind kind) -> bool {
        bool iso8601Exempt = (kind == Kind::PlainDate || kind == Kind::PlainDateTime);
        if (!dtf || (calendarId == iso8601CalendarID() && iso8601Exempt))
            return false;
        if (!IntlDateTimeFormat::calendarMatchesICU(TemporalCore::calendarIDToString(calendarId), dtf->ensureCalendar())) [[unlikely]] {
            throwRangeError(globalObject, scope, "Temporal object's calendar does not match DateTimeFormat calendar"_s);
            return true;
        }
        return false;
    };

    auto makeDateRecord = [&](CalendarID calId, Kind kind, int32_t y, uint8_t m, uint8_t d) -> DateTimeValueRecord {
        // Step 1: calendar mismatch → RangeError.
        if (validateCalendar(calId, kind))
            return { };
        // Steps 2-3: CombineISODateAndTimeRecord(isoDate, NoonTimeRecord()) + GetUTCEpochNanoseconds.
        auto et = ISO8601::ExactTime::fromISOPartsAndOffset(y, m, d, 12, 0, 0, 0, 0, 0, 0);
        // Steps 4-5: [[TemporalXxxFormat]] null → TypeError. getTemporalFormatter covers all
        // cases (explicit time-only fields, timeStyle-only, etc.), not just a dateStyle heuristic.
        if (dtf && !dtf->getTemporalFormatter(vm, kind)) [[unlikely]] {
            throwTypeError(globalObject, scope, "DateTimeFormat has no fields applicable to this Temporal type"_s);
            return { };
        }
        return { static_cast<double>(et.epochMilliseconds()), kind }; // Step 6.
    };

    // Step 2: [[InitializedTemporalDate]] -> HandleDateTimeTemporalDate.
    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaldate
    if (auto* plainDate = dynamicDowncast<TemporalPlainDate>(x))
        return makeDateRecord(plainDate->calendarID(), Kind::PlainDate, plainDate->year(), plainDate->month(), plainDate->day());

    // Step 3: [[InitializedTemporalYearMonth]] -> HandleDateTimeTemporalYearMonth.
    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporalyearmonth
    if (auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(x)) {
        auto iso = yearMonth->plainYearMonth().isoPlainDate();
        return makeDateRecord(yearMonth->calendarID(), Kind::PlainYearMonth, iso.year(), iso.month(), iso.day());
    }

    // Step 4: [[InitializedTemporalMonthDay]] -> HandleDateTimeTemporalMonthDay.
    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporalmonthday
    if (auto* monthDay = dynamicDowncast<TemporalPlainMonthDay>(x)) {
        auto md = monthDay->plainMonthDay();
        return makeDateRecord(monthDay->calendarID(), Kind::PlainMonthDay, md.isoPlainDate().year(), md.isoPlainDate().month(), md.isoPlainDate().day());
    }

    // Step 5: [[InitializedTemporalTime]] -> HandleDateTimeTemporalTime.
    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaltime
    if (auto* plainTime = dynamicDowncast<TemporalPlainTime>(x)) {
        // Step 5: [[TemporalPlainTimeFormat]] null → TypeError.
        if (dtf && !dtf->getTemporalFormatter(vm, Kind::PlainTime)) [[unlikely]] {
            throwTypeError(globalObject, scope, "DateTimeFormat has no fields applicable to this Temporal type"_s);
            return { };
        }
        // Steps 2-3: CombineISODateAndTimeRecord({1970,1,1}, temporalTime) + GetUTCEpochNanoseconds.
        auto t = plainTime->plainTime();
        auto et = ISO8601::ExactTime::fromISOPartsAndOffset(
            1970, 1, 1, t.hour(), t.minute(), t.second(),
            t.millisecond(), t.microsecond(), t.nanosecond(), 0);
        return { static_cast<double>(et.epochMilliseconds()), Kind::PlainTime }; // Step 6.
    }

    // Step 6: [[InitializedTemporalDateTime]] -> HandleDateTimeTemporalDateTime.
    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaldatetime
    if (auto* plainDateTime = dynamicDowncast<TemporalPlainDateTime>(x)) {
        if (validateCalendar(plainDateTime->calendarID(), Kind::PlainDateTime))
            return { };
        auto d = plainDateTime->plainDate();
        auto t = plainDateTime->plainTime();
        auto et = ISO8601::ExactTime::fromISOPartsAndOffset(
            d.year(), d.month(), d.day(), t.hour(), t.minute(), t.second(),
            t.millisecond(), t.microsecond(), t.nanosecond(), 0);
        return { static_cast<double>(et.epochMilliseconds()), Kind::PlainDateTime };
    }

    // Step 7: [[InitializedTemporalInstant]] -> HandleDateTimeTemporalInstant.
    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporalinstant
    if (auto* instant = dynamicDowncast<TemporalInstant>(x))
        return { static_cast<double>(instant->exactTime().epochMilliseconds()), Kind::Instant };

    // Step 8: Assert ZonedDateTime -> throw TypeError.
    if (dynamicDowncast<TemporalZonedDateTime>(x)) [[unlikely]] {
        throwTypeError(globalObject, scope, "Temporal.ZonedDateTime is not supported in Intl.DateTimeFormat; use toLocaleString() or convert to PlainDateTime first"_s);
        return { };
    }

    // Step 1 (HandleDateTimeOthers): x is a Number — TimeClip.
    // https://tc39.es/proposal-temporal/#sec-temporal-handledatetimeothers
    double clipped = WTF::timeClip(x.toNumber(globalObject));
    RETURN_IF_EXCEPTION(scope, { });
    return { clipped, IntlDateTimeFormat::TemporalFieldKind::None };
}

// https://tc39.es/proposal-temporal/#sec-datetime-format-functions
JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatFuncFormatDateTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let dtf be F.[[DateTimeFormat]].
    IntlDateTimeFormat* format = dynamicDowncast<IntlDateTimeFormat>(callFrame->thisValue());
    if (!format) [[unlikely]]
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.format called on value that's not a DateTimeFormat"_s));

    // Steps 3-5: undefined->Date.now() / ToDateTimeFormattable / FormatDateTime (#sec-formatdatetime).
    RELEASE_AND_RETURN(scope, JSValue::encode(format->format(globalObject, callFrame->argument(0))));
}

// https://tc39.es/ecma402/#sec-intl.datetimeformat.prototype.format
JSC_DEFINE_CUSTOM_GETTER(intlDateTimeFormatPrototypeGetterFormat, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Let dtf be this DateTimeFormat object.
    auto* dtf = IntlDateTimeFormat::unwrapForOldFunctions(globalObject, JSValue::decode(thisValue));
    RETURN_IF_EXCEPTION(scope, { });
    // 2. ReturnIfAbrupt(dtf).
    if (!dtf) [[unlikely]]
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.format called on value that's not a DateTimeFormat"_s));

    JSBoundFunction* boundFormat = dtf->boundFormat();
    // 3. If the [[boundFormat]] internal slot of this DateTimeFormat object is undefined,
    if (!boundFormat) {
        JSGlobalObject* globalObject = dtf->realm();
        // a. Let F be a new built-in function object as defined in 12.3.4.
        // b. The value of F's length property is 1. (Note: F's length property was 0 in ECMA-402 1.0)
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

// https://tc39.es/proposal-temporal/#sec-Intl.DateTimeFormat.prototype.formatToParts
JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatToParts, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let dtf be this value. Step 2: RequireInternalSlot.
    auto* dateTimeFormat = dynamicDowncast<IntlDateTimeFormat>(callFrame->thisValue());
    if (!dateTimeFormat) [[unlikely]]
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.formatToParts called on value that's not a DateTimeFormat"_s));

    // Steps 3-5: undefined->Date.now() / ToDateTimeFormattable / FormatDateTimeToParts (#sec-formatdatetimetoparts).
    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->formatToParts(globalObject, callFrame->argument(0))));
}

// https://tc39.es/proposal-temporal/#sec-intl.datetimeformat.prototype.formatRange
JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatRange, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let dtf be this value. Step 2: RequireInternalSlot.
    auto* dateTimeFormat = dynamicDowncast<IntlDateTimeFormat>(callFrame->thisValue());
    if (!dateTimeFormat) [[unlikely]]
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.formatRange called on value that's not a DateTimeFormat"_s));

    // Step 3: If startDate or endDate is undefined, throw TypeError.
    JSValue startDate = callFrame->argument(0);
    JSValue endDate = callFrame->argument(1);
    if (startDate.isUndefined() || endDate.isUndefined()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "startDate or endDate is undefined"_s);

    // Steps 4-5: ToDateTimeFormattable — non-Temporal values convert to Number.
    if (!IntlDateTimeFormat::isTemporalObject(startDate)) {
        startDate = jsNumber(startDate.toNumber(globalObject));
        RETURN_IF_EXCEPTION(scope, { });
    }
    if (!IntlDateTimeFormat::isTemporalObject(endDate)) {
        endDate = jsNumber(endDate.toNumber(globalObject));
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Step 6: Return ?FormatDateTimeRange(dtf, x, y) — PartitionDateTimeRangePattern inside.
    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->formatRange(globalObject, startDate, endDate)));
}

// https://tc39.es/proposal-temporal/#sec-Intl.DateTimeFormat.prototype.formatRangeToParts
JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncFormatRangeToParts, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Let dtf be this value. Step 2: RequireInternalSlot.
    auto* dateTimeFormat = dynamicDowncast<IntlDateTimeFormat>(callFrame->thisValue());
    if (!dateTimeFormat) [[unlikely]]
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.formatRangeToParts called on value that's not a DateTimeFormat"_s));

    // Step 3: If startDate or endDate is undefined, throw TypeError.
    JSValue startDate = callFrame->argument(0);
    JSValue endDate = callFrame->argument(1);
    if (startDate.isUndefined() || endDate.isUndefined()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "startDate or endDate is undefined"_s);

    // Steps 4-5: ToDateTimeFormattable — non-Temporal values convert to Number.
    if (!IntlDateTimeFormat::isTemporalObject(startDate)) {
        startDate = jsNumber(startDate.toNumber(globalObject));
        RETURN_IF_EXCEPTION(scope, { });
    }
    if (!IntlDateTimeFormat::isTemporalObject(endDate)) {
        endDate = jsNumber(endDate.toNumber(globalObject));
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Step 6: Return ?FormatDateTimeRangeToParts(dtf, x, y) — PartitionDateTimeRangePattern inside.
    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->formatRangeToParts(globalObject, startDate, endDate)));
}

// https://tc39.es/ecma402/#sec-intl.datetimeformat.prototype.resolvedoptions
JSC_DEFINE_HOST_FUNCTION(intlDateTimeFormatPrototypeFuncResolvedOptions, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* dateTimeFormat = IntlDateTimeFormat::unwrapForOldFunctions(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });
    if (!dateTimeFormat) [[unlikely]]
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.resolvedOptions called on value that's not a DateTimeFormat"_s));

    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->resolvedOptions(globalObject)));
}

} // namespace JSC
