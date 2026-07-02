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

#include "config.h"
#include "InstantCore.h"

#include "ISO8601.h"
#include "TemporalObject.h"
#include <wtf/DateMath.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {
namespace TemporalCore {

// TemporalInstantToString — temporal_rs: Instant::to_ixdtf_string_with_provider (src/builtins/core/instant.rs)
// https://tc39.es/proposal-temporal/#sec-temporal-temporalinstanttostring
WTF::String instantToString(ISO8601::ExactTime exactTime, std::optional<int64_t> offsetNs, PrecisionData precision)
{
    // 1. Let outputTimeZone be timeZone.
    // 2. If outputTimeZone is undefined, set outputTimeZone to "UTC".
    // NOTE: steps 1-2 and step 7.a (GetOffsetNanosecondsFor) are resolved by the caller;
    // offsetNs is undefined when timeZone is undefined, otherwise the pre-resolved offset.
    // 3. Let epochNs be instant.[[EpochNanoseconds]].  (exactTime carries epochNs)
    // 4. Let isoDateTime be GetISODateTimeFor(outputTimeZone, epochNs).
    // Add the UTC offset to get local epoch-milliseconds, then decompose into date/time fields.
    int64_t epochMs;
    if (offsetNs)
        epochMs = exactTime.floorEpochMilliseconds() + *offsetNs / static_cast<int64_t>(ISO8601::ExactTime::nsPerMillisecond);
    else
        epochMs = exactTime.floorEpochMilliseconds();

    GregorianDateTime gregorianDateTime { static_cast<double>(epochMs), LocalTimeOffset { } };

    // 5. Let dateTimeString be ISODateTimeToString(isoDateTime, "iso8601", precision, ~never~).
    // NOTE: Inlined here — instantToString receives the offset-adjusted GregorianDateTime
    // rather than PlainDate/PlainTime, so temporalDateTimeToString() cannot be called directly.
    // 5.1. Let yearString be PadISOYear(year).
    StringBuilder builder;
    unsigned yearLength = 4;
    if (gregorianDateTime.year() > 9999 || gregorianDateTime.year() < 0) {
        builder.append(gregorianDateTime.year() < 0 ? '-' : '+');
        yearLength = 6;
    }
    // 5.2-5.3. month and day strings.
    // 5.5. Let timeString be FormatTimeString(...).
    builder.append(makeString(pad('0', yearLength, std::abs(gregorianDateTime.year())),
        '-', pad('0', 2, gregorianDateTime.month() + 1),
        '-', pad('0', 2, gregorianDateTime.monthDay()),
        'T', pad('0', 2, gregorianDateTime.hour()),
        ':', pad('0', 2, gregorianDateTime.minute())));

    // 5.4. subSecondNanoseconds = nanosecondsFraction() (sub-second ns of the instant).
    // NOTE: negative for pre-epoch instants; correction maps to the [0, nsPerSecond) range.
    int fraction { exactTime.nanosecondsFraction() };
    if (fraction < 0)
        fraction += static_cast<int>(ISO8601::ExactTime::nsPerSecond);

    formatSecondsStringPart(builder, gregorianDateTime.second(), fraction, precision);

    // 6. If timeZone is undefined, let timeZoneString be "Z".
    // 7. Else, let timeZoneString be FormatDateTimeUTCOffsetRounded(offsetNanoseconds).
    if (offsetNs) {
        static constexpr int64_t nsPerMinute = 60'000'000'000LL;
        int64_t rawOffset = *offsetNs;
        int64_t sign = rawOffset < 0 ? -1 : 1;
        int64_t absNs = std::abs(rawOffset);
        int64_t minutes = (absNs + nsPerMinute / 2) / nsPerMinute;
        int64_t roundedNs = sign * minutes * nsPerMinute;
        builder.append(ISO8601::formatTimeZoneOffsetString(roundedNs));
    } else
        builder.append('Z');

    // 8. Return the string-concatenation of dateTimeString and timeZoneString.
    return builder.toString();
}

} // namespace TemporalCore
} // namespace JSC
