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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AcceleratedEffectStack.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AcceleratedEffectStack);

Ref<AcceleratedEffectStack> AcceleratedEffectStack::create()
{
    return adoptRef(*new AcceleratedEffectStack());
}

AcceleratedEffectStack::AcceleratedEffectStack()
{
}

bool AcceleratedEffectStack::hasEffects() const
{
    return !m_primaryLayerEffects.isEmpty() || !m_backdropLayerEffects.isEmpty();
}

void AcceleratedEffectStack::setEffects(AcceleratedEffects&& effects)
{
    m_primaryLayerEffects.clear();
    m_backdropLayerEffects.clear();

    for (auto& effect : effects) {
        auto& animatedProperties = effect->animatedProperties();

        // If we don't have a keyframe targeting backdrop-filter, we can add the effect
        // as-is to the set of effects targeting the primary layer.
        if (!animatedProperties.contains(AcceleratedEffectProperty::BackdropFilter)) {
            m_primaryLayerEffects.append(effect);
            continue;
        }

        // If the only property targeted is backdrop-filter, we can add the effect
        // as-is to the set of effects targeting the backdrop layer.
        if (animatedProperties.hasExactlyOneBitSet()) {
            m_backdropLayerEffects.append(effect);
            continue;
        }

        // Otherwise, this means we have effects targeting both the primary and backdrop
        // layers, so we must split the effect in two: one for backdrop-filter, and one
        // for all other properties.
        OptionSet<AcceleratedEffectProperty> primaryProperties = animatedProperties - AcceleratedEffectProperty::BackdropFilter;
        m_primaryLayerEffects.append(effect->copyWithProperties(primaryProperties));
        OptionSet<AcceleratedEffectProperty> backdropProperties = { AcceleratedEffectProperty::BackdropFilter };
        m_backdropLayerEffects.append(effect->copyWithProperties(backdropProperties));
    }
}

void AcceleratedEffectStack::setBaseValues(AcceleratedEffectValues&& values)
{
    m_baseValues = WTFMove(values);
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
