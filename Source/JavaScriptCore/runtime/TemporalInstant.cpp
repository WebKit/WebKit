/*
 * Copyright (C) 2021 Igalia S.L.
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
#include "TemporalInstant.h"

#include "AuxiliaryBarrierInlines.h"
#include "Error.h"
#include "ISO8601.h"
#include "IntlObjectInlines.h"
#include "JSBigInt.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "MathCommon.h"
#include "StructureInlines.h"
#include "TemporalDuration.h"
#include "TemporalObject.h"
#include "TemporalTimeZone.h"

namespace JSC {

const ClassInfo TemporalInstant::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TemporalInstant) };

Structure* TemporalInstant::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

TemporalInstant::TemporalInstant(VM& vm, Structure* structure, ISO8601::ExactTime exactTime)
    : Base(vm, structure)
    , m_exactTime(exactTime)
{
}

TemporalInstant* TemporalInstant::create(VM& vm, Structure* structure, ISO8601::ExactTime exactTime)
{
    ASSERT(exactTime.isValid());
    auto* object = new (NotNull, allocateCell<TemporalInstant>(vm)) TemporalInstant(vm, structure, exactTime);
    object->finishCreation(vm);
    return object;
}

// CreateTemporalInstant ( epochNanoseconds [ , newTarget ] )
// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalinstant
TemporalInstant* TemporalInstant::tryCreateIfValid(JSGlobalObject* globalObject, ISO8601::ExactTime exactTime, Structure* structure)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!exactTime.isValid()) {
        throwRangeError(globalObject, scope, makeString(exactTime.asString(), " epoch nanoseconds is outside of supported range for Temporal.Instant"_s));
        return nullptr;
    }

    return create(vm, structure ? structure : globalObject->instantStructure(), exactTime);
}

TemporalInstant* TemporalInstant::tryCreateIfValid(JSGlobalObject* globalObject, JSValue value, Structure* structure)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue epochNanoseconds = value.toBigInt(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

#if USE(BIGINT32)
    if (epochNanoseconds.isBigInt32()) {
        int32_t total = epochNanoseconds.bigInt32AsInt32();
        ISO8601::ExactTime exactTime { total };
        ASSERT(exactTime.isValid());
        return create(vm, structure ? structure : globalObject->instantStructure(), exactTime);
    }
#endif

    JSBigInt* bigint = asHeapBigInt(epochNanoseconds);
    bool bigIntTooLong = false;
    Int128 total;
    if constexpr (sizeof(JSBigInt::Digit) == 4) {
        Int128 d0 { bigint->length() > 0 ? bigint->digit(0) : 0 };
        Int128 d1 { bigint->length() > 1 ? bigint->digit(1) : 0 };
        Int128 d2 { bigint->length() > 2 ? bigint->digit(2) : 0 };
        total = d2 << 64 | d1 << 32 | d0;
        bigIntTooLong = bigint->length() > 3;
    } else {
        ASSERT(sizeof(JSBigInt::Digit) == 8);
        // Handle maxint128 < abs(bigint) <= maxuint128 explicitly, otherwise
        // the int128 arithmetic below is undefined
        if (bigint->length() > 1 && (bigint->digit(1) & 0x8000'0000'0000'0000)) {
            total = 0;
            bigIntTooLong = true;
        } else {
            Int128 low { bigint->length() > 0 ? bigint->digit(0) : 0 };
            Int128 high { bigint->length() > 1 ? bigint->digit(1) : 0 };
            total = high << 64 | low;
            bigIntTooLong = bigint->length() > 2;
        }
    }
    ISO8601::ExactTime exactTime { total * (bigint->sign() ? -1 : 1) };

    if (bigIntTooLong || !exactTime.isValid()) {
        String argAsString = bigint->toString(globalObject, 10);
        if (scope.exception()) {
            scope.clearException();
            argAsString = "The given number of"_s;
        }

        throwRangeError(globalObject, scope, makeString(ellipsizeAt(100, argAsString), " epoch nanoseconds is outside of the supported range for Temporal.Instant"_s));
        return nullptr;
    }

    return create(vm, structure ? structure : globalObject->instantStructure(), exactTime);
}

// ToTemporalInstant ( item )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalinstant
TemporalInstant* TemporalInstant::toInstant(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!itemValue.isObject() && !itemValue.isString()) {
        throwTypeError(globalObject, scope, "can only convert to Instant from object or string values"_s);
        return nullptr;
    }

    if (itemValue.inherits<TemporalInstant>())
        return jsCast<TemporalInstant*>(itemValue);

    // FIXME: when Temporal.ZonedDateTime lands
    // if (itemValue.inherits<TemporalZonedDateTime>())
    //    return TemporalInstant::create(vm, globalObject->instantStructure(), jsCast<TemporalZonedDateTime*>(itemValue)->epochTime());

    String string = itemValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto parsedExactTime = ISO8601::parseInstant(string);
    if (!parsedExactTime) {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, string), "' is not a valid Temporal.Instant string"_s));
        return nullptr;
    }

    RELEASE_AND_RETURN(scope, tryCreateIfValid(globalObject, parsedExactTime.value()));
}

// Temporal.Instant.from ( item )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.from
TemporalInstant* TemporalInstant::from(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();

    if (itemValue.inherits<TemporalInstant>()) {
        ISO8601::ExactTime exactTime = jsCast<TemporalInstant*>(itemValue)->exactTime();
        return TemporalInstant::create(vm, globalObject->instantStructure(), exactTime);
    }

    return toInstant(globalObject, itemValue);
}

// Temporal.Instant.fromEpochSeconds ( epochSeconds )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.fromepochseconds
TemporalInstant* TemporalInstant::fromEpochSeconds(JSGlobalObject* globalObject, JSValue epochSecondsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double epochSeconds = epochSecondsValue.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // NumberToBigInt step 1
    if (!isInteger(epochSeconds)) {
        throwRangeError(globalObject, scope, makeString(epochSeconds, " is not a valid integer number of epoch seconds"_s));
        return nullptr;
    }

    ISO8601::ExactTime exactTime = ISO8601::ExactTime::fromEpochSeconds(epochSeconds);
    RELEASE_AND_RETURN(scope, tryCreateIfValid(globalObject, exactTime));
}

// Temporal.Instant.fromEpochMilliseconds ( epochMilliseconds )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.fromepochmilliseconds
TemporalInstant* TemporalInstant::fromEpochMilliseconds(JSGlobalObject* globalObject, JSValue epochMillisecondsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double epochMilliseconds = epochMillisecondsValue.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // NumberToBigInt step 1
    if (!isInteger(epochMilliseconds)) {
        throwRangeError(globalObject, scope, makeString(epochMilliseconds, " is not a valid integer number of epoch milliseconds"_s));
        return nullptr;
    }

    ISO8601::ExactTime exactTime = ISO8601::ExactTime::fromEpochMilliseconds(epochMilliseconds);
    RELEASE_AND_RETURN(scope, tryCreateIfValid(globalObject, exactTime));
}

// Temporal.Instant.fromEpochMicroseconds ( epochMicroseconds )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.fromepochmicroseconds
TemporalInstant* TemporalInstant::fromEpochMicroseconds(JSGlobalObject* globalObject, JSValue epochMicrosecondsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue epochMicroseconds = epochMicrosecondsValue.toBigInt(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

#if USE(BIGINT32)
    if (epochMicroseconds.isBigInt32()) {
        int32_t microseconds = epochMicroseconds.bigInt32AsInt32();
        auto exactTime = ISO8601::ExactTime::fromEpochMicroseconds(microseconds);
        ASSERT(exactTime.isValid());
        return create(vm, globalObject->instantStructure(), exactTime);
    }
#endif

    JSBigInt* bigint = asHeapBigInt(epochMicroseconds);
    bool bigIntTooLong;
    if constexpr (sizeof(JSBigInt::Digit) == 4) {
        bigIntTooLong = bigint->length() > 2;
        // Handle maxint64 < abs(bigint) <= maxuint64 explicitly, otherwise the
        // cast to int64 below is undefined
        if (bigint->length() == 2 && (bigint->digit(1) & 0x8000'0000))
            bigIntTooLong = true;
    } else {
        ASSERT(sizeof(JSBigInt::Digit) == 8);
        bigIntTooLong = bigint->length() > 1;
        // Handle maxint64 < abs(bigint) <= maxuint64 explicitly, otherwise the
        // cast to int64 below is undefined
        if (bigint->length() == 1 && (bigint->digit(0) & 0x8000'0000'0000'0000))
            bigIntTooLong = true;
    }
    int64_t microseconds = bigIntTooLong ? 0 : JSBigInt::toBigInt64(bigint);
    auto exactTime = ISO8601::ExactTime::fromEpochMicroseconds(microseconds);

    if (bigIntTooLong || !exactTime.isValid()) {
        String argAsString = bigint->toString(globalObject, 10);
        if (scope.exception()) {
            scope.clearException();
            argAsString = "The given number of"_s;
        }

        throwRangeError(globalObject, scope, makeString(ellipsizeAt(100, argAsString), " epoch microseconds is outside of supported range for Temporal.Instant"_s));
        return nullptr;
    }

    return create(vm, globalObject->instantStructure(), exactTime);
}

// Temporal.Instant.fromEpochNanoseconds ( epochNanoseconds )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.fromepochnanoseconds
TemporalInstant* TemporalInstant::fromEpochNanoseconds(JSGlobalObject* globalObject, JSValue epochNanosecondsValue)
{
    return tryCreateIfValid(globalObject, epochNanosecondsValue);
}

// Temporal.Instant.compare ( one, two )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.compare
JSValue TemporalInstant::compare(JSGlobalObject* globalObject, JSValue oneValue, JSValue twoValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    TemporalInstant* one = toInstant(globalObject, oneValue);
    RETURN_IF_EXCEPTION(scope, { });

    TemporalInstant* two = toInstant(globalObject, twoValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (one->exactTime() > two->exactTime())
        return jsNumber(1);
    if (one->exactTime() < two->exactTime())
        return jsNumber(-1);
    return jsNumber(0);
}

ISO8601::Duration TemporalInstant::difference(JSGlobalObject* globalObject, TemporalInstant* other, JSValue optionsValue) const
{
    // https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.since
    // https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.until
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto smallest = temporalSmallestUnit(globalObject, options, { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day });
    RETURN_IF_EXCEPTION(scope, { });
    TemporalUnit smallestUnit = smallest.value_or(TemporalUnit::Nanosecond);

    TemporalUnit defaultLargestUnit = std::min(smallestUnit, TemporalUnit::Second);
    auto largest = temporalLargestUnit(globalObject, options, { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day }, defaultLargestUnit);
    RETURN_IF_EXCEPTION(scope, { });
    TemporalUnit largestUnit = largest.value_or(defaultLargestUnit);

    if (smallest && largest && smallest < largest) {
        throwRangeError(globalObject, scope, "smallestUnit must be smaller than largestUnit"_s);
        return { };
    }

    RoundingMode roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<double> maxIncrement = maximumRoundingIncrement(smallestUnit);
    ASSERT(maxIncrement && *maxIncrement <= 1000); // unbounded increments are impossible with Temporal.Instant
    unsigned increment = temporalRoundingIncrement(globalObject, options, maxIncrement, false);
    RETURN_IF_EXCEPTION(scope, { });

    Int128 roundedDiff = exactTime().difference(other->exactTime(), increment, smallestUnit, roundingMode);
    // NOTE: Duration fields are currently doubles, and the total number of
    // nanoseconds may not fit in a double. This may need to change if the
    // internal representation of Duration changes.
    ASSERT(roundedDiff / 1'000'000'000 < INT64_MAX);
    double seconds { static_cast<double>(static_cast<int64_t>(roundedDiff / 1'000'000'000)) };
    double nanosecondsRemainder { static_cast<double>(static_cast<int64_t>(roundedDiff % 1'000'000'000)) };
    ISO8601::Duration result { 0, 0, 0, 0, 0, 0, seconds, 0, 0, nanosecondsRemainder };

    TemporalDuration::balance(result, largestUnit);
    return result;
}

static double maximumIncrement(TemporalUnit smallestUnit)
{
    switch (smallestUnit) {
    case TemporalUnit::Hour: return 24;
    case TemporalUnit::Minute: return 1440;
    case TemporalUnit::Second: return 86400;
    case TemporalUnit::Millisecond: return 8.64e7;
    case TemporalUnit::Microsecond: return 8.64e10;
    case TemporalUnit::Nanosecond: return 8.64e13;
    case TemporalUnit::Year:
    case TemporalUnit::Month:
    case TemporalUnit::Week:
    case TemporalUnit::Day:
    default:
        { }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

ISO8601::ExactTime TemporalInstant::round(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = nullptr;
    std::optional<TemporalUnit> smallest;
    if (optionsValue.isString()) {
        auto string = optionsValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        smallest = temporalUnitType(string);
        if (!smallest) {
            throwRangeError(globalObject, scope, "smallestUnit is an invalid Temporal unit"_s);
            return { };
        }

        if (smallest.value() <= TemporalUnit::Day) {
            throwRangeError(globalObject, scope, "smallestUnit is a disallowed unit"_s);
            return { };
        }
    } else {
        options = intlGetOptionsObject(globalObject, optionsValue);
        RETURN_IF_EXCEPTION(scope, { });

        smallest = temporalSmallestUnit(globalObject, options, { TemporalUnit::Year, TemporalUnit::Month, TemporalUnit::Week, TemporalUnit::Day });
        RETURN_IF_EXCEPTION(scope, { });

        if (!smallest) {
            throwRangeError(globalObject, scope, "Cannot round without a smallestUnit option"_s);
            return { };
        }
    }
    TemporalUnit smallestUnit = smallest.value();

    RoundingMode roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::HalfExpand);
    RETURN_IF_EXCEPTION(scope, { });

    double increment = temporalRoundingIncrement(globalObject, options, maximumIncrement(smallestUnit), true);
    RETURN_IF_EXCEPTION(scope, { });

    return exactTime().round(increment, smallestUnit, roundingMode);
}

// Temporal.Instant.prototype.toString( [ options ] )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tostring
String TemporalInstant::toString(JSGlobalObject* globalObject, JSValue optionsValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, { });

    if (!options)
        return toString();

    JSObject* timeZone = nullptr;
    JSValue timeZoneValue = options->get(globalObject, vm.propertyNames->timeZone);
    RETURN_IF_EXCEPTION(scope, { });
    if (!timeZoneValue.isUndefined()) {
        timeZone = TemporalTimeZone::from(globalObject, timeZoneValue);
        RETURN_IF_EXCEPTION(scope, { });
    }

    PrecisionData data = secondsStringPrecision(globalObject, options);
    RETURN_IF_EXCEPTION(scope, { });

    RoundingMode roundingMode = temporalRoundingMode(globalObject, options, RoundingMode::Trunc);
    RETURN_IF_EXCEPTION(scope, { });

    // No need to make a new object if we were given explicit defaults.
    if (std::get<0>(data.precision) == Precision::Auto && roundingMode == RoundingMode::Trunc)
        return toString(timeZone);

    ISO8601::ExactTime newExactTime { exactTime().round(data.increment, data.unit, roundingMode) };

    // FIXME: Missing, relies on TimeZone:
    // 1. Let roundedInstant be ! CreateTemporalInstant(roundedNs).
    // ...
    // 1. If _outputTimeZone_ is *undefined*, then
    //   1. Set _outputTimeZone_ to ! CreateTemporalTimeZone(*"UTC"*).
    // 1. Let _isoCalendar_ be ! GetISO8601Calendar().
    // 1. Let _dateTime_ be ? BuiltinTimeZoneGetPlainDateTimeFor(_outputTimeZone_, _instant_, _isoCalendar_).
    JSObject* outputTimeZone = timeZone;
    if (outputTimeZone) {
        throwVMError(globalObject, scope, "FIXME: Temporal.Instant.toString({timeZone}) not implemented yet"_s);
        return { };
    }

    return toString(newExactTime, timeZone, data);
}

// TemporalInstantToString ( instant, timeZone, precision )
// https://tc39.es/proposal-temporal/#sec-temporal-temporalinstanttostring
String TemporalInstant::toString(ISO8601::ExactTime exactTime, JSObject* timeZone, PrecisionData precision)
{
    GregorianDateTime gregorianDateTime { static_cast<double>(exactTime.epochMilliseconds()), LocalTimeOffset { } };
    StringBuilder builder;

    // If the year is outside the bounds of 0 and 9999 inclusive we want to
    // use the extended year format (PadISOYear).
    unsigned yearLength = 4;
    if (gregorianDateTime.year() > 9999 || gregorianDateTime.year() < 0) {
        builder.append(gregorianDateTime.year() < 0 ? '-' : '+');
        yearLength = 6;
    }

    builder.append(makeString(pad('0', yearLength, std::abs(gregorianDateTime.year())),
        '-', pad('0', 2, gregorianDateTime.month() + 1),
        '-', pad('0', 2, gregorianDateTime.monthDay()),
        'T', pad('0', 2, gregorianDateTime.hour()),
        ':', pad('0', 2, gregorianDateTime.minute())));

    static constexpr int nsPerSecond { 1'000'000'000 };
    int fraction { exactTime.nanosecondsFraction() };
    if (fraction < 0)
        fraction += nsPerSecond;

    formatSecondsStringPart(builder, gregorianDateTime.second(), fraction, precision);

    if (timeZone) {
        // FIXME: Missing, relies on TimeZone:
        //   1. Let _timeZoneString_ be ? BuiltinTimeZoneGetOffsetStringFor(_timeZone_, _instant_).
        builder.append('Z');
    } else
        builder.append('Z');

    return builder.toString();
}

} // namespace JSC
