/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012 the V8 project authors. All rights reserved.
 * Copyright (C) 2010 Research In Motion Limited. All rights reserved.
 *
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All rights reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 */

#pragma once

#include <JavaScriptCore/JSCTimeZone.h>
#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/PlainGregorianDateTime.h>
#include <JavaScriptCore/PureNaN.h>
#include <wtf/Compiler.h>
#include <wtf/DateMath.h>
#include <wtf/HashFunctions.h>
#include <wtf/Platform.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TimeZone.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class DateInstance;
class JSGlobalObject;
class OpaqueICUTimeZone;
class VM;

static constexpr double minECMAScriptTime = -8.64E15;

// We do not expose icu::TimeZone in this header file. And we cannot use icu::TimeZone forward declaration
// because icu namespace can be an alias to icu$verNum namespace.
struct OpaqueICUTimeZoneDeleter {
    JS_EXPORT_PRIVATE void operator()(OpaqueICUTimeZone*);
};

struct LocalTimeOffsetCache {
    LocalTimeOffsetCache() = default;

    bool isEmpty() { return start > end; }

    LocalTimeOffset offset { };
    int64_t start { WTF::Int64Milliseconds::maxECMAScriptTime };
    int64_t end { WTF::Int64Milliseconds::minECMAScriptTime };
    uint64_t epoch { 0 };
};

class DateCache {
    WTF_MAKE_TZONE_NON_HEAP_ALLOCATABLE(DateCache);
    WTF_MAKE_NONCOPYABLE(DateCache);
public:
    DateCache();
    ~DateCache();

    bool hasTimeZoneChange()
    {
#if USE(TIME_ZONE_CHANGE_NOTIFICATIONS)
        return m_cachedTimeZoneID != WTF::lastTimeZoneID();
#else
        return true;
#endif
    }

    JS_EXPORT_PRIVATE void clearForTimeZoneChange();

    // A DateInstance's cached local-time breakdown is only valid for the zone it was computed in,
    // and nothing tracks which instances hold one, so a zone change has to sweep the heap. Skip
    // that walk entirely while no instance has decomposed a local time.
    void noteCachedLocalGregorianDateTime() { m_mayHaveCachedLocalGregorianDateTime = true; }
    bool takeMayHaveCachedLocalGregorianDateTime() { return std::exchange(m_mayHaveCachedLocalGregorianDateTime, false); }

    TimeZone defaultTimeZone();
    String timeZoneDisplayName(bool isDST);

    // Whether to consult the cross-instance memo. It pays off for a Date built from a time value
    // some other Date has already decomposed, and costs a probe for one being walked forward.
    enum class UseSharedCache : bool { No, Yes };
    PlainGregorianDateTime msToGregorianDateTime(double millisecondsFromEpoch, TimeType outputTimeType, UseSharedCache = UseSharedCache::Yes);
    double gregorianDateTimeToMS(int32_t year, int32_t month, int32_t monthDay, int32_t hour, int32_t minute, int32_t second, double milliseconds, TimeType);
    double localTimeToMS(double milliseconds, TimeType);
    JS_EXPORT_PRIVATE double parseDate(JSGlobalObject*, VM&, const WTF::String&);

private:
    class DSTCache {
    public:
        static constexpr unsigned cacheSize = 32;
        // The implementation relies on the fact that no time zones have
        // more than one daylight savings offset change per 19 days.
        // In Egypt in 2010 they decided to suspend DST during Ramadan. This
        // led to a short interval where DST is in effect from September 10 to
        // September 30.
        static constexpr int64_t defaultDSTDeltaInMilliseconds = 19 * WTF::Int64Milliseconds::secondsPerDay * 1000;

        DSTCache()
            : m_before(m_entries.data())
            , m_after(m_entries.data() + 1)
        {
        }

        uint64_t bumpEpoch()
        {
            ++m_epoch;
            return m_epoch;
        }

        void reset()
        {
            m_entries.fill(LocalTimeOffsetCache { });
            m_before = m_entries.data();
            m_after = m_entries.data() + 1;
            m_epoch = 0;
        }

        LocalTimeOffset localTimeOffset(DateCache&, int64_t millisecondsFromEpoch, TimeType);

    private:
        LocalTimeOffsetCache* NODELETE leastRecentlyUsed(LocalTimeOffsetCache* exclude);
        std::tuple<LocalTimeOffsetCache*, LocalTimeOffsetCache*> probe(int64_t millisecondsFromEpoch);
        void extendTheAfterCache(int64_t millisecondsFromEpoch, LocalTimeOffset);

        uint64_t m_epoch { 0 };
        std::array<LocalTimeOffsetCache, cacheSize> m_entries { };
        LocalTimeOffsetCache* m_before;
        LocalTimeOffsetCache* m_after;
    };

    struct YearMonthDayCache {
        int32_t m_days { 0 };
        int32_t m_year { 0 };
        int32_t m_month { 0 };
        int32_t m_day { 0 };
    };

    // Memoizes the broken-down form of a time value across DateInstances: building a fresh Date
    // from a time value another Date has already decomposed is a common shape.
    class BrokenDownDateCache {
    public:
        static constexpr unsigned cacheSize = 8;

        PlainGregorianDateTime get(double millisecondsFromEpoch)
        {
            auto& entry = lookup(millisecondsFromEpoch);
            if (entry.key != millisecondsFromEpoch)
                return { };
            return entry.value;
        }

        void set(double millisecondsFromEpoch, PlainGregorianDateTime value)
        {
            lookup(millisecondsFromEpoch) = { millisecondsFromEpoch, value };
        }

        void reset() { m_entries.fill({ }); }

    private:
        struct Entry {
            double key { PNaN };
            PlainGregorianDateTime value;
        };

        Entry& lookup(double millisecondsFromEpoch)
        {
            return m_entries[WTF::FloatHash<double>::hash(millisecondsFromEpoch) & (cacheSize - 1)];
        }

        std::array<Entry, cacheSize> m_entries { };
    };

    void timeZoneCacheSlow();
    LocalTimeOffset localTimeOffset(int64_t millisecondsFromEpoch, TimeType = TimeType::UTCTime);

    LocalTimeOffset calculateLocalTimeOffset(double millisecondsFromEpoch, TimeType inputTimeType);
    PlainGregorianDateTime computeGregorianDateTime(double millisecondsFromEpoch, TimeType outputTimeType);
    std::tuple<int32_t, int32_t, int32_t> yearMonthDayFromDaysWithCache(int32_t days);

    OpaqueICUTimeZone* timeZoneCache();

    std::unique_ptr<OpaqueICUTimeZone, OpaqueICUTimeZoneDeleter> m_timeZoneCache;
    std::array<DSTCache, 2> m_caches;
    std::optional<YearMonthDayCache> m_yearMonthDayCache;
    String m_cachedDateString;
    double m_cachedDateStringValue;
    std::array<BrokenDownDateCache, 2> m_brokenDownDateCaches;
    uint64_t m_cachedTimeZoneID { 0 };
    bool m_mayHaveCachedLocalGregorianDateTime { false };
    String m_timeZoneStandardDisplayNameCache;
    String m_timeZoneDSTDisplayNameCache;
};

ALWAYS_INLINE bool isUTCEquivalent(StringView timeZone)
{
    return timeZone == "Etc/UTC"_s || timeZone == "Etc/GMT"_s || timeZone == "GMT"_s;
}

// non-IANA timezones
// https://github.com/unicode-org/icu/blob/main/icu4c/source/tools/tzcode/icuzones
ALWAYS_INLINE bool isNonIANA(StringView timeZone)
{
    return (
        timeZone == "ACT"_s
        || timeZone == "AET"_s
        || timeZone == "AGT"_s
        || timeZone == "ART"_s
        || timeZone == "AST"_s
        || timeZone == "BET"_s
        || timeZone == "BST"_s
        || timeZone == "CAT"_s
        || timeZone == "CNT"_s
        || timeZone == "CST"_s
        || timeZone == "CTT"_s
        || timeZone == "EAT"_s
        || timeZone == "ECT"_s
        || timeZone == "IET"_s
        || timeZone == "IST"_s
        || timeZone == "JST"_s
        || timeZone == "MIT"_s
        || timeZone == "NET"_s
        || timeZone == "NST"_s
        || timeZone == "PLT"_s
        || timeZone == "PNT"_s
        || timeZone == "PRT"_s
        || timeZone == "PST"_s
        || timeZone == "SST"_s
        || timeZone == "VST"_s
    );
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
