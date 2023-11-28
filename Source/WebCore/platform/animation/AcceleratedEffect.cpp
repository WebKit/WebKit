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
#include "CSSPropertyAnimation.h"
#include "CSSPropertyNames.h"
#include "DeclarativeAnimation.h"
#include "Document.h"
#include "KeyframeEffect.h"
#include "KeyframeList.h"
#include "LayoutSize.h"
#include "WebAnimation.h"
#include "WebAnimationTypes.h"
#include <wtf/IsoMallocInlines.h>

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

WTF_MAKE_ISO_ALLOCATED_IMPL(AcceleratedEffect);

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

Ref<AcceleratedEffect> AcceleratedEffect::create(const KeyframeEffect& effect, const IntRect& borderBoxRect)
{
    return adoptRef(*new AcceleratedEffect(effect, borderBoxRect));
}

Ref<AcceleratedEffect> AcceleratedEffect::create(AnimationEffectTiming timing, Vector<Keyframe>&& keyframes, WebAnimationType type, CompositeOperation composite, RefPtr<TimingFunction>&& defaultKeyframeTimingFunction, OptionSet<WebCore::AcceleratedEffectProperty>&& animatedProperties, bool paused, double playbackRate, std::optional<Seconds> startTime, std::optional<Seconds> holdTime)
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

AcceleratedEffect::AcceleratedEffect(const KeyframeEffect& effect, const IntRect& borderBoxRect)
{
    m_timing = effect.timing();
    m_compositeOperation = effect.composite();
    m_animationType = effect.animationType();

    ASSERT(effect.animation());
    if (auto* animation = effect.animation()) {
        m_paused = animation->playState() == WebAnimation::PlayState::Paused;
        m_playbackRate = animation->playbackRate();
        ASSERT(animation->holdTime() || animation->startTime());
        m_holdTime = animation->holdTime();
        m_startTime = animation->startTime();
        if (is<DeclarativeAnimation>(animation)) {
            if (auto* defaultKeyframeTimingFunction = downcast<DeclarativeAnimation>(*animation).backingAnimation().timingFunction())
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
}

AcceleratedEffect::AcceleratedEffect(AnimationEffectTiming timing, Vector<Keyframe>&& keyframes, WebAnimationType type, CompositeOperation composite, RefPtr<TimingFunction>&& defaultKeyframeTimingFunction, OptionSet<WebCore::AcceleratedEffectProperty>&& animatedProperties, bool paused, double playbackRate, std::optional<Seconds> startTime, std::optional<Seconds> holdTime)
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

bool AcceleratedEffect::animatesTransformRelatedProperty() const
{
    return m_animatedProperties.containsAny({
        AcceleratedEffectProperty::Transform,
        AcceleratedEffectProperty::Translate,
        AcceleratedEffectProperty::Rotate,
        AcceleratedEffectProperty::Scale,
        AcceleratedEffectProperty::OffsetPath,
        AcceleratedEffectProperty::OffsetDistance,
        AcceleratedEffectProperty::OffsetPosition,
        AcceleratedEffectProperty::OffsetAnchor,
        AcceleratedEffectProperty::OffsetRotate
    });
}

const KeyframeInterpolation::Keyframe& AcceleratedEffect::keyframeAtIndex(size_t index) const
{
    ASSERT(index < m_keyframes.size());
    return m_keyframes[index];
}

const TimingFunction* AcceleratedEffect::timingFunctionForKeyframe(const KeyframeInterpolation::Keyframe& keyframe) const
{
    if (!is<Keyframe>(keyframe)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto& acceleratedEffectKeyframe = downcast<Keyframe>(keyframe);
    if (m_animationType == WebAnimationType::CSSAnimation || m_animationType == WebAnimationType::CSSTransition) {
        // If we're dealing with a CSS Animation, the timing function may be specified on the keyframe.
        if (m_animationType == WebAnimationType::CSSAnimation) {
            if (auto& timingFunction = acceleratedEffectKeyframe.timingFunction())
                return timingFunction.get();
        }

        // Failing that, or for a CSS Transition, the timing function is the default timing function.
        return m_defaultKeyframeTimingFunction.get();
    }

    return acceleratedEffectKeyframe.timingFunction().get();
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
