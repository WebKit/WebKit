/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Igalia S.L.
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
#include "CSSPropertyParserConsumer+Anchor.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSValue.h"
#include "CSSValuePair.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// <position-area> syntax, rewritten for expository purpose:
//
// <position-area> =
//   [ <physical-area>
//   | <logical-area> | <self-logical-area>
//   | <ambiguous-logical-area> | <self-ambiguous-logical-area> ]
//
// // Keywords that doesn't depend on an axis. That is, the result is the same,
// // no matter which axis is used.
// <axisless-keyword> = [ center | span-all ]
//
// // Keywords that refer to a physical axis.
// <physical-area> = [ <physical-area-x> || <physical-area-y> ]
// <physical-area-x> =
//   [ left | right | span-left | span-right
//   | x-start | x-end | span-x-start | span-x-end
//   | x-self-start | x-self-end | span-x-self-start | span-x-self-end ] | <axisless-keyword>
// <physical-area-y> =
//   [ top | bottom | span-top | span-bottom
//   | y-start | y-end | span-y-start | span-y-end
//   | y-self-start | y-self-end | span-y-self-start | span-y-self-end ] | <axisless-keyword>
//
// // Keywords that refer to an explicit logical (block/inline) axis. The axis is
// // resolved using the containing block's writing mode.
// <logical-area> = [ <logical-area-block> || <logical-area-inline> ]
// <logical-area-block> =
//   [ block-start | block-end | span-block-start | span-block-end ] | <axisless-keyword>
// <logical-area-inline> =
//   [ inline-start | inline-end | span-inline-start | span-inline-end ] | <axisless-keyword>
//
// // Keywords that refer to an explicit logical (block/inline) axis. The axis is
// // resolved using the element's own writing mode.
// <self-logical-area> = [ <self-logical-area-block> || <self-logical-area-inline> ]
// <self-logical-area-block> =
//   [ self-block-start | self-block-end | span-self-block-start | span-self-block-end ] | <axisless-keyword>
// <self-logical-area-inline> =
//   [ self-inline-start | self-inline-end | span-self-inline-start | span-self-inline-end ] | <axisless-keyword>
//
// // Similar to <logical-area>, but the axis is ambiguous (can be block or inline)
// // and must be resolved.
// <ambiguous-logical-area> = <ambiguous-logical-area-keyword>{1, 2}
// <ambiguous-logical-area-keyword> = [ start | end | span-start | span-end ] | <axisless-keyword>
//
// // Similar to <self-logical-area> but the axis is ambiguous.
// <self-ambiguous-logical-area> = <self-ambiguous-logical-area-keyword>{1, 2}
// <self-ambiguous-logical-area-keyword> = [ self-start | self-end | span-self-start | span-self-end ] | <axisless-keyword>

enum class KeywordType : uint8_t {
    // <physical-area-x>
    PhysicalX,
    // <physical-area-y>
    PhysicalY,

    // <logical-area-block>
    LogicalBlock,
    // <self-logical-area-block>
    SelfLogicalBlock,

    // <logical-area-inline>
    LogicalInline,
    // <self-logical-area-inline>
    SelfLogicalInline,

    // <ambiguous-position-area>
    Ambiguous,
    // <self-ambiguous-position-area>
    SelfAmbiguous,

    // <axisless-keyword>
    Axisless
};

static std::optional<KeywordType> getKeywordType(CSSValueID id)
{
    switch (id) {
    case CSSValueLeft:
    case CSSValueRight:
    case CSSValueSpanLeft:
    case CSSValueSpanRight:
    case CSSValueXStart:
    case CSSValueXEnd:
    case CSSValueSpanXStart:
    case CSSValueSpanXEnd:
    case CSSValueXSelfStart:
    case CSSValueXSelfEnd:
    case CSSValueSpanXSelfStart:
    case CSSValueSpanXSelfEnd:
        return KeywordType::PhysicalX;

    case CSSValueTop:
    case CSSValueBottom:
    case CSSValueSpanTop:
    case CSSValueSpanBottom:
    case CSSValueYStart:
    case CSSValueYEnd:
    case CSSValueSpanYStart:
    case CSSValueSpanYEnd:
    case CSSValueYSelfStart:
    case CSSValueYSelfEnd:
    case CSSValueSpanYSelfStart:
    case CSSValueSpanYSelfEnd:
        return KeywordType::PhysicalY;

    case CSSValueBlockStart:
    case CSSValueBlockEnd:
    case CSSValueSpanBlockStart:
    case CSSValueSpanBlockEnd:
        return KeywordType::LogicalBlock;

    case CSSValueInlineStart:
    case CSSValueInlineEnd:
    case CSSValueSpanInlineStart:
    case CSSValueSpanInlineEnd:
        return KeywordType::LogicalInline;

    case CSSValueSelfBlockStart:
    case CSSValueSelfBlockEnd:
    case CSSValueSpanSelfBlockStart:
    case CSSValueSpanSelfBlockEnd:
        return KeywordType::SelfLogicalBlock;

    case CSSValueSelfInlineStart:
    case CSSValueSelfInlineEnd:
    case CSSValueSpanSelfInlineStart:
    case CSSValueSpanSelfInlineEnd:
        return KeywordType::SelfLogicalInline;

    case CSSValueStart:
    case CSSValueEnd:
    case CSSValueSpanStart:
    case CSSValueSpanEnd:
        return KeywordType::Ambiguous;

    case CSSValueSelfStart:
    case CSSValueSelfEnd:
    case CSSValueSpanSelfStart:
    case CSSValueSpanSelfEnd:
        return KeywordType::SelfAmbiguous;

    case CSSValueCenter:
    case CSSValueSpanAll:
        return KeywordType::Axisless;

    default:
        return { };
    }
}

// Check if the two keyword types are compatible with each other. For example,
// <physical-area-x> must go with <physical-area-y> or <axisless-keyword>
static bool typesAreCompatible(KeywordType dim1Type, KeywordType dim2Type)
{
    switch (dim1Type) {
    case KeywordType::PhysicalX:
        return (dim2Type == KeywordType::PhysicalY || dim2Type == KeywordType::Axisless);

    case KeywordType::PhysicalY:
        return (dim2Type == KeywordType::PhysicalX || dim2Type == KeywordType::Axisless);

    case KeywordType::LogicalBlock:
        return (dim2Type == KeywordType::LogicalInline || dim2Type == KeywordType::Axisless);

    case KeywordType::LogicalInline:
        return (dim2Type == KeywordType::LogicalBlock || dim2Type == KeywordType::Axisless);

    case KeywordType::SelfLogicalBlock:
        return (dim2Type == KeywordType::SelfLogicalInline || dim2Type == KeywordType::Axisless);

    case KeywordType::SelfLogicalInline:
        return (dim2Type == KeywordType::SelfLogicalBlock || dim2Type == KeywordType::Axisless);

    case KeywordType::Ambiguous:
        return (dim2Type == KeywordType::Ambiguous || dim2Type == KeywordType::Axisless);

    case KeywordType::SelfAmbiguous:
        return (dim2Type == KeywordType::SelfAmbiguous || dim2Type == KeywordType::Axisless);

    case KeywordType::Axisless:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

// Check if a keyword type is explicit about its axis.
static bool typeIsAxisExplicit(KeywordType type)
{
    switch (type) {
    case KeywordType::PhysicalX:
    case KeywordType::PhysicalY:
    case KeywordType::LogicalBlock:
    case KeywordType::SelfLogicalBlock:
    case KeywordType::LogicalInline:
    case KeywordType::SelfLogicalInline:
        return true;

    default:
        return false;
    }
}

// Check if a keyword type refers to the X or block axis.
static bool typeIsBlockOrXAxis(KeywordType type)
{
    switch (type) {
    case KeywordType::PhysicalX:
    case KeywordType::LogicalBlock:
    case KeywordType::SelfLogicalBlock:
        return true;

    default:
        return false;
    }
}

// Check if a keyword type refers to the Y or inline axis.
static bool typeIsInlineOrYAxis(KeywordType type)
{
    switch (type) {
    case KeywordType::PhysicalY:
    case KeywordType::LogicalInline:
    case KeywordType::SelfLogicalInline:
        return true;

    default:
        return false;
    }
}

RefPtr<CSSValue> valueForPositionArea(CSSValueID dim1, CSSValueID dim2)
{
    auto maybeDim1Type = getKeywordType(dim1);
    if (!maybeDim1Type)
        return nullptr;
    auto dim1Type = *maybeDim1Type;

    auto maybeDim2Type = getKeywordType(dim2);
    if (!maybeDim2Type)
        return nullptr;
    auto dim2Type = *maybeDim2Type;

    if (!typesAreCompatible(dim1Type, dim2Type))
        return nullptr;

    if (dim1 == CSSValueSpanAll && typeIsAxisExplicit(dim2Type))
        return CSSPrimitiveValue::create(dim2);
    if (typeIsAxisExplicit(dim1Type) && dim2 == CSSValueSpanAll)
        return CSSPrimitiveValue::create(dim1);

    // Ensure the X/block axis keyword goes first in the pair.
    if (typeIsInlineOrYAxis(dim1Type) || typeIsBlockOrXAxis(dim2Type))
        std::swap(dim1, dim2);

    return CSSValuePair::create(CSSPrimitiveValue::create(dim1), CSSPrimitiveValue::create(dim2));
}

RefPtr<CSSValue> consumePositionArea(CSSParserTokenRange& range, const CSSParserContext&)
{
    auto maybeDim1 = consumeIdentRaw(range);
    if (!maybeDim1)
        return nullptr;
    auto dim1 = *maybeDim1;
    if (dim1 == CSSValueNone)
        return CSSPrimitiveValue::create(CSSValueNone);

    auto maybeDim2 = consumeIdentRaw(range);
    if (!maybeDim2)
        return CSSPrimitiveValue::create(dim1);
    auto dim2 = *maybeDim2;

    return valueForPositionArea(dim1, dim2);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
