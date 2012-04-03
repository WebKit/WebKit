/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TouchpadFlingPlatformGestureCurve_h
#define TouchpadFlingPlatformGestureCurve_h

#include "FloatPoint.h"
#include "PlatformGestureCurve.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class PlatformGestureCurveTarget;

// Implementation of PlatformGestureCurve suitable for touch pad/screen-based
// fling scroll. Starts with a flat velocity profile based on 'velocity', which
// tails off to zero. Time is scaled to that duration of the fling is proportional
// the initial velocity.
class TouchpadFlingPlatformGestureCurve : public PlatformGestureCurve {
public:
    static PassOwnPtr<PlatformGestureCurve> create(const FloatPoint& velocity, IntPoint cumulativeScroll = IntPoint());
    static PassOwnPtr<PlatformGestureCurve> create(const FloatPoint& velocity, float p0, float p1, float p2, float p3, float p4, float curveDuration, IntPoint cumulativeScroll = IntPoint());
    virtual ~TouchpadFlingPlatformGestureCurve();

    virtual const char* debugName() const { return "TouchpadFling"; }
    virtual bool apply(double monotonicTime, PlatformGestureCurveTarget*);

private:
    TouchpadFlingPlatformGestureCurve(const FloatPoint& velocity, float p0, float p1, float p2, float p3, float p4, float curveDuration, const IntPoint& cumulativeScroll);

    FloatPoint m_displacementRatio;
    IntPoint m_cumulativeScroll;
    float m_coeffs[5];
    float m_timeOffset;
    float m_curveDuration;
    float m_positionOffset;

    static const int m_maxSearchIterations;
};

} // namespace WebCore

#endif
