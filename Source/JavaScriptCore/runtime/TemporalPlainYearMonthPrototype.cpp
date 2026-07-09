/*
 * Copyright (C) 2022 Apple Inc.
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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
#include "TemporalPlainYearMonthPrototype.h"

#include "CalendarFields.h"
#include "CalendarICUBridge.h"
#include "ISOArithmetic.h"
#include "IntlDateTimeFormat.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainYearMonth.h"
namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncAdd);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncSubtract);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncUntil);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncSince);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToPlainDate);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncWith);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncEquals);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterCalendarId);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthCode);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInMonth);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthsInYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterInLeapYear);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterEra);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterEraYear);

}

#include "TemporalPlainYearMonthPrototype.lut.h"
namespace JSC {

const ClassInfo TemporalPlainYearMonthPrototype::s_info = { "Temporal.PlainYearMonth"_s, &Base::s_info, &plainYearMonthPrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainYearMonthPrototype) };

/* Source for TemporalPlainYearMonthPrototype.lut.h
@begin plainYearMonthPrototypeTable
  add              temporalPlainYearMonthPrototypeFuncAdd                DontEnum|Function 1
  subtract         temporalPlainYearMonthPrototypeFuncSubtract           DontEnum|Function 1
  until            temporalPlainYearMonthPrototypeFuncUntil              DontEnum|Function 1
  since            temporalPlainYearMonthPrototypeFuncSince              DontEnum|Function 1
  toPlainDate      temporalPlainYearMonthPrototypeFuncToPlainDate        DontEnum|Function 1
  toString         temporalPlainYearMonthPrototypeFuncToString           DontEnum|Function 0
  toJSON           temporalPlainYearMonthPrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString   temporalPlainYearMonthPrototypeFuncToLocaleString     DontEnum|Function 0
  with             temporalPlainYearMonthPrototypeFuncWith               DontEnum|Function 1
  equals           temporalPlainYearMonthPrototypeFuncEquals             DontEnum|Function 1
  valueOf          temporalPlainYearMonthPrototypeFuncValueOf            DontEnum|Function 0
  calendarId       temporalPlainYearMonthPrototypeGetterCalendarId       DontEnum|ReadOnly|CustomAccessor
  year             temporalPlainYearMonthPrototypeGetterYear             DontEnum|ReadOnly|CustomAccessor
  month            temporalPlainYearMonthPrototypeGetterMonth            DontEnum|ReadOnly|CustomAccessor
  monthCode        temporalPlainYearMonthPrototypeGetterMonthCode        DontEnum|ReadOnly|CustomAccessor
  daysInMonth      temporalPlainYearMonthPrototypeGetterDaysInMonth      DontEnum|ReadOnly|CustomAccessor
  daysInYear       temporalPlainYearMonthPrototypeGetterDaysInYear       DontEnum|ReadOnly|CustomAccessor
  monthsInYear     temporalPlainYearMonthPrototypeGetterMonthsInYear     DontEnum|ReadOnly|CustomAccessor
  inLeapYear       temporalPlainYearMonthPrototypeGetterInLeapYear       DontEnum|ReadOnly|CustomAccessor
  era              temporalPlainYearMonthPrototypeGetterEra              DontEnum|ReadOnly|CustomAccessor
  eraYear          temporalPlainYearMonthPrototypeGetterEraYear          DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalPlainYearMonthPrototype* TemporalPlainYearMonthPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalPlainYearMonthPrototype>(vm)) TemporalPlainYearMonthPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

Structure* TemporalPlainYearMonthPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainYearMonthPrototype::TemporalPlainYearMonthPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalPlainYearMonthPrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtoyearmonth
template<AddOrSubtract op>
static JSC::EncodedJSValue addDurationToYearMonth(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]] {
        if constexpr (op == AddOrSubtract::Add)
            return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.add called on value that's not a PlainYearMonth"_s);
        else
            return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.subtract called on value that's not a PlainYearMonth"_s);
    }

    // Step 1: duration = ? ToTemporalDuration(temporalDurationLike).
    auto duration = TemporalDuration::toTemporalDurationRecord(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: If operation is ~subtract~, set duration to CreateNegatedTemporalDuration(duration).
    if constexpr (op == AddOrSubtract::Subtract)
        duration = -duration;

    // Step 3: internalDuration = ToInternalDurationRecord(duration) — implicit; ISO8601::Duration already carries the internal date+time components.
    // Steps 4-5: resolvedOptions = ? GetOptionsObject(options); overflow = ? GetTemporalOverflowOption(resolvedOptions).
    TemporalOverflow overflow = toTemporalOverflow(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    // Step 6: durationToAdd = internalDuration.[[Date]] — the (years, months, weeks, days) portion.
    // Step 7: If durationToAdd.[[Weeks]] ≠ 0, or durationToAdd.[[Days]] ≠ 0, or internalDuration.[[Time]] ≠ 0, throw RangeError.
    if (duration.weeks() || duration.days() || duration.hours() || duration.minutes() || duration.seconds() || duration.milliseconds() || duration.microseconds() || duration.nanoseconds()) [[unlikely]] {
        throwRangeError(globalObject, scope, "Duration must not have units below months for PlainYearMonth arithmetic"_s);
        return { };
    }

    // Step 8: calendar = yearMonth.[[Calendar]] — passed to plainYearMonthAdd via `yearMonth->calendarID()`.
    // Steps 9-14 (fused into plainYearMonthAdd)
    auto result = TemporalCore::plainYearMonthAdd(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate(), duration, overflow);
    if (!result) [[unlikely]] {
        if (result.error().kind == TemporalErrorKind::TypeError)
            throwTypeError(globalObject, scope, String(result.error().message));
        else
            throwRangeError(globalObject, scope, String(result.error().message));
        return { };
    }

    // Step 15: Return ! CreateTemporalYearMonth(isoDate, calendar).
    auto* ymResult = TemporalPlainYearMonth::create(vm, globalObject->plainYearMonthStructure(), ISO8601::PlainYearMonth(WTF::move(result->isoDate)));
    if (ymResult && yearMonth->calendarID() != iso8601CalendarID())
        ymResult->setCalendarID(yearMonth->calendarID());
    return JSValue::encode(ymResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.add
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return addDurationToYearMonth<AddOrSubtract::Add>(globalObject, callFrame);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.subtract
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncSubtract, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return addDurationToYearMonth<AddOrSubtract::Subtract>(globalObject, callFrame);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this-value + branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.with called on value that's not a PlainYearMonth"_s);

    // Step 3: If ? IsPartialTemporalObject(temporalYearMonthLike) is false, throw TypeError.
    JSValue temporalYearMonthLike = callFrame->argument(0);
    bool isPartial = isPartialTemporalObject(globalObject, temporalYearMonthLike);
    RETURN_IF_EXCEPTION(scope, { });
    if (!isPartial) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.PlainYearMonth.prototype.with must be a partial Temporal object"_s);
    JSObject* like = asObject(temporalYearMonthLike);

    // Step 4: calendar = plainYearMonth.[[Calendar]] — held on the receiver.

    // Step 6: partialYearMonth = ? PrepareCalendarFields(calendar, temporalYearMonthLike, « year, month, monthCode », « », ~partial~).
    //   CalendarRead::Skip — calendar is known from the receiver; Step 3 already rejected a `calendar` property.
    CalendarID unusedCalId = yearMonth->calendarID();
    auto partialFields = readCalendarFieldsFromObject<FieldSetType::YearMonth, CalendarRead::Skip>(globalObject, like, unusedCalId);
    RETURN_IF_EXCEPTION(scope, { });
    // ~partial~ throws TypeError if none of the requested fields are present with a non-undefined value.
    if (!partialFields.year && !partialFields.month && !partialFields.monthCode
        && !partialFields.era && !partialFields.eraYear) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);

    // Steps 8-9: resolvedOptions = ? GetOptionsObject(options); overflow = ? GetTemporalOverflowOption(resolvedOptions).
    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });
    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 5+7+10: ISODateToFields + CalendarMergeFields + CalendarYearMonthFromFields — fused into plainYearMonthWith.
    auto result = TemporalCore::plainYearMonthWith(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate(), partialFields, overflow);
    if (!result) [[unlikely]] {
        if (result.error().kind == TemporalErrorKind::TypeError)
            throwTypeError(globalObject, scope, String(result.error().message));
        else
            throwRangeError(globalObject, scope, String(result.error().message));
        return { };
    }

    // Step 11: Return ! CreateTemporalYearMonth(isoDate, calendar).
    auto* withResult = TemporalPlainYearMonth::tryCreateIfValid(globalObject, globalObject->plainYearMonthStructure(), WTF::move(result->isoDate));
    RETURN_IF_EXCEPTION(scope, { });
    if (withResult && yearMonth->calendarID() != iso8601CalendarID())
        withResult->setCalendarID(yearMonth->calendarID());
    return JSValue::encode(withResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplainyearmonth
template<DifferenceOperation op>
static JSC::EncodedJSValue differenceTemporalPlainYearMonth(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Prototype op steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]] {
        if constexpr (op == DifferenceOperation::Until)
            return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.until called on value that's not a PlainYearMonth"_s);
        else
            return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.since called on value that's not a PlainYearMonth"_s);
    }

    // Step 1: Set other to ? ToTemporalYearMonth(other).
    auto* other = TemporalPlainYearMonth::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: calendar = yearMonth.[[Calendar]] — held on the receiver.
    CalendarID calendarId = yearMonth->calendarID();

    // Step 3: If CalendarEquals(calendar, other.[[Calendar]]) is false, throw RangeError.
    if (calendarId != other->calendarID()) [[unlikely]] {
        throwRangeError(globalObject, scope, "cannot compute difference between year-months with different calendars"_s);
        return { };
    }

    // Steps 4-5: resolvedOptions = ? GetOptionsObject(options);
    //            settings = ? GetDifferenceSettings(op, resolvedOptions, ~date~, «week, day», ~month~, ~year~).
    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, callFrame->argument(1), UnitGroup::Date, TemporalUnit::Month, TemporalUnit::Year, op);
    RETURN_IF_EXCEPTION(scope, { });
    // Reject the disallowed «week, day» units from spec's GetDifferenceSettings call above.
    if (largestUnit == TemporalUnit::Week || largestUnit == TemporalUnit::Day) [[unlikely]] {
        throwRangeError(globalObject, scope, "largestUnit must be one of year, years, month, months"_s);
        return { };
    }
    if (smallestUnit == TemporalUnit::Week || smallestUnit == TemporalUnit::Day) [[unlikely]] {
        throwRangeError(globalObject, scope, "smallestUnit must be one of year, years, month, months"_s);
        return { };
    }

    auto thisIsoDate = yearMonth->plainYearMonth().isoPlainDate();
    auto otherIsoDate = other->plainYearMonth().isoPlainDate();

    // Step 6: If CompareISODate(yearMonth.[[ISODate]], other.[[ISODate]]) = 0, return zero-duration.
    if (!TemporalCore::isoDateCompare(thisIsoDate, otherIsoDate))
        RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, ISO8601::Duration(), globalObject->durationStructure())));

    // Defensive: both endpoints must be within the representable date-time range before we hand
    // them to calendar arithmetic (icu4x/temporal_rs assume in-range inputs).
    if (!ISO8601::isDateTimeWithinLimits(thisIsoDate.year(), thisIsoDate.month(), thisIsoDate.day(), 12, 0, 0, 0, 0, 0)
        || !ISO8601::isDateTimeWithinLimits(otherIsoDate.year(), otherIsoDate.month(), otherIsoDate.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
        throwRangeError(globalObject, scope, "date/time value is outside of supported range"_s);
        return { };
    }

    // Steps 7-13: thisFields/otherFields with day=1 → thisDate/otherDate → dateDifference.
    //   TemporalCore::differenceYearMonth fuses the fields+day=1+dateFromFields for both endpoints
    //   before invoking CalendarDateUntil. Fallback to direct calendarDateUntil on failure.
    ISO8601::Duration dateDifference;
    auto dateDiffResult = TemporalCore::differenceYearMonth(calendarId, thisIsoDate, otherIsoDate, largestUnit);
    if (!dateDiffResult) {
        if (calendarId != iso8601CalendarID())
            dateDifference = calendarDateUntil(calendarId, thisIsoDate, otherIsoDate, largestUnit);
        else
            dateDifference = TemporalCore::calendarDateUntil(thisIsoDate, otherIsoDate, largestUnit);
    } else
        dateDifference = *dateDiffResult;

    // Steps 14-15: yearsMonthsDifference = ! AdjustDateDurationRecord(dateDifference, 0, 0)  — zero weeks and days;
    //              duration = CombineDateAndTimeDuration(yearsMonthsDifference, 0).
    auto duration = ISO8601::InternalDuration::combineDateAndTimeDuration(
        ISO8601::Duration { dateDifference.years(), dateDifference.months(), 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, Int128(0), Int128(0) }, 0);

    // Step 16: If settings.[[SmallestUnit]] is not ~month~ or settings.[[RoundingIncrement]] != 1, RoundRelativeDuration
    // (spec sub-steps 16.a-e build isoDateTime + originEpochNs + destEpochNs + isoDateTimeOther).
    if (smallestUnit != TemporalUnit::Month || increment != 1) {
        auto originEpochNs = TemporalCore::getUTCEpochNanoseconds(thisIsoDate, ISO8601::PlainTime());
        auto destEpochNs = TemporalCore::getUTCEpochNanoseconds(otherIsoDate, ISO8601::PlainTime());
        auto roundResult = TemporalCore::roundRelativeDuration(duration, originEpochNs, destEpochNs, thisIsoDate, ISO8601::PlainTime(), largestUnit, increment, smallestUnit, roundingMode, nullptr, calendarId);
        if (!roundResult) [[unlikely]] {
            throwTemporalError(globalObject, scope, roundResult.error());
            return { };
        }
    }

    // Step 17: result = ! TemporalDurationFromInternal(duration, ~day~).
    auto durResult = TemporalCore::temporalDurationFromInternal(duration, TemporalUnit::Day);
    if (!durResult) [[unlikely]] {
        throwTemporalError(globalObject, scope, durResult.error());
        return { };
    }
    ISO8601::Duration result = *durResult;

    // Step 18: If op is ~since~, negate result.
    if constexpr (op == DifferenceOperation::Since)
        result = -result;

    // Step 19: Return result.
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalDuration::tryCreateIfValid(globalObject, WTF::move(result), globalObject->durationStructure())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.until
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncUntil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return differenceTemporalPlainYearMonth<DifferenceOperation::Until>(globalObject, callFrame);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.since
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncSince, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return differenceTemporalPlainYearMonth<DifferenceOperation::Since>(globalObject, callFrame);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.equals called on value that's not a PlainYearMonth"_s);

    // Step 3: Set other to ? ToTemporalYearMonth(other).
    auto* other = TemporalPlainYearMonth::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: If CompareISODate(yearMonth.[[ISODate]], other.[[ISODate]]) ≠ 0, return false.
    if (yearMonth->plainYearMonth() != other->plainYearMonth())
        return JSValue::encode(jsBoolean(false));

    // Step 5: Return CalendarEquals(yearMonth.[[Calendar]], other.[[Calendar]]).
    return JSValue::encode(jsBoolean(yearMonth->calendarID() == other->calendarID()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.toplaindate
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToPlainDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this-value + branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toPlainDate called on value that's not a PlainYearMonth"_s);

    // Step 3: If item is not an Object, throw TypeError.
    JSValue itemValue = callFrame->argument(0);
    if (!itemValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toPlainDate: item is not an object"_s);
    JSObject* item = asObject(itemValue);

    // Step 4: calendar = plainYearMonth.[[Calendar]] — held on the receiver.

    // Steps 5-7: fields = ISODateToFields(calendar, plainYearMonth.[[ISODate]], year-month);
    //   inputFields = ? PrepareCalendarFields(calendar, item, «day», «», «»);
    //   merged = CalendarMergeFields(calendar, fields, inputFields).
    //
    //   For PYM the year/monthCode come from the receiver and the only user field is `day`,
    //   so we specialize: read `day` and skip the general merge machinery. `day`'s Conversion
    //   in the calendar-fields table is ~to-positive-integer-with-truncation~, which throws
    //   RangeError for any non-finite value or value ≤ 0.
    std::optional<int32_t> itemDay;
    JSValue dayProperty = item->get(globalObject, vm.propertyNames->day);
    RETURN_IF_EXCEPTION(scope, { });
    if (!dayProperty.isUndefined()) {
        double doubleDay = dayProperty.toIntegerWithTruncation(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(doubleDay)) [[unlikely]]
            return throwVMRangeError(globalObject, scope, "day property must be finite"_s);
        if (doubleDay <= 0) [[unlikely]]
            return throwVMRangeError(globalObject, scope, "day property must be a positive integer"_s);
        if (!isInBounds<int32_t>(doubleDay)) [[unlikely]]
            itemDay = ISO8601::outOfRangeYear; // Later resolve step will report the range error.
        else
            itemDay = static_cast<int32_t>(doubleDay);
    }
    if (!itemDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toPlainDate: item does not have a day field"_s);

    // Step 8: isoDate = ? CalendarDateFromFields(calendar, merged, ~constrain~).
    // Step 9: Return ! CreateTemporalDate(isoDate, calendar).
    if (yearMonth->calendarID() != iso8601CalendarID()) {
        auto resolved = TemporalCore::plainYearMonthToPlainDate(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate(), static_cast<uint8_t>(itemDay.value()));
        if (!resolved) [[unlikely]] {
            if (resolved.error().kind == TemporalErrorKind::TypeError)
                throwTypeError(globalObject, scope, String(resolved.error().message));
            else
                throwRangeError(globalObject, scope, String(resolved.error().message));
            return { };
        }
        auto calIdCopy = resolved->calendarId;
        RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(resolved->isoDate), WTF::move(calIdCopy))));
    }

    auto thisYear = yearMonth->year();
    auto thisMonth = yearMonth->month();
    auto plainDateResult = TemporalCore::regulateISODate(thisYear, thisMonth, itemDay.value(), TemporalOverflow::Constrain);
    if (!plainDateResult) [[unlikely]] {
        throwRangeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toPlainDate: date is invalid"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(*plainDateResult), yearMonth->calendarID())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toString called on value that's not a PlainYearMonth"_s);

    // Steps 3-5: GetOptionsObject + GetTemporalShowCalendarNameOption + TemporalYearMonthToString.
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, yearMonth->toString(globalObject, callFrame->argument(0)))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toJSON called on value that's not a PlainYearMonth"_s);

    // Step 3: Return TemporalYearMonthToString(yearMonth, ~auto~).
    return JSValue::encode(jsString(vm, yearMonth->toString()));
}

// https://tc39.es/proposal-temporal/#sup-temporal.plainyearmonth.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(callFrame->thisValue());
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.toLocaleString called on value that's not a PlainYearMonth"_s);

    // Steps 3-4: CreateDateTimeFormat(%DateTimeFormat%, locales, options, ~date~, ~date~) + FormatDateTime.
    JSValue locales = callFrame->argument(0);
    JSValue options = callFrame->argument(1);
    IntlDateTimeFormat* formatter;
    if (locales.isUndefined() && options.isUndefined())
        formatter = globalObject->defaultDateFormat();
    else {
        formatter = IntlDateTimeFormat::create(vm, globalObject->dateTimeFormatStructure());
        formatter->initializeDateTimeFormat(globalObject, locales, options, IntlDateTimeFormat::RequiredComponent::Date, IntlDateTimeFormat::Defaults::Date);
    }
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(formatter->format(globalObject, callFrame->thisValue())));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalPlainYearMonthPrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Throw a TypeError exception.
    return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.valueOf must not be called. To compare PlainYearMonth values, use Temporal.PlainYearMonth.compare"_s);
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.calendarid
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.calendar called on value that's not a PlainYearMonth"_s);

    // Step 3: Return CanonicalCalendarIdentifierOf(yearMonth.[[Calendar]]).
    return JSValue::encode(jsString(vm, yearMonth->calendarIDAsString()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.year
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.year called on value that's not a PlainYearMonth"_s);

    // Step 3: Return 𝔽(CalendarISOToDate(calendar, isoDate).[[Year]]).
    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarYear(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(yearMonth->year()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.month
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.month called on value that's not a PlainYearMonth"_s);

    // Step 3: Return 𝔽(CalendarISOToDate(calendar, isoDate).[[Month]]).
    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarMonth(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(yearMonth->month()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.monthcode
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.monthCode called on value that's not a PlainYearMonth"_s);

    // Step 3: Return CalendarISOToDate(calendar, isoDate).[[MonthCode]].
    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarMonthCode(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNontrivialString(vm, *result));
    }
    return JSValue::encode(jsNontrivialString(vm, yearMonth->monthCode()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.daysinmonth
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInMonth, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.daysInMonth called on value that's not a PlainYearMonth"_s);

    // Step 3: Return 𝔽(CalendarISOToDate(calendar, isoDate).[[DaysInMonth]]).
    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarDaysInMonth(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(ISO8601::daysInMonth(yearMonth->year(), yearMonth->month())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.daysinyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterDaysInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.daysInYear called on value that's not a PlainYearMonth"_s);

    // Step 3: Return 𝔽(CalendarISOToDate(calendar, isoDate).[[DaysInYear]]).
    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarDaysInYear(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(isLeapYear(yearMonth->year()) ? 366 : 365));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.monthsinyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterMonthsInYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.monthsInYear called on value that's not a PlainYearMonth"_s);

    // Step 3: Return 𝔽(CalendarISOToDate(calendar, isoDate).[[MonthsInYear]]).
    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarMonthsInYear(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    // ISO calendar: always 12 months per year.
    return JSValue::encode(jsNumber(12));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.inleapyear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterInLeapYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.inLeapYear called on value that's not a PlainYearMonth"_s);

    // Step 3: Return CalendarISOToDate(calendar, isoDate).[[InLeapYear]].
    if (!TemporalCore::calendarIsISO(yearMonth->calendarID())) {
        auto result = TemporalCore::calendarInLeapYear(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsBoolean(*result));
    }
    return JSValue::encode(jsBoolean(isLeapYear(yearMonth->year())));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.era
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterEra, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.era called on value that's not a PlainYearMonth"_s);

    // Steps 3-4: result = CalendarISOToDate(calendar, isoDate).[[Era]]; if undefined, return undefined.
    auto result = TemporalCore::calendarEra(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
    if (!result || !*result)
        return JSValue::encode(jsUndefined());
    // Step 5: Return result.
    return JSValue::encode(jsString(vm, **result));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainyearmonth.prototype.erayear
JSC_DEFINE_CUSTOM_GETTER(temporalPlainYearMonthPrototypeGetterEraYear, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: branding.
    auto* yearMonth = dynamicDowncast<TemporalPlainYearMonth>(JSValue::decode(thisValue));
    if (!yearMonth) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainYearMonth.prototype.eraYear called on value that's not a PlainYearMonth"_s);

    // Steps 3-4: result = CalendarISOToDate(calendar, isoDate).[[EraYear]]; if undefined, return undefined.
    auto result = TemporalCore::calendarEraYear(yearMonth->calendarID(), yearMonth->plainYearMonth().isoPlainDate());
    if (!result || !*result)
        return JSValue::encode(jsUndefined());
    // Step 5: Return 𝔽(result).
    return JSValue::encode(jsNumber(**result));
}

} // namespace JSC
