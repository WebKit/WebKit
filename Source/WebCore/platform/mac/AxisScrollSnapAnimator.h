/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef AxisScrollSnapAnimator_h
#define AxisScrollSnapAnimator_h

#if ENABLE(CSS_SCROLL_SNAP)

#include "AxisScrollSnapOffsets.h"
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

enum class WheelEventStatus {
    UserScrollBegin,
    UserScrolling,
    UserScrollEnd,
    InertialScrollBegin,
    InertialScrolling,
    InertialScrollEnd,
    Unknown
};

class AxisScrollSnapAnimatorClient {
protected:
    virtual ~AxisScrollSnapAnimatorClient() { }

public:
    virtual LayoutUnit scrollOffsetInAxis(ScrollEventAxis) = 0;
    virtual void immediateScrollInAxis(ScrollEventAxis, float velocity) = 0;
    virtual void startScrollSnapTimer(ScrollEventAxis) = 0;
    virtual void stopScrollSnapTimer(ScrollEventAxis) = 0;
};

class AxisScrollSnapAnimator {
public:
    AxisScrollSnapAnimator(AxisScrollSnapAnimatorClient*, const Vector<LayoutUnit>* snapOffsets, ScrollEventAxis);
    void handleWheelEvent(const PlatformWheelEvent&);
    bool shouldOverrideWheelEvent(const PlatformWheelEvent&) const;
    void scrollSnapAnimationUpdate();

private:
    void beginScrollSnapAnimation(ScrollSnapState);
    void endScrollSnapAnimation(ScrollSnapState);

    float computeSnapDelta() const;
    float computeGlideDelta() const;

    void initializeGlideParameters(bool);
    void pushInitialWheelDelta(float);
    float averageInitialWheelDelta() const;
    void clearInitialWheelDeltaWindow();

    static const int wheelDeltaWindowSize = 3;

    AxisScrollSnapAnimatorClient* m_client;
    const Vector<LayoutUnit>* m_snapOffsets;
    ScrollEventAxis m_axis;
    // Used to track both snapping and gliding behaviors.
    ScrollSnapState m_currentState;
    LayoutUnit m_initialOffset;
    LayoutUnit m_targetOffset;
    // Used to track gliding behavior.
    LayoutUnit m_beginTrackingWheelDeltaOffset;
    int m_numWheelDeltasTracked;
    float m_wheelDeltaWindow[wheelDeltaWindowSize];
    float m_glideMagnitude;
    float m_glidePhaseShift;
    float m_glideInitialWheelDelta;
    bool m_shouldOverrideWheelEvent;
};

} // namespace WebCore

#endif // ENABLE(CSS_SCROLL_SNAP)

#endif // AxisScrollSnapAnimator_h
