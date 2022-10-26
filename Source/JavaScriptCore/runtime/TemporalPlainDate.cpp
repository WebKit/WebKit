/*
 * Copyright (C) 2022 Apple Inc.
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
#include "TemporalPlainDate.h"

#include "AbstractSlotVisitor.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "LazyPropertyInlines.h"
#include "TemporalDuration.h"
#include "VMTrapsInlines.h"

namespace JSC {
namespace TemporalPlainDateInternal {
static constexpr bool verbose = false;
}

const ClassInfo TemporalPlainDate::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalPlainDate) };

TemporalPlainDate* TemporalPlainDate::create(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate)
{
    auto* object = new (NotNull, allocateCell<TemporalPlainDate>(vm)) TemporalPlainDate(vm, structure, WTFMove(plainDate));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalPlainDate::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalPlainDate::TemporalPlainDate(VM& vm, Structure* structure, ISO8601::PlainDate&& plainDate)
    : Base(vm, structure)
    , m_plainDate(WTFMove(plainDate))
{
}

void TemporalPlainDate::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_calendar.initLater(
        [] (const auto& init) {
            VM& vm = init.vm;
            auto* plainDate = jsCast<TemporalPlainDate*>(init.owner);
            auto* globalObject = plainDate->globalObject();
            auto* calendar = TemporalCalendar::create(vm, globalObject->calendarStructure(), iso8601CalendarID());
            init.set(calendar);
        });
}

template<typename Visitor>
void TemporalPlainDate::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* thisObject = jsCast<TemporalPlainDate*>(cell);
    thisObject->m_calendar.visit(visitor);
}

DEFINE_VISIT_CHILDREN(TemporalPlainDate);

// https://tc39.es/proposal-temporal/#sec-temporal-isvalidisodate
static ISO8601::PlainDate toPlainDate(JSGlobalObject* globalObject, ISO8601::Duration&& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double yearDouble = duration.years();
    double monthDouble = duration.months();
    double dayDouble = duration.days();

    if (!isInBounds<int32_t>(yearDouble)) {
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

    if (!ISO8601::isDateTimeWithinLimits(year, month, day, 0, 0, 0, 0, 0, 0)) {
        throwRangeError(globalObject, scope, "date time is out of range of ECMAScript representation"_s);
        return { };
    }

    return ISO8601::PlainDate {
        year,
        month,
        day
    };
}

// CreateTemporalPlainDate ( years, months, days )
// https://tc39.es/proposal-temporal/#sec-temporal-plaindate-constructor
TemporalPlainDate* TemporalPlainDate::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::Duration&& duration)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto plainDate = toPlainDate(globalObject, WTFMove(duration));
    RETURN_IF_EXCEPTION(scope, { });

    return TemporalPlainDate::create(vm, structure, WTFMove(plainDate));
}

String TemporalPlainDate::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        return toString();

    return ISO8601::temporalDateToString(m_plainDate);
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldate
TemporalPlainDate* TemporalPlainDate::from(JSGlobalObject* globalObject, JSValue itemValue, std::optional<TemporalOverflow> overflowValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    UNUSED_PARAM(overflowValue);

    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalPlainDate>())
            return jsCast<TemporalPlainDate*>(itemValue);
        throwRangeError(globalObject, scope, "unimplemented: from object"_s);
        return { };
    }

    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaldatestring
    // TemporalDateString :
    //     CalendarDateTime
    auto dateTime = ISO8601::parseCalendarDateTime(string);
    if (dateTime) {
        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTFMove(dateTime.value());
        if (!(timeZoneOptional && timeZoneOptional->m_z))
            return TemporalPlainDate::create(vm, globalObject->plainDateStructure(), WTFMove(plainDate));
    }

    throwRangeError(globalObject, scope, "invalid date string"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal-comparetemporaldate
int32_t TemporalPlainDate::compare(TemporalPlainDate* plainDate1, TemporalPlainDate* plainDate2)
{
    ISO8601::PlainDate d1 = plainDate1->plainDate();
    ISO8601::PlainDate d2 = plainDate2->plainDate();
    if (d1.year() > d2.year())
        return 1;
    if (d1.year() < d2.year())
        return -1;
    if (d1.month() > d2.month())
        return 1;
    if (d1.month() < d2.month())
        return -1;
    if (d1.day() > d2.day())
        return 1;
    if (d1.day() < d2.day())
        return -1;
    return 0;
}

String TemporalPlainDate::monthCode() const
{
    return ISO8601::monthCode(m_plainDate.month());
}

uint8_t TemporalPlainDate::dayOfWeek() const
{
    return ISO8601::dayOfWeek(m_plainDate);
}

uint16_t TemporalPlainDate::dayOfYear() const
{
    return ISO8601::dayOfYear(m_plainDate);
}

uint8_t TemporalPlainDate::weekOfYear() const
{
    return ISO8601::weekOfYear(m_plainDate);
}

} // namespace JSC
