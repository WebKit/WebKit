/*
 * Copyright (C) 2021 Apple Inc.
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
#include "TemporalZonedDateTime.h"

#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "LazyPropertyInlines.h"
#include "TemporalDuration.h"
#include "TemporalPlainDateTime.h"
#include "VMTrapsInlines.h"

namespace JSC {

const ClassInfo TemporalZonedDateTime::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalZonedDateTime) };

TemporalZonedDateTime* TemporalZonedDateTime::create(VM& vm, Structure* structure, ISO8601::ExactTime&& exactTime, ISO8601::TimeZone&& timeZone)
{
    auto* object = new (NotNull, allocateCell<TemporalZonedDateTime>(vm)) TemporalZonedDateTime(vm, structure, WTFMove(exactTime), WTFMove(timeZone));
    object->finishCreation(vm);
    return object;
}

Structure* TemporalZonedDateTime::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalZonedDateTime::TemporalZonedDateTime(VM& vm, Structure* structure, ISO8601::ExactTime&& exactTime, ISO8601::TimeZone&& timeZone)
    : Base(vm, structure)
    , m_exactTime(WTFMove(exactTime))
    , m_timeZone(WTFMove(timeZone))
{
}

void TemporalZonedDateTime::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_calendar.initLater(
        [] (const auto& init) {
            VM& vm = init.vm;
            auto* zonedDateTime = jsCast<TemporalZonedDateTime*>(init.owner);
            auto* globalObject = zonedDateTime->globalObject();
            auto* calendar = TemporalCalendar::create(vm, globalObject->calendarStructure(), iso8601CalendarID());
            init.set(calendar);
        });
}

template<typename Visitor>
void TemporalZonedDateTime::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* thisObject = jsCast<TemporalZonedDateTime*>(cell);
    thisObject->m_calendar.visit(visitor);
}

DEFINE_VISIT_CHILDREN(TemporalZonedDateTime);

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.round
TemporalZonedDateTime TemporalZonedDateTime::round(JSGlobalObject* globalObject, JSValue roundTo) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto paramString;
    if (roundTo.isString()) {
        paramString = roundTo;
        roundTo = null();
        options = createDataPropertyOrThrow(roundTo, "smallestUnit", paramString);
    } else
        options = intlGetOptionsObject(globalObject, options);

    roundingIncrement = temporalRoundingIncrement(options);
    roundingMode = temporalRoundingMode(options);
    smallestUnit = getTemporalUnitValuedOption(roundTo, "smallestUnit");

    auto maximum = 1;
    auto inclusive = true;
    if (smallestUnit != TemporalUnit::Day) {
        maximum = maximumTemporalDurationRoundingIncrement(smallestUnit);
        inclusive = false;
    }

    validateTemporalRoundingIncrement(roundingIncrement, maximum, inclusive);
    if (smallestUnit == TemporalUnit::Nanosecond && roundingIncrement == 1)
        return this;
    }

    auto thisNs = exactTime();
    auto timeZone = timeZone();
    auto isoDateTime = getISODateTimeFor(thisNs, timeZone);
    
    if (smallestUnit == TemporalUnit::Day) {
        auto dateStart = isoDateTime.plainDate();
        auto dateEnd = balanceISODate(dateStart.year(), dateStart.month(), dateStart.day() + 1);
        auto startNs = getStartOfDay(timeZone, dateStart);
        ASSERT(thisNs >= startNs);
        auto endNs = getStartOfDay(timeZone, dateEnd);
        ASSERT(thisNs < endNs);
        auto dayLengthNs = endNs - startNs;
        auto dayProgressNs = timeDurationFromEpochNanosecondsDifference(thisNs, startNs);
        auto roundedDayNs = roundTimeDurationToIncrement(dayProgressNs, dayLengthNs, roundingMode);
        auto epochNanoseconds = addTimeDurationToEpochNanoseconds(startNs, roundedDayNs);
    }
    else {
        auto roundResult = roundISODateTime(isoDateTime, roundingIncrement, smallestUnit, roundingMode);
        auto offsetNanoseconds = getOffsetNanosecondsFor(timeZone, thisNs);
        auto epochNanoseconds = interpretISODateTimeOffset(roundResult.plainDate(), roundResult.time(), offsetNanoseconds, timeZone);
    }
    return TemporalZonedDateTime::tryCreateIfValid(globalObject, globalObject->zonedDateTimeStructure, epochNanoseconds, timeZone);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tostring
String TemporalZonedDateTime::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        return toString();

    auto digits = getTemporalFractionalSecondDigitsOption(options);
    RETURN_IF_EXCEPTION(scope, { });
    auto showOffset = getTemporalShowOffsetOption(options);
    RETURN_IF_EXCEPTION(scope, { });
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });
    auto smallestUnit = getTemporalSmallestUnit(options);
    RETURN_IF_EXCEPTION(scope, { });
    if (smallestUnit == TemporalUnit::Hour) {
        throwRangeError(globalObject, "smallestUnit cannot be hour"_s);
        return { };
    }
    auto showTimeZone = getTemporalShowTimeZoneNameOption(options);
    RETURN_IF_EXCEPTION(scope, { });
    PrecisionData precision = secondsStringPrecision(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    return ISO8601::temporalZonedDateTimeToString(this, precision, showTimeZone, showOffset, precision.increment, precision.unit, roundingMode);
}


// https://tc39.es/proposal-temporal/#sec-temporal-totemporalzoneddatetime
TemporalZonedDateTime* TemporalZonedDateTime::from(JSGlobalObject* globalObject, JSValue itemValue, std::optional<JSObject*> options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto offsetBehavior = TemporalOffsetBehavior::Option;
    auto matchBehavior = TemporalMatchBehavior::MatchExactly;

    if (itemValue.isObject()) {
        if (itemValue.inherits<TemporalZonedDateTime>()) {
            if (options) {
                getTemporalDisambiguationOption(options);
                getTemporalOffsetOption(options);
                getTemporalOverflowOption(options);
            }
            return jsCast<TemporalZonedDateTime*>(itemValue);
        }

        [optionalYear, optionalMonth, optionalMonthCode, optionalDay, optionalHour, optionalMinute,
         optionalSecond, optionalMillisecond, optionalMicrosecond, optionalNanosecond, optionalOffset,
         optionalTimeZone] = prepareCalendarFields(jsCast<JSObject*>(itemValue));
        if (!optionalOffset)
            offsetBehavior = TemporalOffsetBehavior::Wall;
        if (options) {
            auto disambiguation = getTemporalDisambiguation(options);
            auto offsetOption = getTemporalOffset(options, TemporalOffset::Reject);
            auto overflow = getTemporalOverflow(options);
        }
        auto result = interpretTemporalDateTimeFields(optionalYear,
            optionalMonth, optionalMonthCode, optionalDay, optionalHour, optionalMinute,
            optionalSecond, optionalMillisecond, optionalMicrosecond, optionalNanosecond,
            overflow);
        auto isoDate = result.plainDate();
        auto time = result.time();
    }


    if (!itemValue.isString()) {
        throwTypeError(globalObject, scope, "can only convert to ZonedDateTime from object or string values"_s);
        return { };
    }

    auto string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    // Validate overflow -- see step 3(g) of ToTemporalTime
    if (options)
        toTemporalOverflow(globalObject, options.value());

    auto dateTime = ISO8601::parseDateTime(string);
    if (dateTime) {
        auto [plainDate, plainTime, timeZoneOptional, calendarOptional] = WTFMove(time.value());
        if (!timeZoneOptional) {
            throwRangeError(globalObject, scope, "string must have a time zone annotation to convert to ZonedDateTime"_s);
            return { };
        }
        auto annotation = timeZoneOptional->timeZoneAnnotation();
        ASSERT(annotation != Empty);
        auto timeZone = toTemporalTimeZoneIdentifier(annotation);
        auto offsetString = timeZoneOptional->offsetString();
        if (timeZoneOptional->z())
            offsetBehavior = TemporalOffsetBehavior::Exact;
        else if (offsetString.length() == 0)
            offsetBehavior = TemporalOffsetBehavior::Wall;
        matchBehavior = TemporalMatchBehavior::Minutes;
        auto disambiguation = getTemporalDisambiguation(options);
        auto offsetOption = getTemporalOffsetOption(options, Reject);
        getTemporalOverflowOption(options);
        auto isoDate = plainDate;
        auto time = plainTime;
    }
    auto offsetNanoseconds = 0;
    if (offsetBehavior == TemporalOffsetBehavior::Option) {
        offsetNanoseconds = parseDateTimeUTCOffset(offsetString);
    }
    auto epochNanoseconds = interpretISODateTimeOffset(isoDate, time, offsetBehavior, offsetNanoseconds, timeZone, disambiguation, offsetOption, matchBehavior);
    return TemporalZonedDateTime::tryCreateIfValid(globalObject, globalObject->zonedDateTimeStructure, epochNanoseconds, timeZone);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.compare
int32_t TemporalZonedDateTime::compare(const ISO8601::ZonedDateTime& t1, const ISO8601::ZonedDateTime& t2)
{
    return ISO8601::ExactTime::compare(t1.exactTime(), t2.exactTime());
}

} // namespace JSC
