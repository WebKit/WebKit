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

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "LazyPropertyInlines.h"
#include "TemporalDuration.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "VMTrapsInlines.h"

namespace JSC {

const ClassInfo TemporalPlainMonthDay::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainMonthDay) };

TemporalPlainMonthDay* TemporalPlainMonthDay::create(VM& vm, Structure* structure, ISO8601::PlainMonthDay&& plainMonthDay)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainMonthDay>(vm)) TemporalPlainMonthDay(vm, structure, WTFMove(plainMonthDay));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainMonthDay::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainMonthDay::TemporalPlainMonthDay(VM& vm, Structure* structure, ISO8601::PlainMonthDay&& plainMonthDay)
    : Base(vm, structure)
    , m_plainMonthDay(WTFMove(plainMonthDay))
{
}

void TemporalPlainMonthDay::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_calendar.initLater(
        [] (const auto& init) {
            VM& vm = init.vm;
            auto* plainMonthDay = jsCast<TemporalPlainMonthDay*>(init.owner);
            auto* globalObject = plainMonthDay->globalObject();
            auto* calendar = TemporalCalendar::create(vm, globalObject->calendarStructure(), iso8601CalendarID());
            init.set(calendar);
        });
}

template<typename Visitor>
void TemporalPlainMonthDay::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* thisObject = jsCast<TemporalPlainMonthDay*>(cell);
    thisObject->m_calendar.visit(visitor);
}

DEFINE_VISIT_CHILDREN(TemporalPlainMonthDay);

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

    return TemporalPlainMonthDay::create(vm, structure, ISO8601::PlainMonthDay(WTFMove(plainDate)));
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

    return ISO8601::temporalMonthDayToString(m_plainMonthDay, calendarName);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.from
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalmonthday
TemporalPlainMonthDay* TemporalPlainMonthDay::from(JSGlobalObject* globalObject, JSValue itemValue, std::optional<JSValue> optionsValue)
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
        if (optionsValue) {
            toTemporalOverflow(globalObject, optionsValue.value());
            RETURN_IF_EXCEPTION(scope, { });
        }
        return result;
    }

    std::optional<JSObject*> options;
    if (optionsValue) {
        options = intlGetOptionsObject(globalObject, optionsValue.value());
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainMonthDay>())
            return jsCast<TemporalPlainMonthDay*>(itemValue);

        if (itemValue.inherits<TemporalPlainMonthDay>())
            return TemporalPlainMonthDay::create(vm, globalObject->plainMonthDayStructure(), jsCast<TemporalPlainMonthDay*>(itemValue)->plainMonthDay());

        JSObject* calendar = TemporalCalendar::getTemporalCalendarWithISODefault(globalObject, itemValue);
        RETURN_IF_EXCEPTION(scope, { });

        // FIXME: Implement after fleshing out Temporal.Calendar.
        if (!calendar->inherits<TemporalCalendar>() || !jsCast<TemporalCalendar*>(calendar)->isISO8601()) [[unlikely]] {
            throwRangeError(globalObject, scope, "unimplemented: from non-ISO8601 calendar"_s);
            return { };
        }

        Variant<JSObject*, TemporalOverflow> optionsOrOverflow = TemporalOverflow::Constrain;
        if (options)
            optionsOrOverflow = options.value();
        auto overflow = TemporalOverflow::Constrain;
        auto plainMonthDay = TemporalCalendar::isoDateFromFields(globalObject, asObject(itemValue), TemporalDateFormat::MonthDay, optionsOrOverflow, overflow);
        RETURN_IF_EXCEPTION(scope, { });

        auto plainDate = ISO8601::PlainDate(1972, plainMonthDay.month(), plainMonthDay.day());
        return TemporalPlainMonthDay::create(vm, globalObject->plainMonthDayStructure(), WTFMove(plainDate));
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
        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTFMove(dateTime.value());
        if (calendarOptional && StringView(calendarOptional.value()) != String::fromLatin1("iso8601")) [[unlikely]] {
            throwRangeError(globalObject, scope,
                "MM-DD format is only valid with iso8601 calendar"_s);
            return { };
        }
        auto dateWithoutYear = ISO8601::PlainDate(1972, plainDate.month(), plainDate.day());
        if (!(timeZoneOptional && timeZoneOptional->m_z))
            RELEASE_AND_RETURN(scope, TemporalPlainMonthDay::tryCreateIfValid(globalObject, globalObject->plainMonthDayStructure(), WTFMove(dateWithoutYear)));
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

    if (!calendar()->isISO8601()) [[unlikely]] {
        throwRangeError(globalObject, scope, "unimplemented: with non-ISO8601 calendar"_s);
        return { };
    }

    auto [y, m, d, optionalMonthCode, overflow, any] =
        TemporalPlainDate::mergeDateFields(globalObject, temporalMonthDayLike, optionsValue,
            1972, month(), day());
    RETURN_IF_EXCEPTION(scope, { });
    if (any == TemporalAnyProperties::None) [[unlikely]] {
        throwTypeError(globalObject, scope, "Object must contain at least one Temporal date property"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, TemporalCalendar::monthDayFromFields(globalObject, y, m, d, optionalMonthCode, overflow));
}

String TemporalPlainMonthDay::monthCode() const
{
    return ISO8601::monthCode(m_plainMonthDay.month());
}

} // namespace JSC
