/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Animation.h"
#include "CSSPropertyBlendingClient.h"
#include "CSSPropertyNames.h"
#include "RenderStyleConstants.h"

namespace WebCore {

class CompositeAnimation;
class Element;
class FloatRect;
class LayoutRect;
class RenderBoxModelObject;
class RenderElement;
class RenderStyle;
class TimingFunction;

class AnimationBase : public RefCounted<AnimationBase>
    , public CSSPropertyBlendingClient {
    friend class CompositeAnimation;
    friend class CSSPropertyAnimation;
    WTF_MAKE_FAST_ALLOCATED;
public:
    AnimationBase(const Animation& transition, Element&, CompositeAnimation&);
    virtual ~AnimationBase();

    Element* element() const { return m_element.get(); }
    const RenderStyle& currentStyle() const override;
    RenderElement* renderer() const override;
    RenderBoxModelObject* compositedRenderer() const;
    void clear();

    double duration() const;

    // Animations and Transitions go through the states below. When entering the STARTED state
    // the animation is started. This may or may not require deferred response from the animator.
    // If so, we stay in this state until that response is received (and it returns the start time).
    // Otherwise, we use the current time as the start time and go immediately to AnimationState::Looping
    // or AnimationState::Ending.
    enum class AnimationState : uint8_t {
        New,                        // animation just created, animation not running yet
        StartWaitTimer,             // start timer running, waiting for fire
        StartWaitStyleAvailable,    // waiting for style setup so we can start animations
        StartWaitResponse,          // animation started, waiting for response
        Looping,                    // response received, animation running, loop timer running, waiting for fire
        Ending,                     // received, animation running, end timer running, waiting for fire
        PausedNew,                  // in pause mode when animation was created
        PausedWaitTimer,            // in pause mode when animation started
        PausedWaitStyleAvailable,   // in pause mode when waiting for style setup
        PausedWaitResponse,         // animation paused when in STARTING state
        PausedRun,                  // animation paused when in LOOPING or ENDING state
        Done,                       // end timer fired, animation finished and removed
        FillingForwards             // animation has ended and is retaining its final value
    };

    enum class AnimationStateInput : uint8_t {
        MakeNew,           // reset back to new from any state
        StartAnimation,    // animation requests a start
        RestartAnimation,  // force a restart from any state
        StartTimerFired,   // start timer fired
        StyleAvailable,    // style is setup, ready to start animating
        StartTimeSet,      // m_startTime was set
        LoopTimerFired,    // loop timer fired
        EndTimerFired,     // end timer fired
        PauseOverride,     // pause an animation due to override
        ResumeOverride,    // resume an overridden animation
        PlayStateRunning,  // play state paused -> running
        PlayStatePaused,   // play state running -> paused
        EndAnimation       // force an end from any state
    };

    // Called when animation is in AnimationState::New to start animation
    void updateStateMachine(AnimationStateInput, double param);

    // Animation has actually started, at passed time
    void onAnimationStartResponse(MonotonicTime startTime)
    {
        updateStateMachine(AnimationStateInput::StartTimeSet, startTime.secondsSinceEpoch().seconds());
    }

    // Called to change to or from paused state
    void updatePlayState(AnimationPlayState);
    bool playStatePlaying() const;

    bool waitingToStart() const { return m_animationState == AnimationState::New || m_animationState == AnimationState::StartWaitTimer || m_animationState == AnimationState::PausedNew; }
    bool preActive() const
    {
        return m_animationState == AnimationState::New || m_animationState == AnimationState::StartWaitTimer || m_animationState == AnimationState::StartWaitStyleAvailable || m_animationState == AnimationState::StartWaitResponse;
    }

    bool postActive() const { return m_animationState == AnimationState::Done; }
    bool fillingForwards() const { return m_animationState == AnimationState::FillingForwards; }
    bool active() const { return !postActive() && !preActive(); }
    bool running() const { return !isNew() && !postActive(); }
    bool paused() const { return m_pauseTime || m_animationState == AnimationState::PausedNew; }
    bool inPausedState() const { return m_animationState >= AnimationState::PausedNew && m_animationState <= AnimationState::PausedRun; }
    bool isNew() const { return m_animationState == AnimationState::New || m_animationState == AnimationState::PausedNew; }
    bool waitingForStartTime() const { return m_animationState == AnimationState::StartWaitResponse; }
    bool waitingForStyleAvailable() const { return m_animationState == AnimationState::StartWaitStyleAvailable; }

    bool isAccelerated() const override { return m_isAccelerated; }

    virtual Optional<Seconds> timeToNextService();

    double progress(double scale = 1, double offset = 0, const TimingFunction* = nullptr) const;

    virtual void getAnimatedStyle(std::unique_ptr<RenderStyle>& /*animatedStyle*/) = 0;

    virtual bool computeExtentOfTransformAnimation(LayoutRect&) const = 0;

    virtual bool shouldFireEvents() const { return false; }

    void fireAnimationEventsIfNeeded();

    bool animationsMatch(const Animation&) const;

    const Animation& animation() const { return m_animation; }
    void setAnimation(const Animation& animation) { m_animation = const_cast<Animation&>(animation); }

    // Return true if this animation is overridden. This will only be the case for
    // ImplicitAnimations and is used to determine whether or not we should force
    // set the start time. If an animation is overridden, it will probably not get
    // back the AnimationStateInput::StartTimeSet input.
    virtual bool overridden() const { return false; }

    // Does this animation/transition involve the given property?
    virtual bool affectsProperty(CSSPropertyID /*property*/) const { return false; }

    enum RunningStates {
        Delaying = 1 << 0,
        Paused = 1 << 1,
        Running = 1 << 2,
    };
    typedef unsigned RunningState;
    bool isAnimatingProperty(CSSPropertyID property, bool acceleratedOnly, RunningState runningState) const
    {
        if (acceleratedOnly && !m_isAccelerated)
            return false;

        if (!affectsProperty(property))
            return false;

        if ((runningState & Delaying) && preActive())
            return true;

        if ((runningState & Paused) && inPausedState())
            return true;

        if ((runningState & Running) && !inPausedState() && (m_animationState >= AnimationState::StartWaitStyleAvailable && m_animationState < AnimationState::Done))
            return true;

        return false;
    }

    bool transformFunctionListsMatch() const override { return m_transformFunctionListsMatch; }
    bool filterFunctionListsMatch() const override { return m_filterFunctionListsMatch; }
#if ENABLE(FILTERS_LEVEL_2)
    bool backdropFilterFunctionListsMatch() const override { return m_backdropFilterFunctionListsMatch; }
#endif
    bool colorFilterFunctionListsMatch() const override { return m_colorFilterFunctionListsMatch; }

    // Freeze the animation; used by DumpRenderTree.
    void freezeAtTime(double t);

    // Play and pause API
    void play();
    void pause();
    
    double beginAnimationUpdateTime() const;
    
    double getElapsedTime() const;
    // Setting the elapsed time will adjust the start time and possibly pause time.
    void setElapsedTime(double);
    
    void styleAvailable() 
    {
        ASSERT(waitingForStyleAvailable());
        updateStateMachine(AnimationStateInput::StyleAvailable, -1);
    }

protected:
    virtual void overrideAnimations() { }
    virtual void resumeOverriddenAnimations() { }

    CompositeAnimation* compositeAnimation() { return m_compositeAnimation; }

    // These are called when the corresponding timer fires so subclasses can do any extra work
    virtual void onAnimationStart(double /*elapsedTime*/) { }
    virtual void onAnimationIteration(double /*elapsedTime*/) { }
    virtual void onAnimationEnd(double /*elapsedTime*/) { }
    
    // timeOffset is an offset from the current time when the animation should start. Negative values are OK.
    // Return value indicates whether to expect an asynchronous notifyAnimationStarted() callback.
    virtual bool startAnimation(double /*timeOffset*/) { return false; }
    // timeOffset is the time at which the animation is being paused.
    virtual void pauseAnimation(double /*timeOffset*/) { }
    virtual void endAnimation(bool /*fillingForwards*/ = false) { }

    virtual const RenderStyle& unanimatedStyle() const = 0;

    void goIntoEndingOrLoopingState();

    AnimationState state() const { return m_animationState; }

    static void setNeedsStyleRecalc(Element*);
    
    void getTimeToNextEvent(Seconds& time, bool& isLooping) const;

    double fractionalTime(double scale, double elapsedTime, double offset) const;

    // These return true if we can easily compute a bounding box by applying the style's transform to the bounds rect.
    bool computeTransformedExtentViaTransformList(const FloatRect& rendererBox, const RenderStyle&, LayoutRect& bounds) const;
    bool computeTransformedExtentViaMatrix(const FloatRect& rendererBox, const RenderStyle&, LayoutRect& bounds) const;

protected:
    bool m_isAccelerated { false };
    bool m_transformFunctionListsMatch { false };
    bool m_filterFunctionListsMatch { false };
#if ENABLE(FILTERS_LEVEL_2)
    bool m_backdropFilterFunctionListsMatch { false };
#endif

private:
    RefPtr<Element> m_element;

protected:
    CompositeAnimation* m_compositeAnimation; // Ideally this would be a reference, but it has to be cleared if an animation is destroyed inside an event callback.
    Ref<Animation> m_animation;

    Optional<double> m_startTime;
    Optional<double> m_pauseTime;
    double m_requestedStartTime { 0 };
    Optional<double> m_totalDuration;
    Optional<double> m_nextIterationDuration;

    AnimationState m_animationState { AnimationState::New };
    bool m_colorFilterFunctionListsMatch { false };
};

} // namespace WebCore
