/*
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

#pragma once

#include "ISO8601.h"
#include "LazyProperty.h"
#include "TemporalCalendar.h"

namespace JSC {

class TemporalZonedDateTime final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    using ExactTime = ISO8601::ExactTime;
    using TimeZone = ISO8601::TimeZone;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.temporalZonedDateTimeSpace<mode>();
    }

    static TemporalZonedDateTime* create(VM&, Structure*, ExactTime&&, TimeZone&&);
    static TemporalZonedDateTime* tryCreateIfValid(JSGlobalObject*, Structure*, ExactTime&&, TimeZone&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    static TemporalZonedDateTime* from(JSGlobalObject*, JSValue, std::optional<JSValue>);
    static int32_t compare(const TemporalZonedDateTime*, const TemporalZonedDateTime*);

    TemporalCalendar* calendar() { return m_calendar.get(this); }

    TemporalZonedDateTime* addDurationToZonedDateTime(JSGlobalObject*, bool, ISO8601::Duration, JSObject*);
    TemporalZonedDateTime* with(JSGlobalObject*, JSObject* temporalDateLike, JSValue options);
    TemporalZonedDateTime* round(JSGlobalObject*, JSValue options);
    ISO8601::Duration since(JSGlobalObject*, JSValue, TemporalZonedDateTime*);
    ISO8601::Duration until(JSGlobalObject*, JSValue, TemporalZonedDateTime*);
    JSValue getTimeZoneTransition(JSGlobalObject*, JSValue);

    String monthCode() const;
    uint8_t dayOfWeek() const;
    uint16_t dayOfYear() const;
    uint8_t weekOfYear() const;

    ExactTime exactTime() const { return m_exactTime.get(); }
    TimeZone timeZone() const { return m_timeZone; }

    String toString(JSGlobalObject*, JSValue options) const;
    String toString() const
    {
        return ISO8601::temporalZonedDateTimeToString(m_exactTime.get(), m_timeZone,
            PrecisionData { { Precision::Auto, 0 }, TemporalUnit::Nanosecond, 1 },
            TemporalShowCalendar::Auto,
            TemporalShowTimeZone::Auto, TemporalShowOffset::Auto, 1, TemporalUnit::Nanosecond, RoundingMode::Trunc);
    }


    DECLARE_VISIT_CHILDREN;

private:
    TemporalZonedDateTime(VM&, Structure*, ExactTime&&, TimeZone&&);
    void finishCreation(VM&);

    ISO8601::Duration differenceTemporalZonedDateTime(bool, JSGlobalObject*, JSValue,
        TemporalZonedDateTime*);

    Packed<ExactTime> m_exactTime;
    TimeZone m_timeZone;
    LazyProperty<TemporalZonedDateTime, TemporalCalendar> m_calendar;
};

} // namespace JSC
