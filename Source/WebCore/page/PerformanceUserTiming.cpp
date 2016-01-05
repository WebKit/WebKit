/*
 * Copyright (C) 2012 Intel Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PerformanceUserTiming.h"

#if ENABLE(USER_TIMING)

#include "Performance.h"
#include "PerformanceMark.h"
#include "PerformanceMeasure.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/dtoa/utils.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace {

static NavigationTimingFunction restrictedMarkFunction(const String& markName)
{
    using MapPair = std::pair<ASCIILiteral, NavigationTimingFunction>;
    static const std::array<MapPair, 21> pairs = { {
        MapPair{ ASCIILiteral("navigationStart"), &PerformanceTiming::navigationStart },
        MapPair{ ASCIILiteral("unloadEventStart"), &PerformanceTiming::unloadEventStart },
        MapPair{ ASCIILiteral("unloadEventEnd"), &PerformanceTiming::unloadEventEnd },
        MapPair{ ASCIILiteral("redirectStart"), &PerformanceTiming::redirectStart },
        MapPair{ ASCIILiteral("redirectEnd"), &PerformanceTiming::redirectEnd },
        MapPair{ ASCIILiteral("fetchStart"), &PerformanceTiming::fetchStart },
        MapPair{ ASCIILiteral("domainLookupStart"), &PerformanceTiming::domainLookupStart },
        MapPair{ ASCIILiteral("domainLookupEnd"), &PerformanceTiming::domainLookupEnd },
        MapPair{ ASCIILiteral("connectStart"), &PerformanceTiming::connectStart },
        MapPair{ ASCIILiteral("connectEnd"), &PerformanceTiming::connectEnd },
        MapPair{ ASCIILiteral("secureConnectionStart"), &PerformanceTiming::secureConnectionStart },
        MapPair{ ASCIILiteral("requestStart"), &PerformanceTiming::requestStart },
        MapPair{ ASCIILiteral("responseStart"), &PerformanceTiming::responseStart },
        MapPair{ ASCIILiteral("responseEnd"), &PerformanceTiming::responseEnd },
        MapPair{ ASCIILiteral("domLoading"), &PerformanceTiming::domLoading },
        MapPair{ ASCIILiteral("domInteractive"), &PerformanceTiming::domInteractive },
        MapPair{ ASCIILiteral("domContentLoadedEventStart"), &PerformanceTiming::domContentLoadedEventStart },
        MapPair{ ASCIILiteral("domContentLoadedEventEnd"), &PerformanceTiming::domContentLoadedEventEnd },
        MapPair{ ASCIILiteral("domComplete"), &PerformanceTiming::domComplete },
        MapPair{ ASCIILiteral("loadEventStart"), &PerformanceTiming::loadEventStart },
        MapPair{ ASCIILiteral("loadEventEnd"), &PerformanceTiming::loadEventEnd },
    } };

    static NeverDestroyed<HashMap<String, NavigationTimingFunction>> map;
    if (map.get().isEmpty()) {
        for (auto& pair : pairs)
            map.get().add(pair.first, pair.second);
    }

    return map.get().get(markName);
}

} // namespace anonymous

UserTiming::UserTiming(Performance* performance)
    : m_performance(performance)
{
}

static void insertPerformanceEntry(PerformanceEntryMap& performanceEntryMap, PassRefPtr<PerformanceEntry> performanceEntry)
{
    RefPtr<PerformanceEntry> entry = performanceEntry;
    PerformanceEntryMap::iterator it = performanceEntryMap.find(entry->name());
    if (it != performanceEntryMap.end())
        it->value.append(entry);
    else
        performanceEntryMap.set(entry->name(), Vector<RefPtr<PerformanceEntry>>{ entry });
}

static void clearPeformanceEntries(PerformanceEntryMap& performanceEntryMap, const String& name)
{
    if (name.isNull()) {
        performanceEntryMap.clear();
        return;
    }

    performanceEntryMap.remove(name);
}

void UserTiming::mark(const String& markName, ExceptionCode& ec)
{
    ec = 0;
    if (restrictedMarkFunction(markName)) {
        ec = SYNTAX_ERR;
        return;
    }

    double startTime = m_performance->now();
    insertPerformanceEntry(m_marksMap, PerformanceMark::create(markName, startTime));
}

void UserTiming::clearMarks(const String& markName)
{
    clearPeformanceEntries(m_marksMap, markName);
}

double UserTiming::findExistingMarkStartTime(const String& markName, ExceptionCode& ec)
{
    ec = 0;

    if (m_marksMap.contains(markName))
        return m_marksMap.get(markName).last()->startTime();

    if (auto function = restrictedMarkFunction(markName)) {
        double value = static_cast<double>((m_performance->timing()->*(function))());
        if (!value) {
            ec = INVALID_ACCESS_ERR;
            return 0.0;
        }
        return value - m_performance->timing()->navigationStart();
    }

    ec = SYNTAX_ERR;
    return 0.0;
}

void UserTiming::measure(const String& measureName, const String& startMark, const String& endMark, ExceptionCode& ec)
{
    double startTime = 0.0;
    double endTime = 0.0;

    if (startMark.isNull())
        endTime = m_performance->now();
    else if (endMark.isNull()) {
        endTime = m_performance->now();
        startTime = findExistingMarkStartTime(startMark, ec);
        if (ec)
            return;
    } else {
        endTime = findExistingMarkStartTime(endMark, ec);
        if (ec)
            return;
        startTime = findExistingMarkStartTime(startMark, ec);
        if (ec)
            return;
    }

    insertPerformanceEntry(m_measuresMap, PerformanceMeasure::create(measureName, startTime, endTime));
}

void UserTiming::clearMeasures(const String& measureName)
{
    clearPeformanceEntries(m_measuresMap, measureName);
}

static Vector<RefPtr<PerformanceEntry> > convertToEntrySequence(const PerformanceEntryMap& performanceEntryMap)
{
    Vector<RefPtr<PerformanceEntry> > entries;

    for (auto& entry : performanceEntryMap.values())
        entries.appendVector(entry);

    return entries;
}

static Vector<RefPtr<PerformanceEntry> > getEntrySequenceByName(const PerformanceEntryMap& performanceEntryMap, const String& name)
{
    Vector<RefPtr<PerformanceEntry> > entries;

    PerformanceEntryMap::const_iterator it = performanceEntryMap.find(name);
    if (it != performanceEntryMap.end())
        entries.appendVector(it->value);

    return entries;
}

Vector<RefPtr<PerformanceEntry> > UserTiming::getMarks() const
{
    return convertToEntrySequence(m_marksMap);
}

Vector<RefPtr<PerformanceEntry> > UserTiming::getMarks(const String& name) const
{
    return getEntrySequenceByName(m_marksMap, name);
}

Vector<RefPtr<PerformanceEntry> > UserTiming::getMeasures() const
{
    return convertToEntrySequence(m_measuresMap);
}

Vector<RefPtr<PerformanceEntry> > UserTiming::getMeasures(const String& name) const
{
    return getEntrySequenceByName(m_measuresMap, name);
}

} // namespace WebCore

#endif // ENABLE(USER_TIMING)
