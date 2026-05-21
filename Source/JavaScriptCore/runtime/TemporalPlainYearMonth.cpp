/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "TemporalPlainYearMonth.h"

#include "CalendarFields.h"
#include "CalendarICUBridge.h"
#include "IntlObjectInlines.h"
#include "Rounding.h"
#include "JSCInlines.h"
#include "Rounding.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "VMTrapsInlines.h"

namespace JSC {

const ClassInfo TemporalPlainYearMonth::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainYearMonth) };

TemporalPlainYearMonth* TemporalPlainYearMonth::create(VM& vm, Structure* structure, ISO8601::PlainYearMonth&& plainYearMonth)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainYearMonth>(vm)) TemporalPlainYearMonth(vm, structure, WTF::move(plainYearMonth));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainYearMonth::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainYearMonth::TemporalPlainYearMonth(VM& vm, Structure* structure, ISO8601::PlainYearMonth&& plainYearMonth)
    : Base(vm, structure)
    , m_plainYearMonth(WTF::move(plainYearMonth))
    , m_calendarID(iso8601CalendarID())
{
}

void TemporalPlainYearMonth::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

// CreateTemporalYearMonth ( isoDate, calendar [, newTarget ] )
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalyearmonth
TemporalPlainYearMonth* TemporalPlainYearMonth::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isYearMonthWithinLimits(plainDate.year(), plainDate.month())) [[unlikely]] {
        throwRangeError(globalObject, scope, "PlainYearMonth is out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainYearMonth::create(vm, structure, ISO8601::PlainYearMonth(WTF::move(plainDate)));
}

String TemporalPlainYearMonth::toString() const
{
    return ISO8601::temporalYearMonthToString(m_plainYearMonth, "auto"_s, TemporalCore::calendarIDToString(m_calendarID));
}

String TemporalPlainYearMonth::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options) [[likely]]
        return toString();

    String calendarName = toTemporalCalendarName(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    return ISO8601::temporalYearMonthToString(m_plainYearMonth, calendarName, TemporalCore::calendarIDToString(m_calendarID));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.from
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalyearmonth
// optionsValue may be undefined, which is treated as the absence of an options argument
TemporalPlainYearMonth* TemporalPlainYearMonth::from(JSGlobalObject* globalObject, JSValue item, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Handle string case first so that string parsing errors (RangeError)
    // can be thrown before options-related errors (TypeError);
    // see step 4 of ToTemporalYearMonth
    auto string = item.getString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!string.isNull()) {
        auto* result = TemporalPlainYearMonth::from(globalObject, string);
        RETURN_IF_EXCEPTION(scope, { });
        // See step 11 of ToTemporalYearMonth
        if (!optionsValue.isUndefined()) {
            toTemporalOverflow(globalObject, optionsValue);
            RETURN_IF_EXCEPTION(scope, { });
        }
        RELEASE_AND_RETURN(scope, result);
    }

    if (item.isObject()) {
        if (item.inherits<TemporalPlainYearMonth>())
            return uncheckedDowncast<TemporalPlainYearMonth>(item);

        // Read ALL fields before options (spec order).
        String calendarId;
        auto fields = readCalendarFieldsFromObject(globalObject, asObject(item), calendarId, FieldSetType::YearMonth);
        RETURN_IF_EXCEPTION(scope, { });

        // Options validated AFTER fields.
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

        auto resolved = TemporalCore::yearMonthFromFields(TemporalCore::calendarIDFromString(calendarId), fields, overflow);
        if (!resolved) {
            if (resolved.error().kind == TemporalErrorKind::TypeError)
                throwTypeError(globalObject, scope, String(resolved.error().message));
            else
                throwRangeError(globalObject, scope, String(resolved.error().message));
            return { };
        }

        auto* result = TemporalPlainYearMonth::create(vm, globalObject->plainYearMonthStructure(), ISO8601::PlainYearMonth(WTF::move(resolved->isoDate)));
        if (resolved->calendarId != "iso8601"_s)
            result->setCalendarId(resolved->calendarId);
        return result;
    }

    throwTypeError(globalObject, scope, "can only convert to PlainYearMonth from object or string values"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.from
TemporalPlainYearMonth* TemporalPlainYearMonth::from(JSGlobalObject* globalObject, StringView string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaldatestring
    // TemporalDateString :
    //     CalendarDateTime
    auto dateTime = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::YearMonth);
    if (dateTime) [[likely]] {
        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTF::move(dateTime.value());
        // YYYY-MM format (no day) is only valid with iso8601 calendar.
        // Full date strings (YYYY-MM-DD) with non-ISO calendar annotations are allowed.
        bool looksLikeYearMonthOnly = false;
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
            looksLikeYearMonthOnly = (digitGroups <= 2);
        }
        if (looksLikeYearMonthOnly && calendarOptional && !WTF::equalIgnoringASCIICase(StringView(*calendarOptional), "iso8601"_s)) [[unlikely]] {
            throwRangeError(globalObject, scope,
                "YYYY-MM format is only valid with iso8601 calendar"_s);
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
        if (!(timeZoneOptional && timeZoneOptional->m_z)) [[likely]] {
            auto* result = TemporalPlainYearMonth::tryCreateIfValid(globalObject, globalObject->plainYearMonthStructure(), WTF::move(plainDate));
            RETURN_IF_EXCEPTION(scope, { });
            if (result && calendarId != "iso8601"_s) {
                result->m_calendarID = TemporalCore::calendarIDFromString(calendarId);
                // Steps 12-14 (temporal_rs from_parsed): re-resolve via CalendarYearMonthFromFields
                // to canonicalize the reference ISO day to the first day of the calendar month.
                // The YearMonth parser stores day=1, but for non-ISO full date strings we need
                // the actual parsed day to determine the correct calendar year/month.
                ISO8601::PlainDate fullISODate = result->m_plainYearMonth.isoPlainDate();
                if (!looksLikeYearMonthOnly) {
                    auto fullDateTime = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::Date);
                    if (fullDateTime) {
                        auto& fullDate = std::get<0>(fullDateTime.value());
                        if (ISO8601::isYearWithinLimits(fullDate.year()))
                            fullISODate = WTF::move(fullDate);
                    }
                }
                auto resolved = TemporalCore::plainYearMonthFromISODate(TemporalCore::calendarIDFromString(calendarId), fullISODate);
                if (!resolved) {
                    throwRangeError(globalObject, scope, String(resolved.error().message));
                    return { };
                }
                result->m_plainYearMonth = ISO8601::PlainYearMonth(WTF::move(resolved->isoDate));
            }
            return result;
        }
    }

    String message = tryMakeString("Temporal.PlainYearMonth.from: invalid date string "_s, string);
    if (!message)
        message = "Temporal.PlainYearMonth.from: invalid date string"_s;
    throwRangeError(globalObject, scope, message);
    return { };
}

ISO8601::PlainDate TemporalPlainYearMonth::with(JSGlobalObject* globalObject, JSObject* temporalYearMonthLike, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    rejectObjectWithCalendarOrTimeZone(globalObject, temporalYearMonthLike);
    RETURN_IF_EXCEPTION(scope, { });

    // Read partial fields from user object.
    auto [optionalMonth, optionalMonthCode, optionalYear] = TemporalPlainDate::toYearMonth(globalObject, temporalYearMonthLike);
    RETURN_IF_EXCEPTION(scope, { });
    if (!optionalMonth && !optionalMonthCode && !optionalYear) [[unlikely]] {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);
        return { };
    }

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });
    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    // Build partial CalendarFieldsIn from user's fields.
    TemporalCore::CalendarFieldsIn partialFields;
    if (optionalYear)
        partialFields.year = *optionalYear;
    if (optionalMonth)
        partialFields.month = static_cast<uint32_t>(*optionalMonth);
    if (optionalMonthCode)
        partialFields.monthCode = optionalMonthCode;

    // Delegate to temporal/core — merges with fallback and calls yearMonthFromFields.
    // Use the actual stored ISO date (not reconstructed from ISO year/month) so that
    // isoToCalendarFields resolves to the correct calendar year/month for non-ISO.
    auto result = TemporalCore::plainYearMonthWith(m_calendarID, m_plainYearMonth.isoPlainDate(), partialFields, overflow);
    if (!result) {
        if (result.error().kind == TemporalErrorKind::TypeError)
            throwTypeError(globalObject, scope, String(result.error().message));
        else
            throwRangeError(globalObject, scope, String(result.error().message));
        return { };
    }
    return result->isoDate;
}

template<DifferenceOperation op>
ISO8601::Duration TemporalPlainYearMonth::sinceOrUntil(JSGlobalObject* globalObject, TemporalPlainYearMonth* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Date, TemporalUnit::Month, TemporalUnit::Year);
    RETURN_IF_EXCEPTION(scope, { });

    if (m_calendarID != other->m_calendarID) {
        throwRangeError(globalObject, scope, "cannot compute difference between year-months with different calendars"_s);
        return { };
    }

    if (op == DifferenceOperation::Since)
        roundingMode = TemporalCore::negateTemporalRoundingMode(roundingMode);

    RELEASE_AND_RETURN(scope, JSC::differenceTemporalPlainYearMonth<op>(globalObject, plainYearMonth(), other->plainYearMonth(), increment, smallestUnit, largestUnit, roundingMode, TemporalCore::calendarIDToString(m_calendarID)));
}

ISO8601::Duration TemporalPlainYearMonth::until(JSGlobalObject* globalObject, TemporalPlainYearMonth* other, JSValue optionsValue)
{
    return sinceOrUntil<DifferenceOperation::Until>(globalObject, other, optionsValue);
}

ISO8601::Duration TemporalPlainYearMonth::since(JSGlobalObject* globalObject, TemporalPlainYearMonth* other, JSValue optionsValue)
{
    return sinceOrUntil<DifferenceOperation::Since>(globalObject, other, optionsValue);
}

String TemporalPlainYearMonth::monthCode() const
{
    return ISO8601::monthCode(m_plainYearMonth.month());
}

} // namespace JSC
