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

// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalmonthday
TemporalPlainMonthDay* TemporalPlainMonthDay::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isValidISODate(plainDate.year(), plainDate.month(), plainDate.day())) [[unlikely]] {
        throwRangeError(globalObject, scope, "PlainMonthDay: invalid date"_s);
        return { };
    }

    // Step 1: If ISODateWithinLimits(isoDate) is false, throw RangeError.
    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
        throwRangeError(globalObject, scope, "PlainMonthDay: date out of range of ECMAScript representation"_s);
        return { };
    }

    // Steps 2-6: OrdinaryCreateFromConstructor + set internal slots.
    return TemporalPlainMonthDay::create(vm, structure, ISO8601::PlainMonthDay(WTF::move(plainDate)));
}

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

        // Steps 5-7: calendar = result.[[Calendar]] (default "iso8601"); calendar = ? CanonicalizeCalendar(calendar).
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

        // Step 8-9: GetOptionsObject + GetTemporalOverflowOption (validate; overflow unused for strings).
        //           Spec runs these AFTER parsing; user's options object is validated but its value is unused.
        if (!optionsValue.isUndefined()) {
            JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            toTemporalOverflow(globalObject, options);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // Step 10 (ISO path): referenceISOYear=1972 → CreateTemporalMonthDay({1972,month,day}, iso8601).
        if (calendarId == iso8601CalendarID()) {
            auto dateWithoutYear = ISO8601::PlainDate(1972, plainDate.month(), plainDate.day());
            RELEASE_AND_RETURN(scope, TemporalPlainMonthDay::tryCreateIfValid(globalObject, globalObject->plainMonthDayStructure(), WTF::move(dateWithoutYear)));
        }

        // Steps 11-12 (non-ISO path): isoDate = {year,month,day}; ISODateWithinLimits check.
        //   For short form (no year), plainDate.year() defaults to a parser sentinel; for full form,
        //   it's the parsed year.
        int32_t fullYear = plainDate.year();
        if (!ISO8601::isYearWithinLimits(fullYear) || !ISO8601::isDateTimeWithinLimits(fullYear, plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) [[unlikely]] {
            throwRangeError(globalObject, scope, "Date is not within ISO date time limits"_s);
            return { };
        }

        // Steps 13-15: ISODateToFields + CalendarMonthDayFromFields(~constrain~) + CreateTemporalMonthDay.
        //              Fused into plainMonthDayFromISODate for non-ISO full date strings.
        if (!looksLikeShortForm && ISO8601::isYearWithinLimits(plainDate.year())) {
            auto resolved = TemporalCore::plainMonthDayFromISODate(calendarId, plainDate, TemporalOverflow::Constrain);
            if (!resolved) [[unlikely]] {
                throwRangeError(globalObject, scope, String(resolved.error().message));
                return { };
            }
            auto* result = TemporalPlainMonthDay::create(vm, globalObject->plainMonthDayStructure(), WTF::move(resolved->isoDate));
                result->setCalendarID(resolved->calendarId);
            return result;
        }

        String message = tryMakeString("Temporal.PlainMonthDay.from: invalid date string "_s, string);
        throwRangeError(globalObject, scope, message.isNull() ? "Temporal.PlainMonthDay.from: invalid date string"_s : message);
        return { };
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

} // namespace JSC
