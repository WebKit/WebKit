/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2010 &yet, LLC. (nate@andyet.net)
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.

 * Copyright 2006-2008 the V8 project authors. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Google Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSDateMath.h"

#include "ExceptionHelpers.h"
#include "VM.h"
#include <limits>
#include <wtf/unicode/icu/ICUHelpers.h>

#if U_ICU_VERSION_MAJOR_NUM >= 69 || (U_ICU_VERSION_MAJOR_NUM == 68 && USE(APPLE_INTERNAL_SDK))
#define HAVE_ICU_C_TIMEZONE_API 1
#ifdef U_HIDE_DRAFT_API
#undef U_HIDE_DRAFT_API
#endif
#include <unicode/ucal.h>
#else
// icu::TimeZone and icu::BasicTimeZone features are only available in ICU C++ APIs.
// We use these C++ APIs as an exception.
#undef U_SHOW_CPLUSPLUS_API
#define U_SHOW_CPLUSPLUS_API 1
#include <unicode/basictz.h>
#include <unicode/timezone.h>
#include <unicode/unistr.h>
#undef U_SHOW_CPLUSPLUS_API
#define U_SHOW_CPLUSPLUS_API 0
#endif

namespace JSC {


#if HAVE(ICU_C_TIMEZONE_API)
class OpaqueICUTimeZone {
    WTF_MAKE_FAST_ALLOCATED(OpaqueICUTimeZone);
public:
    std::unique_ptr<UCalendar, ICUDeleter<ucal_close>> m_calendar;
    String m_canonicalTimeZoneID;
};
#endif

void OpaqueICUTimeZoneDeleter::operator()(OpaqueICUTimeZone* timeZone)
{
    if (timeZone) {
#if HAVE(ICU_C_TIMEZONE_API)
        delete timeZone;
#else
        delete bitwise_cast<icu::TimeZone*>(timeZone);
#endif
    }
}

// Get the combined UTC + DST offset for the time passed in.
//
// NOTE: The implementation relies on the fact that no time zones have
// more than one daylight savings offset change per month.
// If this function is called with NaN it returns random value.
LocalTimeOffset DateCache::calculateLocalTimeOffset(double millisecondsFromEpoch, WTF::TimeType inputTimeType)
{
    int32_t rawOffset = 0;
    int32_t dstOffset = 0;
    UErrorCode status = U_ZERO_ERROR;

    // This function can fail input date is invalid: NaN etc.
    // We can return any values in this case since later we fail when computing non timezone offset part anyway.
    constexpr LocalTimeOffset failed { false, 0 };

#if HAVE(ICU_C_TIMEZONE_API)
    auto& timeZoneCache = *this->timeZoneCache();
    ucal_setMillis(timeZoneCache.m_calendar.get(), millisecondsFromEpoch, &status);
    if (U_FAILURE(status))
        return failed;

    if (inputTimeType != WTF::LocalTime) {
        rawOffset = ucal_get(timeZoneCache.m_calendar.get(), UCAL_ZONE_OFFSET, &status);
        if (U_FAILURE(status))
            return failed;
        dstOffset = ucal_get(timeZoneCache.m_calendar.get(), UCAL_DST_OFFSET, &status);
        if (U_FAILURE(status))
            return failed;
    } else {
        ucal_getTimeZoneOffsetFromLocal(timeZoneCache.m_calendar.get(), UCAL_TZ_LOCAL_FORMER, UCAL_TZ_LOCAL_FORMER, &rawOffset, &dstOffset, &status);
        if (U_FAILURE(status))
            return failed;
    }
#else
    auto& timeZoneCache = *bitwise_cast<icu::TimeZone*>(this->timeZoneCache());
    if (inputTimeType != WTF::LocalTime) {
        constexpr bool isLocalTime = false;
        timeZoneCache.getOffset(millisecondsFromEpoch, isLocalTime, rawOffset, dstOffset, status);
        if (U_FAILURE(status))
            return failed;
    } else {
        // icu::TimeZone is a timezone instance which inherits icu::BasicTimeZone.
        // https://unicode-org.atlassian.net/browse/ICU-13705 will move getOffsetFromLocal to icu::TimeZone.
        static_cast<const icu::BasicTimeZone&>(timeZoneCache).getOffsetFromLocal(millisecondsFromEpoch, icu::BasicTimeZone::kFormer, icu::BasicTimeZone::kFormer, rawOffset, dstOffset, status);
        if (U_FAILURE(status))
            return failed;
    }
#endif

    return { !!dstOffset, rawOffset + dstOffset };
}

LocalTimeOffset DateCache::localTimeOffset(double millisecondsFromEpoch, WTF::TimeType inputTimeType)
{
    LocalTimeOffsetCache& cache = inputTimeType == WTF::LocalTime ? m_localTimeOffsetCache : m_utcTimeOffsetCache;

    double start = cache.start;
    double end = cache.end;

    auto resetCache = [&]() {
        // Compute the DST offset for the time and shrink the cache interval
        // to only contain the time. This allows fast repeated DST offset
        // computations for the same time.
        LocalTimeOffset offset = calculateLocalTimeOffset(millisecondsFromEpoch, inputTimeType);
        cache.offset = offset;
        cache.start = millisecondsFromEpoch;
        cache.end = millisecondsFromEpoch;
        cache.incrementStart = WTF::msPerMonth;
        cache.incrementEnd = WTF::msPerMonth;
        return offset;
    };

    // If the time fits in the cached interval, return the cached offset.
    if (start <= millisecondsFromEpoch && millisecondsFromEpoch <= end)
        return cache.offset;

    if (start <= millisecondsFromEpoch) {
        // Compute a possible new interval end.
        double newEnd = end + cache.incrementEnd;
        if (!(millisecondsFromEpoch <= newEnd))
            return resetCache();

        LocalTimeOffset endOffset = calculateLocalTimeOffset(newEnd, inputTimeType);
        if (cache.offset == endOffset) {
            // If the offset at the end of the new interval still matches
            // the offset in the cache, we grow the cached time interval
            // and return the offset.
            cache.end = newEnd;
            cache.incrementStart = WTF::msPerMonth;
            cache.incrementEnd = WTF::msPerMonth;
            return endOffset;
        }
        LocalTimeOffset offset = calculateLocalTimeOffset(millisecondsFromEpoch, inputTimeType);
        if (offset == endOffset) {
            // The offset at the given time is equal to the offset at the
            // new end of the interval, so that means that we've just skipped
            // the point in time where the DST offset change occurred. Update
            // the interval to reflect this and reset the increment.
            cache.start = millisecondsFromEpoch;
            cache.end = newEnd;
            cache.incrementStart = WTF::msPerMonth;
            cache.incrementEnd = WTF::msPerMonth;
        } else {
            // The interval contains a DST offset change and the given time is
            // before it. Adjust the increment to avoid a linear search for
            // the offset change point and change the end of the interval.
            cache.incrementEnd /= 3;
            cache.end = millisecondsFromEpoch;
        }
        // Update the offset in the cache and return it.
        cache.offset = offset;
        return offset;
    }

    // Compute a possible new interval start.
    double newStart = start - cache.incrementStart;
    if (!(newStart <= millisecondsFromEpoch))
        return resetCache();

    LocalTimeOffset startOffset = calculateLocalTimeOffset(newStart, inputTimeType);
    if (cache.offset == startOffset) {
        // If the offset at the start of the new interval still matches
        // the offset in the cache, we grow the cached time interval
        // and return the offset.
        cache.start = newStart;
        cache.incrementStart = WTF::msPerMonth;
        cache.incrementEnd = WTF::msPerMonth;
        return startOffset;
    }
    LocalTimeOffset offset = calculateLocalTimeOffset(millisecondsFromEpoch, inputTimeType);
    if (offset == startOffset) {
        // The offset at the given time is equal to the offset at the
        // new start of the interval, so that means that we've just skipped
        // the point in time where the DST offset change occurred. Update
        // the interval to reflect this and reset the increment.
        cache.start = newStart;
        cache.end = millisecondsFromEpoch;
        cache.incrementStart = WTF::msPerMonth;
        cache.incrementEnd = WTF::msPerMonth;
    } else {
        // The interval contains a DST offset change and the given time is
        // before it. Adjust the increment to avoid a linear search for
        // the offset change point and change the end of the interval.
        cache.incrementStart /= 3;
        cache.start = millisecondsFromEpoch;
    }
    // Update the offset in the cache and return it.
    cache.offset = offset;
    return offset;
}

static inline double timeToMS(double hour, double min, double sec, double ms)
{
    return (((hour * WTF::minutesPerHour + min) * WTF::secondsPerMinute + sec) * WTF::msPerSecond + ms);
}

double DateCache::gregorianDateTimeToMS(const GregorianDateTime& t, double milliseconds, WTF::TimeType inputTimeType)
{
    double day = dateToDaysFrom1970(t.year(), t.month(), t.monthDay());
    double ms = timeToMS(t.hour(), t.minute(), t.second(), milliseconds);
    double localTimeResult = (day * WTF::msPerDay) + ms;

    double localToUTCTimeOffset = inputTimeType == WTF::LocalTime ? localTimeOffset(localTimeResult, inputTimeType).offset : 0;

    return localTimeResult - localToUTCTimeOffset;
}

// input is UTC
void DateCache::msToGregorianDateTime(double millisecondsFromEpoch, WTF::TimeType outputTimeType, GregorianDateTime& tm)
{
    LocalTimeOffset localTime;
    if (outputTimeType == WTF::LocalTime) {
        localTime = localTimeOffset(millisecondsFromEpoch);
        millisecondsFromEpoch += localTime.offset;
    }
    tm = GregorianDateTime(millisecondsFromEpoch, localTime);
}

double DateCache::parseDate(JSGlobalObject* globalObject, VM& vm, const String& date)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (date == m_cachedDateString)
        return m_cachedDateStringValue;
    auto expectedString = date.tryGetUtf8();
    if (!expectedString) {
        if (expectedString.error() == UTF8ConversionError::OutOfMemory)
            throwOutOfMemoryError(globalObject, scope);
        // https://tc39.github.io/ecma262/#sec-date-objects section 20.3.3.2 states that:
        // "Unrecognizable Strings or dates containing illegal element values in the
        // format String shall cause Date.parse to return NaN."
        return std::numeric_limits<double>::quiet_NaN();
    }

    auto parseDateImpl = [this] (const char* dateString) {
        bool isLocalTime;
        double value = WTF::parseES5DateFromNullTerminatedCharacters(dateString, isLocalTime);
        if (std::isnan(value))
            value = WTF::parseDateFromNullTerminatedCharacters(dateString, isLocalTime);

        if (isLocalTime)
            value -= localTimeOffset(value, WTF::LocalTime).offset;

        return value;
    };

    auto dateUtf8 = expectedString.value();
    double value = parseDateImpl(dateUtf8.data());
    m_cachedDateString = date;
    m_cachedDateStringValue = value;
    return value;
}

// https://tc39.es/ecma402/#sec-defaulttimezone
String DateCache::defaultTimeZone()
{
#if HAVE(ICU_C_TIMEZONE_API)
    return timeZoneCache()->m_canonicalTimeZoneID;
#else
    icu::UnicodeString timeZoneID;
    icu::UnicodeString canonicalTimeZoneID;
    auto& timeZone = *bitwise_cast<icu::TimeZone*>(timeZoneCache());
    timeZone.getID(timeZoneID);

    UErrorCode status = U_ZERO_ERROR;
    UBool isSystem = false;
    icu::TimeZone::getCanonicalID(timeZoneID, canonicalTimeZoneID, isSystem, status);
    if (U_FAILURE(status))
        return "UTC"_s;

    String canonical = String(canonicalTimeZoneID.getBuffer(), canonicalTimeZoneID.length());
    if (isUTCEquivalent(canonical))
        return "UTC"_s;

    return canonical;
#endif
}

// To confine icu::TimeZone destructor invocation in this file.
DateCache::DateCache() = default;
DateCache::~DateCache() = default;

Ref<DateInstanceData> DateCache::cachedDateInstanceData(double millisecondsFromEpoch)
{
    return *m_dateInstanceCache.add(millisecondsFromEpoch);
}

void DateCache::timeZoneCacheSlow()
{
    ASSERT(!m_timeZoneCache);
#if HAVE(ICU_C_TIMEZONE_API)
    auto* cache = new OpaqueICUTimeZone;

    String canonical;
    Vector<UChar, 32> timeZoneID;
    auto status = callBufferProducingFunction(ucal_getHostTimeZone, timeZoneID);
    if (U_SUCCESS(status)) {
        Vector<UChar, 32> canonicalBuffer;
        auto status = callBufferProducingFunction(ucal_getCanonicalTimeZoneID, timeZoneID.data(), timeZoneID.size(), canonicalBuffer, nullptr);
        if (U_SUCCESS(status))
            canonical = String(canonicalBuffer);
    }
    if (canonical.isNull() || isUTCEquivalent(canonical))
        canonical = "UTC"_s;
    cache->m_canonicalTimeZoneID = WTFMove(canonical);

    status = U_ZERO_ERROR;
    cache->m_calendar = std::unique_ptr<UCalendar, ICUDeleter<ucal_close>>(ucal_open(timeZoneID.data(), timeZoneID.size(), "", UCAL_DEFAULT, &status));
    ASSERT_UNUSED(status, U_SUCCESS(status));
    ucal_setGregorianChange(cache->m_calendar.get(), minECMAScriptTime, &status); // Ignore "unsupported" error.
    m_timeZoneCache = std::unique_ptr<OpaqueICUTimeZone, OpaqueICUTimeZoneDeleter>(cache);
#else
    // Do not use icu::TimeZone::createDefault. ICU internally has a cache for timezone and createDefault returns this cached value.
    m_timeZoneCache = std::unique_ptr<OpaqueICUTimeZone, OpaqueICUTimeZoneDeleter>(bitwise_cast<OpaqueICUTimeZone*>(icu::TimeZone::detectHostTimeZone()));
#endif
}

void DateCache::reset()
{
    m_timeZoneCache.reset();
    m_utcTimeOffsetCache = LocalTimeOffsetCache();
    m_localTimeOffsetCache = LocalTimeOffsetCache();
    m_cachedDateString = String();
    m_cachedDateStringValue = std::numeric_limits<double>::quiet_NaN();
    m_dateInstanceCache.reset();
}

} // namespace JSC
