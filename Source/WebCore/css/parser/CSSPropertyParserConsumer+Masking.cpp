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
#include "CSSPropertyParserConsumer+Masking.h"

#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Length.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+Shapes.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParsing.h"
#include "CSSRectValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

static RefPtr<CSSValue> consumeClipRectFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // rect() = rect( <top>, <right>, <bottom>, <left> )
    // "<top>, <right>, <bottom>, and <left> may either have a <length> value or auto."
    // https://drafts.fxtf.org/css-masking/#funcdef-clip-rect

    if (range.peek().functionId() != CSSValueRect)
        return nullptr;

    CSSParserTokenRange args = consumeFunction(range);

    auto consumeClipComponent = [&] -> RefPtr<CSSPrimitiveValue> {
        if (args.peek().id() == CSSValueAuto)
            return consumeIdent(args);
        return consumeLength(args, context, ValueRange::All, UnitlessQuirk::Allow);
    };

    // Support both rect(t, r, b, l) and rect(t r b l).
    //
    // "User agents must support separation with commas, but may also support
    //  separation without commas (but not a combination), because a previous
    //  revision of this specification was ambiguous in this respect"
    auto top = consumeClipComponent();
    if (!top)
        return nullptr;

    bool needsComma = consumeCommaIncludingWhitespace(args);

    auto right = consumeClipComponent();
    if (!right || (needsComma && !consumeCommaIncludingWhitespace(args)))
        return nullptr;

    auto bottom = consumeClipComponent();
    if (!bottom || (needsComma && !consumeCommaIncludingWhitespace(args)))
        return nullptr;

    auto left = consumeClipComponent();
    if (!left || !args.atEnd())
        return nullptr;

    return CSSRectValue::create(
        Rect {
            top.releaseNonNull(),
            right.releaseNonNull(),
            bottom.releaseNonNull(),
            left.releaseNonNull()
        }
    );
}

RefPtr<CSSValue> consumeClip(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'clip'> = <rect()> | auto
    // https://drafts.fxtf.org/css-masking/#propdef-clip

    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeClipRectFunction(range, context);
}

RefPtr<CSSValue> consumeClipPath(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'clip-path'> = none | <clip-source> | [ <basic-shape> || <geometry-box> ]
    // <clip-source> = <url>
    // https://drafts.fxtf.org/css-masking/#propdef-clip-path

    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    if (auto url = consumeURL(range))
        return url;

    RefPtr<CSSValue> shape;
    RefPtr<CSSValue> box;

    auto consumeShape = [&]() -> bool {
        if (shape)
            return false;
        shape = consumeBasicShape(range, context, { });
        return !!shape;
    };
    auto consumeBox = [&]() -> bool {
        if (box)
            return false;
        box = CSSPropertyParsing::consumeGeometryBox(range);
        return !!box;
    };

    while (!range.atEnd()) {
        if (consumeShape() || consumeBox())
            continue;
        break;
    }

    bool hasShape = !!shape;

    CSSValueListBuilder list;
    if (shape)
        list.append(shape.releaseNonNull());
    // Default value is border-box.
    if (box && (box->valueID() != CSSValueBorderBox || !hasShape))
        list.append(box.releaseNonNull());

    if (list.isEmpty())
        return nullptr;

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
