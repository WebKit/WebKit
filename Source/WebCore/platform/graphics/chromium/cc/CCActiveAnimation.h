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

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class CCAnimationCurve;

// A CCActiveAnimation, contains all the state required to play a CCAnimationCurve.
// Specifically, the affected property, the run state (paused, finished, etc.),
// loop count, last pause time, and the total time spent paused.
class CCActiveAnimation {
public:
    // Animations that must be run together are called 'grouped' and have the same GroupID
    // Grouped animations are guaranteed to start at the same time and no other animations
    // may animate any of the group's target properties until all animations in the
    // group have finished animating. Note: an active animation's group id and target
    // property uniquely identify that animation.
    typedef int GroupID;

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

    static PassOwnPtr<CCActiveAnimation> create(PassOwnPtr<CCAnimationCurve> curve, GroupID group, TargetProperty targetProperty)
    {
        return adoptPtr(new CCActiveAnimation(curve, group, targetProperty));
    }

    virtual ~CCActiveAnimation() { }

    GroupID group() const { return m_group; }
    TargetProperty targetProperty() const { return m_targetProperty; }

    RunState runState() const { return m_runState; }
    void setRunState(RunState, double now);

    // This is the number of times that the animation will play. If this
    // value is zero the animation will not play. If it is negative, then
    // the animation will loop indefinitely.
    int iterations() const { return m_iterations; }
    void setIterations(int n) { m_iterations = n; }

    double startTime() const { return m_startTime; }
    void setStartTime(double startTime) { m_startTime = startTime; }

    bool isFinishedAt(double time) const;
    bool isFinished() const { return m_runState == Finished || m_runState == Aborted; }

    CCAnimationCurve* animationCurve() { return m_animationCurve.get(); }
    const CCAnimationCurve* animationCurve() const { return m_animationCurve.get(); }

    // Takes the given absolute time, and using the start time and the number
    // of iterations, returns the relative time in the current iteration.
    double trimTimeToCurrentIteration(double now) const;

private:
    CCActiveAnimation(PassOwnPtr<CCAnimationCurve>, GroupID, TargetProperty);

    OwnPtr<CCAnimationCurve> m_animationCurve;
    GroupID m_group;
    TargetProperty m_targetProperty;
    RunState m_runState;
    int m_iterations;
    double m_startTime;

    // These are used in trimTimeToCurrentIteration to account for time
    // spent while paused. This is not included in AnimationState since it
    // there is absolutely no need for clients of this controller to know
    // about these values.
    double m_pauseTime;
    double m_totalPausedTime;
};

} // namespace WebCore

#endif // CCActiveAnimation_h
