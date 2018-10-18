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

#pragma once

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)

#include <wtf/Seconds.h>

namespace WebKit {

class FullscreenTouchSecheuristic {
public:
    double scoreOfNextTouch(CGPoint location);
    void reset();

    void setRampUpSpeed(Seconds speed) { m_rampUpSpeed = speed; }
    void setRampDownSpeed(Seconds speed) { m_rampDownSpeed = speed; }
    void setXWeight(double weight) { m_xWeight = weight; }
    void setYWeight(double weight) { m_yWeight = weight; }
    void setSize(CGSize size) { m_size = size; }
    void setGamma(double gamma) { m_gamma = gamma; }
    void setGammaCutoff(double cutoff) { m_cutoff = cutoff; }

private:
    double distanceScore(const CGPoint& nextLocation, const CGPoint& lastLocation, const Seconds& deltaTime);
    double attenuationFactor(Seconds delta);

    double m_weight { 0.1 };
    Seconds m_rampUpSpeed { 1 };
    Seconds m_rampDownSpeed { 1 };
    double m_xWeight { 1 };
    double m_yWeight { 1 };
    double m_gamma { 1 };
    double m_cutoff { 1 };
    CGSize m_size { };
    Seconds m_lastTouchTime { 0 };
    CGPoint m_lastTouchLocation { };
    double m_lastScore { 0 };
};

}

#endif // ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
