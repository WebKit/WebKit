/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AcceleratedEffect.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include "AnimationEffect.h"
#include "BlendingKeyframes.h"
#include "CSSPropertyAnimation.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "FilterOperations.h"
#include "FloatRect.h"
#include "KeyframeEffect.h"
#include "LayoutSize.h"
#include "OffsetRotation.h"
#include "StyleOriginatedAnimation.h"
#include "WebAnimation.h"
#include "WebAnimationTypes.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

AcceleratedEffect::Keyframe::Keyframe(double offset, AcceleratedEffectValues&& values)
    : m_offset(offset)
    , m_values(WTFMove(values))
{
}

AcceleratedEffect::Keyframe::Keyframe(double offset, AcceleratedEffectValues&& values, RefPtr<TimingFunction>&& timingFunction, std::optional<CompositeOperation> compositeOperation, OptionSet<AcceleratedEffectProperty>&& animatedProperties)
    : m_offset(offset)
    , m_values(WTFMove(values))
    , m_timingFunction(WTFMove(timingFunction))
    , m_compositeOperation(compositeOperation)
    , m_animatedProperties(WTFMove(animatedProperties))
{
}

void AcceleratedEffect::Keyframe::clearProperty(AcceleratedEffectProperty property)
{
    m_animatedProperties.remove({ property });
}

bool AcceleratedEffect::Keyframe::animatesProperty(KeyframeInterpolation::Property property) const
{
    return WTF::switchOn(property, [&](const AcceleratedEffectProperty acceleratedProperty) {
        return m_animatedProperties.contains(acceleratedProperty);
    }, [] (auto&) {
        ASSERT_NOT_REACHED();
        return false;
    });
}

AcceleratedEffect::Keyframe AcceleratedEffect::Keyframe::clone() const
{
    auto clonedAnimatedProperties = m_animatedProperties;

    RefPtr<TimingFunction> clonedTimingFunction;
    if (auto* srcTimingFunction = m_timingFunction.get())
        clonedTimingFunction = srcTimingFunction->clone();

    return {
        m_offset,
        m_values.clone(),
        WTFMove(clonedTimingFunction),
        m_compositeOperation,
        WTFMove(clonedAnimatedProperties)
    };
}

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(AcceleratedEffect);

static AcceleratedEffectProperty acceleratedPropertyFromCSSProperty(AnimatableCSSProperty property, const Settings& settings)
{
#if ASSERT_ENABLED
    ASSERT(CSSPropertyAnimation::animationOfPropertyIsAccelerated(property, settings));
#else
    UNUSED_PARAM(settings);
#endif
    ASSERT(std::holds_alternative<CSSPropertyID>(property));

    switch (std::get<CSSPropertyID>(property)) {
    case CSSPropertyOpacity:
        return AcceleratedEffectProperty::Opacity;
    case CSSPropertyTransform:
        return AcceleratedEffectProperty::Transform;
    case CSSPropertyTranslate:
        return AcceleratedEffectProperty::Translate;
    case CSSPropertyRotate:
        return AcceleratedEffectProperty::Rotate;
    case CSSPropertyScale:
        return AcceleratedEffectProperty::Scale;
    case CSSPropertyOffsetPath:
        return AcceleratedEffectProperty::OffsetPath;
    case CSSPropertyOffsetDistance:
        return AcceleratedEffectProperty::OffsetDistance;
    case CSSPropertyOffsetPosition:
        return AcceleratedEffectProperty::OffsetPosition;
    case CSSPropertyOffsetAnchor:
        return AcceleratedEffectProperty::OffsetAnchor;
    case CSSPropertyOffsetRotate:
        return AcceleratedEffectProperty::OffsetRotate;
    case CSSPropertyFilter:
        return AcceleratedEffectProperty::Filter;
    case CSSPropertyBackdropFilter:
    case CSSPropertyWebkitBackdropFilter:
        return AcceleratedEffectProperty::BackdropFilter;
    default:
        ASSERT_NOT_REACHED();
        return AcceleratedEffectProperty::Invalid;
    }
}

static CSSPropertyID cssPropertyFromAcceleratedProperty(AcceleratedEffectProperty property)
{
    switch (property) {
    case AcceleratedEffectProperty::Opacity:
        return CSSPropertyOpacity;
    case AcceleratedEffectProperty::Transform:
        return CSSPropertyTransform;
    case AcceleratedEffectProperty::Translate:
        return CSSPropertyTranslate;
    case AcceleratedEffectProperty::Rotate:
        return CSSPropertyRotate;
    case AcceleratedEffectProperty::Scale:
        return CSSPropertyScale;
    case AcceleratedEffectProperty::OffsetPath:
        return CSSPropertyOffsetPath;
    case AcceleratedEffectProperty::OffsetDistance:
        return CSSPropertyOffsetDistance;
    case AcceleratedEffectProperty::OffsetPosition:
        return CSSPropertyOffsetPosition;
    case AcceleratedEffectProperty::OffsetAnchor:
        return CSSPropertyOffsetAnchor;
    case AcceleratedEffectProperty::OffsetRotate:
        return CSSPropertyOffsetRotate;
    case AcceleratedEffectProperty::Filter:
        return CSSPropertyFilter;
    case AcceleratedEffectProperty::BackdropFilter:
        return CSSPropertyWebkitBackdropFilter;
    default:
        ASSERT_NOT_REACHED();
        return CSSPropertyInvalid;
    }
}

RefPtr<AcceleratedEffect> AcceleratedEffect::create(const KeyframeEffect& effect, const IntRect& borderBoxRect, const AcceleratedEffectValues& baseValues, OptionSet<AcceleratedEffectProperty>& disallowedProperties)
{
    auto* acceleratedEffect = new AcceleratedEffect(effect, borderBoxRect, disallowedProperties);
    acceleratedEffect->validateFilters(baseValues, disallowedProperties);
    if (acceleratedEffect->animatedProperties().isEmpty())
        return nullptr;
    return adoptRef(*acceleratedEffect);
}

Ref<AcceleratedEffect> AcceleratedEffect::create(AnimationEffectTiming timing, Vector<Keyframe>&& keyframes, WebAnimationType type, CompositeOperation composite, RefPtr<TimingFunction>&& defaultKeyframeTimingFunction, OptionSet<WebCore::AcceleratedEffectProperty>&& animatedProperties, bool paused, double playbackRate, std::optional<WebAnimationTime> startTime, std::optional<WebAnimationTime> holdTime)
{
    return adoptRef(*new AcceleratedEffect(WTFMove(timing), WTFMove(keyframes), type, composite, WTFMove(defaultKeyframeTimingFunction), WTFMove(animatedProperties), paused, playbackRate, startTime, holdTime));
}

Ref<AcceleratedEffect> AcceleratedEffect::clone() const
{
    auto clonedKeyframes = m_keyframes.map([](const auto& keyframe) {
        return keyframe.clone();
    });

    RefPtr<TimingFunction> clonedDefaultKeyframeTimingFunction;
    if (auto* defaultKeyframeTimingFunction = m_defaultKeyframeTimingFunction.get())
        clonedDefaultKeyframeTimingFunction = defaultKeyframeTimingFunction->clone();

    auto clonedAnimatedProperties = m_animatedProperties;

    return AcceleratedEffect::create(m_timing, WTFMove(clonedKeyframes), m_animationType, m_compositeOperation, WTFMove(clonedDefaultKeyframeTimingFunction), WTFMove(clonedAnimatedProperties), m_paused, m_playbackRate, m_startTime, m_holdTime);
}

Ref<AcceleratedEffect> AcceleratedEffect::copyWithProperties(OptionSet<AcceleratedEffectProperty>& propertyFilter) const
{
    return adoptRef(*new AcceleratedEffect(*this, propertyFilter));
}

AcceleratedEffect::AcceleratedEffect(const KeyframeEffect& effect, const IntRect& borderBoxRect, const OptionSet<AcceleratedEffectProperty>& disallowedProperties)
{
    m_timing = effect.timing();
    m_compositeOperation = effect.composite();
    m_animationType = effect.animationType();

    ASSERT(effect.animation());
    if (auto* animation = effect.animation()) {
        m_paused = animation->playState() == WebAnimation::PlayState::Paused;
        m_playbackRate = animation->playbackRate();
        ASSERT(animation->holdTime() || animation->startTime());
        m_holdTime = animation->holdTime() ? animation->holdTime()->time() : std::nullopt;
        m_startTime = animation->startTime() ? animation->startTime()->time() : std::nullopt;
        if (auto* styleAnimation = dynamicDowncast<StyleOriginatedAnimation>(*animation)) {
            if (auto* defaultKeyframeTimingFunction = styleAnimation->backingAnimation().timingFunction())
                m_defaultKeyframeTimingFunction = defaultKeyframeTimingFunction;
        }
    }

    ASSERT(effect.document());
    auto& settings = effect.document()->settings();

    for (auto& srcKeyframe : effect.blendingKeyframes()) {
        OptionSet<AcceleratedEffectProperty> animatedProperties;
        for (auto animatedCSSProperty : srcKeyframe.properties()) {
            if (CSSPropertyAnimation::animationOfPropertyIsAccelerated(animatedCSSProperty, settings)) {
                auto acceleratedProperty = acceleratedPropertyFromCSSProperty(animatedCSSProperty, settings);
                if (disallowedProperties.contains(acceleratedProperty))
                    continue;
                animatedProperties.add(acceleratedProperty);
                m_animatedProperties.add(acceleratedProperty);
            }
        }

        if (animatedProperties.isEmpty())
            continue;

        auto values = [&]() -> AcceleratedEffectValues {
            if (auto* style = srcKeyframe.style())
                return { *style, borderBoxRect };
            return { };
        }();

        m_keyframes.append({ srcKeyframe.offset(), WTFMove(values), srcKeyframe.timingFunction(), srcKeyframe.compositeOperation(), WTFMove(animatedProperties) });
    }

    m_animatedProperties.remove(disallowedProperties);
}

AcceleratedEffect::AcceleratedEffect(AnimationEffectTiming timing, Vector<Keyframe>&& keyframes, WebAnimationType type, CompositeOperation composite, RefPtr<TimingFunction>&& defaultKeyframeTimingFunction, OptionSet<WebCore::AcceleratedEffectProperty>&& animatedProperties, bool paused, double playbackRate, std::optional<WebAnimationTime> startTime, std::optional<WebAnimationTime> holdTime)
    : m_timing(timing)
    , m_keyframes(WTFMove(keyframes))
    , m_animationType(type)
    , m_compositeOperation(composite)
    , m_defaultKeyframeTimingFunction(WTFMove(defaultKeyframeTimingFunction))
    , m_animatedProperties(WTFMove(animatedProperties))
    , m_paused(paused)
    , m_playbackRate(playbackRate)
    , m_startTime(startTime)
    , m_holdTime(holdTime)
{
}

AcceleratedEffect::AcceleratedEffect(const AcceleratedEffect& source, OptionSet<AcceleratedEffectProperty>& propertyFilter)
{
    m_timing = source.m_timing;
    m_animationType = source.m_animationType;
    m_compositeOperation = source.m_compositeOperation;
    m_paused = source.m_paused;
    m_playbackRate = source.m_playbackRate;
    m_startTime = source.m_startTime;
    m_holdTime = source.m_holdTime;

    m_defaultKeyframeTimingFunction = source.m_defaultKeyframeTimingFunction.copyRef();

    for (auto& srcKeyframe : source.m_keyframes) {
        auto& animatedProperties = srcKeyframe.animatedProperties();
        if (!animatedProperties.containsAny(propertyFilter))
            continue;

        Keyframe keyframe {
            srcKeyframe.offset(),
            srcKeyframe.values().clone(),
            srcKeyframe.timingFunction().get(),
            srcKeyframe.compositeOperation(),
            srcKeyframe.animatedProperties() & propertyFilter
        };

        m_animatedProperties.add(keyframe.animatedProperties());
        m_keyframes.append(WTFMove(keyframe));
    }
}

static void blend(AcceleratedEffectProperty property, AcceleratedEffectValues& output, const AcceleratedEffectValues& from, const AcceleratedEffectValues& to, BlendingContext& blendingContext, const FloatRect& bounds)
{
    switch (property) {
    case AcceleratedEffectProperty::Opacity:
        output.opacity = blend(from.opacity, to.opacity, blendingContext);
        break;
    case AcceleratedEffectProperty::Transform: {
        LayoutSize boxSize { bounds.size() };
        output.transform = to.transform.blend(from.transform, blendingContext, boxSize);
        break;
    }
    case AcceleratedEffectProperty::Translate:
        if (auto& toTranslate = to.translate)
            output.translate = toTranslate->blend(from.translate.get(), blendingContext);
        break;
    case AcceleratedEffectProperty::Rotate:
        if (auto& toRotate = to.rotate)
            output.rotate = toRotate->blend(from.rotate.get(), blendingContext);
        break;
    case AcceleratedEffectProperty::Scale:
        if (auto& toScale = to.scale)
            output.scale = toScale->blend(from.scale.get(), blendingContext);
        break;
    case AcceleratedEffectProperty::OffsetAnchor:
        output.offsetDistance = blend(from.offsetDistance, to.offsetDistance, blendingContext);
        break;
    case AcceleratedEffectProperty::OffsetDistance:
        output.offsetDistance = blend(from.offsetDistance, to.offsetDistance, blendingContext);
        break;
    case AcceleratedEffectProperty::OffsetPath:
        if (auto& fromOffsetPath = from.offsetPath)
            output.offsetPath = fromOffsetPath->blend(to.offsetPath.get(), blendingContext);
        break;
    case AcceleratedEffectProperty::OffsetPosition:
        output.offsetPosition = blend(from.offsetPosition, to.offsetPosition, blendingContext);
        break;
    case AcceleratedEffectProperty::OffsetRotate:
        if (!from.offsetRotate.canBlend(to.offsetRotate)) {
            blendingContext.isDiscrete = true;
            blendingContext.normalizeProgress();
        }
        output.offsetRotate = from.offsetRotate.blend(to.offsetRotate, blendingContext);
        break;
    case AcceleratedEffectProperty::Filter:
        output.filter = to.filter.blend(from.filter, blendingContext);
        break;
    case AcceleratedEffectProperty::BackdropFilter:
        output.backdropFilter = to.backdropFilter.blend(from.backdropFilter, blendingContext);
        break;
    case AcceleratedEffectProperty::Invalid:
        ASSERT_NOT_REACHED();
        break;
    }
}

void AcceleratedEffect::apply(WebAnimationTime currentTime, AcceleratedEffectValues& values, const FloatRect& bounds)
{
    auto localTime = [&]() -> WebAnimationTime {
        ASSERT(m_holdTime || m_startTime);
        if (m_holdTime)
            return *m_holdTime;
        return (currentTime - *m_startTime) * m_playbackRate;
    }();

    // FIXME: when we add threaded animaiton support support for scroll-driven animations,
    // pass in the associated timeline's current time and duration.
    auto resolvedTiming = m_timing.resolve({
        std::nullopt,
        std::nullopt,
        { m_holdTime ? *m_holdTime : *m_startTime },
        { localTime },
        m_playbackRate
    });
    if (!resolvedTiming.transformedProgress)
        return;

    ASSERT(resolvedTiming.currentIteration);
    auto progress = *resolvedTiming.transformedProgress;

    // In the case of CSS Transitions we already know that there are only two keyframes, one where offset=0 and one where offset=1,
    // and only a single CSS property so we can simply blend based on the style available on those keyframes with the provided iteration
    // progress which already accounts for the transition's timing function.
    if (m_animationType == WebAnimationType::CSSTransition) {
        ASSERT(m_animatedProperties.hasExactlyOneBitSet());
        BlendingContext context { progress, false, m_compositeOperation };
        blend(*m_animatedProperties.begin(), values, m_keyframes.first().values(), m_keyframes.last().values(), context, bounds);
        return;
    }

    Keyframe propertySpecificKeyframeWithZeroOffset { 0, values.clone() };
    Keyframe propertySpecificKeyframeWithOneOffset { 1, values.clone() };

    for (auto animatedProperty : m_animatedProperties) {
        auto interval = interpolationKeyframes(animatedProperty, progress, propertySpecificKeyframeWithZeroOffset, propertySpecificKeyframeWithOneOffset);

        auto* startKeyframe = interval.endpoints.first();
        auto* endKeyframe = interval.endpoints.last();

        auto startKeyframeValues = downcast<AcceleratedEffect::Keyframe>(startKeyframe)->values();
        auto endKeyframeValues = downcast<AcceleratedEffect::Keyframe>(endKeyframe)->values();

        KeyframeInterpolation::CompositionCallback composeProperty = [&](const KeyframeInterpolation::Keyframe& keyframe, CompositeOperation compositeOperation) {
            auto& acceleratedKeyframe = downcast<AcceleratedEffect::Keyframe>(keyframe);
            BlendingContext context { 1, false, compositeOperation };
            if (acceleratedKeyframe.offset() == startKeyframe->offset())
                blend(animatedProperty, startKeyframeValues, propertySpecificKeyframeWithZeroOffset.values(), acceleratedKeyframe.values(), context, bounds);
            else
                blend(animatedProperty, endKeyframeValues, propertySpecificKeyframeWithZeroOffset.values(), acceleratedKeyframe.values(), context, bounds);
        };

        KeyframeInterpolation::AccumulationCallback accumulateProperty = [&](const KeyframeInterpolation::Keyframe&) {
            // FIXME: implement accumulation.
        };

        KeyframeInterpolation::InterpolationCallback interpolateProperty = [&](double intervalProgress, double, IterationCompositeOperation) {
            // FIXME: handle currentIteration and iterationCompositeOperation.
            BlendingContext context { intervalProgress };
            blend(animatedProperty, values, startKeyframeValues, endKeyframeValues, context, bounds);
        };

        KeyframeInterpolation::RequiresBlendingForAccumulativeIterationCallback requiresBlendingForAccumulativeIterationCallback = [&]() {
            // FIXME: implement accumulation.
            return false;
        };

        interpolateKeyframes(animatedProperty, interval, progress, *resolvedTiming.currentIteration, m_timing.iterationDuration, resolvedTiming.before, composeProperty, accumulateProperty, interpolateProperty, requiresBlendingForAccumulativeIterationCallback);
    }
}

void AcceleratedEffect::validateFilters(const AcceleratedEffectValues& baseValues, OptionSet<AcceleratedEffectProperty>& disallowedProperties)
{
    auto filterOperations = [&](const AcceleratedEffectValues& values, AcceleratedEffectProperty property) -> const FilterOperations& {
        switch (property) {
        case AcceleratedEffectProperty::Filter:
            return values.filter;
        case AcceleratedEffectProperty::BackdropFilter:
            return values.backdropFilter;
        default:
            ASSERT_NOT_REACHED();
            return values.filter;
        }
    };

    auto isValidProperty = [&](AcceleratedEffectProperty property) {
        // First, let's assemble the matching values.
        Vector<const AcceleratedEffectValues*> values;
        for (auto& keyframe : m_keyframes) {
            if (keyframe.animatesProperty(property)) {
                // If this is the first value we're processing and it's not the
                // keyframe with offset 0, then we need to add the implicit 0% values.
                if (values.isEmpty() && keyframe.offset())
                    values.append(&baseValues);
                values.append(&keyframe.values());
            } else {
                // If this is the last keyframe we'll be processing and it's not the
                // keyframe with offset 1, then we need to add the implicit 100% values.
                if (&keyframe == &m_keyframes.last() && keyframe.offset() == 1)
                    values.append(&baseValues);
            }
        }

        ASSERT(values.size() > 1);

        const FilterOperations* longestFilterList = nullptr;
        for (size_t i = 1; i < values.size(); ++i) {
            auto& fromFilters = filterOperations(*values[i - 1], property);
            auto& toFilters = filterOperations(*values[i], property);
            // FIXME: we should provide the actual composite operation here.
            if (!fromFilters.canInterpolate(toFilters, CompositeOperation::Replace))
                return false;
            if (!longestFilterList || fromFilters.size() > longestFilterList->size())
                longestFilterList = &fromFilters;
            if (!longestFilterList || toFilters.size() > longestFilterList->size())
                longestFilterList = &toFilters;
        }

        // We need to make sure that the longest filter, if it contains a drop-shadow() operation,
        // has it as its final operation since it will be applied by a separate CALayer property
        // from the other filter operations and it will be applied to the layer as the last filer.
        ASSERT(longestFilterList);
        for (auto& operation : *longestFilterList) {
            if (operation->type() == FilterOperation::Type::DropShadow && operation != longestFilterList->last())
                return false;
        }

        return true;
    };

    auto validateProperty = [&](AcceleratedEffectProperty property) {
        if (isValidProperty(property))
            return;
        disallowedProperties.add({ property });
        m_animatedProperties.remove({ property });
        m_disallowedProperties.add({ property });
        for (auto& keyframe : m_keyframes)
            keyframe.clearProperty(property);
    };

    if (m_animatedProperties.contains(AcceleratedEffectProperty::Filter))
        validateProperty(AcceleratedEffectProperty::Filter);
    if (m_animatedProperties.contains(AcceleratedEffectProperty::BackdropFilter))
        validateProperty(AcceleratedEffectProperty::BackdropFilter);
}

bool AcceleratedEffect::animatesTransformRelatedProperty() const
{
    return m_animatedProperties.containsAny(transformRelatedAcceleratedProperties);
}

const KeyframeInterpolation::Keyframe& AcceleratedEffect::keyframeAtIndex(size_t index) const
{
    ASSERT(index < m_keyframes.size());
    return m_keyframes[index];
}

const TimingFunction* AcceleratedEffect::timingFunctionForKeyframe(const KeyframeInterpolation::Keyframe& keyframe) const
{
    auto* acceleratedEffectKeyframe = dynamicDowncast<Keyframe>(keyframe);
    ASSERT(acceleratedEffectKeyframe);
    if (!acceleratedEffectKeyframe)
        return nullptr;

    if (m_animationType == WebAnimationType::CSSAnimation || m_animationType == WebAnimationType::CSSTransition) {
        // If we're dealing with a CSS Animation, the timing function may be specified on the keyframe.
        if (m_animationType == WebAnimationType::CSSAnimation) {
            if (auto& timingFunction = acceleratedEffectKeyframe->timingFunction())
                return timingFunction.get();
        }

        // Failing that, or for a CSS Transition, the timing function is the default timing function.
        return m_defaultKeyframeTimingFunction.get();
    }

    return acceleratedEffectKeyframe->timingFunction().get();
}

bool AcceleratedEffect::isPropertyAdditiveOrCumulative(KeyframeInterpolation::Property property) const
{
    return WTF::switchOn(property, [&](const AcceleratedEffectProperty acceleratedProperty) {
        return CSSPropertyAnimation::isPropertyAdditiveOrCumulative(cssPropertyFromAcceleratedProperty(acceleratedProperty));
    }, [] (auto&) {
        ASSERT_NOT_REACHED();
        return false;
    });
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
