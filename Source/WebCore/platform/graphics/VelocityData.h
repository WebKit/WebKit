/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include "FloatPoint.h"
#include <wtf/MonotonicTime.h>
#include <wtf/Optional.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

struct VelocityData  {
    float horizontalVelocity { 0 };
    float verticalVelocity { 0 };
    float scaleChangeRate { 0 };
    MonotonicTime lastUpdateTime;
    
    VelocityData(float horizontal = 0, float vertical = 0, float scaleChange = 0, MonotonicTime updateTime = MonotonicTime())
        : horizontalVelocity(horizontal)
        , verticalVelocity(vertical)
        , scaleChangeRate(scaleChange)
        , lastUpdateTime(updateTime)
    {
    }
    
    bool velocityOrScaleIsChanging() const
    {
        return horizontalVelocity || verticalVelocity || scaleChangeRate;
    }
    
    bool equalIgnoringTimestamp(const VelocityData& other) const
    {
        return horizontalVelocity == other.horizontalVelocity
            && verticalVelocity == other.verticalVelocity
            && scaleChangeRate == other.scaleChangeRate;
    }
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const VelocityData&);

class HistoricalVelocityData {
public:
    HistoricalVelocityData() = default;

    VelocityData velocityForNewData(FloatPoint newPosition, double scale, MonotonicTime timestamp)
    {
        // Due to all the source of rect update, the input is very noisy. To smooth the output, we accumulate all changes
        // within 1 frame as a single update. No speed computation is ever done on data within the same frame.
        const Seconds filteringThreshold(1.0 / 60);

        VelocityData velocityData;
        if (m_historySize > 0) {
            unsigned oldestDataIndex;
            unsigned distanceToLastHistoricalData = m_historySize - 1;
            if (distanceToLastHistoricalData <= m_latestDataIndex)
                oldestDataIndex = m_latestDataIndex - distanceToLastHistoricalData;
            else
                oldestDataIndex = m_historySize - (distanceToLastHistoricalData - m_latestDataIndex);

            Seconds timeDelta = timestamp - m_positionHistory[oldestDataIndex].timestamp;
            if (timeDelta > filteringThreshold) {
                Data& oldestData = m_positionHistory[oldestDataIndex];
                velocityData = VelocityData((newPosition.x() - oldestData.position.x()) / timeDelta.seconds(), (newPosition.y() - oldestData.position.y()) / timeDelta.seconds(), (scale - oldestData.scale) / timeDelta.seconds(), timestamp);
            }
        }

        Seconds timeSinceLastAppend = timestamp - m_lastAppendTimestamp;
        if (timeSinceLastAppend > filteringThreshold)
            append(newPosition, scale, timestamp);
        else
            m_positionHistory[m_latestDataIndex] = { timestamp, newPosition, scale };

        return velocityData;
    }

    void clear() { m_historySize = 0; }

private:
    void append(FloatPoint newPosition, double scale, MonotonicTime timestamp)
    {
        m_latestDataIndex = (m_latestDataIndex + 1) % maxHistoryDepth;
        m_positionHistory[m_latestDataIndex] = { timestamp, newPosition, scale };
        m_historySize = std::min(m_historySize + 1, maxHistoryDepth);
        m_lastAppendTimestamp = timestamp;
    }

    static constexpr unsigned maxHistoryDepth = 3;

    unsigned m_historySize { 0 };
    unsigned m_latestDataIndex { 0 };
    MonotonicTime m_lastAppendTimestamp;

    struct Data {
        MonotonicTime timestamp;
        FloatPoint position;
        double scale;
    } m_positionHistory[maxHistoryDepth];
};

} // namespace WebCore
