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

#include "DateConstructor.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include "LazyPropertyInlines.h"
#include "ParseInt.h"
#include "TemporalDuration.h"
#include "TemporalInstant.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalTimeZone.h"
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

// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalzoneddatetime
TemporalZonedDateTime* TemporalZonedDateTime::tryCreateIfValid(JSGlobalObject* globalObject, Structure* structure, ISO8601::ExactTime&& epochNanoseconds, ISO8601::TimeZone&& timeZone)
{
    VM& vm = globalObject->vm();

    ASSERT(epochNanoseconds.isValid());

    return TemporalZonedDateTime::create(vm, structure, WTFMove(epochNanoseconds), WTFMove(timeZone));
}

// https://tc39.es/proposal-temporal/#sec-isoffsettimezoneidentifier
static bool isOffsetTimeZoneIdentifier(const ISO8601::TimeZone& t)
{
    return t.isOffset();
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.gettimezonetransition
JSValue TemporalZonedDateTime::getTimeZoneTransition(JSGlobalObject* globalObject,
    JSValue directionParam)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (directionParam.isUndefined()) {
        throwTypeError(globalObject, scope, "getTimeZoneTransition: direction param must not be undefined"_s);
        return { };
    }

    TemporalDirectionOption direction;
    if (directionParam.isString()) {
        auto paramString = directionParam.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (paramString == "next"_s)
            direction = TemporalDirectionOption::Next;
        else if (paramString == "previous"_s)
            direction = TemporalDirectionOption::Previous;
        else {
            throwRangeError(globalObject, scope, "getTimeZoneTransition: direction must be 'next' or 'previous'"_s);
            return { };
        }
    } else {
        JSObject* options = intlGetOptionsObject(globalObject, directionParam);
        RETURN_IF_EXCEPTION(scope, { });
        auto directionOptional = getDirectionOption(globalObject, options);
        RETURN_IF_EXCEPTION(scope, { });
        if (!directionOptional) {
            throwRangeError(globalObject, scope, "getTimeZoneTransition: direction option must be present"_s);
            return { };
        }
        direction = directionOptional.value();            
    }

    if (isOffsetTimeZoneIdentifier(timeZone())) {
        return jsNull();
    }
    std::optional<ExactTime> transition;
    if (direction == TemporalDirectionOption::Next)
        transition = TemporalTimeZone::getNamedTimeZoneNextTransition(timeZone().asID(),
            exactTime().epochNanoseconds());
    else
        transition = TemporalTimeZone::getNamedTimeZonePreviousTransition(timeZone().asID(),
            exactTime().epochNanoseconds());
    if (!transition)
        return jsNull();
    RELEASE_AND_RETURN(scope, TemporalZonedDateTime::tryCreateIfValid(globalObject, globalObject->zonedDateTimeStructure(), WTFMove(transition.value()), timeZone()));
}

// https://tc39.es/proposal-temporal/#sec-temporal-interpretisodatetimeoffset
static ISO8601::ExactTime interpretISODateTimeOffset(JSGlobalObject* globalObject,
    ISO8601::PlainDate isoDate, ISO8601::PlainTime time,
    TemporalOffsetBehavior offsetBehavior, int64_t offsetNanoseconds, ISO8601::TimeZone timeZone,
    TemporalDisambiguation disambiguation, TemporalOffset offsetOption,
    TemporalMatchBehavior matchBehavior)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    auto isoDateTime = TemporalDuration::combineISODateAndTimeRecord(isoDate, time);

    if (offsetBehavior == TemporalOffsetBehavior::Wall
        || (offsetBehavior == TemporalOffsetBehavior::Option && offsetOption == TemporalOffset::Ignore))
        return ISO8601::ExactTime(TemporalTimeZone::getEpochNanosecondsFor(
            globalObject, timeZone, isoDateTime, disambiguation));

    if (offsetBehavior == TemporalOffsetBehavior::Exact
        || (offsetBehavior == TemporalOffsetBehavior::Option && offsetOption == TemporalOffset::Use)) {
        auto balanced = ISO8601::balanceISODateTime(isoDate.year(), isoDate.month(), isoDate.day(),
            time.hour(), time.minute(), time.second(), time.millisecond(), time.microsecond(),
            time.nanosecond() - offsetNanoseconds);
        checkISODaysRange(globalObject, balanced.date());
        RETURN_IF_EXCEPTION(scope, { });
        auto epochNanoseconds = ISO8601::ExactTime(ISO8601::getUTCEpochNanoseconds(balanced));
        if (!epochNanoseconds.isValid()) {
            throwRangeError(globalObject, scope, "invalid epochNanoseconds result in interpretISODateTimeOffset()"_s);
            return { };
        }
        return epochNanoseconds;
    }

    ASSERT(offsetBehavior == TemporalOffsetBehavior::Option);
    ASSERT(offsetOption == TemporalOffset::Prefer
           || offsetOption == TemporalOffset::Reject);

    checkISODaysRange(globalObject, isoDate);
    RETURN_IF_EXCEPTION(scope, { });
    auto utcEpochNanoseconds = ISO8601::getUTCEpochNanoseconds(isoDateTime);
    auto possibleEpochNs = TemporalTimeZone::getPossibleEpochNanoseconds(globalObject, timeZone, isoDateTime);
    RETURN_IF_EXCEPTION(scope, { });
    for (auto candidate : possibleEpochNs) {
        auto candidateOffset = utcEpochNanoseconds - candidate;
        if (candidateOffset == offsetNanoseconds)
            return ISO8601::ExactTime(candidate);
        if (matchBehavior == TemporalMatchBehavior::Minutes) {
            Int128 increment = 60;
            increment *= 1000000000;
            auto roundedCandidateNanoseconds = roundNumberToIncrementInt128(candidateOffset, increment, RoundingMode::HalfExpand);
            if (roundedCandidateNanoseconds == offsetNanoseconds)
                return ISO8601::ExactTime(candidate);
        }
    }

    if (offsetOption == TemporalOffset::Reject) {
        throwRangeError(globalObject, scope, "User-provided offset doesn't match any instants for this time zone and date/time"_s);
        return { };
    }
    
    return TemporalTimeZone::disambiguatePossibleEpochNanoseconds(globalObject,
        possibleEpochNs, timeZone, ISO8601::PlainDateTime(isoDate, time), disambiguation);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.round
TemporalZonedDateTime* TemporalZonedDateTime::round(JSGlobalObject* globalObject, JSValue roundToValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue paramString;
    auto roundingIncrement = 1;
    auto roundingMode = RoundingMode::HalfExpand;
    std::optional<TemporalUnit> smallestUnitOptional;

    if (!roundToValue.isString()) {
        JSObject* roundTo = intlGetOptionsObject(globalObject, roundToValue);
        RETURN_IF_EXCEPTION(scope, { });

        roundingIncrement = getRoundingIncrementOption(globalObject, roundTo);
        RETURN_IF_EXCEPTION(scope, { });
        roundingMode = temporalRoundingMode(globalObject, roundTo, RoundingMode::HalfExpand);
        RETURN_IF_EXCEPTION(scope, { });
        smallestUnitOptional = temporalSmallestUnit(globalObject, roundTo, { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week });
    } else {
        auto smallestUnit = temporalUnitType(roundToValue.toWTFString(globalObject));
        RETURN_IF_EXCEPTION(scope, { });
        if (smallestUnit)
            smallestUnitOptional = temporalSmallestUnit(globalObject, smallestUnit.value(), { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week });
    }
    RETURN_IF_EXCEPTION(scope, { });

    if (!smallestUnitOptional) {
        throwRangeError(globalObject, scope, "Bad value given for smallestUnit in Temporal.ZonedDateTime.round()"_s);
        return { };
    }
    auto smallestUnit = smallestUnitOptional.value();

    auto maximum = 1;
    auto inclusive = true;
    if (smallestUnit != TemporalUnit::Day) {
        switch (smallestUnit) {
        case TemporalUnit::Hour:
            maximum = 24;
            break;
        case TemporalUnit::Minute:
        case TemporalUnit::Second:
            maximum = 60;
            break;
        case TemporalUnit::Millisecond:
        case TemporalUnit::Microsecond:
        case TemporalUnit::Nanosecond:
            maximum = 1000;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        inclusive = false;
    }

    if (!validateTemporalRoundingIncrement(roundingIncrement, maximum, inclusive)) {
        throwRangeError(globalObject, scope, "Bad value given for increment in Temporal.ZonedDateTime.round()"_s);
        return { };
    }
    if (smallestUnit == TemporalUnit::Nanosecond && roundingIncrement == 1)
        RELEASE_AND_RETURN(scope, TemporalZonedDateTime::tryCreateIfValid(globalObject, globalObject->zonedDateTimeStructure(), exactTime(), timeZone()));

    auto thisNs = m_exactTime.get();
    auto timeZone = m_timeZone;
    auto isoDateTime = getISODateTimeFor(timeZone, thisNs);
    
    ISO8601::ExactTime epochNanoseconds;
    if (smallestUnit == TemporalUnit::Day) {
        auto dateStart = isoDateTime.date();
        auto dateEnd = TemporalCalendar::balanceISODate(dateStart.year(), dateStart.month(), dateStart.day() + 1);
        auto startNs = TemporalTimeZone::getStartOfDay(globalObject, timeZone, dateStart);
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(thisNs >= startNs);
        auto endNs = TemporalTimeZone::getStartOfDay(globalObject, timeZone, dateEnd);
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(thisNs < endNs);
        Int128 dayLengthNs = endNs.epochNanoseconds() - startNs.epochNanoseconds();
        Int128 dayProgressNs = TemporalDuration::timeDurationFromEpochNanosecondsDifference(thisNs, startNs);
        std::optional<Int128> roundedDayNsOptional = ISO8601::roundTimeDurationToIncrement(dayProgressNs, dayLengthNs, roundingMode);
        if (!roundedDayNsOptional) {
            throwRangeError(globalObject, scope, "rounded time duration is out of range in Temporal.ZonedDateTime.round()"_s);
            return { };
        }
        epochNanoseconds = ISO8601::ExactTime(startNs.epochNanoseconds() + roundedDayNsOptional.value());
    }
    else {
        auto roundResult = TemporalPlainDateTime::roundISODateTime(isoDateTime, roundingIncrement, smallestUnit, roundingMode);
        auto offsetNanoseconds = ISO8601::getOffsetNanosecondsFor(timeZone, thisNs.epochNanoseconds());
        epochNanoseconds = interpretISODateTimeOffset(globalObject,
            roundResult.date(), roundResult.time(), TemporalOffsetBehavior::Option, offsetNanoseconds, timeZone,
            TemporalDisambiguation::Compatible, TemporalOffset::Prefer, TemporalMatchBehavior::Exactly);
        RETURN_IF_EXCEPTION(scope, { });
    }
    RELEASE_AND_RETURN(scope, TemporalZonedDateTime::tryCreateIfValid(globalObject, globalObject->zonedDateTimeStructure(),
        WTFMove(epochNanoseconds), WTFMove(timeZone)));
}

TemporalZonedDateTime* TemporalZonedDateTime::with(JSGlobalObject* globalObject, JSObject* temporalZonedDateTimeLike, JSValue options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!isPartialTemporalObject(globalObject, temporalZonedDateTimeLike)) {
        RETURN_IF_EXCEPTION(scope, { });
        throwTypeError(globalObject, scope, "argument to with() must be an object, must not be an instance of a time-related or date-related Temporal type, and must not have a calendar or time zone property"_s);
        return { };
    }
    RETURN_IF_EXCEPTION(scope, { });

    auto epochNs = exactTime();
    auto thisTimeZone = timeZone();
    auto thisCalendar = calendar();
    auto offsetNanoseconds = ISO8601::getOffsetNanosecondsFor(thisTimeZone, epochNs.epochNanoseconds());
    auto isoDateTime = ISO8601::getISODateTimeFor(thisTimeZone, epochNs);
    auto isoDate = isoDateTime.date();
    auto isoTime = isoDateTime.time();
    auto year = isoDate.year();
    auto month = isoDate.month();
    std::optional<WTF::String> monthCode = std::nullopt;
    auto day = isoDate.day();
    auto hour = isoTime.hour();
    auto minute = isoTime.minute();
    auto second = isoTime.second();
    auto millisecond = isoTime.millisecond();
    auto microsecond = isoTime.microsecond();
    auto nanosecond = isoTime.nanosecond();

    auto fields =  Vector { FieldName::Day, FieldName::Hour, FieldName::Microsecond, FieldName::Millisecond,
        FieldName::Minute, FieldName::Month, FieldName::MonthCode, FieldName::Nanosecond, FieldName::Offset,
        FieldName::Second, FieldName::Year };
    auto [optionalYear, optionalMonth, optionalMonthCode, optionalDay, optionalHour, optionalMinute,
        optionalSecond, optionalMillisecond, optionalMicrosecond, optionalNanosecond, optionalOffset,
        timeZoneOptional] = TemporalCalendar::prepareCalendarFields(globalObject,
            thisCalendar->identifier(), temporalZonedDateTimeLike,
            fields, std::nullopt);
    RETURN_IF_EXCEPTION(scope, { });
    year = optionalYear.value_or(year);
    month = optionalMonth.value_or(month);
    monthCode = optionalMonthCode;
    day = optionalDay.value_or(day);
    hour = optionalHour.value_or(hour);
    minute = optionalMinute.value_or(minute);
    second = optionalSecond.value_or(second);
    millisecond = optionalMillisecond.value_or(millisecond);
    microsecond = optionalMicrosecond.value_or(microsecond);
    nanosecond = optionalNanosecond.value_or(nanosecond);
    if (optionalOffset) {
        Vector<LChar> asString;
        auto offsetNanosecondsOptional = ISO8601::parseUTCOffset(optionalOffset.value(), asString, false);
        if (!offsetNanosecondsOptional) {
            throwRangeError(globalObject, scope, "invalid offset string in Temporal.ZonedDateTime.with"_s);
            return { };
        }
        offsetNanoseconds = offsetNanosecondsOptional.value();
    }
    auto resolvedOptions = intlGetOptionsObject(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    auto disambiguation = getTemporalDisambiguationOption(globalObject, resolvedOptions);
    RETURN_IF_EXCEPTION(scope, { });
    auto offset = getTemporalOffsetOption(globalObject, resolvedOptions, TemporalOffset::Prefer);
    RETURN_IF_EXCEPTION(scope, { });
    auto overflow = toTemporalOverflow(globalObject, resolvedOptions);
    RETURN_IF_EXCEPTION(scope, { });
    auto dateTimeResult = TemporalCalendar::interpretTemporalDateTimeFields(globalObject,
        thisCalendar->identifier(), year, month, monthCode, day, hour, minute, second, millisecond,
        microsecond, nanosecond, overflow);
    RETURN_IF_EXCEPTION(scope, { });
    auto epochNanoseconds = interpretISODateTimeOffset(globalObject, dateTimeResult.date(),
        dateTimeResult.time(), TemporalOffsetBehavior::Option, offsetNanoseconds, thisTimeZone,
        disambiguation, offset, TemporalMatchBehavior::Exactly);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, TemporalZonedDateTime::tryCreateIfValid(globalObject,
        globalObject->zonedDateTimeStructure(), WTFMove(epochNanoseconds), WTFMove(thisTimeZone)));
}

// https://tc39.es/proposal-temporal/#sec-temporal-addzoneddatetime
static ISO8601::ExactTime addZonedDateTime(JSGlobalObject* globalObject, ISO8601::ExactTime epochNanoseconds,
    ISO8601::TimeZone timeZone, TemporalCalendar* calendar, ISO8601::InternalDuration duration,
    TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // TODO: support non-ISO8601 calendars
    (void) calendar;

    if (!duration.sign())
        return TemporalInstant::addInstant(globalObject, epochNanoseconds, duration.time());
    auto isoDateTime = ISO8601::getISODateTimeFor(timeZone, epochNanoseconds);
    auto addedDate = TemporalCalendar::isoDateAdd(globalObject, isoDateTime.date(),
        duration.dateDuration(), overflow);
    RETURN_IF_EXCEPTION(scope, { });
    auto intermediateDateTime =
        TemporalDuration::combineISODateAndTimeRecord(addedDate, isoDateTime.time());
    if (!isoDateTimeWithinLimits(intermediateDateTime)) {
        throwRangeError(globalObject, scope, "result of adding duration to ZonedDateTime is out of range"_s);
        return { };
    }
    auto intermediateNs = TemporalTimeZone::getEpochNanosecondsFor(globalObject, timeZone,
        intermediateDateTime, TemporalDisambiguation::Compatible);
    RETURN_IF_EXCEPTION(scope, { });
    return TemporalInstant::addInstant(globalObject, intermediateNs, duration.time());
}

// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtozoneddatetime
TemporalZonedDateTime* TemporalZonedDateTime::addDurationToZonedDateTime(JSGlobalObject* globalObject,
    bool isAdd, ISO8601::Duration duration, JSObject* options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!isAdd)
        duration = -duration;
    auto overflow = toTemporalOverflow(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    auto internalDuration = TemporalDuration::toInternalDuration(globalObject, duration);
    RETURN_IF_EXCEPTION(scope, { });
    // TODO: handle other calendars
    TemporalCalendar* calendar = TemporalCalendar::create(vm, globalObject->calendarStructure(), iso8601CalendarID());
    RETURN_IF_EXCEPTION(scope, { });
    auto epochNanoseconds = addZonedDateTime(globalObject, exactTime(), timeZone(),
        calendar, internalDuration, overflow);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, TemporalZonedDateTime::tryCreateIfValid(globalObject, globalObject->zonedDateTimeStructure(), WTFMove(epochNanoseconds), timeZone()));
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencezoneddatetime
static ISO8601::InternalDuration differenceZonedDateTime(JSGlobalObject* globalObject,
    ISO8601::ExactTime ns1, ISO8601::ExactTime ns2, ISO8601::TimeZone timeZone,
    TemporalCalendar*, TemporalUnit largestUnit)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (ns1 == ns2)
        RELEASE_AND_RETURN(scope, ISO8601::InternalDuration::combineDateAndTimeDuration(globalObject,
            ISO8601::Duration(), 0));
    auto startDateTime = getISODateTimeFor(timeZone, ns1);
    auto startDate = startDateTime.date();
    auto startTime = startDateTime.time();
    auto endDateTime = getISODateTimeFor(timeZone, ns2);
    auto endDate = endDateTime.date();
    auto sign = (ns2.epochNanoseconds() - ns1.epochNanoseconds() < 0) ? -1 : 1;
    auto maxDayCorrection = (sign == 1) ? 2 : 1;
    auto dayCorrection = 0;
    auto timeDuration = TemporalPlainTime::differenceTime(startTime, endDateTime.time());
    if (TemporalDuration::timeDurationSign(timeDuration) == -sign)
        dayCorrection++;
    auto success = false;
    ISO8601::PlainDateTime intermediateDateTime;
    while (dayCorrection <= maxDayCorrection && !success) {
        auto intermediateDate = TemporalCalendar::balanceISODate(endDate.year(),
            endDate.month(), endDate.day() - (dayCorrection * sign));
        intermediateDateTime = TemporalDuration::combineISODateAndTimeRecord(intermediateDate,
            startTime);
        auto intermediateNs = TemporalTimeZone::getEpochNanosecondsFor(globalObject, timeZone,
            intermediateDateTime, TemporalDisambiguation::Compatible);
        RETURN_IF_EXCEPTION(scope, { });
        timeDuration = TemporalDuration::timeDurationFromEpochNanosecondsDifference(ns2, intermediateNs);
        auto timeSign = TemporalDuration::timeDurationSign(timeDuration);
        if (sign != -timeSign)
            success = true;
        dayCorrection++;
    }
    ASSERT(success);
    auto dateLargestUnit = largestUnit < TemporalUnit::Day ? largestUnit : TemporalUnit::Day;
    auto dateDifference = TemporalCalendar::calendarDateUntil(startDate,
        intermediateDateTime.date(), dateLargestUnit);
    RELEASE_AND_RETURN(scope, ISO8601::InternalDuration::combineDateAndTimeDuration(globalObject,
        dateDifference, timeDuration));
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencezoneddatetimewithrounding
static ISO8601::InternalDuration differenceZonedDateTimeWithRounding(JSGlobalObject* globalObject,
    ISO8601::ExactTime ns1, ISO8601::ExactTime ns2, ISO8601::TimeZone timeZone,
    TemporalCalendar* calendar, TemporalUnit largestUnit, double roundingIncrement,
    TemporalUnit smallestUnit, RoundingMode roundingMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (largestUnit > TemporalUnit::Day)
        return ns1.difference(globalObject, ns2, roundingIncrement, smallestUnit, roundingMode);

    auto difference = differenceZonedDateTime(globalObject, ns1, ns2,
        timeZone, calendar, largestUnit);
    RETURN_IF_EXCEPTION(scope, { });
    if (smallestUnit == TemporalUnit::Nanosecond && roundingIncrement == 1)
        return difference;

    auto dateTime = getISODateTimeFor(timeZone, ns1);
    RELEASE_AND_RETURN(scope, TemporalDuration::roundRelativeDuration(globalObject,
        difference, ns2.epochNanoseconds(), dateTime, timeZone,
        largestUnit, roundingIncrement, smallestUnit, roundingMode));
}

// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalzoneddatetime
ISO8601::Duration TemporalZonedDateTime::differenceTemporalZonedDateTime(bool isSince, JSGlobalObject* globalObject,
    JSValue options, TemporalZonedDateTime* other)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [smallestUnit, largestUnit, roundingMode, increment] = extractDifferenceOptions(globalObject, options, UnitGroup::DateTime, TemporalUnit::Nanosecond, TemporalUnit::Hour);
    RETURN_IF_EXCEPTION(scope, { });

    if (largestUnit > TemporalUnit::Day) {
        if (isSince)
            roundingMode = negateTemporalRoundingMode(roundingMode);
        auto internalDuration = exactTime().difference(globalObject, other->exactTime(), increment, smallestUnit, roundingMode);
        auto result = TemporalDuration::temporalDurationFromInternal(internalDuration, largestUnit);
        if (isSince)
            result = -result;
        return result;
    }
    if (timeZone() != other->timeZone()) {
        throwRangeError(globalObject, scope, "time zones must match"_s);
        return { };
    }
    if (exactTime() == other->exactTime()) {
        return ISO8601::Duration();
    }
    if (isSince)
        roundingMode = negateTemporalRoundingMode(roundingMode);
    auto internalDuration = differenceZonedDateTimeWithRounding(globalObject,
         exactTime(), other->exactTime(), timeZone(),
         calendar(), largestUnit, increment, smallestUnit, roundingMode);
    RETURN_IF_EXCEPTION(scope, { });
    auto result = TemporalDuration::temporalDurationFromInternal(internalDuration, TemporalUnit::Hour);
    if (isSince)
        result = -result;
    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.since
ISO8601::Duration TemporalZonedDateTime::since(JSGlobalObject* globalObject, JSValue options, TemporalZonedDateTime* other)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_AND_RETURN(scope, differenceTemporalZonedDateTime(true, globalObject, options, other));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.until
ISO8601::Duration TemporalZonedDateTime::until(JSGlobalObject* globalObject, JSValue options, TemporalZonedDateTime* other)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_AND_RETURN(scope, differenceTemporalZonedDateTime(false, globalObject, options, other));
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

    auto showCalendar = getTemporalShowCalendarNameOption(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    TemporalFractionalSecondDigits digits =
        temporalFractionalSecondDigits(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    auto showOffset = getTemporalShowOffsetOption(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });
    std::optional<TemporalUnit> smallestUnit = temporalSmallestUnit(globalObject, options,
        { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day });
    RETURN_IF_EXCEPTION(scope, { });
    if (smallestUnit == TemporalUnit::Hour) {
        throwRangeError(globalObject, scope, "smallestUnit cannot be hour"_s);
        return { };
    }
    auto showTimeZone = getTemporalShowTimeZoneNameOption(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    PrecisionData precision = secondsStringPrecision(smallestUnit, digits);
    RETURN_IF_EXCEPTION(scope, { });

    return ISO8601::temporalZonedDateTimeToString(m_exactTime.get(), m_timeZone, precision, showCalendar, showTimeZone, showOffset, precision.increment, precision.unit, roundingMode);
}

static bool isUTCTimeZoneAnnotation(std::optional<ISO8601::TimeZoneAnnotation>& annotation)
{
    if (!annotation)
        return false;
    return isUTCTimeZoneString(WTF::String(annotation.value().m_annotation));
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalzoneddatetime
TemporalZonedDateTime* TemporalZonedDateTime::from(JSGlobalObject* globalObject, JSValue itemValue, std::optional<JSValue> optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto offsetBehavior = TemporalOffsetBehavior::Option;
    auto matchBehavior = TemporalMatchBehavior::Exactly;
    auto disambiguation = TemporalDisambiguation::Compatible;
    TemporalOffset offsetOption = TemporalOffset::Reject;
    auto overflow = TemporalOverflow::Constrain;
    std::optional<String> offsetString;
    TimeZone timeZone;

    ISO8601::PlainDate isoDate;
    ISO8601::PlainTime time;

    if (itemValue.isObject()) {
        std::optional<JSObject*> options = std::nullopt;
        if (optionsValue) {
            options = intlGetOptionsObject(globalObject, optionsValue.value());
            RETURN_IF_EXCEPTION(scope, { });
        }

        if (itemValue.inherits<TemporalZonedDateTime>()) {
            if (options) {
                getTemporalDisambiguationOption(globalObject, options.value());
                RETURN_IF_EXCEPTION(scope, { });
                getTemporalOffsetOption(globalObject, options.value(), TemporalOffset::Reject);
                RETURN_IF_EXCEPTION(scope, { });
                toTemporalOverflow(globalObject, options.value());
                RETURN_IF_EXCEPTION(scope, { });
            }
            auto zdt = jsCast<TemporalZonedDateTime*>(itemValue);
            RELEASE_AND_RETURN(scope, TemporalZonedDateTime::tryCreateIfValid(globalObject,
                globalObject->zonedDateTimeStructure(), zdt->exactTime(), zdt->timeZone()));
        }

        auto item = jsCast<JSObject*>(itemValue);
        CalendarID calendar = TemporalCalendar::getTemporalCalendarIdentifierWithISODefault(globalObject, item);
        RETURN_IF_EXCEPTION(scope, { });
        auto [optionalYear, optionalMonth, optionalMonthCode, optionalDay, optionalHour, optionalMinute,
            optionalSecond, optionalMillisecond, optionalMicrosecond, optionalNanosecond, optionalOffset,
            timeZoneOptional] = TemporalCalendar::prepareCalendarFields(globalObject, calendar, item,
            Vector { FieldName::Day, FieldName::Hour, FieldName::Microsecond, FieldName::Millisecond, FieldName::Minute, FieldName::Month, FieldName::MonthCode, FieldName::Nanosecond, FieldName::Offset, FieldName::Second, FieldName::TimeZone, FieldName::Year }, Vector { FieldName::TimeZone });
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(timeZoneOptional);
        timeZone = timeZoneOptional.value();
        offsetString = optionalOffset;
        if (!optionalOffset)
            offsetBehavior = TemporalOffsetBehavior::Wall;
        if (options) {
            disambiguation = getTemporalDisambiguationOption(globalObject, options.value());
            RETURN_IF_EXCEPTION(scope, { });
            offsetOption = getTemporalOffsetOption(globalObject, options.value(), TemporalOffset::Reject);
            RETURN_IF_EXCEPTION(scope, { });
            overflow = toTemporalOverflow(globalObject, options.value());
            RETURN_IF_EXCEPTION(scope, { });
        }
        auto result = TemporalCalendar::interpretTemporalDateTimeFields(globalObject, calendar, optionalYear,
            optionalMonth, optionalMonthCode, optionalDay, optionalHour.value_or(0),
            optionalMinute.value_or(0), optionalSecond.value_or(0), optionalMillisecond.value_or(0),
            optionalMicrosecond.value_or(0), optionalNanosecond.value_or(0), overflow);
        RETURN_IF_EXCEPTION(scope, { });
        isoDate = result.date();
        time = result.time();
    } else {
        if (!itemValue.isString()) {
            throwTypeError(globalObject, scope, "can only convert to ZonedDateTime from object or string values"_s);
            return { };
        }

        auto string = itemValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        
        auto dateTime = ISO8601::parseTemporalDateTimeString(string);
        if (!dateTime) {
            throwRangeError(globalObject, scope, makeString("in Temporal.ZonedDateTime.from, error parsing "_s, string));
            return { };
        }

        auto [plainDate, plainTimeOptional, timeZoneOptional, calendarOptional] = WTFMove(dateTime.value());
        if (!timeZoneOptional) {
            throwRangeError(globalObject, scope, "string must have a time zone annotation to convert to ZonedDateTime"_s);
            return { };
    }
        if (!(timeZoneOptional->m_z || timeZoneOptional->m_annotation->m_offset || isUTCTimeZoneAnnotation(timeZoneOptional->m_annotation))) {
            throwRangeError(globalObject, scope, "in Temporal.ZonedDateTime, parsing strings with named time zones not implemented yet"_s);
            return { };
        }

        auto annotation = timeZoneOptional->m_annotation;
        if (!annotation) {
            throwRangeError(globalObject, scope, "Temporal.ZonedDateTime requires a time zone ID in brackets"_s);
            return { };
        }
        timeZone = TemporalTimeZone::toTemporalTimeZoneIdentifier(globalObject,
            jsString(vm, WTF::String(annotation->m_annotation)));
        RETURN_IF_EXCEPTION(scope, { });
        if (timeZoneOptional->m_offset)
            offsetString = WTF::String(timeZoneOptional->m_offset->m_offset_string);
        if (timeZoneOptional->m_z)
            offsetBehavior = TemporalOffsetBehavior::Exact;
        else if (!offsetString)
            offsetBehavior = TemporalOffsetBehavior::Wall;
        matchBehavior = TemporalMatchBehavior::Minutes;
        if (optionsValue) {
            JSObject* options = intlGetOptionsObject(globalObject, optionsValue.value());
            RETURN_IF_EXCEPTION(scope, { });
            disambiguation = getTemporalDisambiguationOption(globalObject, options);
            RETURN_IF_EXCEPTION(scope, { });
            offsetOption = getTemporalOffsetOption(globalObject, options, TemporalOffset::Reject);
            RETURN_IF_EXCEPTION(scope, { });
            toTemporalOverflow(globalObject, options);
            RETURN_IF_EXCEPTION(scope, { });
        }
        isoDate = plainDate;
        time = plainTimeOptional.value_or(ISO8601::PlainTime());
    }
    int64_t offsetNanoseconds = 0;
    if (offsetBehavior == TemporalOffsetBehavior::Option) {
        if (!offsetString) {
            throwRangeError(globalObject, scope, "missing offset in ZonedDateTime.from"_s);
            return { };
        }
        Vector<LChar> ignore;
        std::optional<int64_t> offsetNanosecondsOptional = ISO8601::parseUTCOffset(offsetString.value(), ignore, true);
        if (!offsetNanosecondsOptional) {
            throwRangeError(globalObject, scope, "error parsing offset in ZonedDateTime.from"_s);
            return { };
        }
        offsetNanoseconds = offsetNanosecondsOptional.value();
    }
    auto epochNanoseconds = interpretISODateTimeOffset(globalObject, isoDate, time, offsetBehavior, offsetNanoseconds, timeZone, disambiguation, offsetOption, matchBehavior);
    RETURN_IF_EXCEPTION(scope, { });
    return TemporalZonedDateTime::tryCreateIfValid(globalObject, globalObject->zonedDateTimeStructure(),
        WTFMove(epochNanoseconds), WTFMove(timeZone));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.compare
int32_t TemporalZonedDateTime::compare(const TemporalZonedDateTime* t1, const TemporalZonedDateTime* t2)
{
    return ISO8601::ExactTime::compare(t1->m_exactTime.get(), t2->m_exactTime.get());
}

} // namespace JSC
