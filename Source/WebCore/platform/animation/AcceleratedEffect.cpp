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

AcceleratedEffectKeyframe AcceleratedEffectKeyframe::clone() const
{
    RefPtr<TimingFunction> clonedTimingFunction;
    if (auto* srcTimingFunction = timingFunction.get())
        clonedTimingFunction = srcTimingFunction->clone();

    return {
        offset,
        values.clone(),
        WTFMove(clonedTimingFunction),
        compositeOperation,
        animatedProperties
    };
};

WTF_MAKE_ISO_ALLOCATED_IMPL(AcceleratedEffect);

static AcceleratedEffectProperty acceleratedPropertyFromCSSProperty(AnimatableProperty property, const Settings& settings)
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
#if ENABLE(FILTERS_LEVEL_2)
    case CSSPropertyBackdropFilter:
    case CSSPropertyWebkitBackdropFilter:
        return AcceleratedEffectProperty::BackdropFilter;
#endif
    default:
        ASSERT_NOT_REACHED();
        return AcceleratedEffectProperty::Invalid;
    }
}

Ref<AcceleratedEffect> AcceleratedEffect::create(const KeyframeEffect& effect, const IntRect& borderBoxRect)
{
    return adoptRef(*new AcceleratedEffect(effect, borderBoxRect));
}

Ref<AcceleratedEffect> AcceleratedEffect::create(AnimationEffectTiming timing, Vector<AcceleratedEffectKeyframe>&& keyframes, WebAnimationType type, CompositeOperation composite, RefPtr<TimingFunction>&& defaultKeyframeTimingFunction, OptionSet<WebCore::AcceleratedEffectProperty>&& animatedProperties, bool paused, double playbackRate, std::optional<Seconds> startTime, std::optional<Seconds> holdTime)
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

        AcceleratedEffectKeyframe acceleratedKeyframe;
        acceleratedKeyframe.offset = srcKeyframe.key();
        acceleratedKeyframe.compositeOperation = srcKeyframe.compositeOperation();
        acceleratedKeyframe.animatedProperties = WTFMove(animatedProperties);

        acceleratedKeyframe.values = [&]() -> AcceleratedEffectValues {
            if (auto* style = srcKeyframe.style())
                return { *style, borderBoxRect };
            return { };
        }();

        acceleratedKeyframe.timingFunction = srcKeyframe.timingFunction();

        m_keyframes.append(WTFMove(acceleratedKeyframe));
    }
}

AcceleratedEffect::AcceleratedEffect(AnimationEffectTiming timing, Vector<AcceleratedEffectKeyframe>&& keyframes, WebAnimationType type, CompositeOperation composite, RefPtr<TimingFunction>&& defaultKeyframeTimingFunction, OptionSet<WebCore::AcceleratedEffectProperty>&& animatedProperties, bool paused, double playbackRate, std::optional<Seconds> startTime, std::optional<Seconds> holdTime)
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
        auto& animatedProperties = srcKeyframe.animatedProperties;
        if (!animatedProperties.containsAny(propertyFilter))
            continue;

        AcceleratedEffectKeyframe keyframe;
        keyframe.offset = srcKeyframe.offset;
        keyframe.values = srcKeyframe.values;
        keyframe.compositeOperation = srcKeyframe.compositeOperation;
        keyframe.animatedProperties = srcKeyframe.animatedProperties & propertyFilter;
        keyframe.timingFunction = srcKeyframe.timingFunction.copyRef();

        m_animatedProperties.add(keyframe.animatedProperties);
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

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
