/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WillChangeData.h"

namespace WebCore {

bool WillChangeData::operator==(const WillChangeData& other) const
{
    return m_animatableFeatures == other.m_animatableFeatures;
}

bool WillChangeData::containsScrollPosition() const
{
    for (const auto& feature : m_animatableFeatures) {
        if (feature.feature() == ScrollPosition)
            return true;
    }
    return false;
}

bool WillChangeData::containsContents() const
{
    for (const auto& feature : m_animatableFeatures) {
        if (feature.feature() == Contents)
            return true;
    }
    return false;
}

bool WillChangeData::containsProperty(CSSPropertyID property) const
{
    for (const auto& feature : m_animatableFeatures) {
        if (feature.property() == property)
            return true;
    }
    return false;
}

// "If any non-initial value of a property would create a stacking context on the element,
// specifying that property in will-change must create a stacking context on the element."
bool WillChangeData::propertyCreatesStackingContext(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyPerspective:
    case CSSPropertyTransform:
    case CSSPropertyTransformStyle:
    case CSSPropertyWebkitTransformStyle:
    case CSSPropertyClipPath:
    case CSSPropertyWebkitClipPath:
    case CSSPropertyMask:
    case CSSPropertyOpacity:
    case CSSPropertyPosition:
    case CSSPropertyZIndex:
    case CSSPropertyWebkitBoxReflect:
#if ENABLE(CSS_COMPOSITING)
    case CSSPropertyMixBlendMode:
    case CSSPropertyIsolation:
#endif
    case CSSPropertyFilter:
#if ENABLE(FILTERS_LEVEL_2)
    case CSSPropertyWebkitBackdropFilter:
#endif
    case CSSPropertyWebkitMask:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskBoxImage:
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    case CSSPropertyWebkitOverflowScrolling:
#endif
        return true;
    default:
        return false;
    }
}

static bool propertyTriggersCompositing(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyOpacity:
    case CSSPropertyFilter:
#if ENABLE(FILTERS_LEVEL_2)
    case CSSPropertyWebkitBackdropFilter:
#endif
        return true;
    default:
        return false;
    }
}

static bool propertyTriggersCompositingOnBoxesOnly(CSSPropertyID property)
{
    // Don't trigger for perspective and transform-style, because those
    // only do compositing if they have a 3d-transformed descendant and
    // we don't want to do compositing all the time.
    // Similarly, we don't want -webkit-overflow-scrolling-touch to
    // always composite if there's no scrollable overflow.
    switch (property) {
    case CSSPropertyTransform:
        return true;
    default:
        return false;
    }
}

void WillChangeData::addFeature(Feature feature, CSSPropertyID propertyID)
{
    ASSERT(feature == Property || propertyID == CSSPropertyInvalid);
    m_animatableFeatures.append(AnimatableFeature(feature, propertyID));

    m_canCreateStackingContext |= propertyCreatesStackingContext(propertyID);

    m_canTriggerCompositingOnInline |= propertyTriggersCompositing(propertyID);
    m_canTriggerCompositing |= m_canTriggerCompositingOnInline | propertyTriggersCompositingOnBoxesOnly(propertyID);
}

WillChangeData::FeaturePropertyPair WillChangeData::featureAt(size_t index) const
{
    if (index >= m_animatableFeatures.size())
        return FeaturePropertyPair(Invalid, CSSPropertyInvalid);

    return m_animatableFeatures[index].featurePropertyPair();
}

} // namespace WebCore
