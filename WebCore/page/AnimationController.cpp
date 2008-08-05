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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "AnimationController.h"

#include "CSSPropertyNames.h"
#include "CString.h"
#include "Document.h"
#include "EventNames.h"
#include "FloatConversion.h"
#include "Frame.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "SystemTime.h"
#include "Timer.h"
#include "UnitBezier.h"

namespace WebCore {

static const double cAnimationTimerDelay = 0.025;

static void setChanged(Node* node)
{
    ASSERT(!node || (node->document() && !node->document()->inPageCache()));
    node->setChanged(AnimationStyleChange);
}

// The epsilon value we pass to UnitBezier::solve given that the animation is going to run over |dur| seconds. The longer the
// animation, the more precision we need in the timing function result to avoid ugly discontinuities.
static inline double solveEpsilon(double duration) { return 1. / (200. * duration); }

static inline double solveCubicBezierFunction(double p1x, double p1y, double p2x, double p2y, double t, double duration)
{
    // Convert from input time to parametric value in curve, then from
    // that to output time.
    UnitBezier bezier(p1x, p1y, p2x, p2y);
    return bezier.solve(t, solveEpsilon(duration));
}

class CompositeAnimation;
class AnimationBase;

class AnimationTimerBase {
public:
    AnimationTimerBase(AnimationBase* anim)
    : m_timer(this, &AnimationTimerBase::timerFired)
    , m_anim(anim)
    {
        m_timer.startOneShot(0);
    }
    virtual ~AnimationTimerBase()
    {
    }
    
    void startTimer(double timeout=0)
    {
        m_timer.startOneShot(timeout);
    }
    
    void cancelTimer()
    {
        m_timer.stop();
    }
    
    virtual void timerFired(Timer<AnimationTimerBase>*) = 0;
    
private:
    Timer<AnimationTimerBase> m_timer;
    
protected:
    AnimationBase* m_anim;
};

class AnimationEventDispatcher : public AnimationTimerBase {
public:
    AnimationEventDispatcher(AnimationBase* anim) 
    : AnimationTimerBase(anim)
    , m_property(CSSPropertyInvalid)
    , m_reset(false)
    , m_elapsedTime(-1)
    {
    }
    
    virtual ~AnimationEventDispatcher()
    {
    }
    
    void startTimer(Element* element, const AtomicString& name, int property, bool reset,
                    const AtomicString& eventType, double elapsedTime)
    {
        m_element = element;
        m_name = name;
        m_property = property;
        m_reset = reset;
        m_eventType = eventType;
        m_elapsedTime = elapsedTime;
        AnimationTimerBase::startTimer();
    }
    
    virtual void timerFired(Timer<AnimationTimerBase>*);
    
private:
    RefPtr<Element> m_element;
    AtomicString m_name;
    int m_property;
    bool m_reset;
    AtomicString m_eventType;
    double m_elapsedTime;
};

class AnimationTimerCallback : public AnimationTimerBase {
public:
    AnimationTimerCallback(AnimationBase* anim) 
    : AnimationTimerBase(anim)
    , m_elapsedTime(0)
    { }
    virtual ~AnimationTimerCallback() { }
    
    virtual void timerFired(Timer<AnimationTimerBase>*);
    
    void startTimer(double timeout, const AtomicString& eventType, double elapsedTime)
    {
        m_eventType = eventType;
        m_elapsedTime = elapsedTime;
        AnimationTimerBase::startTimer(timeout);
    }
    
private:
    AtomicString m_eventType;
    double m_elapsedTime;
};

class ImplicitAnimation;
class AnimationControllerPrivate;

// A CompositeAnimation represents a collection of animations that
// are running, such as a number of properties transitioning at once.

class CompositeAnimation : public Noncopyable {
public:
    CompositeAnimation(AnimationControllerPrivate* animationController)
    : m_suspended(false)
    , m_animationController(animationController)
    , m_numStyleAvailableWaiters(0)
    { }
    
    ~CompositeAnimation()
    {
        deleteAllValues(m_transitions);
    }
    
    RenderStyle* animate(RenderObject*, RenderStyle* currentStyle, RenderStyle* targetStyle);
    
    void setAnimating(bool inAnimating);
    bool animating();
    
    bool hasAnimationForProperty(int prop) const { return m_transitions.contains(prop); }
    
    void setTransitionStartTime(int property, double t);
    
    void resetTransitions(RenderObject*);
    void resetAnimations(RenderObject*);
    
    void cleanupFinishedAnimations(RenderObject*);
    
    void suspendAnimations();
    void resumeAnimations();
    bool suspended() const { return m_suspended; }
    
    void overrideImplicitAnimations(int property);
    void resumeOverriddenImplicitAnimations(int property);
    
    void styleAvailable();
    
    bool isAnimatingProperty(int property) const;
    
    void setWaitingForStyleAvailable(bool waiting);
protected:
    void updateTransitions(RenderObject* renderer, RenderStyle* currentStyle, RenderStyle* targetStyle);
    
private:
    typedef HashMap<int, ImplicitAnimation*> CSSPropertyTransitionsMap;
    
    CSSPropertyTransitionsMap   m_transitions;
    bool                        m_suspended;
    AnimationControllerPrivate* m_animationController;
    uint32_t                    m_numStyleAvailableWaiters;
};

class AnimationBase : public Noncopyable {
public:
    AnimationBase(const Animation* transition, RenderObject* renderer, CompositeAnimation* compAnim);
    virtual ~AnimationBase()
    {
        if (m_animState == STATE_START_WAIT_STYLE_AVAILABLE)
            m_compAnim->setWaitingForStyleAvailable(false);
    }
    
    RenderObject* renderer() const { return m_object; }
    double startTime() const { return m_startTime; }
    double duration() const  { return m_animation->duration(); }
    
    void cancelTimers()
    {
        m_animationTimerCallback.cancelTimer();
        m_animationEventDispatcher.cancelTimer();
    }
    
    // Animations and Transitions go through the states below. When entering the STARTED state
    // the animation is started. This may or may not require deferred response from the animator.
    // If so, we stay in this state until that response is received (and it returns the start time).
    // Otherwise, we use the current time as the start time and go immediately to the LOOPING or
    // ENDING state.
    enum AnimState { 
        STATE_NEW,                      // animation just created, animation not running yet
        STATE_START_WAIT_TIMER,         // start timer running, waiting for fire
        STATE_START_WAIT_STYLE_AVAILABLE,   // waiting for style setup so we can start animations
        STATE_START_WAIT_RESPONSE,      // animation started, waiting for response
        STATE_LOOPING,                  // response received, animation running, loop timer running, waiting for fire
        STATE_ENDING,                   // response received, animation running, end timer running, waiting for fire
        STATE_PAUSED_WAIT_TIMER,        // animation in pause mode when animation started
        STATE_PAUSED_WAIT_RESPONSE,     // animation paused when in STARTING state
        STATE_PAUSED_RUN,               // animation paused when in LOOPING or ENDING state
        STATE_DONE                      // end timer fired, animation finished and removed
    };
    
    enum AnimStateInput {
        STATE_INPUT_MAKE_NEW,           // reset back to new from any state
        STATE_INPUT_START_ANIMATION,    // animation requests a start
        STATE_INPUT_RESTART_ANIMATION,  // force a restart from any state
        STATE_INPUT_START_TIMER_FIRED,  // start timer fired
        STATE_INPUT_STYLE_AVAILABLE,        // style is setup, ready to start animating
        STATE_INPUT_START_TIME_SET,     // m_startTime was set
        STATE_INPUT_LOOP_TIMER_FIRED,   // loop timer fired
        STATE_INPUT_END_TIMER_FIRED,    // end timer fired
        STATE_INPUT_PAUSE_OVERRIDE,     // pause an animation due to override
        STATE_INPUT_RESUME_OVERRIDE,    // resume an overridden animation
        STATE_INPUT_PLAY_STATE_RUNNING, // play state paused -> running
        STATE_INPUT_PLAY_STATE_PAUSED,  // play state running -> paused
        STATE_INPUT_END_ANIMATION       // force an end from any state
    };
    
    // Called when animation is in NEW state to start animation
    void updateStateMachine(AnimStateInput input, double param);
    
    // Animation has actually started, at passed time
    // This is a callback and is only received when RenderObject::startAnimation() or RenderObject::startTransition() 
    // returns true. If RenderObject::
    void onAnimationStartResponse(double startTime);
    
    // Called to change to or from paused state
    void updatePlayState(bool running);
    bool playStatePlaying() const { return m_animation; }
    
    bool waitingToStart() const { return m_animState == STATE_NEW || m_animState == STATE_START_WAIT_TIMER; }
    bool preactive() const
    { return m_animState == STATE_NEW ||
             m_animState == STATE_START_WAIT_TIMER ||
             m_animState == STATE_START_WAIT_STYLE_AVAILABLE ||
             m_animState == STATE_START_WAIT_RESPONSE;
    }
    bool postactive() const { return m_animState == STATE_DONE; }
    bool active() const { return !postactive() && !preactive(); }
    bool running() const { return !isnew() && !postactive(); }
    bool paused() const { return m_pauseTime >= 0; }
    bool isnew() const { return m_animState == STATE_NEW; }
    bool waitingForStartTime() const { return m_animState == STATE_START_WAIT_RESPONSE; }
    bool waitingForStyleAvailable() const { return m_animState == STATE_START_WAIT_STYLE_AVAILABLE; }
    bool waitingForEndEvent() const  { return m_waitingForEndEvent; }
    
    // "animating" means that something is running that requires a timer to keep firing
    // (e.g. a software animation)
    void setAnimating(bool inAnimating = true) { m_animating = inAnimating; }
    bool animating() const { return m_animating; }
    
    double progress(double scale, double offset) const;
    
    virtual void animate(CompositeAnimation*, RenderObject*, const RenderStyle* currentStyle, 
                         const RenderStyle* targetStyle, RenderStyle*& ioAnimatedStyle) { }
    virtual void reset(RenderObject* renderer, const RenderStyle* from = 0, const RenderStyle* to = 0) { }
    
    virtual bool shouldFireEvents() const { return false; }
    
    void animationTimerCallbackFired(const AtomicString& eventType, double elapsedTime);
    void animationEventDispatcherFired(Element* element, const AtomicString& name, int property, bool reset,
                                       const AtomicString& eventType, double elapsedTime);
    
    bool animationsMatch(const Animation* anim) const { return m_animation->animationsMatch(anim); }
    
    void setAnimation(const Animation* anim) { m_animation = const_cast<Animation*>(anim); }
    
    // Return true if this animation is overridden. This will only be the case for
    // ImplicitAnimations and is used to determine whether or not we should force
    // set the start time. If an animation is overridden, it will probably not get
    // back the START_TIME event.
    virtual bool overridden() const { return false; }
    
    // Does this animation/transition involve the given property?
    virtual bool affectsProperty(int property) const { return false; }
    bool isAnimatingProperty(int property) const
    {
        return (!waitingToStart() && !postactive()) && affectsProperty(property);
    }
        
protected:
    Element* elementForEventDispatch()
    {
        if (m_object->node() && m_object->node()->isElementNode())
            return static_cast<Element*>(m_object->node());
        return 0;
    }
    
    virtual void overrideAnimations() { }
    virtual void resumeOverriddenAnimations() { }
    
    CompositeAnimation* compositeAnimation() { return m_compAnim; }
    
    // These are called when the corresponding timer fires so subclasses can do any extra work
    virtual void onAnimationStart(double elapsedTime) { }
    virtual void onAnimationIteration(double elapsedTime) { }
    virtual void onAnimationEnd(double elapsedTime) { }
    virtual bool startAnimation(double beginTime) { return false; }
    virtual void endAnimation(bool reset) { }
    
    void primeEventTimers();
    
protected:
    AnimState m_animState;
    int m_iteration;
    
    bool m_animating;       // transition/animation requires continual timer firing
    bool m_waitedForResponse;
    double m_startTime;
    double m_pauseTime;
    RenderObject*   m_object;
    
    AnimationTimerCallback m_animationTimerCallback;
    AnimationEventDispatcher m_animationEventDispatcher;
    RefPtr<Animation> m_animation;
    CompositeAnimation* m_compAnim;
    bool m_waitingForEndEvent;
};

class ImplicitAnimation : public AnimationBase {
public:
    ImplicitAnimation(const Animation* transition, RenderObject* renderer, CompositeAnimation* compAnim)
    : AnimationBase(transition, renderer, compAnim)
    , m_property(transition->property())
    , m_overridden(false)
    , m_fromStyle(0)
    , m_toStyle(0)
    {
    }
    
    virtual ~ImplicitAnimation()
    {
        ASSERT(!m_fromStyle && !m_toStyle);
        
        // If we were waiting for an end event, we need to finish the animation to make sure no old
        // animations stick around in the lower levels
        if (waitingForEndEvent() && m_object)
            ASSERT(0);
        
        // Do the cleanup here instead of in the base class so the specialized methods get called
        if (!postactive())
            updateStateMachine(STATE_INPUT_END_ANIMATION, -1);     
    }
    
    int property() const { return m_property; }
    
    virtual void onAnimationEnd(double inElapsedTime);
    virtual bool startAnimation(double beginTime);
    virtual void endAnimation(bool reset);
    
    virtual void animate(CompositeAnimation*, RenderObject*, const RenderStyle* currentStyle, 
                         const RenderStyle* targetStyle, RenderStyle*& ioAnimatedStyle);
    virtual void reset(RenderObject* renderer, const RenderStyle* from = 0, const RenderStyle* to = 0);
    
    void setOverridden(bool b);
    virtual bool overridden() const { return m_overridden; }
    
    virtual bool shouldFireEvents() const { return true; }
    
    virtual bool affectsProperty(int property) const;
    
    bool hasStyle() const { return m_fromStyle && m_toStyle; }
    
    bool isTargetPropertyEqual(int prop, RenderStyle* targetStyle);

    void blendPropertyValueInStyle(int prop, RenderStyle* currentStyle);
    
protected:
    bool shouldSendEventForListener(Document::ListenerType inListenerType)
    {
        return m_object->document()->hasListenerType(inListenerType);
    }
    
    bool sendTransitionEvent(const AtomicString& inEventType, double inElapsedTime);
    
private:
    int m_property;
    bool m_overridden;
    
    // The two styles that we are blending.
    RenderStyle* m_fromStyle;
    RenderStyle* m_toStyle;
};

void AnimationTimerCallback::timerFired(Timer<AnimationTimerBase>*)
{
    m_anim->animationTimerCallbackFired(m_eventType, m_elapsedTime);
}

void AnimationEventDispatcher::timerFired(Timer<AnimationTimerBase>*)
{
    m_anim->animationEventDispatcherFired(m_element.get(), m_name, m_property, m_reset, m_eventType, m_elapsedTime);
}
    
AnimationBase::AnimationBase(const Animation* transition, RenderObject* renderer, CompositeAnimation* compAnim)
: m_animState(STATE_NEW)
, m_iteration(0)
, m_animating(false)
, m_waitedForResponse(false)
, m_startTime(0)
, m_pauseTime(-1)
, m_object(renderer)
, m_animationTimerCallback(const_cast<AnimationBase*>(this))
, m_animationEventDispatcher(const_cast<AnimationBase*>(this))
, m_animation(const_cast<Animation*>(transition))
, m_compAnim(compAnim)
, m_waitingForEndEvent(false)
{
}

void AnimationBase::updateStateMachine(AnimStateInput input, double param)
{
    // if we get a RESTART then we force a new animation, regardless of state
    if (input == STATE_INPUT_MAKE_NEW) {
        if (m_animState == STATE_START_WAIT_STYLE_AVAILABLE)
            m_compAnim->setWaitingForStyleAvailable(false);
        m_animState = STATE_NEW;
        m_startTime = 0;
        m_pauseTime = -1;
        m_waitedForResponse = false;
        endAnimation(false);
        return;
    }
    else if (input == STATE_INPUT_RESTART_ANIMATION) {
        cancelTimers();
        if (m_animState == STATE_START_WAIT_STYLE_AVAILABLE)
            m_compAnim->setWaitingForStyleAvailable(false);
        m_animState = STATE_NEW;
        m_startTime = 0;
        m_pauseTime = -1;
        endAnimation(false);
        
        if (!paused())
            updateStateMachine(STATE_INPUT_START_ANIMATION, -1);
        return;
    }
    else if (input == STATE_INPUT_END_ANIMATION) {
        cancelTimers();
        if (m_animState == STATE_START_WAIT_STYLE_AVAILABLE)
            m_compAnim->setWaitingForStyleAvailable(false);
        m_animState = STATE_DONE;
        endAnimation(true);
        return;
    }
    else if (input == STATE_INPUT_PAUSE_OVERRIDE) {
        if (m_animState == STATE_START_WAIT_RESPONSE) {
            // If we are in the WAIT_RESPONSE state, the animation will get canceled before 
            // we get a response, so move to the next state
            endAnimation(false);
            updateStateMachine(STATE_INPUT_START_TIME_SET, currentTime());
        }
        return;
    }
    else if (input == STATE_INPUT_RESUME_OVERRIDE) {
        if (m_animState == STATE_LOOPING || m_animState == STATE_ENDING) {
            // Start the animation
            startAnimation(m_startTime);
        }
        return;
    }
    
    // execute state machine
    switch(m_animState) {
        case STATE_NEW:
            ASSERT(input == STATE_INPUT_START_ANIMATION || input == STATE_INPUT_PLAY_STATE_RUNNING || input == STATE_INPUT_PLAY_STATE_PAUSED);
            if (input == STATE_INPUT_START_ANIMATION || input == STATE_INPUT_PLAY_STATE_RUNNING) {
                // Set the start timer to the initial delay (0 if no delay)
                m_waitedForResponse = false;
                m_animState = STATE_START_WAIT_TIMER;
                m_animationTimerCallback.startTimer(m_animation->delay(), EventNames::webkitAnimationStartEvent, m_animation->delay());
            }
            break;
        case STATE_START_WAIT_TIMER:
            ASSERT(input == STATE_INPUT_START_TIMER_FIRED || input == STATE_INPUT_PLAY_STATE_PAUSED);
            
            if (input == STATE_INPUT_START_TIMER_FIRED) {
                ASSERT(param >= 0);
                // Start timer has fired, tell the animation to start and wait for it to respond with start time
                m_animState = STATE_START_WAIT_STYLE_AVAILABLE;
                m_compAnim->setWaitingForStyleAvailable(true);
                
                // Trigger a render so we can start the animation
                setChanged(m_object->element());
                m_object->animation()->startUpdateRenderingDispatcher();
            }
            else {
                ASSERT(running());
                // We're waiting for the start timer to fire and we got a pause. Cancel the timer, pause and wait
                m_pauseTime = currentTime();
                cancelTimers();
                m_animState = STATE_PAUSED_WAIT_TIMER;
            }
            break;
        case STATE_START_WAIT_STYLE_AVAILABLE:
            ASSERT(input == STATE_INPUT_STYLE_AVAILABLE || input == STATE_INPUT_PLAY_STATE_PAUSED);
            
            m_compAnim->setWaitingForStyleAvailable(false);
            
            if (input == STATE_INPUT_STYLE_AVAILABLE) {
                // Start timer has fired, tell the animation to start and wait for it to respond with start time
                m_animState = STATE_START_WAIT_RESPONSE;
                
                overrideAnimations();
                
                // Send start event, if needed
                onAnimationStart(0.0f); // the elapsedTime is always 0 here
                
                // Start the animation
                if (overridden() || !startAnimation(0)) {
                    // We're not going to get a startTime callback, so fire the start time here
                    m_animState = STATE_START_WAIT_RESPONSE;
                    updateStateMachine(STATE_INPUT_START_TIME_SET, currentTime());
                }
                else
                    m_waitedForResponse = true;
            }
            else {
                ASSERT(running());
                // We're waiting for the a notification that the style has been setup. If we're asked to wait
                // at this point, the style must have been processed, so we can deal with this like we would
                // for WAIT_RESPONSE, except that we don't need to do an endAnimation().
                m_pauseTime = 0;
                m_animState = STATE_START_WAIT_RESPONSE;
            }
            break;
        case STATE_START_WAIT_RESPONSE:
            ASSERT(input == STATE_INPUT_START_TIME_SET || input == STATE_INPUT_PLAY_STATE_PAUSED);
            
            if (input == STATE_INPUT_START_TIME_SET) {
                ASSERT(param >= 0);
                // We have a start time, set it, unless the startTime is already set
                if (m_startTime <= 0)
                    m_startTime = param;
                
                // Decide when the end or loop event needs to fire
                primeEventTimers();
                
                // Trigger a render so we can start the animation
                setChanged(m_object->element());
                m_object->animation()->startUpdateRenderingDispatcher();
            }
            else {
                // We are pausing while waiting for a start response. Cancel the animation and wait. When 
                // we unpause, we will act as though the start timer just fired
                m_pauseTime = 0;
                endAnimation(false);
                m_animState = STATE_PAUSED_WAIT_RESPONSE;
            }
            break;
        case STATE_LOOPING:
            ASSERT(input == STATE_INPUT_LOOP_TIMER_FIRED || input == STATE_INPUT_PLAY_STATE_PAUSED);
            
            if (input == STATE_INPUT_LOOP_TIMER_FIRED) {
                ASSERT(param >= 0);
                // Loop timer fired, loop again or end.
                onAnimationIteration(param);
                primeEventTimers();
            }
            else {
                // We are pausing while running. Cancel the animation and wait
                m_pauseTime = currentTime();
                cancelTimers();
                endAnimation(false);
                m_animState = STATE_PAUSED_RUN;
            }
            break;
        case STATE_ENDING:
            ASSERT(input == STATE_INPUT_END_TIMER_FIRED || input == STATE_INPUT_PLAY_STATE_PAUSED);
            
            if (input == STATE_INPUT_END_TIMER_FIRED) {
                ASSERT(param >= 0);
                // End timer fired, finish up
                onAnimationEnd(param);
                
                resumeOverriddenAnimations();
                
                // Fire off another style change so we can set the final value
                setChanged(m_object->element());
                m_animState = STATE_DONE;
                m_object->animation()->startUpdateRenderingDispatcher();
                // |this| may be deleted here when we've been called from timerFired()
            }
            else {
                // We are pausing while running. Cancel the animation and wait
                m_pauseTime = currentTime();
                cancelTimers();
                endAnimation(false);
                m_animState = STATE_PAUSED_RUN;
            }
            // |this| may be deleted here
            break;
        case STATE_PAUSED_WAIT_TIMER:
            ASSERT(input == STATE_INPUT_PLAY_STATE_RUNNING);
            ASSERT(!running());
            // Update the times
            m_startTime += currentTime() - m_pauseTime;
            m_pauseTime = -1;
            
            // we were waiting for the start timer to fire, go back and wait again
            m_animState = STATE_NEW;
            updateStateMachine(STATE_INPUT_START_ANIMATION, 0);
            break;
        case STATE_PAUSED_WAIT_RESPONSE:
        case STATE_PAUSED_RUN:
            // We treat these two cases the same. The only difference is that, when we are in the WAIT_RESPONSE
            // state, we don't yet have a valid startTime, so we send 0 to startAnimation. When the START_TIME
            // event comes in qnd we were in the RUN state, we will notice that we have already set the 
            // startTime and will ignore it.
            ASSERT(input == STATE_INPUT_PLAY_STATE_RUNNING);
            ASSERT(!running());
            // Update the times
            if (m_animState == STATE_PAUSED_RUN)
                m_startTime += currentTime() - m_pauseTime;
            else
                m_startTime = 0;
            m_pauseTime = -1;
            
            // We were waiting for a begin time response from the animation, go back and wait again
            m_animState = STATE_START_WAIT_RESPONSE;
            
            // Start the animation
            if (overridden() || !startAnimation(m_startTime)) {
                // We're not going to get a startTime callback, so fire the start time here
                updateStateMachine(STATE_INPUT_START_TIME_SET, currentTime());
            }
            else
                m_waitedForResponse = true;
            break;
        case STATE_DONE:
            // We're done. Stay in this state until we are deleted
            break;
    }
    // |this| may be deleted here if we came out of STATE_ENDING when we've been called from timerFired()
}
    
void AnimationBase::animationTimerCallbackFired(const AtomicString& eventType, double elapsedTime)
{
    ASSERT(m_object->document() && !m_object->document()->inPageCache());
    
    // FIXME: use an enum
    if (eventType == EventNames::webkitAnimationStartEvent)
        updateStateMachine(STATE_INPUT_START_TIMER_FIRED, elapsedTime);
    else if (eventType == EventNames::webkitAnimationIterationEvent)
        updateStateMachine(STATE_INPUT_LOOP_TIMER_FIRED, elapsedTime);
    else if (eventType == EventNames::webkitAnimationEndEvent) {
        updateStateMachine(STATE_INPUT_END_TIMER_FIRED, elapsedTime);
        // |this| may be deleted here
    }
}

void AnimationBase::animationEventDispatcherFired(Element* element, const AtomicString& name, int property,
                                                  bool reset, const AtomicString& eventType, double elapsedTime)
{
    m_waitingForEndEvent = false;
    
    // Keep an atomic string on the stack to keep it alive until we exit this method
    // (since dispatching the event may cause |this| to be deleted, therefore removing
    // the last ref to the atomic string).
    AtomicString animName(name);
    AtomicString animEventType(eventType);
    // Make sure the element sticks around too
    RefPtr<Element> elementRetainer(element);
    
    ASSERT(!element || (element->document() && !element->document()->inPageCache()));
    if (!element)
        return;
}

void AnimationBase::updatePlayState(bool run)
{
    if (paused() == run || isnew())
        updateStateMachine(run ? STATE_INPUT_PLAY_STATE_RUNNING : STATE_INPUT_PLAY_STATE_PAUSED, -1);
}

double AnimationBase::progress(double scale, double offset) const
{
    if (preactive())
        return 0;
    
    double elapsedTime = running() ? (currentTime() - m_startTime) : (m_pauseTime - m_startTime);
    if (running() && elapsedTime < 0)
        return 0;
    
    double dur = m_animation->duration();
    
    if (postactive() || !m_animation->duration() || elapsedTime >= dur)
        return 1.0;
    
    // Compute the fractional time, taking into account direction.
    // There is no need to worry about iterations, we assume that we would have 
    // short circuited above if we were done
    double t = elapsedTime / m_animation->duration();
    int i = (int) t;
    t -= i;
    
    if (scale != 1 || offset != 0)
        t = (t - offset) * scale;
    
    if (m_animation->timingFunction().type() == LinearTimingFunction)
        return t;
    
    // Cubic bezier.
    double tt = solveCubicBezierFunction(m_animation->timingFunction().x1(), 
                                         m_animation->timingFunction().y1(), 
                                         m_animation->timingFunction().x2(), 
                                         m_animation->timingFunction().y2(),
                                         t, m_animation->duration());
    return tt;
}

void AnimationBase::primeEventTimers()
{
    // Decide when the end or loop event needs to fire
    double ct = currentTime();
    const double elapsedDuration = ct - m_startTime;
    ASSERT(elapsedDuration >= 0);
    
    double totalDuration = m_animation->duration();
    double durationLeft = 0;
    double nextIterationTime = totalDuration;
    if (totalDuration < 0 || elapsedDuration < totalDuration) {
        durationLeft = m_animation->duration() - fmod(elapsedDuration, m_animation->duration());
        nextIterationTime = elapsedDuration + durationLeft;
    }
    
    // At this point, we may have 0 durationLeft, if we've gotten the event late and we are already
    // past totalDuration. In this case we still fire an end timer before processing the end. 
    // This defers the call to sendAnimationEvents to avoid re-entrant calls that destroy
    // the RenderObject, and therefore |this| before we're done with it.
    if (totalDuration < 0 || nextIterationTime < totalDuration) {
        // We are not at the end yet, send a loop event
        ASSERT(nextIterationTime > 0);
        m_animState = STATE_LOOPING;
        m_animationTimerCallback.startTimer(durationLeft, EventNames::webkitAnimationIterationEvent, nextIterationTime);
    }
    else {
        // We are at the end, send an end event
        m_animState = STATE_ENDING;
        m_animationTimerCallback.startTimer(durationLeft, EventNames::webkitAnimationEndEvent, nextIterationTime);
    }
}

static inline int blendFunc(int from, int to, double progress)
{  
    return int(from + (to - from) * progress);
}

static inline double blendFunc(double from, double to, double progress)
{  
    return from + (to - from) * progress;
}

static inline float blendFunc(float from, float to, double progress)
{  
    return narrowPrecisionToFloat(from + (to - from) * progress);
}

static inline Color blendFunc(const Color& from, const Color& to, double progress)
{  
    return Color(blendFunc(from.red(), to.red(), progress),
                 blendFunc(from.green(), to.green(), progress),
                 blendFunc(from.blue(), to.blue(), progress),
                 blendFunc(from.alpha(), to.alpha(), progress));
}

static inline Length blendFunc(const Length& from, const Length& to, double progress)
{  
    return to.blend(from, progress);
}

static inline IntSize blendFunc(const IntSize& from, const IntSize& to, double progress)
{  
    return IntSize(blendFunc(from.width(), to.width(), progress),
                   blendFunc(from.height(), to.height(), progress));
}

static inline ShadowData* blendFunc(const ShadowData* from, const ShadowData* to, double progress)
{  
    ASSERT(from && to);
    return new ShadowData(blendFunc(from->x, to->x, progress), blendFunc(from->y, to->y, progress), 
                          blendFunc(from->blur, to->blur, progress), blendFunc(from->color, to->color, progress));
}

static inline TransformOperations blendFunc(const TransformOperations& from, const TransformOperations& to, double progress)
{    
    // Blend any operations whose types actually match up.  Otherwise don't bother.
    unsigned fromSize = from.size();
    unsigned toSize = to.size();
    unsigned size = max(fromSize, toSize);
    TransformOperations result;
    for (unsigned i = 0; i < size; i++) {
        TransformOperation* fromOp = i < fromSize ? from[i].get() : 0;
        TransformOperation* toOp = i < toSize ? to[i].get() : 0;
        TransformOperation* blendedOp = toOp ? toOp->blend(fromOp, progress) : fromOp->blend(0, progress, true);
        if (blendedOp)
            result.append(blendedOp);
    }
    return result;
}

static inline EVisibility blendFunc(EVisibility from, EVisibility to, double progress)
{
    // Any non-zero result means we consider the object to be visible.  Only at 0 do we consider the object to be
    // invisible.   The invisible value we use (HIDDEN vs. COLLAPSE) depends on the specified from/to values.
    double fromVal = from == VISIBLE ? 1. : 0.;
    double toVal = to == VISIBLE ? 1. : 0.;
    if (fromVal == toVal)
        return to;
    double result = blendFunc(fromVal, toVal, progress);
    return result > 0. ? VISIBLE : (to != VISIBLE ? to : from);
}

class PropertyWrapperBase {
public:
    PropertyWrapperBase(int prop)
    : m_prop(prop)
    { }
    virtual ~PropertyWrapperBase() { }
    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const=0;
    virtual void blend(RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double prog) const=0;
    
    int property() const { return m_prop; }
    
private:
    int m_prop;
};

template <typename T>
class PropertyWrapperGetter : public PropertyWrapperBase {
public:
    PropertyWrapperGetter(int prop, T (RenderStyle::*getter)() const)
    : PropertyWrapperBase(prop)
    , m_getter(getter)
    { }
    
    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const
    {
        return (a->*m_getter)() == (b->*m_getter)();
    }
    
protected:
    T (RenderStyle::*m_getter)() const;
};

template <typename T>
class PropertyWrapper : public PropertyWrapperGetter<T> {
public:
    PropertyWrapper(int prop, T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T))
    : PropertyWrapperGetter<T>(prop, getter)
    , m_setter(setter)
    { }
    
    virtual void blend(RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double prog) const
    {
        (dst->*m_setter)(blendFunc((a->*PropertyWrapperGetter<T>::m_getter)(), (b->*PropertyWrapperGetter<T>::m_getter)(), prog));
    }
    
protected:
    void (RenderStyle::*m_setter)(T);
};

class PropertyWrapperShadow : public PropertyWrapperGetter<ShadowData*> {
public:
    PropertyWrapperShadow(int prop, ShadowData* (RenderStyle::*getter)() const, void (RenderStyle::*setter)(ShadowData*, bool))
    : PropertyWrapperGetter<ShadowData*>(prop, getter)
    , m_setter(setter)
    { }
    
    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const
    {
        ShadowData* shadowa = (a->*m_getter)();
        ShadowData* shadowb = (b->*m_getter)();
        
        if (!shadowa && shadowb || shadowa && !shadowb)
            return false;
        if (shadowa && shadowb && (*shadowa != *shadowb))
            return false;
        return true;
    }
    
    virtual void blend(RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double prog) const
    {
        ShadowData* shadowa = (a->*m_getter)();
        ShadowData* shadowb = (b->*m_getter)();
        ShadowData defaultShadowData(0, 0, 0, Color::transparent);
        
        if (!shadowa)
            shadowa = &defaultShadowData;
        if (!shadowb)
            shadowb = &defaultShadowData;
        
        (dst->*m_setter)(blendFunc(shadowa, shadowb, prog), false);
    }
    
private:
    void (RenderStyle::*m_setter)(ShadowData*, bool);
};

class PropertyWrapperIntSize : public PropertyWrapperGetter<IntSize> {
public:
    PropertyWrapperIntSize(int prop, IntSize (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const IntSize&))
    : PropertyWrapperGetter<IntSize>(prop, getter)
    , m_setter(setter)
    { }
    
    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const
    {
        IntSize sizea = (a->*m_getter)();
        IntSize sizeb = (b->*m_getter)();
        
        return (sizea == sizeb);
    }
    
    virtual void blend(RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double prog) const
    {
        IntSize sizea = (a->*m_getter)();
        IntSize sizeb = (b->*m_getter)();
                
        (dst->*m_setter)(blendFunc(sizea, sizeb, prog));
    }
    
private:
    void (RenderStyle::*m_setter)(const IntSize&);
};

class PropertyWrapperMaybeInvalidColor : public PropertyWrapperBase {
public:
    PropertyWrapperMaybeInvalidColor(int prop, const Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&))
    : PropertyWrapperBase(prop)
    , m_getter(getter)
    , m_setter(setter)
    { }
    
    virtual bool equals(const RenderStyle* a, const RenderStyle* b) const
    {
        Color fromColor = (a->*m_getter)();
        Color toColor = (b->*m_getter)();
        if (!fromColor.isValid())
            fromColor = a->color();
        if (!toColor.isValid())
            toColor = b->color();
        
        return fromColor == toColor;
    }
    
    virtual void blend(RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double prog) const
    {
        Color fromColor = (a->*m_getter)();
        Color toColor = (b->*m_getter)();
        if (!fromColor.isValid())
            fromColor = a->color();
        if (!toColor.isValid())
            toColor = b->color();
        (dst->*m_setter)(blendFunc(fromColor, toColor, prog));
    }
    
private:
    const Color& (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(const Color&);
};

class AnimationControllerPrivate {
public:
    AnimationControllerPrivate(Frame*);
    ~AnimationControllerPrivate();
    
    CompositeAnimation* accessCompositeAnimation(RenderObject*);
    bool clear(RenderObject*);
    
    void animationTimerFired(Timer<AnimationControllerPrivate>*);
    void updateAnimationTimer();
    
    void updateRenderingDispatcherFired(Timer<AnimationControllerPrivate>*);
    void startUpdateRenderingDispatcher();
    
    bool hasAnimations() const { return !m_compositeAnimations.isEmpty(); }
    
    void suspendAnimations(Document* document);
    void resumeAnimations(Document* document);
    
    void styleAvailable();
    
    bool isAnimatingPropertyOnRenderer(RenderObject* obj, int property) const;
    
    static bool propertiesEqual(int prop, const RenderStyle* a, const RenderStyle* b);
    static int getPropertyAtIndex(int i);
    static int getNumProperties() { return gPropertyWrappers->size(); }
    
    // Return true if we need to start software animation timers
    static bool blendProperties(int prop, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double prog);
    
    void setWaitingForStyleAvailable(bool waiting) { if (waiting) m_numStyleAvailableWaiters++; else m_numStyleAvailableWaiters--; }
    
private:
    static void ensurePropertyMap();
    
    typedef HashMap<RenderObject*, CompositeAnimation*> RenderObjectAnimationMap;
    
    RenderObjectAnimationMap m_compositeAnimations;
    Timer<AnimationControllerPrivate> m_animationTimer;
    Timer<AnimationControllerPrivate> m_updateRenderingDispatcher;
    Frame* m_frame;
    uint32_t m_numStyleAvailableWaiters;
    
    static Vector<PropertyWrapperBase*>* gPropertyWrappers;
    static int gPropertyWrapperMap[numCSSProperties];
};

Vector<PropertyWrapperBase*>* AnimationControllerPrivate::gPropertyWrappers = 0;
int AnimationControllerPrivate::gPropertyWrapperMap[];

AnimationControllerPrivate::AnimationControllerPrivate(Frame* frame)
: m_animationTimer(this, &AnimationControllerPrivate::animationTimerFired)
, m_updateRenderingDispatcher(this, &AnimationControllerPrivate::updateRenderingDispatcherFired)
, m_frame(frame)
, m_numStyleAvailableWaiters(0)
{
    ensurePropertyMap();
}

AnimationControllerPrivate::~AnimationControllerPrivate()
{
    deleteAllValues(m_compositeAnimations);
}

// static
void AnimationControllerPrivate::ensurePropertyMap()
{
    // FIXME: This data is never destroyed. Maybe we should ref count it and toss it when the last AnimationController is destroyed?
    if (gPropertyWrappers == 0) {
        gPropertyWrappers = new Vector<PropertyWrapperBase*>();
        
        // build the list of property wrappers to do the comparisons and blends
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyLeft, &RenderStyle::left, &RenderStyle::setLeft));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyRight, &RenderStyle::right, &RenderStyle::setRight));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyTop, &RenderStyle::top, &RenderStyle::setTop));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyBottom, &RenderStyle::bottom, &RenderStyle::setBottom));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWidth, &RenderStyle::width, &RenderStyle::setWidth));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyHeight, &RenderStyle::height, &RenderStyle::setHeight));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyBorderLeftWidth, &RenderStyle::borderLeftWidth, &RenderStyle::setBorderLeftWidth));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyBorderRightWidth, &RenderStyle::borderRightWidth, &RenderStyle::setBorderRightWidth));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyBorderTopWidth, &RenderStyle::borderTopWidth, &RenderStyle::setBorderTopWidth));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyBorderBottomWidth, &RenderStyle::borderBottomWidth, &RenderStyle::setBorderBottomWidth));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMarginLeft, &RenderStyle::marginLeft, &RenderStyle::setMarginLeft));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMarginRight, &RenderStyle::marginRight, &RenderStyle::setMarginRight));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMarginTop, &RenderStyle::marginTop, &RenderStyle::setMarginTop));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyMarginBottom, &RenderStyle::marginBottom, &RenderStyle::setMarginBottom));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyPaddingLeft, &RenderStyle::paddingLeft, &RenderStyle::setPaddingLeft));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyPaddingRight, &RenderStyle::paddingRight, &RenderStyle::setPaddingRight));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyPaddingTop, &RenderStyle::paddingTop, &RenderStyle::setPaddingTop));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyPaddingBottom, &RenderStyle::paddingBottom, &RenderStyle::setPaddingBottom));
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyOpacity, &RenderStyle::opacity, &RenderStyle::setOpacity));
        gPropertyWrappers->append(new PropertyWrapper<const Color&>(CSSPropertyColor, &RenderStyle::color, &RenderStyle::setColor));
        gPropertyWrappers->append(new PropertyWrapper<const Color&>(CSSPropertyBackgroundColor, &RenderStyle::backgroundColor, &RenderStyle::setBackgroundColor));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyWebkitColumnRuleWidth, &RenderStyle::columnRuleWidth, &RenderStyle::setColumnRuleWidth));
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyWebkitColumnGap, &RenderStyle::columnGap, &RenderStyle::setColumnGap));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyWebkitColumnCount, &RenderStyle::columnCount, &RenderStyle::setColumnCount));
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyWebkitColumnWidth, &RenderStyle::columnWidth, &RenderStyle::setColumnWidth));
        gPropertyWrappers->append(new PropertyWrapper<short>(CSSPropertyWebkitBorderHorizontalSpacing, &RenderStyle::horizontalBorderSpacing, &RenderStyle::setHorizontalBorderSpacing));
        gPropertyWrappers->append(new PropertyWrapper<short>(CSSPropertyWebkitBorderVerticalSpacing, &RenderStyle::verticalBorderSpacing, &RenderStyle::setVerticalBorderSpacing));
        gPropertyWrappers->append(new PropertyWrapper<int>(CSSPropertyZIndex, &RenderStyle::zIndex, &RenderStyle::setZIndex));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyLineHeight, &RenderStyle::lineHeight, &RenderStyle::setLineHeight));
        gPropertyWrappers->append(new PropertyWrapper<int>(CSSPropertyOutlineOffset, &RenderStyle::outlineOffset, &RenderStyle::setOutlineOffset));
        gPropertyWrappers->append(new PropertyWrapper<unsigned short>(CSSPropertyOutlineWidth, &RenderStyle::outlineWidth, &RenderStyle::setOutlineWidth));
        gPropertyWrappers->append(new PropertyWrapper<int>(CSSPropertyLetterSpacing, &RenderStyle::letterSpacing, &RenderStyle::setLetterSpacing));
        gPropertyWrappers->append(new PropertyWrapper<int>(CSSPropertyWordSpacing, &RenderStyle::wordSpacing, &RenderStyle::setWordSpacing));
        gPropertyWrappers->append(new PropertyWrapper<const TransformOperations&>(CSSPropertyWebkitTransform, &RenderStyle::transform, &RenderStyle::setTransform));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWebkitTransformOriginX, &RenderStyle::transformOriginX, &RenderStyle::setTransformOriginX));
        gPropertyWrappers->append(new PropertyWrapper<Length>(CSSPropertyWebkitTransformOriginY, &RenderStyle::transformOriginY, &RenderStyle::setTransformOriginY));
        gPropertyWrappers->append(new PropertyWrapperIntSize(CSSPropertyWebkitBorderTopLeftRadius, &RenderStyle::borderTopLeftRadius, &RenderStyle::setBorderTopLeftRadius));
        gPropertyWrappers->append(new PropertyWrapperIntSize(CSSPropertyWebkitBorderTopRightRadius, &RenderStyle::borderTopRightRadius, &RenderStyle::setBorderTopRightRadius));
        gPropertyWrappers->append(new PropertyWrapperIntSize(CSSPropertyWebkitBorderBottomLeftRadius, &RenderStyle::borderBottomLeftRadius, &RenderStyle::setBorderBottomLeftRadius));
        gPropertyWrappers->append(new PropertyWrapperIntSize(CSSPropertyWebkitBorderBottomRightRadius, &RenderStyle::borderBottomRightRadius, &RenderStyle::setBorderBottomRightRadius));
        gPropertyWrappers->append(new PropertyWrapper<EVisibility>(CSSPropertyVisibility, &RenderStyle::visibility, &RenderStyle::setVisibility));
        gPropertyWrappers->append(new PropertyWrapper<float>(CSSPropertyZoom, &RenderStyle::zoom, &RenderStyle::setZoom));
        
        // FIXME: these might be invalid colors, need to check for that
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyWebkitColumnRuleColor, &RenderStyle::columnRuleColor, &RenderStyle::setColumnRuleColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyWebkitTextStrokeColor, &RenderStyle::textStrokeColor, &RenderStyle::setTextStrokeColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyWebkitTextFillColor, &RenderStyle::textFillColor, &RenderStyle::setTextFillColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyBorderLeftColor, &RenderStyle::borderLeftColor, &RenderStyle::setBorderLeftColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyBorderRightColor, &RenderStyle::borderRightColor, &RenderStyle::setBorderRightColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyBorderTopColor, &RenderStyle::borderTopColor, &RenderStyle::setBorderTopColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyBorderBottomColor, &RenderStyle::borderBottomColor, &RenderStyle::setBorderBottomColor));
        gPropertyWrappers->append(new PropertyWrapperMaybeInvalidColor(CSSPropertyOutlineColor, &RenderStyle::outlineColor, &RenderStyle::setOutlineColor));
        
        // These are for shadows
        gPropertyWrappers->append(new PropertyWrapperShadow(CSSPropertyWebkitBoxShadow, &RenderStyle::boxShadow, &RenderStyle::setBoxShadow));
        gPropertyWrappers->append(new PropertyWrapperShadow(CSSPropertyTextShadow, &RenderStyle::textShadow, &RenderStyle::setTextShadow));
        
        // Make sure unused slots have a value
        for (unsigned int i = 0; i < (unsigned int) numCSSProperties; ++i)
            gPropertyWrapperMap[i] = CSSPropertyInvalid;
        
        size_t n = gPropertyWrappers->size();
        for (unsigned int i = 0; i < n; ++i) {
            ASSERT((*gPropertyWrappers)[i]->property() - firstCSSProperty < numCSSProperties);
            gPropertyWrapperMap[(*gPropertyWrappers)[i]->property() - firstCSSProperty] = i;
        }
    }
}

// static
bool AnimationControllerPrivate::propertiesEqual(int prop, const RenderStyle* a, const RenderStyle* b)
{
    if (prop == cAnimateAll) {
        size_t n = gPropertyWrappers->size();
        for (unsigned int i = 0; i < n; ++i) {
            if (!(*gPropertyWrappers)[i]->equals(a, b))
                return false;
        }
    }
    else {
        int propIndex = prop - firstCSSProperty;
        
        if (propIndex >= 0 && propIndex < numCSSProperties) {
            int i = gPropertyWrapperMap[propIndex];
            return (i >= 0) ? (*gPropertyWrappers)[i]->equals(a, b) : true;
        }
    }
    return true;
}

// static
int AnimationControllerPrivate::getPropertyAtIndex(int i)
{
    if (i < 0 || i >= (int) gPropertyWrappers->size())
        return CSSPropertyInvalid;
        
    return (*gPropertyWrappers)[i]->property();
}
    
// static - return true if we need to start software animation timers
bool AnimationControllerPrivate::blendProperties(int prop, RenderStyle* dst, const RenderStyle* a, const RenderStyle* b, double prog)
{
    if (prop == cAnimateAll) {
        bool needsTimer = false;
    
        size_t n = gPropertyWrappers->size();
        for (unsigned int i = 0; i < n; ++i) {
            PropertyWrapperBase* wrapper = (*gPropertyWrappers)[i];
            if (!wrapper->equals(a, b)) {
                wrapper->blend(dst, a, b, prog);
                needsTimer = true;
            }
        }
        return needsTimer;
    }
    
    int propIndex = prop - firstCSSProperty;
    if (propIndex >= 0 && propIndex < numCSSProperties) {
        int i = gPropertyWrapperMap[propIndex];
        if (i >= 0) {
            PropertyWrapperBase* wrapper = (*gPropertyWrappers)[i];
            wrapper->blend(dst, a, b, prog);
            return true;
        }
    }
    
    return false;
}

CompositeAnimation* AnimationControllerPrivate::accessCompositeAnimation(RenderObject* renderer)
{
    CompositeAnimation* animation = m_compositeAnimations.get(renderer);
    if (!animation) {
        animation = new CompositeAnimation(this);
        m_compositeAnimations.set(renderer, animation);
    }
    return animation;
}

bool AnimationControllerPrivate::clear(RenderObject* renderer)
{
    // Return false if we didn't do anything OR we are suspended (so we don't try to
    // do a setChanged() when suspended
    CompositeAnimation* animation = m_compositeAnimations.take(renderer);
    if (!animation)
        return false;
    animation->resetTransitions(renderer);
    bool wasSuspended = animation->suspended();
    delete animation;
    return !wasSuspended;
}

void AnimationControllerPrivate::styleAvailable()
{
    if (m_numStyleAvailableWaiters == 0)
        return;
    
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); 
         it != animationsEnd; ++it) {
        it->second->styleAvailable();
    }
}

void AnimationControllerPrivate::updateAnimationTimer()
{
    bool animating = false;
    
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); 
         it != animationsEnd; ++it) {
        CompositeAnimation* compAnim = it->second;
        if (!compAnim->suspended() && compAnim->animating()) {
            animating = true;
            break;
        }
    }
    
    if (animating) {
        if (!m_animationTimer.isActive())
            m_animationTimer.startRepeating(cAnimationTimerDelay);
    } else if (m_animationTimer.isActive())
        m_animationTimer.stop();
}

void AnimationControllerPrivate::updateRenderingDispatcherFired(Timer<AnimationControllerPrivate>*)
{
    if (m_frame && m_frame->document()) {
        m_frame->document()->updateRendering();
    }
}

void AnimationControllerPrivate::startUpdateRenderingDispatcher()
{
    if (!m_updateRenderingDispatcher.isActive()) {
        m_updateRenderingDispatcher.startOneShot(0);
    }
}

void AnimationControllerPrivate::animationTimerFired(Timer<AnimationControllerPrivate>* timer)
{
    // When the timer fires, all we do is call setChanged on all DOM nodes with running animations and then do an immediate
    // updateRendering.  It will then call back to us with new information.
    bool animating = false;
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); 
         it != animationsEnd; ++it) {
        RenderObject* renderer = it->first;
        CompositeAnimation* compAnim = it->second;
        if (!compAnim->suspended() && compAnim->animating()) {
            animating = true;
            compAnim->setAnimating(false);
            setChanged(renderer->element());
        }
    }
    
    m_frame->document()->updateRendering();
    
    updateAnimationTimer();
}

bool AnimationControllerPrivate::isAnimatingPropertyOnRenderer(RenderObject* obj, int property) const
{
    CompositeAnimation* animation = m_compositeAnimations.get(obj);
    if (!animation) return false;
    
    return animation->isAnimatingProperty(property);
}

void AnimationControllerPrivate::suspendAnimations(Document* document)
{
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); 
         it != animationsEnd; ++it) {
        RenderObject* renderer = it->first;
        CompositeAnimation* compAnim = it->second;
        if (renderer->document() == document)
            compAnim->suspendAnimations();
    }
    
    updateAnimationTimer();
}

void AnimationControllerPrivate::resumeAnimations(Document* document)
{
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); 
         it != animationsEnd; ++it) {
        RenderObject* renderer = it->first;
        CompositeAnimation* compAnim = it->second;
        if (renderer->document() == document)
            compAnim->resumeAnimations();
    }
    
    updateAnimationTimer();
}

void CompositeAnimation::updateTransitions(RenderObject* renderer, RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    // If currentStyle is null, we don't do transitions
    if (!currentStyle || !targetStyle->transitions())
        return;
        
    // Check to see if we need to update the active transitions
    for (size_t i = 0; i < targetStyle->transitions()->size(); ++i) {
        const Animation* anim = (*targetStyle->transitions())[i].get();
        double duration = anim->duration();
        double delay = anim->delay();

        // If this is an empty transition, skip it
        if (duration == 0 && delay <= 0)
            continue;
         
        int prop = anim->property();
        bool all = prop == cAnimateAll;
        
        // Handle both the 'all' and single property cases. For the single prop case, we make only one pass
        // through the loop
        for (int i = 0; ; ++i) {
            if (all) {
                if (i >= AnimationControllerPrivate::getNumProperties())
                    break;
                // get the next prop
                prop = AnimationControllerPrivate::getPropertyAtIndex(i);
            }

            // See if there is a current transition for this prop
            ImplicitAnimation* implAnim = m_transitions.get(prop);
            bool equal = true;
            
            if (implAnim) {
                // There is one, has our target changed?
                if (!implAnim->isTargetPropertyEqual(prop, targetStyle)) {
                    // It has changed - toss it and start over
                    // Opacity is special since it can pop in and out of RenderLayers. We need to compute
                    // the blended opacity value between the previous from and to styles and put that in the currentStyle, which
                    // will become the new fromStyle. This is changing a const RenderStyle, but we know what we are doing, really :-)
                    if (prop == CSSPropertyOpacity) {
                        // get the blended value of opacity into the currentStyle (which will be the new fromStyle)
                        implAnim->blendPropertyValueInStyle(CSSPropertyOpacity, currentStyle);
                    }

                    implAnim->reset(renderer);
                    delete implAnim;
                    m_transitions.remove(prop);
                    equal = false;
                }
            }
            else
                // See if we need to start a new transition
                equal = AnimationControllerPrivate::propertiesEqual(prop, currentStyle, targetStyle);
            
            if (!equal) {
                // AAdd the new transition
                ImplicitAnimation* animation = new ImplicitAnimation(const_cast<Animation*>(anim), renderer, this);
                m_transitions.set(prop, animation);
            }
            
            // We only need one pass for the single prop case
            if (!all)
                break;
        }
    }
}

RenderStyle* CompositeAnimation::animate(RenderObject* renderer, RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    RenderStyle* resultStyle = 0;
    
    // We don't do any transitions if we don't have a currentStyle (on startup)
    updateTransitions(renderer, currentStyle, targetStyle);
    
    if (currentStyle) {
        // Now that we have transition objects ready, let them know about the new goal state.  We want them
        // to fill in a RenderStyle*& only if needed.
        CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
            ImplicitAnimation*  anim = it->second;
            if (anim) {
                anim->animate(this, renderer, currentStyle, targetStyle, resultStyle);
            }
        }
    }
    
    cleanupFinishedAnimations(renderer);
    
    return resultStyle ? resultStyle : targetStyle;
}

// "animating" means that something is running that requires the timer to keep firing
// (e.g. a transition)
void CompositeAnimation::setAnimating(bool inAnimating)
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation*  transition = it->second;
        transition->setAnimating(inAnimating);
    }
}

bool CompositeAnimation::animating()
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation*  transition = it->second;
        if (transition && transition->animating() && transition->running())
            return true;
    }
    return false;
}

void CompositeAnimation::resetTransitions(RenderObject* renderer)
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation*  transition = it->second;
        transition->reset(renderer);
        delete transition;
    }
    m_transitions.clear();
}

void CompositeAnimation::cleanupFinishedAnimations(RenderObject* renderer)
{
    if (suspended())
        return;
    
    // Make a list of transitions to be deleted
    Vector<int> finishedTransitions;
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();

    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (!anim)
            continue;
        if (anim->postactive() && !anim->waitingForEndEvent())
            finishedTransitions.append(anim->property());
    }
    
    // Delete them
    for (Vector<int>::iterator it = finishedTransitions.begin(); it != finishedTransitions.end(); ++it) {
        ImplicitAnimation* anim = m_transitions.get(*it);
        if (anim) {
            anim->reset(renderer);
            delete anim;
        }
        m_transitions.remove(*it);
    }
}

void CompositeAnimation::setTransitionStartTime(int property, double t)
{
    // Set the start time for given property transition
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && anim->waitingForStartTime() && 
                    (anim->property() == property || anim->property() == cAnimateAll))
            anim->updateStateMachine(AnimationBase::STATE_INPUT_START_TIME_SET, t);
    }
}

void CompositeAnimation::suspendAnimations()
{
    if (m_suspended)
        return;
    
    m_suspended = true;

    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && anim->hasStyle()) {
            anim->updatePlayState(false);
        }
    }
}

void CompositeAnimation::resumeAnimations()
{
    if (!m_suspended)
        return;
    
    m_suspended = false;

    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && anim->hasStyle()) {
            anim->updatePlayState(true);
        }
    }
}

void CompositeAnimation::overrideImplicitAnimations(int property)
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && (anim->property() == property || anim->property() == cAnimateAll))
            anim->setOverridden(true);
    }
}

void CompositeAnimation::resumeOverriddenImplicitAnimations(int property)
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && (anim->property() == property || anim->property() == cAnimateAll))
            anim->setOverridden(false);
    }
}

void CompositeAnimation::styleAvailable()
{
    if (m_numStyleAvailableWaiters == 0)
        return;
    
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && anim->waitingForStyleAvailable())
            anim->updateStateMachine(AnimationBase::STATE_INPUT_STYLE_AVAILABLE, -1);
    }
}

bool CompositeAnimation::isAnimatingProperty(int property) const
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && anim->isAnimatingProperty(property))
            return true;
    }
    return false;
}

void ImplicitAnimation::animate(CompositeAnimation* animation, RenderObject* renderer, const RenderStyle* currentStyle, 
                                const RenderStyle* targetStyle, RenderStyle*& ioAnimatedStyle)
{
    if (paused())
        return;
    
    // If we get this far and the animation is done, it means we are cleaning up a just finished animation.
    // If so, send back the targetStyle (it will get tossed later)
    if (postactive()) {
        if (!ioAnimatedStyle)
            ioAnimatedStyle = const_cast<RenderStyle*>(targetStyle);
        return;
    }

    // Reset to start the transition if we are new
    if (isnew())
        reset(renderer, currentStyle, targetStyle);
    
    // Run a cycle of animation.
    // We know we will need a new render style, so make one if needed
    if (!ioAnimatedStyle)
        ioAnimatedStyle = new (renderer->renderArena()) RenderStyle(*targetStyle);
    
    double prog = progress(1, 0);
    bool needsAnim = AnimationControllerPrivate::blendProperties(m_property, ioAnimatedStyle, m_fromStyle, m_toStyle, prog);
    if (needsAnim)
        setAnimating();
}

bool ImplicitAnimation::startAnimation(double beginTime)
{
    return false;
}

void ImplicitAnimation::endAnimation(bool reset)
{
}

void ImplicitAnimation::onAnimationEnd(double inElapsedTime)
{
    // we're converting the animation into a transition here
    if (!sendTransitionEvent(EventNames::webkitTransitionEndEvent, inElapsedTime)) {
        // we didn't dispatch an event, which would call endAnimation(), so we'll just end
        // it here.
        endAnimation(true);
    }
}

bool ImplicitAnimation::sendTransitionEvent(const AtomicString& inEventType, double inElapsedTime)
{
    return false; // didn't dispatch an event
}

void ImplicitAnimation::reset(RenderObject* renderer, const RenderStyle* from /* = 0 */, const RenderStyle* to /* = 0 */)
{
    ASSERT((!m_toStyle && !to) || m_toStyle != to);
    ASSERT((!m_fromStyle && !from) || m_fromStyle != from);
    if (m_fromStyle)
        m_fromStyle->deref(renderer->renderArena());
    if (m_toStyle)
        m_toStyle->deref(renderer->renderArena());
    
    m_fromStyle = const_cast<RenderStyle*>(from);   // it is read-only, other than the ref
    if (m_fromStyle)
        m_fromStyle->ref();
    
    m_toStyle = const_cast<RenderStyle*>(to);       // it is read-only, other than the ref
    if (m_toStyle)
        m_toStyle->ref();
    
    // restart the transition
    if (from && to)
        updateStateMachine(STATE_INPUT_RESTART_ANIMATION, -1);
}

void ImplicitAnimation::setOverridden(bool b)
{
    if (b != m_overridden) {
        m_overridden = b;
        updateStateMachine(m_overridden ? STATE_INPUT_PAUSE_OVERRIDE : STATE_INPUT_RESUME_OVERRIDE, -1);
    }
}

bool ImplicitAnimation::affectsProperty(int property) const
{
    return m_property == property ||
    (m_property == cAnimateAll && !AnimationControllerPrivate::propertiesEqual(property, m_fromStyle, m_toStyle));
}

bool ImplicitAnimation::isTargetPropertyEqual(int prop, RenderStyle* targetStyle)
{
    return AnimationControllerPrivate::propertiesEqual(prop, m_toStyle, targetStyle);
}

void ImplicitAnimation::blendPropertyValueInStyle(int prop, RenderStyle* currentStyle)
{
    double prog = progress(1, 0);
    AnimationControllerPrivate::blendProperties(prop, currentStyle, m_fromStyle, m_toStyle, prog);
}    

AnimationController::AnimationController(Frame* frame)
: m_data(new AnimationControllerPrivate(frame))
{
    
}

AnimationController::~AnimationController()
{
    delete m_data;
}

void AnimationController::cancelAnimations(RenderObject* renderer)
{
    if (!m_data->hasAnimations())
        return;
    
    if (m_data->clear(renderer))
        setChanged(renderer->element());
}

RenderStyle* AnimationController::updateAnimations(RenderObject* renderer, RenderStyle* newStyle)
{    
    // don't do anything if we're in the cache
    if (!renderer->document() || renderer->document()->inPageCache())
        return newStyle;
    
    RenderStyle* oldStyle = renderer->style();
    
    if ((!oldStyle || !oldStyle->transitions()) && !newStyle->transitions())
        return newStyle;
    
    RenderStyle* blendedStyle = newStyle;
    
    // Fetch our current set of implicit animations from a hashtable.  We then compare them
    // against the animations in the style and make sure we're in sync.  If destination values
    // have changed, we reset the animation.  We then do a blend to get new values and we return
    // a new style.
    ASSERT(renderer->element()); // FIXME: We do not animate generated content yet.
    
    CompositeAnimation* rendererAnimations = m_data->accessCompositeAnimation(renderer);
    blendedStyle = rendererAnimations->animate(renderer, oldStyle, newStyle);
    
    m_data->updateAnimationTimer();
    
    if (blendedStyle != newStyle) {
        // If the animations/transitions change opacity or transform, we neeed to update
        // the style to impose the stacking rules. Note that this is also
        // done in CSSStyleSelector::adjustRenderStyle().
        if (blendedStyle->hasAutoZIndex() && (blendedStyle->opacity() < 1.0f || blendedStyle->hasTransform()))
            blendedStyle->setZIndex(0);
    }
    return blendedStyle;
}

bool AnimationController::isAnimatingPropertyOnRenderer(RenderObject* obj, int property) const
{
    return m_data->isAnimatingPropertyOnRenderer(obj, property);
}

void AnimationController::suspendAnimations(Document* document)
{
    m_data->suspendAnimations(document);
}

void AnimationController::resumeAnimations(Document* document)
{
    m_data->resumeAnimations(document);
}

void AnimationController::startUpdateRenderingDispatcher()
{
    m_data->startUpdateRenderingDispatcher();
}

void AnimationController::styleAvailable()
{
    m_data->styleAvailable();
}

void CompositeAnimation::setWaitingForStyleAvailable(bool waiting)
{
    if (waiting)
        m_numStyleAvailableWaiters++;
    else
        m_numStyleAvailableWaiters--;
    m_animationController->setWaitingForStyleAvailable(waiting);
}
    
}
