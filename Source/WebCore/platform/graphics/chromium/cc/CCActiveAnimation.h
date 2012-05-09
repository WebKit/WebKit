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

#ifndef CCActiveAnimation_h
#define CCActiveAnimation_h

#include "cc/CCAnimationCurve.h"

#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

// A CCActiveAnimation, contains all the state required to play a CCAnimationCurve.
// Specifically, the affected property, the run state (paused, finished, etc.),
// loop count, last pause time, and the total time spent paused.
class CCActiveAnimation {
    WTF_MAKE_NONCOPYABLE(CCActiveAnimation);
public:
    // Animations begin in one of the 'waiting' states. Animations waiting for the next tick
    // will start the next time the controller animates. Animations waiting for target
    // availibility will run as soon as their target property is free (and all the animations
    // animating with it are also able to run). Animations waiting for their start time to
    // come have be scheduled to run at a particular point in time. When this time arrives,
    // the controller will move the animations into the Running state. Running animations
    // may toggle between Running and Paused, and may be stopped by moving into either the
    // Aborted or Finished states. A Finished animation was allowed to run to completion, but
    // an Aborted animation was not.
    enum RunState {
        WaitingForNextTick = 1,
        WaitingForTargetAvailability,
        WaitingForStartTime,
        Running,
        Paused,
        Finished,
        Aborted
    };

    enum TargetProperty {
        Transform = 1,
        Opacity
    };

    static PassOwnPtr<CCActiveAnimation> create(PassOwnPtr<CCAnimationCurve>, int animationId, int groupId, TargetProperty);

    virtual ~CCActiveAnimation();

    int id() const { return m_id; }
    int group() const { return m_group; }
    TargetProperty targetProperty() const { return m_targetProperty; }

    RunState runState() const { return m_runState; }
    void setRunState(RunState, double monotonicTime);

    // This is the number of times that the animation will play. If this
    // value is zero the animation will not play. If it is negative, then
    // the animation will loop indefinitely.
    int iterations() const { return m_iterations; }
    void setIterations(int n) { m_iterations = n; }

    double startTime() const { return m_startTime; }
    void setStartTime(double monotonicTime) { m_startTime = monotonicTime; }
    bool hasSetStartTime() const { return m_startTime; }

    double timeOffset() const { return m_timeOffset; }
    void setTimeOffset(double monotonicTime) { m_timeOffset = monotonicTime; }

    void suspend(double monotonicTime);
    void resume(double monotonicTime);

    // If alternatesDirection is true, on odd numbered iterations we reverse the curve.
    bool alternatesDirection() const { return m_alternatesDirection; }
    void setAlternatesDirection(bool alternates) { m_alternatesDirection = alternates; }

    bool isFinishedAt(double monotonicTime) const;
    bool isFinished() const { return m_runState == Finished || m_runState == Aborted; }

    CCAnimationCurve* curve() { return m_curve.get(); }
    const CCAnimationCurve* curve() const { return m_curve.get(); }

    // If this is true, even if the animation is running, it will not be tickable until
    // it is given a start time. This is true for animations running on the main thread.
    bool needsSynchronizedStartTime() const { return m_needsSynchronizedStartTime; }
    void setNeedsSynchronizedStartTime(bool needsSynchronizedStartTime) { m_needsSynchronizedStartTime = needsSynchronizedStartTime; }

    // Takes the given absolute time, and using the start time and the number
    // of iterations, returns the relative time in the current iteration.
    double trimTimeToCurrentIteration(double monotonicTime) const;

    PassOwnPtr<CCActiveAnimation> cloneForImplThread() const;

    void pushPropertiesTo(CCActiveAnimation*) const;

private:
    CCActiveAnimation(PassOwnPtr<CCAnimationCurve>, int animationId, int groupId, TargetProperty);

    OwnPtr<CCAnimationCurve> m_curve;

    // IDs are not necessarily unique.
    int m_id;

    // Animations that must be run together are called 'grouped' and have the same group id
    // Grouped animations are guaranteed to start at the same time and no other animations
    // may animate any of the group's target properties until all animations in the
    // group have finished animating. Note: an active animation's group id and target
    // property uniquely identify that animation.
    int m_group;

    TargetProperty m_targetProperty;
    RunState m_runState;
    int m_iterations;
    double m_startTime;
    bool m_alternatesDirection;

    // The time offset effectively pushes the start of the animation back in time. This is
    // used for resuming paused animations -- an animation is added with a non-zero time
    // offset, causing the animation to skip ahead to the desired point in time.
    double m_timeOffset;

    bool m_needsSynchronizedStartTime;

    // When an animation is suspended, it behaves as if it is paused and it also ignores
    // all run state changes until it is resumed. This is used for testing purposes.
    bool m_suspended;

    // These are used in trimTimeToCurrentIteration to account for time
    // spent while paused. This is not included in AnimationState since it
    // there is absolutely no need for clients of this controller to know
    // about these values.
    double m_pauseTime;
    double m_totalPausedTime;
};

} // namespace WebCore

#endif // CCActiveAnimation_h
