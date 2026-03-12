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
#include "StyleJustifyContent.h"

#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

StyleContentAlignmentData JustifyContent::resolve(std::optional<StyleContentAlignmentData> valueForNormal) const
{
    auto resolveOverflowPosition = [&](auto contentPosition) -> StyleContentAlignmentData {
        switch (overflowPosition()) {
        case OverflowPositionKind::None:
            return { contentPosition, ContentDistribution::Default };
        case OverflowPositionKind::Unsafe:
            return { contentPosition, ContentDistribution::Default, OverflowAlignment::Unsafe };
        case OverflowPositionKind::Safe:
            return { contentPosition, ContentDistribution::Default, OverflowAlignment::Safe };
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    switch (primary()) {
    case PrimaryKind::Normal:
        return valueForNormal.value_or(ContentPosition::Normal);
    case PrimaryKind::SpaceBetween:
        return { ContentPosition::Normal, ContentDistribution::SpaceBetween };
    case PrimaryKind::SpaceAround:
        return { ContentPosition::Normal, ContentDistribution::SpaceAround };
    case PrimaryKind::SpaceEvenly:
        return { ContentPosition::Normal, ContentDistribution::SpaceEvenly };
    case PrimaryKind::Stretch:
        return { ContentPosition::Normal, ContentDistribution::Stretch };
    case PrimaryKind::Center:
        return resolveOverflowPosition(ContentPosition::Center);
    case PrimaryKind::Start:
        return resolveOverflowPosition(ContentPosition::Start);
    case PrimaryKind::End:
        return resolveOverflowPosition(ContentPosition::End);
    case PrimaryKind::FlexStart:
        return resolveOverflowPosition(ContentPosition::FlexStart);
    case PrimaryKind::FlexEnd:
        return resolveOverflowPosition(ContentPosition::FlexEnd);
    case PrimaryKind::Left:
        return resolveOverflowPosition(ContentPosition::Left);
    case PrimaryKind::Right:
        return resolveOverflowPosition(ContentPosition::Right);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Conversion

auto CSSValueConversion<JustifyContent>::operator()(BuilderState& state, const CSSValue& value) -> JustifyContent
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        // <normal>
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        // <content-distribution>
        case CSSValueSpaceBetween:
            return CSS::Keyword::SpaceBetween { };
        case CSSValueSpaceAround:
            return CSS::Keyword::SpaceAround { };
        case CSSValueSpaceEvenly:
            return CSS::Keyword::SpaceEvenly { };
        case CSSValueStretch:
            return CSS::Keyword::Stretch { };
        // <overflow-position>? [ <content-position> | left | right ]
        case CSSValueCenter:
            return CSS::Keyword::Center { };
        case CSSValueStart:
            return CSS::Keyword::Start { };
        case CSSValueEnd:
            return CSS::Keyword::End { };
        case CSSValueFlexStart:
            return CSS::Keyword::FlexStart { };
        case CSSValueFlexEnd:
            return CSS::Keyword::FlexEnd { };
        case CSSValueLeft:
            return CSS::Keyword::Left { };
        case CSSValueRight:
            return CSS::Keyword::Right { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }

    auto pair = requiredPairDowncast<CSSPrimitiveValue>(state, value);
    if (!pair)
        return CSS::Keyword::Normal { };

    auto consumeAfterOverflowPosition = [&](auto overflowPosition, auto secondValueID) -> JustifyContent {
        switch (secondValueID) {
        case CSSValueCenter:
            return { CSS::Keyword::Center { }, overflowPosition };
        case CSSValueStart:
            return { CSS::Keyword::Start { }, overflowPosition };
        case CSSValueEnd:
            return { CSS::Keyword::End { }, overflowPosition };
        case CSSValueFlexStart:
            return { CSS::Keyword::FlexStart { }, overflowPosition };
        case CSSValueFlexEnd:
            return { CSS::Keyword::FlexEnd { }, overflowPosition };
        case CSSValueLeft:
            return { CSS::Keyword::Left { }, overflowPosition };
        case CSSValueRight:
            return { CSS::Keyword::Right { }, overflowPosition };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    };

    switch (pair->first->valueID()) {
    case CSSValueUnsafe:
        return consumeAfterOverflowPosition(CSS::Keyword::Unsafe { }, pair->second->valueID());
    case CSSValueSafe:
        return consumeAfterOverflowPosition(CSS::Keyword::Safe { }, pair->second->valueID());
    default:
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    }
}

} // namespace Style
} // namespace WebCore
