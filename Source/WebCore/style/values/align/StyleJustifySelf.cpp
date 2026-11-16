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
#include "StyleJustifySelf.h"

#include "AnchorPositionEvaluator.h"
#include "CSSKeywordValue.h"
#include "StyleBuilderChecking.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleJustifyItems.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

StyleSelfAlignmentData JustifySelf::resolve(const Style::ComputedStyle* containerStyle) const
{
    if (PrimaryKind::Auto == primary())
        return containerStyle ? containerStyle->justifyItems().resolve() : StyleSelfAlignmentData { ItemPosition::Normal };

    auto resolveOverflowPosition = [&](auto itemPosition) -> StyleSelfAlignmentData {
        switch (overflowPosition()) {
        case OverflowPositionKind::None:
            return { itemPosition };
        case OverflowPositionKind::Unsafe:
            return { itemPosition, OverflowAlignment::Unsafe };
        case OverflowPositionKind::Safe:
            return { itemPosition, OverflowAlignment::Safe };
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    switch (primary()) {
    case PrimaryKind::Auto:
        ASSERT_NOT_REACHED();
        return { ItemPosition::Auto };
    case PrimaryKind::Normal:
        return resolveOverflowPosition(ItemPosition::Normal);
    case PrimaryKind::Stretch:
        return { ItemPosition::Stretch };
    case PrimaryKind::Baseline:
        if (baselineAlignmentPreference() == BaselineAlignmentPreferenceKind::Last)
            return { ItemPosition::LastBaseline };
        return { ItemPosition::Baseline };
    case PrimaryKind::Center:
        return resolveOverflowPosition(ItemPosition::Center);
    case PrimaryKind::Start:
        return resolveOverflowPosition(ItemPosition::Start);
    case PrimaryKind::End:
        return resolveOverflowPosition(ItemPosition::End);
    case PrimaryKind::SelfStart:
        return resolveOverflowPosition(ItemPosition::SelfStart);
    case PrimaryKind::SelfEnd:
        return resolveOverflowPosition(ItemPosition::SelfEnd);
    case PrimaryKind::FlexStart:
        return resolveOverflowPosition(ItemPosition::FlexStart);
    case PrimaryKind::FlexEnd:
        return resolveOverflowPosition(ItemPosition::FlexEnd);
    case PrimaryKind::Left:
        return resolveOverflowPosition(ItemPosition::Left);
    case PrimaryKind::Right:
        return resolveOverflowPosition(ItemPosition::Right);
    case PrimaryKind::AnchorCenter:
        return resolveOverflowPosition(ItemPosition::AnchorCenter);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Conversion

auto CSSValueConversion<JustifySelf>::operator()(BuilderState& state, const CSSValue& value) -> JustifySelf
{
    auto applyPositionTryFallbackTactics = [](auto& state, auto position) -> CSSValueID {
        // Flip the position according to position-try fallback, if specified.
        if (auto positionTryFallback = state.positionTryFallback())
            position = AnchorPositionEvaluator::resolvePositionTryFallbackValueForSelfPosition(state.cssPropertyID(), position, state.style().writingMode(), *positionTryFallback);
        return position;
    };

    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (applyPositionTryFallbackTactics(state, keywordValue->valueID())) {
        // auto
        case CSSValueID::Auto:
            return CSS::Keyword::Auto { };
        // normal
        case CSSValueID::Normal:
            return CSS::Keyword::Normal { };
        // stretch
        case CSSValueID::Stretch:
            return CSS::Keyword::Stretch { };
        // <baseline-position>
        case CSSValueID::Baseline:
            return CSS::Keyword::Baseline { };
        // <overflow-position>? [ <self-position> | left | right ]
        case CSSValueID::Center:
            return CSS::Keyword::Center { };
        case CSSValueID::Start:
            return CSS::Keyword::Start { };
        case CSSValueID::End:
            return CSS::Keyword::End { };
        case CSSValueID::SelfStart:
            return CSS::Keyword::SelfStart { };
        case CSSValueID::SelfEnd:
            return CSS::Keyword::SelfEnd { };
        case CSSValueID::FlexStart:
            return CSS::Keyword::FlexStart { };
        case CSSValueID::FlexEnd:
            return CSS::Keyword::FlexEnd { };
        case CSSValueID::Left:
            return CSS::Keyword::Left { };
        case CSSValueID::Right:
            return CSS::Keyword::Right { };
        case CSSValueID::AnchorCenter:
            return CSS::Keyword::AnchorCenter { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    }

    auto pair = requiredPairDowncast<CSSKeywordValue>(state, value);
    if (!pair)
        return CSS::Keyword::Auto { };

    auto consumeAfterBaselinePositionPreference = [&](auto baselinePositionPreference, auto secondValueID) -> JustifySelf {
        switch (secondValueID) {
        case CSSValueID::Baseline:
            return { CSS::Keyword::Baseline { }, { baselinePositionPreference } };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    };

    auto consumeAfterOverflowPosition = [&](auto overflowPosition, auto secondValueID) -> JustifySelf {
        switch (applyPositionTryFallbackTactics(state, secondValueID)) {
        case CSSValueID::Normal:
            return { CSS::Keyword::Normal { }, overflowPosition };
        case CSSValueID::Start:
            return { CSS::Keyword::Start { }, overflowPosition };
        case CSSValueID::End:
            return { CSS::Keyword::End { }, overflowPosition };
        case CSSValueID::Center:
            return { CSS::Keyword::Center { }, overflowPosition };
        case CSSValueID::SelfStart:
            return { CSS::Keyword::SelfStart { }, overflowPosition };
        case CSSValueID::SelfEnd:
            return { CSS::Keyword::SelfEnd { }, overflowPosition };
        case CSSValueID::FlexStart:
            return { CSS::Keyword::FlexStart { }, overflowPosition };
        case CSSValueID::FlexEnd:
            return { CSS::Keyword::FlexEnd { }, overflowPosition };
        case CSSValueID::Left:
            return { CSS::Keyword::Left { }, overflowPosition };
        case CSSValueID::Right:
            return { CSS::Keyword::Right { }, overflowPosition };
        case CSSValueID::AnchorCenter:
            return { CSS::Keyword::AnchorCenter { }, overflowPosition };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    };

    switch (pair->first->valueID()) {
    // <baseline-position>
    case CSSValueID::First:
        return consumeAfterBaselinePositionPreference(CSS::Keyword::First { }, pair->second->valueID());
    case CSSValueID::Last:
        return consumeAfterBaselinePositionPreference(CSS::Keyword::Last { }, pair->second->valueID());
    // <overflow-position>? [ <self-position> | left | right ]
    case CSSValueID::Unsafe:
        return consumeAfterOverflowPosition(CSS::Keyword::Unsafe { }, pair->second->valueID());
    case CSSValueID::Safe:
        return consumeAfterOverflowPosition(CSS::Keyword::Safe { }, pair->second->valueID());
    default:
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Auto { };
    }
}

} // namespace Style
} // namespace WebCore
