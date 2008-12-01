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
#include "CSSPropertyNames.h"
#include "EventNames.h"
#include "ImplicitAnimation.h"
#include "RenderObject.h"

namespace WebCore {

ImplicitAnimation::ImplicitAnimation(const Animation* transition, int animatingProperty, RenderObject* renderer, CompositeAnimation* compAnim, RenderStyle* fromStyle)
    : AnimationBase(transition, renderer, compAnim)
    , m_transitionProperty(transition->property())
    , m_animatingProperty(animatingProperty)
    , m_overridden(false)
    , m_fromStyle(fromStyle)
{
    ASSERT(animatingProperty != cAnimateAll);
}

ImplicitAnimation::~ImplicitAnimation()
{
    // Do the cleanup here instead of in the base class so the specialized methods get called
    if (!postActive())
        updateStateMachine(AnimationStateInputEndAnimation, -1);
}

bool ImplicitAnimation::shouldSendEventForListener(Document::ListenerType inListenerType)
{
    return m_object->document()->hasListenerType(inListenerType);
}

void ImplicitAnimation::animate(CompositeAnimation* animation, RenderObject* renderer, RenderStyle* currentStyle,
                                RenderStyle* targetStyle, RefPtr<RenderStyle>& animatedStyle)
{
    // If we get this far and the animation is done, it means we are cleaning up a just finished animation.
    // So just return. Everything is already all cleaned up.
    if (postActive())
        return;

    // Reset to start the transition if we are new
    if (isNew())
        reset(targetStyle);

    // Run a cycle of animation.
    // We know we will need a new render style, so make one if needed
    if (!animatedStyle)
        animatedStyle = RenderStyle::clone(targetStyle);

    if (blendProperties(this, m_animatingProperty, animatedStyle.get(), m_fromStyle.get(), m_toStyle.get(), progress(1, 0, 0)))
        setAnimating();
}

void ImplicitAnimation::onAnimationEnd(double elapsedTime)
{
    if (!sendTransitionEvent(eventNames().webkitTransitionEndEvent, elapsedTime)) {
        // We didn't dispatch an event, which would call endAnimation(), so we'll just call it here.
        endAnimation(true);
    }
}

bool ImplicitAnimation::sendTransitionEvent(const AtomicString& eventType, double elapsedTime)
{
    if (eventType == eventNames().webkitTransitionEndEvent) {
        Document::ListenerType listenerType = Document::TRANSITIONEND_LISTENER;

        if (shouldSendEventForListener(listenerType)) {
            String propertyName;
            if (m_animatingProperty != cAnimateAll)
                propertyName = getPropertyName(static_cast<CSSPropertyID>(m_animatingProperty));
                
            // Dispatch the event
            RefPtr<Element> element = 0;
            if (m_object->node() && m_object->node()->isElementNode())
                element = static_cast<Element*>(m_object->node());

            ASSERT(!element || element->document() && !element->document()->inPageCache());
            if (!element)
                return false;

            // Call the event handler
            element->dispatchWebKitTransitionEvent(eventType, propertyName, elapsedTime);

            // Restore the original (unanimated) style
            if (eventType == eventNames().webkitAnimationEndEvent && element->renderer())
                setChanged(element.get());

            return true; // Did dispatch an event
        }
    }

    return false; // Didn't dispatch an event
}

void ImplicitAnimation::reset(RenderStyle* to)
{
    ASSERT(to);
    ASSERT(m_fromStyle);
    

    m_toStyle = to;

    // Restart the transition
    if (m_fromStyle && m_toStyle)
        updateStateMachine(AnimationStateInputRestartAnimation, -1);
        
    // set the transform animation list
    validateTransformFunctionList();
}

void ImplicitAnimation::setOverridden(bool b)
{
    if (b == m_overridden)
        return;

    m_overridden = b;
    updateStateMachine(m_overridden ? AnimationStateInputPauseOverride : AnimationStateInputResumeOverride, -1);
}

bool ImplicitAnimation::affectsProperty(int property) const
{
    return (m_animatingProperty == property);
}

bool ImplicitAnimation::isTargetPropertyEqual(int prop, const RenderStyle* targetStyle)
{
    return propertiesEqual(prop, m_toStyle.get(), targetStyle);
}

void ImplicitAnimation::blendPropertyValueInStyle(int prop, RenderStyle* currentStyle)
{
    blendProperties(this, prop, currentStyle, m_fromStyle.get(), m_toStyle.get(), progress(1, 0, 0));
}

void ImplicitAnimation::validateTransformFunctionList()
{
    m_transformFunctionListValid = false;
    
    if (!m_fromStyle || !m_toStyle)
        return;
        
    const TransformOperations* val = &m_fromStyle->transform();
    const TransformOperations* toVal = &m_toStyle->transform();

    if (val->operations().isEmpty())
        val = toVal;

    if (val->operations().isEmpty())
        return;
        
    // See if the keyframes are valid
    if (val != toVal) {
        // A list of length 0 matches anything
        if (!toVal->operations().isEmpty()) {
            // If the sizes of the function lists don't match, the lists don't match
            if (val->operations().size() != toVal->operations().size())
                return;
        
            // If the types of each function are not the same, the lists don't match
            for (size_t j = 0; j < val->operations().size(); ++j) {
                if (!val->operations()[j]->isSameType(*toVal->operations()[j]))
                    return;
            }
        }
    }

    // Keyframes are valid
    m_transformFunctionListValid = true;
}

} // namespace WebCore
