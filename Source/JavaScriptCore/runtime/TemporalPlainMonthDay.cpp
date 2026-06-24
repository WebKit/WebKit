/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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
#include "TemporalPlainMonthDay.h"

#include "CalendarFields.h"
#include "CalendarICUBridge.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "TemporalCalendar.h"
#include "TemporalPlainDate.h"

namespace JSC {

const ClassInfo TemporalPlainMonthDay::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainMonthDay) };

TemporalPlainMonthDay* TemporalPlainMonthDay::create(VM& vm, Structure* structure, ISO8601::PlainMonthDay&& plainMonthDay)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainMonthDay>(vm)) TemporalPlainMonthDay(vm, structure, WTF::move(plainMonthDay));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainMonthDay::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainMonthDay::TemporalPlainMonthDay(VM& vm, Structure* structure, ISO8601::PlainMonthDay&& plainMonthDay)
    : Base(vm, structure)
    , m_plainMonthDay(WTF::move(plainMonthDay))
    , m_calendarID(iso8601CalendarID())
{
}


// CreateTemporalMonthDay ( isoDate, calendar [, newTarget ]
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalmonthday
TemporalPlainMonthDay* TemporalPlainMonthDay::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isValidISODate(plainDate.year(), plainDate.month(), plainDate.day())) [[unlikely]] {
        throwRangeError(globalObject, scope, "PlainMonthDay: invalid date"_s);
        return { };
    }

    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
        throwRangeError(globalObject, scope, "PlainMonthDay: date out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainMonthDay::create(vm, structure, ISO8601::PlainMonthDay(WTF::move(plainDate)));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.with
String TemporalPlainMonthDay::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        return toString();

    String calendarName = temporalShowCalendarName(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    return ISO8601::temporalMonthDayToString(m_plainMonthDay, calendarName, m_calendarID);
}

static TemporalPlainMonthDay* fromMonthDayString(JSGlobalObject*, WTF::String);

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.from
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalmonthday
TemporalPlainMonthDay* TemporalPlainMonthDay::from(JSGlobalObject* globalObject, JSValue itemValue, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: options defaults to undefined (handled by caller passing jsUndefined()).

    // Steps 4-14 (String path) checked first so RangeError from parsing precedes
    // TypeError from options — see spec step ordering note.
    if (itemValue.isString()) {
        auto string = itemValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        // Steps 4-14: ParseISODateTime → canonicalize calendar → CalendarMonthDayFromFields.
        auto* result = fromMonthDayString(globalObject, string);
        RETURN_IF_EXCEPTION(scope, { });
        // Step 8-9: GetOptionsObject + GetTemporalOverflowOption (validate; overflow unused for strings).
        if (!optionsValue.isUndefined()) {
            JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            toTemporalOverflow(globalObject, options);
            RETURN_IF_EXCEPTION(scope, { });
        }
        return result;
    }

    // Step 2: item is an Object.
    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainMonthDay>()) {
            // Step 2.a.i: GetOptionsObject + GetTemporalOverflowOption (validate), return new CreateTemporalMonthDay.
            if (!optionsValue.isUndefined()) {
                auto* opts = intlGetOptionsObject(globalObject, optionsValue);
                RETURN_IF_EXCEPTION(scope, { });
                if (opts) {
                    toTemporalOverflow(globalObject, opts);
                    RETURN_IF_EXCEPTION(scope, { });
                }
            }
            auto* existing = uncheckedDowncast<TemporalPlainMonthDay>(itemValue);
            auto* cloned = TemporalPlainMonthDay::create(vm, globalObject->plainMonthDayStructure(), existing->plainMonthDay());
            if (existing->calendarID() != iso8601CalendarID())
                cloned->setCalendarID(existing->calendarID());
            return cloned;
        }

        // Step 2.b: GetTemporalCalendarIdentifierWithISODefault(item).
        // Step 2.c: PrepareCalendarFields(calendar, item, {year,month,monthCode,day}, {}, {}).
        // (Steps 2.b-c fused into readCalendarFieldsFromObject.)
        CalendarID calendarId = iso8601CalendarID();
        auto fields = readCalendarFieldsFromObject<FieldSetType::MonthDay>(globalObject, asObject(itemValue), calendarId);
        RETURN_IF_EXCEPTION(scope, { });

        // Steps 2.d-e: GetOptionsObject + GetTemporalOverflowOption (after fields per spec).
        JSObject* opts = nullptr;
        if (!optionsValue.isUndefined()) {
            if (!optionsValue.isObject()) [[unlikely]] {
                throwTypeError(globalObject, scope, "options must be an object"_s);
                return { };
            }
            opts = asObject(optionsValue);
        }
        auto overflow = TemporalOverflow::Constrain;
        if (opts) {
            overflow = toTemporalOverflow(globalObject, opts);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // Step 2.f: CalendarMonthDayFromFields(calendar, fields, overflow).
        // Step 2.g: Return CreateTemporalMonthDay(isoDate, calendar).
        auto resolved = TemporalCore::monthDayFromFields(calendarId, fields, overflow);
        if (!resolved) [[unlikely]] {
            if (resolved.error().kind == TemporalErrorKind::TypeError)
                throwTypeError(globalObject, scope, String(resolved.error().message));
            else
                throwRangeError(globalObject, scope, String(resolved.error().message));
            return { };
        }

        auto* result = TemporalPlainMonthDay::create(vm, globalObject->plainMonthDayStructure(), WTF::move(resolved->isoDate));
        result->setCalendarID(resolved->calendarId);
        return result;
    }

    // Step 3: item is not a String — throw TypeError.
    throwTypeError(globalObject, scope, "can only convert to PlainMonthDay from object or string values"_s);
    return { };
}

// Implements ToTemporalMonthDay steps 4-14 (steps 8-9 done by caller after return):
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalmonthday
static TemporalPlainMonthDay* fromMonthDayString(JSGlobalObject* globalObject, WTF::String string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 4: ParseISODateTime(item, « TemporalMonthDayString »).
    auto dateTime = ISO8601::parseISODateTime(string, ISO8601::TemporalProduction::MonthDay);
    if (!dateTime) [[unlikely]] {
        String message = tryMakeString("Temporal.PlainMonthDay.from: invalid date string "_s, string);
        throwRangeError(globalObject, scope, message.isNull() ? "Temporal.PlainMonthDay.from: invalid date string"_s : message);
        return { };
    }
    auto [plainDateOpt, plainTimeOptional, timeZoneOptional, calendarOptional, matched, isShortForm] = WTF::move(*dateTime);
    ASSERT(plainDateOpt);
    auto plainDate = WTF::move(*plainDateOpt);

    // (parseISODateTime already enforced Step 4.a.ii.(4): short-form non-iso8601 → nullopt.)
    bool looksLikeShortForm = isShortForm;

    // Step 5: Let calendar be result.[[Calendar]].
    // Step 6: If calendar is ~empty~, set calendar to "iso8601".
    // Step 7: Set calendar to ? CanonicalizeCalendar(calendar).
    CalendarID calendarId = iso8601CalendarID();
    if (calendarOptional) {
        auto rawCal = StringView(*calendarOptional).convertToASCIILowercase();
        auto canonicalized = isBuiltinCalendar(rawCal);
        if (!canonicalized) [[unlikely]] {
            throwRangeError(globalObject, scope, makeString("'"_s, rawCal, "' is not a valid calendar identifier"_s));
            return { };
        }
        calendarId = *canonicalized;
    }

    // Step 10: iso8601 path — referenceISOYear=1972, return CreateTemporalMonthDay({1972,month,day}, iso8601).
    if (calendarId == iso8601CalendarID()) {
        auto dateWithoutYear = ISO8601::PlainDate(1972, plainDate.month(), plainDate.day());
        RELEASE_AND_RETURN(scope, TemporalPlainMonthDay::tryCreateIfValid(globalObject, globalObject->plainMonthDayStructure(), WTF::move(dateWithoutYear)));
    }

    // Steps 11-12: isoDate = {year,month,day}; ISODateWithinLimits check.
    // For short form (no year), plainDate.year() defaults to a parser sentinel; for full form,
    // it's the parsed year.
    int32_t fullYear = plainDate.year();
    if (!ISO8601::isYearWithinLimits(fullYear) || !ISO8601::isDateTimeWithinLimits(fullYear, plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
        throwRangeError(globalObject, scope, "Date is not within ISO date time limits"_s);
        return { };
    }

    // Steps 13-15: ISODateToFields + CalendarMonthDayFromFields(~constrain~) + CreateTemporalMonthDay.
    // (Fused into plainMonthDayFromISODate for non-ISO full date strings.)
    if (!looksLikeShortForm) {
        if (ISO8601::isYearWithinLimits(plainDate.year())) {
            auto resolved = TemporalCore::plainMonthDayFromISODate(calendarId, plainDate, TemporalOverflow::Constrain);
            if (!resolved) [[unlikely]] {
                throwRangeError(globalObject, scope, String(resolved.error().message));
                return { };
            }
            auto* result = TemporalPlainMonthDay::create(vm, globalObject->plainMonthDayStructure(), WTF::move(resolved->isoDate));
            result->setCalendarID(resolved->calendarId);
            return result;
        }
    }

    String message = tryMakeString("Temporal.PlainMonthDay.from: invalid date string "_s, string);
    throwRangeError(globalObject, scope, message.isNull() ? "Temporal.PlainMonthDay.from: invalid date string"_s : message);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.with
ISO8601::PlainDate TemporalPlainMonthDay::with(JSGlobalObject* globalObject, JSObject* temporalMonthDayLike, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Steps 1-2: RequireInternalSlot + type check done by prototype caller.

    // Step 3: IsPartialTemporalObject.
    rejectObjectWithCalendarOrTimeZone(globalObject, temporalMonthDayLike);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 4: calendar = this.[[Calendar]] — m_calendarID.
    auto calID = m_calendarID;

    // Step 6: PrepareCalendarFields(calendar, temporalMonthDayLike, {year,month,monthCode,day}, {}, ~partial~).
    // Fields read BEFORE options per spec. skipCalendarRead=true since step 3 ensures no calendar property.
    CalendarID outCalendarId = calID;
    auto partialFields = readCalendarFieldsFromObject<FieldSetType::MonthDay, CalendarRead::Skip>(globalObject, temporalMonthDayLike, outCalendarId);
    RETURN_IF_EXCEPTION(scope, { });
    if (!partialFields.day && !partialFields.month && !partialFields.monthCode
        && !partialFields.year && !partialFields.era && !partialFields.eraYear) [[unlikely]] {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);
        return { };
    }

    // Steps 8-9: GetOptionsObject + GetTemporalOverflowOption (after fields per spec order).
    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });
    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 5: ISODateToFields(calendar, this.[[ISODate]], ~month-day~) — get current fields for merge.
    // Step 7: CalendarMergeFields(calendar, fields, partialMonthDay) — merge current + user fields.
    // (Steps 5, 7 fused into explicit merge below.)
    TemporalCore::CalendarFieldsIn merged;

    // Step 5: current day from ISODateToFields (use calendar day for non-ISO).
    uint8_t currentCalDay = static_cast<uint8_t>(m_plainMonthDay.day());
    if (!TemporalCore::calendarIsISO(calID)) {
        auto dayResult = TemporalCore::calendarDay(calID, m_plainMonthDay.isoPlainDate());
        if (dayResult)
            currentCalDay = *dayResult;
    }
    // Step 7: merge day — user's value takes priority over current.
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
        // Step 7: fall back to current monthCode from ISODateToFields.
        if (!TemporalCore::calendarIsISO(calID)) {
            auto mcStr = TemporalCore::calendarMonthCode(calID, m_plainMonthDay.isoPlainDate());
            if (!mcStr) [[unlikely]] {
                throwRangeError(globalObject, scope, String(mcStr.error().message));
                return { };
            }
            merged.monthCode = ISO8601::parseMonthCode(*mcStr);
        } else
            merged.month = std::optional<uint32_t>(m_plainMonthDay.month());
    }

    // For non-ISO: month ordinal alone is ambiguous without monthCode (depends on year).
    if (!TemporalCore::calendarIsISO(calID) && merged.month.has_value() && !merged.monthCode) [[unlikely]] {
        throwTypeError(globalObject, scope, "monthCode is required for non-ISO calendar PlainMonthDay.with()"_s);
        return { };
    }

    // Step 10: CalendarMonthDayFromFields(calendar, mergedFields, overflow).
    // Step 11: CreateTemporalMonthDay — done by prototype caller from the returned isoDate.
    auto resolved = TemporalCore::monthDayFromFields(calID, merged, overflow);
    if (!resolved) [[unlikely]] {
        if (resolved.error().kind == TemporalErrorKind::TypeError)
            throwTypeError(globalObject, scope, String(resolved.error().message));
        else
            throwRangeError(globalObject, scope, String(resolved.error().message));
        return { };
    }
    return resolved->isoDate;
}

} // namespace JSC
