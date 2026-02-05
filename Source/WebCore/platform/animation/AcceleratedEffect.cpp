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

#if ENABLE(THREADED_ANIMATIONS)

#include "AnimationEffect.h"
#include "AnimationTimeline.h"
#include "AnimationUtilities.h"
#include "BlendingKeyframes.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "FilterOperations.h"
#include "FloatRect.h"
#include "KeyframeEffect.h"
#include "LayoutSize.h"
#include "Settings.h"
#include "StyleInterpolation.h"
#include "StyleOffsetRotate.h"
#include "StyleOriginatedAnimation.h"
#include "WebAnimation.h"
#include "WebAnimationTypes.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

AcceleratedEffect::Keyframe::Keyframe(double offset, AcceleratedEffectValues&& values)
    : m_offset(offset)
    , m_values(WTF::move(values))
{
}

AcceleratedEffect::Keyframe::Keyframe(double offset, AcceleratedEffectValues&& values, RefPtr<TimingFunction>&& timingFunction, std::optional<CompositeOperation> compositeOperation, OptionSet<AcceleratedEffectProperty>&& animatedProperties)
    : m_offset(offset)
    , m_values(WTF::move(values))
    , m_timingFunction(WTF::move(timingFunction))
    , m_compositeOperation(compositeOperation)
    , m_animatedProperties(WTF::move(animatedProperties))
{
}

void AcceleratedEffect::Keyframe::clearProperty(AcceleratedEffectProperty property)
{
    m_animatedProperties.remove({ property });

    // If a filter property is removed, it's because it cannot be represented remotely,
    // so we must ensure we reset it in the base values so that we don't attempt to encode
    // an unsupported filter operation.
    if (property == AcceleratedEffectProperty::Filter)
        m_values.filter = { };
    if (property == AcceleratedEffectProperty::BackdropFilter)
        m_values.backdropFilter = { };
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
    if (RefPtr srcTimingFunction = m_timingFunction)
        clonedTimingFunction = srcTimingFunction->clone();

    return {
        m_offset,
        m_values.clone(),
        WTF::move(clonedTimingFunction),
        m_compositeOperation,
        WTF::move(clonedAnimatedProperties)
    };
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(AcceleratedEffect);

static AcceleratedEffectProperty acceleratedPropertyFromCSSProperty(AnimatableCSSProperty property, const Settings& settings)
{
#if ASSERT_ENABLED
    ASSERT(Style::Interpolation::isAccelerated(property, settings));
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

Ref<AcceleratedEffect> AcceleratedEffect::create(const KeyframeEffect& effect, const IntRect& borderBoxRect, const AcceleratedEffectValues& baseValues, OptionSet<AcceleratedEffectProperty>& disallowedProperties)
{
    Ref acceleratedEffect = adoptRef(*new AcceleratedEffect(effect, borderBoxRect, disallowedProperties));
    acceleratedEffect->validateFilters(baseValues, disallowedProperties);
    return acceleratedEffect;
}

Ref<AcceleratedEffect> AcceleratedEffect::create(AnimationEffectTiming timing, TimelineIdentifier&& timelineIdentifier, Vector<Keyframe>&& keyframes, WebAnimationType type, CompositeOperation composite, RefPtr<TimingFunction>&& defaultKeyframeTimingFunction, OptionSet<WebCore::AcceleratedEffectProperty>&& animatedProperties, bool paused, double playbackRate, std::optional<WebAnimationTime> startTime, std::optional<WebAnimationTime> holdTime)
{
    return adoptRef(*new AcceleratedEffect(WTF::move(timing), WTF::move(timelineIdentifier), WTF::move(keyframes), type, composite, WTF::move(defaultKeyframeTimingFunction), WTF::move(animatedProperties), paused, playbackRate, startTime, holdTime));
}

Ref<AcceleratedEffect> AcceleratedEffect::clone() const
{
    auto clonedKeyframes = m_keyframes.map([](const auto& keyframe) {
        return keyframe.clone();
    });

    RefPtr<TimingFunction> clonedDefaultKeyframeTimingFunction;
    if (RefPtr defaultKeyframeTimingFunction = m_defaultKeyframeTimingFunction)
        clonedDefaultKeyframeTimingFunction = defaultKeyframeTimingFunction->clone();

    auto clonedAnimatedProperties = m_animatedProperties;
    auto clonedIdentifier = m_timelineIdentifier;

    return AcceleratedEffect::create(m_timing, WTF::move(clonedIdentifier), WTF::move(clonedKeyframes), m_animationType, m_compositeOperation, WTF::move(clonedDefaultKeyframeTimingFunction), WTF::move(clonedAnimatedProperties), m_paused, m_playbackRate, m_startTime, m_holdTime);
}

Ref<AcceleratedEffect> AcceleratedEffect::copyWithProperties(OptionSet<AcceleratedEffectProperty>& propertyFilter) const
{
    return adoptRef(*new AcceleratedEffect(*this, propertyFilter));
}

AcceleratedEffect::AcceleratedEffect(const KeyframeEffect& effect, const IntRect& borderBoxRect, const OptionSet<AcceleratedEffectProperty>& disallowedProperties)
    : m_timelineIdentifier(effect.animation()->timeline()->acceleratedTimelineIdentifier())
{
    ASSERT(effect.animation());
    ASSERT(effect.animation()->timeline());
    ASSERT(effect.animation()->timeline()->canBeAccelerated());
    m_timeline = Ref { *effect.animation()->timeline() }->acceleratedRepresentation();

    m_timing = effect.timing();
    m_compositeOperation = effect.composite();
    m_animationType = effect.animationType();

    ASSERT(effect.animation());
    if (RefPtr animation = effect.animation()) {
        m_paused = animation->playState() == WebAnimation::PlayState::Paused;
        m_playbackRate = animation->playbackRate();
        ASSERT(!animation->pending());
        ASSERT(animation->holdTime() || animation->startTime());
        m_holdTime = animation->holdTime();
        m_startTime = animation->startTime();
        if (RefPtr styleAnimation = dynamicDowncast<StyleOriginatedAnimation>(*animation)) {
            if (RefPtr defaultKeyframeTimingFunction = styleAnimation->backingAnimationTimingFunction())
                m_defaultKeyframeTimingFunction = WTF::move(defaultKeyframeTimingFunction);
        }
    }

    ASSERT(effect.document());
    auto& settings = effect.document()->settings();
    CheckedPtr renderLayerModelObject = dynamicDowncast<RenderLayerModelObject>(effect.renderer());

    OptionSet<AcceleratedEffectProperty> propertiesReplacedByZeroKeyframe;
    OptionSet<AcceleratedEffectProperty> propertiesReplacedByOneKeyframe;

    for (auto& srcKeyframe : effect.blendingKeyframes()) {
        ASSERT(!std::isnan(srcKeyframe.offset()));
        auto offset = srcKeyframe.offset();
        auto isReplacingKeyframe = m_compositeOperation == CompositeOperation::Replace
            && (!srcKeyframe.compositeOperation() || srcKeyframe.compositeOperation() == CompositeOperation::Replace);
        OptionSet<AcceleratedEffectProperty> animatedProperties;
        for (auto animatedCSSProperty : srcKeyframe.properties()) {
            if (Style::Interpolation::isAccelerated(animatedCSSProperty, settings)) {
                auto acceleratedProperty = acceleratedPropertyFromCSSProperty(animatedCSSProperty, settings);
                if (disallowedProperties.contains(acceleratedProperty))
                    continue;
                animatedProperties.add(acceleratedProperty);
                m_animatedProperties.add(acceleratedProperty);
                if (!isReplacingKeyframe)
                    continue;
                if (!offset)
                    propertiesReplacedByZeroKeyframe.add(acceleratedProperty);
                if (offset == 1.0)
                    propertiesReplacedByOneKeyframe.add(acceleratedProperty);
            }
        }

        if (animatedProperties.isEmpty())
            continue;

        auto values = [&]() -> AcceleratedEffectValues {
            if (auto* style = srcKeyframe.style())
                return { *style, borderBoxRect, renderLayerModelObject.get() };
            return { };
        }();

        m_keyframes.append({ offset, WTF::move(values), srcKeyframe.timingFunction(), srcKeyframe.compositeOperation(), WTF::move(animatedProperties) });
    }

    // Any property that was added to both the zero and one keyframe replaced
    // properties is a property fully replaced by this effect.
    m_replacedProperties = propertiesReplacedByZeroKeyframe & propertiesReplacedByOneKeyframe;
    ASSERT(!m_replacedProperties.containsAny(disallowedProperties));

    m_animatedProperties.remove(disallowedProperties);
}

AcceleratedEffect::AcceleratedEffect(AnimationEffectTiming timing, TimelineIdentifier&& timelineIdentifier, Vector<Keyframe>&& keyframes, WebAnimationType type, CompositeOperation composite, RefPtr<TimingFunction>&& defaultKeyframeTimingFunction, OptionSet<WebCore::AcceleratedEffectProperty>&& animatedProperties, bool paused, double playbackRate, std::optional<WebAnimationTime> startTime, std::optional<WebAnimationTime> holdTime)
    : m_timing(timing)
    , m_timelineIdentifier(WTF::move(timelineIdentifier))
    , m_keyframes(WTF::move(keyframes))
    , m_animationType(type)
    , m_compositeOperation(composite)
    , m_defaultKeyframeTimingFunction(WTF::move(defaultKeyframeTimingFunction))
    , m_animatedProperties(WTF::move(animatedProperties))
    , m_paused(paused)
    , m_playbackRate(playbackRate)
    , m_startTime(startTime)
    , m_holdTime(holdTime)
{
}

AcceleratedEffect::AcceleratedEffect(const AcceleratedEffect& source, OptionSet<AcceleratedEffectProperty>& propertyFilter)
    : m_timelineIdentifier(source.m_timelineIdentifier)
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
        m_keyframes.append(WTF::move(keyframe));
    }
}

static void blend(AcceleratedEffectProperty property, AcceleratedEffectValues& output, const AcceleratedEffectValues& from, const AcceleratedEffectValues& to, BlendingContext& blendingContext)
{
    switch (property) {
    case AcceleratedEffectProperty::Opacity:
        output.opacity = WebCore::blend(from.opacity, to.opacity, blendingContext);
        break;
    case AcceleratedEffectProperty::Transform:
        output.transform = blend(from.transform, to.transform, blendingContext);
        break;
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
        if (!canBlend(from.offsetAnchor, to.offsetAnchor)) {
            blendingContext.isDiscrete = true;
            blendingContext.normalizeProgress();
        }
        output.offsetAnchor = blend(from.offsetAnchor, to.offsetAnchor, blendingContext);
        break;
    case AcceleratedEffectProperty::OffsetDistance:
        output.offsetDistance = blend(from.offsetDistance, to.offsetDistance, blendingContext);
        break;
    case AcceleratedEffectProperty::OffsetPath:
        if (auto& fromOffsetPath = from.offsetPath)
            output.offsetPath = fromOffsetPath->blend(to.offsetPath.get(), blendingContext);
        break;
    case AcceleratedEffectProperty::OffsetPosition:
        if (!canBlend(from.offsetPosition, to.offsetPosition)) {
            blendingContext.isDiscrete = true;
            blendingContext.normalizeProgress();
        }
        output.offsetPosition = blend(from.offsetPosition, to.offsetPosition, blendingContext);
        break;
    case AcceleratedEffectProperty::OffsetRotate:
        if (!canBlend(from.offsetRotate, to.offsetRotate)) {
            blendingContext.isDiscrete = true;
            blendingContext.normalizeProgress();
        }
        output.offsetRotate = blend(from.offsetRotate, to.offsetRotate, blendingContext);
        break;
    case AcceleratedEffectProperty::Filter:
        output.filter = from.filter.blend(to.filter, blendingContext);
        break;
    case AcceleratedEffectProperty::BackdropFilter:
        output.backdropFilter = from.backdropFilter.blend(to.backdropFilter, blendingContext);
        break;
    case AcceleratedEffectProperty::Invalid:
        ASSERT_NOT_REACHED();
        break;
    }
}

ResolvedEffectTiming AcceleratedEffect::resolvedTimingForTesting(WebAnimationTime timelineTime, std::optional<WebAnimationTime> timelineDuration) const
{
    return resolvedTiming(timelineTime, timelineDuration);
}

ResolvedEffectTiming AcceleratedEffect::resolvedTiming(WebAnimationTime timelineTime, std::optional<WebAnimationTime> timelineDuration) const
{
    ASSERT_IMPLIES(m_paused, m_holdTime);
    ASSERT_IMPLIES(!m_paused, m_startTime);

    auto localTime = [&] {
        if (m_paused)
            return *m_holdTime;
        return (timelineTime - *m_startTime) * m_playbackRate;
    }();

    return m_timing.resolve({
        timelineTime,
        timelineDuration,
        m_paused ? *m_holdTime : *m_startTime,
        localTime,
        EndpointInclusiveActiveInterval::No,
        m_playbackRate
    });
}

void AcceleratedEffect::apply(AcceleratedEffectValues& values, WebAnimationTime timelineTime, std::optional<WebAnimationTime> timelineDuration) const
{
    auto resolvedTiming = this->resolvedTiming(timelineTime, timelineDuration);
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
        blend(*m_animatedProperties.begin(), values, m_keyframes.first().values(), m_keyframes.last().values(), context);
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
                blend(animatedProperty, startKeyframeValues, propertySpecificKeyframeWithZeroOffset.values(), acceleratedKeyframe.values(), context);
            else
                blend(animatedProperty, endKeyframeValues, propertySpecificKeyframeWithZeroOffset.values(), acceleratedKeyframe.values(), context);
        };

        KeyframeInterpolation::AccumulationCallback accumulateProperty = [&](const KeyframeInterpolation::Keyframe&) {
            // FIXME: implement accumulation.
        };

        KeyframeInterpolation::InterpolationCallback interpolateProperty = [&](double intervalProgress, double, IterationCompositeOperation) {
            // FIXME: handle currentIteration and iterationCompositeOperation.
            BlendingContext context { intervalProgress };
            blend(animatedProperty, values, startKeyframeValues, endKeyframeValues, context);
        };

        KeyframeInterpolation::RequiresInterpolationForAccumulativeIterationCallback requiresInterpolationForAccumulativeIterationCallback = [&]() {
            // FIXME: implement accumulation.
            return false;
        };

        interpolateKeyframes(animatedProperty, interval, progress, *resolvedTiming.currentIteration, m_timing.iterationDuration, resolvedTiming.before, composeProperty, accumulateProperty, interpolateProperty, requiresInterpolationForAccumulativeIterationCallback);
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
        bool hasToKeyframe = false;
        for (auto& keyframe : m_keyframes) {
            if (keyframe.animatesProperty(property)) {
                // If this is the first value we're processing and it's not the
                // keyframe with offset 0, then we need to add the implicit 0% values.
                if (values.isEmpty() && keyframe.offset())
                    values.append(&baseValues);
                values.append(&keyframe.values());
                hasToKeyframe = hasToKeyframe || keyframe.offset() == 1;
            }
        }

        // At this stage we should have found at least an explicit 0% keyframe
        // or have added an implicit 0% keyframe.
        ASSERT(values.size());

        // Add the implicit 100% value if one wasn't provided.
        if (!hasToKeyframe)
            values.append(&baseValues);

        // Now we should have at least 0% and 100% values.
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
        // However, drop-shadow() operations with a style color are never supported, see
        // PlatformCAFilters::setFiltersOnLayer().
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
        m_disallowedProperties.add({ property });
        clearProperty(property);
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
        return Style::Interpolation::isAdditiveOrCumulative(cssPropertyFromAcceleratedProperty(acceleratedProperty));
    }, [] (auto&) {
        ASSERT_NOT_REACHED();
        return false;
    });
}

void AcceleratedEffect::clearProperty(AcceleratedEffectProperty property)
{
    m_animatedProperties.remove({ property });

    for (auto& keyframe : m_keyframes)
        keyframe.clearProperty(property);
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
