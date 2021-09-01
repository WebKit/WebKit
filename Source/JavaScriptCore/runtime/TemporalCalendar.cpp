/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "TemporalCalendar.h"

#include "JSObjectInlines.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo TemporalCalendar::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalCalendar) };

namespace TemporalCalendarInternal {
static constexpr bool verbose = false;
}

TemporalCalendar* TemporalCalendar::create(VM& vm, Structure* structure, CalendarID identifier)
{
    TemporalCalendar* format = new (NotNull, allocateCell<TemporalCalendar>(vm.heap)) TemporalCalendar(vm, structure, identifier);
    format->finishCreation(vm);
    return format;
}

Structure* TemporalCalendar::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalCalendar::TemporalCalendar(VM& vm, Structure* structure, CalendarID identifier)
    : Base(vm, structure)
    , m_identifier(identifier)
{
}

std::optional<CalendarID> TemporalCalendar::isBuiltinCalendar(StringView string)
{
    const auto& calendars = intlAvailableCalendars();
    for (unsigned index = 0; index < calendars.size(); ++index) {
        if (calendars[index] == string)
            return index;
    }
    return std::nullopt;
}

// https://tc39.es/proposal-temporal/#sec-temporal-parsetemporalcalendarstring
static std::optional<CalendarID> parseTemporalCalendarString(JSGlobalObject* globalObject, StringView)
{
    // FIXME: Implement parsing temporal calendar string, which requires full ISO 8601 parser.
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwRangeError(globalObject, scope, "invalid calendar ID"_s);
    return std::nullopt;
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalcalendar
JSObject* TemporalCalendar::from(JSGlobalObject* globalObject, JSValue calendarLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (calendarLike.isObject()) {
        JSObject* calendarLikeObject = jsCast<JSObject*>(calendarLike);

        // FIXME: We need to implement code retrieving Calendar from Temporal Date Like objects. But
        // currently they are not implemented yet.

        bool hasProperty = calendarLikeObject->hasProperty(globalObject, vm.propertyNames->calendar);
        RETURN_IF_EXCEPTION(scope, { });
        if (!hasProperty)
            return jsCast<JSObject*>(calendarLike);

        calendarLike = calendarLikeObject->get(globalObject, vm.propertyNames->calendar);
        if (calendarLike.isObject()) {
            bool hasProperty = jsCast<JSObject*>(calendarLike)->hasProperty(globalObject, vm.propertyNames->calendar);
            RETURN_IF_EXCEPTION(scope, { });
            if (!hasProperty)
                return jsCast<JSObject*>(calendarLike);
        }
    }

    auto identifier = calendarLike.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<CalendarID> calendarId = isBuiltinCalendar(identifier);
    if (!calendarId) {
        calendarId = parseTemporalCalendarString(globalObject, identifier);
        RETURN_IF_EXCEPTION(scope, { });
    }

    ASSERT(calendarId);
    return TemporalCalendar::create(vm, globalObject->calendarStructure(), calendarId.value());
}

} // namespace JSC
