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
#include "CSSPropertyParserConsumer+Motion.h"

#include "CSSOffsetRotateValue.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Angle.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+Position.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+Shapes.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParsing.h"
#include "CSSRayValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include <wtf/SortedArrayMap.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

static RefPtr<CSSValue> consumeRayFunction(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // ray( <angle> && <ray-size>? && contain? && [at <position>]? )
    // <ray-size> = closest-side | closest-corner | farthest-side | farthest-corner | sides
    // https://drafts.fxtf.org/motion-1/#ray-function

    static constexpr std::pair<CSSValueID, CSS::RaySize> sizeMappings[] {
        { CSSValueClosestSide, CSS::RaySize { CSS::ClosestSide { } } },
        { CSSValueClosestCorner, CSS::RaySize { CSS::ClosestCorner { } } },
        { CSSValueFarthestSide, CSS::RaySize { CSS::FarthestSide { } } },
        { CSSValueFarthestCorner, CSS::RaySize { CSS::FarthestCorner { } } },
        { CSSValueSides, CSS::RaySize { CSS::Sides { } } },
    };
    static constexpr SortedArrayMap sizeMap { sizeMappings };

    if (range.peek().type() != FunctionToken || range.peek().functionId() != CSSValueRay)
        return { };

    auto args = consumeFunction(range);

    std::optional<CSS::Angle<>> angle;
    std::optional<CSS::RaySize> size;
    std::optional<CSS::Contain> contain;
    std::optional<CSS::Position> position;

    auto consumeAngle = [&] -> bool {
        if (angle)
            return false;
        angle = MetaConsumer<CSS::Angle<>>::consume(args, context, { }, { .parserMode = context.mode });
        return angle.has_value();
    };
    auto consumeSize = [&] -> bool {
        if (size)
            return false;
        size = consumeIdentUsingMapping(args, sizeMap);
        return size.has_value();
    };
    auto consumeContain = [&] -> bool {
        if (contain || !consumeIdentRaw<CSSValueContain>(args).has_value())
            return false;
        contain = CSS::Contain { };
        return contain.has_value();
    };
    auto consumeAtPosition = [&] -> bool {
        if (position || !consumeIdentRaw<CSSValueAt>(args).has_value())
            return false;
        position = consumePositionUnresolved(args, context);
        return position.has_value();
    };

    while (!args.atEnd()) {
        if (consumeAngle() || consumeSize() || consumeContain() || consumeAtPosition())
            continue;
        return { };
    }

    // The <angle> argument is the only one that is required.
    if (!angle)
        return { };

    return CSSRayValue::create(
        CSS::RayFunction {
            .parameters = CSS::Ray {
                WTFMove(*angle),
                size.value_or(CSS::RaySize { CSS::ClosestSide { } }),
                WTFMove(contain),
                WTFMove(position)
            }
        }
    );
}

RefPtr<CSSValue> consumeOffsetPath(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'offset-path'> = none | <offset-path> || <coord-box>
    //
    // NOTE: The sub-production, <offset-path> (without the quotation marks) is distinct and defined as:
    //    <offset-path> = <ray()> | <url> | <basic-shape>
    //
    // So, this expands out to a grammar of:
    //
    // <'offset-path'> = none | [ <ray()> | <url> | <basic-shape> || <coord-box> ]
    //
    // which is almost the same as <'clip-path'> above, with the following differences:
    //
    // 1. <'clip-path'> does not support `ray()`.
    // 2. <'clip-path'> does not allow a `box` to be provided with `<url>`.
    // 3. <'clip-path'> specifies `<geometry-box>` rather than `<coord-box>`.
    //
    // https://drafts.fxtf.org/motion-1/#propdef-offset-path

    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    // FIXME: It should be possible to consume both a <url> and <coord-box>.
    if (auto url = consumeURL(range))
        return url;

    RefPtr<CSSValue> shapeOrRay;
    RefPtr<CSSValue> box;

    auto consumeRay = [&]() -> bool {
        if (shapeOrRay)
            return false;
        shapeOrRay = consumeRayFunction(range, context);
        return !!shapeOrRay;
    };
    auto consumeShape = [&]() -> bool {
        if (shapeOrRay)
            return false;
        shapeOrRay = consumeBasicShape(range, context, PathParsingOption::RejectPathFillRule);
        return !!shapeOrRay;
    };
    auto consumeBox = [&]() -> bool {
        if (box)
            return false;
        // FIXME: The Motion Path spec calls for this to be a <coord-box>, not a <geometry-box>, the difference being that the former does not contain "margin-box" as a valid term.
        // However, the spec also has a few examples using "margin-box", so there seems to be some abiguity to be resolved. See: https://github.com/w3c/fxtf-drafts/issues/481.
        box = CSSPropertyParsing::consumeGeometryBox(range);
        return !!box;
    };

    while (!range.atEnd()) {
        if (consumeRay() || consumeShape() || consumeBox())
            continue;
        break;
    }

    bool hasShapeOrRay = !!shapeOrRay;

    CSSValueListBuilder list;
    if (shapeOrRay)
        list.append(shapeOrRay.releaseNonNull());

    // Default value is border-box.
    if (box && (box->valueID() != CSSValueBorderBox || !hasShapeOrRay))
        list.append(box.releaseNonNull());

    if (list.isEmpty())
        return nullptr;

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeOffsetRotate(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto rangeCopy = range;

    // Attempt to parse the first token as the modifier (auto / reverse keyword). If
    // successful, parse the second token as the angle. If not, try to parse the other
    // way around.
    auto modifier = consumeIdent<CSSValueAuto, CSSValueReverse>(rangeCopy);
    auto angle = consumeAngle(rangeCopy, context);
    if (!modifier)
        modifier = consumeIdent<CSSValueAuto, CSSValueReverse>(rangeCopy);
    if (!angle && !modifier)
        return nullptr;

    range = rangeCopy;
    return CSSOffsetRotateValue::create(WTFMove(modifier), WTFMove(angle));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
