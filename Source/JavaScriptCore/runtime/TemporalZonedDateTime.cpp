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
#include "TemporalDuration.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
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

// https://tc39.es/proposal-temporal/#sec-validatetemporalroundingincrement
static bool validateTemporalRoundingIncrement(unsigned increment, unsigned dividend, bool inclusive)
{
    unsigned maximum;
    if (inclusive)
        maximum = dividend;
    else {
        ASSERT(dividend > 1);
        maximum = dividend - 1;
    }
    if (increment > maximum)
        return false;
    if (dividend % increment != 0)
        return false;
    return true;
}

static Int128 getNamedTimeZoneOffsetNanosecondsImpl(TimeZoneID timeZoneIdentifier, Int128 epochMilliseconds)
{
    (void) epochMilliseconds;

    // FIXME: handle other named time zones
    ASSERT(timeZoneIdentifier == utcTimeZoneID());
    return 0;
}

// https://tc39.es/ecma262/#sec-getnamedtimezoneoffsetnanoseconds
static Int128 getNamedTimeZoneOffsetNanoseconds(TimeZoneID timeZoneIdentifier, Int128 epochNanoseconds)
{
    (void) epochNanoseconds;

    // FIXME: handle other named time zones
    ASSERT(timeZoneIdentifier == utcTimeZoneID());
    return 0;
}

// https://tc39.es/proposal-temporal/#sec-getnamedtimezoneepochnanoseconds
static Vector<ISO8601::ExactTime> getNamedTimeZoneEpochNanoseconds(TimeZoneID timeZoneIdentifier, std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> isoDateTime)
{
    // FIXME: handle other named time zones
    ASSERT(timeZoneIdentifier == utcTimeZoneID());
    auto epochNanoseconds = TemporalDuration::getUTCEpochNanoseconds(isoDateTime);
    return Vector<ISO8601::ExactTime>(epochNanoseconds);
}

// https://tc39.es/proposal-temporal/#sec-temporal-getoffsetnanosecondsfor
static Int128 getOffsetNanosecondsFor(ISO8601::TimeZone timeZone, Int128 epochNs)
{
    if (std::holds_alternative<int64_t>(timeZone))
        return std::get<int64_t>(timeZone) * 60 * 1000000000;
    return getNamedTimeZoneOffsetNanoseconds(std::get<TimeZoneID>(timeZone), epochNs);
}

// https://tc39.es/ecma262/#sec-hourfromtime
static unsigned hourFromTime(Int128 t)
{
    return std::trunc(std::fmod(t / msPerHour, WTF::hoursPerDay));
}

// https://tc39.es/ecma262/#sec-minfromtime
static unsigned minFromTime(Int128 t)
{
    return std::trunc(std::fmod(t / msPerMinute, minutesPerHour));
}

// https://tc39.es/ecma262/#sec-secfromtime
static unsigned secFromTime(Int128 t)
{
    return std::trunc(std::fmod(t / msPerSecond, secondsPerMinute));
}

// https://tc39.es/ecma262/#sec-msfromtime
static unsigned msFromTime(Int128 t)
{
    return std::trunc(std::fmod(t, msPerSecond));
}


// https://tc39.es/proposal-temporal/#sec-temporal-getisopartsfromepoch
static std::tuple<ISO8601::PlainDate, ISO8601::PlainTime>
getISOPartsFromEpoch(ISO8601::ExactTime epochNanoseconds)
{
    ASSERT(epochNanoseconds.isValid());
    Int128 remainderNs = epochNanoseconds.epochNanoseconds() % 1000000;
    auto epochMilliseconds = (epochNanoseconds.epochNanoseconds() - remainderNs) / 1000000;
    auto year = TemporalCalendar::epochTimeToEpochYear(epochMilliseconds);
    auto month = TemporalCalendar::epochTimeToMonthInYear(epochMilliseconds) + 1;
    auto day = TemporalCalendar::epochTimeToDate(epochMilliseconds);
    auto hour = hourFromTime(epochMilliseconds);
    auto minute = minFromTime(epochMilliseconds);
    auto second = secFromTime(epochMilliseconds);
    auto millisecond = msFromTime(epochMilliseconds);
    auto microsecond = remainderNs / 1000;
    ASSERT(microsecond < 1000);
    auto nanosecond = remainderNs % 1000;
    auto isoDate = ISO8601::PlainDate(year, month, day);
    auto time = ISO8601::PlainTime(hour, minute, second, millisecond, microsecond, nanosecond);
    return std::tuple<ISO8601::PlainDate, ISO8601::PlainTime>(isoDate, time);
}

// https://tc39.es/proposal-temporal/#sec-temporal-balanceisodatetime
static std::tuple<ISO8601::PlainDate, ISO8601::PlainTime>
balanceISODateTime(double year, double month, double day, double hour, double minute,
    double second, double millisecond, double microsecond, double nanosecond)
{
    auto balancedTime = TemporalPlainTime::balanceTime(
        hour, minute, second, millisecond, microsecond, nanosecond);
    auto balancedDate = TemporalCalendar::balanceISODate(year, month, day + balancedTime.days());
    return std::tuple<ISO8601::PlainDate, ISO8601::PlainTime>(balancedDate,
        ISO8601::PlainTime(balancedTime.hours(), balancedTime.minutes(),
            balancedTime.seconds(), balancedTime.milliseconds(),
            balancedTime.microseconds(), balancedTime.nanoseconds()));
}

// https://tc39.es/proposal-temporal/#sec-temporal-getisodatetimefor
static std::tuple<ISO8601::PlainDate, ISO8601::PlainTime>
getISODateTimeFor(ISO8601::TimeZone timeZone, ISO8601::ExactTime epochNs)
{
    auto offsetNanoseconds = getOffsetNanosecondsFor(timeZone, epochNs.epochNanoseconds());
    auto [dateResult, timeResult] = getISOPartsFromEpoch(epochNs);
    return balanceISODateTime(dateResult.year(), dateResult.month(), dateResult.day(), timeResult.hour(), timeResult.minute(), timeResult.second(), timeResult.millisecond(), timeResult.microsecond(), timeResult.nanosecond() + offsetNanoseconds);
}

// https://tc39.es/proposal-temporal/#sec-checkisodaysrange
void checkISODaysRange(JSGlobalObject* globalObject, ISO8601::PlainDate isoDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (absInt128(makeDay(isoDate.year(), isoDate.month() - 1, isoDate.day())) > 100000000)
        throwRangeError(globalObject, scope, "date out of range in checkISODaysRange()"_s);
}

// https://tc39.es/proposal-temporal/#sec-temporal-getpossibleepochnanoseconds
static Vector<ISO8601::ExactTime> getPossibleEpochNanoseconds(JSGlobalObject* globalObject, ISO8601::TimeZone timeZone, std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> isoDateTime)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto isoDate = std::get<0>(isoDateTime);
    auto isoTime = std::get<1>(isoDateTime);

    Vector<ISO8601::ExactTime> possibleEpochNanoseconds;
    if (std::holds_alternative<int64_t>(timeZone)) {
        auto balanced = balanceISODateTime(isoDate.year(), isoDate.month(), isoDate.day(),
            isoTime.hour(), isoTime.minute() - std::get<int64_t>(timeZone), isoTime.second(),
            isoTime.millisecond(), isoTime.microsecond(), isoTime.nanosecond());
        checkISODaysRange(globalObject, std::get<0>(balanced));
        RETURN_IF_EXCEPTION(scope, { });
        auto epochNanoseconds = TemporalDuration::getUTCEpochNanoseconds(balanced);
        possibleEpochNanoseconds = Vector<ISO8601::ExactTime>(epochNanoseconds);
    } else {
        checkISODaysRange(globalObject, isoDate);
        RETURN_IF_EXCEPTION(scope, { });
        possibleEpochNanoseconds = getNamedTimeZoneEpochNanoseconds(std::get<TimeZoneID>(timeZone), isoDateTime);
    }
    for (auto epochNanoseconds : possibleEpochNanoseconds) {
        if (!epochNanoseconds.isValid()) {
            throwRangeError(globalObject, scope, "invalid epochNansoeconds result in getPossibleEpochNanoseconds()"_s);
            return { };
        }
    }
    return possibleEpochNanoseconds;
}

static Int128 beforeFirstDST
    = TemporalDuration::getUTCEpochNanoseconds(
        std::tuple<ISO8601::PlainDate, ISO8601::PlainTime>(ISO8601::PlainDate(1847, 0, 1),
            ISO8601::PlainTime()));

Int128 epochNsToMs(Int128 epochNanoseconds)
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

// https://tc39.es/proposal-temporal/#sec-temporal-getnamedtimezonenexttransition
std::optional<ISO8601::ExactTime> getNamedTimeZoneNextTransition(TimeZoneID timeZoneIdentifier, Int128 epochNanoseconds)
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
static ISO8601::ExactTime getStartOfDay(JSGlobalObject* globalObject, ISO8601::TimeZone timeZone, ISO8601::PlainDate isoDate)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto isoDateTime = TemporalDuration::combineISODateAndTimeRecord(isoDate, ISO8601::PlainTime());
    auto possibleEpochNs = getPossibleEpochNanoseconds(globalObject, timeZone, isoDateTime);
    if (possibleEpochNs.size() > 0)
        return possibleEpochNs[0];
    ASSERT(!std::holds_alternative<int64_t>(timeZone));

    auto utcNs = TemporalDuration::getUTCEpochNanoseconds(isoDateTime);
    ISO8601::ExactTime dayBefore = ISO8601::ExactTime(utcNs - ISO8601::ExactTime::nsPerDay);
    if (!dayBefore.isValid()) {
        throwRangeError(globalObject, scope, "day before is not valid in getStartOfDay()"_s);
        return { };
    }
    auto result = getNamedTimeZoneNextTransition(std::get<TimeZoneID>(timeZone), dayBefore.epochNanoseconds());
    if (!result) {
        throwRangeError(globalObject, scope, "unable to get next transition in getStartOfDay()"_s);
        return { };
    }
    return result.value();
}

// https://tc39.es/proposal-temporal/#sec-temporal-timedurationfromepochnanosecondsdifference
static Int128 timeDurationFromEpochNanosecondsDifference(ISO8601::ExactTime one, ISO8601::ExactTime two)
{
    auto result = one.epochNanoseconds() - two.epochNanoseconds();
    ASSERT(absInt128(result) <= ISO8601::ExactTime::maxValue);
    return result;
}

// https://tc39.es/proposal-temporal/#sec-temporal-roundtimedurationtoincrement
static std::optional<Int128> roundTimeDurationToIncrement(Int128 d, unsigned increment, RoundingMode roundingMode)
{
    auto rounded = roundNumberToIncrementInt128(d, increment, roundingMode);
    if (absInt128(rounded) > ISO8601::ExactTime::maxValue)
        return std::nullopt;
    return rounded;
}

// https://tc39.es/proposal-temporal/#sec-temporal-roundisodatetime
static std::tuple<ISO8601::PlainDate, ISO8601::PlainTime>
roundISODateTime(std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> isoDateTime,
    unsigned increment, TemporalUnit unit, RoundingMode roundingMode)
{
    auto isoDate = std::get<0>(isoDateTime);
    auto isoTime = std::get<1>(isoDateTime);

    ASSERT(ISO8601::isDateTimeWithinLimits(isoDate.year(), isoDate.month(), isoDate.day(),
        isoTime.hour(), isoTime.minute(), isoTime.second(), isoTime.millisecond(),
        isoTime.microsecond(), isoTime.nanosecond()));
    auto roundedTime = TemporalPlainTime::roundTime(isoTime, increment, unit, roundingMode, std::nullopt);

    auto balanceResult = TemporalCalendar::balanceISODate(
        isoDate.year(), isoDate.month(), isoDate.day() + roundedTime.days());
    return TemporalDuration::combineISODateAndTimeRecord(balanceResult,
        ISO8601::PlainTime(roundedTime.hours(), roundedTime.minutes(), roundedTime.seconds(),
            roundedTime.milliseconds(), roundedTime.microseconds(), roundedTime.nanoseconds()));
}

// https://tc39.es/proposal-temporal/#sec-temporal-disambiguatepossibleepochnanoseconds
static ISO8601::ExactTime disambiguatePossibleEpochNanoseconds(Vector<ExactTime> possibleEpochNs,
    TimeZone timeZone, std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> isoDateTime,
    TemporalDisambiguation disambiguation)
{
    auto n = possibleEpochNs.size();
}

// https://tc39.es/proposal-temporal/#sec-temporal-interpretisodatetimeoffset
static ISO8601::ExactTime interpretISODateTimeOffset(JSGlobalObject* globalObject,
    ISO8601::PlainDate isoDate, ISO8601::PlainTime time,
    TemporalOffsetBehavior offsetBehavior, unsigned offsetNanoseconds, TimeZoneID timeZone,
    TemporalDisambiguation disambiguation, TemporalOffsetOption offsetOption,
    TemporalMatchBehavior matchBehavior)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    auto isoDateTime = TemporalDuration::combineISODateAndTimeRecord(isoDate, time);

    if (offsetBehavior == TemporalOffsetBehavior::Wall
        || (offsetBehavior == TemporalOffsetBehavior::Option && offsetOption == TemporalOffsetOption::Ignore))
        return ISO8601::ExactTime(TemporalDuration::getEpochNanosecondsFor(timeZone, isoDateTime, disambiguation));

    if (offsetBehavior == TemporalOffsetBehavior::Exact
        || (offsetBehavior == TemporalOffsetBehavior::Option && offsetOption == TemporalOffsetOption::Use)) {
        auto balanced = balanceISODateTime(isoDate.year(), isoDate.month(), isoDate.day(),
            time.hour(), time.minute(), time.second(), time.millisecond(), time.microsecond(),
            time.nanosecond() - offsetNanoseconds);
        checkISODaysRange(globalObject, std::get<0>(balanced));
        RETURN_IF_EXCEPTION(scope, { });
        auto epochNanoseconds = ISO8601::ExactTime(TemporalDuration::getUTCEpochNanoseconds(balanced));
        if (!epochNanoseconds.isValid()) {
            throwRangeError(globalObject, scope, "invalid epochNanoseconds result in interpretISODateTimeOffset()"_s);
            return { };
        }
        return epochNanoseconds;
    }

    ASSERT(offsetBehavior == TemporalOffsetBehavior::Option);
    ASSERT(offsetOption == TemporalOffsetOption::Prefer
           || offsetOption == TemporalOffsetOption::Reject);

    checkISODaysRange(globalObject, isoDate);
    RETURN_IF_EXCEPTION(scope, { });
    auto utcEpochNanoseconds = TemporalDuration::getUTCEpochNanoseconds(isoDateTime);
    auto possibleEpochNs = getPossibleEpochNanoseconds(globalObject, timeZone, isoDateTime);
    RETURN_IF_EXCEPTION(scope, { });
    for (auto candidate : possibleEpochNs) {
        auto candidateOffset = utcEpochNanoseconds - candidate.epochNanoseconds();
        if (candidateOffset == offsetNanoseconds)
            return candidate;
        if (matchBehavior == TemporalMatchBehavior::Minutes) {
            Int128 increment = 60;
            increment *= 1000000000;
            auto roundedCandidateNanoseconds = roundNumberToIncrementInt128(candidateOffset, increment, RoundingMode::HalfExpand);
            if (roundedCandidateNanoseconds == offsetNanoseconds)
                return candidate;
        }
    }

    if (offsetOption == TemporalOffsetOption::Reject) {
        throwRangeError(globalObject, scope, "no matching offset and offsetOption is reject in interpretISODateTimeOffset"_s);
        return { };
    }
    
    return disambiguatePossibleEpochNanoseconds(possibleEpochNs, timeZone, isoDate, time, disambiguation);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.round
TemporalZonedDateTime* TemporalZonedDateTime::round(JSGlobalObject* globalObject, JSValue roundToValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue paramString;
    JSObject* roundTo;
    if (!roundToValue.isString())
        roundTo = intlGetOptionsObject(globalObject, roundToValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto roundingIncrement = doubleNumberOption(globalObject, roundTo, vm.propertyNames->roundingIncrement, 1);
    RETURN_IF_EXCEPTION(scope, { });
    auto roundingMode = temporalRoundingMode(globalObject, roundTo, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });
    std::optional<TemporalUnit> smallestUnitOptional = roundToValue.isString()
        ? temporalUnitType(roundToValue.toWTFString(globalObject))
        : temporalSmallestUnit(globalObject, roundTo, { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day }).value_or(TemporalUnit::Day);
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
        inclusive = false;
    }

    if (!validateTemporalRoundingIncrement(roundingIncrement, maximum, inclusive)) {
        throwRangeError(globalObject, scope, "Bad value given for increment in Temporal.ZonedDateTime.round()"_s);
        return { };
    }
    if (smallestUnit == TemporalUnit::Nanosecond && roundingIncrement == 1)
        return this;
    }

    auto thisNs = m_exactTime.get();
    auto timeZone = m_timeZone;
    auto isoDateTime = getISODateTimeFor(timeZone, thisNs);
    
    Int128 epochNanoseconds;
    if (smallestUnit == TemporalUnit::Day) {
        auto dateStart = std::get<0>(isoDateTime);
        auto dateEnd = TemporalCalendar::balanceISODate(dateStart.year(), dateStart.month(), dateStart.day() + 1);
        auto startNs = getStartOfDay(globalObject, timeZone, dateStart);
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(thisNs >= startNs);
        auto endNs = getStartOfDay(globalObject, timeZone, dateEnd);
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(thisNs < endNs);
        auto dayLengthNs = endNs.epochNanoseconds() - startNs.epochNanoseconds();
        auto dayProgressNs = timeDurationFromEpochNanosecondsDifference(thisNs, startNs);
        auto roundedDayNsOptional = roundTimeDurationToIncrement(dayProgressNs, dayLengthNs, roundingMode);
        if (!roundedDayNsOptional) {
            throwRangeError(globalObject, scope, "rounded time duration is out of range in Temporal.ZonedDateTime.round()"_s);
            return { };
        }
        epochNanoseconds = startNs.epochNanoseconds() + roundedDayNsOptional.value();
    }
    else {
        auto roundResult = roundISODateTime(isoDateTime, roundingIncrement, smallestUnit, roundingMode);
        auto offsetNanoseconds = getOffsetNanosecondsFor(timeZone, thisNs.epochNanoseconds());
        epochNanoseconds = interpretISODateTimeOffset(
            std::get<0>(roundResult), std::get<1>(roundResult), offsetNanoseconds, timeZone);
        RETURN_IF_EXCEPTION(scope, { });
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
