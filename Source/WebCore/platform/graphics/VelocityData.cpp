/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "VelocityData.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

VelocityData HistoricalVelocityData::velocityForNewData(FloatPoint newPosition, double scale, MonotonicTime timestamp)
{
    auto append = [&](FloatPoint newPosition, double scale, MonotonicTime timestamp)
    {
        m_latestDataIndex = (m_latestDataIndex + 1) % maxHistoryDepth;
        m_positionHistory[m_latestDataIndex] = { timestamp, newPosition, scale };
        m_historySize = std::min(m_historySize + 1, maxHistoryDepth);
        m_lastAppendTimestamp = timestamp;
    };

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

TextStream& operator<<(TextStream& ts, const VelocityData& velocityData)
{
    ts.dumpProperty("timestamp", velocityData.lastUpdateTime.secondsSinceEpoch().value());
    if (velocityData.horizontalVelocity)
        ts.dumpProperty("horizontalVelocity", velocityData.horizontalVelocity);
    if (velocityData.verticalVelocity)
        ts.dumpProperty("verticalVelocity", velocityData.verticalVelocity);
    if (velocityData.scaleChangeRate)
        ts.dumpProperty("scaleChangeRate", velocityData.scaleChangeRate);

    return ts;
}

} // namespace WebCore
