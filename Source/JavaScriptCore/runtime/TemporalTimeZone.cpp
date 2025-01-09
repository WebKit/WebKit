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
#include "ParseInt.h"
#include "TemporalZonedDateTime.h"
#include <wtf/text/StringParsingBuffer.h>

namespace JSC {

const ClassInfo TemporalTimeZone::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalTimeZone) };

TemporalTimeZone* TemporalTimeZone::createFromID(VM& vm, Structure* structure, TimeZoneID identifier)
{
    TemporalTimeZone* format = new (NotNull, allocateCell<TemporalTimeZone>(vm)) TemporalTimeZone(vm, structure, TimeZone::named(identifier));
    format->finishCreation(vm);
    return format;
}

TemporalTimeZone* TemporalTimeZone::createFromUTCOffset(VM& vm, Structure* structure, int64_t utcOffset)
{
    TemporalTimeZone* format = new (NotNull, allocateCell<TemporalTimeZone>(vm)) TemporalTimeZone(vm, structure, TimeZone::offset(utcOffset));
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

// https://tc39.es/proposal-temporal/#sec-getnamedtimezoneepochnanoseconds
static Vector<Int128> getNamedTimeZoneEpochNanoseconds(TimeZoneID timeZoneIdentifier, ISO8601::PlainDateTime isoDateTime)
{
    // FIXME: handle other named time zones
    RELEASE_ASSERT(timeZoneIdentifier == utcTimeZoneID());
    Int128 epochNanoseconds = ISO8601::getUTCEpochNanoseconds(isoDateTime);
    return Vector<Int128> { epochNanoseconds };
}

// https://tc39.es/proposal-temporal/#sec-temporal-getpossibleepochnanoseconds
Vector<Int128> TemporalTimeZone::getPossibleEpochNanoseconds(JSGlobalObject* globalObject, ISO8601::TimeZone timeZone, ISO8601::PlainDateTime isoDateTime)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto isoDate = isoDateTime.date();
    auto isoTime = isoDateTime.time();

    Vector<Int128> possibleEpochNanoseconds;
    if (timeZone.isOffset()) {
        auto balanced = ISO8601::balanceISODateTime(isoDate.year(), isoDate.month(), isoDate.day(),
            isoTime.hour(), isoTime.minute() - timeZone.offsetMinutes(), isoTime.second(),
            isoTime.millisecond(), isoTime.microsecond(), isoTime.nanosecond());
        ISO8601::checkISODaysRange(globalObject, balanced.date());
        RETURN_IF_EXCEPTION(scope, { });
        Int128 epochNanoseconds = ISO8601::getUTCEpochNanoseconds(balanced);
        possibleEpochNanoseconds = Vector<Int128> { epochNanoseconds };
    } else {
        ISO8601::checkISODaysRange(globalObject, isoDate);
        RETURN_IF_EXCEPTION(scope, { });
        possibleEpochNanoseconds = getNamedTimeZoneEpochNanoseconds(timeZone.asID(), isoDateTime);
    }
    for (auto epochNanoseconds : possibleEpochNanoseconds) {
        if (!ISO8601::ExactTime(epochNanoseconds).isValid()) {
            throwRangeError(globalObject, scope, "invalid epochNanoseconds result in getPossibleEpochNanoseconds()"_s);
            return { };
        }
    }
    return possibleEpochNanoseconds;
}

// https://tc39.es/proposal-temporal/#sec-temporal-disambiguatepossibleepochnanoseconds
ISO8601::ExactTime TemporalTimeZone::disambiguatePossibleEpochNanoseconds(JSGlobalObject* globalObject,
    Vector<Int128> possibleEpochNs, ISO8601::TimeZone timeZone, ISO8601::PlainDateTime isoDateTime,
    TemporalDisambiguation disambiguation)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto n = possibleEpochNs.size();
    if (n == 1)
        return ISO8601::ExactTime(possibleEpochNs[0]);
    if (n != 0) {
        if (disambiguation == TemporalDisambiguation::Earlier
            || disambiguation == TemporalDisambiguation::Compatible)
            return ISO8601::ExactTime(possibleEpochNs[0]);
        if (disambiguation == TemporalDisambiguation::Later)
            return ISO8601::ExactTime(possibleEpochNs[n - 1]);
        throwRangeError(globalObject, scope, "disambiguation is Reject and multiple instants found in disambiguatePossibleEpochNanoseconds()"_s);
        return { };
    }
    // n == 0
    if (disambiguation == TemporalDisambiguation::Reject) {
        throwRangeError(globalObject, scope, "disambiguation is Reject in disambiguatePossibleEpochNanoseconds() and no possible instants"_s);
        return { };
    }
    auto utcNs = ISO8601::getUTCEpochNanoseconds(isoDateTime);
    auto dayBefore = utcNs - ISO8601::ExactTime::nsPerDay;
    if (!ISO8601::ExactTime(dayBefore).isValid()) {
        throwRangeError(globalObject, scope, "day before is not a valid instant in disambiguatePossibleEpochNanoseconds()"_s);
        return { };
    }
    auto offsetBefore = ISO8601::getOffsetNanosecondsFor(timeZone, dayBefore);
    auto dayAfter = utcNs + ISO8601::ExactTime::nsPerDay;
    auto offsetAfter = ISO8601::getOffsetNanosecondsFor(timeZone, dayAfter);
    auto nanoseconds = offsetAfter - offsetBefore;
    ASSERT(absInt128(nanoseconds) <= ISO8601::ExactTime::nsPerDay);
    
    auto isoDate = isoDateTime.date();

    if (disambiguation == TemporalDisambiguation::Earlier) {
        auto timeDuration = TemporalDuration::timeDurationFromComponents(0, 0, 0, 0, 0, -nanoseconds);
        auto earlierTime = TemporalPlainTime::addTime(isoDateTime.time(), timeDuration);
        auto earlierDate = TemporalCalendar::balanceISODate(
            isoDate.year(), isoDate.month(), isoDate.day() + earlierTime.days());
        auto earlierDateTime = TemporalDuration::combineISODateAndTimeRecord(earlierDate,
            ISO8601::PlainTime(earlierTime.hours(), earlierTime.minutes(), earlierTime.seconds(),
                earlierTime.milliseconds(), earlierTime.microseconds(), earlierTime.nanoseconds()));
        possibleEpochNs = getPossibleEpochNanoseconds(globalObject, timeZone, earlierDateTime);
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(possibleEpochNs.size() > 0);
        return ISO8601::ExactTime(possibleEpochNs[0]);
    }
    auto timeDuration = TemporalDuration::timeDurationFromComponents(0, 0, 0, 0, 0, nanoseconds);
    auto laterTime = TemporalPlainTime::addTime(isoDateTime.time(), timeDuration);
    auto laterDate = TemporalCalendar::balanceISODate(isoDate.year(), isoDate.month(), isoDate.day() - laterTime.days());
    auto laterDateTime = TemporalDuration::combineISODateAndTimeRecord(laterDate, 
        ISO8601::PlainTime(laterTime.hours(), laterTime.minutes(), laterTime.seconds(),
            laterTime.milliseconds(), laterTime.microseconds(), laterTime.nanoseconds()));
    possibleEpochNs = getPossibleEpochNanoseconds(globalObject, timeZone, laterDateTime);
    RETURN_IF_EXCEPTION(scope, { });
    n = possibleEpochNs.size();
    ASSERT(n != 0);
    return ISO8601::ExactTime(possibleEpochNs[n - 1]);
}

static Int128 beforeFirstDST
    = ISO8601::getUTCEpochNanoseconds(ISO8601::PlainDateTime(ISO8601::PlainDate(1847, 0, 1),
            ISO8601::PlainTime()));

static Int128 epochNsToMs(Int128 epochNanoseconds)
{
    auto quotient = epochNanoseconds / 1000000;
    auto remainder = epochNanoseconds % 1000000;
    auto epochMilliseconds = +quotient;
    if (+remainder < 0)
        epochMilliseconds -= 1;
    return epochMilliseconds;
}

static Int128 bisect(std::function<Int128(Int128)> const& getState, Int128 left, Int128 right, Int128 lstate, Int128 rstate)
{
    while (right - left > 1) {
        auto middle = (left + right) / 2;
        auto mstate = getState(middle);
        if (mstate == lstate) {
            left = middle;
            lstate = mstate;
        } else if (mstate == rstate) {
            right = middle;
            rstate = mstate;
        } else {
            ASSERT_NOT_REACHED();
        }
    }
    return right;
}

static Int128 getNamedTimeZoneOffsetNanosecondsImpl(TimeZoneID timeZoneIdentifier, Int128)
{

    RELEASE_ASSERT(timeZoneIdentifier == utcTimeZoneID());
    return 0;
}

// TODO: named time zones
std::optional<ISO8601::ExactTime> TemporalTimeZone::getNamedTimeZonePreviousTransition(TimeZoneID, Int128)
{
    return std::nullopt;
}

// https://tc39.es/proposal-temporal/#sec-temporal-getnamedtimezonenexttransition
std::optional<ISO8601::ExactTime> TemporalTimeZone::getNamedTimeZoneNextTransition(TimeZoneID timeZoneIdentifier,
Int128 epochNanoseconds)
{
    auto epochMilliseconds = epochNsToMs(epochNanoseconds);
    if (epochMilliseconds < beforeFirstDST)
        return getNamedTimeZoneNextTransition(timeZoneIdentifier, beforeFirstDST * 1000000);

    auto now = ISO8601::ExactTime::now();
    auto base = std::max(epochMilliseconds, now.epochNanoseconds() / 1000000);
    auto dayMs = ISO8601::ExactTime::nsPerDay / 1000000;
    auto uppercap = base + dayMs * 366 * 3;
    auto leftMs = epochMilliseconds;
    auto leftOffsetNs = getNamedTimeZoneOffsetNanosecondsImpl(timeZoneIdentifier, leftMs);
    auto rightMs = leftMs;
    auto rightOffsetNs = leftOffsetNs;
    while (leftOffsetNs == rightOffsetNs && leftMs < uppercap) {
        rightMs = leftMs + dayMs * 2 * 7;
        if (rightMs > (ISO8601::ExactTime::maxValue / 1000000))
            return std::nullopt;
        rightOffsetNs = getNamedTimeZoneOffsetNanosecondsImpl(timeZoneIdentifier, rightMs);
        if (leftOffsetNs == rightOffsetNs)
            leftMs = rightMs;
    }
    if (leftOffsetNs == rightOffsetNs)
        return std::nullopt;
    auto result = bisect([timeZoneIdentifier](Int128 epochMs)
        { return getNamedTimeZoneOffsetNanosecondsImpl(timeZoneIdentifier, epochMs); },
        leftMs, rightMs, leftOffsetNs, rightOffsetNs);
    return ISO8601::ExactTime(result * 1000000);
}

// https://tc39.es/proposal-temporal/#sec-temporal-getstartofday
ISO8601::ExactTime TemporalTimeZone::getStartOfDay(JSGlobalObject* globalObject, ISO8601::TimeZone timeZone,
    ISO8601::PlainDate isoDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto isoDateTime = TemporalDuration::combineISODateAndTimeRecord(isoDate, ISO8601::PlainTime());
    auto possibleEpochNs = getPossibleEpochNanoseconds(globalObject, timeZone, isoDateTime);
    if (possibleEpochNs.size() > 0)
        return ISO8601::ExactTime(possibleEpochNs[0]);
    ASSERT(!timeZone.isOffset());

    auto utcNs = ISO8601::getUTCEpochNanoseconds(isoDateTime);
    ISO8601::ExactTime dayBefore = ISO8601::ExactTime(utcNs - ISO8601::ExactTime::nsPerDay);
    if (!dayBefore.isValid()) {
        throwRangeError(globalObject, scope, "day before is not valid in getStartOfDay()"_s);
        return { };
    }
    auto result = getNamedTimeZoneNextTransition(timeZone.asID(), dayBefore.epochNanoseconds());
    if (!result) {
        throwRangeError(globalObject, scope, "unable to get next transition in getStartOfDay()"_s);
        return { };
    }
    return result.value();
}

// https://tc39.es/proposal-temporal/#sec-temporal-getepochnanosecondsfor
ISO8601::ExactTime TemporalTimeZone::getEpochNanosecondsFor(JSGlobalObject* globalObject,
    ISO8601::TimeZone timeZone, ISO8601::PlainDateTime isoDateTime, TemporalDisambiguation disambiguation)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto possibleEpochNs = getPossibleEpochNanoseconds(globalObject, timeZone, isoDateTime);
    RETURN_IF_EXCEPTION(scope, { });
    return disambiguatePossibleEpochNanoseconds(globalObject, possibleEpochNs, timeZone, isoDateTime, disambiguation);
}

// https://tc39.es/proposal-temporal/#sec-getavailablenamedtimezoneidentifier
std::optional<ISO8601::TimeZone> TemporalTimeZone::getAvailableNamedTimeZoneIdentifier(JSGlobalObject* globalObject,
    TimeZoneID timeZoneIdentifier)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (timeZoneIdentifier == utcTimeZoneID())
        return ISO8601::TimeZone::offset(0);
    // TODO: support named time zones
    throwRangeError(globalObject, scope, "getAvailableNamedTimeZoneIdentifier() not yet implemented"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-getavailablenamedtimezoneidentifier
std::optional<ISO8601::TimeZone> TemporalTimeZone::getAvailableNamedTimeZoneIdentifier(JSGlobalObject* globalObject,
    const Vector<LChar>& chars)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (chars.size() == 3 && chars[0] == 'U' && chars[1] == 'T' && chars[2] == 'C')
        return ISO8601::TimeZone::offset(0);
    throwRangeError(globalObject, scope, "getAvailableNamedTimeZoneIdentifier() not yet implemented"_s);
    return { };
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaltimezoneidentifier
ISO8601::TimeZone TemporalTimeZone::toTemporalTimeZoneIdentifier(JSGlobalObject* globalObject,
    JSValue temporalTimeZoneLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (temporalTimeZoneLike.isObject()) {
        if (temporalTimeZoneLike.inherits<TemporalZonedDateTime>()) {
            return jsCast<TemporalZonedDateTime*>(temporalTimeZoneLike)->timeZone();
        }
    }
    if (!temporalTimeZoneLike.isString()) {
        throwTypeError(globalObject, scope, "time zone must be ZonedDateTime or string"_s);
        return { };
    }
    auto toParse = temporalTimeZoneLike.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto parseResultOptional = TemporalTimeZone::parseTemporalTimeZoneString(toParse);
    if (!parseResultOptional) {
        throwRangeError(globalObject, scope, makeString("error parsing time zone from string "_s, toParse));
        return { };
    }
    auto parseResult = parseResultOptional.value();
    if (parseResult.isOffset()) {
        return parseResult;
    }
    auto name = parseResult.asID();
    auto timeZoneIdentifierRecord = getAvailableNamedTimeZoneIdentifier(globalObject, name);
    RETURN_IF_EXCEPTION(scope, { });
    if (!timeZoneIdentifierRecord) {
        throwRangeError(globalObject, scope, "time zone is invalid"_s);
        return { };
    }
    return timeZoneIdentifierRecord.value();
}

static std::optional<TimeZoneID> parseTimeZoneIANAName(StringView)
{
    // TODO: support named time zones
    return std::nullopt;
}

// https://tc39.es/proposal-temporal/#sec-parsetimezoneidentifier
static std::optional<ISO8601::TimeZone> parseTimeZoneIdentifier(StringView identifier)
{
    if (isUTCTimeZoneString(identifier))
        return ISO8601::TimeZone::utc();

    Vector<LChar> ignore;
    auto parseResult = ISO8601::parseUTCOffset(identifier, ignore, false); // Don't accept sub-minute precision
    bool isIANAName = false;
    if (!parseResult) {
        isIANAName = true;
        parseResult = parseTimeZoneIANAName(identifier);
    }
    if (!parseResult)
        return std::nullopt;
       
    if (isIANAName) {
        if (parseResult)
            return ISO8601::TimeZone::named(parseResult.value());
        return std::nullopt;
    }

    int64_t offsetNanoseconds = parseResult.value();
    ASSERT(offsetNanoseconds % ISO8601::ExactTime::nsPerMinute == 0);
    return ISO8601::TimeZone::offset(offsetNanoseconds);
}

static std::optional<ISO8601::TimeZone> parseTimeZoneFromAnnotation(const ISO8601::TimeZoneAnnotation& annotation)
{
    if (annotation.m_offset) {
        auto offsetNanoseconds = annotation.m_offset.value();
        ASSERT(offsetNanoseconds % ISO8601::ExactTime::nsPerMinute == 0);
        return ISO8601::TimeZone::offset(offsetNanoseconds);
    }

    return parseTimeZoneIdentifier(WTF::String(annotation.m_annotation));
}

// https://tc39.es/proposal-temporal/#prod-TimeZoneIdentifier
bool canBeTimeZoneIdentifier(StringView string)
{
    //  TimeZoneIdentifier :::
    //      UTCOffset[~SubMinutePrecision]
    //      TimeZoneIANAName
    //
    //  UTCOffset[SubMinutePrecision] :::
    //      ASCIISign Hour
    //      ASCIISign Hour TimeSeparator[+Extended] MinuteSecond
    //      ASCIISign Hour TimeSeparator[~Extended] MinuteSecond
    //      [+SubMinutePrecision] ASCIISign Hour TimeSeparator[+Extended] MinuteSecond TimeSeparator[+Extended] MinuteSecond TemporalDecimalFractionopt
    //      [+SubMinutePrecision] ASCIISign Hour TimeSeparator[~Extended] MinuteSecond TimeSeparator[~Extended] MinuteSecond TemporalDecimalFractionopt
    //
    //  TimeZoneIANAName :::
    //      TimeZoneIANANameComponent
    //      TimeZoneIANAName / TimeZoneIANANameComponent
    //
    //  TimeZoneIANANameComponent :::
    //      TZLeadingChar
    //      TimeZoneIANANameComponent TZChar
    //
    //  TZLeadingChar :::
    //      Alpha
    //      .
    //      _
    //
    if (string[0] == '+' || string[0] == '-')
        return true;
    if (isASCIIAlpha(string[0]) || string[0] == '.' || string[0] == '_')
        return true;
    return false;
}

// https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaltimezonestring
std::optional<ISO8601::TimeZone> TemporalTimeZone::parseTemporalTimeZoneString(StringView timeZoneString)
{
    if (canBeTimeZoneIdentifier(timeZoneString))
        return parseTimeZoneIdentifier(timeZoneString);
    ISO8601::TimeZoneRecord timeZoneResult;
    auto asDateTime = ISO8601::parseCalendarDateTime(timeZoneString, TemporalDateFormat::Date);
    if (asDateTime) {
        auto [date, optionalTime, optionalTimeZoneRecord, optionalCalendarRecord] = asDateTime.value();
        if (optionalTimeZoneRecord)
            timeZoneResult = optionalTimeZoneRecord.value();
        else
            return std::nullopt;
    }
    else {
        auto asExactTime = ISO8601::parseInstant(timeZoneString);
        if (asExactTime) {
            // TODO: support parsing time zone annotation from Instant
            return std::nullopt;
        } else {
            auto asTime = ISO8601::parseCalendarTime(timeZoneString);
            if (asTime) {
                auto [time, optionalTimeZoneRecord, optionalCalendarRecord] = asTime.value();
                if (optionalTimeZoneRecord)
                    timeZoneResult = optionalTimeZoneRecord.value();
                else
                    return std::nullopt;
            }
            else {
                auto asMonthDay = ISO8601::parseCalendarDateTime(timeZoneString, TemporalDateFormat::MonthDay);
                if (asMonthDay) {
                    auto [date, optionalTime, optionalTimeZoneRecord, optionalCalendarRecord] = asMonthDay.value();
                    if (optionalTimeZoneRecord)
                        timeZoneResult = optionalTimeZoneRecord.value();
                    else
                        return std::nullopt;
                }
                else {
                    auto asYearMonth = ISO8601::parseCalendarDateTime(timeZoneString, TemporalDateFormat::YearMonth);
                    if (asYearMonth) {
                        auto [date, optionalTime, optionalTimeZoneRecord, optionalCalendarRecord] = asYearMonth.value();
                        if (optionalTimeZoneRecord)
                            timeZoneResult = optionalTimeZoneRecord.value();
                        else
                            return std::nullopt;
                    } else {
                        return std::nullopt;
                    }
                }
            }
        }
    }
    if (timeZoneResult.m_annotation) {
        return parseTimeZoneFromAnnotation(timeZoneResult.m_annotation.value());
    }
    if (timeZoneResult.m_z)
        return ISO8601::TimeZone::utc();
    if (timeZoneResult.m_offset) {
        // Check for sub-minute precision in offset string
        Vector<LChar> ignore;
        auto result = ISO8601::parseUTCOffset(WTF::String(timeZoneResult.m_offset->m_offset_string),
            ignore, false);
        if (!result)
            return std::nullopt;
        return ISO8601::TimeZone::offset(timeZoneResult.m_offset->m_offset);
    }
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

    Vector<LChar> ignore;
    std::optional<int64_t> utcOffset = ISO8601::parseUTCOffset(timeZoneString, ignore);
    if (utcOffset)
        return TemporalTimeZone::createFromUTCOffset(vm, globalObject->timeZoneStructure(), utcOffset.value());

    std::optional<TimeZoneID> identifier = ISO8601::parseTimeZoneName(timeZoneString);
    if (identifier)
        return TemporalTimeZone::createFromID(vm, globalObject->timeZoneStructure(), identifier.value());

    std::optional<ISO8601::TimeZone> utcOffsetFromInstant = parseTemporalTimeZoneString(timeZoneString);
    if (utcOffsetFromInstant) {
        if (utcOffsetFromInstant->isOffset())
            return TemporalTimeZone::createFromUTCOffset(vm, globalObject->timeZoneStructure(), utcOffsetFromInstant->offsetNanoseconds());
        else
            return TemporalTimeZone::createFromID(vm, globalObject->timeZoneStructure(), utcOffsetFromInstant->asID());
    }


    throwRangeError(globalObject, scope, "argument needs to be UTC offset string, TimeZone identifier, or temporal Instant string"_s);
    return { };
}

} // namespace JSC
