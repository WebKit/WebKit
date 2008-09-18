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
#include "KeyframeAnimation.h"

#include "CSSPropertyNames.h"
#include "CompositeAnimation.h"
#include "EventNames.h"
#include "RenderObject.h"
#include "SystemTime.h"

namespace WebCore {

KeyframeAnimation::~KeyframeAnimation()
{
    // Do the cleanup here instead of in the base class so the specialized methods get called
    if (!postActive())
        updateStateMachine(AnimationStateInputEndAnimation, -1);
}

void KeyframeAnimation::animate(CompositeAnimation* animation, RenderObject* renderer, const RenderStyle* currentStyle, 
                                    const RenderStyle* targetStyle, RenderStyle*& animatedStyle)
{
    // If we have not yet started, we will not have a valid start time, so just start the animation if needed.
    if (isNew() && m_animation->playState() == AnimPlayStatePlaying)
        updateStateMachine(AnimationStateInputStartAnimation, -1);

    // If we get this far and the animation is done, it means we are cleaning up a just finished animation.
    // If so, we need to send back the targetStyle.
    if (postActive()) {
        if (!animatedStyle)
            animatedStyle = const_cast<RenderStyle*>(targetStyle);
        return;
    }

    // If we are waiting for the start timer, we don't want to change the style yet.
    // Special case - if the delay time is 0, then we do want to set the first frame of the
    // animation right away. This avoids a flash when the animation starts.
    if (waitingToStart() && m_animation->delay() > 0)
        return;

    // FIXME: we need to be more efficient about determining which keyframes we are animating between.
    // We should cache the last pair or something.

    // Find the first key
    double elapsedTime = (m_startTime > 0) ? ((!paused() ? currentTime() : m_pauseTime) - m_startTime) : 0;
    if (elapsedTime < 0)
        elapsedTime = 0;

    double t = m_animation->duration() ? (elapsedTime / m_animation->duration()) : 1;
    int i = static_cast<int>(t);
    t -= i;
    if (m_animation->direction() && (i & 1))
        t = 1 - t;

    RenderStyle* fromStyle = 0;
    RenderStyle* toStyle = 0;
    double scale = 1;
    double offset = 0;
    if (m_keyframes.get()) {
        Vector<KeyframeValue>::const_iterator end = m_keyframes->endKeyframes();
        for (Vector<KeyframeValue>::const_iterator it = m_keyframes->beginKeyframes(); it != end; ++it) {
            if (t < it->key) {
                // The first key should always be 0, so we should never succeed on the first key
                if (!fromStyle)
                    break;
                scale = 1.0 / (it->key - offset);
                toStyle = const_cast<RenderStyle*>(&(it->style));
                break;
            }

            offset = it->key;
            fromStyle = const_cast<RenderStyle*>(&(it->style));
        }
    }

    // If either style is 0 we have an invalid case, just stop the animation.
    if (!fromStyle || !toStyle) {
        updateStateMachine(AnimationStateInputEndAnimation, -1);
        return;
    }

    // Run a cycle of animation.
    // We know we will need a new render style, so make one if needed.
    if (!animatedStyle)
        animatedStyle = new (renderer->renderArena()) RenderStyle(*targetStyle);

    double prog = progress(scale, offset);

    HashSet<int>::const_iterator end = m_keyframes->endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes->beginProperties(); it != end; ++it) {
        if (blendProperties(this, *it, animatedStyle, fromStyle, toStyle, prog))
            setAnimating();
    }
}

void KeyframeAnimation::endAnimation(bool)
{
    // Restore the original (unanimated) style
    if (m_object)
        setChanged(m_object->element());
}

bool KeyframeAnimation::shouldSendEventForListener(Document::ListenerType listenerType)
{
    return m_object->document()->hasListenerType(listenerType);
}

void KeyframeAnimation::onAnimationStart(double elapsedTime)
{
    sendAnimationEvent(EventNames::webkitAnimationStartEvent, elapsedTime);
}

void KeyframeAnimation::onAnimationIteration(double elapsedTime)
{
    sendAnimationEvent(EventNames::webkitAnimationIterationEvent, elapsedTime);
}

void KeyframeAnimation::onAnimationEnd(double elapsedTime)
{
    if (!sendAnimationEvent(EventNames::webkitAnimationEndEvent, elapsedTime)) {
        // We didn't dispatch an event, which would call endAnimation(), so we'll just call it here.
        endAnimation(true);
    }
}

bool KeyframeAnimation::sendAnimationEvent(const AtomicString& eventType, double elapsedTime)
{
    Document::ListenerType listenerType;
    if (eventType == EventNames::webkitAnimationIterationEvent)
        listenerType = Document::ANIMATIONITERATION_LISTENER;
    else if (eventType == EventNames::webkitAnimationEndEvent)
        listenerType = Document::ANIMATIONEND_LISTENER;
    else {
        ASSERT(eventType == EventNames::webkitAnimationStartEvent);
        listenerType = Document::ANIMATIONSTART_LISTENER;
    }

    if (shouldSendEventForListener(listenerType)) {
        if (Element* element = elementForEventDispatch()) {
            m_waitingForEndEvent = true;
            m_animationEventDispatcher.startTimer(element, m_name, -1, true, eventType, elapsedTime);
            return true; // Did dispatch an event
        }
    }

    return false; // Did not dispatch an event
}

void KeyframeAnimation::overrideAnimations()
{
    // This will override implicit animations that match the properties in the keyframe animation
    HashSet<int>::const_iterator end = m_keyframes->endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes->beginProperties(); it != end; ++it)
        compositeAnimation()->overrideImplicitAnimations(*it);
}

void KeyframeAnimation::resumeOverriddenAnimations()
{
    // This will resume overridden implicit animations
    HashSet<int>::const_iterator end = m_keyframes->endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes->beginProperties(); it != end; ++it)
        compositeAnimation()->resumeOverriddenImplicitAnimations(*it);
}

bool KeyframeAnimation::affectsProperty(int property) const
{
    HashSet<int>::const_iterator end = m_keyframes->endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes->beginProperties(); it != end; ++it) {
        if (*it == property)
            return true;
    }
    return false;
}

void KeyframeAnimation::validateTransformFunctionList()
{
    m_transformFunctionListValid = false;
    
    if (!m_keyframes.get() || m_keyframes->size() < 2 || !m_keyframes->containsProperty(CSSPropertyWebkitTransform))
        return;

    Vector<KeyframeValue>::const_iterator end = m_keyframes->endKeyframes();

    // Empty transforms match anything, so find the first non-empty entry as the reference
    size_t firstIndex = 0;
    Vector<KeyframeValue>::const_iterator firstIt = end;
    
    for (Vector<KeyframeValue>::const_iterator it = m_keyframes->beginKeyframes(); it != end; ++it, ++firstIndex) {
        if (it->style.transform().size() > 0) {
            firstIt = it;
            break;
        }
    }
    
    if (firstIt == end)
        return;
        
    const TransformOperations* firstVal = &firstIt->style.transform();
    
    // See if the keyframes are valid
    for (Vector<KeyframeValue>::const_iterator it = firstIt+1; it != end; ++it) {
        const TransformOperations* val = &it->style.transform();
        
        // A null transform matches anything
        if (val->isEmpty())
            continue;
        
        // If the sizes of the function lists don't match, the lists don't match
        if (firstVal->size() != val->size())
            return;
        
        // If the types of each function are not the same, the lists don't match
        for (size_t j = 0; j < firstVal->size(); ++j) {
            if (!firstVal->at(j)->isSameType(*val->at(j)))
                return;
        }
    }

    // Keyframes are valid
    m_transformFunctionListValid = true;
}

} // namespace WebCore
