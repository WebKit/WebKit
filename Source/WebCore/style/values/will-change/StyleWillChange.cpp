/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSCustomIdentValue.h"
#include "CSSKeywordValue.h"
#include "CSSPropertyParser.h"
#include "Settings.h"
#include "StyleBuilderChecking.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ZippedRange.h>

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WillChangeAnimatableFeatures::Data);

// MARK: WillChangeAnimatableFeature

// "If any non-initial value of a property would create a stacking context on the element,
// specifying that property in will-change must create a stacking context on the element."
bool WillChangeAnimatableFeature::propertyCreatesStackingContext(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyID::Perspective:
    case CSSPropertyID::WebkitPerspective:
    case CSSPropertyID::Scale:
    case CSSPropertyID::Rotate:
    case CSSPropertyID::Translate:
    case CSSPropertyID::Transform:
    case CSSPropertyID::TransformStyle:
    case CSSPropertyID::OffsetPath:
    case CSSPropertyID::ClipPath:
    case CSSPropertyID::Mask:
    case CSSPropertyID::WebkitMask:
    case CSSPropertyID::Opacity:
    case CSSPropertyID::Position:
    case CSSPropertyID::ZIndex:
    case CSSPropertyID::WebkitBoxReflect:
    case CSSPropertyID::MixBlendMode:
    case CSSPropertyID::Isolation:
    case CSSPropertyID::Filter:
    case CSSPropertyID::BackdropFilter:
    case CSSPropertyID::WebkitBackdropFilter:
    case CSSPropertyID::MaskImage:
    case CSSPropertyID::MaskBorder:
    case CSSPropertyID::WebkitMaskBoxImage:
#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
    case CSSPropertyID::WebkitOverflowScrolling:
#endif
    case CSSPropertyID::ViewTransitionName:
    case CSSPropertyID::Contain:
        return true;
    default:
        return false;
    }
}

bool WillChangeAnimatableFeature::propertyTriggersCompositing(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyID::Opacity:
    case CSSPropertyID::Filter:
    case CSSPropertyID::BackdropFilter:
    case CSSPropertyID::WebkitBackdropFilter:
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
    case CSSPropertyID::Scale:
    case CSSPropertyID::Rotate:
    case CSSPropertyID::Translate:
    case CSSPropertyID::Transform:
    case CSSPropertyID::OffsetPath:
        return true;
    default:
        return false;
    }
}

// MARK: WillChangeAnimatableFeatures::Data

void WillChangeAnimatableFeatures::Data::initializeCachedChecks()
{
    for (auto& feature : *this) {
        if (auto* value = std::get_if<WillChangeAnimatableFeature::CustomIdentWithCachedPropertyID>(&feature.value)) {
            auto propertyID = value->propertyID;

            m_canCreateStackingContext |= WillChangeAnimatableFeature::propertyCreatesStackingContext(propertyID);
            m_canTriggerCompositingOnInline |= WillChangeAnimatableFeature::propertyTriggersCompositing(propertyID);
            m_canTriggerCompositing |= m_canTriggerCompositingOnInline | WillChangeAnimatableFeature::propertyTriggersCompositingOnBoxesOnly(propertyID);
        }
    }
}

bool WillChangeAnimatableFeatures::Data::operator==(const WillChangeAnimatableFeatures::Data& other) const
{
    return std::ranges::equal(*this, other);
}

bool WillChangeAnimatableFeatures::Data::containsScrollPosition() const
{
    return std::ranges::any_of(*this, [](auto& feature) {
        return WTF::holdsAlternative<CSS::Keyword::ScrollPosition>(feature);
    });
}

bool WillChangeAnimatableFeatures::Data::containsContents() const
{
    return std::ranges::any_of(*this, [](auto& feature) {
        return WTF::holdsAlternative<CSS::Keyword::Contents>(feature);
    });
}

bool WillChangeAnimatableFeatures::Data::containsProperty(CSSPropertyID property) const
{
    return std::ranges::any_of(*this, [&](auto& feature) {
        if (auto* value = std::get_if<WillChangeAnimatableFeature::CustomIdentWithCachedPropertyID>(&feature.value))
            return *value == property;
        return false;
    });
}

bool WillChangeAnimatableFeatures::Data::createsContainingBlockForAbsolutelyPositioned(bool isRootElement) const
{
    return createsContainingBlockForOutOfFlowPositioned(isRootElement)
        || containsProperty(CSSPropertyID::Position);
}

bool WillChangeAnimatableFeatures::Data::createsContainingBlockForOutOfFlowPositioned(bool isRootElement) const
{
    return containsProperty(CSSPropertyID::Perspective)
        || containsProperty(CSSPropertyID::WebkitPerspective)
        // CSS transforms
        || containsProperty(CSSPropertyID::Transform)
        || containsProperty(CSSPropertyID::TransformStyle)
        || containsProperty(CSSPropertyID::Translate)
        || containsProperty(CSSPropertyID::Rotate)
        || containsProperty(CSSPropertyID::Scale)
        || containsProperty(CSSPropertyID::OffsetPath)
        // CSS containment
        || containsProperty(CSSPropertyID::Contain)
        // CSS filter & backdrop-filter
        || (containsProperty(CSSPropertyID::BackdropFilter) && !isRootElement)
        || (containsProperty(CSSPropertyID::WebkitBackdropFilter) && !isRootElement)
        || (containsProperty(CSSPropertyID::Filter) && !isRootElement);
}

bool WillChangeAnimatableFeatures::Data::canBeBackdropRoot() const
{
    return containsProperty(CSSPropertyID::Opacity)
        || containsProperty(CSSPropertyID::BackdropFilter)
        || containsProperty(CSSPropertyID::WebkitBackdropFilter)
        || containsProperty(CSSPropertyID::ClipPath)
        || containsProperty(CSSPropertyID::Filter)
        || containsProperty(CSSPropertyID::MixBlendMode)
        || containsProperty(CSSPropertyID::Mask)
        || containsProperty(CSSPropertyID::ViewTransitionName);
}

// MARK: - Conversion

auto CSSValueConversion<WillChange>::operator()(BuilderState& state, const CSSValue& value) -> WillChange
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueID::Auto:
            return CSS::Keyword::Auto { };
        case CSSValueID::ScrollPosition:
            return WillChangeAnimatableFeatures { CSS::Keyword::ScrollPosition { } };
        case CSSValueID::Contents:
            return WillChangeAnimatableFeatures { CSS::Keyword::Contents { } };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    }

    if (RefPtr customIdentValue = dynamicDowncast<CSSCustomIdentValue>(value)) {
        auto customIdent = toStyle(customIdentValue->customIdent(), state);

        // If the <custom-ident> is a case-insensitive match for a currently enabled CSS property,
        // we store a WillChangeAnimatableFeature::CustomIdentWithCachedPropertyID, caching the
        // property lookup.
        if (auto propertyID = cssPropertyID(customIdent.value); propertyID != CSSPropertyID::Invalid && isExposed(propertyID, &state.document().settings())) {
            return WillChangeAnimatableFeatures { WillChangeAnimatableFeature::CustomIdentWithCachedPropertyID {
                .customIdent = WTF::move(customIdent),
                .propertyID = propertyID,
            } };
        }

        return WillChangeAnimatableFeatures { WTF::move(customIdent) };
    }

    auto list = requiredListDowncast<CSSValueList, CSSValue, 1>(state, value);
    if (!list)
        return CSS::Keyword::Auto { };

    return WillChangeAnimatableFeatures::map(*list, [&](auto& item) -> WillChangeAnimatableFeature {
        if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(item)) {
            switch (keywordValue->valueID()) {
            case CSSValueID::ScrollPosition:
                return CSS::Keyword::ScrollPosition { };
            case CSSValueID::Contents:
                return CSS::Keyword::Contents { };
            default:
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Contents { };
            }
        }

        if (RefPtr customIdentValue = dynamicDowncast<CSSCustomIdentValue>(item)) {
            auto customIdent = toStyle(customIdentValue->customIdent(), state);

            // If the <custom-ident> is a case-insensitive match for a currently enabled CSS property,
            // we store a WillChangeAnimatableFeature::CustomIdentWithCachedPropertyID, caching the
            // property lookup.
            if (auto propertyID = cssPropertyID(customIdent.value); propertyID != CSSPropertyID::Invalid && isExposed(propertyID, &state.document().settings())) {
                return WillChangeAnimatableFeature::CustomIdentWithCachedPropertyID {
                    .customIdent = WTF::move(customIdent),
                    .propertyID = propertyID,
                };
            }

            return customIdent;
        }

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Contents { };
    });
}

} // namespace Style
} // namespace WebCore
