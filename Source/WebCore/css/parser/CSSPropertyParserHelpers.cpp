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
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+Percent.h"
#include "CSSPropertyParserConsumer+PercentDefinitions.h"
#include "CSSPropertyParserConsumer+Position.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+String.h"
#include "CSSPropertyParserConsumer+Time.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParserConsumer+UnevaluatedCalc.h"
#include "CSSPropertyParsing.h"

#include "CSSBackgroundRepeatValue.h"
#include "CSSBasicShapes.h"
#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSCounterValue.h"
#include "CSSCursorImageValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFunctionValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSParser.h"
#include "CSSParserIdioms.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSQuadValue.h"
#include "CSSRayValue.h"
#include "CSSRectValue.h"
#include "CSSReflectValue.h"
#include "CSSScrollValue.h"
#include "CSSShapeSegmentValue.h"
#include "CSSSubgridValue.h"
#include "CSSTransformListValue.h"
#include "CSSValueKeywords.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "CSSVariableData.h"
#include "CSSVariableParser.h"
#include "CSSViewValue.h"
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

namespace WebCore {

namespace CSSPropertyParserHelpers {


RefPtr<CSSShadowValue> consumeSingleShadow(CSSParserTokenRange& range, const CSSParserContext& context, bool allowInset, bool allowSpread, bool isWebkitBoxShadow)
{
    RefPtr<CSSPrimitiveValue> style;
    RefPtr<CSSPrimitiveValue> color;
    RefPtr<CSSPrimitiveValue> horizontalOffset;
    RefPtr<CSSPrimitiveValue> verticalOffset;
    RefPtr<CSSPrimitiveValue> blurRadius;
    RefPtr<CSSPrimitiveValue> spreadDistance;

    for (size_t i = 0; i < 3; i++) {
        if (range.atEnd())
            break;

        const CSSParserToken& nextToken = range.peek();
        // If we have come to a comma (e.g. if this range represents a comma-separated list of <shadow>s), we are done parsing this <shadow>.
        if (nextToken.type() == CommaToken)
            break;

        if (nextToken.id() == CSSValueInset) {
            if (!allowInset || style)
                return nullptr;
            style = consumeIdent(range);
            continue;
        }

        auto maybeColor = consumeColor(range, context);
        if (maybeColor) {
            // If we just parsed a color but already had one, the given token range is not a valid <shadow>.
            if (color)
                return nullptr;
            color = maybeColor;
            continue;
        }

        // If the current token is neither a color nor the `inset` keyword, it must be the lengths component of this value.
        if (horizontalOffset || verticalOffset || blurRadius || spreadDistance) {
            // If we've already parsed these lengths, the given value is invalid as there cannot be two lengths components in a single <shadow> value.
            return nullptr;
        }
        horizontalOffset = consumeLength(range, context.mode);
        if (!horizontalOffset)
            return nullptr;
        verticalOffset = consumeLength(range, context.mode);
        if (!verticalOffset)
            return nullptr;

        const CSSParserToken& token = range.peek();
        // The explicit check for calc() is unfortunate. This is ensuring that we only fail parsing if there is a length, but it fails the range check.
        if (token.type() == DimensionToken || token.type() == NumberToken || (token.type() == FunctionToken && CSSCalcValue::isCalcFunction(token.functionId()))) {
            blurRadius = consumeLength(range, context.mode, ValueRange::NonNegative);
            if (!blurRadius)
                return nullptr;
        }

        if (blurRadius && allowSpread)
            spreadDistance = consumeLength(range, context.mode);
    }

    // In order for this to be a valid <shadow>, at least these lengths must be present.
    if (!horizontalOffset || !verticalOffset)
        return nullptr;
    return CSSShadowValue::create(WTFMove(horizontalOffset), WTFMove(verticalOffset), WTFMove(blurRadius), WTFMove(spreadDistance), WTFMove(style), WTFMove(color), isWebkitBoxShadow);
}

// https://www.w3.org/TR/css-counter-styles-3/#predefined-counters
static bool isPredefinedCounterStyle(CSSValueID valueID)
{
    return valueID >= CSSValueDisc && valueID <= CSSValueEthiopicNumeric;
}

// https://www.w3.org/TR/css-counter-styles-3/#typedef-counter-style-name
RefPtr<CSSPrimitiveValue> consumeCounterStyleName(CSSParserTokenRange& range)
{
    // <counter-style-name> is a <custom-ident> that is not an ASCII case-insensitive match for "none".
    auto valueID = range.peek().id();
    if (valueID == CSSValueNone)
        return nullptr;
    // If the value is an ASCII case-insensitive match for any of the predefined counter styles, lowercase it.
    if (auto name = consumeCustomIdent(range, isPredefinedCounterStyle(valueID)))
        return name;
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#typedef-counter-style-name
AtomString consumeCounterStyleNameInPrelude(CSSParserTokenRange& prelude, CSSParserMode mode)
{
    auto nameToken = prelude.consumeIncludingWhitespace();
    if (!prelude.atEnd())
        return AtomString();
    // Ensure this token is a valid <custom-ident>.
    if (nameToken.type() != IdentToken || !isValidCustomIdentifier(nameToken.id()))
        return AtomString();
    // In the context of the prelude of an @counter-style rule, a <counter-style-name> must not be an ASCII
    // case-insensitive match for "decimal", "disc", "square", "circle", "disclosure-open" and "disclosure-closed". No <counter-style-name>, prelude or not, may be an ASCII
    // case-insensitive match for "none".
    auto id = nameToken.id();
    if (identMatches<CSSValueNone>(id) || (!isUASheetBehavior(mode) && identMatches<CSSValueDecimal, CSSValueDisc, CSSValueCircle, CSSValueSquare, CSSValueDisclosureOpen, CSSValueDisclosureClosed>(id)))
        return AtomString();
    auto name = nameToken.value();
    return isPredefinedCounterStyle(nameToken.id()) ? name.convertToASCIILowercaseAtom() : name.toAtomString();
}

RefPtr<CSSPrimitiveValue> consumeSingleContainerName(CSSParserTokenRange& range)
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

static RefPtr<CSSValueList> consumeAspectRatioValue(CSSParserTokenRange& range)
{
    auto leftValue = consumeNumber(range, ValueRange::NonNegative);
    if (!leftValue)
        return nullptr;

    bool slashSeen = consumeSlashIncludingWhitespace(range);
    auto rightValue = slashSeen
        ? consumeNumber(range, ValueRange::NonNegative)
        : CSSPrimitiveValue::create(1);
    if (!rightValue)
        return nullptr;

    return CSSValueList::createSlashSeparated(leftValue.releaseNonNull(), rightValue.releaseNonNull());
}

RefPtr<CSSValue> consumeAspectRatio(CSSParserTokenRange& range)
{
    RefPtr<CSSPrimitiveValue> autoValue;
    if (range.peek().type() == IdentToken)
        autoValue = consumeIdent<CSSValueAuto>(range);
    if (range.atEnd())
        return autoValue;
    auto ratioList = consumeAspectRatioValue(range);
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
RefPtr<CSSValue> consumeDisplay(CSSParserTokenRange& range, CSSParserMode mode)
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
        return !isRuby || isUASheetBehavior(mode);
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

RefPtr<CSSValue> consumeQuotes(CSSParserTokenRange& range)
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

static RefPtr<CSSValue> consumeCounter(CSSParserTokenRange& range, int defaultValue)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    CSSValueListBuilder list;
    do {
        auto counterName = consumeCustomIdent(range);
        if (!counterName)
            return nullptr;
        int i = defaultValue;
        if (auto counterValue = consumeIntegerRaw(range))
            i = *counterValue;
        list.append(CSSValuePair::create(counterName.releaseNonNull(), CSSPrimitiveValue::createInteger(i)));
    } while (!range.atEnd());
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeCounterIncrement(CSSParserTokenRange& range)
{
    return consumeCounter(range, 1);
}

RefPtr<CSSValue> consumeCounterReset(CSSParserTokenRange& range)
{
    return consumeCounter(range, 0);
}

RefPtr<CSSValue> consumeCounterSet(CSSParserTokenRange& range)
{
    return consumeCounter(range, 0);
}

RefPtr<CSSValue> consumeSize(CSSParserTokenRange& range, CSSParserMode mode)
{
    if (consumeIdentRaw<CSSValueAuto>(range))
        return CSSPrimitiveValue::create(CSSValueAuto);

    if (auto width = consumeLength(range, mode, ValueRange::NonNegative)) {
        auto height = consumeLength(range, mode, ValueRange::NonNegative);
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

RefPtr<CSSValue> consumeTextTransform(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeTextIndent(CSSParserTokenRange& range, CSSParserMode mode)
{
    // [ <length> | <percentage> ] && hanging? && each-line?
    RefPtr<CSSValue> lengthOrPercentage;
    bool eachLine = false;
    bool hanging = false;
    do {
        if (!lengthOrPercentage) {
            if (auto textIndent = consumeLengthOrPercent(range, mode, ValueRange::All, UnitlessQuirk::Allow)) {
                lengthOrPercentage = textIndent;
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
    if (!lengthOrPercentage)
        return nullptr;

    if (!hanging && !eachLine)
        return CSSValueList::createSpaceSeparated(lengthOrPercentage.releaseNonNull());
    if (hanging && !eachLine)
        return CSSValueList::createSpaceSeparated(lengthOrPercentage.releaseNonNull(), CSSPrimitiveValue::create(CSSValueHanging));
    if (!hanging)
        return CSSValueList::createSpaceSeparated(lengthOrPercentage.releaseNonNull(), CSSPrimitiveValue::create(CSSValueEachLine));
    return CSSValueList::createSpaceSeparated(lengthOrPercentage.releaseNonNull(),
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

static bool validWidthOrHeightKeyword(CSSValueID id)
{
    switch (id) {
    case CSSValueIntrinsic:
    case CSSValueMinIntrinsic:
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
    case CSSValueWebkitFillAvailable:
    case CSSValueFitContent:
    case CSSValueWebkitFitContent:
        return true;
    default:
        return false;
    }
}

static RefPtr<CSSValue> consumeAutoOrLengthOrPercent(CSSParserTokenRange& range, CSSParserMode mode, UnitlessQuirk unitless, AnchorPolicy anchorPolicy = AnchorPolicy::Forbid)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLengthOrPercent(range, mode, ValueRange::All, unitless, UnitlessZeroQuirk::Allow, NegativePercentagePolicy::Forbid, anchorPolicy);
}

RefPtr<CSSValue> consumeMarginSide(CSSParserTokenRange& range, CSSPropertyID currentShorthand, CSSParserMode mode)
{
    UnitlessQuirk unitless = currentShorthand != CSSPropertyInset ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
    return consumeAutoOrLengthOrPercent(range, mode, unitless);
}

RefPtr<CSSValue> consumeMarginTrim(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeSide(CSSParserTokenRange& range, CSSPropertyID currentShorthand, const CSSParserContext& context)
{
    UnitlessQuirk unitless = currentShorthand != CSSPropertyInset ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
    AnchorPolicy anchorPolicy = context.propertySettings.cssAnchorPositioningEnabled ? AnchorPolicy::Allow : AnchorPolicy::Forbid;
    return consumeAutoOrLengthOrPercent(range, context.mode, unitless, anchorPolicy);
}

RefPtr<CSSValue> consumeInsetLogicalStartEnd(CSSParserTokenRange& range, const CSSParserContext& context)
{
    AnchorPolicy anchorPolicy = context.propertySettings.cssAnchorPositioningEnabled ? AnchorPolicy::Allow : AnchorPolicy::Forbid;
    return consumeAutoOrLengthOrPercent(range, context.mode, UnitlessQuirk::Forbid, anchorPolicy);
}

static RefPtr<CSSPrimitiveValue> consumeClipComponent(CSSParserTokenRange& range, CSSParserMode mode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeLength(range, mode, ValueRange::All, UnitlessQuirk::Allow);
}

RefPtr<CSSValue> consumeClip(CSSParserTokenRange& range, CSSParserMode mode)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    if (range.peek().functionId() != CSSValueRect)
        return nullptr;

    CSSParserTokenRange args = consumeFunction(range);
    // rect(t, r, b, l) || rect(t r b l)
    auto top = consumeClipComponent(args, mode);
    if (!top)
        return nullptr;
    bool needsComma = consumeCommaIncludingWhitespace(args);
    auto right = consumeClipComponent(args, mode);
    if (!right || (needsComma && !consumeCommaIncludingWhitespace(args)))
        return nullptr;
    auto bottom = consumeClipComponent(args, mode);
    if (!bottom || (needsComma && !consumeCommaIncludingWhitespace(args)))
        return nullptr;
    auto left = consumeClipComponent(args, mode);
    if (!left || !args.atEnd())
        return nullptr;

    return CSSRectValue::create({ top.releaseNonNull(), right.releaseNonNull(), bottom.releaseNonNull(), left.releaseNonNull() });
}

RefPtr<CSSValue> consumeTouchAction(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeSingleTransitionPropertyOrNone(CSSParserTokenRange& range)
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

static RefPtr<CSSValue> consumeShadow(CSSParserTokenRange& range, const CSSParserContext& context, bool isBoxShadowProperty, bool isWebkitBoxShadow)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, consumeSingleShadow, context, isBoxShadowProperty, isBoxShadowProperty, isWebkitBoxShadow);
}

RefPtr<CSSValue> consumeTextShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeShadow(range, context, false, false);
}

RefPtr<CSSValue> consumeBoxShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeShadow(range, context, true, false);
}

RefPtr<CSSValue> consumeWebkitBoxShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeShadow(range, context, true, true);
}

RefPtr<CSSValue> consumeTextDecorationLine(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeTextEmphasisStyle(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeBorderWidth(CSSParserTokenRange& range, CSSPropertyID currentShorthand, const CSSParserContext& context)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueThin || id == CSSValueMedium || id == CSSValueThick)
        return consumeIdent(range);

    bool allowQuirkyLengths = (context.mode == HTMLQuirksMode) && (currentShorthand == CSSPropertyInvalid || currentShorthand == CSSPropertyBorderWidth);
    UnitlessQuirk unitless = allowQuirkyLengths ? UnitlessQuirk::Allow : UnitlessQuirk::Forbid;
    return consumeLength(range, context.mode, ValueRange::NonNegative, unitless);
}

RefPtr<CSSValue> consumeBorderColor(CSSParserTokenRange& range, CSSPropertyID currentShorthand, const CSSParserContext& context)
{
    bool acceptQuirkyColors = (context.mode == HTMLQuirksMode) && (currentShorthand == CSSPropertyInvalid || currentShorthand == CSSPropertyBorderColor);
    return consumeColor(range, context, { .acceptQuirkyColors = acceptQuirkyColors });
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

RefPtr<CSSValue> consumePaintOrder(CSSParserTokenRange& range)
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

bool isFlexBasisIdent(CSSValueID id)
{
    return identMatches<CSSValueAuto, CSSValueContent>(id) || validWidthOrHeightKeyword(id);
}

RefPtr<CSSValue> consumeStrokeDasharray(CSSParserTokenRange& range)
{
    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);
    CSSValueListBuilder dashes;
    do {
        RefPtr<CSSPrimitiveValue> dash = consumeLengthOrPercent(range, HTMLStandardMode, ValueRange::NonNegative, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
        if (!dash)
            dash = consumeNumber(range, ValueRange::NonNegative);
        if (!dash || (consumeCommaIncludingWhitespace(range) && range.atEnd()))
            return nullptr;
        dashes.append(dash.releaseNonNull());
    } while (!range.atEnd());
    return CSSValueList::createCommaSeparated(WTFMove(dashes));
}

RefPtr<CSSValue> consumeCursor(CSSParserTokenRange& range, const CSSParserContext& context, bool inQuirksMode)
{
    CSSValueListBuilder list;
    while (auto image = consumeImage(range, context, { AllowedImageType::URLFunction, AllowedImageType::ImageSet })) {
        std::optional<IntPoint> hotSpot;
        if (auto x = consumeNumberRaw(range)) {
            auto y = consumeNumberRaw(range);
            if (!y)
                return nullptr;
            // FIXME: Should we clamp or round instead of just casting from double to int?
            hotSpot = IntPoint { static_cast<int>(x->value), static_cast<int>(y->value) };
        }
        list.append(CSSCursorImageValue::create(image.releaseNonNull(), hotSpot, context.isContentOpaque ? LoadedFromOpaqueSource::Yes : LoadedFromOpaqueSource::No));
        if (!consumeCommaIncludingWhitespace(range))
            return nullptr;
    }

    CSSValueID id = range.peek().id();
    RefPtr<CSSValue> cursorType;
    if (id == CSSValueHand) {
        if (!inQuirksMode) // Non-standard behavior
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

    if (!args.atEnd())
        return nullptr;

    // FIXME: Consider moving to a CSSFunctionValue with a custom-ident rather than a special CSS_ATTR primitive value.
    return CSSPrimitiveValue::createAttr(WTFMove(attrName));
}

static RefPtr<CSSValue> consumeCounterContent(CSSParserTokenRange args, bool counters, const CSSParserContext& context)
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
                        parsedValue = consumeCounterContent(consumeFunction(range), false, context);
                    else if (range.peek().functionId() == CSSValueCounters)
                        parsedValue = consumeCounterContent(consumeFunction(range), true, context);
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

RefPtr<CSSValue> consumeScrollSnapAlign(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeScrollSnapType(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeScrollbarGutter(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeTextEdge(CSSPropertyID property, CSSParserTokenRange& range)
{
    if (property == CSSPropertyTextBoxEdge && range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    if (property == CSSPropertyLineFitEdge && range.peek().id() == CSSValueLeading)
        return consumeIdent(range);

    auto firstValue = consumeIdent<CSSValueText, CSSValueCap, CSSValueEx, CSSValueIdeographic, CSSValueIdeographicInk>(range);
    if (!firstValue)
        return nullptr;

    auto secondValue = consumeIdent<CSSValueText, CSSValueAlphabetic, CSSValueIdeographic, CSSValueIdeographicInk>(range);
    // https://www.w3.org/TR/css-inline-3/#text-edges
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

RefPtr<CSSValue> consumeBorderRadiusCorner(CSSParserTokenRange& range, CSSParserMode mode)
{
    auto parsedValue1 = consumeLengthOrPercent(range, mode, ValueRange::NonNegative);
    if (!parsedValue1)
        return nullptr;
    auto parsedValue2 = consumeLengthOrPercent(range, mode, ValueRange::NonNegative);
    if (!parsedValue2)
        parsedValue2 = parsedValue1;
    return CSSValuePair::create(parsedValue1.releaseNonNull(), parsedValue2.releaseNonNull());
}

static RefPtr<CSSPrimitiveValue> consumeShapeRadius(CSSParserTokenRange& args, CSSParserMode mode)
{
    if (identMatches<CSSValueClosestSide, CSSValueFarthestSide, CSSValueClosestCorner, CSSValueFarthestCorner>(args.peek().id()))
        return consumeIdent(args);

    return consumeLengthOrPercent(args, mode, ValueRange::NonNegative);
}

static RefPtr<CSSCircleValue> consumeBasicShapeCircle(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
    // circle( [<shape-radius>]? [at <position>]? )
    auto radius = consumeShapeRadius(args, context.mode);
    std::optional<PositionCoordinates> center;
    if (consumeIdent<CSSValueAt>(args)) {
        center = consumePositionCoordinates(args, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
        if (!center)
            return nullptr;
    }
    if (!center)
        return CSSCircleValue::create(WTFMove(radius), nullptr, nullptr);
    return CSSCircleValue::create(WTFMove(radius), WTFMove(center->x), WTFMove(center->y));
}

static RefPtr<CSSEllipseValue> consumeBasicShapeEllipse(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // spec: https://drafts.csswg.org/css-shapes/#supported-basic-shapes
    // ellipse( [<shape-radius>{2}]? [at <position>]? )
    auto radiusX = consumeShapeRadius(args, context.mode);
    RefPtr<CSSValue> radiusY;
    if (radiusX) {
        radiusY = consumeShapeRadius(args, context.mode);
        if (!radiusY)
            return nullptr;
    }
    std::optional<PositionCoordinates> center;
    if (consumeIdent<CSSValueAt>(args)) {
        center = consumePositionCoordinates(args, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
        if (!center)
            return nullptr;
    }
    if (!center)
        return CSSEllipseValue::create(WTFMove(radiusX), WTFMove(radiusY), nullptr, nullptr);
    return CSSEllipseValue::create(WTFMove(radiusX), WTFMove(radiusY), WTFMove(center->x), WTFMove(center->y));
}

static RefPtr<CSSPolygonValue> consumeBasicShapePolygon(CSSParserTokenRange& args, const CSSParserContext& context)
{
    auto rule = WindRule::NonZero;
    if (identMatches<CSSValueEvenodd, CSSValueNonzero>(args.peek().id())) {
        if (args.consumeIncludingWhitespace().id() == CSSValueEvenodd)
            rule = WindRule::EvenOdd;
        if (!consumeCommaIncludingWhitespace(args))
            return nullptr;
    }

    CSSValueListBuilder points;
    do {
        auto xLength = consumeLengthOrPercent(args, context.mode);
        if (!xLength)
            return nullptr;
        auto yLength = consumeLengthOrPercent(args, context.mode);
        if (!yLength)
            return nullptr;
        points.append(xLength.releaseNonNull());
        points.append(yLength.releaseNonNull());
    } while (consumeCommaIncludingWhitespace(args));

    return CSSPolygonValue::create(WTFMove(points), rule);
}

static RefPtr<CSSPathValue> consumeBasicShapePath(CSSParserTokenRange& args, OptionSet<PathParsingOption> options)
{
    auto rule = WindRule::NonZero;
    if (identMatches<CSSValueEvenodd, CSSValueNonzero>(args.peek().id())) {
        if (options.contains(RejectFillRule))
            return nullptr;
        if (args.consumeIncludingWhitespace().id() == CSSValueEvenodd)
            rule = WindRule::EvenOdd;
        if (!consumeCommaIncludingWhitespace(args))
            return nullptr;
    }

    if (args.peek().type() != StringToken)
        return nullptr;

    SVGPathByteStream byteStream;
    if (!buildSVGPathByteStreamFromString(args.consumeIncludingWhitespace().value(), byteStream, UnalteredParsing) || byteStream.isEmpty())
        return nullptr;

    return CSSPathValue::create(WTFMove(byteStream), rule);
}

static RefPtr<CSSValuePair> consumeCoordinatePair(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto xDimension = consumeLengthOrPercent(range, context.mode);
    if (!xDimension)
        return nullptr;

    auto yDimension = consumeLengthOrPercent(range, context.mode);
    if (!yDimension)
        return nullptr;

    return CSSValuePair::createNoncoalescing(xDimension.releaseNonNull(), yDimension.releaseNonNull());
}

static RefPtr<CSSValue> consumeShapeCommand(CSSParserTokenRange& range, const CSSParserContext& context, OptionSet<PathParsingOption>)
{
    if (range.peek().type() != IdentToken)
        return nullptr;

    auto consumeAffinity = [&]() -> std::optional<CoordinateAffinity> {
        if (range.peek().type() != IdentToken)
            return std::nullopt;

        CSSValueID token = range.peek().id();
        if (token != CSSValueBy && token != CSSValueTo)
            return std::nullopt;

        if (!consumeIdent(range))
            return std::nullopt;

        return token == CSSValueBy ? CoordinateAffinity::Relative : CoordinateAffinity::Absolute;
    };

    auto atEndOfCommand = [&] () {
        return range.atEnd() || range.peek().type() == CommaToken;
    };

    auto id = range.consumeIncludingWhitespace().id();
    switch (id) {
    case CSSValueMove: {
        // <move-command> = move <by-to> <coordinate-pair>
        auto affinityValue = consumeAffinity();
        if (!affinityValue)
            return nullptr;

        auto toCoordinates = consumeCoordinatePair(range, context);
        if (!toCoordinates)
            return nullptr;

        return CSSShapeSegmentValue::createMove(*affinityValue, toCoordinates.releaseNonNull());
    }
    case CSSValueLine: {
        // <line-command> = line <by-to> <coordinate-pair>
        auto affinityValue = consumeAffinity();
        if (!affinityValue)
            return nullptr;

        auto toCoordinates = consumeCoordinatePair(range, context);
        if (!toCoordinates)
            return nullptr;

        return CSSShapeSegmentValue::createLine(*affinityValue, toCoordinates.releaseNonNull());
    }
    case CSSValueHline:
    case CSSValueVline: {
        // <hv-line-command> = [hline | vline] <by-to> <length-percentage>
        auto affinityValue = consumeAffinity();
        if (!affinityValue)
            return nullptr;

        auto length = consumeLengthOrPercent(range, context.mode);
        if (!length)
            return nullptr;

        if (id == CSSValueHline)
            return CSSShapeSegmentValue::createHorizontalLine(*affinityValue, length.releaseNonNull());

        return CSSShapeSegmentValue::createVerticalLine(*affinityValue, length.releaseNonNull());
    }
    case CSSValueCurve: {
        // <curve-command> = curve <by-to> <coordinate-pair> using <coordinate-pair>{1,2}
        auto affinityValue = consumeAffinity();
        if (!affinityValue)
            return nullptr;

        auto toCoordinates = consumeCoordinatePair(range, context);
        if (!toCoordinates)
            return nullptr;

        if (!consumeIdent<CSSValueUsing>(range))
            return nullptr;

        auto controlPoint1 = consumeCoordinatePair(range, context);
        if (!controlPoint1)
            return nullptr;

        auto controlPoint2 = consumeCoordinatePair(range, context);
        if (controlPoint2)
            return CSSShapeSegmentValue::createCubicCurve(*affinityValue, toCoordinates.releaseNonNull(), controlPoint1.releaseNonNull(), controlPoint2.releaseNonNull());

        return CSSShapeSegmentValue::createQuadraticCurve(*affinityValue, toCoordinates.releaseNonNull(), controlPoint1.releaseNonNull());
    }
    case CSSValueSmooth: {
        // <smooth-command> = smooth <by-to> <coordinate-pair> [using <coordinate-pair>]?
        auto affinityValue = consumeAffinity();
        if (!affinityValue)
            return nullptr;

        auto toCoordinates = consumeCoordinatePair(range, context);
        if (!toCoordinates)
            return nullptr;

        if (consumeIdent<CSSValueUsing>(range)) {
            auto controlPoint = consumeCoordinatePair(range, context);
            if (!controlPoint)
                return nullptr;

            return CSSShapeSegmentValue::createSmoothCubicCurve(*affinityValue, toCoordinates.releaseNonNull(), controlPoint.releaseNonNull());
        }

        return CSSShapeSegmentValue::createSmoothQuadraticCurve(*affinityValue, toCoordinates.releaseNonNull());
    }
    case CSSValueArc: {
        // arc <by-to> <coordinate-pair> of <length-percentage>{1,2} [ <arc-sweep> || <arc-size> || rotate <angle> ]?
        auto affinityValue = consumeAffinity();
        if (!affinityValue)
            return nullptr;

        auto toCoordinates = consumeCoordinatePair(range, context);
        if (!toCoordinates)
            return nullptr;

        if (!consumeIdent<CSSValueOf>(range))
            return nullptr;

        auto radiusX = consumeLengthOrPercent(range, context.mode);
        auto radiusY = radiusX;
        if (auto value = consumeLengthOrPercent(range, context.mode))
            radiusY = value;

        std::optional<CSSValueID> sweep;
        std::optional<CSSValueID> size;
        RefPtr<CSSValue> angle;

        while (!atEndOfCommand()) {
            auto ident = consumeIdent<CSSValueCw, CSSValueCcw, CSSValueLarge, CSSValueSmall, CSSValueRotate>(range);
            if (!ident)
                return nullptr;

            switch (ident->valueID()) {
            case CSSValueCw:
            case CSSValueCcw:
                if (sweep)
                    return nullptr;
                sweep = ident->valueID();
                break;
            case CSSValueLarge:
            case CSSValueSmall:
                if (size)
                    return nullptr;
                size = ident->valueID();
                break;
            case CSSValueRotate:
                if (angle)
                    return nullptr;
                angle = consumeAngle(range, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
                break;
            default:
                break;
            }
        }

        if (!angle)
            angle = CSSPrimitiveValue::create(0, CSSUnitType::CSS_DEG);

        auto radius = CSSValuePair::create(radiusX.releaseNonNull(), radiusY.releaseNonNull());
        return CSSShapeSegmentValue::createArc(*affinityValue, toCoordinates.releaseNonNull(), WTFMove(radius), sweep.value_or(CSSValueCcw), size.value_or(CSSValueSmall), angle.releaseNonNull());
    }
    case CSSValueClose:
        return CSSShapeSegmentValue::createClose();
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return nullptr;
}

// https://drafts.csswg.org/css-shapes-2/#shape-function
static RefPtr<CSSShapeValue> consumeBasicShapeShape(CSSParserTokenRange& range, const CSSParserContext& context, OptionSet<PathParsingOption> options)
{
    if (!context.cssShapeFunctionEnabled)
        return nullptr;

    // shape() = shape( <'fill-rule'>? from <coordinate-pair>, <shape-command>#)
    auto rule = WindRule::NonZero;
    if (identMatches<CSSValueEvenodd, CSSValueNonzero>(range.peek().id())) {
        if (range.consumeIncludingWhitespace().id() == CSSValueEvenodd)
            rule = WindRule::EvenOdd;
    }

    if (!consumeIdent<CSSValueFrom>(range))
        return nullptr;

    auto fromCoordinates = consumeCoordinatePair(range, context);
    if (!fromCoordinates)
        return nullptr;

    if (!consumeCommaIncludingWhitespace(range))
        return nullptr;

    CSSValueListBuilder commands;
    do {
        auto command = consumeShapeCommand(range, context, options);
        if (!command)
            return nullptr;

        commands.append(command.releaseNonNull());
    } while (consumeCommaIncludingWhitespace(range));

    return CSSShapeValue::create(rule, fromCoordinates.releaseNonNull(), WTFMove(commands));
}

template<typename ElementType> static void complete4Sides(std::array<ElementType, 4>& sides)
{
    if (!sides[1])
        sides[1] = sides[0];
    if (!sides[2])
        sides[2] = sides[0];
    if (!sides[3])
        sides[3] = sides[1];
}

bool consumeRadii(std::array<RefPtr<CSSValue>, 4>& horizontalRadii, std::array<RefPtr<CSSValue>, 4>& verticalRadii, CSSParserTokenRange& range, CSSParserMode mode, bool useLegacyParsing)
{
    unsigned i = 0;
    for (; i < 4 && !range.atEnd() && range.peek().type() != DelimiterToken; ++i) {
        horizontalRadii[i] = consumeLengthOrPercent(range, mode, ValueRange::NonNegative);
        if (!horizontalRadii[i])
            return false;
    }
    if (!horizontalRadii[0])
        return false;
    if (range.atEnd()) {
        // Legacy syntax: -webkit-border-radius: l1 l2; is equivalent to border-radius: l1 / l2;
        if (useLegacyParsing && i == 2) {
            verticalRadii[0] = horizontalRadii[1];
            horizontalRadii[1] = nullptr;
        } else {
            complete4Sides(horizontalRadii);
            for (unsigned i = 0; i < 4; ++i)
                verticalRadii[i] = horizontalRadii[i];
            return true;
        }
    } else {
        if (!consumeSlashIncludingWhitespace(range))
            return false;
        for (i = 0; i < 4 && !range.atEnd(); ++i) {
            verticalRadii[i] = consumeLengthOrPercent(range, mode, ValueRange::NonNegative);
            if (!verticalRadii[i])
                return false;
        }
        if (!verticalRadii[0] || !range.atEnd())
            return false;
    }
    complete4Sides(horizontalRadii);
    complete4Sides(verticalRadii);
    return true;
}

static bool consumeShapeBorderRadius(CSSParserTokenRange& args, const CSSParserContext& context, std::array<RefPtr<CSSValuePair>, 4>& radii)
{
    if (consumeIdentRaw<CSSValueRound>(args)) {
        std::array<RefPtr<CSSValue>, 4> horizontalRadii;
        std::array<RefPtr<CSSValue>, 4> verticalRadii;
        if (!consumeRadii(horizontalRadii, verticalRadii, args, context.mode, false))
            return false;
        for (unsigned i = 0; i < 4; ++i)
            radii[i] = CSSValuePair::create(horizontalRadii[i].releaseNonNull(), verticalRadii[i].releaseNonNull());
    }
    return true;
}

static RefPtr<CSSRectShapeValue> consumeBasicShapeRect(CSSParserTokenRange& args, const CSSParserContext& context)
{
    std::array<RefPtr<CSSValue>, 4> offsets;
    for (auto& offset : offsets) {
        offset = consumeAutoOrLengthOrPercent(args, context.mode, UnitlessQuirk::Forbid);

        if (!offset)
            return nullptr;
    }

    std::array<RefPtr<CSSValuePair>, 4> radii;
    if (consumeShapeBorderRadius(args, context, radii))
        return CSSRectShapeValue::create(offsets[0].releaseNonNull(), offsets[1].releaseNonNull(), offsets[2].releaseNonNull(), offsets[3].releaseNonNull(), WTFMove(radii[0]), WTFMove(radii[1]), WTFMove(radii[2]), WTFMove(radii[3]));
    return nullptr;
}

static RefPtr<CSSXywhValue> consumeBasicShapeXywh(CSSParserTokenRange& args, const CSSParserContext& context)
{
    std::array<RefPtr<CSSValue>, 2> insets;
    for (auto& inset : insets) {
        inset = consumeLengthOrPercent(args, context.mode);
        if (!inset)
            return nullptr;
    }

    std::array<RefPtr<CSSValue>, 2> dimensions;
    for (auto& dimension : dimensions) {
        dimension = consumeLengthOrPercent(args, context.mode, ValueRange::NonNegative);
        if (!dimension)
            return nullptr;
    }

    std::array<RefPtr<CSSValuePair>, 4> radii;
    if (consumeShapeBorderRadius(args, context, radii))
        return CSSXywhValue::create(insets[0].releaseNonNull(), insets[1].releaseNonNull(), dimensions[0].releaseNonNull(), dimensions[1].releaseNonNull(), WTFMove(radii[0]), WTFMove(radii[1]), WTFMove(radii[2]), WTFMove(radii[3]));
    return nullptr;
}

static RefPtr<CSSInsetShapeValue> consumeBasicShapeInset(CSSParserTokenRange& args, const CSSParserContext& context)
{
    std::array<RefPtr<CSSValue>, 4> sides;
    for (unsigned i = 0; i < 4; ++i) {
        sides[i] = consumeLengthOrPercent(args, context.mode);
        if (!sides[i])
            break;
    }
    if (!sides[0])
        return nullptr;
    complete4Sides(sides);

    std::array<RefPtr<CSSValuePair>, 4> radii;
    if (consumeShapeBorderRadius(args, context, radii))
        return CSSInsetShapeValue::create(sides[0].releaseNonNull(), sides[1].releaseNonNull(), sides[2].releaseNonNull(), sides[3].releaseNonNull(), WTFMove(radii[0]), WTFMove(radii[1]), WTFMove(radii[2]), WTFMove(radii[3]));
    return nullptr;
}

static RefPtr<CSSValue> consumeBasicShape(CSSParserTokenRange& range, const CSSParserContext& context, OptionSet<PathParsingOption> options)
{
    auto rangeCopy = range;

    RefPtr<CSSValue> result;
    if (rangeCopy.peek().type() != FunctionToken)
        return nullptr;
    auto id = rangeCopy.peek().functionId();
    auto args = consumeFunction(rangeCopy);
    if (id == CSSValueCircle)
        result = consumeBasicShapeCircle(args, context);
    else if (id == CSSValueEllipse)
        result = consumeBasicShapeEllipse(args, context);
    else if (id == CSSValuePolygon)
        result = consumeBasicShapePolygon(args, context);
    else if (id == CSSValueInset)
        result = consumeBasicShapeInset(args, context);
    else if (id == CSSValueRect)
        result = consumeBasicShapeRect(args, context);
    else if (id == CSSValueXywh)
        result = consumeBasicShapeXywh(args, context);
    else if (id == CSSValuePath)
        result = consumeBasicShapePath(args, options);
    else if (id == CSSValueShape)
        result = consumeBasicShapeShape(args, context, options);

    if (!result || !args.atEnd())
        return nullptr;

    range = rangeCopy;
    return result;
}

// Parses the ray() definition as defined in https://drafts.fxtf.org/motion-1/#ray-function
// ray( <angle> && <ray-size>? && contain? && [at <position>]? )
static RefPtr<CSSRayValue> consumeRayShape(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().type() != FunctionToken || range.peek().functionId() != CSSValueRay)
        return nullptr;

    auto args = consumeFunction(range);

    RefPtr<CSSPrimitiveValue> angle;
    std::optional<CSSValueID> size;
    bool isContaining = false;
    std::optional<PositionCoordinates> position;
    while (!args.atEnd()) {
        if (!angle && (angle = consumeAngle(args, context.mode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid)))
            continue;
        if (!size && (size = consumeIdentRaw<CSSValueClosestSide, CSSValueClosestCorner, CSSValueFarthestSide, CSSValueFarthestCorner, CSSValueSides>(args)))
            continue;
        if (!isContaining && (isContaining = consumeIdentRaw<CSSValueContain>(args).has_value()))
            continue;
        auto consumeAtPosition = [&](CSSParserTokenRange& subrange) -> std::optional<PositionCoordinates> {
            if (consumeIdentRaw<CSSValueAt>(subrange))
                return consumePositionCoordinates(subrange, context.mode, UnitlessQuirk::Forbid, PositionSyntax::Position);
            return std::nullopt;
        };
        if (!position && (position = consumeAtPosition(args)))
            continue;
        return nullptr;
    }

    // <angle> must be present.
    if (!angle)
        return nullptr;

    return CSSRayValue::create(
        angle.releaseNonNull(),
        size.value_or(CSSValueClosestSide),
        isContaining,
        position ? RefPtr { CSSValuePair::createNoncoalescing(WTFMove(position->x), WTFMove(position->y)) } : nullptr
    );
}

static RefPtr<CSSValue> consumeBasicShapeRayOrBox(CSSParserTokenRange& range, const CSSParserContext& context, OptionSet<PathParsingOption> options)
{
    CSSValueListBuilder list;
    RefPtr<CSSValue> shape;
    RefPtr<CSSValue> box;

    auto consumeShapeOrRay = [&](CSSParserTokenRange& subrange) -> RefPtr<CSSValue> {
        RefPtr<CSSValue> ray;
        if (!options.contains(RejectRay) && (ray = consumeRayShape(subrange, context)))
            return ray;
        return consumeBasicShape(subrange, context, options);
    };

    while (!range.atEnd()) {
        if (!shape && (shape = consumeShapeOrRay(range)))
            continue;

        // FIXME: The Motion Path spec calls for this to be a <coord-box>, not a <geometry-box>, the difference being that the former does not contain "margin-box" as a valid term.
        // However, the spec also has a few examples using "margin-box", so there seems to be some abiguity to be resolved. See: https://github.com/w3c/fxtf-drafts/issues/481.
        if (!box && (box = CSSPropertyParsing::consumeGeometryBox(range)))
            continue;

        break;
    }

    bool hasShape = !!shape;
    if (shape)
        list.append(shape.releaseNonNull());
    // Default value is border-box.
    if (box && (box->valueID() != CSSValueBorderBox || !hasShape))
        list.append(box.releaseNonNull());

    if (list.isEmpty())
        return nullptr;

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumePathOperation(CSSParserTokenRange& range, const CSSParserContext& context, OptionSet<PathParsingOption> options)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    if (auto url = consumeURL(range))
        return url;
    return consumeBasicShapeRayOrBox(range, context, options);
}

RefPtr<CSSValue> consumePath(CSSParserTokenRange& range, const CSSParserContext&)
{
    if (range.peek().type() != FunctionToken)
        return nullptr;
    if (range.peek().functionId() != CSSValuePath)
        return nullptr;
    auto args = consumeFunction(range);
    auto result = consumeBasicShapePath(args, { });
    if (!result || !args.atEnd())
        return nullptr;
    return result;
}

RefPtr<CSSValue> consumeListStyleType(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    if (range.peek().type() == StringToken)
        return consumeString(range);

    if (auto predefinedValues = consumeIdent(range, isPredefinedCounterStyle))
        return predefinedValues;

    if (context.propertySettings.cssCounterStyleAtRulesEnabled)
        return consumeCustomIdent(range);

    return nullptr;
}

RefPtr<CSSValue> consumeShapeOutside(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (auto imageValue = consumeImageOrNone(range, context))
        return imageValue;
    CSSValueListBuilder list;
    auto boxValue = CSSPropertyParsing::consumeShapeBox(range);
    bool hasShapeValue = false;
    if (auto shapeValue = consumeBasicShape(range, context, { })) {
        if (shapeValue->isPath())
            return nullptr;
        list.append(shapeValue.releaseNonNull());
        hasShapeValue = true;
    }
    if (!boxValue)
        boxValue = CSSPropertyParsing::consumeShapeBox(range);
    // margin-box is the default.
    if (boxValue && (boxValue->valueID() != CSSValueMarginBox || !hasShapeValue))
        list.append(boxValue.releaseNonNull());
    if (list.isEmpty())
        return nullptr;
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static bool isAuto(CSSValueID id)
{
    return identMatches<CSSValueAuto>(id);
}

static bool isNormalOrStretch(CSSValueID id)
{
    return identMatches<CSSValueNormal, CSSValueStretch>(id);
}

static bool isLeftOrRightKeyword(CSSValueID id)
{
    return identMatches<CSSValueLeft, CSSValueRight>(id);
}

bool isBaselineKeyword(CSSValueID id)
{
    return identMatches<CSSValueFirst, CSSValueLast, CSSValueBaseline>(id);
}

bool isContentPositionKeyword(CSSValueID id)
{
    return identMatches<CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueFlexStart, CSSValueFlexEnd>(id);
}

bool isContentPositionOrLeftOrRightKeyword(CSSValueID id)
{
    return isContentPositionKeyword(id) || isLeftOrRightKeyword(id);
}

static bool isContentDistributionKeyword(CSSValueID id)
{
    return identMatches<CSSValueSpaceBetween, CSSValueSpaceAround, CSSValueSpaceEvenly, CSSValueStretch>(id);
}

static bool isOverflowKeyword(CSSValueID id)
{
    return identMatches<CSSValueUnsafe, CSSValueSafe>(id);
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

RefPtr<CSSValue> consumeContentDistributionOverflowPosition(CSSParserTokenRange& range, IsPositionKeyword isPositionKeyword)
{
    ASSERT(isPositionKeyword);
    CSSValueID id = range.peek().id();
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
    CSSValueID overflow = isOverflowKeyword(id) ? range.consumeIncludingWhitespace().id() : CSSValueInvalid;
    if (isPositionKeyword(range.peek().id()))
        return CSSContentDistributionValue::create(CSSValueInvalid, range.consumeIncludingWhitespace().id(), overflow);
    return nullptr;
}

RefPtr<CSSValue> consumeJustifyContent(CSSParserTokenRange& range)
{
    // justify-content property does not allow the <baseline-position> values.
    if (isBaselineKeyword(range.peek().id()))
        return nullptr;
    return consumeContentDistributionOverflowPosition(range, isContentPositionOrLeftOrRightKeyword);
}

static RefPtr<CSSPrimitiveValue> consumeBorderImageRepeatKeyword(CSSParserTokenRange& range)
{
    return consumeIdent<CSSValueStretch, CSSValueRepeat, CSSValueSpace, CSSValueRound>(range);
}

RefPtr<CSSValue> consumeBorderImageRepeat(CSSParserTokenRange& range)
{
    auto horizontal = consumeBorderImageRepeatKeyword(range);
    if (!horizontal)
        return nullptr;
    auto vertical = consumeBorderImageRepeatKeyword(range);
    if (!vertical)
        vertical = horizontal;
    return CSSValuePair::create(horizontal.releaseNonNull(), vertical.releaseNonNull());
}

RefPtr<CSSValue> consumeBorderImageSlice(CSSPropertyID property, CSSParserTokenRange& range)
{
    bool fill = consumeIdentRaw<CSSValueFill>(range).has_value();
    std::array<RefPtr<CSSPrimitiveValue>, 4> slices;

    for (auto& value : slices) {
        value = consumePercent(range, ValueRange::NonNegative);
        if (!value)
            value = consumeNumber(range, ValueRange::NonNegative);
        if (!value)
            break;
    }
    if (!slices[0])
        return nullptr;
    if (consumeIdent<CSSValueFill>(range)) {
        if (fill)
            return nullptr;
        fill = true;
    }
    complete4Sides(slices);

    // FIXME: For backwards compatibility, -webkit-border-image, -webkit-mask-box-image and -webkit-box-reflect do fill by default.
    if (property == CSSPropertyWebkitBorderImage || property == CSSPropertyWebkitMaskBoxImage || property == CSSPropertyWebkitBoxReflect)
        fill = true;

    return CSSBorderImageSliceValue::create({ slices[0].releaseNonNull(), slices[1].releaseNonNull(), slices[2].releaseNonNull(), slices[3].releaseNonNull() }, fill);
}

RefPtr<CSSValue> consumeBorderImageOutset(CSSParserTokenRange& range)
{
    std::array<RefPtr<CSSPrimitiveValue>, 4> outsets;

    for (auto& value : outsets) {
        value = consumeNumber(range, ValueRange::NonNegative);
        if (!value)
            value = consumeLength(range, HTMLStandardMode, ValueRange::NonNegative);
        if (!value)
            break;
    }
    if (!outsets[0])
        return nullptr;
    complete4Sides(outsets);

    return CSSQuadValue::create({ outsets[0].releaseNonNull(), outsets[1].releaseNonNull(), outsets[2].releaseNonNull(), outsets[3].releaseNonNull() });
}

RefPtr<CSSValue> consumeBorderImageWidth(CSSPropertyID property, CSSParserTokenRange& range)
{
    std::array<RefPtr<CSSPrimitiveValue>, 4> widths;

    bool hasLength = false;
    for (auto& value : widths) {
        value = consumeNumber(range, ValueRange::NonNegative);
        if (value)
            continue;
        if (auto numericValue = consumeLengthOrPercent(range, HTMLStandardMode, ValueRange::NonNegative, UnitlessQuirk::Forbid)) {
            if (numericValue->isLength())
                hasLength = true;
            value = numericValue;
            continue;
        }
        value = consumeIdent<CSSValueAuto>(range);
        if (!value)
            break;
    }
    if (!widths[0])
        return nullptr;
    complete4Sides(widths);

    // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
    bool overridesBorderWidths = property == CSSPropertyWebkitBorderImage && hasLength;

    return CSSBorderImageWidthValue::create({ widths[0].releaseNonNull(), widths[1].releaseNonNull(), widths[2].releaseNonNull(), widths[3].releaseNonNull() }, overridesBorderWidths);
}

bool consumeBorderImageComponents(CSSPropertyID property, CSSParserTokenRange& range, const CSSParserContext& context, RefPtr<CSSValue>& source,
    RefPtr<CSSValue>& slice, RefPtr<CSSValue>& width, RefPtr<CSSValue>& outset, RefPtr<CSSValue>& repeat)
{
    do {
        if (!source) {
            source = consumeImageOrNone(range, context);
            if (source)
                continue;
        }
        if (!repeat) {
            repeat = consumeBorderImageRepeat(range);
            if (repeat)
                continue;
        }
        if (!slice) {
            slice = consumeBorderImageSlice(property, range);
            if (slice) {
                ASSERT(!width && !outset);
                if (consumeSlashIncludingWhitespace(range)) {
                    width = consumeBorderImageWidth(property, range);
                    if (consumeSlashIncludingWhitespace(range)) {
                        outset = consumeBorderImageOutset(range);
                        if (!outset)
                            return false;
                    } else if (!width)
                        return false;
                }
            } else
                return false;
        } else
            return false;
    } while (!range.atEnd());

    // If we're setting from the legacy shorthand, make sure to set the `mask-border-slice` fill to true.
    if (property == CSSPropertyWebkitMaskBoxImage && !slice)
        slice = CSSBorderImageSliceValue::create({ CSSPrimitiveValue::create(0), CSSPrimitiveValue::create(0), CSSPrimitiveValue::create(0), CSSPrimitiveValue::create(0) }, true);

    return true;
}

RefPtr<CSSValue> consumeReflect(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    auto direction = consumeIdentRaw<CSSValueAbove, CSSValueBelow, CSSValueLeft, CSSValueRight>(range);
    if (!direction)
        return nullptr;

    // FIXME: Does not seem right to create "0px" here. We'd like to omit "0px" when serializing if there is also no image.
    RefPtr<CSSPrimitiveValue> offset;
    if (range.atEnd())
        offset = CSSPrimitiveValue::create(0, CSSUnitType::CSS_PX);
    else {
        offset = consumeLengthOrPercent(range, context.mode);
        if (!offset)
            return nullptr;
    }

    RefPtr<CSSValue> mask;
    if (!range.atEnd()) {
        RefPtr<CSSValue> source;
        RefPtr<CSSValue> slice;
        RefPtr<CSSValue> width;
        RefPtr<CSSValue> outset;
        RefPtr<CSSValue> repeat;
        if (!consumeBorderImageComponents(CSSPropertyWebkitBoxReflect, range, context, source, slice, width, outset, repeat))
            return nullptr;
        mask = createBorderImageValue(WTFMove(source), WTFMove(slice), WTFMove(width), WTFMove(outset), WTFMove(repeat));
    }
    return CSSReflectValue::create(*direction, offset.releaseNonNull(), WTFMove(mask));
}

template<CSSPropertyID property> RefPtr<CSSValue> consumeBackgroundSize(CSSParserTokenRange& range, CSSParserMode mode)
{
    // https://www.w3.org/TR/css-backgrounds-3/#typedef-bg-size
    //
    //    <bg-size> = [ <length-percentage [0,∞]> | auto ]{1,2} | cover | contain
    //

    if (identMatches<CSSValueContain, CSSValueCover>(range.peek().id()))
        return consumeIdent(range);

    bool shouldCoalesce = true;
    RefPtr<CSSPrimitiveValue> horizontal = consumeIdent<CSSValueAuto>(range);
    if (!horizontal) {
        horizontal = consumeLengthOrPercent(range, mode, ValueRange::NonNegative, UnitlessQuirk::Forbid);
        if (!horizontal)
            return nullptr;
        shouldCoalesce = false;
    }

    RefPtr<CSSPrimitiveValue> vertical;
    if (!range.atEnd()) {
        vertical = consumeIdent<CSSValueAuto>(range);
        if (!vertical)
            vertical = consumeLengthOrPercent(range, mode, ValueRange::NonNegative, UnitlessQuirk::Forbid);
    }
    if (!vertical) {
        if constexpr (property == CSSPropertyWebkitBackgroundSize) {
            // Legacy syntax: "-webkit-background-size: 10px" is equivalent to "background-size: 10px 10px".
            vertical = horizontal;
        } else if constexpr (property == CSSPropertyBackgroundSize) {
            vertical = CSSPrimitiveValue::create(CSSValueAuto);
        } else if constexpr (property == CSSPropertyMaskSize) {
            return horizontal;
        }
    }

    if (shouldCoalesce)
        return CSSValuePair::create(horizontal.releaseNonNull(), vertical.releaseNonNull());
    return CSSValuePair::createNoncoalescing(horizontal.releaseNonNull(), vertical.releaseNonNull());
}

RefPtr<CSSValue> consumeGridAutoFlow(CSSParserTokenRange& range)
{
    auto rowOrColumnValue = consumeIdent<CSSValueRow, CSSValueColumn>(range);
    auto denseAlgorithm = consumeIdent<CSSValueDense>(range);
    if (!rowOrColumnValue) {
        rowOrColumnValue = consumeIdent<CSSValueRow, CSSValueColumn>(range);
        if (!rowOrColumnValue && !denseAlgorithm)
            return nullptr;
    }
    CSSValueListBuilder parsedValues;
    if (rowOrColumnValue) {
        CSSValueID value = rowOrColumnValue->valueID();
        if (value == CSSValueColumn || (value == CSSValueRow && !denseAlgorithm))
            parsedValues.append(rowOrColumnValue.releaseNonNull());
    }
    if (denseAlgorithm)
        parsedValues.append(denseAlgorithm.releaseNonNull());
    return CSSValueList::createSpaceSeparated(WTFMove(parsedValues));
}

RefPtr<CSSValueList> consumeMasonryAutoFlow(CSSParserTokenRange& range)
{
    auto packOrNextValue = consumeIdent<CSSValuePack, CSSValueNext>(range);
    auto definiteFirstOrOrderedValue = consumeIdent<CSSValueDefiniteFirst, CSSValueOrdered>(range);

    if (!packOrNextValue) {
        packOrNextValue = consumeIdent<CSSValuePack, CSSValueNext>(range);
        if (!packOrNextValue)
            packOrNextValue = CSSPrimitiveValue::create(CSSValuePack);
    }

    CSSValueListBuilder parsedValues;
    if (packOrNextValue) {
        CSSValueID packOrNextValueID = packOrNextValue->valueID();
        if (!definiteFirstOrOrderedValue || (definiteFirstOrOrderedValue && definiteFirstOrOrderedValue->valueID() == CSSValueDefiniteFirst) || packOrNextValueID == CSSValueNext)
            parsedValues.append(packOrNextValue.releaseNonNull());
    }
    if (definiteFirstOrOrderedValue && definiteFirstOrOrderedValue->valueID() == CSSValueOrdered)
        parsedValues.append(definiteFirstOrOrderedValue.releaseNonNull());
    return CSSValueList::createSpaceSeparated(WTFMove(parsedValues));
}

RefPtr<CSSValue> consumeRepeatStyle(CSSParserTokenRange& range, const CSSParserContext&)
{
    // https://www.w3.org/TR/css-backgrounds-3/#typedef-repeat-style
    if (consumeIdentRaw<CSSValueRepeatX>(range))
        return CSSBackgroundRepeatValue::create(CSSValueRepeat, CSSValueNoRepeat);
    if (consumeIdentRaw<CSSValueRepeatY>(range))
        return CSSBackgroundRepeatValue::create(CSSValueNoRepeat, CSSValueRepeat);
    auto value1 = consumeIdentRaw<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound, CSSValueSpace>(range);
    if (!value1)
        return nullptr;
    auto value2 = consumeIdentRaw<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound, CSSValueSpace>(range);
    if (!value2)
        value2 = value1;
    return CSSBackgroundRepeatValue::create(*value1, *value2);
}

RefPtr<CSSValue> consumeSingleBackgroundClip(CSSParserTokenRange& range, const CSSParserContext& context)
{
    switch (auto keyword = range.peek().id(); keyword) {
    case CSSValueID::CSSValueBorderBox:
    case CSSValueID::CSSValuePaddingBox:
    case CSSValueID::CSSValueContentBox:
    case CSSValueID::CSSValueText:
    case CSSValueID::CSSValueWebkitText:
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(keyword);
    case CSSValueID::CSSValueBorderArea:
        if (!context.cssBackgroundClipBorderAreaEnabled)
            return nullptr;
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(keyword);

    default:
        return nullptr;
    }
}

RefPtr<CSSValue> consumeBackgroundClip(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto lambda = [&](CSSParserTokenRange& range) -> RefPtr<CSSValue> {
        return consumeSingleBackgroundClip(range, context);
    };
    return consumeCommaSeparatedListWithSingleValueOptimization(range, lambda);
}

RefPtr<CSSValue> consumeSingleBackgroundSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://www.w3.org/TR/css-backgrounds-3/#background-size
    return consumeBackgroundSize<CSSPropertyBackgroundSize>(range, context.mode);
}

RefPtr<CSSValue> consumeSingleMaskSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://www.w3.org/TR/css-masking-1/#the-mask-size
    return consumeBackgroundSize<CSSPropertyMaskSize>(range, context.mode);
}

RefPtr<CSSValue> consumeSingleWebkitBackgroundSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeBackgroundSize<CSSPropertyWebkitBackgroundSize>(range, context.mode);
}

bool isSelfPositionKeyword(CSSValueID id)
{
    return identMatches<CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueSelfStart, CSSValueSelfEnd, CSSValueFlexStart, CSSValueFlexEnd>(id);
}

bool isSelfPositionOrLeftOrRightKeyword(CSSValueID id)
{
    return isSelfPositionKeyword(id) || isLeftOrRightKeyword(id);
}

bool isGridBreadthIdent(CSSValueID id)
{
    return identMatches<CSSValueMinContent, CSSValueWebkitMinContent, CSSValueMaxContent, CSSValueWebkitMaxContent, CSSValueAuto>(id);
}

RefPtr<CSSValue> consumeSelfPositionOverflowPosition(CSSParserTokenRange& range, IsPositionKeyword isPositionKeyword)
{
    ASSERT(isPositionKeyword);
    CSSValueID id = range.peek().id();
    if (isAuto(id) || isNormalOrStretch(id))
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

RefPtr<CSSValue> consumeAlignItems(CSSParserTokenRange& range)
{
    // align-items property does not allow the 'auto' value.
    if (identMatches<CSSValueAuto>(range.peek().id()))
        return nullptr;
    return consumeSelfPositionOverflowPosition(range, isSelfPositionKeyword);
}

RefPtr<CSSValue> consumeJustifyItems(CSSParserTokenRange& range)
{
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

static RefPtr<CSSValue> consumeFitContent(CSSParserTokenRange& range, CSSParserMode mode)
{
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    auto length = consumeLengthOrPercent(args, mode, ValueRange::NonNegative, UnitlessQuirk::Allow);
    if (!length || !args.atEnd())
        return nullptr;
    range = rangeCopy;
    return CSSFunctionValue::create(CSSValueFitContent, length.releaseNonNull());
}

static RefPtr<CSSPrimitiveValue> consumeCustomIdentForGridLine(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto || range.peek().id() == CSSValueSpan)
        return nullptr;
    return consumeCustomIdent(range);
}

RefPtr<CSSValue> consumeGridLine(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    RefPtr<CSSPrimitiveValue> spanValue;
    RefPtr<CSSPrimitiveValue> gridLineName;
    RefPtr<CSSPrimitiveValue> numericValue = consumeInteger(range);
    if (numericValue) {
        gridLineName = consumeCustomIdentForGridLine(range);
        spanValue = consumeIdent<CSSValueSpan>(range);
    } else {
        spanValue = consumeIdent<CSSValueSpan>(range);
        if (spanValue) {
            numericValue = consumeInteger(range);
            gridLineName = consumeCustomIdentForGridLine(range);
            if (!numericValue)
                numericValue = consumeInteger(range);
        } else {
            gridLineName = consumeCustomIdentForGridLine(range);
            if (gridLineName) {
                numericValue = consumeInteger(range);
                spanValue = consumeIdent<CSSValueSpan>(range);
                if (!spanValue && !numericValue)
                    return gridLineName;
            } else
                return nullptr;
        }
    }

    if (spanValue && !numericValue && !gridLineName)
        return nullptr; // "span" keyword alone is invalid.
    if (spanValue && numericValue && numericValue->isNegative().value_or(false))
        return nullptr; // Negative numbers are not allowed for span.
    if (numericValue && numericValue->isZero().value_or(false))
        return nullptr; // An <integer> value of zero makes the declaration invalid.

    CSSValueListBuilder values;
    if (spanValue)
        values.append(spanValue.releaseNonNull());
    if (numericValue)
        values.append(numericValue.releaseNonNull());
    if (gridLineName)
        values.append(gridLineName.releaseNonNull());
    return CSSValueList::createSpaceSeparated(WTFMove(values));
}

static bool isGridTrackFixedSized(const CSSPrimitiveValue& primitiveValue)
{
    switch (primitiveValue.valueID()) {
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
    case CSSValueAuto:
        return false;
    default:
        return !primitiveValue.isFlex();
    }
}

static bool isGridTrackFixedSized(const CSSValue& value)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return isGridTrackFixedSized(*primitiveValue);
    auto& function = downcast<CSSFunctionValue>(value);
    if (function.name() == CSSValueFitContent || function.length() < 2)
        return false;
    return isGridTrackFixedSized(downcast<CSSPrimitiveValue>(*function.item(0)))
        || isGridTrackFixedSized(downcast<CSSPrimitiveValue>(*function.item(1)));
}

static Vector<String> parseGridTemplateAreasColumnNames(StringView gridRowNames)
{
    ASSERT(!gridRowNames.isEmpty());
    Vector<String> columnNames;
    StringBuilder areaName;
    for (auto character : gridRowNames.codeUnits()) {
        if (isCSSSpace(character)) {
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
            continue;
        }
        if (character == '.') {
            if (areaName == "."_s)
                continue;
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        } else {
            if (!isNameCodePoint(character))
                return Vector<String>();
            if (areaName == "."_s) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        }
        areaName.append(character);
    }
    if (!areaName.isEmpty())
        columnNames.append(areaName.toString());
    return columnNames;
}

bool parseGridTemplateAreasRow(StringView gridRowNames, NamedGridAreaMap& gridAreaMap, const size_t rowCount, size_t& columnCount)
{
    if (gridRowNames.containsOnly<isCSSSpace>())
        return false;

    auto columnNames = parseGridTemplateAreasColumnNames(gridRowNames);
    if (!rowCount) {
        columnCount = columnNames.size();
        if (!columnCount)
            return false;
    } else if (columnCount != columnNames.size()) {
        // The declaration is invalid if all the rows don't have the number of columns.
        return false;
    }

    for (size_t currentColumn = 0; currentColumn < columnCount; ++currentColumn) {
        const String& gridAreaName = columnNames[currentColumn];

        // Unamed areas are always valid (we consider them to be 1x1).
        if (gridAreaName == "."_s)
            continue;

        size_t lookAheadColumn = currentColumn + 1;
        while (lookAheadColumn < columnCount && columnNames[lookAheadColumn] == gridAreaName)
            lookAheadColumn++;

        auto result = gridAreaMap.map.ensure(gridAreaName, [&] {
            return GridArea(GridSpan::translatedDefiniteGridSpan(rowCount, rowCount + 1), GridSpan::translatedDefiniteGridSpan(currentColumn, lookAheadColumn));
        });
        if (!result.isNewEntry) {
            auto& gridArea = result.iterator->value;

            // The following checks test that the grid area is a single filled-in rectangle.
            // 1. The new row is adjacent to the previously parsed row.
            if (rowCount != gridArea.rows.endLine())
                return false;

            // 2. The new area starts at the same position as the previously parsed area.
            if (currentColumn != gridArea.columns.startLine())
                return false;

            // 3. The new area ends at the same position as the previously parsed area.
            if (lookAheadColumn != gridArea.columns.endLine())
                return false;

            gridArea.rows = GridSpan::translatedDefiniteGridSpan(gridArea.rows.startLine(), gridArea.rows.endLine() + 1);
        }
        currentColumn = lookAheadColumn - 1;
    }

    return true;
}

static RefPtr<CSSPrimitiveValue> consumeGridBreadth(CSSParserTokenRange& range, CSSParserMode mode)
{
    const CSSParserToken& token = range.peek();
    if (isGridBreadthIdent(token.id()))
        return consumeIdent(range);
    if (token.type() == DimensionToken && token.unitType() == CSSUnitType::CSS_FR) {
        if (range.peek().numericValue() < 0)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_FR);
    }
    return consumeLengthOrPercent(range, mode, ValueRange::NonNegative);
}

RefPtr<CSSValue> consumeGridTrackSize(CSSParserTokenRange& range, CSSParserMode mode)
{
    const CSSParserToken& token = range.peek();
    if (identMatches<CSSValueAuto>(token.id()))
        return consumeIdent(range);

    if (token.functionId() == CSSValueMinmax) {
        CSSParserTokenRange rangeCopy = range;
        CSSParserTokenRange args = consumeFunction(rangeCopy);
        auto minTrackBreadth = consumeGridBreadth(args, mode);
        if (!minTrackBreadth || minTrackBreadth->isFlex() || !consumeCommaIncludingWhitespace(args))
            return nullptr;
        auto maxTrackBreadth = consumeGridBreadth(args, mode);
        if (!maxTrackBreadth || !args.atEnd())
            return nullptr;
        range = rangeCopy;
        return CSSFunctionValue::create(CSSValueMinmax, minTrackBreadth.releaseNonNull(), maxTrackBreadth.releaseNonNull());
    }

    if (token.functionId() == CSSValueFitContent)
        return consumeFitContent(range, mode);

    return consumeGridBreadth(range, mode);
}

// Appends to the passed in CSSGridLineNamesValue if any, otherwise creates a new one. Returns nullptr if an empty list is consumed.
RefPtr<CSSGridLineNamesValue> consumeGridLineNames(CSSParserTokenRange& range, AllowEmpty allowEmpty)
{
    CSSParserTokenRange rangeCopy = range;
    if (rangeCopy.consumeIncludingWhitespace().type() != LeftBracketToken)
        return nullptr;

    Vector<String, 4> lineNames;
    while (auto lineName = consumeCustomIdentForGridLine(rangeCopy))
        lineNames.append(lineName->customIdent());
    if (rangeCopy.consumeIncludingWhitespace().type() != RightBracketToken)
        return nullptr;
    range = rangeCopy;
    if (allowEmpty == AllowEmpty::No && lineNames.isEmpty())
        return nullptr;
    return CSSGridLineNamesValue::create(lineNames);
}

static bool consumeGridTrackRepeatFunction(CSSParserTokenRange& range, CSSParserMode mode, CSSValueListBuilder& list, bool& isAutoRepeat, bool& allTracksAreFixedSized)
{
    CSSParserTokenRange args = consumeFunction(range);

    CSSValueListBuilder repeatedValues;

    // The number of repetitions for <auto-repeat> is not important at parsing level
    // because it will be computed later, let's set it to 1.
    size_t repetitions = 1;
    auto autoRepeatType = consumeIdentRaw<CSSValueAutoFill, CSSValueAutoFit>(args);
    isAutoRepeat = autoRepeatType.has_value();
    if (!isAutoRepeat) {
        auto repetition = consumePositiveIntegerRaw(args);
        if (!repetition)
            return false;
        repetitions = clampTo<size_t>(static_cast<size_t>(*repetition), 0, GridPosition::max());
    }
    if (!consumeCommaIncludingWhitespace(args))
        return false;
    if (auto lineNames = consumeGridLineNames(args))
        repeatedValues.append(lineNames.releaseNonNull());

    size_t numberOfTracks = 0;
    while (!args.atEnd()) {
        auto trackSize = consumeGridTrackSize(args, mode);
        if (!trackSize)
            return false;
        if (allTracksAreFixedSized)
            allTracksAreFixedSized = isGridTrackFixedSized(*trackSize);
        repeatedValues.append(trackSize.releaseNonNull());
        ++numberOfTracks;
        if (auto lineNames = consumeGridLineNames(args))
            repeatedValues.append(lineNames.releaseNonNull());
    }
    // We should have found at least one <track-size> or else it is not a valid <track-list>.
    if (!numberOfTracks)
        return false;

    if (isAutoRepeat)
        list.append(CSSGridAutoRepeatValue::create(*autoRepeatType, WTFMove(repeatedValues)));
    else {
        // We clamp the repetitions to a multiple of the repeat() track list's size, while staying below the max grid size.
        repetitions = std::min(repetitions, GridPosition::max() / numberOfTracks);
        list.append(CSSGridIntegerRepeatValue::create(repetitions, WTFMove(repeatedValues)));
    }
    return true;
}

static bool consumeSubgridNameRepeatFunction(CSSParserTokenRange& range, CSSValueListBuilder& list, bool& isAutoRepeat)
{
    CSSParserTokenRange args = consumeFunction(range);
    size_t repetitions = 1;
    isAutoRepeat = consumeIdentRaw<CSSValueAutoFill>(args).has_value();
    if (!isAutoRepeat) {
        auto repetition = consumePositiveIntegerRaw(args);
        if (!repetition)
            return false;
        repetitions = clampTo<size_t>(static_cast<size_t>(*repetition), 0, GridPosition::max());
    }
    if (!consumeCommaIncludingWhitespace(args))
        return false;

    CSSValueListBuilder repeatedValues;
    do {
        auto lineNames = consumeGridLineNames(args, AllowEmpty::Yes);
        if (!lineNames)
            return false;
        repeatedValues.append(lineNames.releaseNonNull());
    } while (!args.atEnd());

    if (isAutoRepeat)
        list.append(CSSGridAutoRepeatValue::create(CSSValueAutoFill, WTFMove(repeatedValues)));
    else
        list.append(CSSGridIntegerRepeatValue::create(repetitions, WTFMove(repeatedValues)));
    return true;
}

RefPtr<CSSValue> consumeGridTrackList(CSSParserTokenRange& range, const CSSParserContext& context, TrackListType trackListType)
{
    if (context.masonryEnabled && range.peek().id() == CSSValueMasonry)
        return consumeIdent(range);
    bool seenAutoRepeat = false;
    if (trackListType == GridTemplate && range.peek().id() == CSSValueSubgrid) {
        consumeIdent(range);
        CSSValueListBuilder values;
        while (!range.atEnd() && range.peek().type() != DelimiterToken) {
            if (range.peek().functionId() == CSSValueRepeat) {
                bool isAutoRepeat;
                if (!consumeSubgridNameRepeatFunction(range, values, isAutoRepeat))
                    return nullptr;
                if (isAutoRepeat && seenAutoRepeat)
                    return nullptr;
                seenAutoRepeat = seenAutoRepeat || isAutoRepeat;
            } else if (auto value = consumeGridLineNames(range, AllowEmpty::Yes))
                values.append(value.releaseNonNull());
            else
                return nullptr;
        }
        return CSSSubgridValue::create(WTFMove(values));
    }

    bool allowGridLineNames = trackListType != GridAuto;
    if (!allowGridLineNames && range.peek().type() == LeftBracketToken)
        return nullptr;

    CSSValueListBuilder values;
    bool allowRepeat = trackListType == GridTemplate;
    bool allTracksAreFixedSized = true;
    if (auto lineNames = consumeGridLineNames(range))
        values.append(lineNames.releaseNonNull());
    do {
        bool isAutoRepeat;
        if (range.peek().functionId() == CSSValueRepeat) {
            if (!allowRepeat)
                return nullptr;
            if (!consumeGridTrackRepeatFunction(range, context.mode, values, isAutoRepeat, allTracksAreFixedSized))
                return nullptr;
            if (isAutoRepeat && seenAutoRepeat)
                return nullptr;
            seenAutoRepeat = seenAutoRepeat || isAutoRepeat;
        } else if (RefPtr<CSSValue> value = consumeGridTrackSize(range, context.mode)) {
            if (allTracksAreFixedSized)
                allTracksAreFixedSized = isGridTrackFixedSized(*value);
            values.append(value.releaseNonNull());
        } else
            return nullptr;
        if (seenAutoRepeat && !allTracksAreFixedSized)
            return nullptr;
        if (!allowGridLineNames && range.peek().type() == LeftBracketToken)
            return nullptr;
        if (auto lineNames = consumeGridLineNames(range))
            values.append(lineNames.releaseNonNull());
    } while (!range.atEnd() && range.peek().type() != DelimiterToken);
    return CSSValueList::createSpaceSeparated(WTFMove(values));
}

RefPtr<CSSValue> consumeGridTemplatesRowsOrColumns(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    if (context.masonryEnabled && range.peek().id() == CSSValueMasonry)
        return consumeIdent(range);
    return consumeGridTrackList(range, context, GridTemplate);
}

RefPtr<CSSValue> consumeGridTemplateAreas(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    NamedGridAreaMap map;
    size_t rowCount = 0;
    size_t columnCount = 0;
    while (range.peek().type() == StringToken) {
        if (!parseGridTemplateAreasRow(range.consumeIncludingWhitespace().value(), map, rowCount, columnCount))
            return nullptr;
        ++rowCount;
    }
    if (!rowCount)
        return nullptr;
    return CSSGridTemplateAreasValue::create(WTFMove(map), rowCount, columnCount);
}

RefPtr<CSSValue> consumeLineBoxContain(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeContainerName(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    CSSValueListBuilder list;
    do {
        auto name = consumeSingleContainerName(range);
        if (!name)
            break;
        list.append(name.releaseNonNull());
    } while (!range.atEnd());
    if (list.isEmpty())
        return nullptr;
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeWebkitInitialLetter(CSSParserTokenRange& range)
{
    if (auto ident = consumeIdent<CSSValueNormal>(range))
        return ident;
    auto height = consumeNumber(range, ValueRange::NonNegative);
    if (!height)
        return nullptr;
    RefPtr<CSSPrimitiveValue> position;
    if (!range.atEnd()) {
        position = consumeNumber(range, ValueRange::NonNegative);
        if (!position || !range.atEnd())
            return nullptr;
    } else
        position = height.copyRef();
    return CSSValuePair::create(position.releaseNonNull(), height.releaseNonNull());
}

RefPtr<CSSValue> consumeSpeakAs(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeHangingPunctuation(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeContain(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeContainIntrinsicSize(CSSParserTokenRange& range)
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

    if (auto lengthValue = consumeLength(range, HTMLStandardMode, ValueRange::NonNegative)) {
        if (autoValue)
            return CSSValuePair::create(autoValue.releaseNonNull(), lengthValue.releaseNonNull());
        return lengthValue;
    }
    return nullptr;
}

RefPtr<CSSValue> consumeTextEmphasisPosition(CSSParserTokenRange& range)
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

#if ENABLE(DARK_MODE_CSS)

RefPtr<CSSValue> consumeColorScheme(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNormal)
        return consumeIdent(range);

    Vector<CSSValueID, 3> identifiers;

    while (!range.atEnd()) {
        if (range.peek().type() != IdentToken)
            return nullptr;

        CSSValueID id = range.peek().id();

        switch (id) {
        case CSSValueNormal:
            // `normal` is only allowed as a single value, and was handled earlier.
            // Don't allow it in the list.
            return nullptr;

        // FIXME: Specification seems to indicate that we can only have "only" once.
        // FIXME: Specification seems to indicate "only" by itself without some other keyword is not valid.
        // FIXME: Specification seems to indicate that "only" should be put at the end of the list rather than where it appeared in parsing sequence.
        case CSSValueOnly:
        case CSSValueLight:
        case CSSValueDark:
            if (!identifiers.appendIfNotContains(id))
                return nullptr;
            break;

        default:
            // Unknown identifiers are allowed and ignored.
            break;
        }

        range.consumeIncludingWhitespace();
    }

    CSSValueListBuilder list;
    for (auto id : identifiers)
        list.append(CSSPrimitiveValue::create(id));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

#endif

RefPtr<CSSValue> consumeOffsetRotate(CSSParserTokenRange& range, CSSParserMode mode)
{
    auto rangeCopy = range;

    // Attempt to parse the first token as the modifier (auto / reverse keyword). If
    // successful, parse the second token as the angle. If not, try to parse the other
    // way around.
    auto modifier = consumeIdent<CSSValueAuto, CSSValueReverse>(rangeCopy);
    auto angle = consumeAngle(rangeCopy, mode);
    if (!modifier)
        modifier = consumeIdent<CSSValueAuto, CSSValueReverse>(rangeCopy);
    if (!angle && !modifier)
        return nullptr;

    range = rangeCopy;
    return CSSOffsetRotateValue::create(WTFMove(modifier), WTFMove(angle));
}

RefPtr<CSSValue> consumeViewTransitionClass(CSSParserTokenRange& range)
{
    if (auto noneValue = consumeIdent<CSSValueNone>(range))
        return noneValue;

    CSSValueListBuilder list;
    do {
        if (range.peek().id() == CSSValueNone)
            return nullptr;

        auto ident = consumeCustomIdent(range);
        if (!ident)
            return nullptr;

        list.append(ident.releaseNonNull());
    } while (!range.atEnd());

    if (list.isEmpty())
        return nullptr;

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> consumeViewTransitionName(CSSParserTokenRange& range)
{
    if (auto noneValue = consumeIdent<CSSValueNone>(range))
        return noneValue;
    if (isAuto(range.peek().id()))
        return nullptr;
    return consumeCustomIdent(range);
}

// MARK: - @-rule descriptor consumers:

// MARK: @counter-style

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-system
RefPtr<CSSValue> consumeCounterStyleSystem(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // cyclic | numeric | alphabetic | symbolic | additive | [fixed <integer>?] | [ extends <counter-style-name> ]

    if (auto ident = consumeIdent<CSSValueCyclic, CSSValueNumeric, CSSValueAlphabetic, CSSValueSymbolic, CSSValueAdditive>(range))
        return ident;

    if (isUASheetBehavior(context.mode)) {
        auto internalKeyword = consumeIdent<
            CSSValueInternalDisclosureClosed,
            CSSValueInternalDisclosureOpen,
            CSSValueInternalSimplifiedChineseInformal,
            CSSValueInternalSimplifiedChineseFormal,
            CSSValueInternalTraditionalChineseInformal,
            CSSValueInternalTraditionalChineseFormal,
            CSSValueInternalEthiopicNumeric
        >(range);
        if (internalKeyword)
            return internalKeyword;
    }

    if (auto ident = consumeIdent<CSSValueFixed>(range)) {
        if (range.atEnd())
            return ident;
        // If we have the `fixed` keyword but the range is not at the end, the next token must be a integer.
        // If it's not, this value is invalid.
        auto firstSymbolValue = consumeInteger(range);
        if (!firstSymbolValue)
            return nullptr;
        return CSSValuePair::create(ident.releaseNonNull(), firstSymbolValue.releaseNonNull());
    }

    if (auto ident = consumeIdent<CSSValueExtends>(range)) {
        // There must be a `<counter-style-name>` following the `extends` keyword. If there isn't, this value is invalid.
        auto parsedCounterStyleName = consumeCounterStyleName(range);
        if (!parsedCounterStyleName)
            return nullptr;
        return CSSValuePair::create(ident.releaseNonNull(), parsedCounterStyleName.releaseNonNull());
    }
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#typedef-symbol
RefPtr<CSSValue> consumeCounterStyleSymbol(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // [ <string> | <image> | <custom-ident> ]

    if (auto string = consumeString(range))
        return string;
    if (auto customIdent = consumeCustomIdent(range))
        return customIdent;
    // There are inherent difficulties in supporting <image> symbols in @counter-styles, so gate them behind a
    // flag for now. https://bugs.webkit.org/show_bug.cgi?id=167645
    if (context.counterStyleAtRuleImageSymbolsEnabled) {
        if (auto image = consumeImage(range, context, { AllowedImageType::URLFunction, AllowedImageType::GeneratedImage }))
            return image;
    }
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-negative
RefPtr<CSSValue> consumeCounterStyleNegative(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <symbol> <symbol>?

    auto prependValue = consumeCounterStyleSymbol(range, context);
    if (!prependValue || range.atEnd())
        return prependValue;

    auto appendValue = consumeCounterStyleSymbol(range, context);
    if (!appendValue || !range.atEnd())
        return nullptr;

    return CSSValueList::createSpaceSeparated(prependValue.releaseNonNull(), appendValue.releaseNonNull());
}

static RefPtr<CSSPrimitiveValue> consumeCounterStyleRangeBound(CSSParserTokenRange& range)
{
    if (auto infinite = consumeIdent<CSSValueInfinite>(range))
        return infinite;
    if (auto integer = consumeInteger(range))
        return integer;
    return nullptr;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-range
RefPtr<CSSValue> consumeCounterStyleRange(CSSParserTokenRange& range)
{
    // [ [ <integer> | infinite ]{2} ]# | auto

    if (auto autoValue = consumeIdent<CSSValueAuto>(range))
        return autoValue;

    auto rangeList = consumeCommaSeparatedListWithoutSingleValueOptimization(range, [](auto& range) -> RefPtr<CSSValue> {
        auto lowerBound = consumeCounterStyleRangeBound(range);
        if (!lowerBound)
            return nullptr;
        auto upperBound = consumeCounterStyleRangeBound(range);
        if (!upperBound)
            return nullptr;

        // If the lower bound of any range is higher than the upper bound, the entire descriptor is invalid and must be
        // ignored.
        if (lowerBound->isInteger() && upperBound->isInteger() && lowerBound->resolveAsIntegerDeprecated() > upperBound->resolveAsIntegerDeprecated())
            return nullptr;

        return CSSValuePair::createNoncoalescing(lowerBound.releaseNonNull(), upperBound.releaseNonNull());
    });

    if (!range.atEnd() || !rangeList || !rangeList->length())
        return nullptr;
    return rangeList;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-pad
RefPtr<CSSValue> consumeCounterStylePad(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <integer [0,∞]> && <symbol>
    RefPtr<CSSPrimitiveValue> integer;
    RefPtr<CSSValue> symbol;
    while (!integer || !symbol) {
        if (!integer) {
            integer = consumeNonNegativeInteger(range);
            if (integer)
                continue;
        }
        if (!symbol) {
            symbol = consumeCounterStyleSymbol(range, context);
            if (symbol)
                continue;
        }
        return nullptr;
    }
    if (!range.atEnd())
        return nullptr;
    return CSSValueList::createSpaceSeparated(integer.releaseNonNull(), symbol.releaseNonNull());
}

// https://www.w3.org/TR/css-counter-styles-3/#descdef-counter-style-symbols
RefPtr<CSSValue> consumeCounterStyleSymbols(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <symbol>+
    CSSValueListBuilder symbols;
    while (!range.atEnd()) {
        auto symbol = consumeCounterStyleSymbol(range, context);
        if (!symbol)
            return nullptr;
        symbols.append(symbol.releaseNonNull());
    }
    if (symbols.isEmpty())
        return nullptr;
    return CSSValueList::createSpaceSeparated(WTFMove(symbols));
}

// https://www.w3.org/TR/css-counter-styles-3/#descdef-counter-style-additive-symbols
RefPtr<CSSValue> consumeCounterStyleAdditiveSymbols(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // [ <integer [0,∞]> && <symbol> ]#

    std::optional<int> lastWeight;
    auto values = consumeCommaSeparatedListWithoutSingleValueOptimization(range, [&lastWeight](auto& range, auto& context) -> RefPtr<CSSValue> {
        auto integer = consumeNonNegativeInteger(range);
        auto symbol = consumeCounterStyleSymbol(range, context);
        if (!integer) {
            if (!symbol)
                return nullptr;
            integer = consumeNonNegativeInteger(range);
            if (!integer)
                return nullptr;
        }
        if (!symbol)
            return nullptr;

        // Additive tuples must be specified in order of strictly descending weight.
        auto weight = integer->resolveAsIntegerDeprecated();
        if (lastWeight && !(weight < lastWeight))
            return nullptr;
        lastWeight = weight;

        return CSSValuePair::create(integer.releaseNonNull(), symbol.releaseNonNull());
    }, context);

    if (!range.atEnd() || !values || !values->length())
        return nullptr;
    return values;
}

// https://www.w3.org/TR/css-counter-styles-3/#counter-style-speak-as
RefPtr<CSSValue> consumeCounterStyleSpeakAs(CSSParserTokenRange& range)
{
    // auto | bullets | numbers | words | spell-out | <counter-style-name>
    if (auto speakAsIdent = consumeIdent<CSSValueAuto, CSSValueBullets, CSSValueNumbers, CSSValueWords, CSSValueSpellOut>(range))
        return speakAsIdent;
    return consumeCounterStyleName(range);
}

RefPtr<CSSValue> consumeDeclarationValue(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return CSSVariableParser::parseDeclarationValue(nullAtom(), range.consumeAll(), context);
}

RefPtr<CSSValue> consumeTextSpacingTrim(CSSParserTokenRange& range)
{
    // auto | space-all |  trim-all | [ allow-end || space-first ]
    // FIXME: add remaining values;
    if (auto value = consumeIdent<CSSValueAuto, CSSValueSpaceAll>(range)) {
        if (!range.atEnd())
            return nullptr;
        return value;
    }
    return nullptr;
}

RefPtr<CSSValue> consumeTextAutospace(CSSParserTokenRange& range)
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

RefPtr<CSSValue> consumeAnimationTimeline(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeCommaSeparatedListWithSingleValueOptimization(range, [context](CSSParserTokenRange& range) -> RefPtr<CSSValue> {
        return consumeSingleAnimationTimeline(range, context);
    });
}

RefPtr<CSSValue> consumeSingleAnimationTimeline(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <single-animation-timeline> = auto | none | <dashed-ident> | <scroll()> | <view()>
    auto id = range.peek().id();
    if (id == CSSValueAuto || id == CSSValueNone)
        return consumeIdent(range);
    if (auto name = consumeDashedIdent(range))
        return name;
    if (auto scroll = consumeAnimationTimelineScroll(range))
        return scroll;
    return consumeAnimationTimelineView(range, context);
}

RefPtr<CSSValue> consumeAnimationTimelineScroll(CSSParserTokenRange& range)
{
    // https://drafts.csswg.org/scroll-animations-1/#scroll-notation
    // <scroll()> = scroll( [ <scroller> || <axis> ]? )
    // <scroller> = root | nearest | self
    // <axis> = block | inline | x | y

    if (range.peek().type() != FunctionToken || range.peek().functionId() != CSSValueScroll)
        return nullptr;

    auto args = consumeFunction(range);

    if (!args.size())
        return CSSScrollValue::create(nullptr, nullptr);

    auto scroller = CSSPropertyParsing::consumeScroller(args);
    auto axis = CSSPropertyParsing::consumeAxis(args);

    // Try <scroller> again since the order of <scroller> and <axis> is not guaranteed.
    if (!scroller)
        scroller = CSSPropertyParsing::consumeScroller(args);

    // If there are values left to consume, these are not valid <scroller> or <axis> and the function is invalid.
    if (args.size())
        return nullptr;

    return CSSScrollValue::create(WTFMove(scroller), WTFMove(axis));
}

RefPtr<CSSValue> consumeAnimationTimelineView(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // https://drafts.csswg.org/scroll-animations-1/#view-notation
    // <view()> = view( [ <axis> || <'view-timeline-inset'> ]? )
    // <axis> = block | inline | x | y
    // <'view-timeline-inset'> = [ [ auto | <length-percentage> ]{1,2} ]#

    if (range.peek().type() != FunctionToken || range.peek().functionId() != CSSValueView)
        return nullptr;

    auto args = consumeFunction(range);

    if (!args.size())
        return CSSViewValue::create();

    auto axis = CSSPropertyParsing::consumeAxis(args);

    auto startInset = CSSPropertyParsing::consumeSingleViewTimelineInset(args, context);
    auto endInset = CSSPropertyParsing::consumeSingleViewTimelineInset(args, context);

    // Try <axis> again since the order of <axis> and <'view-timeline-inset'> is not guaranteed.
    if (!axis)
        axis = CSSPropertyParsing::consumeAxis(args);

    // If there are values left to consume, these are not valid <axis> or <'view-timeline-inset'> and the function is invalid.
    if (args.size())
        return nullptr;

    return CSSViewValue::create(WTFMove(axis), WTFMove(startInset), WTFMove(endInset));
}

RefPtr<CSSValue> consumeViewTimelineInsetListItem(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto startInset = CSSPropertyParsing::consumeSingleViewTimelineInset(range, context);
    if (!startInset)
        return nullptr;

    if (auto endInset = CSSPropertyParsing::consumeSingleViewTimelineInset(range, context)) {
        if (endInset != startInset)
            return CSSValuePair::createNoncoalescing(startInset.releaseNonNull(), endInset.releaseNonNull());
    }

    return startInset;
}

RefPtr<CSSValue> consumeViewTimelineInset(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, [context](CSSParserTokenRange& range) {
        return consumeViewTimelineInsetListItem(range, context);
    });
}

RefPtr<CSSPrimitiveValue> consumeAnchor(CSSParserTokenRange& range, CSSParserMode mode)
{
    // https://drafts.csswg.org/css-anchor-position-1/#anchor-pos
    // <anchor()> = anchor( <anchor-element>? && <anchor-side>, <length-percentage>? )
    if (range.peek().type() != FunctionToken || range.peek().functionId() != CSSValueAnchor)
        return nullptr;

    auto rangeCopy = range;
    auto args = consumeFunction(rangeCopy);
    if (!args.size())
        return nullptr;

    auto anchorElement = consumeDashedIdent(args);
    auto anchorSidePtr = CSSPropertyParsing::consumeAnchorSide(args);
    if (!anchorSidePtr)
        return nullptr;
    auto anchorSide = anchorSidePtr.releaseNonNull();

    if (!anchorElement)
        anchorElement = consumeDashedIdent(args);

    if (!args.size()) {
        range = rangeCopy;
        auto anchor = CSSAnchorValue::create(WTFMove(anchorElement), WTFMove(anchorSide), nullptr);
        return CSSPrimitiveValue::create(anchor);
    }

    if (!consumeCommaIncludingWhitespace(args))
        return nullptr;

    auto fallback = consumeLengthOrPercent(args, mode, ValueRange::All, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid, NegativePercentagePolicy::Allow, AnchorPolicy::Allow);
    if (!fallback || args.size())
        return nullptr;

    range = rangeCopy;
    auto anchor = CSSAnchorValue::create(WTFMove(anchorElement), WTFMove(anchorSide), WTFMove(fallback));
    return CSSPrimitiveValue::create(anchor);
}

RefPtr<CSSValue> consumeViewTransitionTypes(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    CSSValueListBuilder list;
    do {
        if (range.peek().id() == CSSValueNone)
            return nullptr;
        auto type = consumeCustomIdent(range);
        if (!type)
            return nullptr;
        if (type->customIdent().startsWith("-ua-"_s))
            return nullptr;
        list.append(type.releaseNonNull());
    } while (!range.atEnd());
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
