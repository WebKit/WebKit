/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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

// Stub implementations for ICU calendar/formatter APIs not exported by
// the PlayStation SDK's libSceIcu_stub_weak.a.
//
// PlayStation's ICU exports only *read/epoch-set* APIs:
//   ucal_open, ucal_close, ucal_get, ucal_setMillis, ucal_setGregorianChange
//
// Missing (stubbed here):
//   Write/mutation: ucal_set, ucal_setDateTime, ucal_add, ucal_clear, ucal_getLimit,
//                   ucal_setTimeZone, ucal_getTimeZoneTransitionDate
//   Formatter: udat_clone, udat_applyPattern
//
// Degradation contract:
//   - Stubs that have UErrorCode* set U_UNSUPPORTED_ERROR so our U_FAILURE(status)
//     guards return TemporalResult errors, which surface as RangeError in JavaScript.
//   - ucal_set has no UErrorCode* (ICU API design), so it cannot signal failure.
//     It is safe as a no-op because every caller either:
//     (a) follows it with ucal_getMillis (stubbed → error caught), or
//     (b) uses setCalendarToISODate (which calls ucal_setMillis, not ucal_set) first.
//
// Temporal operations on PlayStation:
//   WORK: PlainDate/Time/DateTime, Instant, Duration (pure ISO arithmetic)
//          ZonedDateTime with UTC-offset timezones (isUTCOffset() fast path)
//          getOffsetNanosecondsFor on named TZs (uses ucal_setMillis + ucal_get)
//          Non-ISO calendar property getters for non-lunisolar calendars
//            (isoToCalendarFields via ucal_setMillis + ucal_get; computeOrdinalMonth = UCAL_MONTH+1)
//   WRONG RESULTS (silent): lunisolar calendars (Chinese, Dangi, Hebrew) property getters
//            (computeOrdinalMonth saves/restores state via ucal_getMillis which returns 0)
//   THROW RangeError:
//          ZonedDateTime with named timezones (getPossibleEpochNanosecondsFor needs ucal_setDateTime)
//          Calendar arithmetic: add/diff for non-ISO calendars (needs ucal_add/getMillis)
//          getTimeZoneTransition (needs ucal_getTimeZoneTransitionDate)

#include "config.h"

#if PLATFORM(PLAYSTATION)

#include <unicode/ucal.h>
#include <unicode/udat.h>

// These extern "C" stubs use ICU's C naming convention (underscores) intentionally.
// NOLINT(readability/naming/underscores)
extern "C" {

// ucal_set has no UErrorCode* — cannot propagate failure. Safe as no-op because
// callers always follow it with ucal_getMillis (stubbed) or ucal_add (stubbed).
void ucal_set(UCalendar* /*cal*/, UCalendarDateFields /*field*/, int32_t /*value*/) // NOLINT
{
}

void ucal_setDateTime(UCalendar* /*cal*/, int32_t /*year*/, int32_t /*month*/, int32_t /*date*/, // NOLINT
    int32_t /*hour*/, int32_t /*minute*/, int32_t /*second*/, UErrorCode* status)
{
    if (status && U_SUCCESS(*status))
        *status = U_UNSUPPORTED_ERROR;
}

double ucal_getMillis(const UCalendar* /*cal*/, UErrorCode* status) // NOLINT
{
    if (status && U_SUCCESS(*status))
        *status = U_UNSUPPORTED_ERROR;
    return 0;
}

void ucal_add(UCalendar* /*cal*/, UCalendarDateFields /*field*/, int32_t /*amount*/, UErrorCode* status) // NOLINT
{
    if (status && U_SUCCESS(*status))
        *status = U_UNSUPPORTED_ERROR;
}

void ucal_clear(UCalendar* /*cal*/) // NOLINT
{
}

int32_t ucal_getLimit(const UCalendar* /*cal*/, UCalendarDateFields /*field*/, // NOLINT
    UCalendarLimitType /*type*/, UErrorCode* status)
{
    if (status && U_SUCCESS(*status))
        *status = U_UNSUPPORTED_ERROR;
    return -1;
}

void ucal_setTimeZone(UCalendar* /*cal*/, const UChar* /*zoneID*/, int32_t /*len*/, UErrorCode* status) // NOLINT
{
    if (status && U_SUCCESS(*status))
        *status = U_UNSUPPORTED_ERROR;
}

UBool ucal_getTimeZoneTransitionDate(const UCalendar* /*cal*/, UTimeZoneTransitionType /*type*/, // NOLINT
    UDate* /*transition*/, UErrorCode* status)
{
    if (status && U_SUCCESS(*status))
        *status = U_UNSUPPORTED_ERROR;
    return false;
}

UDateFormat* udat_clone(const UDateFormat* /*fmt*/, UErrorCode* status) // NOLINT
{
    if (status && U_SUCCESS(*status))
        *status = U_UNSUPPORTED_ERROR;
    return nullptr;
}

void udat_applyPattern(UDateFormat* /*format*/, UBool /*localized*/, // NOLINT
    const UChar* /*pattern*/, int32_t /*patternLength*/)
{
}

} // extern "C"

#endif // PLATFORM(PLAYSTATION)
