/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#ifndef ScrollSnapAnimatorState_h
#define ScrollSnapAnimatorState_h

#if ENABLE(CSS_SCROLL_SNAP)

#include "AxisScrollSnapOffsets.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include "LayoutUnit.h"
#include "PlatformWheelEvent.h"
#include "ScrollTypes.h"

namespace WebCore {

enum class ScrollSnapState {
    Snapping,
    Gliding,
    DestinationReached,
    UserInteraction
};

struct ScrollSnapAnimatorState {
    ScrollSnapAnimatorState(ScrollEventAxis, const Vector<LayoutUnit>&);

    void pushInitialWheelDelta(float);
    float averageInitialWheelDelta() const;
    void clearInitialWheelDeltaWindow();
    bool isSnapping() const;
    bool canReachTargetWithCurrentInitialScrollDelta() const;
    float interpolatedOffsetAtProgress(float) const;
    
    static const int wheelDeltaWindowSize = 3;

    Vector<LayoutUnit> m_snapOffsets;
    ScrollEventAxis m_axis;
    // Used to track both snapping and gliding behaviors.
    ScrollSnapState m_currentState;
    LayoutUnit m_initialOffset;
    LayoutUnit m_targetOffset;
    // Used to track gliding behavior.
    LayoutUnit m_beginTrackingWheelDeltaOffset;
    int m_numWheelDeltasTracked { 0 };
    unsigned m_activeSnapIndex { 0 };
    float m_wheelDeltaWindow[wheelDeltaWindowSize];
    float m_initialScrollDelta { 0 };
    bool m_shouldOverrideWheelEvent { false };
};
    
/**
 * Stores state variables necessary to coordinate snapping animations between
 * horizontal and vertical axes.
 */
struct ScrollSnapAnimationCurveState {
    
    void initializeSnapProgressCurve(const FloatSize&, const FloatSize&, const FloatSize&);
    void initializeInterpolationCoefficientsIfNecessary(const FloatSize&, const FloatSize&, const FloatSize&);
    FloatPoint interpolatedPositionAtProgress(float) const;
    bool shouldCompleteSnapAnimationImmediatelyAtTime(double) const;
    float animationProgressAtTime(double) const;

    bool shouldAnimateDirectlyToSnapPoint { false };
    
private:
    double m_startTime { 0 };
    float m_snapAnimationCurveMagnitude { 0 };
    float m_snapAnimationDecayFactor { 0 };
    FloatSize m_snapAnimationCurveCoefficients[4] { };
};


} // namespace WebCore

#endif // ENABLE(CSS_SCROLL_SNAP)

#endif // ScrollSnapAnimatorState_h
