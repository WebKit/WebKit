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

#include "AnimationControllerPrivate.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "CompositeAnimation.h"
#include "EventNames.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderStyle.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

KeyframeAnimation::KeyframeAnimation(const Animation* animation, RenderObject* renderer, int index, CompositeAnimation* compAnim, RenderStyle* unanimatedStyle)
    : AnimationBase(animation, renderer, compAnim)
    , m_keyframes(renderer, animation->name())
    , m_index(index)
    , m_unanimatedStyle(unanimatedStyle)
{
    // Get the keyframe RenderStyles
    if (m_object && m_object->node() && m_object->node()->isElementNode())
        m_object->document()->styleSelector()->keyframeStylesForAnimation(static_cast<Element*>(m_object->node()), unanimatedStyle, m_keyframes);

    // Update the m_transformFunctionListValid flag based on whether the function lists in the keyframes match.
    validateTransformFunctionList();
}

KeyframeAnimation::~KeyframeAnimation()
{
    // Make sure to tell the renderer that we are ending. This will make sure any accelerated animations are removed.
    if (!postActive())
        endAnimation();
}

void KeyframeAnimation::getKeyframeAnimationInterval(const RenderStyle*& fromStyle, const RenderStyle*& toStyle, double& prog) const
{
    // Find the first key
    double elapsedTime = getElapsedTime();

    double t = m_animation->duration() ? (elapsedTime / m_animation->duration()) : 1;
    int i = static_cast<int>(t);
    t -= i;
    if (m_animation->direction() && (i & 1))
        t = 1 - t;

    double scale = 1;
    double offset = 0;
    Vector<KeyframeValue>::const_iterator endKeyframes = m_keyframes.endKeyframes();
    for (Vector<KeyframeValue>::const_iterator it = m_keyframes.beginKeyframes(); it != endKeyframes; ++it) {
        if (t < it->key()) {
            // The first key should always be 0, so we should never succeed on the first key
            if (!fromStyle)
                break;
            scale = 1.0 / (it->key() - offset);
            toStyle = it->style();
            break;
        }

        offset = it->key();
        fromStyle = it->style();
    }

    if (!fromStyle || !toStyle)
        return;

    const TimingFunction* timingFunction = 0;
    if (fromStyle->animations() && fromStyle->animations()->size() > 0) {
        // We get the timing function from the first animation, because we've synthesized a RenderStyle for each keyframe.
        timingFunction = &(fromStyle->animations()->animation(0)->timingFunction());
    }

    prog = progress(scale, offset, timingFunction);
}

void KeyframeAnimation::animate(CompositeAnimation*, RenderObject*, const RenderStyle*, RenderStyle* targetStyle, RefPtr<RenderStyle>& animatedStyle)
{
    // Fire the start timeout if needed
    fireAnimationEventsIfNeeded();
    
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
    
    // Get the from/to styles and progress between
    const RenderStyle* fromStyle = 0;
    const RenderStyle* toStyle = 0;
    double progress;
    getKeyframeAnimationInterval(fromStyle, toStyle, progress);

    // If either style is 0 we have an invalid case, just stop the animation.
    if (!fromStyle || !toStyle) {
        updateStateMachine(AnimationStateInputEndAnimation, -1);
        return;
    }

    // Run a cycle of animation.
    // We know we will need a new render style, so make one if needed.
    if (!animatedStyle)
        animatedStyle = RenderStyle::clone(targetStyle);

    HashSet<int>::const_iterator endProperties = m_keyframes.endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes.beginProperties(); it != endProperties; ++it) {
        bool needsAnim = blendProperties(this, *it, animatedStyle.get(), fromStyle, toStyle, progress);
        if (needsAnim)
            setAnimating();
        else {
#if USE(ACCELERATED_COMPOSITING)
            // If we are running an accelerated animation, set a flag in the style
            // to indicate it. This can be used to make sure we get an updated
            // style for hit testing, etc.
            animatedStyle->setIsRunningAcceleratedAnimation();
#endif
        }
    }
}

void KeyframeAnimation::getAnimatedStyle(RefPtr<RenderStyle>& animatedStyle)
{
    // Get the from/to styles and progress between
    const RenderStyle* fromStyle = 0;
    const RenderStyle* toStyle = 0;
    double progress;
    getKeyframeAnimationInterval(fromStyle, toStyle, progress);

    // If either style is 0 we have an invalid case
    if (!fromStyle || !toStyle)
        return;

    if (!animatedStyle)
        animatedStyle = RenderStyle::clone(m_object->style());

    HashSet<int>::const_iterator endProperties = m_keyframes.endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes.beginProperties(); it != endProperties; ++it)
        blendProperties(this, *it, animatedStyle.get(), fromStyle, toStyle, progress);
}

bool KeyframeAnimation::hasAnimationForProperty(int property) const
{
    HashSet<int>::const_iterator end = m_keyframes.endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes.beginProperties(); it != end; ++it) {
        if (*it == property)
            return true;
    }
    
    return false;
}

bool KeyframeAnimation::startAnimation(double timeOffset)
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_object && m_object->hasLayer()) {
        RenderLayer* layer = toRenderBoxModelObject(m_object)->layer();
        if (layer->isComposited())
            return layer->backing()->startAnimation(timeOffset, m_animation.get(), m_keyframes);
    }
#else
    UNUSED_PARAM(timeOffset);
#endif
    return false;
}

void KeyframeAnimation::pauseAnimation(double timeOffset)
{
    if (!m_object)
        return;

#if USE(ACCELERATED_COMPOSITING)
    if (m_object->hasLayer()) {
        RenderLayer* layer = toRenderBoxModelObject(m_object)->layer();
        if (layer->isComposited())
            layer->backing()->animationPaused(timeOffset, m_keyframes.animationName());
    }
#else
    UNUSED_PARAM(timeOffset);
#endif
    // Restore the original (unanimated) style
    if (!paused())
        setNeedsStyleRecalc(m_object->node());
}

void KeyframeAnimation::endAnimation()
{
    if (!m_object)
        return;

#if USE(ACCELERATED_COMPOSITING)
    if (m_object->hasLayer()) {
        RenderLayer* layer = toRenderBoxModelObject(m_object)->layer();
        if (layer->isComposited())
            layer->backing()->animationFinished(m_keyframes.animationName());
    }
#endif
    // Restore the original (unanimated) style
    if (!paused())
        setNeedsStyleRecalc(m_object->node());
}

bool KeyframeAnimation::shouldSendEventForListener(Document::ListenerType listenerType) const
{
    return m_object->document()->hasListenerType(listenerType);
}

void KeyframeAnimation::onAnimationStart(double elapsedTime)
{
    sendAnimationEvent(eventNames().webkitAnimationStartEvent, elapsedTime);
}

void KeyframeAnimation::onAnimationIteration(double elapsedTime)
{
    sendAnimationEvent(eventNames().webkitAnimationIterationEvent, elapsedTime);
}

void KeyframeAnimation::onAnimationEnd(double elapsedTime)
{
    sendAnimationEvent(eventNames().webkitAnimationEndEvent, elapsedTime);
    endAnimation();
}

bool KeyframeAnimation::sendAnimationEvent(const AtomicString& eventType, double elapsedTime)
{
    Document::ListenerType listenerType;
    if (eventType == eventNames().webkitAnimationIterationEvent)
        listenerType = Document::ANIMATIONITERATION_LISTENER;
    else if (eventType == eventNames().webkitAnimationEndEvent)
        listenerType = Document::ANIMATIONEND_LISTENER;
    else {
        ASSERT(eventType == eventNames().webkitAnimationStartEvent);
        listenerType = Document::ANIMATIONSTART_LISTENER;
    }

    if (shouldSendEventForListener(listenerType)) {
        // Dispatch the event
        RefPtr<Element> element;
        if (m_object->node() && m_object->node()->isElementNode())
            element = static_cast<Element*>(m_object->node());

        ASSERT(!element || (element->document() && !element->document()->inPageCache()));
        if (!element)
            return false;

        // Schedule event handling
        m_compAnim->animationController()->addEventToDispatch(element, eventType, m_keyframes.animationName(), elapsedTime);

        // Restore the original (unanimated) style
        if (eventType == eventNames().webkitAnimationEndEvent && element->renderer())
            setNeedsStyleRecalc(element.get());

        return true; // Did dispatch an event
    }

    return false; // Did not dispatch an event
}

void KeyframeAnimation::overrideAnimations()
{
    // This will override implicit animations that match the properties in the keyframe animation
    HashSet<int>::const_iterator end = m_keyframes.endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes.beginProperties(); it != end; ++it)
        compositeAnimation()->overrideImplicitAnimations(*it);
}

void KeyframeAnimation::resumeOverriddenAnimations()
{
    // This will resume overridden implicit animations
    HashSet<int>::const_iterator end = m_keyframes.endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes.beginProperties(); it != end; ++it)
        compositeAnimation()->resumeOverriddenImplicitAnimations(*it);
}

bool KeyframeAnimation::affectsProperty(int property) const
{
    HashSet<int>::const_iterator end = m_keyframes.endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes.beginProperties(); it != end; ++it) {
        if (*it == property)
            return true;
    }
    return false;
}

void KeyframeAnimation::validateTransformFunctionList()
{
    m_transformFunctionListValid = false;
    
    if (m_keyframes.size() < 2 || !m_keyframes.containsProperty(CSSPropertyWebkitTransform))
        return;

    Vector<KeyframeValue>::const_iterator end = m_keyframes.endKeyframes();

    // Empty transforms match anything, so find the first non-empty entry as the reference
    size_t firstIndex = 0;
    Vector<KeyframeValue>::const_iterator firstIt = end;
    
    for (Vector<KeyframeValue>::const_iterator it = m_keyframes.beginKeyframes(); it != end; ++it, ++firstIndex) {
        if (it->style()->transform().operations().size() > 0) {
            firstIt = it;
            break;
        }
    }
    
    if (firstIt == end)
        return;
        
    const TransformOperations* firstVal = &firstIt->style()->transform();
    
    // See if the keyframes are valid
    for (Vector<KeyframeValue>::const_iterator it = firstIt+1; it != end; ++it) {
        const TransformOperations* val = &it->style()->transform();
        
        // A null transform matches anything
        if (val->operations().isEmpty())
            continue;
        
        // If the sizes of the function lists don't match, the lists don't match
        if (firstVal->operations().size() != val->operations().size())
            return;
        
        // If the types of each function are not the same, the lists don't match
        for (size_t j = 0; j < firstVal->operations().size(); ++j) {
            if (!firstVal->operations()[j]->isSameType(*val->operations()[j]))
                return;
        }
    }

    // Keyframes are valid
    m_transformFunctionListValid = true;
}

double KeyframeAnimation::timeToNextService()
{
    double t = AnimationBase::timeToNextService();
#if USE(ACCELERATED_COMPOSITING)
    if (t != 0 || preActive())
        return t;
        
    // A return value of 0 means we need service. But if we only have accelerated animations we 
    // only need service at the end of the transition
    HashSet<int>::const_iterator endProperties = m_keyframes.endProperties();
    bool acceleratedPropertiesOnly = true;
    
    for (HashSet<int>::const_iterator it = m_keyframes.beginProperties(); it != endProperties; ++it) {
        if (!animationOfPropertyIsAccelerated(*it) || isFallbackAnimating()) {
            acceleratedPropertiesOnly = false;
            break;
        }
    }

    if (acceleratedPropertiesOnly) {
        bool isLooping;
        getTimeToNextEvent(t, isLooping);
    }
#endif
    return t;
}

} // namespace WebCore
