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

#include "Error.h"
#include "ISO8601.h"
#include "JSBigInt.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "MathCommon.h"
#include "TemporalObject.h"
#include "TemporalZonedDateTime.h"

#include <wtf/text/MakeString.h>
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

std::optional<ISO8601::ExactTime> bigIntValueToExactTime(JSGlobalObject* globalObject, JSValue bigIntValue, ASCIILiteral typeName)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

#if USE(BIGINT32)
    if (bigIntValue.isBigInt32()) {
        // BigInt32 values (≤ 2^30) are always within the valid epoch-ns range.
        ISO8601::ExactTime exactTime { Int128 { bigIntValue.bigInt32AsInt32() } };
        ASSERT(exactTime.isValid());
        return exactTime;
    }
#endif

    JSBigInt* bigint = asHeapBigInt(bigIntValue);
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
        // Guard: abs(bigint) in (2^127, 2^128] would overflow Int128 arithmetic.
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
            TRY_CLEAR_EXCEPTION(scope, std::nullopt);
            argAsString = "The given number of"_s;
        }
        throwRangeError(globalObject, scope, makeString(ellipsizeAt(100, argAsString), " epoch nanoseconds is outside of the supported range for "_s, typeName));
        return std::nullopt;
    }
    return exactTime;
}

// ToTemporalInstant ( item )
// https://tc39.es/proposal-temporal/#sec-temporal-totemporalinstant
TemporalInstant* TemporalInstant::toInstant(JSGlobalObject* globalObject, JSValue itemValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: If item is an Object, then
    if (itemValue.isObject()) {
        // Step 1.a: If item has [[InitializedTemporalInstant]] or [[InitializedTemporalZonedDateTime]],
        //   return ! CreateTemporalInstant(item.[[EpochNanoseconds]]). Spec returns a NEW instance.
        if (itemValue.inherits<TemporalInstant>())
            return create(vm, globalObject->instantStructure(), uncheckedDowncast<TemporalInstant>(itemValue)->exactTime());
        if (itemValue.inherits<TemporalZonedDateTime>())
            return create(vm, globalObject->instantStructure(), uncheckedDowncast<TemporalZonedDateTime>(itemValue)->exactTime());

        // Step 1.c: Set item to ? ToPrimitive(item, STRING).
        itemValue = itemValue.toPrimitive(globalObject, PreferString);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    // Step 2: If item is not a String, throw a TypeError exception.
    if (!itemValue.isString()) [[unlikely]] {
        throwTypeError(globalObject, scope, "can only convert to Instant from object or string values"_s);
        return nullptr;
    }
    String string = asString(itemValue)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // Step 3: Let parsed be ? ParseISODateTime(item, « TemporalInstantString »).
    auto parsedOpt = ISO8601::parseISODateTime(string, ISO8601::TemporalProduction::Instant);
    if (!parsedOpt) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, string), "' is not a valid Temporal.Instant string"_s));
        return nullptr;
    }
    auto [plainDateOpt, plainTimeOpt, timeZoneOpt, calendarOpt, matched, isShortForm] = WTF::move(*parsedOpt);
    ASSERT(plainDateOpt && plainTimeOpt && timeZoneOpt);

    // Step 4: Assert: parsed.[[TimeZone]].[[OffsetString]] is not empty XOR parsed.[[TimeZone]].[[Z]] is true.
    //   No-op: parseISODateTime guarantees this for the Instant production.

    // Step 5: offsetNanoseconds = Z ? 0 : ParseDateTimeUTCOffset(OffsetString).
    int64_t offsetNanoseconds = timeZoneOpt->m_z ? 0 : *timeZoneOpt->m_offset;

    // Step 6: Let time be parsed.[[Time]].
    const ISO8601::PlainTime& plainTime = *plainTimeOpt;
    const ISO8601::PlainDate& plainDate = *plainDateOpt;

    // Step 7: Assert: time is not start-of-day. (No-op: parser guarantees this.)

    // Steps 8 + 10: BalanceISODateTime + GetUTCEpochNanoseconds collapsed.
    //   BalanceISODateTime preserves the sum days×nsPerDay + Σ(timeFields). We only need the
    //   sum (epochNs); intermediate balanced fields are unread, so the rebalance is unobservable.
    auto exactTime = ISO8601::ExactTime::fromISOPartsAndOffset(
        plainDate.year(), plainDate.month(), plainDate.day(),
        plainTime.hour(), plainTime.minute(), plainTime.second(),
        plainTime.millisecond(), plainTime.microsecond(), plainTime.nanosecond(),
        offsetNanoseconds);

    // Steps 9 + 11: CheckISODaysRange + IsValidEpochNanoseconds collapsed.
    //   |dateDays| ≤ 10⁸ ↔ |epochNs| ≤ 8.64×10²¹ at the boundary; isValid() covers both.
    if (!exactTime.isValid()) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString("'"_s, ellipsizeAt(100, string), "' is not a valid Temporal.Instant string"_s));
        return nullptr;
    }

    // Step 12: Return ! CreateTemporalInstant(epochNanoseconds).
    return create(vm, globalObject->instantStructure(), exactTime);
}

// Temporal.Instant.fromEpochMilliseconds ( epochMilliseconds )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.fromepochmilliseconds
TemporalInstant* TemporalInstant::fromEpochMilliseconds(JSGlobalObject* globalObject, JSValue epochMillisecondsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Set epochMilliseconds to ? ToNumber(epochMilliseconds).
    double epochMilliseconds = epochMillisecondsValue.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // Step 2: Set epochMilliseconds to ? NumberToBigInt(epochMilliseconds).
    //   NumberToBigInt's two effects (RangeError if !IsIntegralNumber, then ℤ-conversion) are
    //   inlined as isInteger() + the int64_t cast in fromEpochMilliseconds — no JSBigInt needed.
    if (!isInteger(epochMilliseconds)) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString(epochMilliseconds, " is not a valid integer number of epoch milliseconds"_s));
        return nullptr;
    }

    // Step 3: Let epochNanoseconds be epochMilliseconds × 10^6ℤ.
    ISO8601::ExactTime exactTime = ISO8601::ExactTime::fromEpochMilliseconds(epochMilliseconds);

    // Step 4: If IsValidEpochNanoseconds(epochNanoseconds) is false, throw a RangeError exception.
    if (!exactTime.isValid()) [[unlikely]] {
        throwRangeError(globalObject, scope, makeString(exactTime.asString(), " epoch nanoseconds is outside of supported range for Temporal.Instant"_s));
        return nullptr;
    }

    // Step 5: Return ! CreateTemporalInstant(epochNanoseconds).
    return create(vm, globalObject->instantStructure(), exactTime);
}

// Temporal.Instant.fromEpochNanoseconds ( epochNanoseconds )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.fromepochnanoseconds
TemporalInstant* TemporalInstant::fromEpochNanoseconds(JSGlobalObject* globalObject, JSValue epochNanosecondsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Set epochNanoseconds to ? ToBigInt(epochNanoseconds).
    JSValue bigIntValue = epochNanosecondsValue.toBigInt(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    // Step 2: If IsValidEpochNanoseconds(epochNanoseconds) is false, throw a RangeError exception.
    //   bigIntValueToExactTime does both the BigInt → Int128 narrowing and the validity check;
    //   nullopt only after throwing.
    auto exactTimeOpt = bigIntValueToExactTime(globalObject, bigIntValue, "Temporal.Instant"_s);
    RETURN_IF_EXCEPTION(scope, nullptr);
    ASSERT(exactTimeOpt);

    // Step 3: Return ! CreateTemporalInstant(epochNanoseconds).
    return create(vm, globalObject->instantStructure(), *exactTimeOpt);
}

// Temporal.Instant.compare ( one, two )
// https://tc39.es/proposal-temporal/#sec-temporal.instant.compare
JSValue TemporalInstant::compare(JSGlobalObject* globalObject, JSValue oneValue, JSValue twoValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Step 1: Set one = ? ToTemporalInstant(one).
    TemporalInstant* one = toInstant(globalObject, oneValue);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 2: Set two = ? ToTemporalInstant(two).
    TemporalInstant* two = toInstant(globalObject, twoValue);
    RETURN_IF_EXCEPTION(scope, { });

    // Step 3: Return CompareEpochNanoseconds(one.[[EpochNanoseconds]], two.[[EpochNanoseconds]]).
    if (one->exactTime() > two->exactTime())
        return jsNumber(1);
    if (one->exactTime() < two->exactTime())
        return jsNumber(-1);
    return jsNumber(0);
}

} // namespace JSC
