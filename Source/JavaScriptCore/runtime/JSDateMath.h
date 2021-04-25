/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 * the Initial Developer. All Rights Reserved.
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

#include "DateInstanceCache.h"
#include <wtf/DateMath.h>
#include <wtf/GregorianDateTime.h>

namespace JSC {

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

    LocalTimeOffset offset;
    double start { 0.0 };
    double end { -1.0 };
    double incrementStart { 0.0 };
    double incrementEnd { 0.0 };
};

class DateCache {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DateCache);
public:
    DateCache();
    ~DateCache();

    JS_EXPORT_PRIVATE void reset();

    String defaultTimeZone();

    Ref<DateInstanceData> cachedDateInstanceData(double millisecondsFromEpoch);

    void msToGregorianDateTime(double millisecondsFromEpoch, WTF::TimeType outputTimeType, GregorianDateTime&);
    double gregorianDateTimeToMS(const GregorianDateTime&, double milliseconds, WTF::TimeType inputTimeType);
    JS_EXPORT_PRIVATE double parseDate(JSGlobalObject*, VM&, const WTF::String&);

private:
    void timeZoneCacheSlow();
    LocalTimeOffset localTimeOffset(double millisecondsFromEpoch, WTF::TimeType inputTimeType = WTF::UTCTime);
    LocalTimeOffset calculateLocalTimeOffset(double millisecondsFromEpoch, WTF::TimeType inputTimeType);

    OpaqueICUTimeZone* timeZoneCache()
    {
        if (!m_timeZoneCache)
            timeZoneCacheSlow();
        return m_timeZoneCache.get();
    }

    std::unique_ptr<OpaqueICUTimeZone, OpaqueICUTimeZoneDeleter> m_timeZoneCache;
    LocalTimeOffsetCache m_utcTimeOffsetCache;
    LocalTimeOffsetCache m_localTimeOffsetCache;
    String m_cachedDateString;
    double m_cachedDateStringValue;
    DateInstanceCache m_dateInstanceCache;
};

ALWAYS_INLINE bool isUTCEquivalent(StringView timeZone)
{
    return timeZone == "Etc/UTC"_s || timeZone == "Etc/GMT"_s;
}

} // namespace JSC
