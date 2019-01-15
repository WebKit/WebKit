/*
 * Copyright (C) 2007, 2012 Apple Inc. All rights reserved.
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
#include "KeyframeAnimation.h"

#include "CSSAnimationControllerPrivate.h"
#include "CSSPropertyAnimation.h"
#include "CSSPropertyNames.h"
#include "CompositeAnimation.h"
#include "EventNames.h"
#include "GeometryUtilities.h"
#include "RenderBox.h"
#include "RenderStyle.h"
#include "StylePendingResources.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "TranslateTransformOperation.h"
#include "WillChangeData.h"

namespace WebCore {

KeyframeAnimation::KeyframeAnimation(const Animation& animation, Element& element, CompositeAnimation& compositeAnimation, const RenderStyle& unanimatedStyle)
    : AnimationBase(animation, element, compositeAnimation)
    , m_keyframes(animation.name())
    , m_unanimatedStyle(RenderStyle::clonePtr(unanimatedStyle))
{
    resolveKeyframeStyles();

    // Update the m_transformFunctionListValid flag based on whether the function lists in the keyframes match.
    validateTransformFunctionList();
    checkForMatchingFilterFunctionLists();
#if ENABLE(FILTERS_LEVEL_2)
    checkForMatchingBackdropFilterFunctionLists();
#endif
    checkForMatchingColorFilterFunctionLists();

    for (auto propertyID : m_keyframes.properties()) {
        if (WillChangeData::propertyCreatesStackingContext(propertyID))
            m_triggersStackingContext = true;
        
        if (CSSPropertyAnimation::animationOfPropertyIsAccelerated(propertyID))
            m_hasAcceleratedProperty = true;
        
        if (m_triggersStackingContext && m_hasAcceleratedProperty)
            break;
    }

    computeLayoutDependency();
}

KeyframeAnimation::~KeyframeAnimation()
{
    // Make sure to tell the renderer that we are ending. This will make sure any accelerated animations are removed.
    if (!postActive())
        endAnimation();
}

void KeyframeAnimation::computeLayoutDependency()
{
    if (!m_keyframes.containsProperty(CSSPropertyTransform))
        return;

    size_t numKeyframes = m_keyframes.size();
    for (size_t i = 0; i < numKeyframes; i++) {
        auto* keyframeStyle = m_keyframes[i].style();
        if (!keyframeStyle) {
            ASSERT_NOT_REACHED();
            continue;
        }
        if (keyframeStyle->hasTransform()) {
            auto& transformOperations = keyframeStyle->transform();
            for (const auto& operation : transformOperations.operations()) {
                if (operation->isTranslateTransformOperationType()) {
                    auto translation = downcast<TranslateTransformOperation>(operation.get());
                    if (translation->x().isPercent() || translation->y().isPercent()) {
                        m_dependsOnLayout = true;
                        return;
                    }
                }
            }
        }
    }
}

void KeyframeAnimation::fetchIntervalEndpointsForProperty(CSSPropertyID property, const RenderStyle*& fromStyle, const RenderStyle*& toStyle, double& prog) const
{
    size_t numKeyframes = m_keyframes.size();
    if (!numKeyframes)
        return;

    // Find the first key
    double elapsedTime = getElapsedTime();
    if (m_animation->duration() && m_animation->iterationCount() != Animation::IterationCountInfinite)
        elapsedTime = std::min(elapsedTime, m_animation->duration() * m_animation->iterationCount());

    const double fractionalTime = this->fractionalTime(1, elapsedTime, 0);
    ASSERT(!m_keyframes[0].key());
    ASSERT(m_keyframes[m_keyframes.size() - 1].key() == 1);

    int prevIndex = -1;
    int nextIndex = -1;
    // FIXME: with a lot of keys, this linear search will be slow. We could binary search.
    for (size_t i = 0; i < numKeyframes; ++i) {
        const KeyframeValue& currKeyFrame = m_keyframes[i];

        if (!currKeyFrame.containsProperty(property))
            continue;

        if (fractionalTime < currKeyFrame.key()) {
            nextIndex = i;
            break;
        }
        prevIndex = i;
    }

    if (prevIndex == -1)
        prevIndex = 0;
    if (nextIndex == -1)
        nextIndex = m_keyframes.size() - 1;

    const KeyframeValue& prevKeyframe = m_keyframes[prevIndex];
    const KeyframeValue& nextKeyframe = m_keyframes[nextIndex];

    fromStyle = prevKeyframe.style();
    toStyle = nextKeyframe.style();

    double offset = prevKeyframe.key();
    double scale = 1.0 / (nextIndex == prevIndex ?  1 : (nextKeyframe.key() - prevKeyframe.key()));

    prog = progress(scale, offset, prevKeyframe.timingFunction());
}

OptionSet<AnimateChange> KeyframeAnimation::animate(CompositeAnimation& compositeAnimation, const RenderStyle& targetStyle, std::unique_ptr<RenderStyle>& animatedStyle)
{
    AnimationState oldState = state();

    // Update state and fire the start timeout if needed (FIXME: this function needs a better name).
    fireAnimationEventsIfNeeded();

    // If we have not yet started, we will not have a valid start time, so just start the animation if needed.
    if (isNew()) {
        if (m_animation->playState() == AnimationPlayState::Playing && !compositeAnimation.isSuspended())
            updateStateMachine(AnimationStateInput::StartAnimation, -1);
        else if (m_animation->playState() == AnimationPlayState::Paused)
            updateStateMachine(AnimationStateInput::PlayStatePaused, -1);
    }

    // If we get this far and the animation is done, it means we are cleaning up a just-finished animation.
    // If so, we need to send back the targetStyle.
    if (postActive()) {
        if (!animatedStyle)
            animatedStyle = RenderStyle::clonePtr(targetStyle);
        return { };
    }

    // If we are waiting for the start timer, we don't want to change the style yet.
    // Special case 1 - if the delay time is 0, then we do want to set the first frame of the
    // animation right away. This avoids a flash when the animation starts.
    // Special case 2 - if there is a backwards fill mode, then we want to continue
    // through to the style blend so that we get the fromStyle.
    if (waitingToStart() && m_animation->delay() > 0 && !m_animation->fillsBackwards())
        return { };
    
    // If we have no keyframes, don't animate.
    if (!m_keyframes.size()) {
        updateStateMachine(AnimationStateInput::EndAnimation, -1);
        return { };
    }

    // Run a cycle of animation.
    // We know we will need a new render style, so make one if needed.
    if (!animatedStyle)
        animatedStyle = RenderStyle::clonePtr(targetStyle);

    // FIXME: we need to be more efficient about determining which keyframes we are animating between.
    // We should cache the last pair or something.
    for (auto propertyID : m_keyframes.properties()) {
        // Get the from/to styles and progress between
        const RenderStyle* fromStyle = nullptr;
        const RenderStyle* toStyle = nullptr;
        double progress = 0;
        fetchIntervalEndpointsForProperty(propertyID, fromStyle, toStyle, progress);

        CSSPropertyAnimation::blendProperties(this, propertyID, animatedStyle.get(), fromStyle, toStyle, progress);
    }

    OptionSet<AnimateChange> change(AnimateChange::StyleBlended);
    if (state() != oldState)
        change.add(AnimateChange::StateChange);

    if ((isPausedState(oldState) || isRunningState(oldState)) != (isPausedState(state()) || isRunningState(state())))
        change.add(AnimateChange::RunningStateChange);

    return change;
}

void KeyframeAnimation::getAnimatedStyle(std::unique_ptr<RenderStyle>& animatedStyle)
{
    // If we're done, or in the delay phase and we're not backwards filling, tell the caller to use the current style.
    if (postActive() || (waitingToStart() && m_animation->delay() > 0 && !m_animation->fillsBackwards()))
        return;

    if (!m_keyframes.size())
        return;

    if (!animatedStyle)
        animatedStyle = RenderStyle::clonePtr(renderer()->style());

    for (auto propertyID : m_keyframes.properties()) {
        // Get the from/to styles and progress between
        const RenderStyle* fromStyle = nullptr;
        const RenderStyle* toStyle = nullptr;
        double progress = 0;
        fetchIntervalEndpointsForProperty(propertyID, fromStyle, toStyle, progress);

        CSSPropertyAnimation::blendProperties(this, propertyID, animatedStyle.get(), fromStyle, toStyle, progress);
    }
}

bool KeyframeAnimation::computeExtentOfTransformAnimation(LayoutRect& bounds) const
{
    ASSERT(m_keyframes.containsProperty(CSSPropertyTransform));

    if (!is<RenderBox>(renderer()))
        return true; // Non-boxes don't get transformed;

    auto& box = downcast<RenderBox>(*renderer());
    auto rendererBox = snapRectToDevicePixels(box.borderBoxRect(), box.document().deviceScaleFactor());

    auto cumulativeBounds = bounds;

    for (auto& keyframe : m_keyframes.keyframes()) {
        const RenderStyle* keyframeStyle = keyframe.style();

        if (!keyframe.containsProperty(CSSPropertyTransform)) {
            // If the first keyframe is missing transform style, use the current style.
            if (!keyframe.key())
                keyframeStyle = &box.style();
            else
                continue;
        }

        auto keyframeBounds = bounds;
        
        bool canCompute;
        if (transformFunctionListsMatch())
            canCompute = computeTransformedExtentViaTransformList(rendererBox, *keyframeStyle, keyframeBounds);
        else
            canCompute = computeTransformedExtentViaMatrix(rendererBox, *keyframeStyle, keyframeBounds);
        
        if (!canCompute)
            return false;
        
        cumulativeBounds.unite(keyframeBounds);
    }

    bounds = cumulativeBounds;
    return true;
}

bool KeyframeAnimation::hasAnimationForProperty(CSSPropertyID property) const
{
    return m_keyframes.containsProperty(property);
}

bool KeyframeAnimation::startAnimation(double timeOffset)
{
    if (auto* renderer = compositedRenderer())
        return renderer->startAnimation(timeOffset, m_animation.ptr(), m_keyframes);
    return false;
}

void KeyframeAnimation::pauseAnimation(double timeOffset)
{
    if (!element())
        return;

    if (auto* renderer = compositedRenderer())
        renderer->animationPaused(timeOffset, m_keyframes.animationName());

    // Restore the original (unanimated) style
    if (!paused())
        setNeedsStyleRecalc(element());
}

void KeyframeAnimation::endAnimation(bool fillingForwards)
{
    if (!element())
        return;

    if (auto* renderer = compositedRenderer())
        renderer->animationFinished(m_keyframes.animationName());

    // Restore the original (unanimated) style
    if (!fillingForwards && !paused())
        setNeedsStyleRecalc(element());
}

bool KeyframeAnimation::shouldSendEventForListener(Document::ListenerType listenerType) const
{
    return element()->document().hasListenerType(listenerType);
}

void KeyframeAnimation::onAnimationStart(double elapsedTime)
{
    sendAnimationEvent(eventNames().animationstartEvent, elapsedTime);
}

void KeyframeAnimation::onAnimationIteration(double elapsedTime)
{
    sendAnimationEvent(eventNames().animationiterationEvent, elapsedTime);
}

void KeyframeAnimation::onAnimationEnd(double elapsedTime)
{
    sendAnimationEvent(eventNames().animationendEvent, elapsedTime);
    endAnimation(m_animation->fillsForwards());
}

bool KeyframeAnimation::sendAnimationEvent(const AtomicString& eventType, double elapsedTime)
{
    Document::ListenerType listenerType;
    if (eventType == eventNames().webkitAnimationIterationEvent || eventType == eventNames().animationiterationEvent)
        listenerType = Document::ANIMATIONITERATION_LISTENER;
    else if (eventType == eventNames().webkitAnimationEndEvent || eventType == eventNames().animationendEvent)
        listenerType = Document::ANIMATIONEND_LISTENER;
    else {
        ASSERT(eventType == eventNames().webkitAnimationStartEvent || eventType == eventNames().animationstartEvent);
        if (m_startEventDispatched)
            return false;
        m_startEventDispatched = true;
        listenerType = Document::ANIMATIONSTART_LISTENER;
    }

    if (shouldSendEventForListener(listenerType)) {
        // Dispatch the event
        auto element = makeRefPtr(this->element());

        ASSERT(!element || element->document().pageCacheState() == Document::NotInPageCache);
        if (!element)
            return false;

        // Schedule event handling
        m_compositeAnimation->animationController().addEventToDispatch(*element, eventType, m_keyframes.animationName(), elapsedTime);

        // Restore the original (unanimated) style
        if ((eventType == eventNames().webkitAnimationEndEvent || eventType == eventNames().animationendEvent) && element->renderer())
            setNeedsStyleRecalc(element.get());

        return true; // Did dispatch an event
    }

    return false; // Did not dispatch an event
}

void KeyframeAnimation::overrideAnimations()
{
    // This will override implicit animations that match the properties in the keyframe animation
    for (auto propertyID : m_keyframes.properties())
        compositeAnimation()->overrideImplicitAnimations(propertyID);
}

void KeyframeAnimation::resumeOverriddenAnimations()
{
    // This will resume overridden implicit animations
    for (auto propertyID : m_keyframes.properties())
        compositeAnimation()->resumeOverriddenImplicitAnimations(propertyID);
}

bool KeyframeAnimation::affectsProperty(CSSPropertyID property) const
{
    return m_keyframes.containsProperty(property);
}

void KeyframeAnimation::resolveKeyframeStyles()
{
    if (!element())
        return;

    if (auto* styleScope = Style::Scope::forOrdinal(*element(), m_animation->nameStyleScopeOrdinal()))
        styleScope->resolver().keyframeStylesForAnimation(*element(), m_unanimatedStyle.get(), m_keyframes);

    // Ensure resource loads for all the frames.
    for (auto& keyframe : m_keyframes.keyframes()) {
        if (auto* style = const_cast<RenderStyle*>(keyframe.style()))
            Style::loadPendingResources(*style, element()->document(), element());
    }
}

void KeyframeAnimation::validateTransformFunctionList()
{
    m_transformFunctionListsMatch = false;
    
    if (m_keyframes.size() < 2 || !m_keyframes.containsProperty(CSSPropertyTransform))
        return;

    // Empty transforms match anything, so find the first non-empty entry as the reference
    size_t numKeyframes = m_keyframes.size();
    size_t firstNonEmptyTransformKeyframeIndex = numKeyframes;

    for (size_t i = 0; i < numKeyframes; ++i) {
        const KeyframeValue& currentKeyframe = m_keyframes[i];
        if (currentKeyframe.style()->transform().operations().size()) {
            firstNonEmptyTransformKeyframeIndex = i;
            break;
        }
    }
    
    if (firstNonEmptyTransformKeyframeIndex == numKeyframes)
        return;
        
    const TransformOperations* firstVal = &m_keyframes[firstNonEmptyTransformKeyframeIndex].style()->transform();
    
    // See if the keyframes are valid
    for (size_t i = firstNonEmptyTransformKeyframeIndex + 1; i < numKeyframes; ++i) {
        const KeyframeValue& currentKeyframe = m_keyframes[i];
        const TransformOperations* val = &currentKeyframe.style()->transform();
        
        // An emtpy transform list matches anything.
        if (val->operations().isEmpty())
            continue;
        
        if (!firstVal->operationsMatch(*val))
            return;
    }

    m_transformFunctionListsMatch = true;
}

bool KeyframeAnimation::checkForMatchingFilterFunctionLists(CSSPropertyID propertyID, const std::function<const FilterOperations& (const RenderStyle&)>& filtersGetter) const
{
    if (m_keyframes.size() < 2 || !m_keyframes.containsProperty(propertyID))
        return false;

    // Empty filters match anything, so find the first non-empty entry as the reference
    size_t numKeyframes = m_keyframes.size();
    size_t firstNonEmptyKeyframeIndex = numKeyframes;

    for (size_t i = 0; i < numKeyframes; ++i) {
        if (filtersGetter(*m_keyframes[i].style()).operations().size()) {
            firstNonEmptyKeyframeIndex = i;
            break;
        }
    }
    
    if (firstNonEmptyKeyframeIndex == numKeyframes)
        return false;

    auto& firstVal = filtersGetter(*m_keyframes[firstNonEmptyKeyframeIndex].style());
    
    for (size_t i = firstNonEmptyKeyframeIndex + 1; i < numKeyframes; ++i) {
        auto& value = filtersGetter(*m_keyframes[i].style());

        // An emtpy filter list matches anything.
        if (value.operations().isEmpty())
            continue;

        if (!firstVal.operationsMatch(value))
            return false;
    }

    return true;
}

void KeyframeAnimation::checkForMatchingFilterFunctionLists()
{
    m_filterFunctionListsMatch = checkForMatchingFilterFunctionLists(CSSPropertyFilter, [] (const RenderStyle& style) -> const FilterOperations& {
        return style.filter();
    });
}

#if ENABLE(FILTERS_LEVEL_2)
void KeyframeAnimation::checkForMatchingBackdropFilterFunctionLists()
{
    m_backdropFilterFunctionListsMatch = checkForMatchingFilterFunctionLists(CSSPropertyWebkitBackdropFilter, [] (const RenderStyle& style) -> const FilterOperations& {
        return style.backdropFilter();
    });
}
#endif

void KeyframeAnimation::checkForMatchingColorFilterFunctionLists()
{
    m_colorFilterFunctionListsMatch = checkForMatchingFilterFunctionLists(CSSPropertyAppleColorFilter, [] (const RenderStyle& style) -> const FilterOperations& {
        return style.appleColorFilter();
    });
}

Optional<Seconds> KeyframeAnimation::timeToNextService()
{
    Optional<Seconds> t = AnimationBase::timeToNextService();
    if (!t || t.value() != 0_s || preActive())
        return t;

    // A return value of 0 means we need service. But if we only have accelerated animations we 
    // only need service at the end of the transition.
    bool acceleratedPropertiesOnly = true;
    
    for (auto propertyID : m_keyframes.properties()) {
        if (!CSSPropertyAnimation::animationOfPropertyIsAccelerated(propertyID) || !isAccelerated()) {
            acceleratedPropertiesOnly = false;
            break;
        }
    }

    if (acceleratedPropertiesOnly) {
        bool isLooping;
        getTimeToNextEvent(t.value(), isLooping);
    }

    return t;
}

} // namespace WebCore
