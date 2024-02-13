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
#import "RemoteAcceleratedEffectStack.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#import <pal/spi/cocoa/QuartzCoreSPI.h>
#include <wtf/IsoMallocInlines.h>

namespace WebKit {

WTF_MAKE_ISO_ALLOCATED_IMPL(RemoteAcceleratedEffectStack);

Ref<RemoteAcceleratedEffectStack> RemoteAcceleratedEffectStack::create(WebCore::FloatRect bounds, Seconds acceleratedTimelineTimeOrigin)
{
    return adoptRef(*new RemoteAcceleratedEffectStack(bounds, acceleratedTimelineTimeOrigin));
}

RemoteAcceleratedEffectStack::RemoteAcceleratedEffectStack(WebCore::FloatRect bounds, Seconds acceleratedTimelineTimeOrigin)
    : m_bounds(bounds)
    , m_acceleratedTimelineTimeOrigin(acceleratedTimelineTimeOrigin)
{
}

void RemoteAcceleratedEffectStack::setEffects(AcceleratedEffects&& effects)
{
    AcceleratedEffectStack::setEffects(WTFMove(effects));

    bool affectsOpacity = false;
    bool affectsTransform = false;

    for (auto& effect : m_primaryLayerEffects) {
        auto& properties = effect->animatedProperties();
        affectsOpacity = affectsOpacity || properties.contains(AcceleratedEffectProperty::Opacity);
        affectsTransform = affectsTransform || properties.containsAny(transformRelatedAcceleratedProperties);
        if (affectsOpacity && affectsTransform)
            break;
    }

    if (affectsOpacity)
        m_affectedLayerProperties.add(LayerProperty::Opacity);
    if (affectsTransform)
        m_affectedLayerProperties.add(LayerProperty::Transform);
}

#if PLATFORM(MAC)
void RemoteAcceleratedEffectStack::initEffectsFromMainThread(PlatformLayer *layer, MonotonicTime now)
{
    ASSERT(!m_opacityPresentationModifier);
    ASSERT(!m_transformPresentationModifier);
    ASSERT(!m_presentationModifierGroup);

    if (!m_affectedLayerProperties.containsAny({ LayerProperty::Opacity, LayerProperty::Transform }))
        return;

    auto computedValues = computeValues(now);

    auto numberOfPresentationModifiers = m_affectedLayerProperties.containsAll({ LayerProperty::Opacity, LayerProperty::Transform }) ? 2 : 1;
    m_presentationModifierGroup = [CAPresentationModifierGroup groupWithCapacity:numberOfPresentationModifiers];

    if (m_affectedLayerProperties.contains(LayerProperty::Opacity)) {
        auto *opacity = @(computedValues.opacity);
        m_opacityPresentationModifier = adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"opacity" initialValue:opacity additive:NO group:m_presentationModifierGroup.get()]);
        [layer addPresentationModifier:m_opacityPresentationModifier.get()];
    }

    if (m_affectedLayerProperties.contains(LayerProperty::Transform)) {
        auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);
        auto *transform = [NSValue valueWithCATransform3D:computedTransform];
        m_transformPresentationModifier = adoptNS([[CAPresentationModifier alloc] initWithKeyPath:@"transform" initialValue:transform additive:NO group:m_presentationModifierGroup.get()]);
        [layer addPresentationModifier:m_transformPresentationModifier.get()];
    }

    [m_presentationModifierGroup flushWithTransaction];
}

void RemoteAcceleratedEffectStack::applyEffectsFromScrollingThread(MonotonicTime now) const
{
    if (!m_affectedLayerProperties.containsAny({ LayerProperty::Opacity, LayerProperty::Transform }))
        return;

    ASSERT(m_opacityPresentationModifier || m_transformPresentationModifier);
    ASSERT(m_presentationModifierGroup);

    auto computedValues = computeValues(now);

    if (m_opacityPresentationModifier) {
        auto *opacity = @(computedValues.opacity);
        [m_opacityPresentationModifier setValue:opacity];
    }

    if (m_transformPresentationModifier) {
        auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);
        auto *transform = [NSValue valueWithCATransform3D:computedTransform];
        [m_transformPresentationModifier setValue:transform];
    }

    [m_presentationModifierGroup flush];
}
#endif

void RemoteAcceleratedEffectStack::applyEffectsFromMainThread(PlatformLayer *layer, MonotonicTime now) const
{
    if (!m_affectedLayerProperties.containsAny({ LayerProperty::Opacity, LayerProperty::Transform }))
        return;

    auto computedValues = computeValues(now);

    if (m_affectedLayerProperties.contains(LayerProperty::Opacity))
        [layer setOpacity:computedValues.opacity];

    if (m_affectedLayerProperties.contains(LayerProperty::Transform)) {
        auto computedTransform = computedValues.computedTransformationMatrix(m_bounds);
        [layer setTransform:computedTransform];
    }
}

AcceleratedEffectValues RemoteAcceleratedEffectStack::computeValues(MonotonicTime now) const
{
    auto values = m_baseValues;
    auto currentTime = now.secondsSinceEpoch() - m_acceleratedTimelineTimeOrigin;
    for (auto& effect : m_primaryLayerEffects)
        effect->apply(currentTime, values, m_bounds);
    return values;
}

void RemoteAcceleratedEffectStack::clear(PlatformLayer *layer)
{
    if (!m_presentationModifierGroup) {
        ASSERT(!m_opacityPresentationModifier && !m_transformPresentationModifier);
        return;
    }

    ASSERT(m_opacityPresentationModifier || m_transformPresentationModifier);

    if (m_opacityPresentationModifier)
        [layer removePresentationModifier:m_opacityPresentationModifier.get()];
    if (m_transformPresentationModifier)
        [layer removePresentationModifier:m_transformPresentationModifier.get()];

    [m_presentationModifierGroup flushWithTransaction];

    m_opacityPresentationModifier = nil;
    m_transformPresentationModifier = nil;
    m_presentationModifierGroup = nil;
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
