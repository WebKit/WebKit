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
#include "TemporalPlainDateTime.h"
#include "TemporalPlainTime.h"
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

static Int128 getNamedTimeZoneOffsetNanosecondsImpl(TimeZoneID timeZoneIdentifier, Int128)
{

    RELEASE_ASSERT(timeZoneIdentifier == utcTimeZoneID());
    return 0;
}

// https://tc39.es/proposal-temporal/#sec-getnamedtimezoneepochnanoseconds
static Vector<ISO8601::ExactTime> getNamedTimeZoneEpochNanoseconds(TimeZoneID timeZoneIdentifier, std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> isoDateTime)
{
    // FIXME: handle other named time zones
    RELEASE_ASSERT(timeZoneIdentifier == utcTimeZoneID());
    auto epochNanoseconds = TemporalDuration::getUTCEpochNanoseconds(isoDateTime);
    return Vector<ISO8601::ExactTime>(epochNanoseconds);
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
Vector<ISO8601::ExactTime> TemporalZonedDateTime::getPossibleEpochNanoseconds(JSGlobalObject* globalObject, ISO8601::TimeZone timeZone, std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> isoDateTime)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto isoDate = std::get<0>(isoDateTime);
    auto isoTime = std::get<1>(isoDateTime);

    Vector<ISO8601::ExactTime> possibleEpochNanoseconds;
    if (std::holds_alternative<int64_t>(timeZone)) {
        auto balanced = ISO8601::balanceISODateTime(isoDate.year(), isoDate.month(), isoDate.day(),
            isoTime.hour(), isoTime.minute(), isoTime.second(),
            isoTime.millisecond(), isoTime.microsecond(), isoTime.nanosecond() - std::get<int64_t>(timeZone));
        checkISODaysRange(globalObject, std::get<0>(balanced));
        RETURN_IF_EXCEPTION(scope, { });
        ISO8601::ExactTime epochNanoseconds = ISO8601::ExactTime(TemporalDuration::getUTCEpochNanoseconds(balanced));
        possibleEpochNanoseconds = Vector<ISO8601::ExactTime>({ epochNanoseconds });
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
    auto possibleEpochNs = TemporalZonedDateTime::getPossibleEpochNanoseconds(globalObject, timeZone, isoDateTime);
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
ISO8601::ExactTime TemporalZonedDateTime::disambiguatePossibleEpochNanoseconds(JSGlobalObject* globalObject,
    Vector<ISO8601::ExactTime> possibleEpochNs,
    ISO8601::TimeZone timeZone, std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> isoDateTime,
    TemporalDisambiguation disambiguation)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto n = possibleEpochNs.size();
    if (n == 1)
        return possibleEpochNs[0];
    if (n != 0) {
        if (disambiguation == TemporalDisambiguation::Earlier
            || disambiguation == TemporalDisambiguation::Compatible)
            return possibleEpochNs[0];
        if (disambiguation == TemporalDisambiguation::Later)
            return possibleEpochNs[n - 1];
        throwRangeError(globalObject, scope, "disambiguation is Reject and multiple instants found in disambiguatePossibleEpochNanoseconds()"_s);
        return { };
    }
    // n == 0
    if (disambiguation == TemporalDisambiguation::Reject) {
        throwRangeError(globalObject, scope, "disambiguation is Reject in disambiguatePossibleEpochNanoseconds() and no possible instants"_s);
        return { };
    }
    auto utcNs = TemporalDuration::getUTCEpochNanoseconds(isoDateTime);
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
    
//    auto beforePossible = offsetBefore;
//    auto afterPossible = offsetAfter;

    auto isoDate = std::get<0>(isoDateTime);

    if (disambiguation == TemporalDisambiguation::Earlier) {
        auto timeDuration = TemporalDuration::timeDurationFromComponents(0, 0, 0, 0, 0, -nanoseconds);
        auto earlierTime = TemporalPlainTime::addTime(std::get<1>(isoDateTime), timeDuration);
        auto earlierDate = TemporalCalendar::balanceISODate(
            isoDate.year(), isoDate.month(), isoDate.day() + earlierTime.days());
        auto earlierDateTime = TemporalDuration::combineISODateAndTimeRecord(earlierDate,
            ISO8601::PlainTime(earlierTime.hours(), earlierTime.minutes(), earlierTime.seconds(),
                earlierTime.milliseconds(), earlierTime.microseconds(), earlierTime.nanoseconds()));
        possibleEpochNs = TemporalZonedDateTime::getPossibleEpochNanoseconds(globalObject, timeZone, earlierDateTime);
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(possibleEpochNs.size() > 0);
        return possibleEpochNs[0];
    }
    auto timeDuration = TemporalDuration::timeDurationFromComponents(0, 0, 0, 0, 0, nanoseconds);
    auto laterTime = TemporalPlainTime::addTime(std::get<1>(isoDateTime), timeDuration);
    auto laterDate = TemporalCalendar::balanceISODate(isoDate.year(), isoDate.month(), isoDate.day() - laterTime.days());
    auto laterDateTime = TemporalDuration::combineISODateAndTimeRecord(laterDate, 
        ISO8601::PlainTime(laterTime.hours(), laterTime.minutes(), laterTime.seconds(),
            laterTime.milliseconds(), laterTime.microseconds(), laterTime.nanoseconds()));
    possibleEpochNs = TemporalZonedDateTime::getPossibleEpochNanoseconds(globalObject, timeZone, laterDateTime);
    RETURN_IF_EXCEPTION(scope, { });
    n = possibleEpochNs.size();
    ASSERT(n != 0);
    return possibleEpochNs[n - 1];
}

// https://tc39.es/proposal-temporal/#sec-temporal-interpretisodatetimeoffset
static ISO8601::ExactTime interpretISODateTimeOffset(JSGlobalObject* globalObject,
    ISO8601::PlainDate isoDate, ISO8601::PlainTime time,
    TemporalOffsetBehavior offsetBehavior, unsigned offsetNanoseconds, ISO8601::TimeZone timeZone,
    TemporalDisambiguation disambiguation, TemporalOffset offsetOption,
    TemporalMatchBehavior matchBehavior)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    auto isoDateTime = TemporalDuration::combineISODateAndTimeRecord(isoDate, time);

    if (offsetBehavior == TemporalOffsetBehavior::Wall
        || (offsetBehavior == TemporalOffsetBehavior::Option && offsetOption == TemporalOffset::Ignore))
        return ISO8601::ExactTime(TemporalDuration::getEpochNanosecondsFor(
            globalObject, timeZone, isoDateTime, disambiguation));

    if (offsetBehavior == TemporalOffsetBehavior::Exact
        || (offsetBehavior == TemporalOffsetBehavior::Option && offsetOption == TemporalOffset::Use)) {
        auto balanced = ISO8601::balanceISODateTime(isoDate.year(), isoDate.month(), isoDate.day(),
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
    ASSERT(offsetOption == TemporalOffset::Prefer
           || offsetOption == TemporalOffset::Reject);

    checkISODaysRange(globalObject, isoDate);
    RETURN_IF_EXCEPTION(scope, { });
    auto utcEpochNanoseconds = TemporalDuration::getUTCEpochNanoseconds(isoDateTime);
    auto possibleEpochNs = TemporalZonedDateTime::getPossibleEpochNanoseconds(globalObject, timeZone, isoDateTime);
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

    if (offsetOption == TemporalOffset::Reject) {
        throwRangeError(globalObject, scope, "no matching offset and offsetOption is reject in interpretISODateTimeOffset"_s);
        return { };
    }
    
    return TemporalZonedDateTime::disambiguatePossibleEpochNanoseconds(globalObject,
        possibleEpochNs, timeZone,
        std::tuple<ISO8601::PlainDate, ISO8601::PlainTime>(isoDate, time), disambiguation);
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
    
    ISO8601::ExactTime epochNanoseconds;
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
        epochNanoseconds = ISO8601::ExactTime(startNs.epochNanoseconds() + roundedDayNsOptional.value());
    }
    else {
        auto roundResult = roundISODateTime(isoDateTime, roundingIncrement, smallestUnit, roundingMode);
        auto offsetNanoseconds = ISO8601::getOffsetNanosecondsFor(timeZone, thisNs.epochNanoseconds());
        epochNanoseconds = interpretISODateTimeOffset(globalObject,
            std::get<0>(roundResult), std::get<1>(roundResult),
            TemporalOffsetBehavior::Option, offsetNanoseconds, timeZone,
            TemporalDisambiguation::Compatible, TemporalOffset::Prefer, TemporalMatchBehavior::Exactly);
        RETURN_IF_EXCEPTION(scope, { });
    }
    return TemporalZonedDateTime::tryCreateIfValid(globalObject, globalObject->zonedDateTimeStructure(),
        WTFMove(epochNanoseconds), WTFMove(timeZone));
}

TemporalZonedDateTime* TemporalZonedDateTime::with(JSGlobalObject* globalObject, JSObject*, JSValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    throwRangeError(globalObject, scope, "Temporal.ZonedDateTime.with not yet implemented"_s);
    return nullptr;
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
//    auto digits = temporalFractionalSecondDigits(globalObject, options);
//    RETURN_IF_EXCEPTION(scope, { });
    auto showOffset = getTemporalShowOffsetOption(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    auto roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });
    auto smallestUnit = temporalSmallestUnit(globalObject, options,
        { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day });
    RETURN_IF_EXCEPTION(scope, { });
    if (smallestUnit == TemporalUnit::Hour) {
        throwRangeError(globalObject, scope, "smallestUnit cannot be hour"_s);
        return { };
    }
    auto showTimeZone = getTemporalShowTimeZoneNameOption(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });
    PrecisionData precision = secondsStringPrecision(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    return ISO8601::temporalZonedDateTimeToString(m_exactTime.get(), m_timeZone, precision, showCalendar, showTimeZone, showOffset, precision.increment, precision.unit, roundingMode);
}

enum class FieldName : uint8_t {
    Year,
    Month,
    MonthCode,
    Day,
    Hour,
    Minute,
    Second,
    Millisecond,
    Microsecond,
    Nanosecond,
    Offset,
    TimeZone,
};

static PropertyName propertyName(VM& vm, FieldName property)
{
    switch (property) {
    case FieldName:: Year:
        return vm.propertyNames->year;
    case FieldName:: Month:
        return vm.propertyNames->month;
    case FieldName:: MonthCode:
        return vm.propertyNames->monthCode;
    case FieldName:: Day:
        return vm.propertyNames->day;
    case FieldName:: Hour:
        return vm.propertyNames->hour;
    case FieldName:: Minute:
        return vm.propertyNames->minute;
    case FieldName:: Second:
        return vm.propertyNames->second;
    case FieldName:: Millisecond:
        return vm.propertyNames->millisecond;
    case FieldName:: Microsecond:
        return vm.propertyNames->microsecond;
    case FieldName:: Nanosecond:
        return vm.propertyNames->nanosecond;
    case FieldName:: Offset:
        return vm.propertyNames->offset;
    case FieldName:: TimeZone:
        return vm.propertyNames->timeZone;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

unsigned toIntegerWithTruncation(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto number = argument.toIntegerOrInfinity(globalObject);
    if (!std::isfinite(number)) {
        throwRangeError(globalObject, scope, "Temporal properties must be finite"_s);
        return { };
    }
    return number;
}

unsigned toPositiveIntegerWithTruncation(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto integer = toIntegerWithTruncation(globalObject, argument);
    RETURN_IF_EXCEPTION(scope, { });
    if (integer <= 0) {
        throwRangeError(globalObject, scope, "Temporal property must be a positive integer"_s);
        return { };
    }
    return integer;
}

// https://tc39.es/proposal-temporal/#sec-temporal-tooffsetstring
static String toOffsetString(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!argument.isString()) {
        throwTypeError(globalObject, scope, "offset must be a string"_s);
        return { };
    }
    String offset = argument.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (!TemporalTimeZone::parseDateTimeUTCOffset(offset)) {
        throwRangeError(globalObject, scope, makeString("error parsing offset string "_s, offset));
        return { };
    }
    return offset;
}

// https://tc39.es/proposal-temporal/#sec-temporal-tomonthcode
static String toMonthCode(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!argument.isString()) {
        throwTypeError(globalObject, scope, "month code must be a string"_s);
        return { };
    }
    String monthCode = argument.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (monthCode.length() < 3 || monthCode.length() > 4) {
        throwRangeError(globalObject, scope, "length of month code must be 3 or 4"_s);
        return { };
    }
    if (monthCode[0] != 'M') {
        throwRangeError(globalObject, scope, "month code must begin with 'M'"_s);
        return { };
    }
    if (monthCode[1] < '0' || monthCode[1] > '9' || monthCode[2] < '0' || monthCode[2] > '9') {
        throwRangeError(globalObject, scope, "digits expected in month code"_s);
        return { };
    }
    if (monthCode.length() == 4 && monthCode[3] != 'L') {
        throwRangeError(globalObject, scope, "monthCode must end in 'L' if length is 4"_s);
        return { };
    }
    auto monthCodeInteger = parseInt(monthCode.substring(1, 3).span8(), 10);
    if (monthCodeInteger == 0 && monthCode.length() != 4) {
        throwRangeError(globalObject, scope, "monthCode cannot be 0 if last character is not 'L'"_s);
        return { };
    }
    return monthCode;
}

// TODO
// https://tc39.es/proposal-temporal/#sec-getavailablenamedtimezoneidentifier
std::optional<ISO8601::TimeZone> TemporalZonedDateTime::getAvailableNamedTimeZoneIdentifier(JSGlobalObject* globalObject,
    TimeZoneID timeZoneIdentifier)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (timeZoneIdentifier == utcTimeZoneID())
        return 0;
    throwRangeError(globalObject, scope, "getAvailableNamedTimeZoneIdentifier() not yet implemented"_s);
    return { };
}

// TODO
// https://tc39.es/proposal-temporal/#sec-getavailablenamedtimezoneidentifier
std::optional<ISO8601::TimeZone> TemporalZonedDateTime::getAvailableNamedTimeZoneIdentifier(JSGlobalObject* globalObject, Vector<LChar> chars)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (chars.size() == 3 && chars[0] == 'U' && chars[1] == 'T' && chars[2] == 'C')
        return 0;
    throwRangeError(globalObject, scope, "getAvailableNamedTimeZoneIdentifier() not yet implemented"_s);
    return { };
}

ISO8601::TimeZone TemporalZonedDateTime::toTemporalTimeZoneIdentifier(JSGlobalObject* globalObject,
    JSValue temporalTimeZoneLike)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (temporalTimeZoneLike.isObject()) {
        if (temporalTimeZoneLike.inherits<TemporalZonedDateTime>()) {
            return jsCast<TemporalZonedDateTime*>(temporalTimeZoneLike)->m_timeZone;
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
    if (std::holds_alternative<int64_t>(parseResult)) {
        int64_t offsetMinutes = std::get<int64_t>(parseResult) % 60;
        offsetMinutes = offsetMinutes % 1000000000;
        if (offsetMinutes) {
            throwRangeError(globalObject, scope, "Sub-minute precision not allowed in offset time zone"_s);
            return { };
        }
        return parseResult;
    }
    auto name = std::get<TimeZoneID>(parseResult);
    auto timeZoneIdentifierRecord = getAvailableNamedTimeZoneIdentifier(globalObject, name);
    RETURN_IF_EXCEPTION(scope, { });
    if (!timeZoneIdentifierRecord) {
        throwRangeError(globalObject, scope, "time zone is invalid"_s);
        return { };
    }
    return timeZoneIdentifierRecord.value();
}

static
std::tuple<std::optional<double>, std::optional<double>, std::optional<String>, std::optional<double>,
double, double, double, double, double, double,
std::optional<String>, std::optional<ISO8601::TimeZone>>
prepareCalendarFields(JSGlobalObject* globalObject, TemporalCalendar* calendar, JSObject* fields, Vector<FieldName> calendarFieldNames, Vector<FieldName> nonCalendarFieldNames, std::optional<Vector<FieldName>> requiredFieldNames)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    (void) calendar; // TODO

    auto fieldNames = calendarFieldNames;
    fieldNames.appendVector(nonCalendarFieldNames);
// TODO: non-iso8601 calendars
//    auto extraFieldNames = calendarExtraFields(calendar, calendarFieldNames);
//    fieldNames.append(extraFieldNames);
    std::optional<unsigned> yearOptional;
    std::optional<unsigned> monthOptional;
    std::optional<String> monthCodeOptional;
    std::optional<unsigned> dayOptional;
    unsigned hour = 0;
    unsigned minute = 0;
    unsigned second = 0;
    unsigned millisecond = 0;
    unsigned microsecond = 0;
    unsigned nanosecond = 0;
    std::optional<String> offsetOptional;
    std::optional<ISO8601::TimeZone> timeZoneOptional;
    auto any = false;
    for (auto property : fieldNames) {
        auto value = fields->get(globalObject, propertyName(vm, property));
        RETURN_IF_EXCEPTION(scope, { });
        if (!value.isUndefined()) {
            any = true;
            switch (property) {
            case FieldName::Year: {
                unsigned val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                yearOptional = val;
                break;
            }
            case FieldName::Month: {
                unsigned val = toPositiveIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                monthOptional = val;
                break;
            }
            case FieldName::MonthCode: {
                String val = toMonthCode(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                monthCodeOptional = val;
                break;
            }
            case FieldName::Day: {
                unsigned val = toPositiveIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                dayOptional = val;
                break;
            }
            case FieldName::Hour: {
                unsigned val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                hour = val;
                break;
            }
            case FieldName::Minute: {
                unsigned val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                minute = val;
                break;
            }
            case FieldName::Second: {
                unsigned val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                second = val;
                break;
            }
            case FieldName::Millisecond: {
                unsigned val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                millisecond = val;
                break;
            }
            case FieldName::Microsecond: {
                unsigned val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                microsecond = val;
                break;
            }
            case FieldName::Nanosecond: {
                unsigned val = toIntegerWithTruncation(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                nanosecond = val;
                break;
            }
            case FieldName::Offset: {
                String val = toOffsetString(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                offsetOptional = val;
                break;
            }
            case FieldName::TimeZone: {
                ISO8601::TimeZone val = TemporalZonedDateTime::toTemporalTimeZoneIdentifier(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                timeZoneOptional = val;
                break;
            }
            }
        } else {
            if (requiredFieldNames && requiredFieldNames->contains(property)) {
                throwTypeError(globalObject, scope, "prepareCalendarFields: missing required property name"_s);
                return { };
            }
        }
    }
    if (!requiredFieldNames && !any) {
        throwTypeError(globalObject, scope, "prepareCalendarNames: at least one Temporal property must be given"_s);
        return { };
    }
    return { yearOptional, monthOptional, monthCodeOptional, dayOptional,
        hour, minute, second, millisecond, microsecond, nanosecond, offsetOptional, timeZoneOptional };
}

static void calendarResolveFields(JSGlobalObject* globalObject, TemporalCalendar* calendar, std::optional<double> optionalYear,
    std::optional<double> optionalMonth, std::optional<String> optionalMonthCode, std::optional<double> optionalDay,
    double& month, TemporalDateFormat format)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (calendar->isISO8601())
    {
        if ((format == TemporalDateFormat::Date || format == TemporalDateFormat::YearMonth) && !optionalYear) {
            throwTypeError(globalObject, scope, "year property missing in Temporal.ZonedDateTime.from"_s);
            return;
        }
        if ((format == TemporalDateFormat::Date || format == TemporalDateFormat::MonthDay) && !optionalDay) {
            throwTypeError(globalObject, scope, "day property missing in Temporal.ZonedDateTime.from"_s);
            return;
        }
        if (!optionalMonthCode && !optionalMonth) {
            throwTypeError(globalObject, scope, "month and monthCode properties missing in Temporal.ZonedDateTime.from"_s);
            return;
        }
        if (!optionalMonthCode) {
            month = optionalMonth.value();
            return;
        }
        auto monthCode = optionalMonthCode.value();
        if (monthCode.length() != 3) {
            throwRangeError(globalObject, scope, "invalid month code in Temporal.ZonedDateTime.from"_s);
            return;
        }
        if (monthCode[0] != 'M') {
            throwRangeError(globalObject, scope, "invalid month code in Temporal.ZonedDateTime.from (must begin with 'M')"_s);
            return;
        }
        auto monthCodeInteger = parseInt(monthCode.substring(1, 3).span8(), 10);
        if (optionalMonth && optionalMonth.value() != monthCodeInteger) {
            throwRangeError(globalObject, scope, "month and monthCode properties disagree in Temporal.ZonedDateTime.from"_s);
            return;
        }
        month = monthCodeInteger;
        return;
    }
    throwRangeError(globalObject, scope, "non-ISO8601 calendars not supported yet"_s);
    return;
}

static std::tuple<ISO8601::PlainDate, ISO8601::PlainTime>
interpretTemporalDateTimeFields(JSGlobalObject* globalObject, TemporalCalendar* calendar, std::optional<double> optionalYear,
    std::optional<double> optionalMonth, std::optional<String> optionalMonthCode, std::optional<double> optionalDay,
    double hour, double minute, double second, double millisecond, double microsecond, double nanosecond,
    TemporalOverflow overflow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double month = 0;
    calendarResolveFields(globalObject, calendar, optionalYear, optionalMonth, optionalMonthCode, optionalDay, month, TemporalDateFormat::Date);
    RETURN_IF_EXCEPTION(scope, { });
    auto isoDate = ISO8601::PlainDate(optionalYear.value(), month, optionalDay.value());
    auto time = TemporalPlainTime::regulateTime(globalObject, ISO8601::Duration { 0, 0, 0, 0, hour, minute, second, millisecond, microsecond, nanosecond }, overflow);
    RETURN_IF_EXCEPTION(scope, { });
    return TemporalDuration::combineISODateAndTimeRecord(isoDate, time);
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalzoneddatetime
TemporalZonedDateTime* TemporalZonedDateTime::from(JSGlobalObject* globalObject, JSValue itemValue, std::optional<JSObject*> options)
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
        if (itemValue.inherits<TemporalZonedDateTime>()) {
            if (options) {
                getTemporalDisambiguationOption(globalObject, options.value());
                RETURN_IF_EXCEPTION(scope, { });
                getTemporalOffsetOption(globalObject, options.value(), TemporalOffset::Reject);
                RETURN_IF_EXCEPTION(scope, { });
                toTemporalOverflow(globalObject, options.value());
                RETURN_IF_EXCEPTION(scope, { });
            }
            return jsCast<TemporalZonedDateTime*>(itemValue);
        }

        // TODO
        TemporalCalendar* calendar = TemporalCalendar::create(vm, globalObject->calendarStructure(), iso8601CalendarID());
        auto [optionalYear, optionalMonth, optionalMonthCode, optionalDay, hour, minute,
            second, millisecond, microsecond, nanosecond, optionalOffset,
            timeZoneOptional] = prepareCalendarFields(globalObject, calendar, jsCast<JSObject*>(itemValue),
            Vector { FieldName::Day, FieldName::Month, FieldName::MonthCode, FieldName::Year },
            Vector { FieldName::Hour, FieldName::Microsecond, FieldName::Millisecond,
              FieldName::Minute, FieldName::Month, FieldName::MonthCode, FieldName::Nanosecond,
              FieldName::Offset, FieldName::TimeZone }, Vector { FieldName::TimeZone });
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
        auto result = interpretTemporalDateTimeFields(globalObject, calendar, optionalYear,
            optionalMonth, optionalMonthCode, optionalDay, hour, minute,
            second, millisecond, microsecond, nanosecond, overflow);
        RETURN_IF_EXCEPTION(scope, { });
        isoDate = std::get<0>(result);
        time = std::get<1>(result);
    } else {
        if (!itemValue.isString()) {
            throwTypeError(globalObject, scope, "can only convert to ZonedDateTime from object or string values"_s);
            return { };
        }

        auto string = itemValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        // Validate overflow -- see step 3(g) of ToTemporalTime
        if (options)
            overflow = toTemporalOverflow(globalObject, options.value());
        
        auto dateTime = ISO8601::parseDateTime(string, TemporalDateFormat::Date);
        if (!dateTime) {
            throwRangeError(globalObject, scope, makeString("in Temporal.ZonedDateTime.from, error parsing "_s, string));
            return { };
        }

        auto [plainDate, plainTimeOptional, timeZoneOptional] = WTFMove(dateTime.value());
        if (!timeZoneOptional) {
            throwRangeError(globalObject, scope, "string must have a time zone annotation to convert to ZonedDateTime"_s);
            return { };
    }
        if (!timeZoneOptional->m_offset_string) {
            throwRangeError(globalObject, scope, "in Temporal.ZonedDateTime, parsing strings with named time zones not implemented yet"_s);
            return { };
        }

        auto annotation = timeZoneOptional->m_annotation;
        ASSERT(annotation);
        timeZone = toTemporalTimeZoneIdentifier(globalObject, jsString(vm, WTF::String(annotation.value())));
        RETURN_IF_EXCEPTION(scope, { });
        if (timeZoneOptional->m_offset_string)
            offsetString = WTF::String(std::get<0>(timeZoneOptional->m_offset_string.value()));
        if (timeZoneOptional->m_z)
            offsetBehavior = TemporalOffsetBehavior::Exact;
        else if (offsetString)
            offsetBehavior = TemporalOffsetBehavior::Wall;
        matchBehavior = TemporalMatchBehavior::Minutes;
        if (options) {
            disambiguation = getTemporalDisambiguationOption(globalObject, options.value());
            RETURN_IF_EXCEPTION(scope, { });
            offsetOption = getTemporalOffsetOption(globalObject, options.value(), TemporalOffset::Reject);
            RETURN_IF_EXCEPTION(scope, { });
            toTemporalOverflow(globalObject, options.value());
            RETURN_IF_EXCEPTION(scope, { });
        }
        isoDate = plainDate;
        if (!plainTimeOptional) {
            throwRangeError(globalObject, scope, "string must include time in ZonedDateTime.from"_s);
            return { };
        }
        time = plainTimeOptional.value();
    }
    auto offsetNanoseconds = 0;
    if (offsetBehavior == TemporalOffsetBehavior::Option) {
        if (!offsetString) {
            throwRangeError(globalObject, scope, "missing offset in ZonedDateTime.from"_s);
            return { };
        }
        auto offsetNanosecondsOptional = TemporalTimeZone::parseDateTimeUTCOffset(offsetString.value());
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
