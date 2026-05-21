/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/DurationArithmetic.h>
#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/JSCTimeZone.h>

namespace JSC {

class TemporalDuration final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr uint8_t numberOfLowerTierPreciseCells = 0;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.temporalDurationSpace<mode>();
    }

    static TemporalDuration* create(VM&, Structure*, ISO8601::Duration&&);
    static TemporalDuration* tryCreateIfValid(JSGlobalObject*, ISO8601::Duration&&, Structure* = nullptr);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    static TemporalDuration* toTemporalDuration(JSGlobalObject*, JSValue);
    static ISO8601::Duration toLimitedDuration(JSGlobalObject*, JSValue, std::initializer_list<TemporalUnit> disallowedUnits);
    static TemporalDuration* from(JSGlobalObject*, JSValue);
    static JSValue compare(JSGlobalObject*, JSValue, JSValue, JSValue options);

#define JSC_DEFINE_TEMPORAL_DURATION_FIELD(name, capitalizedName) \
    double name##s() const { return static_cast<double>(m_duration.name##s()); } \
    void set##capitalizedName##s(double value) { m_duration.setField(TemporalUnit::capitalizedName, value); }
    JSC_TEMPORAL_UNITS(JSC_DEFINE_TEMPORAL_DURATION_FIELD);
#undef JSC_DEFINE_TEMPORAL_DURATION_FIELD

    int sign() const { return TemporalCore::durationSign(m_duration); }
    const ISO8601::Duration& duration() const { return m_duration; }

    ISO8601::Duration with(JSGlobalObject*, JSObject* durationLike) const;
    ISO8601::Duration add(JSGlobalObject*, JSValue other) const;
    ISO8601::Duration subtract(JSGlobalObject*, JSValue other) const;
    ISO8601::Duration round(JSGlobalObject*, JSValue options) const;
    double total(JSGlobalObject*, JSValue options) const;
    String toString(JSGlobalObject*, JSValue options) const;
    String toString(JSGlobalObject* globalObject, std::tuple<Precision, unsigned> precision = { Precision::Auto, 0 }) const { return toString(globalObject, m_duration, precision); }

    static ISO8601::InternalDuration toInternalDuration(ISO8601::Duration);
    static ISO8601::InternalDuration toInternalDurationRecordWith24HourDays(JSGlobalObject*, ISO8601::Duration);
    ISO8601::Duration addDurations(JSGlobalObject*, AddOrSubtract, ISO8601::Duration, TemporalUnit) const;
    static ISO8601::Duration temporalDurationFromInternal(ISO8601::InternalDuration, TemporalUnit);

    static ISO8601::Duration fromDurationLike(JSGlobalObject*, JSObject*);
    static ISO8601::Duration toISO8601Duration(JSGlobalObject*, JSValue);

    static ISO8601::InternalDuration round(JSGlobalObject*, ISO8601::InternalDuration, double increment, TemporalUnit, RoundingMode);
    static std::tuple<ISO8601::PlainDate, ISO8601::PlainTime> combineISODateAndTimeRecord(ISO8601::PlainDate, ISO8601::PlainTime);
    static std::optional<ISO8601::PlainDate> regulateISODate(int32_t year, int32_t month, int64_t day, TemporalOverflow);
    static ISO8601::Duration toDateDurationRecordWithoutTime(JSGlobalObject*, const ISO8601::Duration&);
private:
    TemporalDuration(VM&, Structure*, ISO8601::Duration&&);
    DECLARE_DEFAULT_FINISH_CREATION;

    template<typename CharacterType>
    static std::optional<ISO8601::Duration> parse(StringParsingBuffer<CharacterType>&);

    static String toString(JSGlobalObject*, const ISO8601::Duration&, std::tuple<Precision, unsigned> precision);

    ISO8601::Duration m_duration;
};

Int128 getUTCEpochNanoseconds(std::tuple<ISO8601::PlainDate, ISO8601::PlainTime>);

} // namespace JSC
