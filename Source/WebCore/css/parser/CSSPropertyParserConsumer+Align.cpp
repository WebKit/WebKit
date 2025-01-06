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

#include "CSSContentDistributionValue.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSValueKeywords.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include <optional>

namespace WebCore {
namespace CSSPropertyParserHelpers {

using PositionKeywordPredicate = bool (*)(CSSValueID);

static bool isBaselineKeyword(CSSValueID id)
{
    return identMatches<CSSValueFirst, CSSValueLast, CSSValueBaseline>(id);
}

static bool isNormalOrStretch(CSSValueID id)
{
    return identMatches<CSSValueNormal, CSSValueStretch>(id);
}

static bool isLeftOrRightKeyword(CSSValueID id)
{
    return identMatches<CSSValueLeft, CSSValueRight>(id);
}

static bool isContentDistributionKeyword(CSSValueID id)
{
    return identMatches<CSSValueSpaceBetween, CSSValueSpaceAround, CSSValueSpaceEvenly, CSSValueStretch>(id);
}

static bool isOverflowKeyword(CSSValueID id)
{
    return identMatches<CSSValueUnsafe, CSSValueSafe>(id);
}

static bool isContentPositionKeyword(CSSValueID id)
{
    return identMatches<CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueFlexStart, CSSValueFlexEnd>(id);
}

static bool isContentPositionOrLeftOrRightKeyword(CSSValueID id)
{
    return isContentPositionKeyword(id) || isLeftOrRightKeyword(id);
}

static bool isSelfPositionKeyword(CSSValueID id)
{
    return identMatches<CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueSelfStart, CSSValueSelfEnd, CSSValueFlexStart, CSSValueFlexEnd, CSSValueAnchorCenter>(id);
}

static bool isSelfPositionOrLeftOrRightKeyword(CSSValueID id)
{
    return isSelfPositionKeyword(id) || isLeftOrRightKeyword(id);
}

static RefPtr<CSSPrimitiveValue> consumeOverflowPositionKeyword(CSSParserTokenRange& range)
{
    return isOverflowKeyword(range.peek().id()) ? consumeIdent(range) : nullptr;
}

static std::optional<CSSValueID> consumeBaselineKeywordRaw(CSSParserTokenRange& range)
{
    auto preference = consumeIdentRaw<CSSValueFirst, CSSValueLast>(range);
    if (!consumeIdent<CSSValueBaseline>(range))
        return std::nullopt;
    return preference == CSSValueLast ? CSSValueLastBaseline : CSSValueBaseline;
}

static RefPtr<CSSValue> consumeBaselineKeyword(CSSParserTokenRange& range)
{
    auto keyword = consumeBaselineKeywordRaw(range);
    if (!keyword)
        return nullptr;
    if (*keyword == CSSValueLastBaseline)
        return CSSValuePair::create(CSSPrimitiveValue::create(CSSValueLast), CSSPrimitiveValue::create(CSSValueBaseline));
    return CSSPrimitiveValue::create(CSSValueBaseline);
}

static RefPtr<CSSValue> consumeContentDistributionOverflowPosition(CSSParserTokenRange& range, PositionKeywordPredicate isPositionKeyword)
{
    ASSERT(isPositionKeyword);
    auto id = range.peek().id();
    if (identMatches<CSSValueNormal>(id))
        return CSSContentDistributionValue::create(CSSValueInvalid, range.consumeIncludingWhitespace().id(), CSSValueInvalid);
    if (isBaselineKeyword(id)) {
        auto baseline = consumeBaselineKeywordRaw(range);
        if (!baseline)
            return nullptr;
        return CSSContentDistributionValue::create(CSSValueInvalid, *baseline, CSSValueInvalid);
    }
    if (isContentDistributionKeyword(id))
        return CSSContentDistributionValue::create(range.consumeIncludingWhitespace().id(), CSSValueInvalid, CSSValueInvalid);
    auto overflow = isOverflowKeyword(id) ? range.consumeIncludingWhitespace().id() : CSSValueInvalid;
    if (isPositionKeyword(range.peek().id()))
        return CSSContentDistributionValue::create(CSSValueInvalid, range.consumeIncludingWhitespace().id(), overflow);
    return nullptr;
}

static RefPtr<CSSValue> consumeSelfPositionOverflowPosition(CSSParserTokenRange& range, PositionKeywordPredicate isPositionKeyword)
{
    ASSERT(isPositionKeyword);
    auto id = range.peek().id();
    if (identMatches<CSSValueAuto>(id) || isNormalOrStretch(id))
        return consumeIdent(range);
    if (isBaselineKeyword(id))
        return consumeBaselineKeyword(range);
    auto overflowPosition = consumeOverflowPositionKeyword(range);
    if (!isPositionKeyword(range.peek().id()))
        return nullptr;
    auto selfPosition = consumeIdent(range);
    if (overflowPosition)
        return CSSValuePair::create(overflowPosition.releaseNonNull(), selfPosition.releaseNonNull());
    return selfPosition;
}

RefPtr<CSSValue> consumeAlignContent(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'align-content'> = normal | <baseline-position> | <content-distribution> | <overflow-position>? <content-position>
    // https://drafts.csswg.org/css-align/#propdef-align-content

    return consumeContentDistributionOverflowPosition(range, isContentPositionKeyword);
}

RefPtr<CSSValue> consumeJustifyContent(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'justify-content'> = normal | <content-distribution> | <overflow-position>? [ <content-position> | left | right ]
    // https://drafts.csswg.org/css-align/#propdef-justify-content

    // justify-content property does not allow the <baseline-position> values.
    if (isBaselineKeyword(range.peek().id()))
        return nullptr;
    return consumeContentDistributionOverflowPosition(range, isContentPositionOrLeftOrRightKeyword);
}

RefPtr<CSSValue> consumeAlignSelf(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'align-self'> = auto | normal | stretch | <baseline-position> | <overflow-position>? <self-position>
    // https://drafts.csswg.org/css-align/#propdef-align-self

    return consumeSelfPositionOverflowPosition(range, isSelfPositionKeyword);
}

RefPtr<CSSValue> consumeJustifySelf(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'justify-self'> = auto | normal | stretch | <baseline-position> | <overflow-position>? [ <self-position> | left | right ]
    // https://drafts.csswg.org/css-align/#propdef-justify-self

    return consumeSelfPositionOverflowPosition(range, isSelfPositionOrLeftOrRightKeyword);
}

RefPtr<CSSValue> consumeAlignItems(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'align-items'> = normal | stretch | <baseline-position> | [ <overflow-position>? <self-position> ]
    // https://drafts.csswg.org/css-align/#propdef-align-items

    // align-items property does not allow the 'auto' value.
    if (identMatches<CSSValueAuto>(range.peek().id()))
        return nullptr;
    return consumeSelfPositionOverflowPosition(range, isSelfPositionKeyword);
}

RefPtr<CSSValue> consumeJustifyItems(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'justify-items'> = normal | stretch | <baseline-position> | <overflow-position>? [ <self-position> | left | right ] | legacy | legacy && [ left | right | center ]
    // https://drafts.csswg.org/css-align/#propdef-justify-items

    // justify-items property does not allow the 'auto' value.
    if (identMatches<CSSValueAuto>(range.peek().id()))
        return nullptr;
    CSSParserTokenRange rangeCopy = range;
    auto legacy = consumeIdent<CSSValueLegacy>(rangeCopy);
    auto positionKeyword = consumeIdent<CSSValueCenter, CSSValueLeft, CSSValueRight>(rangeCopy);
    if (!legacy)
        legacy = consumeIdent<CSSValueLegacy>(rangeCopy);
    if (legacy) {
        range = rangeCopy;
        if (positionKeyword)
            return CSSValuePair::create(legacy.releaseNonNull(), positionKeyword.releaseNonNull());
        return legacy;
    }
    return consumeSelfPositionOverflowPosition(range, isSelfPositionOrLeftOrRightKeyword);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
