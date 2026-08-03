/*
 * Copyright (C) 2025 Igalia, S.L.
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
#include "TemporalPlainMonthDayPrototype.h"

#include "CalendarFields.h"
#include "CalendarICUBridge.h"
#include "ISOArithmetic.h"
#include "IntlDateTimeFormat.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "TemporalCalendar.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainTime.h"
namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToPlainDate);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToJSON);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncWith);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncEquals);
static JSC_DECLARE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncValueOf);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterCalendarId);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterDay);
static JSC_DECLARE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterMonthCode);

}

#include "TemporalPlainMonthDayPrototype.lut.h"
namespace JSC {

const ClassInfo TemporalPlainMonthDayPrototype::s_info = { "Temporal.PlainMonthDay"_s, &Base::s_info, &plainMonthDayPrototypeTable, nullptr, CREATE_METHOD_TABLE(TemporalPlainMonthDayPrototype) };

/* Source for TemporalPlainMonthDayPrototype.lut.h
@begin plainMonthDayPrototypeTable
  toPlainDate      temporalPlainMonthDayPrototypeFuncToPlainDate        DontEnum|Function 1
  toString         temporalPlainMonthDayPrototypeFuncToString           DontEnum|Function 0
  toJSON           temporalPlainMonthDayPrototypeFuncToJSON             DontEnum|Function 0
  toLocaleString   temporalPlainMonthDayPrototypeFuncToLocaleString     DontEnum|Function 0
  with             temporalPlainMonthDayPrototypeFuncWith               DontEnum|Function 1
  equals           temporalPlainMonthDayPrototypeFuncEquals             DontEnum|Function 1
  valueOf          temporalPlainMonthDayPrototypeFuncValueOf            DontEnum|Function 0
  calendarId       temporalPlainMonthDayPrototypeGetterCalendarId       DontEnum|ReadOnly|CustomAccessor
  day              temporalPlainMonthDayPrototypeGetterDay              DontEnum|ReadOnly|CustomAccessor
  monthCode        temporalPlainMonthDayPrototypeGetterMonthCode        DontEnum|ReadOnly|CustomAccessor
@end
*/

TemporalPlainMonthDayPrototype* TemporalPlainMonthDayPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* prototype = new (NotNull, allocateCell<TemporalPlainMonthDayPrototype>(vm)) TemporalPlainMonthDayPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

Structure* TemporalPlainMonthDayPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainMonthDayPrototype::TemporalPlainMonthDayPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void TemporalPlainMonthDayPrototype::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.tostring
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this-value + branding.
    auto* monthDay = dynamicDowncast<TemporalPlainMonthDay>(callFrame->thisValue());
    if (!monthDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toString called on value that's not a PlainMonthDay"_s);

    // Step 3: resolvedOptions = ? GetOptionsObject(options).
    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    // Fast path: no options → default showCalendar = "auto" (matches the no-arg toString()).
    if (!options)
        RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, monthDay->toString())));

    // Step 4: showCalendar = ? GetTemporalShowCalendarNameOption(resolvedOptions).
    String calendarName = temporalShowCalendarName(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 5: Return TemporalMonthDayToString(plainMonthDay, showCalendar).
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, ISO8601::temporalMonthDayToString(monthDay->plainMonthDay(), calendarName, monthDay->calendarID()))));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.tojson
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToJSON, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = dynamicDowncast<TemporalPlainMonthDay>(callFrame->thisValue());
    if (!monthDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toJSON called on value that's not a PlainMonthDay"_s);

    return JSValue::encode(jsString(vm, monthDay->toString()));
}

// https://tc39.es/proposal-temporal/#sup-temporal.plainmonthday.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = dynamicDowncast<TemporalPlainMonthDay>(callFrame->thisValue());
    if (!monthDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toLocaleString called on value that's not a PlainMonthDay"_s);

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

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.with
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this-value + branding.
    auto* monthDay = dynamicDowncast<TemporalPlainMonthDay>(callFrame->thisValue());
    if (!monthDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.with called on value that's not a PlainMonthDay"_s);

    // Step 3: If ? IsPartialTemporalObject(temporalMonthDayLike) is false, throw TypeError.
    //   Rejects non-Objects, Temporal instances, and objects carrying calendar/timeZone properties.
    JSValue temporalMonthDayLike = callFrame->argument(0);
    bool isPartial = isPartialTemporalObject(globalObject, temporalMonthDayLike);
    RETURN_IF_EXCEPTION(scope, { });
    if (!isPartial) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "First argument to Temporal.PlainMonthDay.prototype.with must be a partial Temporal object"_s);
    JSObject* like = asObject(temporalMonthDayLike);

    // Step 4: calendar = plainMonthDay.[[Calendar]] — held on the receiver.
    CalendarID calendarId = monthDay->calendarID();

    // Step 6: partialMonthDay = ? PrepareCalendarFields(calendar, temporalMonthDayLike,
    //         «year, month, monthCode, day», «», ~partial~).
    // Calendar comes from the receiver — Step 3 already rejected a `calendar` property.
    auto partialFields = readCalendarFieldsFromObject<FieldSetType::MonthDay>(globalObject, like, calendarId);
    RETURN_IF_EXCEPTION(scope, { });
    // ~partial~ throws TypeError if none of the requested fields are present with a non-undefined value.
    if (!partialFields.day && !partialFields.month && !partialFields.monthCode
        && !partialFields.year && !partialFields.era && !partialFields.eraYear) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);

    // Steps 8-9: resolvedOptions = ? GetOptionsObject(options); overflow = ? GetTemporalOverflowOption(resolvedOptions).
    JSObject* options = intlGetOptionsObject(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });
    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Steps 5+7: fields = ISODateToFields(calendar, plainMonthDay.[[ISODate]], ~month-day~);
    //            fields = CalendarMergeFields(calendar, fields, partialMonthDay).
    //   Fused inline: fill unset merged fields from the receiver's own monthCode/day.
    TemporalCore::CalendarFieldsIn merged;

    // day from ISODateToFields (use calendar day for non-ISO).
    uint8_t currentCalDay = static_cast<uint8_t>(monthDay->plainMonthDay().day());
    bool isISO = TemporalCore::calendarIsISO(calendarId);
    if (!isISO) {
        auto dayResult = TemporalCore::calendarDay(calendarId, monthDay->plainMonthDay().isoPlainDate());
        if (dayResult)
            currentCalDay = *dayResult;
    }
    // User's day takes priority over the receiver's.
    merged.day = partialFields.day.has_value() ? partialFields.day : std::optional<uint8_t>(currentCalDay);

    if (partialFields.month.has_value())
        merged.month = partialFields.month;
    if (partialFields.monthCode)
        merged.monthCode = partialFields.monthCode;
    if (partialFields.year)
        merged.year = partialFields.year;
    if (partialFields.era)
        merged.era = partialFields.era;
    if (partialFields.eraYear)
        merged.eraYear = partialFields.eraYear;
    if (!partialFields.month.has_value() && !partialFields.monthCode) {
        // Neither given — fall back to current monthCode (non-ISO) or numeric month (ISO).
        if (!isISO) {
            auto mcStr = TemporalCore::calendarMonthCode(calendarId, monthDay->plainMonthDay().isoPlainDate());
            if (!mcStr) [[unlikely]] {
                throwRangeError(globalObject, scope, String(mcStr.error().message));
                return { };
            }
            merged.monthCode = ISO8601::parseMonthCode(*mcStr);
        } else
            merged.month = std::optional<uint32_t>(monthDay->plainMonthDay().month());
    }

    if (!isISO && merged.month.has_value() && !merged.year && !(merged.era && merged.eraYear)) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "year is required with month for non-ISO calendar PlainMonthDay.with()"_s);

    // Step 10: isoDate = ? CalendarMonthDayFromFields(calendar, fields, overflow).
    auto resolved = TemporalCore::monthDayFromFields(calendarId, merged, overflow);
    if (!resolved) [[unlikely]] {
        if (resolved.error().kind == TemporalErrorKind::TypeError)
            throwTypeError(globalObject, scope, String(resolved.error().message));
        else
            throwRangeError(globalObject, scope, String(resolved.error().message));
        return { };
    }

    // Step 11: Return ! CreateTemporalMonthDay(isoDate, calendar).
    auto* withResult = TemporalPlainMonthDay::tryCreateIfValid(globalObject, globalObject->plainMonthDayStructure(), WTF::move(resolved->isoDate));
    RETURN_IF_EXCEPTION(scope, { });
    if (withResult && calendarId != iso8601CalendarID())
        withResult->setCalendarID(calendarId);
    return JSValue::encode(withResult);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.equals
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncEquals, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this-value + branding.
    auto* monthDay = dynamicDowncast<TemporalPlainMonthDay>(callFrame->thisValue());
    if (!monthDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.equals called on value that's not a PlainMonthDay"_s);

    // Step 3: other = ? ToTemporalMonthDay(other).
    auto* other = TemporalPlainMonthDay::from(globalObject, callFrame->argument(0), jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: If CompareISODate(monthDay.[[ISODate]], other.[[ISODate]]) != 0, return false.
    if (monthDay->plainMonthDay() != other->plainMonthDay())
        return JSValue::encode(jsBoolean(false));

    // Step 5: Return CalendarEquals(monthDay.[[Calendar]], other.[[Calendar]]).
    return JSValue::encode(jsBoolean(monthDay->calendarID() == other->calendarID()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.toplaindate
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncToPlainDate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: this-value + branding.
    auto* monthDay = dynamicDowncast<TemporalPlainMonthDay>(callFrame->thisValue());
    if (!monthDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toPlainDate called on value that's not a PlainMonthDay"_s);

    // Step 3: If item is not an Object, throw TypeError.
    JSValue itemValue = callFrame->argument(0);
    if (!itemValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toPlainDate: item is not an object"_s);
    JSObject* item = asObject(itemValue);

    // Step 4: calendar = plainMonthDay.[[Calendar]] — held on the receiver.
    CalendarID calendarId = monthDay->calendarID();

    // Steps 5-7 (fused): fields ← ISODateToFields(receiver, month-day);
    //   inputFields ← ? PrepareCalendarFields(calendar, item, «year», «», «»);
    //   merged ← CalendarMergeFields(...).
    //   Step 6 reads ONLY the fields spec says to read — day/monthCode come from the receiver at
    //   Step 5, so we must NOT re-read them from `item` (test262 order-of-operations pins this).
    //   CalendarExtraFields expands «year» to «year, era, eraYear» for era-based calendars.
    //   Alphabetical read order: era, eraYear, year.
    TemporalCore::CalendarFieldsIn merged;
    bool calHasEras = TemporalCore::calendarHasEras(calendarId);
    if (calHasEras) {
        JSValue eraProp = item->get(globalObject, Identifier::fromString(vm, "era"_s));
        RETURN_IF_EXCEPTION(scope, { });
        if (!eraProp.isUndefined()) {
            merged.era = eraProp.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
        }
        JSValue eraYearProp = item->get(globalObject, Identifier::fromString(vm, "eraYear"_s));
        RETURN_IF_EXCEPTION(scope, { });
        if (!eraYearProp.isUndefined()) {
            double ey = eraYearProp.toIntegerWithTruncation(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (!std::isfinite(ey)) [[unlikely]]
                return throwVMRangeError(globalObject, scope, "eraYear must be finite"_s);
            merged.eraYear = clampTo<int32_t>(ey);
        }
    }
    JSValue yearProp = item->get(globalObject, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, { });
    if (!yearProp.isUndefined()) {
        double y = yearProp.toIntegerWithTruncation(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!std::isfinite(y)) [[unlikely]]
            return throwVMRangeError(globalObject, scope, "year must be finite"_s);
        merged.year = clampTo<int32_t>(y);
    }
    // Merge requires a year identifier — either `year` or era+eraYear pair.
    bool hasEraPair = merged.era.has_value() && merged.eraYear.has_value();
    if (!merged.year && !hasEraPair) [[unlikely]] {
        throwTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.toPlainDate: item does not have a year or era/eraYear field"_s);
        return { };
    }

    // Step 5 (receiver side): populate monthCode + day from the PMD's stored ISO date.
    bool isISO = TemporalCore::calendarIsISO(calendarId);
    if (isISO) {
        merged.month = monthDay->month();
        merged.day = monthDay->day();
    } else {
        auto mcStr = TemporalCore::calendarMonthCode(calendarId, monthDay->plainMonthDay().isoPlainDate());
        if (!mcStr) [[unlikely]] {
            throwRangeError(globalObject, scope, String(mcStr.error().message));
            return { };
        }
        merged.monthCode = ISO8601::parseMonthCode(*mcStr);
        auto dayResult = TemporalCore::calendarDay(calendarId, monthDay->plainMonthDay().isoPlainDate());
        if (!dayResult) [[unlikely]] {
            throwRangeError(globalObject, scope, String(dayResult.error().message));
            return { };
        }
        merged.day = static_cast<uint8_t>(*dayResult);
    }

    // Step 8: isoDate = ? CalendarDateFromFields(calendar, merged, ~constrain~).
    auto resolved = TemporalCore::dateFromFields(calendarId, merged, TemporalOverflow::Constrain);
    if (!resolved) [[unlikely]] {
        if (resolved.error().kind == TemporalErrorKind::TypeError)
            throwTypeError(globalObject, scope, String(resolved.error().message));
        else
            throwRangeError(globalObject, scope, String(resolved.error().message));
        return { };
    }

    // Step 9: Return ! CreateTemporalDate(isoDate, calendar).
    RELEASE_AND_RETURN(scope, JSValue::encode(TemporalPlainDate::tryCreateIfValid(globalObject, globalObject->plainDateStructure(), WTF::move(resolved->isoDate), resolved->calendarId)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.valueof
JSC_DEFINE_HOST_FUNCTION(temporalPlainMonthDayPrototypeFuncValueOf, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.valueOf must not be called. To compare PlainMonthDay values, use Temporal.PlainDate.compare on the corresponding PlainDate objects."_s);
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainmonthday.prototype.calendarid
JSC_DEFINE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterCalendarId, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = dynamicDowncast<TemporalPlainMonthDay>(JSValue::decode(thisValue));
    if (!monthDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.calendar called on value that's not a PlainMonthDay"_s);

    return JSValue::encode(jsString(vm, monthDay->calendarIDAsString()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainmonthday.prototype.day
JSC_DEFINE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterDay, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = dynamicDowncast<TemporalPlainMonthDay>(JSValue::decode(thisValue));
    if (!monthDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.day called on value that's not a PlainMonthDay"_s);

    if (!TemporalCore::calendarIsISO(monthDay->calendarID())) {
        auto result = TemporalCore::calendarDay(monthDay->calendarID(), monthDay->plainMonthDay().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNumber(*result));
    }
    return JSValue::encode(jsNumber(monthDay->day()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.plainmonthday.prototype.monthcode
JSC_DEFINE_CUSTOM_GETTER(temporalPlainMonthDayPrototypeGetterMonthCode, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* monthDay = dynamicDowncast<TemporalPlainMonthDay>(JSValue::decode(thisValue));
    if (!monthDay) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Temporal.PlainMonthDay.prototype.monthCode called on value that's not a PlainMonthDay"_s);

    if (!TemporalCore::calendarIsISO(monthDay->calendarID())) {
        auto result = TemporalCore::calendarMonthCode(monthDay->calendarID(), monthDay->plainMonthDay().isoPlainDate());
        if (!result) [[unlikely]]
            return throwVMRangeError(globalObject, scope, result.error().message);
        return JSValue::encode(jsNontrivialString(vm, *result));
    }
    return JSValue::encode(jsNontrivialString(vm, monthDay->monthCode()));
}

} // namespace JSC
