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
#include "TemporalTimeZone.h"

#include "ISO8601.h"
#include "JSObjectInlines.h"

namespace JSC {

const ClassInfo TemporalTimeZone::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalTimeZone) };

TemporalTimeZone* TemporalTimeZone::createFromID(VM& vm, Structure* structure, TimeZoneID identifier)
{
    TemporalTimeZone* format = new (NotNull, allocateCell<TemporalTimeZone>(vm)) TemporalTimeZone(vm, structure, TimeZone { std::in_place_index_t<0>(), identifier });
    format->finishCreation(vm);
    return format;
}

TemporalTimeZone* TemporalTimeZone::createFromUTCOffset(VM& vm, Structure* structure, int64_t utcOffset)
{
    TemporalTimeZone* format = new (NotNull, allocateCell<TemporalTimeZone>(vm)) TemporalTimeZone(vm, structure, TimeZone { std::in_place_index_t<1>(), utcOffset });
    format->finishCreation(vm);
    return format;
}

Structure* TemporalTimeZone::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalTimeZone::TemporalTimeZone(VM& vm, Structure* structure, TimeZone timeZone)
    : Base(vm, structure)
    , m_timeZone(timeZone)
{
}

// https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaltimeZonestring
static std::optional<int64_t> parseTemporalTimeZoneString(StringView)
{
    // FIXME: Implement parsing temporal timeZone string, which requires full ISO 8601 parser.
    return std::nullopt;
}

// https://tc39.es/proposal-temporal/#sec-temporal.timezone.from
// https://tc39.es/proposal-temporal/#sec-temporal-totemporaltimezone
JSObject* TemporalTimeZone::from(JSGlobalObject* globalObject, JSValue timeZoneLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (timeZoneLike.isObject()) {
        JSObject* timeZoneLikeObject = jsCast<JSObject*>(timeZoneLike);

        // FIXME: We need to implement code retrieving TimeZone from Temporal Date Like objects. But
        // currently they are not implemented yet.

        bool hasProperty = timeZoneLikeObject->hasProperty(globalObject, vm.propertyNames->timeZone);
        RETURN_IF_EXCEPTION(scope, { });
        if (!hasProperty)
            return timeZoneLikeObject;

        timeZoneLike = timeZoneLikeObject->get(globalObject, vm.propertyNames->timeZone);
        if (timeZoneLike.isObject()) {
            JSObject* timeZoneLikeObject = jsCast<JSObject*>(timeZoneLike);
            bool hasProperty = timeZoneLikeObject->hasProperty(globalObject, vm.propertyNames->timeZone);
            RETURN_IF_EXCEPTION(scope, { });
            if (!hasProperty)
                return timeZoneLikeObject;
        }
    }

    auto timeZoneString = timeZoneLike.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<int64_t> utcOffset = ISO8601::parseTimeZoneNumericUTCOffset(timeZoneString);
    if (utcOffset)
        return TemporalTimeZone::createFromUTCOffset(vm, globalObject->timeZoneStructure(), utcOffset.value());

    std::optional<TimeZoneID> identifier = ISO8601::parseTimeZoneName(timeZoneString);
    if (identifier)
        return TemporalTimeZone::createFromID(vm, globalObject->timeZoneStructure(), identifier.value());

    std::optional<int64_t> utcOffsetFromInstant = parseTemporalTimeZoneString(timeZoneString);
    if (utcOffsetFromInstant)
        return TemporalTimeZone::createFromUTCOffset(vm, globalObject->timeZoneStructure(), utcOffsetFromInstant.value());

    throwRangeError(globalObject, scope, "argument needs to be UTC offset string, TimeZone identifier, or temporal Instant string"_s);
    return { };
}

} // namespace JSC
