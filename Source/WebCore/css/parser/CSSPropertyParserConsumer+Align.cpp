/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPropertyParserConsumer+Align.h"

#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserState.h"
#include "CSSValueKeywords.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include <optional>

namespace WebCore {
namespace CSSPropertyParserHelpers {

static RefPtr<CSSValue> consumeAlignmentBaseline(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    ASSERT(range.peek().id() == CSSValueBaseline);

    // FIXME: The spec states that <baseline-position> is defined as `<baseline-position> = [ first | last ]? && baseline`, allowing any ordering, but tests expect `[ first | last ]` to always be precede `baseline`.

    range.consumeIncludingWhitespace();
    return CSSPrimitiveValue::create(CSSValueBaseline);
}

static RefPtr<CSSValue> consumeAlignmentFirstBaseline(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    ASSERT(range.peek().id() == CSSValueFirst);

    auto copy = range;
    copy.consumeIncludingWhitespace();
    if (copy.peek().id() != CSSValueBaseline)
        return nullptr;

    range = copy;
    range.consumeIncludingWhitespace();
    return CSSPrimitiveValue::create(CSSValueBaseline);
}

static RefPtr<CSSValue> consumeAlignmentLastBaseline(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    ASSERT(range.peek().id() == CSSValueLast);

    auto copy = range;
    copy.consumeIncludingWhitespace();
    if (copy.peek().id() != CSSValueBaseline)
        return nullptr;

    range = copy;
    range.consumeIncludingWhitespace();
    return CSSValuePair::create(
        CSSPrimitiveValue::create(CSSValueLast),
        CSSPrimitiveValue::create(CSSValueBaseline)
    );
}

template<typename F> static RefPtr<CSSValue> consumeAlignmentOverflowPosition(CSSParserTokenRange& range, CSS::PropertyParserState&, CSSValueID overflowSafety, F&& predicate)
{
    ASSERT(range.peek().id() == CSSValueSafe || range.peek().id() == CSSValueUnsafe);

    auto copy = range;
    copy.consumeIncludingWhitespace();
    if (auto position = copy.peek().id(); predicate(position)) {
        range = copy;
        range.consumeIncludingWhitespace();
        return CSSValuePair::create(
            CSSPrimitiveValue::create(overflowSafety),
            CSSPrimitiveValue::create(position)
        );
    }
    return nullptr;
}

RefPtr<CSSValue> consumeAlignContent(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'align-content'> = normal | <baseline-position> | <content-distribution> | <overflow-position>? <content-position>
    // https://drafts.csswg.org/css-align/#propdef-align-content

    switch (auto initial = range.peek().id(); initial) {
    // normal
    case CSSValueNormal:
    // <content-distribution>
    case CSSValueSpaceBetween:
    case CSSValueSpaceAround:
    case CSSValueSpaceEvenly:
    case CSSValueStretch:
    // <content-position>
    case CSSValueStart:
    case CSSValueEnd:
    case CSSValueCenter:
    case CSSValueFlexStart:
    case CSSValueFlexEnd:
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(initial);

    // <baseline-position>
    case CSSValueFirst:
        return consumeAlignmentFirstBaseline(range, state);
    case CSSValueLast:
        return consumeAlignmentLastBaseline(range, state);
    case CSSValueBaseline:
        return consumeAlignmentBaseline(range, state);

    // <overflow-position>? <content-position>
    case CSSValueUnsafe:
    case CSSValueSafe:
        return consumeAlignmentOverflowPosition(range, state, initial, [](auto second) {
            switch (second) {
            case CSSValueStart:
            case CSSValueEnd:
            case CSSValueCenter:
            case CSSValueFlexStart:
            case CSSValueFlexEnd:
                return true;
            default:
                return false;
            }
        });

    default:
        return nullptr;
    }
}

RefPtr<CSSValue> consumeJustifyContent(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'justify-content'> = normal | <content-distribution> | <overflow-position>? [ <content-position> | left | right ]
    // https://drafts.csswg.org/css-align/#propdef-justify-content

    switch (auto initial = range.peek().id(); initial) {
    // normal
    case CSSValueNormal:
    // <content-distribution>
    case CSSValueSpaceBetween:
    case CSSValueSpaceAround:
    case CSSValueSpaceEvenly:
    case CSSValueStretch:
    // [ <content-position> | left | right ]
    case CSSValueStart:
    case CSSValueEnd:
    case CSSValueCenter:
    case CSSValueFlexStart:
    case CSSValueFlexEnd:
    case CSSValueLeft:
    case CSSValueRight:
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(initial);

    // <overflow-position>? [ <content-position> | left | right ]
    case CSSValueUnsafe:
    case CSSValueSafe:
        return consumeAlignmentOverflowPosition(range, state, initial, [](auto second) {
            switch (second) {
            case CSSValueStart:
            case CSSValueEnd:
            case CSSValueCenter:
            case CSSValueFlexStart:
            case CSSValueFlexEnd:
            case CSSValueLeft:
            case CSSValueRight:
                return true;
            default:
                return false;
            }
        });

    default:
        return nullptr;
    }
}

RefPtr<CSSValue> consumeAlignSelf(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'align-self'> = auto | normal | stretch | <baseline-position> | <overflow-position>? <self-position>
    // https://drafts.csswg.org/css-align/#propdef-align-self

    switch (auto initial = range.peek().id(); initial) {
    // auto
    case CSSValueAuto:
    // normal
    case CSSValueNormal:
    // stretch
    case CSSValueStretch:
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(initial);

    // <self-position>
    case CSSValueAnchorCenter:
        if (!state.context.propertySettings.cssAnchorPositioningEnabled)
            return nullptr;
        [[fallthrough]];
    case CSSValueStart:
    case CSSValueEnd:
    case CSSValueCenter:
    case CSSValueSelfStart:
    case CSSValueSelfEnd:
    case CSSValueFlexStart:
    case CSSValueFlexEnd:
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(initial);

    // <baseline-position>
    case CSSValueFirst:
        return consumeAlignmentFirstBaseline(range, state);
    case CSSValueLast:
        return consumeAlignmentLastBaseline(range, state);
    case CSSValueBaseline:
        return consumeAlignmentBaseline(range, state);

    // <overflow-position>? <self-position>
    case CSSValueUnsafe:
    case CSSValueSafe:
        return consumeAlignmentOverflowPosition(range, state, initial, [&](auto second) {
            switch (second) {
            case CSSValueAnchorCenter:
                if (!state.context.propertySettings.cssAnchorPositioningEnabled)
                    return false;
                [[fallthrough]];
            case CSSValueStart:
            case CSSValueEnd:
            case CSSValueCenter:
            case CSSValueSelfStart:
            case CSSValueSelfEnd:
            case CSSValueFlexStart:
            case CSSValueFlexEnd:
                return true;
            default:
                return false;
            }
        });

    default:
        return nullptr;
    }
}

RefPtr<CSSValue> consumeJustifySelf(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'justify-self'> = auto | normal | stretch | <baseline-position> | <overflow-position>? [ <self-position> | left | right ]
    // https://drafts.csswg.org/css-align/#propdef-justify-self

    switch (auto initial = range.peek().id(); initial) {
    // auto
    case CSSValueAuto:
    // normal
    case CSSValueNormal:
    // stretch
    case CSSValueStretch:
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(initial);

    // [ <self-position> | left | right ]
    case CSSValueAnchorCenter:
        if (!state.context.propertySettings.cssAnchorPositioningEnabled)
            return nullptr;
        [[fallthrough]];
    case CSSValueStart:
    case CSSValueEnd:
    case CSSValueCenter:
    case CSSValueSelfStart:
    case CSSValueSelfEnd:
    case CSSValueFlexStart:
    case CSSValueFlexEnd:
    case CSSValueLeft:
    case CSSValueRight:
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(initial);

    // <baseline-position>
    case CSSValueFirst:
        return consumeAlignmentFirstBaseline(range, state);
    case CSSValueLast:
        return consumeAlignmentLastBaseline(range, state);
    case CSSValueBaseline:
        return consumeAlignmentBaseline(range, state);

    // <overflow-position>? [ <self-position> | left | right ]
    case CSSValueUnsafe:
    case CSSValueSafe:
        return consumeAlignmentOverflowPosition(range, state, initial, [&](auto second) {
            switch (second) {
            case CSSValueAnchorCenter:
                if (!state.context.propertySettings.cssAnchorPositioningEnabled)
                    return false;
                [[fallthrough]];
            case CSSValueStart:
            case CSSValueEnd:
            case CSSValueCenter:
            case CSSValueSelfStart:
            case CSSValueSelfEnd:
            case CSSValueFlexStart:
            case CSSValueFlexEnd:
            case CSSValueLeft:
            case CSSValueRight:
                return true;
            default:
                return false;
            }
        });

    default:
        return nullptr;
    }
}

RefPtr<CSSValue> consumeAlignItems(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'align-items'> = normal | stretch | <baseline-position> | <overflow-position>? <self-position>
    // https://drafts.csswg.org/css-align/#propdef-align-items

    switch (auto initial = range.peek().id(); initial) {
    // normal
    case CSSValueNormal:
    // stretch
    case CSSValueStretch:
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(initial);

    // <self-position>
    case CSSValueAnchorCenter:
        if (!state.context.propertySettings.cssAnchorPositioningEnabled)
            return nullptr;
        [[fallthrough]];
    case CSSValueStart:
    case CSSValueEnd:
    case CSSValueCenter:
    case CSSValueSelfStart:
    case CSSValueSelfEnd:
    case CSSValueFlexStart:
    case CSSValueFlexEnd:
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(initial);

    // <baseline-position>
    case CSSValueFirst:
        return consumeAlignmentFirstBaseline(range, state);
    case CSSValueLast:
        return consumeAlignmentLastBaseline(range, state);
    case CSSValueBaseline:
        return consumeAlignmentBaseline(range, state);

    // <overflow-position>? <self-position>
    case CSSValueUnsafe:
    case CSSValueSafe:
        return consumeAlignmentOverflowPosition(range, state, initial, [&](auto second) {
            switch (second) {
            case CSSValueAnchorCenter:
                if (!state.context.propertySettings.cssAnchorPositioningEnabled)
                    return false;
                [[fallthrough]];
            case CSSValueStart:
            case CSSValueEnd:
            case CSSValueCenter:
            case CSSValueSelfStart:
            case CSSValueSelfEnd:
            case CSSValueFlexStart:
            case CSSValueFlexEnd:
                return true;
            default:
                return false;
            }
        });

    default:
        return nullptr;
    }
}

RefPtr<CSSValue> consumeJustifyItems(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'justify-items'> = normal | stretch | <baseline-position> | <overflow-position>? [ <self-position> | left | right ] | legacy | legacy && [ left | right | center ]
    // https://drafts.csswg.org/css-align/#propdef-justify-items

    switch (auto initial = range.peek().id(); initial) {
    // normal
    case CSSValueNormal:
    // stretch
    case CSSValueStretch:
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(initial);

    // [ <self-position> | left | right ] - NOTE: `left`, `right`, and `center` handled further below to account for additional `legacy` keyword.
    case CSSValueAnchorCenter:
        if (!state.context.propertySettings.cssAnchorPositioningEnabled)
            return nullptr;
        [[fallthrough]];
    case CSSValueStart:
    case CSSValueEnd:
    case CSSValueSelfStart:
    case CSSValueSelfEnd:
    case CSSValueFlexStart:
    case CSSValueFlexEnd:
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(initial);

    // <baseline-position>
    case CSSValueFirst:
        return consumeAlignmentFirstBaseline(range, state);
    case CSSValueLast:
        return consumeAlignmentLastBaseline(range, state);
    case CSSValueBaseline:
        return consumeAlignmentBaseline(range, state);

    // <overflow-position>? [ <self-position> | left | right ]
    case CSSValueUnsafe:
    case CSSValueSafe:
        return consumeAlignmentOverflowPosition(range, state, initial, [&](auto second) {
            switch (second) {
            case CSSValueAnchorCenter:
                if (!state.context.propertySettings.cssAnchorPositioningEnabled)
                    return false;
                [[fallthrough]];
            case CSSValueStart:
            case CSSValueEnd:
            case CSSValueCenter:
            case CSSValueSelfStart:
            case CSSValueSelfEnd:
            case CSSValueFlexStart:
            case CSSValueFlexEnd:
            case CSSValueLeft:
            case CSSValueRight:
                return true;
            default:
                return false;
            }
        });

    // legacy | legacy && [ left | right | center ]
    case CSSValueLegacy: {
        range.consumeIncludingWhitespace();

        switch (auto second = range.peek().id(); second) {
        case CSSValueLeft:
        case CSSValueRight:
        case CSSValueCenter:
            range.consumeIncludingWhitespace();
            return CSSValuePair::create(
                CSSPrimitiveValue::create(initial),
                CSSPrimitiveValue::create(second)
            );
        default:
            return CSSPrimitiveValue::create(initial);
        }
    }
    case CSSValueCenter:
    case CSSValueLeft:
    case CSSValueRight: {
        range.consumeIncludingWhitespace();

        switch (auto second = range.peek().id(); second) {
        case CSSValueLegacy:
            range.consumeIncludingWhitespace();
            // NOTE: Order is flipped to canonicalize to 'legacy *foo*' for serialization.
            return CSSValuePair::create(
                CSSPrimitiveValue::create(second),
                CSSPrimitiveValue::create(initial)
            );
        default:
            return CSSPrimitiveValue::create(initial);
        }
    }
    default:
        return nullptr;
    }
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
