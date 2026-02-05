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
    return ISO8601::temporalYearMonthToString(m_plainYearMonth, ""_s);
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

    return ISO8601::temporalYearMonthToString(m_plainYearMonth, calendarName);
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

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (item.isObject()) {
        if (item.inherits<TemporalPlainYearMonth>())
            return jsCast<TemporalPlainYearMonth*>(item);

        JSObject* calendar = TemporalCalendar::getTemporalCalendarWithISODefault(globalObject, item);
        RETURN_IF_EXCEPTION(scope, { });

        // FIXME: Implement after fleshing out Temporal.Calendar.
        if (!calendar->inherits<TemporalCalendar>() || !jsCast<TemporalCalendar*>(calendar)->isISO8601()) [[unlikely]] {
            throwRangeError(globalObject, scope, "unimplemented: from non-ISO8601 calendar"_s);
            return { };
        }

        Variant<JSObject*, TemporalOverflow> optionsOrOverflow = TemporalOverflow::Constrain;
        if (options)
            optionsOrOverflow = options;
        auto overflow = TemporalOverflow::Constrain;
        auto plainYearMonth = TemporalCalendar::isoDateFromFields(globalObject, asObject(item), TemporalDateFormat::YearMonth, optionsOrOverflow, overflow);
        RETURN_IF_EXCEPTION(scope, { });

        return TemporalPlainYearMonth::create(vm, globalObject->plainYearMonthStructure(), WTF::move(plainYearMonth));
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
        if (calendarOptional && !equal(calendarOptional.value().span(), "iso8601"_span8)) [[unlikely]] {
            throwRangeError(globalObject, scope,
                "YYYY-MM format is only valid with iso8601 calendar"_s);
            return { };
        }
        if (!(timeZoneOptional && timeZoneOptional->m_z)) [[likely]]
            RELEASE_AND_RETURN(scope, TemporalPlainYearMonth::tryCreateIfValid(globalObject, globalObject->plainYearMonthStructure(), WTF::move(plainDate)));
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

    if (!calendar()->isISO8601()) [[unlikely]] {
        throwRangeError(globalObject, scope, "unimplemented: with non-ISO8601 calendar"_s);
        return { };
    }

    auto [optionalMonth, optionalMonthCode, optionalYear] = TemporalPlainDate::toYearMonth(globalObject, temporalYearMonthLike);
    RETURN_IF_EXCEPTION(scope, { });
    if (!optionalMonth && !optionalMonthCode && !optionalYear) [[unlikely]] {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);
        return { };
    }

    int32_t y = optionalYear.value_or(year());
    int32_t m = 0;
    if (optionalMonth)
        m = optionalMonth.value();
    else if (optionalMonthCode)
        m = optionalMonthCode->monthNumber;
    else
        m = month();

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });
    TemporalOverflow overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, TemporalCalendar::yearMonthFromFields(globalObject, y, m, optionalMonthCode, overflow));
}

template<DifferenceOperation op>
ISO8601::Duration TemporalPlainYearMonth::sinceOrUntil(JSGlobalObject* globalObject, TemporalPlainYearMonth* other, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool calendarsMatch = calendar()->equals(globalObject, other->calendar());
    RETURN_IF_EXCEPTION(scope, { });
    if (!calendarsMatch) [[unlikely]] {
        throwRangeError(globalObject, scope, "calendars must match"_s);
        return { };
    }

    if (!calendar()->isISO8601()) [[unlikely]] {
        throwRangeError(globalObject, scope, "unimplemented: with non-ISO8601 calendar"_s);
        return { };
    }

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, optionsValue, UnitGroup::Date, TemporalUnit::Month, TemporalUnit::Year);
    RETURN_IF_EXCEPTION(scope, { });

    if (op == DifferenceOperation::Since)
        roundingMode = negateTemporalRoundingMode(roundingMode);

    RELEASE_AND_RETURN(scope, TemporalCalendar::differenceTemporalPlainYearMonth<op>(globalObject, plainYearMonth(), other->plainYearMonth(), increment, smallestUnit, largestUnit, roundingMode));
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
