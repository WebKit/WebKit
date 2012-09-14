/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
#include "MemoryInfo.h"

#include "Frame.h"
#include "ScriptGCEvent.h"
#include "Settings.h"
#include <limits>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>

namespace WebCore {

#if ENABLE(INSPECTOR)

class HeapSizeCache {
    WTF_MAKE_NONCOPYABLE(HeapSizeCache); WTF_MAKE_FAST_ALLOCATED;
public:
    HeapSizeCache()
        : m_lastUpdateTime(0)
    {
    }

    void getCachedHeapSize(HeapInfo& info)
    {
        maybeUpdate();
        info = m_info;
    }

private:
    void maybeUpdate()
    {
        // We rate-limit queries to once every twenty minutes to make it more difficult
        // for attackers to compare memory usage before and after some event.
        const double TwentyMinutesInSeconds = 20 * 60;

        double now = monotonicallyIncreasingTime();
        if (now - m_lastUpdateTime >= TwentyMinutesInSeconds) {
            update();
            m_lastUpdateTime = now;
        }
    }

    void update()
    {
        ScriptGCEvent::getHeapSize(m_info);
        m_info.usedJSHeapSize = quantizeMemorySize(m_info.usedJSHeapSize);
        m_info.totalJSHeapSize = quantizeMemorySize(m_info.totalJSHeapSize);
        m_info.jsHeapSizeLimit = quantizeMemorySize(m_info.jsHeapSizeLimit);
    }

    double m_lastUpdateTime;

    HeapInfo m_info;
};

// We quantize the sizes to make it more difficult for an attacker to see precise
// impact of operations on memory. The values are used for performance tuning,
// and hence don't need to be as refined when the value is large, so we threshold
// at a list of exponentially separated buckets.
size_t quantizeMemorySize(size_t size)
{
    const int numberOfBuckets = 100;
    DEFINE_STATIC_LOCAL(Vector<size_t>, bucketSizeList, ());

    ASSERT(isMainThread());
    if (bucketSizeList.isEmpty()) {
        bucketSizeList.resize(numberOfBuckets);

        float sizeOfNextBucket = 10000000.0; // First bucket size is roughly 10M.
        const float largestBucketSize = 4000000000.0; // Roughly 4GB.
        // We scale with the Nth root of the ratio, so that we use all the bucktes.
        const float scalingFactor = exp(log(largestBucketSize / sizeOfNextBucket) / numberOfBuckets);

        size_t nextPowerOfTen = static_cast<size_t>(pow(10, floor(log10(sizeOfNextBucket)) + 1) + 0.5);
        size_t granularity = nextPowerOfTen / 1000; // We want 3 signficant digits.

        for (int i = 0; i < numberOfBuckets; ++i) {
            size_t currentBucketSize = static_cast<size_t>(sizeOfNextBucket);
            bucketSizeList[i] = currentBucketSize - (currentBucketSize % granularity);

            sizeOfNextBucket *= scalingFactor;
            if (sizeOfNextBucket >= nextPowerOfTen) {
                if (std::numeric_limits<size_t>::max() / 10 <= nextPowerOfTen)
                    nextPowerOfTen = std::numeric_limits<size_t>::max();                       
                else {
                    nextPowerOfTen *= 10;
                    granularity *= 10;
                }
            }

            // Watch out for overflow, if the range is too large for size_t.
            if (i > 0 && bucketSizeList[i] < bucketSizeList[i - 1])
                bucketSizeList[i] = std::numeric_limits<size_t>::max();
        }
    }

    for (int i = 0; i < numberOfBuckets; ++i) {
        if (size <= bucketSizeList[i])
            return bucketSizeList[i];
    }

    return bucketSizeList[numberOfBuckets - 1];
}

#endif

MemoryInfo::MemoryInfo(Frame* frame)
{
    if (!frame || !frame->settings())
        return;

#if ENABLE(INSPECTOR)
    if (frame->settings()->memoryInfoEnabled())
        ScriptGCEvent::getHeapSize(m_info);
    else if (true || frame->settings()->quantizedMemoryInfoEnabled()) {
        DEFINE_STATIC_LOCAL(HeapSizeCache, heapSizeCache, ());
        heapSizeCache.getCachedHeapSize(m_info);
    }
#endif
}

} // namespace WebCore
