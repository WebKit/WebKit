/*
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "Document.h"
#include "PerformanceTiming.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using NavigationTimingFunction = unsigned long long (PerformanceTiming::*)() const;

static NavigationTimingFunction restrictedMarkFunction(const String& markName)
{
    ASSERT(isMainThread());

    static const auto map = makeNeverDestroyed([] {
        static const std::pair<ASCIILiteral, NavigationTimingFunction> pairs[] = {
            { "connectEnd"_s, &PerformanceTiming::connectEnd },
            { "connectStart"_s, &PerformanceTiming::connectStart },
            { "domComplete"_s, &PerformanceTiming::domComplete },
            { "domContentLoadedEventEnd"_s, &PerformanceTiming::domContentLoadedEventEnd },
            { "domContentLoadedEventStart"_s, &PerformanceTiming::domContentLoadedEventStart },
            { "domInteractive"_s, &PerformanceTiming::domInteractive },
            { "domLoading"_s, &PerformanceTiming::domLoading },
            { "domainLookupEnd"_s, &PerformanceTiming::domainLookupEnd },
            { "domainLookupStart"_s, &PerformanceTiming::domainLookupStart },
            { "fetchStart"_s, &PerformanceTiming::fetchStart },
            { "loadEventEnd"_s, &PerformanceTiming::loadEventEnd },
            { "loadEventStart"_s, &PerformanceTiming::loadEventStart },
            { "navigationStart"_s, &PerformanceTiming::navigationStart },
            { "redirectEnd"_s, &PerformanceTiming::redirectEnd },
            { "redirectStart"_s, &PerformanceTiming::redirectStart },
            { "requestStart"_s, &PerformanceTiming::requestStart },
            { "responseEnd"_s, &PerformanceTiming::responseEnd },
            { "responseStart"_s, &PerformanceTiming::responseStart },
            { "secureConnectionStart"_s, &PerformanceTiming::secureConnectionStart },
            { "unloadEventEnd"_s, &PerformanceTiming::unloadEventEnd },
            { "unloadEventStart"_s, &PerformanceTiming::unloadEventStart },
        };
        HashMap<String, NavigationTimingFunction> map;
        for (auto& pair : pairs)
            map.add(pair.first, pair.second);
        return map;
    }());

    return map.get().get(markName);
}

UserTiming::UserTiming(Performance& performance)
    : m_performance(performance)
{
}

static void clearPerformanceEntries(PerformanceEntryMap& map, const String& name)
{
    if (name.isNull())
        map.clear();
    else
        map.remove(name);
}

ExceptionOr<Ref<PerformanceMark>> UserTiming::mark(const String& markName)
{
    if (is<Document>(m_performance.scriptExecutionContext()) && restrictedMarkFunction(markName))
        return Exception { SyntaxError };

    auto& performanceEntryList = m_marksMap.ensure(markName, [] { return Vector<RefPtr<PerformanceEntry>>(); }).iterator->value;
    auto entry = PerformanceMark::create(markName, m_performance.now());
    performanceEntryList.append(entry.copyRef());
    return entry;
}

void UserTiming::clearMarks(const String& markName)
{
    clearPerformanceEntries(m_marksMap, markName);
}

ExceptionOr<double> UserTiming::findExistingMarkStartTime(const String& markName)
{
    auto iterator = m_marksMap.find(markName);
    if (iterator != m_marksMap.end())
        return iterator->value.last()->startTime();

    auto* timing = m_performance.timing();
    if (!timing)
        return Exception { SyntaxError, makeString("No mark named '", markName, "' exists") };

    if (auto function = restrictedMarkFunction(markName)) {
        double value = ((*timing).*(function))();
        if (!value)
            return Exception { InvalidAccessError };
        return value - timing->navigationStart();
    }

    return Exception { SyntaxError };
}

ExceptionOr<Ref<PerformanceMeasure>> UserTiming::measure(const String& measureName, const String& startMark, const String& endMark)
{
    double startTime = 0.0;
    double endTime = 0.0;

    if (startMark.isNull())
        endTime = m_performance.now();
    else if (endMark.isNull()) {
        endTime = m_performance.now();
        auto startMarkResult = findExistingMarkStartTime(startMark);
        if (startMarkResult.hasException())
            return startMarkResult.releaseException();
        startTime = startMarkResult.releaseReturnValue();
    } else {
        auto endMarkResult = findExistingMarkStartTime(endMark);
        if (endMarkResult.hasException())
            return endMarkResult.releaseException();
        auto startMarkResult = findExistingMarkStartTime(startMark);
        if (startMarkResult.hasException())
            return startMarkResult.releaseException();
        startTime = startMarkResult.releaseReturnValue();
        endTime = endMarkResult.releaseReturnValue();
    }

    auto& performanceEntryList = m_measuresMap.ensure(measureName, [] { return Vector<RefPtr<PerformanceEntry>>(); }).iterator->value;
    auto entry = PerformanceMeasure::create(measureName, startTime, endTime);
    performanceEntryList.append(entry.copyRef());
    return entry;
}

void UserTiming::clearMeasures(const String& measureName)
{
    clearPerformanceEntries(m_measuresMap, measureName);
}

static Vector<RefPtr<PerformanceEntry>> convertToEntrySequence(const PerformanceEntryMap& map)
{
    Vector<RefPtr<PerformanceEntry>> entries;
    for (auto& entry : map.values())
        entries.appendVector(entry);
    return entries;
}

Vector<RefPtr<PerformanceEntry>> UserTiming::getMarks() const
{
    return convertToEntrySequence(m_marksMap);
}

Vector<RefPtr<PerformanceEntry>> UserTiming::getMarks(const String& name) const
{
    return m_marksMap.get(name);
}

Vector<RefPtr<PerformanceEntry>> UserTiming::getMeasures() const
{
    return convertToEntrySequence(m_measuresMap);
}

Vector<RefPtr<PerformanceEntry>> UserTiming::getMeasures(const String& name) const
{
    return m_measuresMap.get(name);
}

} // namespace WebCore
