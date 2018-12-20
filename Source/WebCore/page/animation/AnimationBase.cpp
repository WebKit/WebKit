/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "AnimationBase.h"

#include "CSSAnimationControllerPrivate.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyAnimation.h"
#include "CompositeAnimation.h"
#include "Document.h"
#include "FloatConversion.h"
#include "GeometryUtilities.h"
#include "Logging.h"
#include "RenderBox.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include <algorithm>
#include <wtf/Ref.h>

namespace WebCore {

AnimationBase::AnimationBase(const Animation& animation, Element& element, CompositeAnimation& compositeAnimation)
    : m_element(&element)
    , m_compositeAnimation(&compositeAnimation)
    , m_animation(const_cast<Animation&>(animation))
{
    // Compute the total duration
    if (m_animation->iterationCount() > 0)
        m_totalDuration = m_animation->duration() * m_animation->iterationCount();
}

AnimationBase::~AnimationBase() = default;

const RenderStyle& AnimationBase::currentStyle() const
{
    if (auto* renderer = this->renderer())
        return renderer->style();
    return unanimatedStyle();
}

RenderElement* AnimationBase::renderer() const
{
    return m_element ? m_element->renderer() : nullptr;
}

RenderBoxModelObject* AnimationBase::compositedRenderer() const
{
    auto* renderer = this->renderer();
    if (!renderer || !renderer->isComposited())
        return nullptr;
    return downcast<RenderBoxModelObject>(renderer);
}

void AnimationBase::clear()
{
    endAnimation();
    m_element = nullptr;
    m_compositeAnimation = nullptr;
}

void AnimationBase::setNeedsStyleRecalc(Element* element)
{
    if (!element || element->document().renderTreeBeingDestroyed())
        return;

    ASSERT(element->document().pageCacheState() == Document::NotInPageCache);
    element->invalidateStyleAndLayerComposition();
}

double AnimationBase::duration() const
{
    return m_animation->duration();
}

bool AnimationBase::playStatePlaying() const
{
    return m_animation->playState() == AnimationPlayState::Playing;
}

bool AnimationBase::animationsMatch(const Animation& animation) const
{
    return m_animation->animationsMatch(animation);
}

#if !LOG_DISABLED
static const char* nameForState(AnimationBase::AnimationState state)
{
    switch (state) {
    case AnimationBase::AnimationState::New: return "New";
    case AnimationBase::AnimationState::StartWaitTimer: return "StartWaitTimer";
    case AnimationBase::AnimationState::StartWaitStyleAvailable: return "StartWaitStyleAvailable";
    case AnimationBase::AnimationState::StartWaitResponse: return "StartWaitResponse";
    case AnimationBase::AnimationState::Looping: return "Looping";
    case AnimationBase::AnimationState::Ending: return "Ending";
    case AnimationBase::AnimationState::PausedNew: return "PausedNew";
    case AnimationBase::AnimationState::PausedWaitTimer: return "PausedWaitTimer";
    case AnimationBase::AnimationState::PausedWaitStyleAvailable: return "PausedWaitStyleAvailable";
    case AnimationBase::AnimationState::PausedWaitResponse: return "PausedWaitResponse";
    case AnimationBase::AnimationState::PausedRun: return "PausedRun";
    case AnimationBase::AnimationState::Done: return "Done";
    case AnimationBase::AnimationState::FillingForwards: return "FillingForwards";
    }
    return "";
}

static const char* nameForStateInput(AnimationBase::AnimationStateInput input)
{
    switch (input) {
    case AnimationBase::AnimationStateInput::MakeNew: return "MakeNew";
    case AnimationBase::AnimationStateInput::StartAnimation: return "StartAnimation";
    case AnimationBase::AnimationStateInput::RestartAnimation: return "RestartAnimation";
    case AnimationBase::AnimationStateInput::StartTimerFired: return "StartTimerFired";
    case AnimationBase::AnimationStateInput::StyleAvailable: return "StyleAvailable";
    case AnimationBase::AnimationStateInput::StartTimeSet: return "StartTimeSet";
    case AnimationBase::AnimationStateInput::LoopTimerFired: return "LoopTimerFired";
    case AnimationBase::AnimationStateInput::EndTimerFired: return "EndTimerFired";
    case AnimationBase::AnimationStateInput::PauseOverride: return "PauseOverride";
    case AnimationBase::AnimationStateInput::ResumeOverride: return "ResumeOverride";
    case AnimationBase::AnimationStateInput::PlayStateRunning: return "PlayStateRunning";
    case AnimationBase::AnimationStateInput::PlayStatePaused: return "PlayStatePaused";
    case AnimationBase::AnimationStateInput::EndAnimation: return "EndAnimation";
    }
    return "";
}
#endif

void AnimationBase::updateStateMachine(AnimationStateInput input, double param)
{
    if (!m_compositeAnimation)
        return;

    // If we get AnimationStateInput::RestartAnimation then we force a new animation, regardless of state.
    if (input == AnimationStateInput::MakeNew) {
        if (m_animationState == AnimationState::StartWaitStyleAvailable)
            m_compositeAnimation->animationController().removeFromAnimationsWaitingForStyle(this);
        LOG(Animations, "%p AnimationState %s -> New", this, nameForState(m_animationState));
        m_animationState = AnimationState::New;
        m_startTime = WTF::nullopt;
        m_pauseTime = WTF::nullopt;
        m_requestedStartTime = 0;
        m_nextIterationDuration = WTF::nullopt;
        endAnimation();
        return;
    }

    if (input == AnimationStateInput::RestartAnimation) {
        if (m_animationState == AnimationState::StartWaitStyleAvailable)
            m_compositeAnimation->animationController().removeFromAnimationsWaitingForStyle(this);
        LOG(Animations, "%p AnimationState %s -> New", this, nameForState(m_animationState));
        m_animationState = AnimationState::New;
        m_startTime = WTF::nullopt;
        m_pauseTime = WTF::nullopt;
        m_requestedStartTime = 0;
        m_nextIterationDuration = WTF::nullopt;
        endAnimation();

        if (!paused())
            updateStateMachine(AnimationStateInput::StartAnimation, -1);
        return;
    }

    if (input == AnimationStateInput::EndAnimation) {
        if (m_animationState == AnimationState::StartWaitStyleAvailable)
            m_compositeAnimation->animationController().removeFromAnimationsWaitingForStyle(this);
        LOG(Animations, "%p AnimationState %s -> Done", this, nameForState(m_animationState));
        m_animationState = AnimationState::Done;
        endAnimation();
        return;
    }

    if (input == AnimationStateInput::PauseOverride) {
        if (m_animationState == AnimationState::StartWaitResponse) {
            // If we are in AnimationState::StartWaitResponse, the animation will get canceled before 
            // we get a response, so move to the next state.
            endAnimation();
            updateStateMachine(AnimationStateInput::StartTimeSet, beginAnimationUpdateTime());
        }
        return;
    }

    if (input == AnimationStateInput::ResumeOverride) {
        if (m_animationState == AnimationState::Looping || m_animationState == AnimationState::Ending) {
            // Start the animation
            startAnimation(beginAnimationUpdateTime() - m_startTime.value_or(0));
        }
        return;
    }

    // Execute state machine
    switch (m_animationState) {
        case AnimationState::New:
            ASSERT(input == AnimationStateInput::StartAnimation || input == AnimationStateInput::PlayStateRunning || input == AnimationStateInput::PlayStatePaused);

            if (input == AnimationStateInput::StartAnimation || input == AnimationStateInput::PlayStateRunning) {
                m_requestedStartTime = beginAnimationUpdateTime();
                LOG(Animations, "%p AnimationState %s -> StartWaitTimer", this, nameForState(m_animationState));
                m_animationState = AnimationState::StartWaitTimer;
            } else {
                // We are pausing before we even started.
                LOG(Animations, "%p AnimationState %s -> AnimationState::PausedNew", this, nameForState(m_animationState));
                m_animationState = AnimationState::PausedNew;
                m_pauseTime = WTF::nullopt;
            }

            break;
        case AnimationState::StartWaitTimer:
            ASSERT(input == AnimationStateInput::StartTimerFired || input == AnimationStateInput::PlayStatePaused);

            if (input == AnimationStateInput::StartTimerFired) {
                ASSERT(param >= 0);
                // Start timer has fired, tell the animation to start and wait for it to respond with start time
                LOG(Animations, "%p AnimationState %s -> StartWaitStyleAvailable (time is %f)", this, nameForState(m_animationState), param);
                m_animationState = AnimationState::StartWaitStyleAvailable;
                m_compositeAnimation->animationController().addToAnimationsWaitingForStyle(this);

                // Trigger a render so we can start the animation
                if (m_element)
                    m_compositeAnimation->animationController().addElementChangeToDispatch(*m_element);
            } else {
                ASSERT(!paused());
                // We're waiting for the start timer to fire and we got a pause. Cancel the timer, pause and wait
                m_pauseTime = beginAnimationUpdateTime();
                LOG(Animations, "%p AnimationState %s -> PausedWaitTimer", this, nameForState(m_animationState));
                m_animationState = AnimationState::PausedWaitTimer;
            }
            break;
        case AnimationState::StartWaitStyleAvailable:
            ASSERT(input == AnimationStateInput::StyleAvailable || input == AnimationStateInput::PlayStatePaused);

            if (input == AnimationStateInput::StyleAvailable) {
                // Start timer has fired, tell the animation to start and wait for it to respond with start time
                LOG(Animations, "%p AnimationState %s -> StartWaitResponse (time is %f)", this, nameForState(m_animationState), param);
                m_animationState = AnimationState::StartWaitResponse;

                overrideAnimations();

                // Start the animation
                if (overridden()) {
                    // We won't try to start accelerated animations if we are overridden and
                    // just move on to the next state.
                    LOG(Animations, "%p AnimationState %s -> StartWaitResponse", this, nameForState(m_animationState));
                    m_animationState = AnimationState::StartWaitResponse;
                    m_isAccelerated = false;
                    updateStateMachine(AnimationStateInput::StartTimeSet, beginAnimationUpdateTime());
                } else {
                    double timeOffset = 0;
                    // If the value for 'animation-delay' is negative then the animation appears to have started in the past.
                    if (m_animation->delay() < 0)
                        timeOffset = -m_animation->delay();
                    bool started = startAnimation(timeOffset);

                    m_compositeAnimation->animationController().addToAnimationsWaitingForStartTimeResponse(this, started);
                    m_isAccelerated = started;
                }
            } else {
                // We're waiting for the style to be available and we got a pause. Pause and wait
                m_pauseTime = beginAnimationUpdateTime();
                LOG(Animations, "%p AnimationState %s -> PausedWaitStyleAvailable", this, nameForState(m_animationState));
                m_animationState = AnimationState::PausedWaitStyleAvailable;
            }
            break;
        case AnimationState::StartWaitResponse:
            ASSERT(input == AnimationStateInput::StartTimeSet || input == AnimationStateInput::PlayStatePaused);

            if (input == AnimationStateInput::StartTimeSet) {
                ASSERT(param > -0.001); // Sometimes Core Animation gives us a beginTime slightly into the future.
                LOG(Animations, "%p AnimationState %s -> StartTimeSet (time is %f)", this, nameForState(m_animationState), param);

                // We have a start time, set it, unless the startTime is already set
                if (!m_startTime) {
                    m_startTime = param;
                    // If the value for 'animation-delay' is negative then the animation appears to have started in the past.
                    if (m_animation->delay() < 0)
                        m_startTime = m_startTime.value() + m_animation->delay();
                }

                // Now that we know the start time, fire the start event.
                onAnimationStart(0); // The elapsedTime is 0.

                // Decide whether to go into looping or ending state
                goIntoEndingOrLoopingState();

                // Dispatch updateStyleIfNeeded so we can start the animation
                if (m_element)
                    m_compositeAnimation->animationController().addElementChangeToDispatch(*m_element);
            } else {
                // We are pausing while waiting for a start response. Cancel the animation and wait. When 
                // we unpause, we will act as though the start timer just fired
                m_pauseTime = beginAnimationUpdateTime();
                pauseAnimation(beginAnimationUpdateTime() - m_startTime.value_or(0));
                LOG(Animations, "%p AnimationState %s -> PausedWaitResponse", this, nameForState(m_animationState));
                m_animationState = AnimationState::PausedWaitResponse;
            }
            break;
        case AnimationState::Looping:
            ASSERT(input == AnimationStateInput::LoopTimerFired || input == AnimationStateInput::PlayStatePaused);

            if (input == AnimationStateInput::LoopTimerFired) {
                ASSERT(param >= 0);
                LOG(Animations, "%p AnimationState %s -> LoopTimerFired (time is %f)", this, nameForState(m_animationState), param);

                // Loop timer fired, loop again or end.
                onAnimationIteration(param);

                // Decide whether to go into looping or ending state
                goIntoEndingOrLoopingState();
            } else {
                // We are pausing while running. Cancel the animation and wait
                m_pauseTime = beginAnimationUpdateTime();
                pauseAnimation(beginAnimationUpdateTime() - m_startTime.value_or(0));
                LOG(Animations, "%p AnimationState %s -> PausedRun", this, nameForState(m_animationState));
                m_animationState = AnimationState::PausedRun;
            }
            break;
        case AnimationState::Ending:
#if !LOG_DISABLED
            if (input != AnimationStateInput::EndTimerFired && input != AnimationStateInput::PlayStatePaused)
                LOG_ERROR("State is AnimationState::Ending, but input is not AnimationStateInput::EndTimerFired or AnimationStateInput::PlayStatePaused. It is %s.", nameForStateInput(input));
#endif
            if (input == AnimationStateInput::EndTimerFired) {
                ASSERT(param >= 0);
                // End timer fired, finish up
                onAnimationEnd(param);

                LOG(Animations, "%p AnimationState %s -> Done (time is %f)", this, nameForState(m_animationState), param);
                m_animationState = AnimationState::Done;
                
                if (m_element) {
                    if (m_animation->fillsForwards()) {
                        LOG(Animations, "%p AnimationState %s -> FillingForwards", this, nameForState(m_animationState));
                        m_animationState = AnimationState::FillingForwards;
                    } else
                        resumeOverriddenAnimations();

                    // Fire off another style change so we can set the final value
                    if (m_element)
                        m_compositeAnimation->animationController().addElementChangeToDispatch(*m_element);
                }
            } else {
                // We are pausing while running. Cancel the animation and wait
                m_pauseTime = beginAnimationUpdateTime();
                pauseAnimation(beginAnimationUpdateTime() - m_startTime.value_or(0));
                LOG(Animations, "%p AnimationState %s -> PausedRun", this, nameForState(m_animationState));
                m_animationState = AnimationState::PausedRun;
            }
            // |this| may be deleted here
            break;
        case AnimationState::PausedWaitTimer:
            ASSERT(input == AnimationStateInput::PlayStateRunning);
            ASSERT(paused());
            // Update the times
            m_startTime = m_startTime.value_or(0) + beginAnimationUpdateTime() - m_pauseTime.value_or(0);
            m_pauseTime = WTF::nullopt;

            // we were waiting for the start timer to fire, go back and wait again
            LOG(Animations, "%p AnimationState %s -> New", this, nameForState(m_animationState));
            m_animationState = AnimationState::New;
            updateStateMachine(AnimationStateInput::StartAnimation, 0);
            break;
        case AnimationState::PausedNew:
        case AnimationState::PausedWaitResponse:
        case AnimationState::PausedWaitStyleAvailable:
        case AnimationState::PausedRun:
            // We treat these two cases the same. The only difference is that, when we are in
            // AnimationState::PausedWaitResponse, we don't yet have a valid startTime, so we send 0 to startAnimation.
            // When the AnimationStateInput::StartTimeSet comes in and we were in AnimationState::PausedRun, we will notice
            // that we have already set the startTime and will ignore it.
            ASSERT(input == AnimationStateInput::PlayStatePaused || input == AnimationStateInput::PlayStateRunning || input == AnimationStateInput::StartTimeSet || input == AnimationStateInput::StyleAvailable);
            ASSERT(paused());

            if (input == AnimationStateInput::PlayStateRunning) {
                if (m_animationState == AnimationState::PausedNew) {
                    // We were paused before we even started, and now we're supposed
                    // to start, so jump back to the New state and reset.
                    LOG(Animations, "%p AnimationState %s -> AnimationState::New", this, nameForState(m_animationState));
                    m_animationState = AnimationState::New;
                    m_pauseTime = WTF::nullopt;
                    updateStateMachine(input, param);
                    break;
                }

                // Update the times
                if (m_animationState == AnimationState::PausedRun)
                    m_startTime = m_startTime.value_or(0) + beginAnimationUpdateTime() - m_pauseTime.value_or(0);
                else
                    m_startTime = 0;

                m_pauseTime = WTF::nullopt;

                if (m_animationState == AnimationState::PausedWaitStyleAvailable) {
                    LOG(Animations, "%p AnimationState %s -> StartWaitStyleAvailable", this, nameForState(m_animationState));
                    m_animationState = AnimationState::StartWaitStyleAvailable;
                } else {
                    // We were either running or waiting for a begin time response from the animation.
                    // Either way we need to restart the animation (possibly with an offset if we
                    // had already been running) and wait for it to start.
                    LOG(Animations, "%p AnimationState %s -> StartWaitResponse", this, nameForState(m_animationState));
                    m_animationState = AnimationState::StartWaitResponse;

                    // Start the animation
                    if (overridden()) {
                        // We won't try to start accelerated animations if we are overridden and
                        // just move on to the next state.
                        updateStateMachine(AnimationStateInput::StartTimeSet, beginAnimationUpdateTime());
                        m_isAccelerated = true;
                    } else {
                        bool started = startAnimation(beginAnimationUpdateTime() - m_startTime.value_or(0));
                        m_compositeAnimation->animationController().addToAnimationsWaitingForStartTimeResponse(this, started);
                        m_isAccelerated = started;
                    }
                }
                break;
            }
            
            if (input == AnimationStateInput::StartTimeSet) {
                ASSERT(m_animationState == AnimationState::PausedWaitResponse);
                
                // We are paused but we got the callback that notifies us that an accelerated animation started.
                // We ignore the start time and just move into the paused-run state.
                LOG(Animations, "%p AnimationState %s -> PausedRun (time is %f)", this, nameForState(m_animationState), param);
                m_animationState = AnimationState::PausedRun;
                ASSERT(!m_startTime);
                m_startTime = param;
                m_pauseTime = m_pauseTime.value_or(0) + param;
                break;
            }

            ASSERT(m_animationState == AnimationState::PausedNew || m_animationState == AnimationState::PausedWaitStyleAvailable);

            if (input == AnimationStateInput::PlayStatePaused)
                break;

            ASSERT(input == AnimationStateInput::StyleAvailable);

            // We are paused but we got the callback that notifies us that style has been updated.
            // We move to the AnimationState::PausedWaitResponse state
            LOG(Animations, "%p AnimationState %s -> PausedWaitResponse", this, nameForState(m_animationState));
            m_animationState = AnimationState::PausedWaitResponse;
            overrideAnimations();
            break;
        case AnimationState::FillingForwards:
        case AnimationState::Done:
            // We're done. Stay in this state until we are deleted
            break;
    }
}

void AnimationBase::fireAnimationEventsIfNeeded()
{
    if (!m_compositeAnimation)
        return;

    // If we are waiting for the delay time to expire and it has, go to the next state
    if (m_animationState != AnimationState::StartWaitTimer && m_animationState != AnimationState::Looping && m_animationState != AnimationState::Ending)
        return;

    // We have to make sure to keep a ref to the this pointer, because it could get destroyed
    // during an animation callback that might get called. Since the owner is a CompositeAnimation
    // and it ref counts this object, we will keep a ref to that instead. That way the AnimationBase
    // can still access the resources of its CompositeAnimation as needed.
    Ref<AnimationBase> protectedThis(*this);
    Ref<CompositeAnimation> protectCompositeAnimation(*m_compositeAnimation);
    
    // Check for start timeout
    if (m_animationState == AnimationState::StartWaitTimer) {
        if (beginAnimationUpdateTime() - m_requestedStartTime >= m_animation->delay())
            updateStateMachine(AnimationStateInput::StartTimerFired, 0);
        return;
    }

    double elapsedDuration = beginAnimationUpdateTime() - m_startTime.value_or(0);

    // FIXME: we need to ensure that elapsedDuration is never < 0. If it is, this suggests that
    // we had a recalcStyle() outside of beginAnimationUpdate()/endAnimationUpdate().
    // Also check in getTimeToNextEvent().
    elapsedDuration = std::max(elapsedDuration, 0.0);
    
    // Check for end timeout
    if (m_totalDuration && elapsedDuration >= m_totalDuration.value()) {
        // We may still be in AnimationState::Looping if we've managed to skip a
        // whole iteration, in which case we should jump to the end state.
        LOG(Animations, "%p AnimationState %s -> Ending", this, nameForState(m_animationState));
        m_animationState = AnimationState::Ending;

        // Fire an end event
        updateStateMachine(AnimationStateInput::EndTimerFired, m_totalDuration.value());
    } else {
        // Check for iteration timeout
        if (!m_nextIterationDuration) {
            // Hasn't been set yet, set it
            double durationLeft = m_animation->duration() - fmod(elapsedDuration, m_animation->duration());
            m_nextIterationDuration = elapsedDuration + durationLeft;
        }
        
        if (elapsedDuration >= m_nextIterationDuration) {
            // Set to the next iteration
            double previous = m_nextIterationDuration.value();
            double durationLeft = m_animation->duration() - fmod(elapsedDuration, m_animation->duration());
            m_nextIterationDuration = elapsedDuration + durationLeft;
            
            // Send the event
            updateStateMachine(AnimationStateInput::LoopTimerFired, previous);
        }
    }
}

void AnimationBase::updatePlayState(AnimationPlayState playState)
{
    if (!m_compositeAnimation)
        return;

    // When we get here, we can have one of 4 desired states: running, paused, suspended, paused & suspended.
    // The state machine can be in one of two states: running, paused.
    // Set the state machine to the desired state.
    bool pause = playState == AnimationPlayState::Paused || m_compositeAnimation->isSuspended();

    if (pause == paused() && !isNew())
        return;

    updateStateMachine(pause ?  AnimationStateInput::PlayStatePaused : AnimationStateInput::PlayStateRunning, -1);
}

Optional<Seconds> AnimationBase::timeToNextService()
{
    // Returns the time at which next service is required. WTF::nullopt means no service is required. 0 means
    // service is required now, and > 0 means service is required that many seconds in the future.
    if (paused() || isNew() || postActive() || fillingForwards())
        return WTF::nullopt;
    
    if (m_animationState == AnimationState::StartWaitTimer) {
        double timeFromNow = m_animation->delay() - (beginAnimationUpdateTime() - m_requestedStartTime);
        return std::max(Seconds { timeFromNow }, 0_s);
    }
    
    fireAnimationEventsIfNeeded();
        
    // In all other cases, we need service right away.
    return 0_s;
}

// Compute the fractional time, taking into account direction.
// There is no need to worry about iterations, we assume that we would have
// short circuited above if we were done.

double AnimationBase::fractionalTime(double scale, double elapsedTime, double offset) const
{
    double fractionalTime = m_animation->duration() ? (elapsedTime / m_animation->duration()) : 1;
    // FIXME: startTime can be before the current animation "frame" time. This is to sync with the frame time
    // concept in AnimationTimeController. So we need to somehow sync the two. Until then, the possible
    // error is small and will probably not be noticeable. Until we fix this, remove the assert.
    // https://bugs.webkit.org/show_bug.cgi?id=52037
    // ASSERT(fractionalTime >= 0);
    if (fractionalTime < 0)
        fractionalTime = 0;

    int integralTime = static_cast<int>(fractionalTime);
    const int integralIterationCount = static_cast<int>(m_animation->iterationCount());
    const bool iterationCountHasFractional = m_animation->iterationCount() - integralIterationCount;
    if (m_animation->iterationCount() != Animation::IterationCountInfinite && !iterationCountHasFractional)
        integralTime = std::min(integralTime, integralIterationCount - 1);

    fractionalTime -= integralTime;

    if (((m_animation->direction() == Animation::AnimationDirectionAlternate) && (integralTime & 1))
        || ((m_animation->direction() == Animation::AnimationDirectionAlternateReverse) && !(integralTime & 1))
        || m_animation->direction() == Animation::AnimationDirectionReverse)
        fractionalTime = 1 - fractionalTime;

    if (scale != 1 || offset)
        fractionalTime = (fractionalTime - offset) * scale;

    return fractionalTime;
}

double AnimationBase::progress(double scale, double offset, const TimingFunction* timingFunction) const
{
    if (preActive())
        return 0;

    if (postActive())
        return 1;

    double elapsedTime = getElapsedTime();

    double duration = m_animation->duration();
    if (m_animation->iterationCount() > 0)
        duration *= m_animation->iterationCount();

    if (fillingForwards())
        elapsedTime = duration;

    double fractionalTime = this->fractionalTime(scale, elapsedTime, offset);

    if (m_animation->iterationCount() > 0 && elapsedTime >= duration) {
        if (WTF::isIntegral(fractionalTime))
            return fractionalTime;
    }

    if (!timingFunction)
        timingFunction = m_animation->timingFunction();

    return timingFunction->transformTime(fractionalTime, m_animation->duration());
}

void AnimationBase::getTimeToNextEvent(Seconds& time, bool& isLooping) const
{
    // Decide when the end or loop event needs to fire
    const double elapsedDuration = std::max(beginAnimationUpdateTime() - m_startTime.value_or(0), 0.0);
    double durationLeft = 0;
    double nextIterationTime = m_totalDuration.value_or(0);

    if (!m_totalDuration || elapsedDuration < m_totalDuration.value()) {
        durationLeft = m_animation->duration() > 0 ? (m_animation->duration() - fmod(elapsedDuration, m_animation->duration())) : 0;
        nextIterationTime = elapsedDuration + durationLeft;
    }
    
    if (!m_totalDuration || nextIterationTime < m_totalDuration.value()) {
        // We are not at the end yet
        ASSERT(nextIterationTime > 0);
        isLooping = true;
    } else {
        // We are at the end
        isLooping = false;
    }
    
    time = Seconds { durationLeft };
}

void AnimationBase::goIntoEndingOrLoopingState()
{
    Seconds t;
    bool isLooping;
    getTimeToNextEvent(t, isLooping);
    LOG(Animations, "%p AnimationState %s -> %s", this, nameForState(m_animationState), isLooping ? "Looping" : "Ending");
    m_animationState = isLooping ? AnimationState::Looping : AnimationState::Ending;
}
  
void AnimationBase::freezeAtTime(double t)
{
    if (!m_compositeAnimation)
        return;

    if (!m_startTime) {
        // If we haven't started yet, make it as if we started.
        LOG(Animations, "%p AnimationState %s -> StartWaitResponse", this, nameForState(m_animationState));
        m_animationState = AnimationState::StartWaitResponse;
        onAnimationStartResponse(MonotonicTime::now());
    }

    ASSERT(m_startTime); // If m_startTime is zero, we haven't started yet, so we'll get a bad pause time.
    if (t <= m_animation->delay())
        m_pauseTime = m_startTime.value_or(0);
    else
        m_pauseTime = m_startTime.value_or(0) + t - m_animation->delay();

    if (auto* renderer = compositedRenderer())
        renderer->suspendAnimations(MonotonicTime::fromRawSeconds(m_pauseTime.value()));
}

double AnimationBase::beginAnimationUpdateTime() const
{
    if (!m_compositeAnimation)
        return 0;

    return m_compositeAnimation->animationController().beginAnimationUpdateTime().secondsSinceEpoch().seconds();
}

double AnimationBase::getElapsedTime() const
{
    if (paused()) {
        double delayOffset = (!m_startTime && m_animation->delay() < 0) ? m_animation->delay() : 0;
        return m_pauseTime.value_or(0) - m_startTime.value_or(0) - delayOffset;
    }

    if (!m_startTime)
        return 0;

    if (postActive() || fillingForwards())
        return m_totalDuration.value_or(0);

    return beginAnimationUpdateTime() - m_startTime.value_or(0);
}

void AnimationBase::setElapsedTime(double time)
{
    // FIXME: implement this method
    UNUSED_PARAM(time);
}

void AnimationBase::play()
{
    // FIXME: implement this method
}

void AnimationBase::pause()
{
    // FIXME: implement this method
}

static bool containsRotation(const Vector<RefPtr<TransformOperation>>& operations)
{
    for (const auto& operation : operations) {
        if (operation->type() == TransformOperation::ROTATE)
            return true;
    }
    return false;
}

bool AnimationBase::computeTransformedExtentViaTransformList(const FloatRect& rendererBox, const RenderStyle& style, LayoutRect& bounds) const
{
    FloatRect floatBounds = bounds;
    FloatPoint transformOrigin;
    
    bool applyTransformOrigin = containsRotation(style.transform().operations()) || style.transform().affectedByTransformOrigin();
    if (applyTransformOrigin) {
        transformOrigin.setX(rendererBox.x() + floatValueForLength(style.transformOriginX(), rendererBox.width()));
        transformOrigin.setY(rendererBox.y() + floatValueForLength(style.transformOriginY(), rendererBox.height()));
        // Ignore transformOriginZ because we'll bail if we encounter any 3D transforms.

        floatBounds.moveBy(-transformOrigin);
    }

    for (const auto& operation : style.transform().operations()) {
        if (operation->type() == TransformOperation::ROTATE) {
            // For now, just treat this as a full rotation. This could take angle into account to reduce inflation.
            floatBounds = boundsOfRotatingRect(floatBounds);
        } else {
            TransformationMatrix transform;
            operation->apply(transform, rendererBox.size());
            if (!transform.isAffine())
                return false;

            if (operation->type() == TransformOperation::MATRIX || operation->type() == TransformOperation::MATRIX_3D) {
                TransformationMatrix::Decomposed2Type toDecomp;
                transform.decompose2(toDecomp);
                // Any rotation prevents us from using a simple start/end rect union.
                if (toDecomp.angle)
                    return false;
            }

            floatBounds = transform.mapRect(floatBounds);
        }
    }

    if (applyTransformOrigin)
        floatBounds.moveBy(transformOrigin);

    bounds = LayoutRect(floatBounds);
    return true;
}

bool AnimationBase::computeTransformedExtentViaMatrix(const FloatRect& rendererBox, const RenderStyle& style, LayoutRect& bounds) const
{
    TransformationMatrix transform;
    style.applyTransform(transform, rendererBox, RenderStyle::IncludeTransformOrigin);
    if (!transform.isAffine())
        return false;

    TransformationMatrix::Decomposed2Type fromDecomp;
    transform.decompose2(fromDecomp);
    // Any rotation prevents us from using a simple start/end rect union.
    if (fromDecomp.angle)
        return false;

    bounds = LayoutRect(transform.mapRect(bounds));
    return true;

}

} // namespace WebCore
