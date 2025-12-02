/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleWillChange.h"

#include "Settings.h"
#include "StyleBuilderChecking.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WillChangeAnimatableFeatures::Data);

// MARK: WillChangeAnimatableFeature

WillChangeAnimatableFeature::WillChangeAnimatableFeature(Feature willChange, CSSPropertyID willChangeProperty)
{
    switch (willChange) {
    case Feature::Property:
        ASSERT(willChangeProperty != CSSPropertyInvalid);
        m_cssPropertyID = willChangeProperty;
        [[fallthrough]];
    case Feature::ScrollPosition:
    case Feature::Contents:
        m_feature = willChange;
        break;
    }
}

// "If any non-initial value of a property would create a stacking context on the element,
// specifying that property in will-change must create a stacking context on the element."
bool WillChangeAnimatableFeature::propertyCreatesStackingContext(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyPerspective:
    case CSSPropertyWebkitPerspective:
    case CSSPropertyScale:
    case CSSPropertyRotate:
    case CSSPropertyTranslate:
    case CSSPropertyTransform:
    case CSSPropertyTransformStyle:
    case CSSPropertyOffsetPath:
    case CSSPropertyClipPath:
    case CSSPropertyMask:
    case CSSPropertyWebkitMask:
    case CSSPropertyOpacity:
    case CSSPropertyPosition:
    case CSSPropertyZIndex:
    case CSSPropertyWebkitBoxReflect:
    case CSSPropertyMixBlendMode:
    case CSSPropertyIsolation:
    case CSSPropertyFilter:
    case CSSPropertyBackdropFilter:
    case CSSPropertyWebkitBackdropFilter:
    case CSSPropertyMaskImage:
    case CSSPropertyMaskBorder:
    case CSSPropertyWebkitMaskBoxImage:
#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
    case CSSPropertyWebkitOverflowScrolling:
#endif
    case CSSPropertyViewTransitionName:
    case CSSPropertyContain:
        return true;
    default:
        return false;
    }
}

bool WillChangeAnimatableFeature::propertyTriggersCompositing(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyOpacity:
    case CSSPropertyFilter:
    case CSSPropertyBackdropFilter:
    case CSSPropertyWebkitBackdropFilter:
        return true;
    default:
        return false;
    }
}

bool WillChangeAnimatableFeature::propertyTriggersCompositingOnBoxesOnly(CSSPropertyID property)
{
    // Don't trigger for perspective and transform-style, because those
    // only do compositing if they have a 3d-transformed descendant and
    // we don't want to do compositing all the time.
    // Similarly, we don't want -webkit-overflow-scrolling-touch to
    // always composite if there's no scrollable overflow.
    switch (property) {
    case CSSPropertyScale:
    case CSSPropertyRotate:
    case CSSPropertyTranslate:
    case CSSPropertyTransform:
    case CSSPropertyOffsetPath:
        return true;
    default:
        return false;
    }
}

// MARK: WillChangeAnimatableFeatures::Data

bool WillChangeAnimatableFeatures::Data::operator==(const WillChangeAnimatableFeatures::Data& other) const
{
    return m_animatableFeatures == other.m_animatableFeatures;
}

bool WillChangeAnimatableFeatures::Data::containsScrollPosition() const
{
    return std::ranges::any_of(m_animatableFeatures, [](auto& feature) { return feature.feature() == Feature::ScrollPosition; });
}

bool WillChangeAnimatableFeatures::Data::containsContents() const
{
    return std::ranges::any_of(m_animatableFeatures, [](auto& feature) { return feature.feature() == Feature::Contents; });
}

bool WillChangeAnimatableFeatures::Data::containsProperty(CSSPropertyID property) const
{
    return std::ranges::any_of(m_animatableFeatures, [&](auto& feature) { return feature.property() == property; });
}

bool WillChangeAnimatableFeatures::Data::createsContainingBlockForAbsolutelyPositioned(bool isRootElement) const
{
    return createsContainingBlockForOutOfFlowPositioned(isRootElement)
        || containsProperty(CSSPropertyPosition);
}

bool WillChangeAnimatableFeatures::Data::createsContainingBlockForOutOfFlowPositioned(bool isRootElement) const
{
    return containsProperty(CSSPropertyPerspective)
        // CSS transforms
        || containsProperty(CSSPropertyTransform)
        || containsProperty(CSSPropertyTransformStyle)
        || containsProperty(CSSPropertyTranslate)
        || containsProperty(CSSPropertyRotate)
        || containsProperty(CSSPropertyScale)
        || containsProperty(CSSPropertyOffsetPath)
        // CSS containment
        || containsProperty(CSSPropertyContain)
        // CSS filter & backdrop-filter
        || (containsProperty(CSSPropertyBackdropFilter) && !isRootElement)
        || (containsProperty(CSSPropertyWebkitBackdropFilter) && !isRootElement)
        || (containsProperty(CSSPropertyFilter) && !isRootElement);
}

bool WillChangeAnimatableFeatures::Data::canBeBackdropRoot() const
{
    return containsProperty(CSSPropertyOpacity)
        || containsProperty(CSSPropertyBackdropFilter)
        || containsProperty(CSSPropertyWebkitBackdropFilter)
        || containsProperty(CSSPropertyClipPath)
        || containsProperty(CSSPropertyFilter)
        || containsProperty(CSSPropertyMixBlendMode)
        || containsProperty(CSSPropertyMask)
        || containsProperty(CSSPropertyViewTransitionName);
}

void WillChangeAnimatableFeatures::Data::addFeature(Feature feature, CSSPropertyID propertyID)
{
    ASSERT(feature == Feature::Property || propertyID == CSSPropertyInvalid);
    m_animatableFeatures.append(WillChangeAnimatableFeature(feature, propertyID));

    m_canCreateStackingContext |= WillChangeAnimatableFeature::propertyCreatesStackingContext(propertyID);
    m_canTriggerCompositingOnInline |= WillChangeAnimatableFeature::propertyTriggersCompositing(propertyID);
    m_canTriggerCompositing |= m_canTriggerCompositingOnInline | WillChangeAnimatableFeature::propertyTriggersCompositingOnBoxesOnly(propertyID);
}

// MARK: - Conversion

auto CSSValueConversion<WillChange>::operator()(BuilderState& state, const CSSValue& value) -> WillChange
{
    // FIXME: This should also be storing <custom-ident> values that aren't valid CSSPropertyIDs for computed value serialization.

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueAuto:
            return CSS::Keyword::Auto { };
        case CSSValueScrollPosition:
            return WillChangeAnimatableFeatures { WillChangeAnimatableFeature::Feature::ScrollPosition };
        case CSSValueContents:
            return WillChangeAnimatableFeatures { WillChangeAnimatableFeature::Feature::Contents };
        default:
            if (primitiveValue->isPropertyID()) {
                if (auto propertyID = primitiveValue->propertyID(); isExposed(propertyID, &state.document().settings()))
                    return WillChangeAnimatableFeatures { WillChangeAnimatableFeature::Feature::Property, propertyID };
            }

            return CSS::Keyword::Auto { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue, 1>(state, value);
    if (!list)
        return CSS::Keyword::Auto { };

    auto result = WillChangeAnimatableFeatures { };

    for (Ref item : *list) {
        switch (item->valueID()) {
        case CSSValueScrollPosition:
            result.addFeature(WillChangeAnimatableFeature::Feature::ScrollPosition);
            break;
        case CSSValueContents:
            result.addFeature(WillChangeAnimatableFeature::Feature::Contents);
            break;
        default:
            if (item->isPropertyID()) {
                if (auto propertyID = item->propertyID(); isExposed(propertyID, &state.document().settings())) {
                    result.addFeature(WillChangeAnimatableFeature::Feature::Property, propertyID);
                    break;
                }
            }
            break;
        }
    }

    return result;
}

} // namespace Style
} // namespace WebCore
