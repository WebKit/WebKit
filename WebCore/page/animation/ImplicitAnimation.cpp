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
#include "ImplicitAnimation.h"
#include "RenderObject.h"
#include "EventNames.h"
#include "CSSPropertyNames.h"

namespace WebCore {

ImplicitAnimation::ImplicitAnimation(const Animation* transition, int animatingProperty, RenderObject* renderer, CompositeAnimation* compAnim)
: AnimationBase(transition, renderer, compAnim)
, m_transitionProperty(transition->property())
, m_animatingProperty(animatingProperty)
, m_overridden(false)
, m_fromStyle(0)
, m_toStyle(0)
{
    ASSERT(animatingProperty != cAnimateAll);
}
    
bool ImplicitAnimation::shouldSendEventForListener(Document::ListenerType inListenerType)
{
    return m_object->document()->hasListenerType(inListenerType);
}

void ImplicitAnimation::animate(CompositeAnimation* animation, RenderObject* renderer, const RenderStyle* currentStyle, 
                                const RenderStyle* targetStyle, RenderStyle*& animatedStyle)
{
    if (paused())
        return;
    
    // If we get this far and the animation is done, it means we are cleaning up a just finished animation.
    // So just return. Everything is already all cleaned up
    if (postActive())
        return;

    // Reset to start the transition if we are new
    if (isNew())
        reset(renderer, currentStyle, targetStyle);
    
    // Run a cycle of animation.
    // We know we will need a new render style, so make one if needed
    if (!animatedStyle)
        animatedStyle = new (renderer->renderArena()) RenderStyle(*targetStyle);
    
    double prog = progress(1, 0);
    bool needsAnim = blendProperties(m_animatingProperty, animatedStyle, m_fromStyle, m_toStyle, prog);
    if (needsAnim)
        setAnimating();
}

void ImplicitAnimation::onAnimationEnd(double inElapsedTime)
{
    if (!sendTransitionEvent(EventNames::webkitTransitionEndEvent, inElapsedTime)) {
        // We didn't dispatch an event, which would call endAnimation(), so we'll just call it here.
        endAnimation(true);
    }
}

bool ImplicitAnimation::sendTransitionEvent(const AtomicString& inEventType, double inElapsedTime)
{
    if (inEventType == EventNames::webkitTransitionEndEvent) {
        Document::ListenerType listenerType = Document::TRANSITIONEND_LISTENER;
        
        if (shouldSendEventForListener(listenerType)) {
            Element* element = elementForEventDispatch();
            if (element) {
                String propertyName;
                if (m_transitionProperty != cAnimateAll)
                    propertyName = String(getPropertyName((CSSPropertyID)m_transitionProperty));
                m_waitingForEndEvent = true;
                m_animationEventDispatcher.startTimer(element, propertyName, m_transitionProperty, true, inEventType, inElapsedTime);
                return true; // Did dispatch an event
            }
        }
    }
    
    return false; // Didn't dispatch an event
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
        updateStateMachine(AnimationStateInputRestartAnimation, -1);
}

void ImplicitAnimation::setOverridden(bool b)
{
    if (b != m_overridden) {
        m_overridden = b;
        updateStateMachine(m_overridden ? AnimationStateInputPauseOverride : AnimationStateInputResumeOverride, -1);
    }
}

bool ImplicitAnimation::affectsProperty(int property) const
{
    return (m_animatingProperty == property);
}

bool ImplicitAnimation::isTargetPropertyEqual(int prop, const RenderStyle* targetStyle)
{
    return propertiesEqual(prop, m_toStyle, targetStyle);
}

void ImplicitAnimation::blendPropertyValueInStyle(int prop, RenderStyle* currentStyle)
{
    double prog = progress(1, 0);
    blendProperties(prop, currentStyle, m_fromStyle, m_toStyle, prog);
}    
    
}
