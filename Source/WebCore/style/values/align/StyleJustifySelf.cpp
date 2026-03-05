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
#include "RenderStyle.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleBuilderChecking.h"
#include "StyleJustifyItems.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

StyleSelfAlignmentData JustifySelf::resolve(const RenderStyle* containerStyle) const
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
        return { ItemPosition::Normal };
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

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (applyPositionTryFallbackTactics(state, primitiveValue->valueID())) {
        // auto
        case CSSValueAuto:
            return CSS::Keyword::Auto { };
        // normal
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        // stretch
        case CSSValueStretch:
            return CSS::Keyword::Stretch { };
        // <baseline-position>
        case CSSValueBaseline:
            return CSS::Keyword::Baseline { };
        // <overflow-position>? [ <self-position> | left | right ]
        case CSSValueCenter:
            return CSS::Keyword::Center { };
        case CSSValueStart:
            return CSS::Keyword::Start { };
        case CSSValueEnd:
            return CSS::Keyword::End { };
        case CSSValueSelfStart:
            return CSS::Keyword::SelfStart { };
        case CSSValueSelfEnd:
            return CSS::Keyword::SelfEnd { };
        case CSSValueFlexStart:
            return CSS::Keyword::FlexStart { };
        case CSSValueFlexEnd:
            return CSS::Keyword::FlexEnd { };
        case CSSValueLeft:
            return CSS::Keyword::Left { };
        case CSSValueRight:
            return CSS::Keyword::Right { };
        case CSSValueAnchorCenter:
            return CSS::Keyword::AnchorCenter { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    }

    auto pair = requiredPairDowncast<CSSPrimitiveValue>(state, value);
    if (!pair)
        return CSS::Keyword::Auto { };

    auto consumeAfterBaselinePositionPreference = [&](auto baselinePositionPreference, auto secondValueID) -> JustifySelf {
        switch (secondValueID) {
        case CSSValueBaseline:
            return { CSS::Keyword::Baseline { }, { baselinePositionPreference } };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    };

    auto consumeAfterOverflowPosition = [&](auto overflowPosition, auto secondValueID) -> JustifySelf {
        switch (applyPositionTryFallbackTactics(state, secondValueID)) {
        case CSSValueStart:
            return { CSS::Keyword::Start { }, overflowPosition };
        case CSSValueEnd:
            return { CSS::Keyword::End { }, overflowPosition };
        case CSSValueCenter:
            return { CSS::Keyword::Center { }, overflowPosition };
        case CSSValueSelfStart:
            return { CSS::Keyword::SelfStart { }, overflowPosition };
        case CSSValueSelfEnd:
            return { CSS::Keyword::SelfEnd { }, overflowPosition };
        case CSSValueFlexStart:
            return { CSS::Keyword::FlexStart { }, overflowPosition };
        case CSSValueFlexEnd:
            return { CSS::Keyword::FlexEnd { }, overflowPosition };
        case CSSValueLeft:
            return { CSS::Keyword::Left { }, overflowPosition };
        case CSSValueRight:
            return { CSS::Keyword::Right { }, overflowPosition };
        case CSSValueAnchorCenter:
            return { CSS::Keyword::AnchorCenter { }, overflowPosition };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    };

    switch (pair->first->valueID()) {
    // <baseline-position>
    case CSSValueFirst:
        return consumeAfterBaselinePositionPreference(CSS::Keyword::First { }, pair->second->valueID());
    case CSSValueLast:
        return consumeAfterBaselinePositionPreference(CSS::Keyword::Last { }, pair->second->valueID());
    // <overflow-position>? [ <self-position> | left | right ]
    case CSSValueUnsafe:
        return consumeAfterOverflowPosition(CSS::Keyword::Unsafe { }, pair->second->valueID());
    case CSSValueSafe:
        return consumeAfterOverflowPosition(CSS::Keyword::Safe { }, pair->second->valueID());
    default:
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Auto { };
    }
}

} // namespace Style
} // namespace WebCore
