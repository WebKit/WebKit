/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/JSExportMacros.h>
#include <cstdint>
#include <limits>
#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>

namespace JSC {

using TimeZoneID = unsigned;

extern JS_EXPORT_PRIVATE TimeZoneID utcTimeZoneIDStorage;
JS_EXPORT_PRIVATE TimeZoneID utcTimeZoneIDSlow();

inline TimeZoneID utcTimeZoneID()
{
    unsigned value = utcTimeZoneIDStorage;
    if (value == std::numeric_limits<TimeZoneID>::max())
        return utcTimeZoneIDSlow();
    return value;
}

// Look up the IANA primary identifier for a TimeZoneID. id must have been
// produced by intlResolveTimeZoneID() or utcTimeZoneID().
JS_EXPORT_PRIVATE const String& intlTimeZoneIDToString(TimeZoneID);

class TimeZone final {
public:
    TimeZone()
        : TimeZone(utcTimeZoneID(), 0)
    {
    }

    static TimeZone fromID(TimeZoneID id) { return TimeZone(id, 0); }
    static TimeZone fromUTCOffset(int64_t offsetNanoseconds) { return TimeZone(utcTimeZoneID(), offsetNanoseconds); }

    bool isUTCOffset() const { return m_id == utcTimeZoneID(); }
    bool isID() const { return !isUTCOffset(); }

    TimeZoneID id() const { ASSERT(isID()); return m_id; }
    int64_t utcOffsetNanoseconds() const { ASSERT(isUTCOffset()); return m_offset; }

    // IANA primary identifier for non-UTC named zones, "UTC" for offset 0, or formatted
    // "+HH:MM[:SS[.fff...]]" for non-zero UTC offsets.
    JS_EXPORT_PRIVATE String toString() const;

    // ICU accepts named identifiers as-is, and offsets only in "GMT+HHMM" form.
    JS_EXPORT_PRIVATE String toICUString() const;

    friend bool operator==(const TimeZone&, const TimeZone&) = default;

private:
    constexpr TimeZone(TimeZoneID id, int64_t offset) : m_id(id), m_offset(offset) { }

    TimeZoneID m_id { };
    int64_t m_offset { };
};

} // namespace JSC
