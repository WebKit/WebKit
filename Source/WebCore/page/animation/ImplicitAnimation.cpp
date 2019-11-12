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

#include "config.h"
#include "ImplicitAnimation.h"

#include "CSSAnimationControllerPrivate.h"
#include "CSSPropertyAnimation.h"
#include "CompositeAnimation.h"
#if PLATFORM(IOS_FAMILY)
#include "ContentChangeObserver.h"
#endif
#include "EventNames.h"
#include "GeometryUtilities.h"
#include "KeyframeAnimation.h"
#include "RenderBox.h"
#include "StylePendingResources.h"

namespace WebCore {

ImplicitAnimation::ImplicitAnimation(const Animation& transition, CSSPropertyID animatingProperty, Element& element, CompositeAnimation& compositeAnimation, const RenderStyle& fromStyle)
    : AnimationBase(transition, element, compositeAnimation)
    , m_fromStyle(RenderStyle::clonePtr(fromStyle))
    , m_transitionProperty(transition.property())
    , m_animatingProperty(animatingProperty)
{
#if PLATFORM(IOS_FAMILY)
    element.document().contentChangeObserver().didAddTransition(element, transition);
#endif
    ASSERT(animatingProperty != CSSPropertyInvalid);
}

ImplicitAnimation::~ImplicitAnimation()
{
#if PLATFORM(IOS_FAMILY)
    if (auto* element = this->element())
        element->document().contentChangeObserver().didRemoveTransition(*element, m_animatingProperty);
#endif
    // // Make sure to tell the renderer that we are ending. This will make sure any accelerated animations are removed.
    if (!postActive())
        endAnimation();
}

bool ImplicitAnimation::shouldSendEventForListener(Document::ListenerType inListenerType) const
{
    return element()->document().hasListenerType(inListenerType);
}

OptionSet<AnimateChange> ImplicitAnimation::animate(CompositeAnimation& compositeAnimation, const RenderStyle& targetStyle, std::unique_ptr<RenderStyle>& animatedStyle)
{
    // If we get this far and the animation is done, it means we are cleaning up a just finished animation.
    // So just return. Everything is already all cleaned up.
    if (postActive())
        return { };

    AnimationState oldState = state();

    // Reset to start the transition if we are new
    if (isNew())
        reset(targetStyle, compositeAnimation);

    // Run a cycle of animation.
    // We know we will need a new render style, so make one if needed
    if (!animatedStyle)
        animatedStyle = RenderStyle::clonePtr(targetStyle);

    CSSPropertyAnimation::blendProperties(this, m_animatingProperty, animatedStyle.get(), m_fromStyle.get(), m_toStyle.get(), progress());
    // FIXME: we also need to detect cases where we have to software animate for other reasons,
    // such as a child using inheriting the transform. https://bugs.webkit.org/show_bug.cgi?id=23902

    // Fire the start timeout if needed
    fireAnimationEventsIfNeeded();

    OptionSet<AnimateChange> change(AnimateChange::StyleBlended);
    if (state() != oldState)
        change.add(AnimateChange::StateChange);

    if ((isPausedState(oldState) || isRunningState(oldState)) != (isPausedState(state()) || isRunningState(state())))
        change.add(AnimateChange::RunningStateChange);

    return change;
}

void ImplicitAnimation::getAnimatedStyle(std::unique_ptr<RenderStyle>& animatedStyle)
{
    if (!animatedStyle)
        animatedStyle = RenderStyle::clonePtr(*m_toStyle);

    CSSPropertyAnimation::blendProperties(this, m_animatingProperty, animatedStyle.get(), m_fromStyle.get(), m_toStyle.get(), progress());
}

bool ImplicitAnimation::computeExtentOfTransformAnimation(LayoutRect& bounds) const
{
    ASSERT(hasStyle());

    if (!is<RenderBox>(renderer()))
        return true; // Non-boxes don't get transformed;

    ASSERT(m_animatingProperty == CSSPropertyTransform);

    RenderBox& box = downcast<RenderBox>(*renderer());
    FloatRect rendererBox = snapRectToDevicePixels(box.borderBoxRect(), box.document().deviceScaleFactor());

    LayoutRect startBounds = bounds;
    LayoutRect endBounds = bounds;

    if (transformFunctionListsMatch()) {
        if (!computeTransformedExtentViaTransformList(rendererBox, *m_fromStyle, startBounds))
            return false;

        if (!computeTransformedExtentViaTransformList(rendererBox, *m_toStyle, endBounds))
            return false;
    } else {
        if (!computeTransformedExtentViaMatrix(rendererBox, *m_fromStyle, startBounds))
            return false;

        if (!computeTransformedExtentViaMatrix(rendererBox, *m_toStyle, endBounds))
            return false;
    }

    bounds = unionRect(startBounds, endBounds);
    return true;
}

bool ImplicitAnimation::affectsAcceleratedProperty() const
{
    return CSSPropertyAnimation::animationOfPropertyIsAccelerated(m_animatingProperty);
}

bool ImplicitAnimation::startAnimation(double timeOffset)
{
    if (auto* renderer = this->renderer())
        return renderer->startTransition(timeOffset, m_animatingProperty, m_fromStyle.get(), m_toStyle.get());
    return false;
}

void ImplicitAnimation::pauseAnimation(double timeOffset)
{
    if (auto* renderer = this->renderer())
        renderer->transitionPaused(timeOffset, m_animatingProperty);
    // Restore the original (unanimated) style
    if (!paused())
        setNeedsStyleRecalc(element());
}

void ImplicitAnimation::clear()
{
#if PLATFORM(IOS_FAMILY)
    if (auto* element = this->element())
        element->document().contentChangeObserver().didRemoveTransition(*element, m_animatingProperty);
#endif
    AnimationBase::clear();
}

void ImplicitAnimation::endAnimation(bool)
{
    if (auto* renderer = this->renderer())
        renderer->transitionFinished(m_animatingProperty);
}

void ImplicitAnimation::onAnimationEnd(double elapsedTime)
{
#if PLATFORM(IOS_FAMILY)
    if (auto* element = this->element())
        element->document().contentChangeObserver().didFinishTransition(*element, m_animatingProperty);
#endif
    // If we have a keyframe animation on this property, this transition is being overridden. The keyframe
    // animation keeps an unanimated style in case a transition starts while the keyframe animation is
    // running. But now that the transition has completed, we need to update this style with its new
    // destination. If we didn't, the next time through we would think a transition had started
    // (comparing the old unanimated style with the new final style of the transition).
    if (auto* animation = m_compositeAnimation->animationForProperty(m_animatingProperty))
        animation->setUnanimatedStyle(RenderStyle::clonePtr(*m_toStyle));

    sendTransitionEvent(eventNames().transitionendEvent, elapsedTime);
    endAnimation();
}

bool ImplicitAnimation::sendTransitionEvent(const AtomString& eventType, double elapsedTime)
{
    if (eventType == eventNames().transitionendEvent) {
        Document::ListenerType listenerType = Document::TRANSITIONEND_LISTENER;

        if (shouldSendEventForListener(listenerType)) {
            String propertyName = getPropertyNameString(m_animatingProperty);
                
            // Dispatch the event
            auto element = makeRefPtr(this->element());

            ASSERT(!element || element->document().backForwardCacheState() == Document::NotInBackForwardCache);
            if (!element)
                return false;

            // Schedule event handling
            m_compositeAnimation->animationController().addEventToDispatch(*element, eventType, propertyName, elapsedTime);

            // Restore the original (unanimated) style
            if (eventType == eventNames().transitionendEvent && element->renderer())
                setNeedsStyleRecalc(element.get());

            return true; // Did dispatch an event
        }
    }

    return false; // Didn't dispatch an event
}

void ImplicitAnimation::reset(const RenderStyle& to, CompositeAnimation& compositeAnimation)
{
    ASSERT(m_fromStyle);

    m_toStyle = RenderStyle::clonePtr(to);

    if (element())
        Style::loadPendingResources(*m_toStyle, element()->document(), element());

    // Restart the transition.
    if (m_fromStyle && m_toStyle && !compositeAnimation.isSuspended())
        updateStateMachine(AnimationStateInput::RestartAnimation, -1);

    // Set the transform animation list.
    validateTransformFunctionList();
    checkForMatchingFilterFunctionLists();
#if ENABLE(FILTERS_LEVEL_2)
    checkForMatchingBackdropFilterFunctionLists();
#endif
    checkForMatchingColorFilterFunctionLists();
}

void ImplicitAnimation::setOverridden(bool b)
{
    if (b == m_overridden)
        return;

    m_overridden = b;
    updateStateMachine(m_overridden ? AnimationStateInput::PauseOverride : AnimationStateInput::ResumeOverride, -1);
}

bool ImplicitAnimation::affectsProperty(CSSPropertyID property) const
{
    return (m_animatingProperty == property);
}

bool ImplicitAnimation::isTargetPropertyEqual(CSSPropertyID prop, const RenderStyle* targetStyle)
{
    // We can get here for a transition that has not started yet. This would make m_toStyle unset and null. 
    // So we check that here (see <https://bugs.webkit.org/show_bug.cgi?id=26706>)
    if (!m_toStyle)
        return false;
    return CSSPropertyAnimation::propertiesEqual(prop, m_toStyle.get(), targetStyle);
}

void ImplicitAnimation::blendPropertyValueInStyle(CSSPropertyID prop, RenderStyle* currentStyle)
{
    // We should never add a transition with a 0 duration and delay. But if we ever did
    // it would have a null toStyle. So just in case, let's check that here. (See
    // <https://bugs.webkit.org/show_bug.cgi?id=24787>
    if (!m_toStyle)
        return;
        
    CSSPropertyAnimation::blendProperties(this, prop, currentStyle, m_fromStyle.get(), m_toStyle.get(), progress());
}

void ImplicitAnimation::validateTransformFunctionList()
{
    m_transformFunctionListsMatch = false;
    
    if (!m_fromStyle || !m_toStyle)
        return;
        
    const TransformOperations* val = &m_fromStyle->transform();
    const TransformOperations* toVal = &m_toStyle->transform();

    if (val->operations().isEmpty())
        val = toVal;

    if (val->operations().isEmpty())
        return;
        
    // An empty transform list matches anything.
    if (val != toVal && !toVal->operations().isEmpty() && !val->operationsMatch(*toVal))
        return;

    // Transform lists match.
    m_transformFunctionListsMatch = true;
}

static bool filterOperationsMatch(const FilterOperations* fromOperations, const FilterOperations& toOperations)
{
    if (fromOperations->operations().isEmpty())
        fromOperations = &toOperations;

    if (fromOperations->operations().isEmpty())
        return false;

    if (fromOperations != &toOperations && !toOperations.operations().isEmpty() && !fromOperations->operationsMatch(toOperations))
        return false;

    return true;
}

void ImplicitAnimation::checkForMatchingFilterFunctionLists()
{
    m_filterFunctionListsMatch = false;

    if (!m_fromStyle || !m_toStyle)
        return;

    m_filterFunctionListsMatch = filterOperationsMatch(&m_fromStyle->filter(), m_toStyle->filter());
}

#if ENABLE(FILTERS_LEVEL_2)
void ImplicitAnimation::checkForMatchingBackdropFilterFunctionLists()
{
    m_backdropFilterFunctionListsMatch = false;

    if (!m_fromStyle || !m_toStyle)
        return;

    m_backdropFilterFunctionListsMatch = filterOperationsMatch(&m_fromStyle->backdropFilter(), m_toStyle->backdropFilter());
}
#endif

void ImplicitAnimation::checkForMatchingColorFilterFunctionLists()
{
    m_filterFunctionListsMatch = false;

    if (!m_fromStyle || !m_toStyle)
        return;

    m_colorFilterFunctionListsMatch = filterOperationsMatch(&m_fromStyle->appleColorFilter(), m_toStyle->appleColorFilter());
}

Optional<Seconds> ImplicitAnimation::timeToNextService()
{
    Optional<Seconds> t = AnimationBase::timeToNextService();
    if (!t || t.value() != 0_s || preActive())
#if COMPILER(MSVC) && _MSC_VER >= 1920
        return WTFMove(t);
#else
        return t;
#endif

    // A return value of 0 means we need service. But if this is an accelerated animation we 
    // only need service at the end of the transition.
    if (CSSPropertyAnimation::animationOfPropertyIsAccelerated(m_animatingProperty) && isAccelerated()) {
        bool isLooping;
        getTimeToNextEvent(t.value(), isLooping);
    }
    return t;
}

} // namespace WebCore
