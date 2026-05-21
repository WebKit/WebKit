/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/CalendarICUBridge.h>
#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/JSCTimeZone.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/TemporalEnums.h>
#include <optional>
#include <wtf/Packed.h>

namespace JSC {

class TemporalZonedDateTime final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.temporalZonedDateTimeSpace<mode>();
    }

    static TemporalZonedDateTime* create(VM&, Structure*, ISO8601::ExactTime, TimeZone, String&&, String&&);
    static TemporalZonedDateTime* tryCreate(JSGlobalObject*, Structure*, ISO8601::ExactTime, TimeZone, String&&, String&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    // ToTemporalZonedDateTime: converts a ZDT object, ZDT string, or property bag to a ZDT.
    // The no-options overload uses all defaults (Compatible disambiguation, Reject offset option, Constrain overflow).
    static TemporalZonedDateTime* from(JSGlobalObject*, JSValue item);
    static TemporalZonedDateTime* from(JSGlobalObject*, JSValue item, JSValue options);

    DECLARE_INFO;

    ISO8601::ExactTime exactTime() const { return m_exactTime.get(); }
    const TimeZone& timeZone() const { return m_timeZone; }
    const String& timeZoneId() const { return m_timeZoneId; }
    String calendarId() const { return TemporalCore::calendarIDToString(m_calendarID).toString(); }
    CalendarID calendarID() const { return m_calendarID; }

    // Returns the UTC offset in nanoseconds at this ZDT's exact time.
    // Returns std::nullopt and throws on ICU failure.
    std::optional<int64_t> getOffsetNanoseconds(JSGlobalObject*) const;

    // Decomposes this ZDT into local (PlainDate, PlainTime).
    // Returns false and throws on ICU failure.
    bool getLocalDateAndTime(JSGlobalObject*, ISO8601::PlainDate&, ISO8601::PlainTime&) const;

    // Resolve a local (date, time) in a timezone to a unique ExactTime using disambiguation.
    // Returns std::nullopt and throws on ICU failure or Reject mode with ambiguous/nonexistent time.
    static std::optional<ISO8601::ExactTime> getEpochNanosecondsFor(JSGlobalObject*, const TimeZone&, const ISO8601::PlainDate&, const ISO8601::PlainTime&, TemporalDisambiguation);

private:
    TemporalZonedDateTime(VM&, Structure*, ISO8601::ExactTime, TimeZone, String&&, String&&);
    void finishCreation(VM&);

    Packed<ISO8601::ExactTime> m_exactTime;
    TimeZone m_timeZone;
    String m_timeZoneId;
    CalendarID m_calendarID { 0 };
};

} // namespace JSC
