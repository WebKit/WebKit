/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "FullscreenTouchSecheuristic.h"

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)

#include <wtf/MonotonicTime.h>

namespace WebKit {

double FullscreenTouchSecheuristic::scoreOfNextTouch(CGPoint location)
{
    Seconds now = MonotonicTime::now().secondsSinceEpoch();

    if (!m_lastTouchTime) {
        m_lastTouchTime = WTFMove(now);
        m_lastTouchLocation = WTFMove(location);
        return 0;
    }

    Seconds deltaTime = now - m_lastTouchTime;

    double coefficient = attenuationFactor(deltaTime);
    m_lastScore = coefficient * distanceScore(location, m_lastTouchLocation, deltaTime) + (1 - coefficient) * m_lastScore;

    m_lastTouchTime = now;
    m_lastTouchLocation = location;
    return m_lastScore;
}

void FullscreenTouchSecheuristic::reset()
{
    m_lastTouchTime = 0_s;
    m_lastTouchLocation = { };
    m_lastScore = 0;
}

double FullscreenTouchSecheuristic::distanceScore(const CGPoint& nextLocation, const CGPoint& lastLocation, const Seconds& deltaTime)
{
    double distance = sqrt(
        m_xWeight * pow(nextLocation.x - lastLocation.x, 2) +
        m_yWeight * pow(nextLocation.y - lastLocation.y, 2));
    double sizeFactor = sqrt(
        m_xWeight * pow(m_size.width, 2) +
        m_yWeight * pow(m_size.height, 2));
    double scaledDistance = distance / sizeFactor;
    if (scaledDistance <= m_cutoff)
        return scaledDistance * (m_rampUpSpeed / deltaTime);

    double exponentialDistance = m_cutoff + pow((scaledDistance - m_cutoff) / (1 - m_cutoff), m_gamma);
    return exponentialDistance * (m_rampUpSpeed / deltaTime);
}

double FullscreenTouchSecheuristic::attenuationFactor(Seconds delta)
{
    double normalizedTimeDelta = delta / m_rampDownSpeed;
    return std::max(std::min(normalizedTimeDelta * m_weight, 1.0), 0.0);
}

}

#endif // ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
