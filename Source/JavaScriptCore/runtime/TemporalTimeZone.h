/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "IntlObject.h"
#include "JSObject.h"
#include <variant>

namespace JSC {

class TemporalTimeZone final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.temporalTimeZoneSpace<mode>();
    }

    static TemporalTimeZone* createFromID(VM&, Structure*, TimeZoneID);
    static TemporalTimeZone* createFromUTCOffset(VM&, Structure*, int64_t);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    using TimeZone = ISO8601::TimeZone;

    TimeZone timeZone() const { return m_timeZone; }

    static JSObject* from(JSGlobalObject*, JSValue);

    static Int128 getOffsetNanosecondsFor(ISO8601::TimeZone, Int128);
    static ISO8601::ExactTime getEpochNanosecondsFor(JSGlobalObject*,
        ISO8601::TimeZone, ISO8601::PlainDateTime, TemporalDisambiguation);
    static ISO8601::PlainDateTime getISODateTimeFor(ISO8601::TimeZone, ISO8601::ExactTime);
    static Vector<Int128> getPossibleEpochNanoseconds(JSGlobalObject*,
        ISO8601::TimeZone, ISO8601::PlainDateTime);
    static ISO8601::ExactTime disambiguatePossibleEpochNanoseconds(JSGlobalObject*,
        Vector<Int128>, ISO8601::TimeZone, ISO8601::PlainDateTime, TemporalDisambiguation);
    static ISO8601::ExactTime getStartOfDay(JSGlobalObject*, ISO8601::TimeZone, ISO8601::PlainDate);

    static std::optional<ISO8601::ExactTime> getNamedTimeZoneNextTransition(TimeZoneID, Int128);
    static std::optional<ISO8601::ExactTime> getNamedTimeZonePreviousTransition(TimeZoneID, Int128);

    static std::optional<ISO8601::TimeZone> getAvailableNamedTimeZoneIdentifier(JSGlobalObject*, TimeZoneID);
    static std::optional<ISO8601::TimeZone> getAvailableNamedTimeZoneIdentifier(JSGlobalObject*,
        const Vector<LChar>&);

    static std::optional<int64_t> parseDateTimeUTCOffset(StringView);
    static std::optional<TimeZone> parseTemporalTimeZoneString(StringView);
    static ISO8601::TimeZone toTemporalTimeZoneIdentifier(JSGlobalObject*, JSValue);
    static String formatDateTimeUTCOffsetRounded(Int128);
    static String formatOffsetTimeZoneIdentifier(int64_t, std::optional<bool>);

private:
    TemporalTimeZone(VM&, Structure*, TimeZone);

    // TimeZoneID or UTC offset.
    TimeZone m_timeZone;
};

constexpr bool isUTCTimeZoneString(StringView str)
{
    auto lowered = str.convertToASCIILowercase();
    return (lowered.length() == 3 && lowered[0] == 'u' && lowered[1] == 't' && lowered[2] == 'c');
}

} // namespace JSC
