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
#include "StatisticsManager.h"

#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WTF {

WTF_MAKE_TZONE_ALLOCATED_IMPL(StatisticsManager);

StatisticsManager& StatisticsManager::singleton()
{
    static NeverDestroyed<UniqueRef<StatisticsManager>> instance = makeUniqueRef<StatisticsManager>();
    return instance.get();
}

NO_RETURN_DUE_TO_CRASH StatisticsManager::~StatisticsManager()
{
    RELEASE_ASSERT_NOT_REACHED();
}

void StatisticsManager::addDataPoint(ASCIILiteral id, double value)
{
    Locker locker { m_lock };
    m_data.add(id, Vector<double>()).iterator->value.append(value);
}

static void dumpHistogram(const AbstractLocker&, const Vector<double>& values, double min, double max, size_t binCount = 10)
{
    if (values.isEmpty() || min == max)
        return;

    Vector<size_t> bins(binCount, 0);
    double binSize = (max - min) / binCount;

    for (double value : values) {
        size_t index = std::min(binCount - 1, static_cast<size_t>((value - min) / binSize));
        ++bins[index];
    }

    for (size_t i = 0; i < binCount; ++i) {
        double start = min + i * binSize;
        double end = start + binSize;
        dataLogF("  [%.2f - %.2f): %zu\n", start, end, bins[i]);
    }
}

void StatisticsManager::dumpStatistics()
{
    Locker locker { m_lock };

    for (const auto& entry : m_data) {
        const String& id = entry.key;
        const Vector<double>& values = entry.value;
        size_t count = values.size();
        if (!count)
            continue;

        double sum = 0;
        double sumOfSquares = 0;
        double min = std::numeric_limits<double>::infinity();
        double max = -min;

        for (double value : values) {
            sum += value;
            sumOfSquares += value * value;
            min = std::min(min, value);
            max = std::max(max, value);
        }

        double mean = sum / count;
        double variance = (sumOfSquares / count) - (mean * mean);
        double stddev = std::sqrt(variance);

        CString utf8ID = id.utf8();
        dataLogF(
            "Statistics for %s:\n"
            "  Count    : %zu\n"
            "  Min      : %.6f\n"
            "  Max      : %.6f\n"
            "  Mean     : %.6f\n"
            "  Variance : %.6f\n"
            "  Std Dev  : %.6f\n"
            "  Histogram:\n",
            utf8ID.data(), count, min, max, mean, variance, stddev);

        dumpHistogram(locker, values, min, max);
        dataLogF("\n");
    }
}

void StatisticsManager::clear()
{
    Locker locker { m_lock };
    m_data.clear();
}

} // namespace WTF
