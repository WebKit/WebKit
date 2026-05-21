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

void TemporalPlainMonthDay::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

// CreateTemporalMonthDay ( isoDate, calendar [, newTarget ]
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalmonthday
TemporalPlainMonthDay* TemporalPlainMonthDay::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isValidISODate(plainDate.year(), plainDate.month(), plainDate.day())) {
        throwRangeError(globalObject, scope, "PlainMonthDay: invalid date"_s);
        return { };
    }

    if (!ISO8601::isDateTimeWithinLimits(plainDate.year(), plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) {
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

    String calendarName = toTemporalCalendarName(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    return ISO8601::temporalMonthDayToString(m_plainMonthDay, calendarName, TemporalCore::calendarIDToString(m_calendarID));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.from
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalmonthday
TemporalPlainMonthDay* TemporalPlainMonthDay::from(JSGlobalObject* globalObject, JSValue itemValue, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // Handle string case first so that string parsing errors (RangeError)
    // can be thrown before options-related errors (TypeError);
    // see step 4 of ToTemporalMonthDay
    TemporalPlainMonthDay* result;
    if (itemValue.isString()) {
        auto string = itemValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        result = TemporalPlainMonthDay::from(globalObject, string);
        RETURN_IF_EXCEPTION(scope, { });
        // Overflow has to be validated even though it's not used;
        // see step 9 of ToTemporalMonthDay
        if (!optionsValue.isUndefined()) {
            JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
            toTemporalOverflow(globalObject, options);
            RETURN_IF_EXCEPTION(scope, { });
        }
        return result;
    }

    // For property bags, fields must be read before options validation (spec order).
    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainMonthDay>()) {
            auto* existing = uncheckedDowncast<TemporalPlainMonthDay>(itemValue);
            auto* cloned = TemporalPlainMonthDay::create(vm, globalObject->plainMonthDayStructure(), existing->plainMonthDay());
            if (existing->calendarId() != "iso8601"_s && !existing->calendarId().isEmpty())
                cloned->setCalendarId(String(existing->calendarId()));
            return cloned;
        }

        String calendarId;
        auto fields = readCalendarFieldsFromObject(globalObject, asObject(itemValue), calendarId, FieldSetType::MonthDay);
        RETURN_IF_EXCEPTION(scope, { });

        // Options validated AFTER fields (spec order).
        JSObject* opts = nullptr;
        if (!optionsValue.isUndefined()) {
            if (!optionsValue.isObject()) {
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

        auto resolved = TemporalCore::monthDayFromFields(TemporalCore::calendarIDFromString(calendarId), fields, overflow);
        if (!resolved) {
            if (resolved.error().kind == TemporalErrorKind::TypeError)
                throwTypeError(globalObject, scope, String(resolved.error().message));
            else
                throwRangeError(globalObject, scope, String(resolved.error().message));
            return { };
        }

        auto* result = TemporalPlainMonthDay::create(vm, globalObject->plainMonthDayStructure(), WTF::move(resolved->isoDate));
        if (resolved->calendarId != "iso8601"_s)
            result->setCalendarId(resolved->calendarId);
        return result;
    }

    throwTypeError(globalObject, scope, "can only convert to PlainMonthDay from object or string values"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.from
TemporalPlainMonthDay* TemporalPlainMonthDay::from(JSGlobalObject* globalObject, WTF::String string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaldatestring
    // TemporalDateString :
    //     CalendarDateTime
    auto dateTime = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::MonthDay);
    if (dateTime) {
        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTF::move(dateTime.value());
        // MM-DD short format only valid with iso8601. Full dates with non-ISO are allowed.
        bool looksLikeShortForm = false;
        {
            unsigned digitGroups = 0;
            bool inDigits = false;
            for (unsigned j = 0; j < string.length() && string[j] != '['; j++) {
                if (isASCIIDigit(string[j])) {
                    if (!inDigits) {
                        digitGroups++;
                        inDigits = true;
                    }
                } else
                    inDigits = false;
            }
            looksLikeShortForm = (digitGroups <= 2);
        }
        if (looksLikeShortForm && calendarOptional && !WTF::equalIgnoringASCIICase(StringView(*calendarOptional), "iso8601"_s)) [[unlikely]] {
            throwRangeError(globalObject, scope,
                "PlainMonthDay string must use iso8601 calendar"_s);
            return { };
        }
        String calendarId = "iso8601"_s;
        if (calendarOptional) {
            auto rawCal = StringView(*calendarOptional).convertToASCIILowercase();
            auto canonicalized = isBuiltinCalendar(rawCal);
            if (!canonicalized) [[unlikely]] {
                throwRangeError(globalObject, scope, makeString("'"_s, rawCal, "' is not a valid calendar identifier"_s));
                return { };
            }
            calendarId = intlAvailableCalendars().at(*canonicalized);
        }
        // For non-ISO calendars, validate the full date is within ISO limits.
        // The MonthDay parser may truncate the year, so check the original parsed year.
        if (calendarId != "iso8601"_s) {
            // Re-parse as Date to get the full year.
            auto dateParse = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::Date);
            int32_t fullYear = plainDate.year();
            if (dateParse)
                fullYear = std::get<0>(dateParse.value()).year();
            if (!ISO8601::isYearWithinLimits(fullYear) || !ISO8601::isDateTimeWithinLimits(fullYear, plainDate.month(), plainDate.day(), 12, 0, 0, 0, 0, 0)) {
                throwRangeError(globalObject, scope, "Date is not within ISO date time limits"_s);
                return { };
            }
        }
        if (!(timeZoneOptional && timeZoneOptional->m_z)) {
            if (calendarId != "iso8601"_s && !looksLikeShortForm) {
                // Non-ISO full date string: re-parse to get actual ISO date, then
                // resolve calendar monthCode+day and find reference ISO year.
                // Matches temporal_rs from_parsed() steps 12-14.
                auto fullDateTime = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::Date);
                if (fullDateTime) {
                    auto& fullDate = std::get<0>(fullDateTime.value());
                    if (ISO8601::isYearWithinLimits(fullDate.year())) {
                        auto resolved = TemporalCore::plainMonthDayFromISODate(TemporalCore::calendarIDFromString(calendarId), fullDate, TemporalOverflow::Constrain);
                        if (!resolved) {
                            throwRangeError(globalObject, scope, String(resolved.error().message));
                            return { };
                        }
                        auto* result = TemporalPlainMonthDay::create(vm, globalObject->plainMonthDayStructure(), WTF::move(resolved->isoDate));
                        if (resolved->calendarId != "iso8601"_s)
                            result->setCalendarId(resolved->calendarId);
                        return result;
                    }
                }
            }
            auto dateWithoutYear = ISO8601::PlainDate(1972, plainDate.month(), plainDate.day());
            auto* result = TemporalPlainMonthDay::tryCreateIfValid(globalObject, globalObject->plainMonthDayStructure(), WTF::move(dateWithoutYear));
            RETURN_IF_EXCEPTION(scope, { });
            if (result && calendarId != "iso8601"_s)
                result->setCalendarId(calendarId);
            return result;
        }
    }

    throwRangeError(globalObject, scope,
        makeString("Temporal.PlainMonthDay.from: invalid date string "_s, string));
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.with
ISO8601::PlainDate TemporalPlainMonthDay::with(JSGlobalObject* globalObject, JSObject* temporalMonthDayLike, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    rejectObjectWithCalendarOrTimeZone(globalObject, temporalMonthDayLike);
    RETURN_IF_EXCEPTION(scope, { });

    // Read partial fields from user object FIRST (spec requires fields before options).
    // Use a separate outCalendarId since readCalendarFieldsFromObject resets it.
    String calId = TemporalCore::calendarIDToString(m_calendarID).toString();
    String outCalendarId;
    auto partialFields = readCalendarFieldsFromObject(globalObject, temporalMonthDayLike, outCalendarId, FieldSetType::MonthDay, /* skipCalendarRead */ true);
    RETURN_IF_EXCEPTION(scope, { });
    if (!partialFields.day && !partialFields.month && !partialFields.monthCode
        && !partialFields.year && !partialFields.era && !partialFields.eraYear) {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);
        return { };
    }

    // Read options AFTER reading fields (spec order).
    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });
    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Merge per temporal_rs PlainMonthDay::with():
    // - day: user's day OR current calendar day (not ISO day)
    // - monthCode: user's month/monthCode, or fall back to current PMD's monthCode
    TemporalCore::CalendarFieldsIn merged;

    // Get current calendar day (not ISO day) for fallback.
    uint8_t currentCalDay = static_cast<uint8_t>(m_plainMonthDay.day()); // ISO day fallback
    auto calID = m_calendarID;
    if (!TemporalCore::calendarIsISO(calID)) {
        auto dayResult = TemporalCore::calendarDay(calID, m_plainMonthDay.isoPlainDate());
        if (dayResult)
            currentCalDay = *dayResult;
    }
    merged.day = partialFields.day.has_value() ? partialFields.day : std::optional<uint8_t>(currentCalDay);

    if (partialFields.month.has_value())
        merged.month = partialFields.month;
    if (partialFields.monthCode)
        merged.monthCode = partialFields.monthCode;
    // Pass year/era/eraYear from partial to enable overflow validation in monthDayFromFields.
    if (partialFields.year)
        merged.year = partialFields.year;
    if (partialFields.era)
        merged.era = partialFields.era;
    if (partialFields.eraYear)
        merged.eraYear = partialFields.eraYear;
    if (!partialFields.month.has_value() && !partialFields.monthCode) {
        // Fall back to current PMD's monthCode (from stored ISO date via calendar).
        if (!TemporalCore::calendarIsISO(calID)) {
            auto mcStr = TemporalCore::calendarMonthCode(calID, m_plainMonthDay.isoPlainDate());
            if (!mcStr) {
                throwRangeError(globalObject, scope, String(mcStr.error().message));
                return { };
            }
            merged.monthCode = ISO8601::parseMonthCode(*mcStr);
        } else
            merged.month = std::optional<uint32_t>(m_plainMonthDay.month());
    }

    // For non-ISO PMD.with(): providing only month ordinal (no monthCode) is insufficient
    // because month ordinals depend on the year. Require monthCode per temporal_rs.
    if (!TemporalCore::calendarIsISO(calID) && merged.month.has_value() && !merged.monthCode) {
        throwTypeError(globalObject, scope, "monthCode is required for non-ISO calendar PlainMonthDay.with()"_s);
        return { };
    }

    auto resolved = TemporalCore::monthDayFromFields(calID, merged, overflow);
    if (!resolved) {
        if (resolved.error().kind == TemporalErrorKind::TypeError)
            throwTypeError(globalObject, scope, String(resolved.error().message));
        else
            throwRangeError(globalObject, scope, String(resolved.error().message));
        return { };
    }
    return resolved->isoDate;
}

String TemporalPlainMonthDay::monthCode() const
{
    return ISO8601::monthCode(m_plainMonthDay.month());
}

} // namespace JSC
