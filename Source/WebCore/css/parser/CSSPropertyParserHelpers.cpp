// Copyright 2016 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2023 Apple Inc. All rights reserved.
// Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+Angle.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Image.h"
#include "CSSPropertyParserConsumer+Integer.h"
#include "CSSPropertyParserConsumer+Length.h"
#include "CSSPropertyParserConsumer+LengthPercentage.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+Lists.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+Percentage.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Position.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+String.h"
#include "CSSPropertyParserConsumer+Time.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParsing.h"

#include "CSSBackgroundRepeatValue.h"
#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcTree+Parser.h"
#include "CSSCalcValue.h"
#include "CSSCounterValue.h"
#include "CSSCursorImageValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFunctionValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSParser.h"
#include "CSSParserIdioms.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSQuadValue.h"
#include "CSSRectValue.h"
#include "CSSReflectValue.h"
#include "CSSShadowValue.h"
#include "CSSTransformListValue.h"
#include "CSSValueKeywords.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "CSSVariableData.h"
#include "CSSVariableParser.h"
#include "CalculationCategory.h"
#include "ColorInterpolation.h"
#include "LengthPoint.h"
#include "Logging.h"
#include "RenderStyleConstants.h"
#include "SVGPathByteStream.h"
#include "SVGPathUtilities.h"
#include "ScriptExecutionContext.h"
#include "StyleColor.h"
#include <wtf/SortedArrayMap.h>
#include <wtf/text/TextStream.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

namespace CSSPropertyParserHelpers {


RefPtr<CSSPrimitiveValue> consumeSingleContainerName(CSSParserTokenRange& range, const CSSParserContext&)
{
    switch (range.peek().id()) {
    case CSSValueNone:
    case CSSValueAnd:
    case CSSValueOr:
    case CSSValueNot:
        return nullptr;
    default:
        if (auto ident = consumeCustomIdent(range))
            return ident;
        return nullptr;
    }
}

static RefPtr<CSSValueList> consumeAspectRatioValue(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto leftValue = consumeNumber(range, context, ValueRange::NonNegative);
    if (!leftValue)
        return nullptr;

    bool slashSeen = consumeSlashIncludingWhitespace(range);
    auto rightValue = slashSeen
        ? consumeNumber(range, context, ValueRange::NonNegative)
        : CSSPrimitiveValue::create(1);
    if (!rightValue)
        return nullptr;

    return CSSValueList::createSlashSeparated(leftValue.releaseNonNull(), rightValue.releaseNonNull());
}

RefPtr<CSSValue> consumeAspectRatio(CSSParserTokenRange& range, const CSSParserContext& context)
{
    RefPtr<CSSPrimitiveValue> autoValue;
    if (range.peek().type() == IdentToken)
        autoValue = consumeIdent<CSSValueAuto>(range);
    if (range.atEnd())
        return autoValue;
    auto ratioList = consumeAspectRatioValue(range, context);
    if (!ratioList)
        return nullptr;
    if (!autoValue) {
        autoValue = consumeIdent<CSSValueAuto>(range);
        if (!autoValue)
            return ratioList;
    }
    return CSSValueList::createSpaceSeparated(autoValue.releaseNonNull(), ratioList.releaseNonNull());
}

// Keep in sync with the single keyword value fast path of CSSParserFastPaths's parseDisplay.
RefPtr<CSSValue> consumeDisplay(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // Parse single keyword values
    auto singleKeyword = consumeIdent<
        // <display-box>
        CSSValueContents,
        CSSValueNone,
        // <display-internal>
        CSSValueTableCaption,
        CSSValueTableCell,
        CSSValueTableColumnGroup,
        CSSValueTableColumn,
        CSSValueTableHeaderGroup,
        CSSValueTableFooterGroup,
        CSSValueTableRow,
        CSSValueTableRowGroup,
        CSSValueRubyBase,
        CSSValueRubyText,
        // <display-legacy>
        CSSValueInlineBlock,
        CSSValueInlineFlex,
        CSSValueInlineGrid,
        CSSValueInlineTable,
        // Prefixed values
        CSSValueWebkitInlineBox,
        CSSValueWebkitBox,
        // No layout support for the full <display-listitem> syntax, so treat it as <display-legacy>
        CSSValueListItem
    >(range);

    auto allowsValue = [&](CSSValueID value) {
        bool isRuby = value == CSSValueRubyBase || value == CSSValueRubyText || value == CSSValueBlockRuby || value == CSSValueRuby;
        return !isRuby || isUASheetBehavior(context.mode);
    };

    if (singleKeyword) {
        if (!allowsValue(singleKeyword->valueID()))
            return nullptr;
        return singleKeyword;
    }

    // Empty value, stop parsing
    if (range.atEnd())
        return nullptr;

    // Convert -webkit-flex/-webkit-inline-flex to flex/inline-flex
    CSSValueID nextValueID = range.peek().id();
    if (nextValueID == CSSValueWebkitInlineFlex || nextValueID == CSSValueWebkitFlex) {
        consumeIdent(range);
        return CSSPrimitiveValue::create(
            nextValueID == CSSValueWebkitInlineFlex ? CSSValueInlineFlex : CSSValueFlex);
    }

    // Parse [ <display-outside> || <display-inside> ]
    std::optional<CSSValueID> parsedDisplayOutside;
    std::optional<CSSValueID> parsedDisplayInside;
    while (!range.atEnd()) {
        auto nextValueID = range.peek().id();
        switch (nextValueID) {
        // <display-outside>
        case CSSValueBlock:
        case CSSValueInline:
            if (parsedDisplayOutside)
                return nullptr;
            parsedDisplayOutside = nextValueID;
            break;
        // <display-inside>
        case CSSValueFlex:
        case CSSValueFlow:
        case CSSValueFlowRoot:
        case CSSValueGrid:
        case CSSValueTable:
        case CSSValueRuby:
            if (parsedDisplayInside)
                return nullptr;
            parsedDisplayInside = nextValueID;
            break;
        default:
            return nullptr;
        }
        consumeIdent(range);
    }

    // Set defaults when one of the two values are unspecified
    CSSValueID displayInside = parsedDisplayInside.value_or(CSSValueFlow);

    auto selectShortValue = [&]() -> CSSValueID {
        if (!parsedDisplayOutside || *parsedDisplayOutside == CSSValueInline) {
            if (displayInside == CSSValueRuby)
                return CSSValueRuby;
        }

        if (!parsedDisplayOutside || *parsedDisplayOutside == CSSValueBlock) {
            // Alias display: flow to display: block
            if (displayInside == CSSValueFlow)
                return CSSValueBlock;
            if (displayInside == CSSValueRuby)
                return CSSValueBlockRuby;
            return displayInside;
        }

        // Convert `display: inline <display-inside>` to the equivalent short value
        switch (displayInside) {
        case CSSValueFlex:
            return CSSValueInlineFlex;
        case CSSValueFlow:
            return CSSValueInline;
        case CSSValueFlowRoot:
            return CSSValueInlineBlock;
        case CSSValueGrid:
            return CSSValueInlineGrid;
        case CSSValueTable:
            return CSSValueInlineTable;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueInline;
        }
    };

    auto shortValue = selectShortValue();
    if (!allowsValue(shortValue))
        return nullptr;

    return CSSPrimitiveValue::create(shortValue);
}

RefPtr<CSSValue> consumeWillChange(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    // Every comma-separated list of identifiers is a valid will-change value,
    // unless the list includes an explicitly disallowed identifier.
    CSSValueListBuilder values;
    while (!range.atEnd()) {
        switch (range.peek().id()) {
        case CSSValueContents:
        case CSSValueScrollPosition:
            values.append(consumeIdent(range).releaseNonNull());
            break;
        case CSSValueNone:
        case CSSValueAll:
        case CSSValueAuto:
            return nullptr;
        default:
            if (range.peek().type() != IdentToken)
                return nullptr;
            CSSPropertyID propertyID = cssPropertyID(range.peek().value());
            if (propertyID == CSSPropertyWillChange)
                return nullptr;
            if (!isExposed(propertyID, &context.propertySettings))
                propertyID = CSSPropertyInvalid;
            if (propertyID != CSSPropertyInvalid) {
                values.append(CSSPrimitiveValue::create(propertyID));
                range.consumeIncludingWhitespace();
                break;
            }
            if (auto customIdent = consumeCustomIdent(range)) {
                // Append properties we don't recognize, but that are legal.
                values.append(customIdent.releaseNonNull());
                break;
            }
            return nullptr;
        }
        // This is a comma separated list
        if (!range.atEnd() && !consumeCommaIncludingWhitespace(range))
            return nullptr;
    }
    return CSSValueList::createCommaSeparated(WTFMove(values));
}

RefPtr<CSSValue> consumeQuotes(CSSParserTokenRange& range, const CSSParserContext&)
{
    auto id = range.peek().id();
    if (id == CSSValueNone || id == CSSValueAuto)
        return consumeIdent(range);
    CSSValueListBuilder values;
    while (!range.atEnd()) {
        auto parsedValue = consumeString(range);
        if (!parsedValue)
            return nullptr;
        values.append(parsedValue.releaseNonNull());
    }
    if (values.size() && !(values.size() % 2))
        return CSSValueList::createSpaceSeparated(WTFMove(values));
    return nullptr;
}

RefPtr<CSSValue> consumeSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (consumeIdentRaw<CSSValueAuto>(range))
        return CSSPrimitiveValue::create(CSSValueAuto);

    if (auto width = consumeLength(range, context, ValueRange::NonNegative)) {
        auto height = consumeLength(range, context, ValueRange::NonNegative);
        if (!height)
            return width;
        return CSSValuePair::create(width.releaseNonNull(), height.releaseNonNull());
    }

    auto pageSize = CSSPropertyParsing::consumePageSize(range);
    auto orientation = consumeIdent<CSSValuePortrait, CSSValueLandscape>(range);
    if (!pageSize)
        pageSize = CSSPropertyParsing::consumePageSize(range);
    if (!orientation && !pageSize)
        return nullptr;
    if (pageSize && !orientation)
        return pageSize;
    if (!pageSize)
        return orientation;
    return CSSValuePair::create(pageSize.releaseNonNull(), orientation.releaseNonNull());
}

RefPtr<CSSValue> consumeTextTransform(CSSParserTokenRange& range, const CSSParserContext&)
{
    if (consumeIdentRaw<CSSValueNone>(range))
        return CSSPrimitiveValue::create(CSSValueNone);

    bool fullSizeKana = false;
    bool fullWidth = false;
    bool uppercase = false;
    bool capitalize = false;
    bool lowercase = false;

    do {
        auto ident = consumeIdentRaw(range);
        if (!ident)
            return nullptr;

        if (ident == CSSValueFullSizeKana && !fullSizeKana) {
            fullSizeKana = true;
            continue;
        }
        if (ident == CSSValueFullWidth && !fullWidth) {
            fullWidth = true;
            continue;
        }
        bool alreadySet = uppercase || capitalize || lowercase;
        if (ident == CSSValueUppercase && !alreadySet) {
            uppercase = true;
            continue;
        }
        if (ident == CSSValueCapitalize && !alreadySet) {
            capitalize = true;
            continue;
        }
        if (ident == CSSValueLowercase && !alreadySet) {
            lowercase = true;
            continue;
        }
        return nullptr;
    } while (!range.atEnd());

    // Construct the result list in canonical order
    CSSValueListBuilder list;
    if (capitalize)
        list.append(CSSPrimitiveValue::create(CSSValueCapitalize));
    else if (uppercase)
        list.append(CSSPrimitiveValue::create(CSSValueUppercase));
    else if (lowercase)
        list.append(CSSPrimitiveValue::create(CSSValueLowercase));

    if (fullWidth)
        list.append(CSSPrimitiveValue::create(CSSValueFullWidth));

    if (fullSizeKana)
        list.append(CSSPrimitiveValue::create(CSSValueFullSizeKana));

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeTextIndent(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // [ <length> | <percentage> ] && hanging? && each-line?
    RefPtr<CSSValue> lengthPercentage;
    bool eachLine = false;
    bool hanging = false;
    do {
        if (!lengthPercentage) {
            if (auto textIndent = consumeLengthPercentage(range, context, ValueRange::All, UnitlessQuirk::Allow)) {
                lengthPercentage = textIndent;
                continue;
            }
        }
        if (!eachLine && consumeIdentRaw<CSSValueEachLine>(range)) {
            eachLine = true;
            continue;
        }
        if (!hanging && consumeIdentRaw<CSSValueHanging>(range)) {
            hanging = true;
            continue;
        }
        return nullptr;
    } while (!range.atEnd());
    if (!lengthPercentage)
        return nullptr;

    if (!hanging && !eachLine)
        return CSSValueList::createSpaceSeparated(lengthPercentage.releaseNonNull());
    if (hanging && !eachLine)
        return CSSValueList::createSpaceSeparated(lengthPercentage.releaseNonNull(), CSSPrimitiveValue::create(CSSValueHanging));
    if (!hanging)
        return CSSValueList::createSpaceSeparated(lengthPercentage.releaseNonNull(), CSSPrimitiveValue::create(CSSValueEachLine));
    return CSSValueList::createSpaceSeparated(lengthPercentage.releaseNonNull(),
        CSSPrimitiveValue::create(CSSValueHanging), CSSPrimitiveValue::create(CSSValueEachLine));
}

// auto | [ [ under | from-font ] || [ left | right ] ]
RefPtr<CSSValue> consumeTextUnderlinePosition(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto ident = consumeIdent<CSSValueAuto>(range))
        return ident;

    auto metric = consumeIdentRaw<CSSValueUnder, CSSValueFromFont>(range);

    std::optional<CSSValueID> side;
    if (context.cssTextUnderlinePositionLeftRightEnabled)
        side = consumeIdentRaw<CSSValueLeft, CSSValueRight>(range);

    if (side && !metric)
        metric = consumeIdentRaw<CSSValueUnder, CSSValueFromFont>(range);

    if (metric && side)
        return CSSValuePair::create(CSSPrimitiveValue::create(*metric), CSSPrimitiveValue::create(*side));
    if (metric)
        return CSSPrimitiveValue::create(*metric);
    if (side)
        return CSSPrimitiveValue::create(*side);

    return nullptr;
}

static RefPtr<CSSValue> consumeAutoOrLengthPercentage(CSSParserTokenRange& range, const CSSParserContext& context, UnitlessQuirk unitless, AnchorPolicy anchorPolicy = AnchorPolicy::Forbid, AnchorSizePolicy anchorSizePolicy = AnchorSizePolicy::Forbid)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLengthPercentage(range, context, ValueRange::All, unitless, UnitlessZeroQuirk::Allow, anchorPolicy, anchorSizePolicy);
}

RefPtr<CSSValue> consumeMarginSide(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID currentShorthand)
{
    UnitlessQuirk unitless = currentShorthand != CSSPropertyInset ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
    AnchorSizePolicy anchorSizePolicy = context.propertySettings.cssAnchorPositioningEnabled ? AnchorSizePolicy::Allow : AnchorSizePolicy::Forbid;
    return consumeAutoOrLengthPercentage(range, context, unitless, AnchorPolicy::Forbid, anchorSizePolicy);
}

RefPtr<CSSValue> consumeMarginTrim(CSSParserTokenRange& range, const CSSParserContext&)
{
    auto firstValue = range.peek().id();
    if (firstValue == CSSValueBlock || firstValue == CSSValueInline || firstValue == CSSValueNone)
        return consumeIdent(range).releaseNonNull();
    Vector<CSSValueID, 4> idents;
    while (auto ident = consumeIdentRaw<CSSValueBlockStart, CSSValueBlockEnd, CSSValueInlineStart, CSSValueInlineEnd>(range)) {
        if (idents.contains(*ident))
            return nullptr;
        idents.append(*ident);
    }
    // Try to serialize into either block or inline form
    if (idents.size() == 2) {
        if (idents.contains(CSSValueBlockStart) && idents.contains(CSSValueBlockEnd))
            return CSSPrimitiveValue::create(CSSValueBlock);
        if (idents.contains(CSSValueInlineStart) && idents.contains(CSSValueInlineEnd))
            return CSSPrimitiveValue::create(CSSValueInline);
    }
    CSSValueListBuilder list;
    for (auto ident : idents)
        list.append(CSSPrimitiveValue::create(ident));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeSide(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyID currentShorthand)
{
    UnitlessQuirk unitless = currentShorthand != CSSPropertyInset ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
    AnchorPolicy anchorPolicy = context.propertySettings.cssAnchorPositioningEnabled ? AnchorPolicy::Allow : AnchorPolicy::Forbid;
    AnchorSizePolicy anchorSizePolicy = context.propertySettings.cssAnchorPositioningEnabled ? AnchorSizePolicy::Allow : AnchorSizePolicy::Forbid;
    return consumeAutoOrLengthPercentage(range, context, unitless, anchorPolicy, anchorSizePolicy);
}

RefPtr<CSSValue> consumeInsetLogicalStartEnd(CSSParserTokenRange& range, const CSSParserContext& context)
{
    AnchorPolicy anchorPolicy = context.propertySettings.cssAnchorPositioningEnabled ? AnchorPolicy::Allow : AnchorPolicy::Forbid;
    AnchorSizePolicy anchorSizePolicy = context.propertySettings.cssAnchorPositioningEnabled ? AnchorSizePolicy::Allow : AnchorSizePolicy::Forbid;
    return consumeAutoOrLengthPercentage(range, context, UnitlessQuirk::Forbid, anchorPolicy, anchorSizePolicy);
}

RefPtr<CSSValue> consumeTouchAction(CSSParserTokenRange& range, const CSSParserContext&)
{
    if (auto ident = consumeIdent<CSSValueNone, CSSValueAuto, CSSValueManipulation>(range))
        return ident;

    bool hasPanX = false;
    bool hasPanY = false;
    bool hasPinchZoom = false;
    while (true) {
        auto ident = consumeIdentRaw<CSSValuePanX, CSSValuePanY, CSSValuePinchZoom>(range);
        if (!ident)
            break;
        switch (*ident) {
        case CSSValuePanX:
            if (hasPanX)
                return nullptr;
            hasPanX = true;
            break;
        case CSSValuePanY:
            if (hasPanY)
                return nullptr;
            hasPanY = true;
            break;
        case CSSValuePinchZoom:
            if (hasPinchZoom)
                return nullptr;
            hasPinchZoom = true;
            break;
        default:
            return nullptr;
        }
    }

    if (!hasPanX && !hasPanY && !hasPinchZoom)
        return nullptr;

    CSSValueListBuilder builder;
    if (hasPanX)
        builder.append(CSSPrimitiveValue::create(CSSValuePanX));
    if (hasPanY)
        builder.append(CSSPrimitiveValue::create(CSSValuePanY));
    if (hasPinchZoom)
        builder.append(CSSPrimitiveValue::create(CSSValuePinchZoom));

    return CSSValueList::createSpaceSeparated(WTFMove(builder));
}

RefPtr<CSSValue> consumeKeyframesName(CSSParserTokenRange& range, const CSSParserContext&)
{
    // https://www.w3.org/TR/css-animations-1/#typedef-keyframes-name
    //
    // <keyframes-name> = <custom-ident> | <string>

    if (range.peek().type() == StringToken) {
        auto& token = range.consumeIncludingWhitespace();
        if (equalLettersIgnoringASCIICase(token.value(), "none"_s))
            return CSSPrimitiveValue::create(CSSValueNone);
        return CSSPrimitiveValue::create(token.value().toString());
    }

    return consumeCustomIdent(range);
}

static RefPtr<CSSValue> consumeSingleTransitionPropertyIdent(CSSParserTokenRange& range, const CSSParserToken& token)
{
    if (token.id() == CSSValueAll)
        return consumeIdent(range);
    if (auto property = token.parseAsCSSPropertyID()) {
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(property);
    }
    return consumeCustomIdent(range);
}

RefPtr<CSSValue> consumeSingleTransitionPropertyOrNone(CSSParserTokenRange& range, const CSSParserContext&)
{
    // This variant of consumeSingleTransitionProperty is used for the slightly different
    // parse rules used for the 'transition' shorthand which allows 'none':
    //
    // <single-transition> = [ none | <single-transition-property> ] || <time> || <timing-function> || <time>

    auto& token = range.peek();
    if (token.type() != IdentToken)
        return nullptr;
    if (token.id() == CSSValueNone)
        return consumeIdent(range);

    return consumeSingleTransitionPropertyIdent(range, token);
}

RefPtr<CSSValue> consumeSingleTransitionProperty(CSSParserTokenRange& range, const CSSParserContext&)
{
    // https://www.w3.org/TR/css-transitions-1/#single-transition-property
    //
    // <single-transition-property> = all | <custom-ident>;
    //
    // "The <custom-ident> production in <single-transition-property> also excludes the keyword
    //  none, in addition to the keywords always excluded from <custom-ident>."

    auto& token = range.peek();
    if (token.type() != IdentToken)
        return nullptr;
    if (token.id() == CSSValueNone)
        return nullptr;

    return consumeSingleTransitionPropertyIdent(range, token);
}

static RefPtr<CSSValue> consumeSingleTextShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <single-text-shadow> = [ <color>? && <length>{2,3} ]
    // https://drafts.csswg.org/css-text-decor-3/#propdef-text-shadow

    RefPtr<CSSPrimitiveValue> color;

    RefPtr<CSSPrimitiveValue> horizontalOffset;
    RefPtr<CSSPrimitiveValue> verticalOffset;
    RefPtr<CSSPrimitiveValue> blurRadius;

    for (size_t i = 0; i < 2; i++) {
        if (range.atEnd())
            break;

        const CSSParserToken& nextToken = range.peek();

        // If we have come to a comma (e.g. if this range represents a comma-separated list of <single-text-shadow>s), we are done parsing this <single-text-shadow>.
        if (nextToken.type() == CommaToken)
            break;

        auto maybeColor = consumeColor(range, context);
        if (maybeColor) {
            if (color)
                return nullptr;
            color = maybeColor;
            continue;
        }

        if (horizontalOffset || verticalOffset || blurRadius)
            return nullptr;

        horizontalOffset = consumeLength(range, context);
        if (!horizontalOffset)
            return nullptr;
        verticalOffset = consumeLength(range, context);
        if (!verticalOffset)
            return nullptr;
        // FIXME: the CSS filters spec does not say this should be non-negative, but tests currently rely on this.
        blurRadius = consumeLength(range, context, ValueRange::NonNegative);
    }

    // In order for this to be a valid <shadow>, at least these lengths must be present.
    if (!horizontalOffset || !verticalOffset)
        return nullptr;
    return CSSShadowValue::create(WTFMove(horizontalOffset), WTFMove(verticalOffset), WTFMove(blurRadius), nullptr, nullptr, WTFMove(color));
}

RefPtr<CSSValue> consumeTextShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'text-shadow'> = none | [ <color>? && <length>{2,3} ]#
    // https://drafts.csswg.org/css-text-decor-3/#propdef-text-shadow

    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, consumeSingleTextShadow, context);
}

RefPtr<CSSValue> consumeTextDecorationLine(CSSParserTokenRange& range, const CSSParserContext&)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    Vector<CSSValueID, 4> list;
    while (true) {
        auto ident = consumeIdentRaw<CSSValueBlink, CSSValueUnderline, CSSValueOverline, CSSValueLineThrough>(range);
        if (!ident)
            break;
        if (list.contains(*ident))
            return nullptr;
        list.append(*ident);
    }
    if (list.isEmpty())
        return nullptr;
    CSSValueListBuilder builder;
    for (auto ident : list)
        builder.append(CSSPrimitiveValue::create(ident));
    return CSSValueList::createSpaceSeparated(WTFMove(builder));
}

RefPtr<CSSValue> consumeTextEmphasisStyle(CSSParserTokenRange& range, const CSSParserContext&)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    if (auto textEmphasisStyle = consumeString(range))
        return textEmphasisStyle;

    auto fill = consumeIdent<CSSValueFilled, CSSValueOpen>(range);
    auto shape = consumeIdent<CSSValueDot, CSSValueCircle, CSSValueDoubleCircle, CSSValueTriangle, CSSValueSesame>(range);
    if (!fill)
        fill = consumeIdent<CSSValueFilled, CSSValueOpen>(range);
    if (fill && shape)
        return CSSValueList::createSpaceSeparated(fill.releaseNonNull(), shape.releaseNonNull());
    return fill ? fill : shape;
}

RefPtr<CSSValue> consumePaintStroke(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    auto url = consumeURL(range);
    if (url) {
        RefPtr<CSSValue> parsedValue;
        if (range.peek().id() == CSSValueNone)
            parsedValue = consumeIdent(range);
        else
            parsedValue = consumeColor(range, context);
        if (parsedValue)
            return CSSValueList::createSpaceSeparated(url.releaseNonNull(), parsedValue.releaseNonNull());
        return url;
    }
    return consumeColor(range, context);
}

RefPtr<CSSValue> consumePaintOrder(CSSParserTokenRange& range, const CSSParserContext&)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    Vector<CSSValueID, 3> paintTypeList;
    RefPtr<CSSPrimitiveValue> fill;
    RefPtr<CSSPrimitiveValue> stroke;
    RefPtr<CSSPrimitiveValue> markers;
    do {
        CSSValueID id = range.peek().id();
        if (id == CSSValueFill && !fill)
            fill = consumeIdent(range);
        else if (id == CSSValueStroke && !stroke)
            stroke = consumeIdent(range);
        else if (id == CSSValueMarkers && !markers)
            markers = consumeIdent(range);
        else
            return nullptr;
        paintTypeList.append(id);
    } while (!range.atEnd());

    // After parsing we serialize the paint-order list. Since it is not possible to
    // pop a last list items from CSSValueList without bigger cost, we create the
    // list after parsing.
    CSSValueID firstPaintOrderType = paintTypeList.at(0);
    CSSValueListBuilder paintOrderList;
    switch (firstPaintOrderType) {
    case CSSValueFill:
    case CSSValueStroke:
        paintOrderList.append(firstPaintOrderType == CSSValueFill ? fill.releaseNonNull() : stroke.releaseNonNull());
        if (paintTypeList.size() > 1) {
            if (paintTypeList.at(1) == CSSValueMarkers)
                paintOrderList.append(markers.releaseNonNull());
        }
        break;
    case CSSValueMarkers:
        paintOrderList.append(markers.releaseNonNull());
        if (paintTypeList.size() > 1) {
            if (paintTypeList.at(1) == CSSValueStroke)
                paintOrderList.append(stroke.releaseNonNull());
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return CSSValueList::createSpaceSeparated(WTFMove(paintOrderList));
}

RefPtr<CSSValue> consumeStrokeDasharray(CSSParserTokenRange& range, const CSSParserContext& context)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);
    CSSValueListBuilder dashes;
    do {
        RefPtr<CSSPrimitiveValue> dash = consumeLengthPercentage(range, context, HTMLStandardMode, ValueRange::NonNegative, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
        if (!dash)
            dash = consumeNumber(range, context, ValueRange::NonNegative);
        if (!dash || (consumeCommaIncludingWhitespace(range) && range.atEnd()))
            return nullptr;
        dashes.append(dash.releaseNonNull());
    } while (!range.atEnd());
    return CSSValueList::createCommaSeparated(WTFMove(dashes));
}

RefPtr<CSSValue> consumeCursor(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/css-ui/#propdef-cursor

    CSSValueListBuilder list;
    while (auto image = consumeImage(range, context, { AllowedImageType::URLFunction, AllowedImageType::ImageSet })) {
        RefPtr<CSSValuePair> hotSpot;
        if (auto x = consumeNumber(range, context)) {
            auto y = consumeNumber(range, context);
            if (!y)
                return nullptr;
            hotSpot = CSSValuePair::createNoncoalescing(x.releaseNonNull(), y.releaseNonNull());
        }
        list.append(CSSCursorImageValue::create(image.releaseNonNull(), WTFMove(hotSpot), context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No));
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    CSSValueID id = range.peek().id();
    RefPtr<CSSValue> cursorType;
    if (id == CSSValueHand) {
        if (context.mode != HTMLQuirksMode) // Non-standard behavior
            return nullptr;
        cursorType = CSSPrimitiveValue::create(CSSValuePointer);
        range.consumeIncludingWhitespace();
    } else if ((id >= CSSValueAuto && id <= CSSValueWebkitZoomOut) || id == CSSValueCopy || id == CSSValueNone)
        cursorType = consumeIdent(range);
    else
        return nullptr;

    if (list.isEmpty())
        return cursorType;
    list.append(cursorType.releaseNonNull());
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeAttr(CSSParserTokenRange args, const CSSParserContext& context)
{
    if (args.peek().type() != IdentToken)
        return nullptr;

    CSSParserToken token = args.consumeIncludingWhitespace();

    AtomString attrName;
    if (context.isHTMLDocument)
        attrName = token.value().convertToASCIILowercaseAtom();
    else
        attrName = token.value().toAtomString();

    if (!args.atEnd() && !consumeCommaIncludingWhitespace(args))
        return nullptr;

    RefPtr<CSSValue> fallback;
    if (args.peek().type() == StringToken) {
        token = args.consumeIncludingWhitespace();
        fallback = CSSPrimitiveValue::create(token.value().toString());
    }

    if (!args.atEnd())
        return nullptr;

    auto attr = CSSAttrValue::create(WTFMove(attrName), WTFMove(fallback));
    // FIXME: Consider moving to a CSSFunctionValue with a custom-ident rather than a special CSS_ATTR primitive value.

    return CSSPrimitiveValue::create(WTFMove(attr));
}

static RefPtr<CSSValue> consumeCounterContent(CSSParserTokenRange args, const CSSParserContext& context, bool counters)
{
    AtomString identifier { consumeCustomIdentRaw(args) };
    if (identifier.isNull())
        return nullptr;

    AtomString separator;
    if (counters) {
        if (!consumeCommaIncludingWhitespace(args) || args.peek().type() != StringToken)
            return nullptr;
        separator = args.consumeIncludingWhitespace().value().toAtomString();
    }

    RefPtr<CSSValue> listStyleType = CSSPrimitiveValue::create(CSSValueDecimal);
    if (consumeCommaIncludingWhitespace(args)) {
        if (args.peek().id() == CSSValueNone || args.peek().type() == StringToken)
            return nullptr;
        listStyleType = consumeListStyleType(args, context);
        if (!listStyleType)
            return nullptr;
    }

    if (!args.atEnd())
        return nullptr;

    return CSSCounterValue::create(WTFMove(identifier), WTFMove(separator), WTFMove(listStyleType));
}

RefPtr<CSSValue> consumeContent(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (identMatches<CSSValueNone, CSSValueNormal>(range.peek().id()))
        return consumeIdent(range);

    enum class ContentListType : bool { VisibleContent, AltText };
    auto consumeContentList = [&](CSSValueListBuilder& values, ContentListType type) -> bool {
        bool shouldEnd = false;
        do {
            RefPtr<CSSValue> parsedValue = consumeString(range);
            if (type == ContentListType::VisibleContent) {
                if (!parsedValue)
                    parsedValue = consumeImage(range, context);
                if (!parsedValue)
                    parsedValue = consumeIdent<CSSValueOpenQuote, CSSValueCloseQuote, CSSValueNoOpenQuote, CSSValueNoCloseQuote>(range);
            }
            if (!parsedValue) {
                if (range.peek().functionId() == CSSValueAttr)
                    parsedValue = consumeAttr(consumeFunction(range), context);
                // FIXME: Alt-text should support counters.
                else if (type == ContentListType::VisibleContent) {
                    if (range.peek().functionId() == CSSValueCounter)
                        parsedValue = consumeCounterContent(consumeFunction(range), context, false);
                    else if (range.peek().functionId() == CSSValueCounters)
                        parsedValue = consumeCounterContent(consumeFunction(range), context, true);
                }
                if (!parsedValue)
                    return false;
            }
            values.append(parsedValue.releaseNonNull());

            // Visible content parsing ends at '/' or end of range.
            if (type == ContentListType::VisibleContent && !range.atEnd()) {
                CSSParserToken value = range.peek();
                if (value.type() == DelimiterToken && value.delimiter() == '/')
                    shouldEnd = true;
            }
            shouldEnd = shouldEnd || range.atEnd();
        } while (!shouldEnd);
        return true;
    };

    CSSValueListBuilder visibleContent;
    if (!consumeContentList(visibleContent, ContentListType::VisibleContent))
        return nullptr;

    // Consume alt-text content if there is any.
    if (consumeSlashIncludingWhitespace(range)) {
        CSSValueListBuilder altText;
        if (!consumeContentList(altText, ContentListType::AltText))
            return nullptr;
        return CSSValuePair::createSlashSeparated(
            CSSValueList::createSpaceSeparated(WTFMove(visibleContent)),
            CSSValueList::createSpaceSeparated(WTFMove(altText))
        );
    }

    return CSSValueList::createSpaceSeparated(WTFMove(visibleContent));
}

RefPtr<CSSValue> consumeScrollSnapAlign(CSSParserTokenRange& range, const CSSParserContext&)
{
    auto firstValue = consumeIdent<CSSValueNone, CSSValueStart, CSSValueCenter, CSSValueEnd>(range);
    if (!firstValue)
        return nullptr;

    auto secondValue = consumeIdent<CSSValueNone, CSSValueStart, CSSValueCenter, CSSValueEnd>(range);
    bool shouldAddSecondValue = secondValue && !secondValue->equals(*firstValue);

    // Only add the second value if it differs from the first so that we produce the canonical
    // serialization of this CSSValueList.
    if (shouldAddSecondValue)
        return CSSValueList::createSpaceSeparated(firstValue.releaseNonNull(), secondValue.releaseNonNull());

    return CSSValueList::createSpaceSeparated(firstValue.releaseNonNull());
}

RefPtr<CSSValue> consumeScrollSnapType(CSSParserTokenRange& range, const CSSParserContext&)
{
    auto firstValue = consumeIdent<CSSValueNone, CSSValueX, CSSValueY, CSSValueBlock, CSSValueInline, CSSValueBoth>(range);
    if (!firstValue)
        return nullptr;

    // We only add the second value if it is not the initial value as described in specification
    // so that serialization of this CSSValueList produces the canonical serialization.
    auto secondValue = consumeIdent<CSSValueProximity, CSSValueMandatory>(range);
    if (secondValue && secondValue->valueID() != CSSValueProximity)
        return CSSValueList::createSpaceSeparated(firstValue.releaseNonNull(), secondValue.releaseNonNull());

    return CSSValueList::createSpaceSeparated(firstValue.releaseNonNull());
}

RefPtr<CSSValue> consumeScrollbarColor(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto ident = consumeIdent<CSSValueAuto>(range))
        return ident;

    if (auto thumbColor = consumeColor(range, context)) {
        if (auto trackColor = consumeColor(range, context))
            return CSSValuePair::createNoncoalescing(thumbColor.releaseNonNull(), trackColor.releaseNonNull());
    }

    return nullptr;
}

RefPtr<CSSValue> consumeScrollbarGutter(CSSParserTokenRange& range, const CSSParserContext&)
{
    if (auto ident = consumeIdent<CSSValueAuto>(range))
        return CSSPrimitiveValue::create(CSSValueAuto);

    if (auto first = consumeIdent<CSSValueStable>(range)) {
        if (auto second = consumeIdent<CSSValueBothEdges>(range))
            return CSSValuePair::create(first.releaseNonNull(), second.releaseNonNull());
        return first;
    }

    if (auto first = consumeIdent<CSSValueBothEdges>(range)) {
        if (auto second = consumeIdent<CSSValueStable>(range))
            return CSSValuePair::create(second.releaseNonNull(), first.releaseNonNull());
        return nullptr;
    }

    return nullptr;
}

static RefPtr<CSSValue> consumeTextEdge(CSSParserTokenRange& range)
{
    // <text-edge> = [ text | cap | ex | ideographic | ideographic-ink ]
    //               [ text | alphabetic | ideographic | ideographic-ink ]?
    // https://drafts.csswg.org/css-inline-3/#typedef-text-edge

    auto firstValue = consumeIdent<CSSValueText, CSSValueCap, CSSValueEx, CSSValueIdeographic, CSSValueIdeographicInk>(range);
    if (!firstValue)
        return nullptr;

    auto secondValue = consumeIdent<CSSValueText, CSSValueAlphabetic, CSSValueIdeographic, CSSValueIdeographicInk>(range);

    // https://drafts.csswg.org/css-inline-3/#text-edges
    // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
    auto shouldSerializeSecondValue = [&]() {
        if (!secondValue)
            return false;
        if (firstValue->valueID() == CSSValueCap || firstValue->valueID() == CSSValueEx)
            return secondValue->valueID() != CSSValueText;
        return firstValue->valueID() != secondValue->valueID();
    }();
    if (!shouldSerializeSecondValue)
        return firstValue;

    return CSSValuePair::create(firstValue.releaseNonNull(), secondValue.releaseNonNull());
}

RefPtr<CSSValue> consumeLineFitEdge(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'line-fit-edge'> = leading | <text-edge>
    // https://drafts.csswg.org/css-inline-3/#propdef-line-fit-edge

    if (range.peek().id() == CSSValueLeading)
        return consumeIdent(range);
    return consumeTextEdge(range);
}

RefPtr<CSSValue> consumeTextBoxEdge(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'text-box-edge'> = auto | <text-edge>
    // https://drafts.csswg.org/css-inline-3/#propdef-text-box-edge

    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeTextEdge(range);
}

RefPtr<CSSValue> consumeWebKitRubyPosition(CSSParserTokenRange& range, const CSSParserContext&)
{
    if (range.peek().id() == CSSValueInterCharacter) {
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(CSSValueLegacyInterCharacter);
    }
    return consumeIdent<CSSValueBefore, CSSValueAfter>(range);
}

RefPtr<CSSValue> consumeLineBoxContain(CSSParserTokenRange& range, const CSSParserContext&)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    OptionSet<LineBoxContain> value;
    while (range.peek().type() == IdentToken) {
        auto flag = [&]() -> std::optional<LineBoxContain> {
            switch (range.peek().id()) {
            case CSSValueBlock:
                return LineBoxContain::Block;
            case CSSValueInline:
                return LineBoxContain::Inline;
            case CSSValueFont:
                return LineBoxContain::Font;
            case CSSValueGlyphs:
                return LineBoxContain::Glyphs;
            case CSSValueReplaced:
                return LineBoxContain::Replaced;
            case CSSValueInlineBox:
                return LineBoxContain::InlineBox;
            case CSSValueInitialLetter:
                return LineBoxContain::InitialLetter;
            default:
                return std::nullopt;
            }
        }();
        if (!flag || value.contains(*flag))
            return nullptr;
        value.add(flag);
        range.consumeIncludingWhitespace();
    }
    if (!value)
        return nullptr;
    return CSSLineBoxContainValue::create(value);
}

RefPtr<CSSValue> consumeContainerName(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    CSSValueListBuilder list;
    do {
        auto name = consumeSingleContainerName(range, context);
        if (!name)
            break;
        list.append(name.releaseNonNull());
    } while (!range.atEnd());
    if (list.isEmpty())
        return nullptr;
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeWebkitInitialLetter(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto ident = consumeIdent<CSSValueNormal>(range))
        return ident;
    auto height = consumeNumber(range, context, ValueRange::NonNegative);
    if (!height)
        return nullptr;
    RefPtr<CSSPrimitiveValue> position;
    if (!range.atEnd()) {
        position = consumeNumber(range, context, ValueRange::NonNegative);
        if (!position || !range.atEnd())
            return nullptr;
    } else
        position = height.copyRef();
    return CSSValuePair::create(position.releaseNonNull(), height.releaseNonNull());
}

RefPtr<CSSValue> consumeSpeakAs(CSSParserTokenRange& range, const CSSParserContext&)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    CSSValueListBuilder list;

    bool seenNormal = false;
    bool seenSpellOut = false;
    bool seenLiteralPunctuation = false;
    bool seenNoPunctuation = false;

    // normal | spell-out || digits || [ literal-punctuation | no-punctuation ]
    while (!range.atEnd()) {
        CSSValueID valueID = range.peek().id();
        if ((valueID == CSSValueNormal && seenSpellOut)
            || (valueID == CSSValueSpellOut && seenNormal)
            || (valueID == CSSValueLiteralPunctuation && seenNoPunctuation)
            || (valueID == CSSValueNoPunctuation && seenLiteralPunctuation))
            return nullptr;
        auto ident = consumeIdent<CSSValueNormal, CSSValueSpellOut, CSSValueDigits, CSSValueLiteralPunctuation, CSSValueNoPunctuation>(range);
        if (!ident)
            return nullptr;
        switch (valueID) {
        case CSSValueNormal:
            seenNormal = true;
            break;
        case CSSValueSpellOut:
            seenSpellOut = true;
            break;
        case CSSValueLiteralPunctuation:
            seenLiteralPunctuation = true;
            break;
        case CSSValueNoPunctuation:
            seenNoPunctuation = true;
            break;
        default:
            break;
        }
        list.append(ident.releaseNonNull());
    }

    if (list.isEmpty())
        return nullptr;
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeHangingPunctuation(CSSParserTokenRange& range, const CSSParserContext&)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    CSSValueListBuilder list;

    bool seenForceEnd = false;
    bool seenAllowEnd = false;
    bool seenFirst = false;
    bool seenLast = false;

    while (!range.atEnd()) {
        CSSValueID valueID = range.peek().id();
        if ((valueID == CSSValueFirst && seenFirst)
            || (valueID == CSSValueLast && seenLast)
            || (valueID == CSSValueAllowEnd && (seenAllowEnd || seenForceEnd))
            || (valueID == CSSValueForceEnd && (seenAllowEnd || seenForceEnd)))
            return nullptr;
        auto ident = consumeIdent<CSSValueAllowEnd, CSSValueForceEnd, CSSValueFirst, CSSValueLast>(range);
        if (!ident)
            return nullptr;
        switch (valueID) {
        case CSSValueAllowEnd:
            seenAllowEnd = true;
            break;
        case CSSValueForceEnd:
            seenForceEnd = true;
            break;
        case CSSValueFirst:
            seenFirst = true;
            break;
        case CSSValueLast:
            seenLast = true;
            break;
        default:
            break;
        }
        list.append(ident.releaseNonNull());
    }

    if (list.isEmpty())
        return nullptr;
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeContain(CSSParserTokenRange& range, const CSSParserContext&)
{
    if (auto singleValue = consumeIdent<CSSValueNone, CSSValueStrict, CSSValueContent>(range))
        return singleValue;
    RefPtr<CSSPrimitiveValue> values[5];
    auto& size = values[0];
    auto& inlineSize = values[1];
    auto& layout = values[2];
    auto& style = values[3];
    auto& paint = values[4];
    while (!range.atEnd()) {
        switch (range.peek().id()) {
        case CSSValueSize:
            if (size)
                return nullptr;
            size = consumeIdent(range);
            break;
        case CSSValueInlineSize:
            if (inlineSize || size)
                return nullptr;
            inlineSize = consumeIdent(range);
            break;
        case CSSValueLayout:
            if (layout)
                return nullptr;
            layout = consumeIdent(range);
            break;
        case CSSValuePaint:
            if (paint)
                return nullptr;
            paint = consumeIdent(range);
            break;
        case CSSValueStyle:
            if (style)
                return nullptr;
            style = consumeIdent(range);
            break;
        default:
            return nullptr;
        }
    }
    CSSValueListBuilder list;
    for (auto& value : values) {
        if (value)
            list.append(value.releaseNonNull());
    }
    if (list.isEmpty())
        return nullptr;
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeContainIntrinsicSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    RefPtr<CSSPrimitiveValue> autoValue;
    if ((autoValue = consumeIdent<CSSValueAuto>(range))) {
        if (range.atEnd())
            return nullptr;
    }

    if (auto noneValue = consumeIdent<CSSValueNone>(range)) {
        if (autoValue)
            return CSSValuePair::create(autoValue.releaseNonNull(), noneValue.releaseNonNull());
        return noneValue;
    }

    if (auto lengthValue = consumeLength(range, context, HTMLStandardMode, ValueRange::NonNegative)) {
        if (autoValue)
            return CSSValuePair::create(autoValue.releaseNonNull(), lengthValue.releaseNonNull());
        return lengthValue;
    }
    return nullptr;
}

RefPtr<CSSValue> consumeTextEmphasisPosition(CSSParserTokenRange& range, const CSSParserContext&)
{
    std::optional<CSSValueID> overUnderValueID;
    std::optional<CSSValueID> leftRightValueID;
    while (!range.atEnd()) {
        auto valueID = range.peek().id();
        switch (valueID) {
        case CSSValueOver:
        case CSSValueUnder:
            if (overUnderValueID)
                return nullptr;
            overUnderValueID = valueID;
            break;
        case CSSValueLeft:
        case CSSValueRight:
            if (leftRightValueID)
                return nullptr;
            leftRightValueID = valueID;
            break;
        default:
            return nullptr;
        }
        range.consumeIncludingWhitespace();
    }
    if (!overUnderValueID)
        return nullptr;
    if (!leftRightValueID)
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(*overUnderValueID));
    return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(*overUnderValueID),
        CSSPrimitiveValue::create(*leftRightValueID));
}

RefPtr<CSSValue> consumeDeclarationValue(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return CSSVariableParser::parseDeclarationValue(nullAtom(), range.consumeAll(), context);
}

RefPtr<CSSValue> consumeTextAutospace(CSSParserTokenRange& range, const CSSParserContext&)
{
    //  normal | auto | no-autospace | [ ideograph-alpha || ideograph-numeric || punctuation ] || [ insert | replace ]
    // FIXME: add remaining values;
    if (auto value = consumeIdent<CSSValueAuto, CSSValueNoAutospace, CSSValueNormal>(range)) {
        if (!range.atEnd())
            return nullptr;
        return value;
    }

    CSSValueListBuilder list;
    bool seenIdeographAlpha = false;
    bool seenIdeographNumeric = false;

    while (!range.atEnd()) {
        auto valueID = range.peek().id();

        if ((valueID == CSSValueIdeographAlpha && seenIdeographAlpha) || (valueID == CSSValueIdeographNumeric && seenIdeographNumeric))
            return nullptr;

        auto ident = consumeIdent<CSSValueIdeographAlpha, CSSValueIdeographNumeric>(range);
        if (!ident)
            return nullptr;
        switch (valueID) {
        case CSSValueIdeographAlpha:
            seenIdeographAlpha = true;
            break;
        case CSSValueIdeographNumeric:
            seenIdeographNumeric = true;
            break;
        default:
            return nullptr;
        }
    }

    if (seenIdeographAlpha)
        list.append(CSSPrimitiveValue::create(CSSValueIdeographAlpha));
    if (seenIdeographNumeric)
        list.append(CSSPrimitiveValue::create(CSSValueIdeographNumeric));

    if (list.isEmpty())
        return nullptr;
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
