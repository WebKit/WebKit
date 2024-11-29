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

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "LazyPropertyInlines.h"
#include "TemporalDuration.h"
#include "TemporalPlainDateTime.h"
#include "VMTrapsInlines.h"

namespace JSC {

const ClassInfo TemporalPlainYearMonth::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainYearMonth) };

TemporalPlainYearMonth* TemporalPlainYearMonth::create(VM& vm, Structure* structure, ISO8601::PlainYearMonth&& plainYearMonth)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainYearMonth>(vm)) TemporalPlainYearMonth(vm, structure, WTFMove(plainYearMonth));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainYearMonth::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainYearMonth::TemporalPlainYearMonth(VM& vm, Structure* structure, ISO8601::PlainYearMonth&& plainYearMonth)
    : Base(vm, structure)
    , m_plainYearMonth(WTFMove(plainYearMonth))
{
}

void TemporalPlainYearMonth::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_calendar.initLater(
        [] (const auto& init) {
            VM& vm = init.vm;
            auto* plainYearMonth = jsCast<TemporalPlainYearMonth*>(init.owner);
            auto* globalObject = plainYearMonth->globalObject();
            auto* calendar = TemporalCalendar::create(vm, globalObject->calendarStructure(), iso8601CalendarID());
            init.set(calendar);
        });
}

template<typename Visitor>
void TemporalPlainYearMonth::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* thisObject = jsCast<TemporalPlainYearMonth*>(cell);
    thisObject->m_calendar.visit(visitor);
}

DEFINE_VISIT_CHILDREN(TemporalPlainYearMonth);

#if false

ISO8601::PlainYearMonth TemporalPlainYearMonth::toPlainYearMonth(JSGlobalObject* globalObject, const ISO8601::Duration& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double yearDouble = duration.years();
    double monthDouble = duration.months();
    double dayDouble = duration.days();

    if (!ISO8601::isYearWithinLimits(yearDouble)) {
        throwRangeError(globalObject, scope, "year is out of range"_s);
        return { };
    }
    int32_t year = static_cast<int32_t>(yearDouble);

    if (!(monthDouble >= 1 && monthDouble <= 12)) {
        throwRangeError(globalObject, scope, "month is out of range"_s);
        return { };
    }
    unsigned month = static_cast<unsigned>(monthDouble);

    double daysInMonth = ISO8601::daysInMonth(year, month);
    if (!(dayDouble >= 1 && dayDouble <= daysInMonth)) {
        throwRangeError(globalObject, scope, "day is out of range"_s);
        return { };
    }
    unsigned day = static_cast<unsigned>(dayDouble);

    return ISO8601::PlainYearMonth {
        year,
        month,
        day
    };
}

#endif

// CreateTemporalYearMonth ( isoDate, calendar [, newTarget ] )
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalyearmonth
TemporalPlainYearMonth* TemporalPlainYearMonth::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!ISO8601::isYearMonthWithinLimits(plainDate.year(), plainDate.month())) {
        throwRangeError(globalObject, scope, "PlainYearMonth is out of range of ECMAScript representation"_s);
        return { };
    }

    return TemporalPlainYearMonth::create(vm, structure, ISO8601::PlainYearMonth(WTFMove(plainDate)));
}

String TemporalPlainYearMonth::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        return toString();

    String calendarName = toTemporalCalendarName(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    return ISO8601::temporalYearMonthToString(m_plainYearMonth, calendarName);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.from
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalyearmonth
TemporalPlainYearMonth* TemporalPlainYearMonth::from(JSGlobalObject* globalObject, JSValue itemValue, std::optional<JSValue> optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Handle string case first so that string parsing errors (RangeError)
    // can be thrown before options-related errors (TypeError);
    // see step 4 of ToTemporalYearMonth
    bool isString = false;
    TemporalPlainYearMonth* result;
    if (itemValue.isString()) {
        isString = true;
        auto string = itemValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        result = TemporalPlainYearMonth::from(globalObject, string);
        RETURN_IF_EXCEPTION(scope, { });
    }
    std::optional<JSObject*> options;
    if (optionsValue) {
        options = intlGetOptionsObject(globalObject, optionsValue.value());
        RETURN_IF_EXCEPTION(scope, { });
    }


    if (isString) {
        // Overflow has to be validated even though it's not used,
        // so that an error can be thrown for a bad overflow option)
         if (options)
            toTemporalOverflow(globalObject, options.value());
        RELEASE_AND_RETURN(scope, result);
    }

    if (itemValue.isObject()) {

        if (itemValue.inherits<TemporalPlainYearMonth>())
            return jsCast<TemporalPlainYearMonth*>(itemValue);

        if (itemValue.inherits<TemporalPlainYearMonth>())
            return TemporalPlainYearMonth::create(vm, globalObject->plainYearMonthStructure(), jsCast<TemporalPlainYearMonth*>(itemValue)->plainYearMonth());

        JSObject* calendar = TemporalCalendar::getTemporalCalendarWithISODefault(globalObject, itemValue);
        RETURN_IF_EXCEPTION(scope, { });

        // FIXME: Implement after fleshing out Temporal.Calendar.
        if (!calendar->inherits<TemporalCalendar>() || !jsCast<TemporalCalendar*>(calendar)->isISO8601()) {
            throwRangeError(globalObject, scope, "unimplemented: from non-ISO8601 calendar"_s);
            return { };
        }

        std::variant<JSObject*, TemporalOverflow> optionsOrOverflow = TemporalOverflow::Constrain;
        if (options)
            optionsOrOverflow = options.value();
        auto overflow = TemporalOverflow::Constrain;
        auto plainYearMonth = TemporalCalendar::isoDateFromFields(globalObject, asObject(itemValue), TemporalDateFormat::YearMonth, optionsOrOverflow, overflow);
        RETURN_IF_EXCEPTION(scope, { });

        return TemporalPlainYearMonth::create(vm, globalObject->plainYearMonthStructure(), WTFMove(plainYearMonth));
    }

    throwTypeError(globalObject, scope, "can only convert to PlainYearMonth from object or string values"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.from
TemporalPlainYearMonth* TemporalPlainYearMonth::from(JSGlobalObject* globalObject, WTF::String string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaldatestring
    // TemporalDateString :
    //     CalendarDateTime
    auto dateTime = ISO8601::parseCalendarDateTime(string, TemporalDateFormat::YearMonth);
    if (dateTime) {
        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTFMove(dateTime.value());
        if (calendarOptional && StringView(calendarOptional->m_name) != String::fromLatin1("iso8601")) {
            throwRangeError(globalObject, scope,
                "YYYY-MM format is only valid with iso8601 calendar"_s);
            return { };
        }
        if (!(timeZoneOptional && timeZoneOptional->m_z))
            RELEASE_AND_RETURN(scope, TemporalPlainYearMonth::tryCreateIfValid(globalObject, globalObject->plainYearMonthStructure(), WTFMove(plainDate)));
    }

    throwRangeError(globalObject, scope,
        makeString("Temporal.PlainYearMonth.from: invalid date string "_s, string));
    return { };
}


std::array<std::optional<double>, 2> TemporalPlainYearMonth::toPartialDate(JSGlobalObject* globalObject, JSObject* temporalDateLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    std::optional<double> month;
    JSValue monthProperty = temporalDateLike->get(globalObject, vm.propertyNames->month);
    RETURN_IF_EXCEPTION(scope, { });
    if (!monthProperty.isUndefined()) {
        month = monthProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (month.value() <= 0 || !std::isfinite(month.value())) {
            throwRangeError(globalObject, scope, "month property must be positive and finite"_s);
            return { };
        }
    }

    JSValue monthCodeProperty = temporalDateLike->get(globalObject, vm.propertyNames->monthCode);
    RETURN_IF_EXCEPTION(scope, { });
    if (!monthCodeProperty.isUndefined()) {
        auto monthCode = monthCodeProperty.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        auto otherMonth = ISO8601::monthFromCode(monthCode);
        if (!otherMonth) {
            throwRangeError(globalObject, scope, "Invalid monthCode property"_s);
            return { };
        }

        if (!month)
            month = otherMonth;
        else if (month.value() != otherMonth) {
            throwRangeError(globalObject, scope, "month and monthCode properties must match if both are provided"_s);
            return { };
        }
    }

    std::optional<double> year;
    JSValue yearProperty = temporalDateLike->get(globalObject, vm.propertyNames->year);
    RETURN_IF_EXCEPTION(scope, { });
    if (!yearProperty.isUndefined()) {
        year = yearProperty.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (!std::isfinite(year.value())) {
            throwRangeError(globalObject, scope, "year property must be finite"_s);
            return { };
        }
    }

    return { year, month };
}

ISO8601::PlainDate TemporalPlainYearMonth::with(JSGlobalObject* globalObject, JSObject* temporalYearMonthLike, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    rejectObjectWithCalendarOrTimeZone(globalObject, temporalYearMonthLike);
    RETURN_IF_EXCEPTION(scope, { });

    if (!calendar()->isISO8601()) {
        throwRangeError(globalObject, scope, "unimplemented: with non-ISO8601 calendar"_s);
        return { };
    }

    auto [optionalYear, optionalMonth] = toPartialDate(globalObject, temporalYearMonthLike);
    RETURN_IF_EXCEPTION(scope, { });
    if (!optionalYear && !optionalMonth) {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);
        return { };
    }

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    double y = optionalYear.value_or(year());
    double m = optionalMonth.value_or(month());
    return TemporalCalendar::yearMonthFromFields(globalObject, y, m, overflow);
}

ISO8601::Duration TemporalPlainYearMonth::until(JSGlobalObject* globalObject, TemporalPlainYearMonth* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool calendarsMatch = calendar()->equals(globalObject, other->calendar());
    RETURN_IF_EXCEPTION(scope, { });
    if (!calendarsMatch) {
        throwRangeError(globalObject, scope, "calendars must match"_s);
        return { };
    }

    if (!calendar()->isISO8601()) {
        throwRangeError(globalObject, scope, "unimplemented: with non-ISO8601 calendar"_s);
        return { };
    }

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Date, TemporalUnit::Month, TemporalUnit::Year);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = TemporalCalendar::differenceTemporalPlainYearMonth(
        globalObject, false, plainYearMonth(), other->plainYearMonth(), increment, smallestUnit, largestUnit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });

    return result;
}

ISO8601::Duration TemporalPlainYearMonth::since(JSGlobalObject* globalObject, TemporalPlainYearMonth* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool calendarsMatch = calendar()->equals(globalObject, other->calendar());
    RETURN_IF_EXCEPTION(scope, { });
    if (!calendarsMatch) {
        throwRangeError(globalObject, scope, "calendars must match"_s);
        return { };
    }

    if (!calendar()->isISO8601()) {
        throwRangeError(globalObject, scope, "unimplemented: with non-ISO8601 calendar"_s);
        return { };
    }

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Date, TemporalUnit::Month, TemporalUnit::Year);
    RETURN_IF_EXCEPTION(scope, { });
    roundingMode = negateTemporalRoundingMode(roundingMode);

    auto result = TemporalCalendar::differenceTemporalPlainYearMonth(
        globalObject, true, plainYearMonth(), other->plainYearMonth(), increment, smallestUnit, largestUnit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });

    return result;
}

String TemporalPlainYearMonth::monthCode() const
{
    return ISO8601::monthCode(m_plainYearMonth.month());
}

static int32_t durationSign(const ISO8601::Duration& d)
{
    if (d.years() > 0)
        return 1;
    if (d.years() < 0)
        return -1;
    if (d.months() > 0)
        return 1;
    if (d.months() < 0)
        return -1;
    if (d.weeks() > 0)
        return 1;
    if (d.weeks() < 0)
        return -1;
    if (d.days() > 0)
        return 1;
    if (d.days() < 0)
        return -1;
    if (d.hours() > 0)
        return 1;
    if (d.hours() < 0)
        return -1;
    if (d.minutes() > 0)
        return 1;
    if (d.minutes() < 0)
        return -1;
    if (d.seconds() > 0)
        return 1;
    if (d.seconds() < 0)
        return -1;
    if (d.milliseconds() > 0)
        return 1;
    if (d.milliseconds() < 0)
        return -1;
    if (d.microseconds() > 0)
        return 1;
    if (d.microseconds() < 0)
        return -1;
    if (d.nanoseconds() > 0)
        return 1;
    if (d.nanoseconds() < 0)
        return -1;
    return 0;
}

// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtoyearmonth
ISO8601::PlainYearMonth TemporalPlainYearMonth::addDurationToYearMonth(JSGlobalObject* globalObject,
    bool isAdd, ISO8601::PlainYearMonth yearMonth, ISO8601::Duration duration, TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!isAdd)
        duration = -duration;
    auto sign = durationSign(duration);
    auto year = yearMonth.year();
    auto month = yearMonth.month();
    auto day = 1;
    auto intermediateDate = ISO8601::PlainDate(year, month, day);
    ISO8601::PlainDate date;
    if (sign < 0) {
        auto oneMonthDuration = ISO8601::Duration { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        auto nextMonth = TemporalCalendar::isoDateAdd(globalObject,
            intermediateDate, oneMonthDuration, TemporalOverflow::Constrain);
        RETURN_IF_EXCEPTION(scope, { });
        double y = nextMonth.year();
        double m = nextMonth.month();
        double d = nextMonth.day() - 1;
        date = TemporalCalendar::balanceISODate(y, m, d);
    } else
        date = intermediateDate;
    auto durationToAdd = TemporalDuration::toDateDurationRecordWithoutTime(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });
    auto addedDate = TemporalCalendar::isoDateAdd(globalObject, date, durationToAdd, overflow);
    RETURN_IF_EXCEPTION(scope, { });
    return ISO8601::PlainYearMonth(addedDate.year(), addedDate.month());
}

} // namespace JSC
