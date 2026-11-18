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

#import "config.h"
#import "RemoteAnimationStack.h"

#if ENABLE(THREADED_ANIMATIONS)

#import "RemoteAnimationUtilities.h"
#import "RemoteProgressBasedTimeline.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/Borrow.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAnimationStack);

Ref<RemoteAnimationStack> RemoteAnimationStack::create(RemoteAnimations&& animations, WebCore::AcceleratedEffectValues&& baseValues, WebCore::FloatRect bounds)
{
    return adoptRef(*new RemoteAnimationStack(WTF::move(animations), WTF::move(baseValues), bounds));
}

RemoteAnimationStack::RemoteAnimationStack(RemoteAnimations&& animations, WebCore::AcceleratedEffectValues&& baseValues, WebCore::FloatRect bounds)
    : m_animations(WTF::move(animations))
    , m_baseValues(WTF::move(baseValues))
    , m_bounds(bounds)
{
    bool affectsFilter = false;
    bool affectsOpacity = false;
    bool affectsTransform = false;

    for (auto& animation : m_animations) {
        auto& properties = animation->animatedProperties();
        m_filtersAffectBackdrop = m_filtersAffectBackdrop || properties.contains(WebCore::AcceleratedEffectProperty::BackdropFilter);
        affectsFilter = affectsFilter || m_filtersAffectBackdrop || properties.contains(WebCore::AcceleratedEffectProperty::Filter);
        affectsOpacity = affectsOpacity || properties.contains(WebCore::AcceleratedEffectProperty::Opacity);
        affectsTransform = affectsTransform || properties.containsAny(WebCore::transformRelatedAcceleratedProperties);
        m_hasProgressBasedAnimations = m_hasProgressBasedAnimations || animation->timeline().isProgressBased();
        m_hasTimeBasedAnimations = m_hasTimeBasedAnimations || animation->timeline().isMonotonic();
        if (affectsFilter && affectsOpacity && affectsTransform && m_hasProgressBasedAnimations && m_hasTimeBasedAnimations)
            break;
    }

    ASSERT(affectsFilter || affectsOpacity || affectsTransform);

#if ASSERT_ENABLED
    if (affectsFilter) {
        // We are not supposed to have an effect stack in the remote layer tree affecting both `filter` and `backdrop-filter`.
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=329905
        bool hasAnimationTargetingFilter = false;
        bool hasAnimationTargetingBackdropFilter = false;
        for (auto& animation : m_animations) {
            auto& properties = animation->animatedProperties();
            hasAnimationTargetingFilter = hasAnimationTargetingFilter || properties.contains(WebCore::AcceleratedEffectProperty::Filter);
            hasAnimationTargetingBackdropFilter = hasAnimationTargetingBackdropFilter || properties.contains(WebCore::AcceleratedEffectProperty::BackdropFilter);
        }
        ASSERT(hasAnimationTargetingFilter != hasAnimationTargetingBackdropFilter);
    }
#endif

    if (affectsFilter)
        m_affectedLayerProperties.add(LayerProperty::Filter);
    if (affectsOpacity)
        m_affectedLayerProperties.add(LayerProperty::Opacity);
    if (affectsTransform)
        m_affectedLayerProperties.add(LayerProperty::Transform);
}

#if PLATFORM(MAC)
const WebCore::FilterOperations* RemoteAnimationStack::longestFilterList() const
{
    if (!m_affectedLayerProperties.contains(LayerProperty::Filter))
        return nullptr;

    // FIXME: while m_affectedLayerProperties does not make the distinction between backdrop-filter
    // and filter, the animations and keyframes do so we must check against both. However, it should
    // only be either filter or backdrop-filter as a different layer will be used for those properties.
    OptionSet<WebCore::AcceleratedEffectProperty> filterOrBackdropFilter = { WebCore::AcceleratedEffectProperty::Filter, WebCore::AcceleratedEffectProperty::BackdropFilter };

    auto keyframeFilter = [](const WebCore::AcceleratedEffect::Keyframe& keyframe) -> const WebCore::FilterOperations* {
        auto& animatedProperties = keyframe.animatedProperties();
        if (animatedProperties.contains(WebCore::AcceleratedEffectProperty::Filter))
            return &keyframe.values().filter;
        if (animatedProperties.contains(WebCore::AcceleratedEffectProperty::BackdropFilter))
            return &keyframe.values().backdropFilter;
        return nullptr;
    };

    const WebCore::FilterOperations* longestFilterList = nullptr;
    for (auto& animation : m_animations) {
        if (!animation->animatedProperties().containsAny(filterOrBackdropFilter))
            continue;
        for (auto& keyframe : animation->keyframes()) {
            auto* filter = keyframeFilter(keyframe);
            if (!filter)
                continue;
            if (!longestFilterList || longestFilterList->size() < filter->size())
                longestFilterList = filter;
        }
    }

    if (longestFilterList) {
        auto& baseFilter = m_filtersAffectBackdrop ? m_baseValues.backdropFilter : m_baseValues.filter;
        if (longestFilterList->size() < baseFilter.size())
            longestFilterList = &baseFilter;
    }

    return longestFilterList && !longestFilterList->isEmpty() ? longestFilterList : nullptr;
}

void RemoteAnimationStack::initEffectsFromMainThread(PlatformLayer *layer)
{
    ASSERT(m_filterPresentationModifiers.isEmpty());
    ASSERT(!m_opacityPresentationModifier);
    ASSERT(!m_transformPresentationModifier);
    ASSERT(!m_presentationModifierGroup);

    auto computedValues = computeValues();

    // While m_affectedLayerProperties may contain LayerProperty::Filter, in practice
    // we could be in a situation where all `filter` values for this animation stack
    // are `none`. In that case, longestFilterList() will return nullptr, so we can
    // use this alone to determine whether this stack interpolates `filter`.
    auto* canonicalFilters = longestFilterList();

    // FIXME: If we're only animating a filter property and we haven't found a filter, then
    // we won't have anything to animate. Ideally, we wouldn't end up in this state where we
    // have a no-op animation stack. See https://bugs.webkit.org/show_bug.cgi?id=309658.
    if (!canonicalFilters && m_affectedLayerProperties.containsOnly({ LayerProperty::Filter }))
        return;

    auto numberOfPresentationModifiers = [&]() {
        size_t count = 0;
        if (canonicalFilters)
            count += WebCore::PlatformCAFilters::presentationModifierCount(*canonicalFilters);
        if (m_affectedLayerProperties.contains(LayerProperty::Opacity))
            count++;
        if (m_affectedLayerProperties.contains(LayerProperty::Transform))
            count++;
        return count;
    }();

    m_presentationModifierGroup = [CAPresentationModifierGroup groupWithCapacity:numberOfPresentationModifiers];

    // By default the render server continuously monitors a presentation group for flushed changes.
    // We disable this behavior putting us in charge of calling -flushWithTransaction to explicitly
    // inform the render server a change happened. This helps with performance when a layer is covered
    // by another opaque layer.
    [m_presentationModifierGroup setUpdatesAsynchronously:NO];

    if (canonicalFilters) {
        auto& filter = m_filtersAffectBackdrop ? computedValues.backdropFilter : computedValues.filter;
        WebCore::PlatformCAFilters::presentationModifiers(filter, canonicalFilters, m_filterPresentationModifiers, m_presentationModifierGroup);
        for (auto& filterPresentationModifier : borrow(m_filterPresentationModifiers))
            [layer addPresentationModifier:filterPresentationModifier.second.get()];
        m_lastAppliedFilter = filter;
    }

    if (m_affectedLayerProperties.contains(LayerProperty::Opacity)) {
        RetainPtr opacity = @(computedValues.opacity.value);
        m_opacityPresentationModifier = adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"opacity" initialValue:opacity.get() additive:NO group:m_presentationModifierGroup.get()]);
        [layer addPresentationModifier:m_opacityPresentationModifier.get()];
        m_lastAppliedOpacity = computedValues.opacity.value;
    }

    if (m_affectedLayerProperties.contains(LayerProperty::Transform)) {
        auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);
        RetainPtr transform = [NSValue valueWithCATransform3D:computedTransform];
        m_transformPresentationModifier = adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"transform" initialValue:transform.get() additive:NO group:m_presentationModifierGroup.get()]);
        [layer addPresentationModifier:m_transformPresentationModifier.get()];
        m_lastAppliedTransform = WTF::move(computedTransform);
    }

    [m_presentationModifierGroup flushWithTransaction];
}

auto RemoteAnimationStack::applyEffects() -> ValuesChanged
{
    auto changed = ValuesChanged::No;
    if (!m_presentationModifierGroup)
        return changed;

    auto computedValues = computeValues();

    if (!m_filterPresentationModifiers.isEmpty()) {
        auto& filter = m_filtersAffectBackdrop ? computedValues.backdropFilter : computedValues.filter;
        if (m_lastAppliedFilter != filter) {
            WebCore::PlatformCAFilters::updatePresentationModifiers(filter, m_filterPresentationModifiers);
            m_lastAppliedFilter = filter;
            changed = ValuesChanged::Yes;
        }
    }

    if (m_opacityPresentationModifier) {
        // Setting 0.f here may result in a stale value for the CAPresentationModifier.
        // FIXME: rdar://188819673.
        auto opacity = computedValues.opacity.value ? computedValues.opacity.value : std::numeric_limits<float>::min();
        if (m_lastAppliedOpacity != opacity) {
            RetainPtr opacityValue = @(opacity);
            [m_opacityPresentationModifier setValue:opacityValue.get()];
            m_lastAppliedOpacity = opacity;
            changed = ValuesChanged::Yes;
        }
    }

    if (m_transformPresentationModifier) {
        auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);
        if (m_lastAppliedTransform != computedTransform) {
            RetainPtr transform = [NSValue valueWithCATransform3D:computedTransform];
            [m_transformPresentationModifier setValue:transform.get()];
            m_lastAppliedTransform = WTF::move(computedTransform);
            changed = ValuesChanged::Yes;
        }
    }

    if (changed == ValuesChanged::Yes)
        [m_presentationModifierGroup flushWithTransaction];

    return changed;
}
#endif

void RemoteAnimationStack::applyEffectsFromMainThread(PlatformLayer *layer, bool backdropRootIsOpaque) const
{
    auto computedValues = computeValues();

    if (m_affectedLayerProperties.contains(LayerProperty::Filter))
        WebCore::PlatformCAFilters::setFiltersOnLayer(layer, m_filtersAffectBackdrop ? computedValues.backdropFilter : computedValues.filter, backdropRootIsOpaque);

    // We compare against the layer's current value rather than the last value we applied since
    // a remote layer tree commit may have reset the property to an updated base value.
    if (m_affectedLayerProperties.contains(LayerProperty::Opacity) && [layer opacity] != computedValues.opacity.value)
        [layer setOpacity:computedValues.opacity.value];

    if (m_affectedLayerProperties.contains(LayerProperty::Transform)) {
        CATransform3D computedTransform = computedValues.computedTransformationMatrix(m_bounds);
        if (!CATransform3DEqualToTransform([layer transform], computedTransform))
            [layer setTransform:computedTransform];
    }
}

WebCore::AcceleratedEffectValues RemoteAnimationStack::computeValues() const
{
    auto values = m_baseValues;
    for (auto& animation : borrow(m_animations))
        animation->apply(values);
    return values;
}

void RemoteAnimationStack::clear(PlatformLayer *layer)
{
#if PLATFORM(MAC)
    if (!m_presentationModifierGroup)
        return;

    for (auto& filterPresentationModifier : borrow(m_filterPresentationModifiers))
        [layer removePresentationModifier:filterPresentationModifier.second.get()];
    if (m_opacityPresentationModifier)
        [layer removePresentationModifier:m_opacityPresentationModifier.get()];
    if (m_transformPresentationModifier)
        [layer removePresentationModifier:m_transformPresentationModifier.get()];

    [m_presentationModifierGroup flushWithTransaction];

    m_filterPresentationModifiers.clear();
    m_opacityPresentationModifier = nil;
    m_transformPresentationModifier = nil;
    m_presentationModifierGroup = nil;
    m_lastAppliedOpacity = std::nullopt;
    m_lastAppliedTransform = std::nullopt;
    m_lastAppliedFilter = std::nullopt;
#endif
}

bool RemoteAnimationStack::isDependentOnScrollingNodeWithID(WebCore::ScrollingNodeID scrollingNodeID) const
{
    return m_animations.containsIf([scrollingNodeID](auto& animation) {
        RefPtr progressBasedTimeline = dynamicDowncast<RemoteProgressBasedTimeline>(animation->timeline());
        return progressBasedTimeline && progressBasedTimeline->source() == scrollingNodeID;
    });
}

Ref<JSON::Object> RemoteAnimationStack::toJSONForTesting() const
{
    Ref convertedAnimations = JSON::Array::create();
    OptionSet<WebCore::AcceleratedEffectProperty> animatedProperties;

    for (auto& animation : borrow(m_animations)) {
        animatedProperties.add(animation->animatedProperties());
        convertedAnimations->pushObject(animation->toJSONForTesting());
    }

    Ref object = JSON::Object::create();
    object->setArray("animations"_s, WTF::move(convertedAnimations));
    object->setObject("baseValues"_s, WebKit::toJSONForTesting(m_baseValues, animatedProperties));
    return object;
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATIONS)
