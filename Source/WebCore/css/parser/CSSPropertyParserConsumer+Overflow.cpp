/*
 * Copyright (C) 2026 Suraj Thanugundla <contact@surajt.com>
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
#include "CSSPropertyParserConsumer+Overflow.h"

#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserState.h"
#include "CSSPropertyParsing.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSValue> consumeOverflowClipMargin(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'overflow-clip-margin'> = <visual-box> || <length [0,∞]>
    // https://drafts.csswg.org/css-overflow/#overflow-clip-margin
    RefPtr<CSSValue> visualBox;
    auto tryConsumeVisualBox = [&visualBox](CSSParserTokenRange& range) -> bool {
        if (visualBox)
            return false;
        visualBox = CSSPropertyParsing::consumeVisualBox(range);
        return !!visualBox;
    };

    RefPtr<CSSPrimitiveValue> length;
    auto tryConsumeLength = [&length](CSSParserTokenRange& range, CSS::PropertyParserState& state) -> bool {
        auto consumeLength = [](CSSParserTokenRange& range, CSS::PropertyParserState& state) -> RefPtr<CSSPrimitiveValue> {
            return CSSPrimitiveValueResolver<CSS::Length<CSS::Range { 0, CSS::Range::infinity } >>::consumeAndResolve(range, state);
        };
        if (length)
            return false;
        length = consumeLength(range, state);
        return !!length;
    };

    for (size_t i = 0; i < 2 && !range.atEnd(); ++i) {
        if (tryConsumeVisualBox(range) || tryConsumeLength(range, state))
            continue;
        break;
    }

    if (!visualBox && !length)
        return nullptr;

    CSSValueListBuilder list;
    // Default value is padding-box
    if (visualBox && !isValueID(visualBox, CSSValueID::CSSValuePaddingBox))
        list.append(visualBox.releaseNonNull());

    // Default value is 0px
    if (length && !length->isZero().value_or(false))
        list.append(length.releaseNonNull());

    if (list.isEmpty())
        return { CSSPrimitiveValue::create(0, CSSUnitType::CSS_PX) };

    if (list.size() == 1)
        return WTF::move(list[0]);

    return CSSValueList::createSpaceSeparated(WTF::move(list));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
