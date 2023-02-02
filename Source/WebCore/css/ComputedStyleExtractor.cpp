/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"
#include "ComputedStyleExtractor.h"

#include "BasicShapeFunctions.h"
#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontValue.h"
#include "CSSFontVariantAlternatesValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSProperty.h"
#include "CSSPropertyAnimation.h"
#include "CSSRayValue.h"
#include "CSSReflectValue.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSShadowValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSTransformListValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "ComposedTreeAncestorIterator.h"
#include "ContentData.h"
#include "CursorList.h"
#include "CustomPropertyRegistry.h"
#include "FontSelectionValueInlines.h"
#include "GridPositionsResolver.h"
#include "NodeRenderStyle.h"
#include "Pair.h"
#include "PerspectiveTransformOperation.h"
#include "QuotesData.h"
#include "Rect.h"
#include "RenderBlock.h"
#include "RenderBox.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RotateTransformOperation.h"
#include "SVGElement.h"
#include "ScaleTransformOperation.h"
#include "SkewTransformOperation.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "Styleable.h"
#include "TranslateTransformOperation.h"
#include "WebAnimationUtilities.h"

namespace WebCore {

class OrderedNamedLinesCollector {
    WTF_MAKE_NONCOPYABLE(OrderedNamedLinesCollector);
public:
    OrderedNamedLinesCollector(const RenderStyle& style, bool isRowAxis)
        : m_orderedNamedGridLines(isRowAxis ? style.orderedNamedGridColumnLines() : style.orderedNamedGridRowLines())
        , m_orderedNamedAutoRepeatGridLines(isRowAxis ? style.autoRepeatOrderedNamedGridColumnLines() : style.autoRepeatOrderedNamedGridRowLines())
    {
    }
    virtual ~OrderedNamedLinesCollector() = default;

    bool isEmpty() const { return m_orderedNamedGridLines.isEmpty() && m_orderedNamedAutoRepeatGridLines.isEmpty(); }
    virtual void collectLineNamesForIndex(CSSGridLineNamesValue&, unsigned index) const = 0;

    virtual int namedGridLineCount() const { return m_orderedNamedGridLines.size(); }

protected:

    enum NamedLinesType { NamedLines, AutoRepeatNamedLines };
    void appendLines(CSSGridLineNamesValue&, unsigned index, NamedLinesType) const;

    const OrderedNamedGridLinesMap& m_orderedNamedGridLines;
    const OrderedNamedGridLinesMap& m_orderedNamedAutoRepeatGridLines;
};

class OrderedNamedLinesCollectorInGridLayout : public OrderedNamedLinesCollector {
public:
    OrderedNamedLinesCollectorInGridLayout(const RenderStyle& style, bool isRowAxis, unsigned autoRepeatTracksCount, unsigned autoRepeatTrackListLength)
        : OrderedNamedLinesCollector(style, isRowAxis)
        , m_insertionPoint(isRowAxis ? style.gridAutoRepeatColumnsInsertionPoint() : style.gridAutoRepeatRowsInsertionPoint())
        , m_autoRepeatTotalTracks(autoRepeatTracksCount)
        , m_autoRepeatTrackListLength(autoRepeatTrackListLength)
    {
    }

    void collectLineNamesForIndex(CSSGridLineNamesValue&, unsigned index) const override;

private:
    unsigned m_insertionPoint;
    unsigned m_autoRepeatTotalTracks;
    unsigned m_autoRepeatTrackListLength;
};

class OrderedNamedLinesCollectorInSubgridLayout : public OrderedNamedLinesCollector {
public:
    OrderedNamedLinesCollectorInSubgridLayout(const RenderStyle& style, bool isRowAxis, unsigned totalTracksCount)
        : OrderedNamedLinesCollector(style, isRowAxis)
        , m_insertionPoint(isRowAxis ? style.gridAutoRepeatColumnsInsertionPoint() : style.gridAutoRepeatRowsInsertionPoint())
        , m_autoRepeatLineSetListLength((isRowAxis ? style.autoRepeatOrderedNamedGridColumnLines() : style.autoRepeatOrderedNamedGridRowLines()).size())
        , m_totalLines(totalTracksCount + 1)
    {
        if (!m_autoRepeatLineSetListLength) {
            m_autoRepeatTotalLineSets = 0;
            return;
        }
        unsigned named = (isRowAxis ? style.orderedNamedGridColumnLines() : style.orderedNamedGridRowLines()).size();
        if (named >= m_totalLines) {
            m_autoRepeatTotalLineSets = 0;
            return;
        }
        m_autoRepeatTotalLineSets = (m_totalLines - named) / m_autoRepeatLineSetListLength;
        m_autoRepeatTotalLineSets *= m_autoRepeatLineSetListLength;
    }

    void collectLineNamesForIndex(CSSGridLineNamesValue&, unsigned index) const override;

    int namedGridLineCount() const override { return m_totalLines; }
private:
    unsigned m_insertionPoint;
    unsigned m_autoRepeatTotalLineSets;
    unsigned m_autoRepeatLineSetListLength;
    unsigned m_totalLines;
};

void OrderedNamedLinesCollector::appendLines(CSSGridLineNamesValue& lineNamesValue, unsigned index, NamedLinesType type) const
{
    auto iter = type == NamedLines ? m_orderedNamedGridLines.find(index) : m_orderedNamedAutoRepeatGridLines.find(index);
    auto endIter = type == NamedLines ? m_orderedNamedGridLines.end() : m_orderedNamedAutoRepeatGridLines.end();
    if (iter == endIter)
        return;

    for (const auto& lineName : iter->value)
        lineNamesValue.append(CSSPrimitiveValue::createCustomIdent(lineName));
}

void OrderedNamedLinesCollectorInGridLayout::collectLineNamesForIndex(CSSGridLineNamesValue& lineNamesValue, unsigned i) const
{
    ASSERT(!isEmpty());
    if (!m_autoRepeatTrackListLength || i < m_insertionPoint) {
        appendLines(lineNamesValue, i, NamedLines);
        return;
    }

    ASSERT(m_autoRepeatTotalTracks);

    if (i > m_insertionPoint + m_autoRepeatTotalTracks) {
        appendLines(lineNamesValue, i - (m_autoRepeatTotalTracks - 1), NamedLines);
        return;
    }

    if (i == m_insertionPoint) {
        appendLines(lineNamesValue, i, NamedLines);
        appendLines(lineNamesValue, 0, AutoRepeatNamedLines);
        return;
    }

    if (i == m_insertionPoint + m_autoRepeatTotalTracks) {
        appendLines(lineNamesValue, m_autoRepeatTrackListLength, AutoRepeatNamedLines);
        appendLines(lineNamesValue, m_insertionPoint + 1, NamedLines);
        return;
    }

    unsigned autoRepeatIndexInFirstRepetition = (i - m_insertionPoint) % m_autoRepeatTrackListLength;
    if (!autoRepeatIndexInFirstRepetition && i > m_insertionPoint)
        appendLines(lineNamesValue, m_autoRepeatTrackListLength, AutoRepeatNamedLines);
    appendLines(lineNamesValue, autoRepeatIndexInFirstRepetition, AutoRepeatNamedLines);
}

void OrderedNamedLinesCollectorInSubgridLayout::collectLineNamesForIndex(CSSGridLineNamesValue& lineNamesValue, unsigned i) const
{
    if (!m_autoRepeatLineSetListLength || i < m_insertionPoint) {
        appendLines(lineNamesValue, i, NamedLines);
        return;
    }

    if (i >= m_insertionPoint + m_autoRepeatTotalLineSets) {
        appendLines(lineNamesValue, i - m_autoRepeatTotalLineSets, NamedLines);
        return;
    }

    unsigned autoRepeatIndexInFirstRepetition = (i - m_insertionPoint) % m_autoRepeatLineSetListLength;
    appendLines(lineNamesValue, autoRepeatIndexInFirstRepetition, AutoRepeatNamedLines);
}

static CSSValueID valueForRepeatRule(NinePieceImageRule rule)
{
    switch (rule) {
    case NinePieceImageRule::Repeat:
        return CSSValueRepeat;
    case NinePieceImageRule::Round:
        return CSSValueRound;
    case NinePieceImageRule::Space:
        return CSSValueSpace;
    default:
        return CSSValueStretch;
    }
}

static Ref<CSSPrimitiveValue> valueForImageSliceSide(const Length& length)
{
    // These values can be percentages, numbers, or while an animation of mixed types is in progress,
    // a calculation that combines a percentage and a number.
    if (length.isPercent())
        return CSSPrimitiveValue::create(length.percent(), CSSUnitType::CSS_PERCENTAGE);
    if (length.isAuto() || length.isFixed())
        return CSSPrimitiveValue::create(length.value(), CSSUnitType::CSS_NUMBER);

    // Calculating the actual length currently in use would require most of the code from RenderBoxModelObject::paintNinePieceImage.
    // And even if we could do that, it's not clear if that's exactly what we'd want during animation.
    // FIXME: For now, just return 0.
    ASSERT(length.isCalculated());
    return CSSPrimitiveValue::create(0, CSSUnitType::CSS_NUMBER);
}

static inline Ref<CSSBorderImageSliceValue> valueForNinePieceImageSlice(const NinePieceImage& image)
{
    auto& slices = image.imageSlices();

    RefPtr<CSSPrimitiveValue> top = valueForImageSliceSide(slices.top());

    RefPtr<CSSPrimitiveValue> right;
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;

    if (slices.right() == slices.top() && slices.bottom() == slices.top() && slices.left() == slices.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        right = valueForImageSliceSide(slices.right());

        if (slices.bottom() == slices.top() && slices.right() == slices.left()) {
            bottom = top;
            left = right;
        } else {
            bottom = valueForImageSliceSide(slices.bottom());

            if (slices.left() == slices.right())
                left = right;
            else
                left = valueForImageSliceSide(slices.left());
        }
    }

    auto quad = Quad::create();
    quad->setTop(WTFMove(top));
    quad->setRight(WTFMove(right));
    quad->setBottom(WTFMove(bottom));
    quad->setLeft(WTFMove(left));

    return CSSBorderImageSliceValue::create(WTFMove(quad), image.fill());
}

static Ref<CSSPrimitiveValue> valueForNinePieceImageQuad(const LengthBox& box, const RenderStyle& style)
{
    RefPtr<CSSPrimitiveValue> top;
    RefPtr<CSSPrimitiveValue> right;
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;

    if (box.top().isRelative())
        top = CSSPrimitiveValue::create(box.top().value(), CSSUnitType::CSS_NUMBER);
    else
        top = CSSPrimitiveValue::create(box.top(), style);

    if (box.right() == box.top() && box.bottom() == box.top() && box.left() == box.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (box.right().isRelative())
            right = CSSPrimitiveValue::create(box.right().value(), CSSUnitType::CSS_NUMBER);
        else
            right = CSSPrimitiveValue::create(box.right(), style);

        if (box.bottom() == box.top() && box.right() == box.left()) {
            bottom = top;
            left = right;
        } else {
            if (box.bottom().isRelative())
                bottom = CSSPrimitiveValue::create(box.bottom().value(), CSSUnitType::CSS_NUMBER);
            else
                bottom = CSSPrimitiveValue::create(box.bottom(), style);

            if (box.left() == box.right())
                left = right;
            else {
                if (box.left().isRelative())
                    left = CSSPrimitiveValue::create(box.left().value(), CSSUnitType::CSS_NUMBER);
                else
                    left = CSSPrimitiveValue::create(box.left(), style);
            }
        }
    }

    auto quad = Quad::create();
    quad->setTop(WTFMove(top));
    quad->setRight(WTFMove(right));
    quad->setBottom(WTFMove(bottom));
    quad->setLeft(WTFMove(left));

    return CSSPrimitiveValue::create(WTFMove(quad));
}

static Ref<CSSValue> valueForNinePieceImageRepeat(const NinePieceImage& image)
{
    auto horizontalRepeat = CSSPrimitiveValue::create(valueForRepeatRule(image.horizontalRule()));
    RefPtr<CSSPrimitiveValue> verticalRepeat;
    if (image.horizontalRule() == image.verticalRule())
        verticalRepeat = horizontalRepeat.copyRef();
    else
        verticalRepeat = CSSPrimitiveValue::create(valueForRepeatRule(image.verticalRule()));
    return CSSPrimitiveValue::create(Pair::create(WTFMove(horizontalRepeat), WTFMove(verticalRepeat)));
}

static RefPtr<CSSValue> valueForNinePieceImage(CSSPropertyID propertyID, const NinePieceImage& image, const RenderStyle& style)
{
    if (!image.hasImage())
        return CSSPrimitiveValue::create(CSSValueNone);

    RefPtr<CSSValue> imageValue;
    if (image.image())
        imageValue = image.image()->computedStyleValue(style);

    // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
    const LengthBox& slices = image.borderSlices();
    bool overridesBorderWidths = propertyID == CSSPropertyWebkitBorderImage && (slices.top().isFixed() || slices.right().isFixed() || slices.bottom().isFixed() || slices.left().isFixed());
    if (overridesBorderWidths != image.overridesBorderWidths())
        return nullptr;

    auto imageSlices = valueForNinePieceImageSlice(image);
    auto borderSlices = valueForNinePieceImageQuad(slices, style);
    auto outset = valueForNinePieceImageQuad(image.outset(), style);
    auto repeat = valueForNinePieceImageRepeat(image);

    return createBorderImageValue(WTFMove(imageValue), WTFMove(imageSlices), WTFMove(borderSlices), WTFMove(outset), WTFMove(repeat));
}

static Ref<CSSPrimitiveValue> fontSizeAdjustFromStyle(const RenderStyle& style)
{
    auto adjust = style.fontSizeAdjust();
    if (!adjust)
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSPrimitiveValue::create(*adjust, CSSUnitType::CSS_NUMBER);
}

static Ref<CSSPrimitiveValue> zoomAdjustedPixelValue(double value, const RenderStyle& style)
{
    return CSSPrimitiveValue::create(adjustFloatForAbsoluteZoom(value, style), CSSUnitType::CSS_PX);
}

static Ref<CSSPrimitiveValue> zoomAdjustedPixelValueForLength(const Length& length, const RenderStyle& style)
{
    if (length.isFixed())
        return zoomAdjustedPixelValue(length.value(), style);
    return CSSPrimitiveValue::create(length, style);
}

static inline Ref<CSSValue> valueForReflection(const StyleReflection* reflection, const RenderStyle& style)
{
    if (!reflection)
        return CSSPrimitiveValue::create(CSSValueNone);

    RefPtr<CSSPrimitiveValue> offset;
    if (reflection->offset().isPercentOrCalculated())
        offset = CSSPrimitiveValue::create(reflection->offset().percent(), CSSUnitType::CSS_PERCENTAGE);
    else
        offset = zoomAdjustedPixelValue(reflection->offset().value(), style);

    RefPtr<CSSPrimitiveValue> direction;
    switch (reflection->direction()) {
    case ReflectionDirection::Below:
        direction = CSSPrimitiveValue::create(CSSValueBelow);
        break;
    case ReflectionDirection::Above:
        direction = CSSPrimitiveValue::create(CSSValueAbove);
        break;
    case ReflectionDirection::Left:
        direction = CSSPrimitiveValue::create(CSSValueLeft);
        break;
    case ReflectionDirection::Right:
        direction = CSSPrimitiveValue::create(CSSValueRight);
        break;
    }

    return CSSReflectValue::create(direction.releaseNonNull(), offset.releaseNonNull(), valueForNinePieceImage(CSSPropertyWebkitBoxReflect, reflection->mask(), style));
}

static Ref<CSSValueList> createPositionListForLayer(CSSPropertyID propertyID, const FillLayer& layer, const RenderStyle& style)
{
    auto list = CSSValueList::createSpaceSeparated();
    if (layer.isBackgroundXOriginSet() && layer.backgroundXOrigin() != Edge::Left) {
        ASSERT_UNUSED(propertyID, propertyID == CSSPropertyBackgroundPosition || propertyID == CSSPropertyMaskPosition || propertyID == CSSPropertyWebkitMaskPosition);
        list->append(CSSPrimitiveValue::create(layer.backgroundXOrigin()));
    }
    list->append(zoomAdjustedPixelValueForLength(layer.xPosition(), style));
    if (layer.isBackgroundYOriginSet() && layer.backgroundYOrigin() != Edge::Top) {
        ASSERT(propertyID == CSSPropertyBackgroundPosition || propertyID == CSSPropertyMaskPosition || propertyID == CSSPropertyWebkitMaskPosition);
        list->append(CSSPrimitiveValue::create(layer.backgroundYOrigin()));
    }
    list->append(zoomAdjustedPixelValueForLength(layer.yPosition(), style));
    return list;
}

static Ref<CSSValue> createSingleAxisPositionValueForLayer(CSSPropertyID propertyID, const FillLayer& layer, const RenderStyle& style)
{
    if (propertyID == CSSPropertyBackgroundPositionX || propertyID == CSSPropertyWebkitMaskPositionX) {
        if (!layer.isBackgroundXOriginSet() || layer.backgroundXOrigin() == Edge::Left)
            return zoomAdjustedPixelValueForLength(layer.xPosition(), style);

        auto list = CSSValueList::createSpaceSeparated();
        list->append(CSSPrimitiveValue::create(layer.backgroundXOrigin()));
        list->append(zoomAdjustedPixelValueForLength(layer.xPosition(), style));
        return list;
    }

    if (!layer.isBackgroundYOriginSet() || layer.backgroundYOrigin() == Edge::Top)
        return zoomAdjustedPixelValueForLength(layer.yPosition(), style);

    auto list = CSSValueList::createSpaceSeparated();
    list->append(CSSPrimitiveValue::create(layer.backgroundYOrigin()));
    list->append(zoomAdjustedPixelValueForLength(layer.yPosition(), style));
    return list;
}

static Length getOffsetComputedLength(const RenderStyle& style, CSSPropertyID propertyID)
{
    // If specified as a length, the corresponding absolute length; if specified as
    // a percentage, the specified value; otherwise, 'auto'. Hence, we can just
    // return the value in the style.
    //
    // See http://www.w3.org/TR/CSS21/cascade.html#computed-value
    switch (propertyID) {
    case CSSPropertyLeft:
        return style.left();
    case CSSPropertyRight:
        return style.right();
    case CSSPropertyTop:
        return style.top();
    case CSSPropertyBottom:
        return style.bottom();
    default:
        ASSERT_NOT_REACHED();
    }

    return { };
}

static LayoutUnit getOffsetUsedStyleRelative(RenderBox& box, CSSPropertyID propertyID)
{
    // For relatively positioned boxes, the offset is with respect to the top edges
    // of the box itself. This ties together top/bottom and left/right to be
    // opposites of each other.
    //
    // See http://www.w3.org/TR/CSS2/visuren.html#relative-positioning
    //
    // Specifically;
    //   Since boxes are not split or stretched as a result of 'left' or
    //   'right', the used values are always: left = -right.
    // and
    //   Since boxes are not split or stretched as a result of 'top' or
    //   'bottom', the used values are always: top = -bottom.
    switch (propertyID) {
    case CSSPropertyTop:
        return box.relativePositionOffset().height();
    case CSSPropertyBottom:
        return -(box.relativePositionOffset().height());
    case CSSPropertyLeft:
        return box.relativePositionOffset().width();
    case CSSPropertyRight:
        return -(box.relativePositionOffset().width());
    default:
        ASSERT_NOT_REACHED();
    }

    return 0;
}

static LayoutUnit getOffsetUsedStyleOutOfFlowPositioned(RenderBlock& container, RenderBox& box, CSSPropertyID propertyID)
{
    // For out-of-flow positioned boxes, the offset is how far an box's margin
    // edge is offset below the edge of the box's containing block.
    // See http://www.w3.org/TR/CSS2/visuren.html#position-props

    // Margins are included in offsetTop/offsetLeft so we need to remove them here.
    switch (propertyID) {
    case CSSPropertyTop:
        return box.offsetTop() - box.marginTop();
    case CSSPropertyBottom:
        return container.clientHeight() - (box.offsetTop() + box.offsetHeight()) - box.marginBottom();
    case CSSPropertyLeft:
        return box.offsetLeft() - box.marginLeft();
    case CSSPropertyRight:
        return container.clientWidth() - (box.offsetLeft() + box.offsetWidth()) - box.marginRight();
    default:
        ASSERT_NOT_REACHED();
    }

    return 0;
}

static RefPtr<CSSValue> positionOffsetValue(const RenderStyle& style, CSSPropertyID propertyID, RenderObject* renderer)
{
    auto offset = getOffsetComputedLength(style, propertyID);

    // If the element is not displayed; return the "computed value".
    if (!renderer || !renderer->isBox())
        return zoomAdjustedPixelValueForLength(offset, style);

    auto& box = downcast<RenderBox>(*renderer);
    auto* containingBlock = box.containingBlock();

    // Resolve a "computed value" percentage if the element is positioned.
    if (containingBlock && offset.isPercentOrCalculated() && box.isPositioned()) {
        bool isVerticalProperty;
        if (propertyID == CSSPropertyTop || propertyID == CSSPropertyBottom)
            isVerticalProperty = true;
        else {
            ASSERT(propertyID == CSSPropertyLeft || propertyID == CSSPropertyRight);
            isVerticalProperty = false;
        }
        LayoutUnit containingBlockSize;
        if (box.isStickilyPositioned()) {
            auto& enclosingClippingBox = box.enclosingClippingBoxForStickyPosition().first;
            if (isVerticalProperty == enclosingClippingBox.isHorizontalWritingMode())
                containingBlockSize = enclosingClippingBox.contentLogicalHeight();
            else
                containingBlockSize = enclosingClippingBox.contentLogicalWidth();
        } else {
            if (isVerticalProperty == containingBlock->isHorizontalWritingMode()) {
                containingBlockSize = box.isOutOfFlowPositioned()
                    ? box.containingBlockLogicalHeightForPositioned(*containingBlock, false)
                    : box.containingBlockLogicalHeightForContent(ExcludeMarginBorderPadding);
            } else {
                containingBlockSize = box.isOutOfFlowPositioned()
                    ? box.containingBlockLogicalWidthForPositioned(*containingBlock, nullptr, false)
                    : box.containingBlockLogicalWidthForContent();
            }
        }
        return zoomAdjustedPixelValue(floatValueForLength(offset, containingBlockSize), style);
    }

    // Return a "computed value" length.
    if (!offset.isAuto())
        return zoomAdjustedPixelValueForLength(offset, style);

    // The property won't be overconstrained if its computed value is "auto", so the "used value" can be returned.
    if (box.isRelativelyPositioned())
        return zoomAdjustedPixelValue(getOffsetUsedStyleRelative(box, propertyID), style);

    if (containingBlock && box.isOutOfFlowPositioned())
        return zoomAdjustedPixelValue(getOffsetUsedStyleOutOfFlowPositioned(*containingBlock, box, propertyID), style);

    return CSSPrimitiveValue::create(CSSValueAuto);
}

Ref<CSSPrimitiveValue> ComputedStyleExtractor::currentColorOrValidColor(const RenderStyle& style, const StyleColor& color)
{
    // This function does NOT look at visited information, so that computed style doesn't expose that.
    return CSSValuePool::singleton().createColorValue(style.colorResolvingCurrentColor(color));
}

static Ref<CSSPrimitiveValue> percentageOrZoomAdjustedValue(Length length, const RenderStyle& style)
{
    if (length.isPercent())
        return CSSPrimitiveValue::create(length.percent(), CSSUnitType::CSS_PERCENTAGE);

    return zoomAdjustedPixelValueForLength(length, style);
}

static Ref<CSSPrimitiveValue> autoOrZoomAdjustedValue(Length length, const RenderStyle& style)
{
    if (length.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);

    return zoomAdjustedPixelValueForLength(length, style);
}

static Ref<CSSValue> valueForQuotes(const QuotesData* quotes)
{
    if (!quotes)
        return CSSPrimitiveValue::create(CSSValueAuto);
    unsigned size = quotes->size();
    if (!size)
        return CSSPrimitiveValue::create(CSSValueNone);
    auto list = CSSValueList::createSpaceSeparated();
    for (unsigned i = 0; i < size; ++i) {
        list->append(CSSPrimitiveValue::create(quotes->openQuote(i), CSSUnitType::CSS_STRING));
        list->append(CSSPrimitiveValue::create(quotes->closeQuote(i), CSSUnitType::CSS_STRING));
    }
    return list;
}

static Ref<Pair> borderRadiusCornerValues(const LengthSize& radius, const RenderStyle& style)
{
    auto x = percentageOrZoomAdjustedValue(radius.width, style);
    auto y = radius.width == radius.height ? x.copyRef() : percentageOrZoomAdjustedValue(radius.height, style);
    return Pair::create(WTFMove(x), WTFMove(y));
}

static Ref<CSSPrimitiveValue> borderRadiusCornerValue(const LengthSize& radius, const RenderStyle& style)
{
    return CSSPrimitiveValue::create(borderRadiusCornerValues(radius, style));
}

static Ref<CSSValueList> borderRadiusShorthandValue(const RenderStyle& style, CSSPropertyID propertyID)
{
    auto list = CSSValueList::createSlashSeparated();
    bool showHorizontalBottomLeft = style.borderTopRightRadius().width != style.borderBottomLeftRadius().width;
    bool showHorizontalBottomRight = showHorizontalBottomLeft || (style.borderBottomRightRadius().width != style.borderTopLeftRadius().width);
    bool showHorizontalTopRight = showHorizontalBottomRight || (style.borderTopRightRadius().width != style.borderTopLeftRadius().width);

    bool showVerticalBottomLeft = style.borderTopRightRadius().height != style.borderBottomLeftRadius().height;
    bool showVerticalBottomRight = showVerticalBottomLeft || (style.borderBottomRightRadius().height != style.borderTopLeftRadius().height);
    bool showVerticalTopRight = showVerticalBottomRight || (style.borderTopRightRadius().height != style.borderTopLeftRadius().height);

    auto topLeftRadius = borderRadiusCornerValues(style.borderTopLeftRadius(), style);
    CSSValue* topLeftRadiusX = topLeftRadius->first();
    CSSValue* topLeftRadiusY = topLeftRadius->second();
    auto topRightRadius = borderRadiusCornerValues(style.borderTopRightRadius(), style);
    CSSValue* topRightRadiusX = topRightRadius->first();
    CSSValue* topRightRadiusY = topRightRadius->second();
    auto bottomRightRadius = borderRadiusCornerValues(style.borderBottomRightRadius(), style);
    CSSValue* bottomRightRadiusX = bottomRightRadius->first();
    CSSValue* bottomRightRadiusY = bottomRightRadius->second();
    auto bottomLeftRadius = borderRadiusCornerValues(style.borderBottomLeftRadius(), style);
    CSSValue* bottomLeftRadiusX = bottomLeftRadius->first();
    CSSValue* bottomLeftRadiusY = bottomLeftRadius->second();

    auto horizontalRadii = CSSValueList::createSpaceSeparated();
    horizontalRadii->append(*topLeftRadiusX);
    if (showHorizontalTopRight)
        horizontalRadii->append(*topRightRadiusX);
    if (showHorizontalBottomRight)
        horizontalRadii->append(*bottomRightRadiusX);
    if (showHorizontalBottomLeft)
        horizontalRadii->append(*bottomLeftRadiusX);

    list->append(WTFMove(horizontalRadii));

    auto verticalRadiiList = CSSValueList::createSpaceSeparated();
    verticalRadiiList->append(*topLeftRadiusY);
    if (showVerticalTopRight)
        verticalRadiiList->append(*topRightRadiusY);
    if (showVerticalBottomRight)
        verticalRadiiList->append(*bottomRightRadiusY);
    if (showVerticalBottomLeft)
        verticalRadiiList->append(*bottomLeftRadiusY);

    if (!verticalRadiiList->equals(downcast<CSSValueList>(*list->item(0))))
        list->append(WTFMove(verticalRadiiList));
    else if (propertyID == CSSPropertyWebkitBorderRadius && showHorizontalTopRight && !showHorizontalBottomRight)
        downcast<CSSValueList>(list->item(0))->append(*bottomRightRadiusX);

    return list;
}

static LayoutRect sizingBox(RenderObject& renderer)
{
    if (!is<RenderBox>(renderer))
        return LayoutRect();

    auto& box = downcast<RenderBox>(renderer);
    return box.style().boxSizing() == BoxSizing::BorderBox ? box.borderBoxRect() : box.computedCSSContentBoxRect();
}

static Ref<CSSFunctionValue> matrixTransformValue(const TransformationMatrix& transform, const RenderStyle& style)
{
    RefPtr<CSSFunctionValue> transformValue;
    auto zoom = style.effectiveZoom();
    if (transform.isAffine()) {
        transformValue = CSSFunctionValue::create(CSSValueMatrix);

        transformValue->append(CSSPrimitiveValue::create(transform.a(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.b(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.c(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.d(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.e() / zoom, CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.f() / zoom, CSSUnitType::CSS_NUMBER));
    } else {
        transformValue = CSSFunctionValue::create(CSSValueMatrix3d);

        transformValue->append(CSSPrimitiveValue::create(transform.m11(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.m12(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.m13(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.m14() * zoom, CSSUnitType::CSS_NUMBER));

        transformValue->append(CSSPrimitiveValue::create(transform.m21(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.m22(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.m23(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.m24() * zoom, CSSUnitType::CSS_NUMBER));

        transformValue->append(CSSPrimitiveValue::create(transform.m31(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.m32(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.m33(), CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.m34() * zoom, CSSUnitType::CSS_NUMBER));

        transformValue->append(CSSPrimitiveValue::create(transform.m41() / zoom, CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.m42() / zoom, CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.m43() / zoom, CSSUnitType::CSS_NUMBER));
        transformValue->append(CSSPrimitiveValue::create(transform.m44(), CSSUnitType::CSS_NUMBER));
    }

    return transformValue.releaseNonNull();
}

RefPtr<CSSFunctionValue> transformOperationAsCSSValue(const TransformOperation& operation, const RenderStyle& style)
{
    auto translateLengthAsCSSValue = [&](const Length& length) {
        if (length.isZero())
            return CSSPrimitiveValue::create(0, CSSUnitType::CSS_PX);
        return zoomAdjustedPixelValueForLength(length, style);
    };

    auto includeLength = [](const Length& length) -> bool {
        return !length.isZero() || length.isPercent();
    };

    switch (operation.type()) {
    // translate
    case TransformOperation::Type::TranslateX: {
        auto functionValue = CSSFunctionValue::create(CSSValueTranslateX);
        functionValue->append(translateLengthAsCSSValue(downcast<TranslateTransformOperation>(operation).x()));
        return functionValue;
    }
    case TransformOperation::Type::TranslateY: {
        auto functionValue = CSSFunctionValue::create(CSSValueTranslateY);
        functionValue->append(translateLengthAsCSSValue(downcast<TranslateTransformOperation>(operation).y()));
        return functionValue;
    }
    case TransformOperation::Type::TranslateZ: {
        auto functionValue = CSSFunctionValue::create(CSSValueTranslateZ);
        functionValue->append(translateLengthAsCSSValue(downcast<TranslateTransformOperation>(operation).z()));
        return functionValue;
    }
    case TransformOperation::Type::Translate:
    case TransformOperation::Type::Translate3D: {
        auto& translate = downcast<TranslateTransformOperation>(operation);
        auto is3D = translate.is3DOperation();
        auto functionValue = CSSFunctionValue::create(is3D ? CSSValueTranslate3d : CSSValueTranslate);
        functionValue->append(translateLengthAsCSSValue(translate.x()));
        if (is3D || includeLength(translate.y()))
            functionValue->append(translateLengthAsCSSValue(translate.y()));
        if (is3D)
            functionValue->append(translateLengthAsCSSValue(translate.z()));
        return functionValue;
    }
    // scale
    case TransformOperation::Type::ScaleX: {
        auto functionValue = CSSFunctionValue::create(CSSValueScaleX);
        functionValue->append(CSSPrimitiveValue::create(downcast<ScaleTransformOperation>(operation).x(), CSSUnitType::CSS_NUMBER));
        return functionValue;
    }
    case TransformOperation::Type::ScaleY: {
        auto functionValue = CSSFunctionValue::create(CSSValueScaleY);
        functionValue->append(CSSPrimitiveValue::create(downcast<ScaleTransformOperation>(operation).y(), CSSUnitType::CSS_NUMBER));
        return functionValue;
    }
    case TransformOperation::Type::ScaleZ: {
        auto functionValue = CSSFunctionValue::create(CSSValueScaleZ);
        functionValue->append(CSSPrimitiveValue::create(downcast<ScaleTransformOperation>(operation).z(), CSSUnitType::CSS_NUMBER));
        return functionValue;
    }
    case TransformOperation::Type::Scale:
    case TransformOperation::Type::Scale3D: {
        auto& scale = downcast<ScaleTransformOperation>(operation);
        auto is3D = scale.is3DOperation();
        auto functionValue = CSSFunctionValue::create(is3D ? CSSValueScale3d : CSSValueScale);
        functionValue->append(CSSPrimitiveValue::create(scale.x(), CSSUnitType::CSS_NUMBER));
        if (is3D || scale.x() != scale.y())
            functionValue->append(CSSPrimitiveValue::create(scale.y(), CSSUnitType::CSS_NUMBER));
        if (is3D)
            functionValue->append(CSSPrimitiveValue::create(scale.z(), CSSUnitType::CSS_NUMBER));
        return functionValue;
    }
    // rotate
    case TransformOperation::Type::RotateX: {
        auto functionValue = CSSFunctionValue::create(CSSValueRotateX);
        functionValue->append(CSSPrimitiveValue::create(downcast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
        return functionValue;
    }
    case TransformOperation::Type::RotateY: {
        auto functionValue = CSSFunctionValue::create(CSSValueRotateX);
        functionValue->append(CSSPrimitiveValue::create(downcast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
        return functionValue;
    }
    case TransformOperation::Type::RotateZ: {
        auto functionValue = CSSFunctionValue::create(CSSValueRotateZ);
        functionValue->append(CSSPrimitiveValue::create(downcast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
        return functionValue;
    }
    case TransformOperation::Type::Rotate: {
        auto& rotate = downcast<RotateTransformOperation>(operation);
        auto functionValue = CSSFunctionValue::create(CSSValueRotate);
        functionValue->append(CSSPrimitiveValue::create(rotate.angle(), CSSUnitType::CSS_DEG));
        return functionValue;
    }
    case TransformOperation::Type::Rotate3D: {
        auto& rotate = downcast<RotateTransformOperation>(operation);
        auto functionValue = CSSFunctionValue::create(CSSValueRotate3d);
        functionValue->append(CSSPrimitiveValue::create(rotate.x(), CSSUnitType::CSS_NUMBER));
        functionValue->append(CSSPrimitiveValue::create(rotate.y(), CSSUnitType::CSS_NUMBER));
        functionValue->append(CSSPrimitiveValue::create(rotate.z(), CSSUnitType::CSS_NUMBER));
        functionValue->append(CSSPrimitiveValue::create(rotate.angle(), CSSUnitType::CSS_DEG));
        return functionValue;
    }
    // skew
    case TransformOperation::Type::SkewX: {
        auto functionValue = CSSFunctionValue::create(CSSValueSkewX);
        functionValue->append(CSSPrimitiveValue::create(downcast<SkewTransformOperation>(operation).angleX(), CSSUnitType::CSS_DEG));
        return functionValue;
    }
    case TransformOperation::Type::SkewY: {
        auto functionValue = CSSFunctionValue::create(CSSValueSkewY);
        functionValue->append(CSSPrimitiveValue::create(downcast<SkewTransformOperation>(operation).angleY(), CSSUnitType::CSS_DEG));
        return functionValue;
    }
    case TransformOperation::Type::Skew: {
        auto& skew = downcast<SkewTransformOperation>(operation);
        auto functionValue = CSSFunctionValue::create(CSSValueSkew);
        functionValue->append(CSSPrimitiveValue::create(skew.angleX(), CSSUnitType::CSS_DEG));
        if (skew.angleY())
            functionValue->append(CSSPrimitiveValue::create(skew.angleY(), CSSUnitType::CSS_DEG));
        return functionValue;
    }
    // perspective
    case TransformOperation::Type::Perspective: {
        auto functionValue = CSSFunctionValue::create(CSSValuePerspective);
        if (auto perspective = downcast<PerspectiveTransformOperation>(operation).perspective())
            functionValue->append(zoomAdjustedPixelValueForLength(*perspective, style));
        else
            functionValue->append(CSSPrimitiveValue::create(CSSValueNone));
        return functionValue;
    }
    // matrix
    case TransformOperation::Type::Matrix:
    case TransformOperation::Type::Matrix3D: {
        TransformationMatrix transform;
        operation.apply(transform, { });
        return matrixTransformValue(transform, style);
    }
    case TransformOperation::Type::Identity:
    case TransformOperation::Type::None:
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

static Ref<CSSValue> computedTransform(RenderElement* renderer, const RenderStyle& style, ComputedStyleExtractor::PropertyValueType valueType)
{
    if (!style.hasTransform() || is<RenderInline>(renderer))
        return CSSPrimitiveValue::create(CSSValueNone);

    if (renderer) {
        TransformationMatrix transform;
        style.applyTransform(transform, renderer->transformReferenceBoxRect(style), { });
        auto list = CSSTransformListValue::create();
        list->append(matrixTransformValue(transform, style));
        return list;
    }

    // https://w3c.github.io/csswg-drafts/css-transforms-1/#serialization-of-the-computed-value
    // If we don't have a renderer, then the value should be "none" if we're asking for the
    // resolved value (such as when calling getComputedStyle()).
    if (valueType == ComputedStyleExtractor::PropertyValueType::Resolved)
        return CSSPrimitiveValue::create(CSSValueNone);

    auto list = CSSTransformListValue::create();

    for (auto& operation : style.transform().operations()) {
        auto functionValue = transformOperationAsCSSValue(*operation, style);
        if (!functionValue)
            continue;
        list->append(functionValue.releaseNonNull());
    }

    if (list->length())
        return list;

    return CSSPrimitiveValue::create(CSSValueNone);
}

// https://drafts.csswg.org/css-transforms-2/#propdef-translate
// Computed value: the keyword none or a pair of computed <length-percentage> values and an absolute length
static Ref<CSSValue> computedTranslate(RenderObject* renderer, const RenderStyle& style)
{
    auto* translate = style.translate();
    if (!translate || is<RenderInline>(renderer) || translate->isIdentity())
        return CSSPrimitiveValue::create(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();
    list->append(zoomAdjustedPixelValueForLength(translate->x(), style));

    auto includeLength = [](const Length& length) -> bool {
        return !length.isZero() || length.isPercent();
    };

    if (includeLength(translate->y()) || includeLength(translate->z()))
        list->append(zoomAdjustedPixelValueForLength(translate->y(), style));

    if (includeLength(translate->z()))
        list->append(zoomAdjustedPixelValueForLength(translate->z(), style));

    return list;
}

static Ref<CSSValue> computedScale(RenderObject* renderer, const RenderStyle& style)
{
    auto* scale = style.scale();
    if (!scale || is<RenderInline>(renderer) || scale->isIdentity())
        return CSSPrimitiveValue::create(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();
    list->append(CSSPrimitiveValue::create(scale->x(), CSSUnitType::CSS_NUMBER));
    if (scale->z() != 1) {
        list->append(CSSPrimitiveValue::create(scale->y(), CSSUnitType::CSS_NUMBER));
        list->append(CSSPrimitiveValue::create(scale->z(), CSSUnitType::CSS_NUMBER));
    } else if (scale->x() != scale->y())
        list->append(CSSPrimitiveValue::create(scale->y(), CSSUnitType::CSS_NUMBER));
    return list;
}

static Ref<CSSValue> computedRotate(RenderObject* renderer, const RenderStyle& style)
{
    auto* rotate = style.rotate();
    if (!rotate || is<RenderInline>(renderer) || rotate->isIdentity())
        return CSSPrimitiveValue::create(CSSValueNone);

    if (!rotate->is3DOperation() || (!rotate->x() && !rotate->y() && rotate->z()))
        return CSSPrimitiveValue::create(rotate->angle(), CSSUnitType::CSS_DEG);

    auto list = CSSValueList::createSpaceSeparated();

    if (rotate->x() && !rotate->y() && !rotate->z())
        list->append(CSSPrimitiveValue::create(CSSValueX));
    else if (!rotate->x() && rotate->y() && !rotate->z())
        list->append(CSSPrimitiveValue::create(CSSValueY));
    else {
        list->append(CSSPrimitiveValue::create(rotate->x(), CSSUnitType::CSS_NUMBER));
        list->append(CSSPrimitiveValue::create(rotate->y(), CSSUnitType::CSS_NUMBER));
        list->append(CSSPrimitiveValue::create(rotate->z(), CSSUnitType::CSS_NUMBER));
    }

    list->append(CSSPrimitiveValue::create(rotate->angle(), CSSUnitType::CSS_DEG));

    return list;
}

static inline Ref<CSSPrimitiveValue> adjustLengthForZoom(const Length& length, const RenderStyle& style, ComputedStyleExtractor::AdjustPixelValuesForComputedStyle adjust)
{
    return adjust == ComputedStyleExtractor::AdjustPixelValuesForComputedStyle::Yes ? zoomAdjustedPixelValue(length.value(), style) : CSSPrimitiveValue::create(length);
}

Ref<CSSValue> ComputedStyleExtractor::valueForShadow(const ShadowData* shadow, CSSPropertyID propertyID, const RenderStyle& style, AdjustPixelValuesForComputedStyle adjust)
{
    if (!shadow)
        return CSSPrimitiveValue::create(CSSValueNone);

    auto& cssValuePool = CSSValuePool::singleton();
    auto list = CSSValueList::createCommaSeparated();
    for (const ShadowData* currShadowData = shadow; currShadowData; currShadowData = currShadowData->next()) {
        auto x = adjustLengthForZoom(currShadowData->x(), style, adjust);
        auto y = adjustLengthForZoom(currShadowData->y(), style, adjust);
        auto blur = adjustLengthForZoom(currShadowData->radius(), style, adjust);
        auto spread = propertyID == CSSPropertyTextShadow ? RefPtr<CSSPrimitiveValue>() : adjustLengthForZoom(currShadowData->spread(), style, adjust);
        auto style = propertyID == CSSPropertyTextShadow || currShadowData->style() == ShadowStyle::Normal ? RefPtr<CSSPrimitiveValue>() : CSSPrimitiveValue::create(CSSValueInset);
        auto color = cssValuePool.createColorValue(currShadowData->color());
        list->prepend(CSSShadowValue::create(WTFMove(x), WTFMove(y), WTFMove(blur), WTFMove(spread), WTFMove(style), WTFMove(color)));
    }
    return list;
}

Ref<CSSValue> ComputedStyleExtractor::valueForFilter(const RenderStyle& style, const FilterOperations& filterOperations, AdjustPixelValuesForComputedStyle adjust)
{
    if (filterOperations.operations().isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();

    Vector<RefPtr<FilterOperation>>::const_iterator end = filterOperations.operations().end();
    for (Vector<RefPtr<FilterOperation>>::const_iterator it = filterOperations.operations().begin(); it != end; ++it) {
        FilterOperation& filterOperation = **it;

        if (filterOperation.type() == FilterOperation::Type::Reference) {
            ReferenceFilterOperation& referenceOperation = downcast<ReferenceFilterOperation>(filterOperation);
            list->append(CSSPrimitiveValue::create(referenceOperation.url(), CSSUnitType::CSS_URI));
        } else {
            RefPtr<CSSFunctionValue> filterValue;
            switch (filterOperation.type()) {
            case FilterOperation::Type::Grayscale: {
                filterValue = CSSFunctionValue::create(CSSValueGrayscale);
                filterValue->append(CSSPrimitiveValue::create(downcast<BasicColorMatrixFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::Type::Sepia: {
                filterValue = CSSFunctionValue::create(CSSValueSepia);
                filterValue->append(CSSPrimitiveValue::create(downcast<BasicColorMatrixFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::Type::Saturate: {
                filterValue = CSSFunctionValue::create(CSSValueSaturate);
                filterValue->append(CSSPrimitiveValue::create(downcast<BasicColorMatrixFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::Type::HueRotate: {
                filterValue = CSSFunctionValue::create(CSSValueHueRotate);
                filterValue->append(CSSPrimitiveValue::create(downcast<BasicColorMatrixFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_DEG));
                break;
            }
            case FilterOperation::Type::Invert: {
                filterValue = CSSFunctionValue::create(CSSValueInvert);
                filterValue->append(CSSPrimitiveValue::create(downcast<BasicComponentTransferFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::Type::AppleInvertLightness: {
                filterValue = CSSFunctionValue::create(CSSValueAppleInvertLightness);
                break;
            }
            case FilterOperation::Type::Opacity: {
                filterValue = CSSFunctionValue::create(CSSValueOpacity);
                filterValue->append(CSSPrimitiveValue::create(downcast<BasicComponentTransferFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::Type::Brightness: {
                filterValue = CSSFunctionValue::create(CSSValueBrightness);
                filterValue->append(CSSPrimitiveValue::create(downcast<BasicComponentTransferFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::Type::Contrast: {
                filterValue = CSSFunctionValue::create(CSSValueContrast);
                filterValue->append(CSSPrimitiveValue::create(downcast<BasicComponentTransferFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::Type::Blur: {
                filterValue = CSSFunctionValue::create(CSSValueBlur);
                filterValue->append(adjustLengthForZoom(downcast<BlurFilterOperation>(filterOperation).stdDeviation(), style, adjust));
                break;
            }
            case FilterOperation::Type::DropShadow: {
                DropShadowFilterOperation& dropShadowOperation = downcast<DropShadowFilterOperation>(filterOperation);
                filterValue = CSSFunctionValue::create(CSSValueDropShadow);
                // We want our computed style to look like that of a text shadow (has neither spread nor inset style).
                ShadowData shadowData = ShadowData({ Length(dropShadowOperation.location().x(), LengthType::Fixed), Length(dropShadowOperation.location().y(), LengthType::Fixed) }, Length(dropShadowOperation.stdDeviation(), LengthType::Fixed), Length(0, LengthType::Fixed), ShadowStyle::Normal, false, dropShadowOperation.color());
                filterValue->append(valueForShadow(&shadowData, CSSPropertyTextShadow, style, adjust));
                break;
            }
            default:
                ASSERT_NOT_REACHED();
                filterValue = CSSFunctionValue::create(CSSValueInvalid);
                break;
            }
            list->append(filterValue.releaseNonNull());
        }
    }
    return list;
}

static Ref<CSSValue> specifiedValueForGridTrackBreadth(const GridLength& trackBreadth, const RenderStyle& style)
{
    if (!trackBreadth.isLength())
        return CSSPrimitiveValue::create(trackBreadth.flex(), CSSUnitType::CSS_FR);

    const Length& trackBreadthLength = trackBreadth.length();
    if (trackBreadthLength.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return zoomAdjustedPixelValueForLength(trackBreadthLength, style);
}

static Ref<CSSValue> specifiedValueForGridTrackSize(const GridTrackSize& trackSize, const RenderStyle& style)
{
    switch (trackSize.type()) {
    case LengthTrackSizing:
        return specifiedValueForGridTrackBreadth(trackSize.minTrackBreadth(), style);
    case FitContentTrackSizing: {
        auto fitContentTrackSize = CSSFunctionValue::create(CSSValueFitContent);
        fitContentTrackSize->append(zoomAdjustedPixelValueForLength(trackSize.fitContentTrackBreadth().length(), style));
        return fitContentTrackSize;
    }
    default:
        ASSERT(trackSize.type() == MinMaxTrackSizing);
        if (trackSize.minTrackBreadth().isAuto() && trackSize.maxTrackBreadth().isFlex())
            return CSSPrimitiveValue::create(trackSize.maxTrackBreadth().flex(), CSSUnitType::CSS_FR);

        auto minMaxTrackBreadths = CSSFunctionValue::create(CSSValueMinmax);
        minMaxTrackBreadths->append(specifiedValueForGridTrackBreadth(trackSize.minTrackBreadth(), style));
        minMaxTrackBreadths->append(specifiedValueForGridTrackBreadth(trackSize.maxTrackBreadth(), style));
        return minMaxTrackBreadths;
    }
}

static void addValuesForNamedGridLinesAtIndex(OrderedNamedLinesCollector& collector, unsigned i, CSSValueList& list, bool renderEmpty = false)
{
    if (collector.isEmpty() && !renderEmpty)
        return;

    auto lineNames = CSSGridLineNamesValue::create();
    collector.collectLineNamesForIndex(lineNames.get(), i);
    if (lineNames->length() || renderEmpty)
        list.append(WTFMove(lineNames));
}

static Ref<CSSValueList> valueForGridTrackSizeList(GridTrackSizingDirection direction, const RenderStyle& style)
{
    auto& autoTrackSizes = direction == ForColumns ? style.gridAutoColumns() : style.gridAutoRows();

    auto list = CSSValueList::createSpaceSeparated();
    for (auto& trackSize : autoTrackSizes)
        list->append(specifiedValueForGridTrackSize(trackSize, style));
    return list;
}

template <typename T, typename F>
void populateGridTrackList(CSSValueList& list, OrderedNamedLinesCollector& collector, const Vector<T>& tracks, F getTrackSize, int offset = 0)
{
    int start = 0;
    int end = tracks.size();
    ASSERT(start <= end);
    ASSERT(static_cast<unsigned>(end) <= tracks.size());
    for (int i = start; i < end; ++i) {
        if (i + offset >= 0)
            addValuesForNamedGridLinesAtIndex(collector, i + offset, list);
        list.append(getTrackSize(tracks[i]));
    }
    if (end + offset >= 0)
        addValuesForNamedGridLinesAtIndex(collector, end + offset, list);
}

static void populateSubgridLineNameList(CSSValueList& list, OrderedNamedLinesCollector& collector)
{
    for (int i = 0; i < collector.namedGridLineCount(); i++)
        addValuesForNamedGridLinesAtIndex(collector, i, list, true);
}

static Ref<CSSValue> valueForGridTrackList(GridTrackSizingDirection direction, RenderObject* renderer, const RenderStyle& style)
{
    bool isRowAxis = direction == ForColumns;
    bool isRenderGrid = is<RenderGrid>(renderer);
    bool isSubgrid = isRowAxis ? style.gridSubgridColumns() : style.gridSubgridRows();
    bool isMasonry = (direction == ForRows) ? style.gridMasonryRows() : style.gridMasonryColumns();
    auto& trackSizes = isRowAxis ? style.gridColumnTrackSizes() : style.gridRowTrackSizes();
    auto& autoRepeatTrackSizes = isRowAxis ? style.gridAutoRepeatColumns() : style.gridAutoRepeatRows();

    // Handle the 'none' case.
    bool trackListIsEmpty = trackSizes.isEmpty() && autoRepeatTrackSizes.isEmpty();
    if (isRenderGrid && trackListIsEmpty) {
        // For grids we should consider every listed track, whether implicitly or explicitly
        // created. Empty grids have a sole grid line per axis.
        auto& grid = downcast<RenderGrid>(*renderer);
        auto& positions = isRowAxis ? grid.columnPositions() : grid.rowPositions();
        trackListIsEmpty = positions.size() == 1;
    }

    if (trackListIsEmpty && !isSubgrid)
        return isMasonry ? CSSPrimitiveValue::create(CSSValueMasonry) : CSSPrimitiveValue::create(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();

    // If the element is a grid container, the resolved value is the used value,
    // specifying track sizes in pixels and expanding the repeat() notation.
    // If subgrid was specified, but the element isn't a subgrid (due to not having
    // an appropriate grid parent), then we fall back to using the specified value.
    if (isRenderGrid && (!isSubgrid || downcast<RenderGrid>(renderer)->isSubgrid(direction))) {
        auto* grid = downcast<RenderGrid>(renderer);
        if (isSubgrid) {
            list->append(CSSPrimitiveValue::create(CSSValueSubgrid));

            OrderedNamedLinesCollectorInSubgridLayout collector(style, isRowAxis, grid->numTracks(direction));
            populateSubgridLineNameList(list.get(), collector);
            return list;
        }
        OrderedNamedLinesCollectorInGridLayout collector(style, isRowAxis, grid->autoRepeatCountForDirection(direction), autoRepeatTrackSizes.size());
        // Named grid line indices are relative to the explicit grid, but we are including all tracks.
        // So we need to subtract the number of leading implicit tracks in order to get the proper line index.
        int offset = -grid->explicitGridStartForDirection(direction);
        populateGridTrackList(list.get(), collector, grid->trackSizesForComputedStyle(direction), [&](const LayoutUnit& v) {
            return zoomAdjustedPixelValue(v, style);
        }, offset);
        return list;
    }

    // Otherwise, the resolved value is the computed value, preserving repeat().
    const GridTrackList& computedTracks = isRowAxis ? style.gridColumnList() : style.gridRowList();

    auto repeatVisitor = [&](CSSValueList& dest, const RepeatEntry& entry) {
        if (std::holds_alternative<Vector<String>>(entry)) {
            const auto& names = std::get<Vector<String>>(entry);
            if (names.isEmpty() && !isSubgrid)
                return;
            auto lineNamesValue = CSSGridLineNamesValue::create();
            for (const auto& name : names)
                lineNamesValue->append(CSSPrimitiveValue::createCustomIdent(name));
            dest.append(lineNamesValue);
        } else
            dest.append(specifiedValueForGridTrackSize(std::get<GridTrackSize>(entry), style));
    };

    auto trackEntryVisitor = WTF::makeVisitor([&](const GridTrackSize& size) {
        list->append(specifiedValueForGridTrackSize(size, style));
    }, [&](const Vector<String>& names) {
        // Subgrids don't have track sizes specified, so empty line names sets
        // need to be serialized, as they are meaningful placeholders.
        if (names.isEmpty() && !isSubgrid)
            return;

        auto lineNamesValue = CSSGridLineNamesValue::create();
        for (const auto& name : names)
            lineNamesValue->append(CSSPrimitiveValue::createCustomIdent(name));
        list->append(lineNamesValue);
    }, [&](const GridTrackEntryRepeat& repeat) {
        auto repeatedValues = CSSGridIntegerRepeatValue::create(repeat.repeats);
        for (const auto& entry : repeat.list)
            repeatVisitor(repeatedValues, entry);
        list->append(repeatedValues);
    }, [&](const GridTrackEntryAutoRepeat& repeat) {
        auto repeatedValues = CSSGridAutoRepeatValue::create(repeat.type == AutoRepeatType::Fill ? CSSValueAutoFill : CSSValueAutoFit);
        for (const auto& entry : repeat.list)
            repeatVisitor(repeatedValues, entry);
        list->append(repeatedValues);
    }, [&](const GridTrackEntrySubgrid&) {
        list->append(CSSPrimitiveValue::create(CSSValueSubgrid));
    }, [&](const GridTrackEntryMasonry) {
        list->append(CSSPrimitiveValue::create(CSSValueMasonry));
    });

    for (const auto& entry : computedTracks)
        std::visit(trackEntryVisitor, entry);

    return list;
}

static Ref<CSSValue> valueForGridPosition(const GridPosition& position)
{
    if (position.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);

    if (position.isNamedGridArea())
        return CSSPrimitiveValue::createCustomIdent(position.namedGridLine());

    bool hasNamedGridLine = !position.namedGridLine().isNull();
    auto list = CSSValueList::createSpaceSeparated();
    if (position.isSpan()) {
        list->append(CSSPrimitiveValue::create(CSSValueSpan));
        if (!hasNamedGridLine || position.spanPosition() != 1)
            list->append(CSSPrimitiveValue::create(position.spanPosition(), CSSUnitType::CSS_INTEGER));
    } else
        list->append(CSSPrimitiveValue::create(position.integerPosition(), CSSUnitType::CSS_INTEGER));

    if (hasNamedGridLine)
        list->append(CSSPrimitiveValue::createCustomIdent(position.namedGridLine()));
    return list;
}

static Ref<CSSValue> createTransitionPropertyValue(const Animation& animation)
{
    auto transitionProperty = animation.property();
    switch (transitionProperty.mode) {
    case Animation::TransitionMode::None:
        return CSSPrimitiveValue::create(CSSValueNone);
    case Animation::TransitionMode::All:
        return CSSPrimitiveValue::create(CSSValueAll);
    case Animation::TransitionMode::SingleProperty:
    case Animation::TransitionMode::UnknownProperty:
        auto transitionPropertyAsString = animatablePropertyAsString(transitionProperty.animatableProperty);
        return CSSPrimitiveValue::createCustomIdent(transitionPropertyAsString);
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueNone);
}

static Ref<CSSValueList> valueForScrollSnapType(const ScrollSnapType& type)
{
    auto value = CSSValueList::createSpaceSeparated();
    if (type.strictness == ScrollSnapStrictness::None)
        value->append(CSSPrimitiveValue::create(CSSValueNone));
    else {
        value->append(CSSPrimitiveValue::create(type.axis));
        if (type.strictness != ScrollSnapStrictness::Proximity)
            value->append(CSSPrimitiveValue::create(type.strictness));
    }
    return value;
}

static Ref<CSSValueList> valueForScrollSnapAlignment(const ScrollSnapAlign& alignment)
{
    auto value = CSSValueList::createSpaceSeparated();
    value->append(CSSPrimitiveValue::create(alignment.blockAlign));
    if (alignment.inlineAlign != alignment.blockAlign)
        value->append(CSSPrimitiveValue::create(alignment.inlineAlign));
    return value;
}

static Ref<CSSValueList> valueForTextEdge(const TextEdge& textEdge)
{
    auto value = CSSValueList::createSpaceSeparated();
    value->append(CSSPrimitiveValue::create(textEdge.over));
    value->append(CSSPrimitiveValue::create(textEdge.under));
    return value;
}

static Ref<CSSValue> willChangePropertyValue(const WillChangeData* willChangeData)
{
    if (!willChangeData || !willChangeData->numFeatures())
        return CSSPrimitiveValue::create(CSSValueAuto);

    auto list = CSSValueList::createCommaSeparated();
    for (size_t i = 0; i < willChangeData->numFeatures(); ++i) {
        WillChangeData::FeaturePropertyPair feature = willChangeData->featureAt(i);
        switch (feature.first) {
        case WillChangeData::ScrollPosition:
            list->append(CSSPrimitiveValue::create(CSSValueScrollPosition));
            break;
        case WillChangeData::Contents:
            list->append(CSSPrimitiveValue::create(CSSValueContents));
            break;
        case WillChangeData::Property:
            list->append(CSSPrimitiveValue::create(feature.second));
            break;
        case WillChangeData::Invalid:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return list;
}

static inline void appendLigaturesValue(CSSValueList& list, FontVariantLigatures value, CSSValueID yesValue, CSSValueID noValue)
{
    switch (value) {
    case FontVariantLigatures::Normal:
        return;
    case FontVariantLigatures::No:
        list.append(CSSPrimitiveValue::create(noValue));
        return;
    case FontVariantLigatures::Yes:
        list.append(CSSPrimitiveValue::create(yesValue));
        return;
    }
    ASSERT_NOT_REACHED();
}

static Ref<CSSValue> fontVariantLigaturesPropertyValue(FontVariantLigatures common, FontVariantLigatures discretionary, FontVariantLigatures historical, FontVariantLigatures contextualAlternates)
{
    if (common == FontVariantLigatures::No && discretionary == FontVariantLigatures::No && historical == FontVariantLigatures::No && contextualAlternates == FontVariantLigatures::No)
        return CSSPrimitiveValue::create(CSSValueNone);
    if (common == FontVariantLigatures::Normal && discretionary == FontVariantLigatures::Normal && historical == FontVariantLigatures::Normal && contextualAlternates == FontVariantLigatures::Normal)
        return CSSPrimitiveValue::create(CSSValueNormal);

    auto valueList = CSSValueList::createSpaceSeparated();
    appendLigaturesValue(valueList, common, CSSValueCommonLigatures, CSSValueNoCommonLigatures);
    appendLigaturesValue(valueList, discretionary, CSSValueDiscretionaryLigatures, CSSValueNoDiscretionaryLigatures);
    appendLigaturesValue(valueList, historical, CSSValueHistoricalLigatures, CSSValueNoHistoricalLigatures);
    appendLigaturesValue(valueList, contextualAlternates, CSSValueContextual, CSSValueNoContextual);
    return valueList;
}

static Ref<CSSValue> fontVariantPositionPropertyValue(FontVariantPosition position)
{
    CSSValueID valueID = CSSValueNormal;
    switch (position) {
    case FontVariantPosition::Normal:
        break;
    case FontVariantPosition::Subscript:
        valueID = CSSValueSub;
        break;
    case FontVariantPosition::Superscript:
        valueID = CSSValueSuper;
        break;
    }
    return CSSPrimitiveValue::create(valueID);
}

static Ref<CSSValue> fontVariantCapsPropertyValue(FontVariantCaps caps)
{
    CSSValueID valueID = CSSValueNormal;
    switch (caps) {
    case FontVariantCaps::Normal:
        break;
    case FontVariantCaps::Small:
        valueID = CSSValueSmallCaps;
        break;
    case FontVariantCaps::AllSmall:
        valueID = CSSValueAllSmallCaps;
        break;
    case FontVariantCaps::Petite:
        valueID = CSSValuePetiteCaps;
        break;
    case FontVariantCaps::AllPetite:
        valueID = CSSValueAllPetiteCaps;
        break;
    case FontVariantCaps::Unicase:
        valueID = CSSValueUnicase;
        break;
    case FontVariantCaps::Titling:
        valueID = CSSValueTitlingCaps;
        break;
    }
    return CSSPrimitiveValue::create(valueID);
}

static Ref<CSSValue> fontVariantNumericPropertyValue(FontVariantNumericFigure figure, FontVariantNumericSpacing spacing, FontVariantNumericFraction fraction, FontVariantNumericOrdinal ordinal, FontVariantNumericSlashedZero slashedZero)
{
    if (figure == FontVariantNumericFigure::Normal && spacing == FontVariantNumericSpacing::Normal && fraction == FontVariantNumericFraction::Normal && ordinal == FontVariantNumericOrdinal::Normal && slashedZero == FontVariantNumericSlashedZero::Normal)
        return CSSPrimitiveValue::create(CSSValueNormal);

    auto valueList = CSSValueList::createSpaceSeparated();
    switch (figure) {
    case FontVariantNumericFigure::Normal:
        break;
    case FontVariantNumericFigure::LiningNumbers:
        valueList->append(CSSPrimitiveValue::create(CSSValueLiningNums));
        break;
    case FontVariantNumericFigure::OldStyleNumbers:
        valueList->append(CSSPrimitiveValue::create(CSSValueOldstyleNums));
        break;
    }

    switch (spacing) {
    case FontVariantNumericSpacing::Normal:
        break;
    case FontVariantNumericSpacing::ProportionalNumbers:
        valueList->append(CSSPrimitiveValue::create(CSSValueProportionalNums));
        break;
    case FontVariantNumericSpacing::TabularNumbers:
        valueList->append(CSSPrimitiveValue::create(CSSValueTabularNums));
        break;
    }

    switch (fraction) {
    case FontVariantNumericFraction::Normal:
        break;
    case FontVariantNumericFraction::DiagonalFractions:
        valueList->append(CSSPrimitiveValue::create(CSSValueDiagonalFractions));
        break;
    case FontVariantNumericFraction::StackedFractions:
        valueList->append(CSSPrimitiveValue::create(CSSValueStackedFractions));
        break;
    }

    if (ordinal == FontVariantNumericOrdinal::Yes)
        valueList->append(CSSPrimitiveValue::create(CSSValueOrdinal));
    if (slashedZero == FontVariantNumericSlashedZero::Yes)
        valueList->append(CSSPrimitiveValue::create(CSSValueSlashedZero));

    return valueList;
}

static FontVariantAlternatesValues historicalFormsValues()
{
    FontVariantAlternatesValues values;
    values.historicalForms = true;
    return values;
}

static Ref<CSSValue> fontVariantAlternatesPropertyValue(FontVariantAlternates alternates)
{
    if (alternates.isNormal())
        return CSSPrimitiveValue::create(CSSValueNormal);
    if (alternates.values() == historicalFormsValues())
        return CSSPrimitiveValue::create(CSSValueHistoricalForms);

    return CSSFontVariantAlternatesValue::create(WTFMove(alternates));
}

static Ref<CSSValue> fontVariantEastAsianPropertyValue(FontVariantEastAsianVariant variant, FontVariantEastAsianWidth width, FontVariantEastAsianRuby ruby)
{
    if (variant == FontVariantEastAsianVariant::Normal && width == FontVariantEastAsianWidth::Normal && ruby == FontVariantEastAsianRuby::Normal)
        return CSSPrimitiveValue::create(CSSValueNormal);

    auto valueList = CSSValueList::createSpaceSeparated();
    switch (variant) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        valueList->append(CSSPrimitiveValue::create(CSSValueJis78));
        break;
    case FontVariantEastAsianVariant::Jis83:
        valueList->append(CSSPrimitiveValue::create(CSSValueJis83));
        break;
    case FontVariantEastAsianVariant::Jis90:
        valueList->append(CSSPrimitiveValue::create(CSSValueJis90));
        break;
    case FontVariantEastAsianVariant::Jis04:
        valueList->append(CSSPrimitiveValue::create(CSSValueJis04));
        break;
    case FontVariantEastAsianVariant::Simplified:
        valueList->append(CSSPrimitiveValue::create(CSSValueSimplified));
        break;
    case FontVariantEastAsianVariant::Traditional:
        valueList->append(CSSPrimitiveValue::create(CSSValueTraditional));
        break;
    }

    switch (width) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        valueList->append(CSSPrimitiveValue::create(CSSValueFullWidth));
        break;
    case FontVariantEastAsianWidth::Proportional:
        valueList->append(CSSPrimitiveValue::create(CSSValueProportionalWidth));
        break;
    }

    if (ruby == FontVariantEastAsianRuby::Yes)
        valueList->append(CSSPrimitiveValue::create(CSSValueRuby));

    return valueList;
}

static Ref<CSSPrimitiveValue> valueForAnimationDuration(double duration)
{
    return CSSPrimitiveValue::create(duration, CSSUnitType::CSS_S);
}

static Ref<CSSPrimitiveValue> valueForAnimationDelay(double delay)
{
    return CSSPrimitiveValue::create(delay, CSSUnitType::CSS_S);
}

static Ref<CSSPrimitiveValue> valueForAnimationIterationCount(double iterationCount)
{
    if (iterationCount == Animation::IterationCountInfinite)
        return CSSPrimitiveValue::create(CSSValueInfinite);
    return CSSPrimitiveValue::create(iterationCount, CSSUnitType::CSS_NUMBER);
}

static Ref<CSSPrimitiveValue> valueForAnimationDirection(Animation::AnimationDirection direction)
{
    switch (direction) {
    case Animation::AnimationDirectionNormal:
        return CSSPrimitiveValue::create(CSSValueNormal);
    case Animation::AnimationDirectionAlternate:
        return CSSPrimitiveValue::create(CSSValueAlternate);
    case Animation::AnimationDirectionReverse:
        return CSSPrimitiveValue::create(CSSValueReverse);
    case Animation::AnimationDirectionAlternateReverse:
        return CSSPrimitiveValue::create(CSSValueAlternateReverse);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Ref<CSSPrimitiveValue> valueForAnimationFillMode(AnimationFillMode fillMode)
{
    switch (fillMode) {
    case AnimationFillMode::None:
        return CSSPrimitiveValue::create(CSSValueNone);
    case AnimationFillMode::Forwards:
        return CSSPrimitiveValue::create(CSSValueForwards);
    case AnimationFillMode::Backwards:
        return CSSPrimitiveValue::create(CSSValueBackwards);
    case AnimationFillMode::Both:
        return CSSPrimitiveValue::create(CSSValueBoth);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Ref<CSSPrimitiveValue> valueForAnimationComposition(CompositeOperation operation)
{
    switch (operation) {
    case CompositeOperation::Add:
        return CSSPrimitiveValue::create(CSSValueAdd);
    case CompositeOperation::Accumulate:
        return CSSPrimitiveValue::create(CSSValueAccumulate);
    case CompositeOperation::Replace:
        return CSSPrimitiveValue::create(CSSValueReplace);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Ref<CSSPrimitiveValue> valueForAnimationPlayState(AnimationPlayState playState)
{
    switch (playState) {
    case AnimationPlayState::Playing:
        return CSSPrimitiveValue::create(CSSValueRunning);
    case AnimationPlayState::Paused:
        return CSSPrimitiveValue::create(CSSValuePaused);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Ref<CSSPrimitiveValue> valueForAnimationName(const Animation::Name& name)
{
    if (name.isIdentifier)
        return CSSPrimitiveValue::createCustomIdent(name.string);
    return CSSPrimitiveValue::create(name.string, CSSUnitType::CSS_STRING);
}

static Ref<CSSValue> valueForAnimationTimingFunction(const TimingFunction& timingFunction)
{
    switch (timingFunction.type()) {
    case TimingFunction::Type::CubicBezierFunction: {
        auto& function = downcast<CubicBezierTimingFunction>(timingFunction);
        if (function.timingFunctionPreset() != CubicBezierTimingFunction::TimingFunctionPreset::Custom) {
            CSSValueID valueId = CSSValueInvalid;
            switch (function.timingFunctionPreset()) {
            case CubicBezierTimingFunction::TimingFunctionPreset::Ease:
                valueId = CSSValueEase;
                break;
            case CubicBezierTimingFunction::TimingFunctionPreset::EaseIn:
                valueId = CSSValueEaseIn;
                break;
            case CubicBezierTimingFunction::TimingFunctionPreset::EaseOut:
                valueId = CSSValueEaseOut;
                break;
            case CubicBezierTimingFunction::TimingFunctionPreset::Custom:
            case CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut:
                ASSERT(function.timingFunctionPreset() == CubicBezierTimingFunction::TimingFunctionPreset::EaseInOut);
                valueId = CSSValueEaseInOut;
                break;
            }
            return CSSPrimitiveValue::create(valueId);
        }
        return CSSCubicBezierTimingFunctionValue::create(function.x1(), function.y1(), function.x2(), function.y2());
    }
    case TimingFunction::Type::StepsFunction: {
        auto& function = downcast<StepsTimingFunction>(timingFunction);
        return CSSStepsTimingFunctionValue::create(function.numberOfSteps(), function.stepPosition());
    }
    case TimingFunction::Type::SpringFunction: {
        auto& function = downcast<SpringTimingFunction>(timingFunction);
        return CSSSpringTimingFunctionValue::create(function.mass(), function.stiffness(), function.damping(), function.initialVelocity());
    }
    case TimingFunction::Type::LinearFunction:
        return CSSPrimitiveValue::create(CSSValueLinear);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void ComputedStyleExtractor::addValueForAnimationPropertyToList(CSSValueList& list, CSSPropertyID property, const Animation* animation)
{
    if (property == CSSPropertyAnimationDuration || property == CSSPropertyTransitionDuration) {
        if (!animation || !animation->isDurationFilled())
            list.append(valueForAnimationDuration(animation ? animation->duration() : Animation::initialDuration()));
    } else if (property == CSSPropertyAnimationDelay || property == CSSPropertyTransitionDelay) {
        if (!animation || !animation->isDelayFilled())
            list.append(valueForAnimationDelay(animation ? animation->delay() : Animation::initialDelay()));
    } else if (property == CSSPropertyAnimationIterationCount) {
        if (!animation || !animation->isIterationCountFilled())
            list.append(valueForAnimationIterationCount(animation ? animation->iterationCount() : Animation::initialIterationCount()));
    } else if (property == CSSPropertyAnimationDirection) {
        if (!animation || !animation->isDirectionFilled())
            list.append(valueForAnimationDirection(animation ? animation->direction() : Animation::initialDirection()));
    } else if (property == CSSPropertyAnimationFillMode) {
        if (!animation || !animation->isFillModeFilled())
            list.append(valueForAnimationFillMode(animation ? animation->fillMode() : Animation::initialFillMode()));
    } else if (property == CSSPropertyAnimationPlayState) {
        if (!animation || !animation->isPlayStateFilled())
            list.append(valueForAnimationPlayState(animation ? animation->playState() : Animation::initialPlayState()));
    } else if (property == CSSPropertyAnimationName)
        list.append(valueForAnimationName(animation ? animation->name() : Animation::initialName()));
    else if (property == CSSPropertyAnimationComposition) {
        if (!animation || !animation->isCompositeOperationFilled())
            list.append(valueForAnimationComposition(animation ? animation->compositeOperation() : Animation::initialCompositeOperation()));
    } else if (property == CSSPropertyTransitionProperty) {
        if (animation) {
            if (!animation->isPropertyFilled())
                list.append(createTransitionPropertyValue(*animation));
        } else
            list.append(CSSPrimitiveValue::create(CSSValueAll));
    } else if (property == CSSPropertyAnimationTimingFunction || property == CSSPropertyTransitionTimingFunction) {
        if (animation) {
            if (!animation->isTimingFunctionFilled())
                list.append(valueForAnimationTimingFunction(*animation->timingFunction()));
        } else
            list.append(valueForAnimationTimingFunction(CubicBezierTimingFunction::defaultTimingFunction()));
    } else
        ASSERT_NOT_REACHED();
}

static Ref<CSSValueList> valueListForAnimationOrTransitionProperty(CSSPropertyID property, const AnimationList* animationList)
{
    auto list = CSSValueList::createCommaSeparated();
    if (animationList) {
        for (const auto& animation : *animationList)
            ComputedStyleExtractor::addValueForAnimationPropertyToList(list.get(), property, animation.ptr());
    } else
        ComputedStyleExtractor::addValueForAnimationPropertyToList(list.get(), property, nullptr);
    return list;
}

static Ref<CSSValueList> animationShorthandValue(CSSPropertyID property, const AnimationList* animationList)
{
    auto parentList = CSSValueList::createCommaSeparated();

    auto addAnimation = [&](Ref<Animation> animation) {
        auto childList = CSSValueList::createSpaceSeparated();
        for (auto longhand : shorthandForProperty(property))
            ComputedStyleExtractor::addValueForAnimationPropertyToList(childList.get(), longhand, animation.ptr());
        parentList->append(childList);
    };

    if (animationList && !animationList->isEmpty()) {
        for (const auto& animation : *animationList)
            addAnimation(animation);
    } else
        addAnimation(Animation::create());

    return parentList;
}

static Ref<CSSValue> createLineBoxContainValue(OptionSet<LineBoxContain> lineBoxContain)
{
    if (!lineBoxContain)
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSLineBoxContainValue::create(lineBoxContain);
}

static Element* styleElementForNode(Node* node)
{
    if (!node)
        return nullptr;
    if (is<Element>(*node))
        return downcast<Element>(node);
    return composedTreeAncestors(*node).first();
}

static Ref<CSSValue> valueForPosition(const RenderStyle& style, const LengthPoint& position)
{
    auto list = CSSValueList::createSpaceSeparated();
    list->append(zoomAdjustedPixelValueForLength(position.x(), style));
    list->append(zoomAdjustedPixelValueForLength(position.y(), style));
    return list;
}

static bool isAuto(const LengthPoint& position)
{
    return position.x().isAuto() && position.y().isAuto();
}

static Ref<CSSValue> valueForPositionOrAuto(const RenderStyle& style, const LengthPoint& position)
{
    if (position.x().isAuto() && position.y().isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);

    return valueForPosition(style, position);
}

static CSSValueID valueIDForRaySize(RayPathOperation::Size size)
{
    switch (size) {
    case RayPathOperation::Size::ClosestCorner:
        return CSSValueClosestCorner;
    case RayPathOperation::Size::ClosestSide:
        return CSSValueClosestSide;
    case RayPathOperation::Size::FarthestCorner:
        return CSSValueFarthestCorner;
    case RayPathOperation::Size::FarthestSide:
        return CSSValueFarthestSide;
    case RayPathOperation::Size::Sides:
        return CSSValueSides;
    }

    ASSERT_NOT_REACHED();
    return CSSValueInvalid;
}

static Ref<CSSValue> valueForPathOperation(const RenderStyle& style, const PathOperation* operation, SVGPathConversion conversion = SVGPathConversion::None)
{
    if (!operation)
        return CSSPrimitiveValue::create(CSSValueNone);

    switch (operation->type()) {
    case PathOperation::Reference:
        return CSSPrimitiveValue::create(downcast<ReferencePathOperation>(*operation).url(), CSSUnitType::CSS_URI);

    case PathOperation::Shape: {
        auto list = CSSValueList::createSpaceSeparated();

        auto& shapeOperation = downcast<ShapePathOperation>(*operation);
        list->append(valueForBasicShape(style, shapeOperation.basicShape(), conversion));

        if (shapeOperation.referenceBox() != CSSBoxType::BoxMissing)
            list->append(CSSPrimitiveValue::create(shapeOperation.referenceBox()));

        return list;
    }

    case PathOperation::Box:
        return CSSPrimitiveValue::create(downcast<BoxPathOperation>(*operation).referenceBox());

    case PathOperation::Ray: {
        auto& ray = downcast<RayPathOperation>(*operation);

        auto angle = CSSPrimitiveValue::create(ray.angle(), CSSUnitType::CSS_DEG);
        auto size = CSSPrimitiveValue::create(valueIDForRaySize(ray.size()));

        return CSSRayValue::create(WTFMove(angle), WTFMove(size), ray.isContaining());
    }
    }

    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueNone);
}

static Ref<CSSValue> valueForContainIntrinsicSize(const RenderStyle& style, const ContainIntrinsicSizeType& type, const std::optional<Length> containIntrinsicLength)
{
    switch (type) {
    case ContainIntrinsicSizeType::None:
        return CSSPrimitiveValue::create(CSSValueNone);
    case ContainIntrinsicSizeType::Length: {
        ASSERT(containIntrinsicLength.has_value());
        return zoomAdjustedPixelValueForLength(containIntrinsicLength.value(), style);
    }
    case ContainIntrinsicSizeType::AutoAndLength: {
        auto autoValue = CSSPrimitiveValue::create(CSSValueAuto);
        auto list = CSSValueList::createSpaceSeparated();
        list->append(autoValue);
        ASSERT(containIntrinsicLength.has_value());
        list->append(zoomAdjustedPixelValueForLength(containIntrinsicLength.value(), style));
        return list;
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueNone);
}

ComputedStyleExtractor::ComputedStyleExtractor(Node* node, bool allowVisitedStyle, PseudoId pseudoElementSpecifier)
    : m_element(styleElementForNode(node))
    , m_pseudoElementSpecifier(pseudoElementSpecifier)
    , m_allowVisitedStyle(allowVisitedStyle)
{
}

ComputedStyleExtractor::ComputedStyleExtractor(Element* element, bool allowVisitedStyle, PseudoId pseudoElementSpecifier)
    : m_element(element)
    , m_pseudoElementSpecifier(pseudoElementSpecifier)
    , m_allowVisitedStyle(allowVisitedStyle)
{
}

RefPtr<CSSPrimitiveValue> ComputedStyleExtractor::getFontSizeCSSValuePreferringKeyword()
{
    if (!m_element)
        return nullptr;

    m_element->document().updateLayoutIgnorePendingStylesheets();

    auto* style = m_element->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return nullptr;

    if (CSSValueID sizeIdentifier = style->fontDescription().keywordSizeAsIdentifier())
        return CSSPrimitiveValue::create(sizeIdentifier);

    return zoomAdjustedPixelValue(style->fontDescription().computedSize(), *style);
}

bool ComputedStyleExtractor::useFixedFontDefaultSize()
{
    if (!m_element)
        return false;
    auto* style = m_element->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return false;

    return style->fontDescription().useFixedDefaultSize();
}

static CSSValueID identifierForFamily(const AtomString& family)
{
    if (family == cursiveFamily)
        return CSSValueCursive;
    if (family == fantasyFamily)
        return CSSValueFantasy;
    if (family == monospaceFamily)
        return CSSValueMonospace;
    if (family == pictographFamily)
        return CSSValueWebkitPictograph;
    if (family == sansSerifFamily)
        return CSSValueSansSerif;
    if (family == serifFamily)
        return CSSValueSerif;
    if (family == systemUiFamily)
        return CSSValueSystemUi;
    return CSSValueInvalid;
}

static Ref<CSSPrimitiveValue> valueForFamily(const AtomString& family)
{
    if (CSSValueID familyIdentifier = identifierForFamily(family))
        return CSSPrimitiveValue::create(familyIdentifier);
    return CSSValuePool::singleton().createFontFamilyValue(family);
}

static Ref<CSSValue> touchActionFlagsToCSSValue(OptionSet<TouchAction> touchActions)
{
    if (touchActions & TouchAction::Auto)
        return CSSPrimitiveValue::create(CSSValueAuto);
    if (touchActions & TouchAction::None)
        return CSSPrimitiveValue::create(CSSValueNone);
    if (touchActions & TouchAction::Manipulation)
        return CSSPrimitiveValue::create(CSSValueManipulation);

    auto list = CSSValueList::createSpaceSeparated();
    if (touchActions & TouchAction::PanX)
        list->append(CSSPrimitiveValue::create(CSSValuePanX));
    if (touchActions & TouchAction::PanY)
        list->append(CSSPrimitiveValue::create(CSSValuePanY));
    if (touchActions & TouchAction::PinchZoom)
        list->append(CSSPrimitiveValue::create(CSSValuePinchZoom));

    if (!list->length())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return list;
}

static Ref<CSSValue> renderTextDecorationLineFlagsToCSSValue(OptionSet<TextDecorationLine> textDecorationLine)
{
    // Blink value is ignored.
    auto list = CSSValueList::createSpaceSeparated();
    if (textDecorationLine & TextDecorationLine::Underline)
        list->append(CSSPrimitiveValue::create(CSSValueUnderline));
    if (textDecorationLine & TextDecorationLine::Overline)
        list->append(CSSPrimitiveValue::create(CSSValueOverline));
    if (textDecorationLine & TextDecorationLine::LineThrough)
        list->append(CSSPrimitiveValue::create(CSSValueLineThrough));

    if (!list->length())
        return CSSPrimitiveValue::create(CSSValueNone);
    return list;
}

static Ref<CSSValue> renderTextDecorationStyleFlagsToCSSValue(TextDecorationStyle textDecorationStyle)
{
    switch (textDecorationStyle) {
    case TextDecorationStyle::Solid:
        return CSSPrimitiveValue::create(CSSValueSolid);
    case TextDecorationStyle::Double:
        return CSSPrimitiveValue::create(CSSValueDouble);
    case TextDecorationStyle::Dotted:
        return CSSPrimitiveValue::create(CSSValueDotted);
    case TextDecorationStyle::Dashed:
        return CSSPrimitiveValue::create(CSSValueDashed);
    case TextDecorationStyle::Wavy:
        return CSSPrimitiveValue::create(CSSValueWavy);
    }

    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueInitial);
}

static RefPtr<CSSValue> renderTextDecorationSkipToCSSValue(TextDecorationSkipInk textDecorationSkipInk)
{
    switch (textDecorationSkipInk) {
    case TextDecorationSkipInk::None:
        return CSSPrimitiveValue::create(CSSValueNone);
    case TextDecorationSkipInk::Auto:
        return CSSPrimitiveValue::create(CSSValueAuto);
    case TextDecorationSkipInk::All:
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueInitial);
}

static Ref<CSSValue> textUnderlineOffsetToCSSValue(const TextUnderlineOffset& textUnderlineOffset)
{
    if (textUnderlineOffset.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    ASSERT(textUnderlineOffset.isLength());
    return CSSPrimitiveValue::create(textUnderlineOffset.lengthValue(), CSSUnitType::CSS_PX);
}

static Ref<CSSValue> textDecorationThicknessToCSSValue(const TextDecorationThickness& textDecorationThickness)
{
    if (textDecorationThickness.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    if (textDecorationThickness.isFromFont())
        return CSSPrimitiveValue::create(CSSValueFromFont);
    ASSERT(textDecorationThickness.isLength());
    return CSSPrimitiveValue::create(textDecorationThickness.lengthValue(), CSSUnitType::CSS_PX);
}

static Ref<CSSValue> renderEmphasisPositionFlagsToCSSValue(OptionSet<TextEmphasisPosition> textEmphasisPosition)
{
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Over) && (textEmphasisPosition & TextEmphasisPosition::Under)));
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Left) && (textEmphasisPosition & TextEmphasisPosition::Right)));
    ASSERT((textEmphasisPosition & TextEmphasisPosition::Over) || (textEmphasisPosition & TextEmphasisPosition::Under));

    auto list = CSSValueList::createSpaceSeparated();
    if (textEmphasisPosition & TextEmphasisPosition::Over)
        list->append(CSSPrimitiveValue::create(CSSValueOver));
    if (textEmphasisPosition & TextEmphasisPosition::Under)
        list->append(CSSPrimitiveValue::create(CSSValueUnder));
    if (textEmphasisPosition & TextEmphasisPosition::Left)
        list->append(CSSPrimitiveValue::create(CSSValueLeft));
    return list;
}

static Ref<CSSValue> valueForTextEmphasisStyle(const RenderStyle& style)
{
    switch (style.textEmphasisMark()) {
    case TextEmphasisMark::None:
        return CSSPrimitiveValue::create(CSSValueNone);
    case TextEmphasisMark::Custom:
        return CSSPrimitiveValue::create(style.textEmphasisCustomMark(), CSSUnitType::CSS_STRING);
    case TextEmphasisMark::Auto:
        ASSERT_NOT_REACHED();
#if !ASSERT_ENABLED
        FALLTHROUGH;
#endif
    case TextEmphasisMark::Dot:
    case TextEmphasisMark::Circle:
    case TextEmphasisMark::DoubleCircle:
    case TextEmphasisMark::Triangle:
    case TextEmphasisMark::Sesame:
        auto list = CSSValueList::createSpaceSeparated();
        if (style.textEmphasisFill() != TextEmphasisFill::Filled)
            list->append(CSSPrimitiveValue::create(style.textEmphasisFill()));
        list->append(CSSPrimitiveValue::create(style.textEmphasisMark()));
        return list;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Ref<CSSValue> speakAsToCSSValue(OptionSet<SpeakAs> speakAs)
{
    auto list = CSSValueList::createSpaceSeparated();
    if (speakAs & SpeakAs::SpellOut)
        list->append(CSSPrimitiveValue::create(CSSValueSpellOut));
    if (speakAs & SpeakAs::Digits)
        list->append(CSSPrimitiveValue::create(CSSValueDigits));
    if (speakAs & SpeakAs::LiteralPunctuation)
        list->append(CSSPrimitiveValue::create(CSSValueLiteralPunctuation));
    if (speakAs & SpeakAs::NoPunctuation)
        list->append(CSSPrimitiveValue::create(CSSValueNoPunctuation));
    if (!list->length())
        return CSSPrimitiveValue::create(CSSValueNormal);
    return list;
}

static Ref<CSSValue> hangingPunctuationToCSSValue(OptionSet<HangingPunctuation> hangingPunctuation)
{
    auto list = CSSValueList::createSpaceSeparated();
    if (hangingPunctuation & HangingPunctuation::First)
        list->append(CSSPrimitiveValue::create(CSSValueFirst));
    if (hangingPunctuation & HangingPunctuation::AllowEnd)
        list->append(CSSPrimitiveValue::create(CSSValueAllowEnd));
    if (hangingPunctuation & HangingPunctuation::ForceEnd)
        list->append(CSSPrimitiveValue::create(CSSValueForceEnd));
    if (hangingPunctuation & HangingPunctuation::Last)
        list->append(CSSPrimitiveValue::create(CSSValueLast));
    if (!list->length())
        return CSSPrimitiveValue::create(CSSValueNone);
    return list;
}

static Ref<CSSValue> fillRepeatToCSSValue(FillRepeatXY repeat)
{
    // For backwards compatibility, if both values are equal, just return one of them. And
    // if the two values are equivalent to repeat-x or repeat-y, just return the shorthand.
    if (repeat.x == repeat.y)
        return CSSPrimitiveValue::create(repeat.x);

    if (repeat.x == FillRepeat::Repeat && repeat.y == FillRepeat::NoRepeat)
        return CSSPrimitiveValue::create(CSSValueRepeatX);

    if (repeat.x == FillRepeat::NoRepeat && repeat.y == FillRepeat::Repeat)
        return CSSPrimitiveValue::create(CSSValueRepeatY);

    auto list = CSSValueList::createSpaceSeparated();
    list->append(CSSPrimitiveValue::create(repeat.x));
    list->append(CSSPrimitiveValue::create(repeat.y));
    return list;
}

static Ref<CSSValue> maskSourceTypeToCSSValue(MaskMode type)
{
    switch (type) {
    case MaskMode::Alpha:
        return CSSPrimitiveValue::create(CSSValueAlpha);
    case MaskMode::Luminance:
        ASSERT(type == MaskMode::Luminance);
        return CSSPrimitiveValue::create(CSSValueLuminance);
    case MaskMode::MatchSource:
        // MatchSource is only available in the mask-mode property.
        return CSSPrimitiveValue::create(CSSValueAlpha);
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueAlpha);
}

static Ref<CSSValue> maskModeToCSSValue(MaskMode type)
{
    switch (type) {
    case MaskMode::Alpha:
        return CSSPrimitiveValue::create(CSSValueAlpha);
    case MaskMode::Luminance:
        return CSSPrimitiveValue::create(CSSValueLuminance);
    case MaskMode::MatchSource:
        return CSSPrimitiveValue::create(CSSValueMatchSource);
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueMatchSource);
}

static Ref<CSSValue> fillSizeToCSSValue(CSSPropertyID propertyID, const FillSize& fillSize, const RenderStyle& style)
{
    if (fillSize.type == FillSizeType::Contain)
        return CSSPrimitiveValue::create(CSSValueContain);

    if (fillSize.type == FillSizeType::Cover)
        return CSSPrimitiveValue::create(CSSValueCover);

    if (fillSize.size.height.isAuto() && (propertyID == CSSPropertyMaskSize || fillSize.size.width.isAuto()))
        return zoomAdjustedPixelValueForLength(fillSize.size.width, style);

    auto list = CSSValueList::createSpaceSeparated();
    list->append(zoomAdjustedPixelValueForLength(fillSize.size.width, style));
    list->append(zoomAdjustedPixelValueForLength(fillSize.size.height, style));
    return list;
}

static Ref<CSSValue> altTextToCSSValue(const RenderStyle& style)
{
    return CSSPrimitiveValue::create(style.contentAltText(), CSSUnitType::CSS_STRING);
}

static Ref<CSSValueList> contentToCSSValue(const RenderStyle& style)
{
    auto list = CSSValueList::createSpaceSeparated();
    for (auto* contentData = style.contentData(); contentData; contentData = contentData->next()) {
        if (is<CounterContentData>(*contentData))
            list->append(CSSPrimitiveValue::create(downcast<CounterContentData>(*contentData).counter().identifier(), CSSUnitType::CSS_COUNTER_NAME));
        else if (is<ImageContentData>(*contentData))
            list->append(downcast<ImageContentData>(*contentData).image().computedStyleValue(style));
        else if (is<TextContentData>(*contentData))
            list->append(CSSPrimitiveValue::create(downcast<TextContentData>(*contentData).text(), CSSUnitType::CSS_STRING));
    }
    if (!list->length())
        list->append(CSSPrimitiveValue::create(style.hasEffectiveContentNone() ? CSSValueNone : CSSValueNormal));
    return list;
}

static Ref<CSSValue> counterToCSSValue(const RenderStyle& style, CSSPropertyID propertyID)
{
    auto* map = style.counterDirectives();
    if (!map)
        return CSSPrimitiveValue::create(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();
    for (auto& keyValue : *map) {
        if (auto number = (propertyID == CSSPropertyCounterIncrement ? keyValue.value.incrementValue : keyValue.value.resetValue)) {
            list->append(CSSPrimitiveValue::createCustomIdent(keyValue.key));
            list->append(CSSPrimitiveValue::create(*number, CSSUnitType::CSS_INTEGER));
        }
    }

    if (list->length())
        return list;

    return CSSPrimitiveValue::create(CSSValueNone);
}

static Ref<CSSValueList> fontFamilyList(const RenderStyle& style)
{
    auto list = CSSValueList::createCommaSeparated();
    for (unsigned i = 0; i < style.fontCascade().familyCount(); ++i)
        list->append(valueForFamily(style.fontCascade().familyAt(i)));
    return list;
}

static Ref<CSSValue> fontFamily(const RenderStyle& style)
{
    if (style.fontCascade().familyCount() == 1)
        return valueForFamily(style.fontCascade().familyAt(0));
    return fontFamilyList(style);
}

static RefPtr<CSSPrimitiveValue> optionalLineHeight(const RenderStyle& style, ComputedStyleExtractor::PropertyValueType valueType)
{
    Length length = style.lineHeight();
    if (length.isNegative())
        return nullptr;
    if (length.isPercent()) {
        // BuilderConverter::convertLineHeight() will convert a percentage value to a fixed value,
        // and a number value to a percentage value. To be able to roundtrip a number value, we thus
        // look for a percent value and convert it back to a number.
        if (valueType == ComputedStyleExtractor::PropertyValueType::Computed)
            return CSSPrimitiveValue::create(length.value() / 100, CSSUnitType::CSS_NUMBER);

        // This is imperfect, because it doesn't include the zoom factor and the real computation
        // for how high to be in pixels does include things like minimum font size and the zoom factor.
        // On the other hand, since font-size doesn't include the zoom factor, we really can't do
        // that here either.
        return zoomAdjustedPixelValue(static_cast<double>(length.percent() * style.fontDescription().computedSize()) / 100, style);
    }
    return zoomAdjustedPixelValue(floatValueForLength(length, 0), style);
}

static Ref<CSSPrimitiveValue> lineHeight(const RenderStyle& style, ComputedStyleExtractor::PropertyValueType valueType)
{
    if (auto lineHeight = optionalLineHeight(style, valueType))
        return lineHeight.releaseNonNull();
    return CSSPrimitiveValue::create(CSSValueNormal);
}

static Ref<CSSPrimitiveValue> fontSize(const RenderStyle& style)
{
    return zoomAdjustedPixelValue(style.fontDescription().computedSize(), style);
}

static Ref<CSSPrimitiveValue> fontPalette(const RenderStyle& style)
{
    auto fontPalette = style.fontDescription().fontPalette();
    switch (fontPalette.type) {
    case FontPalette::Type::Normal:
        return CSSPrimitiveValue::create(CSSValueNormal);
    case FontPalette::Type::Light:
        return CSSPrimitiveValue::create(CSSValueLight);
    case FontPalette::Type::Dark:
        return CSSPrimitiveValue::create(CSSValueDark);
    case FontPalette::Type::Custom:
        return CSSPrimitiveValue::createCustomIdent(fontPalette.identifier);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Ref<CSSPrimitiveValue> fontWeight(FontSelectionValue weight)
{
    return CSSPrimitiveValue::create(static_cast<float>(weight), CSSUnitType::CSS_NUMBER);
}

static Ref<CSSPrimitiveValue> fontWeight(const RenderStyle& style)
{
    return fontWeight(style.fontDescription().weight());
}

static Ref<CSSPrimitiveValue> fontStretch(FontSelectionValue stretch)
{
    return CSSPrimitiveValue::create(static_cast<float>(stretch), CSSUnitType::CSS_PERCENTAGE);
}

static Ref<CSSPrimitiveValue> fontStretch(const RenderStyle& style)
{
    return fontStretch(style.fontDescription().stretch());
}

static Ref<CSSValue> fontStyle(std::optional<FontSelectionValue> italic, FontStyleAxis axis)
{
    if (auto keyword = fontStyleKeyword(italic, axis))
        return CSSPrimitiveValue::create(keyword.value());
    float angle = *italic;
    return CSSFontStyleWithAngleValue::create(CSSPrimitiveValue::create(angle, CSSUnitType::CSS_DEG));
}

static Ref<CSSValue> fontStyle(const RenderStyle& style)
{
    return fontStyle(style.fontDescription().italic(), style.fontDescription().fontStyleAxis());
}

Ref<CSSValue> ComputedStyleExtractor::fontVariantShorthandValue()
{
    auto list = CSSValueList::createSpaceSeparated();
    for (auto longhand : fontVariantShorthand()) {
        auto value = propertyValue(longhand, UpdateLayout::No);
        if (isValueID(value, CSSValueNormal))
            continue;
        list->append(value.releaseNonNull());
    }
    if (!list->length())
        return CSSPrimitiveValue::create(CSSValueNormal);
    return list;
}

static Ref<CSSValue> fontSynthesis(const RenderStyle& style)
{
    auto list = CSSValueList::createSpaceSeparated();
    if (style.fontDescription().hasAutoFontSynthesisWeight())
        list->append(CSSPrimitiveValue::create(CSSValueWeight));
    if (style.fontDescription().hasAutoFontSynthesisStyle())
        list->append(CSSPrimitiveValue::create(CSSValueStyle));
    if (style.fontDescription().hasAutoFontSynthesisSmallCaps())
        list->append(CSSPrimitiveValue::create(CSSValueSmallCaps));
    if (!list->length())
        return CSSPrimitiveValue::create(CSSValueNone);
    return list;
}

static Ref<CSSValue> fontSynthesisLonghandToCSSValue(FontSynthesisLonghandValue value)
{
    return CSSPrimitiveValue::create(value == FontSynthesisLonghandValue::Auto ? CSSValueAuto : CSSValueNone);
}

static Ref<CSSValue> fontSynthesisWeight(const RenderStyle& style)
{
    return fontSynthesisLonghandToCSSValue(style.fontDescription().fontSynthesisWeight());
}

static Ref<CSSValue> fontSynthesisStyle(const RenderStyle& style)
{
    return fontSynthesisLonghandToCSSValue(style.fontDescription().fontSynthesisStyle());
}

static Ref<CSSValue> fontSynthesisSmallCaps(const RenderStyle& style)
{
    return fontSynthesisLonghandToCSSValue(style.fontDescription().fontSynthesisSmallCaps());
}

typedef const Length& (RenderStyle::*RenderStyleLengthGetter)() const;
typedef LayoutUnit (RenderBoxModelObject::*RenderBoxComputedCSSValueGetter)() const;

template<RenderStyleLengthGetter lengthGetter, RenderBoxComputedCSSValueGetter computedCSSValueGetter>
static RefPtr<CSSValue> zoomAdjustedPaddingOrMarginPixelValue(const RenderStyle& style, RenderObject* renderer)
{
    Length unzoomzedLength = (style.*lengthGetter)();
    if (!is<RenderBox>(renderer) || unzoomzedLength.isFixed())
        return zoomAdjustedPixelValueForLength(unzoomzedLength, style);
    return zoomAdjustedPixelValue((downcast<RenderBox>(*renderer).*computedCSSValueGetter)(), style);
}

template<RenderStyleLengthGetter lengthGetter>
static bool paddingOrMarginIsRendererDependent(const RenderStyle* style, RenderObject* renderer)
{
    return renderer && style && renderer->isBox() && !(style->*lengthGetter)().isFixed();
}

static bool positionOffsetValueIsRendererDependent(const RenderStyle* style, RenderObject* renderer)
{
    return renderer && style && renderer->isBox();
}

static CSSValueID convertToPageBreak(BreakBetween value)
{
    if (value == BreakBetween::Page || value == BreakBetween::LeftPage || value == BreakBetween::RightPage
        || value == BreakBetween::RectoPage || value == BreakBetween::VersoPage)
        return CSSValueAlways; // CSS 2.1 allows us to map these to always.
    if (value == BreakBetween::Avoid || value == BreakBetween::AvoidPage)
        return CSSValueAvoid;
    return CSSValueAuto;
}

static CSSValueID convertToColumnBreak(BreakBetween value)
{
    if (value == BreakBetween::Column)
        return CSSValueAlways;
    if (value == BreakBetween::Avoid || value == BreakBetween::AvoidColumn)
        return CSSValueAvoid;
    return CSSValueAuto;
}

static CSSValueID convertToPageBreak(BreakInside value)
{
    if (value == BreakInside::Avoid || value == BreakInside::AvoidPage)
        return CSSValueAvoid;
    return CSSValueAuto;
}

static CSSValueID convertToColumnBreak(BreakInside value)
{
    if (value == BreakInside::Avoid || value == BreakInside::AvoidColumn)
        return CSSValueAvoid;
    return CSSValueAuto;
}

static inline bool isNonReplacedInline(RenderObject& renderer)
{
    return renderer.isInline() && !renderer.isReplacedOrInlineBlock();
}

static bool isLayoutDependent(CSSPropertyID propertyID, const RenderStyle* style, RenderObject* renderer)
{
    switch (propertyID) {
    case CSSPropertyTop:
    case CSSPropertyBottom:
    case CSSPropertyLeft:
    case CSSPropertyRight:
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetBlockEnd:
    case CSSPropertyInsetInlineStart:
    case CSSPropertyInsetInlineEnd:
        return positionOffsetValueIsRendererDependent(style, renderer);
    case CSSPropertyWidth:
    case CSSPropertyHeight:
    case CSSPropertyInlineSize:
    case CSSPropertyBlockSize:
        return renderer && !renderer->isRenderOrLegacyRenderSVGModelObject() && !isNonReplacedInline(*renderer);
    case CSSPropertyPerspectiveOrigin:
    case CSSPropertyTransformOrigin:
    case CSSPropertyTransform:
    case CSSPropertyFilter: // Why are filters layout-dependent?
#if ENABLE(FILTERS_LEVEL_2)
    case CSSPropertyWebkitBackdropFilter: // Ditto for backdrop-filter.
#endif
        return true;
    case CSSPropertyMargin: {
        if (!renderer || !renderer->isBox())
            return false;
        return !(style && style->marginTop().isFixed() && style->marginRight().isFixed()
            && style->marginBottom().isFixed() && style->marginLeft().isFixed());
    }
    case CSSPropertyMarginTop:
        return paddingOrMarginIsRendererDependent<&RenderStyle::marginTop>(style, renderer);
    case CSSPropertyMarginRight:
        return paddingOrMarginIsRendererDependent<&RenderStyle::marginRight>(style, renderer);
    case CSSPropertyMarginBottom:
        return paddingOrMarginIsRendererDependent<&RenderStyle::marginBottom>(style, renderer);
    case CSSPropertyMarginLeft:
        return paddingOrMarginIsRendererDependent<&RenderStyle::marginLeft>(style, renderer);
    case CSSPropertyPadding: {
        if (!renderer || !renderer->isBox())
            return false;
        return !(style && style->paddingTop().isFixed() && style->paddingRight().isFixed()
            && style->paddingBottom().isFixed() && style->paddingLeft().isFixed());
    }
    case CSSPropertyPaddingTop:
        return paddingOrMarginIsRendererDependent<&RenderStyle::paddingTop>(style, renderer);
    case CSSPropertyPaddingRight:
        return paddingOrMarginIsRendererDependent<&RenderStyle::paddingRight>(style, renderer);
    case CSSPropertyPaddingBottom:
        return paddingOrMarginIsRendererDependent<&RenderStyle::paddingBottom>(style, renderer);
    case CSSPropertyPaddingLeft:
        return paddingOrMarginIsRendererDependent<&RenderStyle::paddingLeft>(style, renderer);
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
    case CSSPropertyGridTemplate:
    case CSSPropertyGrid:
        return renderer && renderer->isRenderGrid();
    default:
        return false;
    }
}

RenderElement* ComputedStyleExtractor::styledRenderer() const
{
    if (!m_element)
        return nullptr;
    if (m_pseudoElementSpecifier != PseudoId::None)
        return Styleable(*m_element, m_pseudoElementSpecifier).renderer();
    if (m_element->hasDisplayContents())
        return nullptr;
    return m_element->renderer();
}

static inline bool hasValidStyleForProperty(Element& element, CSSPropertyID propertyID)
{
    if (element.styleValidity() != Style::Validity::Valid)
        return false;
    if (element.document().hasPendingFullStyleRebuild())
        return false;
    if (!element.document().childNeedsStyleRecalc())
        return true;

    if (auto* keyframeEffectStack = Styleable(element, PseudoId::None).keyframeEffectStack()) {
        if (keyframeEffectStack->containsProperty(propertyID))
            return false;
    }

    auto isQueryContainer = [&](Element& element) {
        auto* style = element.renderStyle();
        return style && style->containerType() != ContainerType::Normal;
    };

    if (isQueryContainer(element))
        return false;

    const auto* currentElement = &element;
    for (auto& ancestor : composedTreeAncestors(element)) {
        if (ancestor.styleValidity() != Style::Validity::Valid)
            return false;

        if (isQueryContainer(ancestor))
            return false;

        if (ancestor.directChildNeedsStyleRecalc() && currentElement->styleIsAffectedByPreviousSibling())
            return false;

        currentElement = &ancestor;
    }

    return true;
}

bool ComputedStyleExtractor::updateStyleIfNeededForProperty(Element& element, CSSPropertyID propertyID)
{
    auto& document = element.document();

    document.styleScope().flushPendingUpdate();

    auto hasValidStyle = [&] {
        auto shorthand = shorthandForProperty(propertyID);
        if (shorthand.length()) {
            for (auto longhand : shorthand) {
                if (!hasValidStyleForProperty(element, longhand))
                    return false;
            }
            return true;
        }
        return hasValidStyleForProperty(element, propertyID);
    }();

    if (hasValidStyle)
        return false;

    document.updateStyleIfNeeded();
    return true;
}

static inline const RenderStyle* computeRenderStyleForProperty(Element& element, PseudoId pseudoElementSpecifier, CSSPropertyID propertyID, std::unique_ptr<RenderStyle>& ownedStyle, WeakPtr<RenderElement> renderer)
{
    if (!renderer)
        renderer = element.renderer();

    if (renderer && renderer->isComposited() && CSSPropertyAnimation::animationOfPropertyIsAccelerated(propertyID)) {
        ownedStyle = renderer->animatedStyle();
        if (pseudoElementSpecifier != PseudoId::None) {
            // FIXME: This cached pseudo style will only exist if the animation has been run at least once.
            return ownedStyle->getCachedPseudoStyle(pseudoElementSpecifier);
        }
        return ownedStyle.get();
    }

    return element.computedStyle(pseudoElementSpecifier);
}

static Ref<CSSValue> shapePropertyValue(const RenderStyle& style, const ShapeValue* shapeValue)
{
    if (!shapeValue)
        return CSSPrimitiveValue::create(CSSValueNone);

    if (shapeValue->type() == ShapeValue::Type::Box)
        return CSSPrimitiveValue::create(shapeValue->cssBox());

    if (shapeValue->type() == ShapeValue::Type::Image) {
        if (shapeValue->image())
            return shapeValue->image()->computedStyleValue(style);
        return CSSPrimitiveValue::create(CSSValueNone);
    }

    ASSERT(shapeValue->type() == ShapeValue::Type::Shape);

    auto list = CSSValueList::createSpaceSeparated();
    list->append(valueForBasicShape(style, *shapeValue->shape()));
    if (shapeValue->cssBox() != CSSBoxType::BoxMissing)
        list->append(CSSPrimitiveValue::create(shapeValue->cssBox()));
    return list;
}

static Ref<CSSValueList> valueForItemPositionWithOverflowAlignment(const StyleSelfAlignmentData& data)
{
    auto result = CSSValueList::createSpaceSeparated();
    if (data.positionType() == ItemPositionType::Legacy)
        result->append(CSSPrimitiveValue::create(CSSValueLegacy));
    if (data.position() == ItemPosition::Baseline)
        result->append(CSSPrimitiveValue::create(CSSValueBaseline));
    else if (data.position() == ItemPosition::LastBaseline) {
        result->append(CSSPrimitiveValue::create(CSSValueLast));
        result->append(CSSPrimitiveValue::create(CSSValueBaseline));
    } else {
        if (data.position() >= ItemPosition::Center && data.overflow() != OverflowAlignment::Default)
            result->append(CSSPrimitiveValue::create(data.overflow()));
        if (data.position() == ItemPosition::Legacy)
            result->append(CSSPrimitiveValue::create(CSSValueNormal));
        else
            result->append(CSSPrimitiveValue::create(data.position()));
    }
    ASSERT(result->length() <= 2);
    return result;
}

static Ref<CSSValueList> valueForContentPositionAndDistributionWithOverflowAlignment(const StyleContentAlignmentData& data)
{
    auto result = CSSValueList::createSpaceSeparated();
    // Handle content-distribution values
    if (data.distribution() != ContentDistribution::Default)
        result->append(CSSPrimitiveValue::create(data.distribution()));

    // Handle content-position values (either as fallback or actual value)
    switch (data.position()) {
    case ContentPosition::Normal:
        // Handle 'normal' value, not valid as content-distribution fallback.
        if (data.distribution() == ContentDistribution::Default)
            result->append(CSSPrimitiveValue::create(CSSValueNormal));
        break;
    case ContentPosition::LastBaseline:
        result->append(CSSPrimitiveValue::create(CSSValueLast));
        result->append(CSSPrimitiveValue::create(CSSValueBaseline));
        break;
    default:
        // Handle overflow-alignment (only allowed for content-position values)
        if ((data.position() >= ContentPosition::Center || data.distribution() != ContentDistribution::Default) && data.overflow() != OverflowAlignment::Default)
            result->append(CSSPrimitiveValue::create(data.overflow()));
        result->append(CSSPrimitiveValue::create(data.position()));
    }

    ASSERT(result->length() > 0);
    ASSERT(result->length() <= 3);
    return result;
}

static Ref<CSSValueList> valueForOffsetRotate(const OffsetRotation& rotation)
{
    auto result = CSSValueList::createSpaceSeparated();

    if (rotation.hasAuto())
        result->append(CSSPrimitiveValue::create(CSSValueAuto));
    result->append(CSSPrimitiveValue::create(rotation.angle(), CSSUnitType::CSS_DEG));

    return result;
}

static Ref<CSSValue> valueForOffsetShorthand(const RenderStyle& style)
{
    // [ <'offset-position'>? [ <'offset-path'> [ <'offset-distance'> || <'offset-rotate'> ]? ]? ]! [ / <'offset-anchor'> ]?

    // The first four elements are serialized in a space separated CSSValueList.
    // This is then combined with offset-anchor in a slash separated CSSValueList.

    auto innerList = CSSValueList::createSpaceSeparated();

    if (!isAuto(style.offsetPosition()))
        innerList->append(valueForPositionOrAuto(style, style.offsetPosition()));

    bool nonInitialDistance = !style.offsetDistance().isZero();
    bool nonInitialRotate = style.offsetRotate() != style.initialOffsetRotate();

    if (style.offsetPath() || nonInitialDistance || nonInitialRotate)
        innerList->append(valueForPathOperation(style, style.offsetPath(), SVGPathConversion::ForceAbsolute));

    if (nonInitialDistance)
        innerList->append(CSSPrimitiveValue::create(style.offsetDistance(), style));
    if (nonInitialRotate)
        innerList->append(valueForOffsetRotate(style.offsetRotate()));

    auto inner = innerList->length()
        ? Ref<CSSValue> { WTFMove(innerList) }
        : Ref<CSSValue> { CSSPrimitiveValue::create(CSSValueAuto) };

    if (isAuto(style.offsetAnchor()))
        return inner;

    auto outerList = CSSValueList::createSlashSeparated();
    outerList->append(WTFMove(inner));
    outerList->append(valueForPosition(style, style.offsetAnchor()));
    return outerList;
}

static Ref<CSSValue> paintOrder(PaintOrder paintOrder)
{
    if (paintOrder == PaintOrder::Normal)
        return CSSPrimitiveValue::create(CSSValueNormal);

    auto paintOrderList = CSSValueList::createSpaceSeparated();
    switch (paintOrder) {
    case PaintOrder::Normal:
        ASSERT_NOT_REACHED();
        break;
    case PaintOrder::Fill:
        paintOrderList->append(CSSPrimitiveValue::create(CSSValueFill));
        break;
    case PaintOrder::FillMarkers:
        paintOrderList->append(CSSPrimitiveValue::create(CSSValueFill));
        paintOrderList->append(CSSPrimitiveValue::create(CSSValueMarkers));
        break;
    case PaintOrder::Stroke:
        paintOrderList->append(CSSPrimitiveValue::create(CSSValueStroke));
        break;
    case PaintOrder::StrokeMarkers:
        paintOrderList->append(CSSPrimitiveValue::create(CSSValueStroke));
        paintOrderList->append(CSSPrimitiveValue::create(CSSValueMarkers));
        break;
    case PaintOrder::Markers:
        paintOrderList->append(CSSPrimitiveValue::create(CSSValueMarkers));
        break;
    case PaintOrder::MarkersStroke:
        paintOrderList->append(CSSPrimitiveValue::create(CSSValueMarkers));
        paintOrderList->append(CSSPrimitiveValue::create(CSSValueStroke));
        break;
    }
    return paintOrderList;
}

static inline bool isFlexOrGridItem(RenderObject* renderer)
{
    if (!renderer || !renderer->isBox())
        return false;
    auto& box = downcast<RenderBox>(*renderer);
    return box.isFlexItem() || box.isGridItem();
}

RefPtr<CSSValue> ComputedStyleExtractor::customPropertyValue(const AtomString& propertyName)
{
    Element* styledElement = m_element.get();
    if (!styledElement)
        return nullptr;

    updateStyleIfNeededForProperty(*styledElement, CSSPropertyCustom);

    std::unique_ptr<RenderStyle> ownedStyle;
    auto* style = computeRenderStyleForProperty(*styledElement, m_pseudoElementSpecifier, CSSPropertyCustom, ownedStyle, nullptr);
    if (!style)
        return nullptr;

    auto* value = style->customPropertyValue(propertyName, styledElement->document().customPropertyRegistry());

    return const_cast<CSSCustomPropertyValue*>(value);
}

String ComputedStyleExtractor::customPropertyText(const AtomString& propertyName)
{
    RefPtr<CSSValue> propertyValue = customPropertyValue(propertyName);
    return propertyValue ? propertyValue->cssText() : emptyString();
}

static Ref<CSSFontValue> fontShorthandValue(const RenderStyle& style, ComputedStyleExtractor::PropertyValueType valueType)
{
    auto& description = style.fontDescription();
    auto fontStretch = fontStretchKeyword(description.stretch());
    auto fontStyle = fontStyleKeyword(description.italic(), description.fontStyleAxis());

    auto propertiesResetByShorthandAreExpressible = [&] {
        // The font shorthand can express "font-variant-caps: small-caps". Overwrite with "normal" so we can use isAllNormal to check that all the other settings are normal.
        auto variantSettingsOmittingExpressible = description.variantSettings();
        if (variantSettingsOmittingExpressible.caps == FontVariantCaps::Small)
            variantSettingsOmittingExpressible.caps = FontVariantCaps::Normal;

        // When we add font-language-override, also add code to check for non-expressible values for it here.
        return variantSettingsOmittingExpressible.isAllNormal()
            && fontStretch
            && fontStyle
            && !description.fontSizeAdjust()
            && description.kerning() == Kerning::Auto
            && description.featureSettings().isEmpty()
            && description.opticalSizing() == FontOpticalSizing::Enabled
            && description.variationSettings().isEmpty();
    };

    auto computedFont = CSSFontValue::create();

    if (!propertiesResetByShorthandAreExpressible())
        return computedFont;

    if (description.variantCaps() == FontVariantCaps::Small)
        computedFont->variant = CSSPrimitiveValue::create(CSSValueSmallCaps);
    if (float weight = description.weight(); weight != 400)
        computedFont->weight = CSSPrimitiveValue::create(weight, CSSUnitType::CSS_NUMBER);
    if (*fontStretch != CSSValueNormal)
        computedFont->stretch = CSSPrimitiveValue::create(*fontStretch);
    if (*fontStyle != CSSValueNormal)
        computedFont->style = CSSPrimitiveValue::create(*fontStyle);
    computedFont->size = fontSize(style);
    computedFont->lineHeight = optionalLineHeight(style, valueType);
    computedFont->family = fontFamilyList(style);

    return computedFont;
}

RefPtr<CSSValue> ComputedStyleExtractor::propertyValue(CSSPropertyID propertyID, UpdateLayout updateLayout, PropertyValueType valueType)
{
    auto* styledElement = m_element.get();
    if (!styledElement)
        return nullptr;

    if (!isExposed(propertyID, m_element->document().settings())) {
        // Exit quickly, and avoid us ever having to update layout in this case.
        return nullptr;
    }

    std::unique_ptr<RenderStyle> ownedStyle;
    const RenderStyle* style = nullptr;
    bool forceFullLayout = false;
    if (updateLayout == UpdateLayout::Yes) {
        Document& document = m_element->document();

        updateStyleIfNeededForProperty(*styledElement, propertyID);
        if (propertyID == CSSPropertyDisplay && !styledRenderer() && is<SVGElement>(*styledElement) && !downcast<SVGElement>(*styledElement).isValid())
            return nullptr;

        style = computeRenderStyleForProperty(*styledElement, m_pseudoElementSpecifier, propertyID, ownedStyle, styledRenderer());

        forceFullLayout = [&] {
            // FIXME: Some of these cases could be narrowed down or optimized better.
            if (isLayoutDependent(propertyID, style, styledRenderer()))
                return true;
            // FIXME: Why?
            if (styledElement->isInShadowTree())
                return true;
            if (!document.ownerElement())
                return false;
            if (!document.styleScope().resolverIfExists())
                return false;
            auto& ruleSets = document.styleScope().resolverIfExists()->ruleSets();
            return ruleSets.hasViewportDependentMediaQueries() || ruleSets.hasContainerQueries();
        }();

        if (forceFullLayout)
            document.updateLayoutIgnorePendingStylesheets();
    }

    if (updateLayout == UpdateLayout::No || forceFullLayout)
        style = computeRenderStyleForProperty(*styledElement, m_pseudoElementSpecifier, propertyID, ownedStyle, styledRenderer());

    if (!style)
        return nullptr;

    return valueForPropertyInStyle(*style, propertyID, valueType == PropertyValueType::Resolved ? styledRenderer() : nullptr, valueType);
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForPropertyInStyle(const RenderStyle& style, CSSPropertyID propertyID, RenderElement* renderer, PropertyValueType valueType)
{
    auto& cssValuePool = CSSValuePool::singleton();
    propertyID = CSSProperty::resolveDirectionAwareProperty(propertyID, style.direction(), style.writingMode());

    ASSERT(isExposed(propertyID, m_element->document().settings()));

    switch (propertyID) {
    case CSSPropertyInvalid:
        return nullptr;
    case CSSPropertyAccentColor: {
        if (style.hasAutoAccentColor())
            return CSSPrimitiveValue::create(CSSValueAuto);
        return currentColorOrValidColor(style, style.accentColor());
    }
    case CSSPropertyAlignTracks:
    case CSSPropertyJustifyTracks: {
        auto list = CSSValueList::createCommaSeparated();
        auto alignments = (propertyID == CSSPropertyAlignTracks) ? style.alignTracks() : style.justifyTracks();

        for (auto alignment : alignments)
            list->append(valueForContentPositionAndDistributionWithOverflowAlignment(alignment));

        return list;
    }
    case CSSPropertyBackgroundColor:
        return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyBackgroundColor)) : currentColorOrValidColor(style, style.backgroundColor());
    case CSSPropertyBackgroundImage:
    case CSSPropertyMaskImage: {
        auto& layers = propertyID == CSSPropertyMaskImage ? style.maskLayers() : style.backgroundLayers();
        if (!layers.next()) {
            if (layers.image())
                return layers.image()->computedStyleValue(style);
            return CSSPrimitiveValue::create(CSSValueNone);
        }
        auto list = CSSValueList::createCommaSeparated();
        for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next()) {
            if (currLayer->image())
                list->append(currLayer->image()->computedStyleValue(style));
            else
                list->append(CSSPrimitiveValue::create(CSSValueNone));
        }
        return list;
    }
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyMaskSize: {
        auto& layers = propertyID == CSSPropertyMaskSize ? style.maskLayers() : style.backgroundLayers();
        if (!layers.next())
            return fillSizeToCSSValue(propertyID, layers.size(), style);
        auto list = CSSValueList::createCommaSeparated();
        for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
            list->append(fillSizeToCSSValue(propertyID, currLayer->size(), style));
        return list;
    }
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyMaskRepeat: {
        auto& layers = propertyID == CSSPropertyMaskRepeat ? style.maskLayers() : style.backgroundLayers();
        if (!layers.next())
            return fillRepeatToCSSValue(layers.repeat());
        auto list = CSSValueList::createCommaSeparated();
        for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
            list->append(fillRepeatToCSSValue(currLayer->repeat()));
        return list;
    }
    case CSSPropertyWebkitMaskSourceType: {
        auto& layers = style.maskLayers();
        if (!layers.next())
            return maskSourceTypeToCSSValue(layers.maskMode());
        auto list = CSSValueList::createCommaSeparated();
        for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
            list->append(maskSourceTypeToCSSValue(currLayer->maskMode()));
        return list;
    }
    case CSSPropertyMaskMode: {
        auto& layers = style.maskLayers();
        if (!layers.next())
            return maskModeToCSSValue(layers.maskMode());
        auto list = CSSValueList::createCommaSeparated();
        for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
            list->append(maskModeToCSSValue(currLayer->maskMode()));
        return list;
    }
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyMaskComposite: {
        auto& layers = style.maskLayers();
        if (!layers.next())
            return CSSPrimitiveValue::create(toCSSValueID(layers.composite(), propertyID));
        auto list = CSSValueList::createCommaSeparated();
        for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
            list->append(CSSPrimitiveValue::create(toCSSValueID(currLayer->composite(), propertyID)));
        return list;
    }
    case CSSPropertyBackgroundAttachment: {
        auto& layers = style.backgroundLayers();
        if (!layers.next())
            return CSSPrimitiveValue::create(layers.attachment());
        auto list = CSSValueList::createCommaSeparated();
        for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
            list->append(CSSPrimitiveValue::create(currLayer->attachment()));
        return list;
    }
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyMaskClip:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyMaskOrigin: {
        auto& layers = (propertyID == CSSPropertyMaskClip || propertyID == CSSPropertyWebkitMaskClip || propertyID == CSSPropertyMaskOrigin) ? style.maskLayers() : style.backgroundLayers();
        bool isClip = propertyID == CSSPropertyBackgroundClip || propertyID == CSSPropertyWebkitBackgroundClip || propertyID == CSSPropertyMaskClip || propertyID == CSSPropertyWebkitMaskClip;
        if (!layers.next())
            return CSSPrimitiveValue::create(isClip ? layers.clip() : layers.origin());
        auto list = CSSValueList::createCommaSeparated();
        for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
            list->append(CSSPrimitiveValue::create(isClip ? currLayer->clip() : currLayer->origin()));
        return list;
    }
    case CSSPropertyBackgroundPosition:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyMaskPosition: {
        auto& layers = propertyID == CSSPropertyBackgroundPosition ? style.backgroundLayers() : style.maskLayers();
        if (!layers.next())
            return createPositionListForLayer(propertyID, layers, style);

        auto list = CSSValueList::createCommaSeparated();
        for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
            list->append(createPositionListForLayer(propertyID, *currLayer, style));
        return list;
    }
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyWebkitMaskPositionX: {
        auto& layers = propertyID == CSSPropertyWebkitMaskPositionX ? style.maskLayers() : style.backgroundLayers();
        if (!layers.next())
            return createSingleAxisPositionValueForLayer(propertyID, layers, style);

        auto list = CSSValueList::createCommaSeparated();
        for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
            list->append(createSingleAxisPositionValueForLayer(propertyID, *currLayer, style));

        return list;
    }
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionY: {
        auto& layers = propertyID == CSSPropertyWebkitMaskPositionY ? style.maskLayers() : style.backgroundLayers();
        if (!layers.next())
            return createSingleAxisPositionValueForLayer(propertyID, layers, style);

        auto list = CSSValueList::createCommaSeparated();
        for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
            list->append(createSingleAxisPositionValueForLayer(propertyID, *currLayer, style));

        return list;
    }
    case CSSPropertyBorderCollapse:
        if (style.borderCollapse() == BorderCollapse::Collapse)
            return CSSPrimitiveValue::create(CSSValueCollapse);
        return CSSPrimitiveValue::create(CSSValueSeparate);
    case CSSPropertyBorderSpacing: {
        auto list = CSSValueList::createSpaceSeparated();
        list->append(zoomAdjustedPixelValue(style.horizontalBorderSpacing(), style));
        list->append(zoomAdjustedPixelValue(style.verticalBorderSpacing(), style));
        return list;
    }
    case CSSPropertyWebkitBorderHorizontalSpacing:
        return zoomAdjustedPixelValue(style.horizontalBorderSpacing(), style);
    case CSSPropertyWebkitBorderVerticalSpacing:
        return zoomAdjustedPixelValue(style.verticalBorderSpacing(), style);
    case CSSPropertyBorderImageSource:
        if (style.borderImageSource())
            return style.borderImageSource()->computedStyleValue(style);
        return CSSPrimitiveValue::create(CSSValueNone);
    case CSSPropertyBorderTopColor:
        return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyBorderTopColor)) : currentColorOrValidColor(style, style.borderTopColor());
    case CSSPropertyBorderRightColor:
        return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyBorderRightColor)) : currentColorOrValidColor(style, style.borderRightColor());
    case CSSPropertyBorderBottomColor:
        return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyBorderBottomColor)) : currentColorOrValidColor(style, style.borderBottomColor());
    case CSSPropertyBorderLeftColor:
        return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyBorderLeftColor)) : currentColorOrValidColor(style, style.borderLeftColor());
    case CSSPropertyBorderTopStyle:
        return CSSPrimitiveValue::create(style.borderTopStyle());
    case CSSPropertyBorderRightStyle:
        return CSSPrimitiveValue::create(style.borderRightStyle());
    case CSSPropertyBorderBottomStyle:
        return CSSPrimitiveValue::create(style.borderBottomStyle());
    case CSSPropertyBorderLeftStyle:
        return CSSPrimitiveValue::create(style.borderLeftStyle());
    case CSSPropertyBorderTopWidth:
        return zoomAdjustedPixelValue(style.borderTopWidth(), style);
    case CSSPropertyBorderRightWidth:
        return zoomAdjustedPixelValue(style.borderRightWidth(), style);
    case CSSPropertyBorderBottomWidth:
        return zoomAdjustedPixelValue(style.borderBottomWidth(), style);
    case CSSPropertyBorderLeftWidth:
        return zoomAdjustedPixelValue(style.borderLeftWidth(), style);
    case CSSPropertyBottom:
        return positionOffsetValue(style, CSSPropertyBottom, renderer);
    case CSSPropertyWebkitBoxAlign:
        return CSSPrimitiveValue::create(style.boxAlign());
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    case CSSPropertyWebkitBoxDecorationBreak:
        if (style.boxDecorationBreak() == BoxDecorationBreak::Slice)
            return CSSPrimitiveValue::create(CSSValueSlice);
    return CSSPrimitiveValue::create(CSSValueClone);
#endif
    case CSSPropertyWebkitBoxDirection:
        return CSSPrimitiveValue::create(style.boxDirection());
    case CSSPropertyWebkitBoxFlex:
        return CSSPrimitiveValue::create(style.boxFlex(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyWebkitBoxFlexGroup:
        return CSSPrimitiveValue::create(style.boxFlexGroup(), CSSUnitType::CSS_INTEGER);
    case CSSPropertyWebkitBoxLines:
        return CSSPrimitiveValue::create(style.boxLines());
    case CSSPropertyWebkitBoxOrdinalGroup:
        return CSSPrimitiveValue::create(style.boxOrdinalGroup(), CSSUnitType::CSS_INTEGER);
    case CSSPropertyWebkitBoxOrient:
        return CSSPrimitiveValue::create(style.boxOrient());
    case CSSPropertyWebkitBoxPack:
        return CSSPrimitiveValue::create(style.boxPack());
    case CSSPropertyWebkitBoxReflect:
        return valueForReflection(style.boxReflect(), style);
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow:
        return valueForShadow(style.boxShadow(), propertyID, style);
    case CSSPropertyCaptionSide:
        return CSSPrimitiveValue::create(style.captionSide());
    case CSSPropertyCaretColor:
        return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyCaretColor)) : currentColorOrValidColor(style, style.caretColor());
    case CSSPropertyClear:
        return CSSPrimitiveValue::create(style.clear());
    case CSSPropertyLeadingTrim:
        return CSSPrimitiveValue::create(style.leadingTrim());
    case CSSPropertyColor:
        return cssValuePool.createColorValue(m_allowVisitedStyle ? style.visitedDependentColor(CSSPropertyColor) : style.color());
    case CSSPropertyPrintColorAdjust:
        return CSSPrimitiveValue::create(style.printColorAdjust());
    case CSSPropertyWebkitColumnAxis:
        return CSSPrimitiveValue::create(style.columnAxis());
    case CSSPropertyColumnCount:
        if (style.hasAutoColumnCount())
            return CSSPrimitiveValue::create(CSSValueAuto);
        return CSSPrimitiveValue::create(style.columnCount(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyColumnFill:
        return CSSPrimitiveValue::create(style.columnFill());
    case CSSPropertyColumnGap:
        if (style.columnGap().isNormal())
            return CSSPrimitiveValue::create(CSSValueNormal);
        return zoomAdjustedPixelValueForLength(style.columnGap().length(), style);
    case CSSPropertyRowGap:
        if (style.rowGap().isNormal())
            return CSSPrimitiveValue::create(CSSValueNormal);
        return zoomAdjustedPixelValueForLength(style.rowGap().length(), style);
    case CSSPropertyWebkitColumnProgression:
        return CSSPrimitiveValue::create(style.columnProgression());
    case CSSPropertyColumnRuleColor:
        return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyOutlineColor)) : currentColorOrValidColor(style, style.columnRuleColor());
    case CSSPropertyColumnRuleStyle:
        return CSSPrimitiveValue::create(style.columnRuleStyle());
    case CSSPropertyColumnRuleWidth:
        return zoomAdjustedPixelValue(style.columnRuleWidth(), style);
    case CSSPropertyColumnSpan:
        return CSSPrimitiveValue::create(style.columnSpan() == ColumnSpan::All ? CSSValueAll : CSSValueNone);
    case CSSPropertyWebkitColumnBreakAfter:
        return CSSPrimitiveValue::create(convertToColumnBreak(style.breakAfter()));
    case CSSPropertyWebkitColumnBreakBefore:
        return CSSPrimitiveValue::create(convertToColumnBreak(style.breakBefore()));
    case CSSPropertyWebkitColumnBreakInside:
        return CSSPrimitiveValue::create(convertToColumnBreak(style.breakInside()));
    case CSSPropertyColumnWidth:
        if (style.hasAutoColumnWidth())
            return CSSPrimitiveValue::create(CSSValueAuto);
        return zoomAdjustedPixelValue(style.columnWidth(), style);
    case CSSPropertyTabSize:
        return CSSPrimitiveValue::create(style.tabSize().widthInPixels(1.0), style.tabSize().isSpaces() ? CSSUnitType::CSS_NUMBER : CSSUnitType::CSS_PX);
    case CSSPropertyCursor: {
        RefPtr<CSSValueList> list;
        auto* cursors = style.cursors();
        if (cursors && cursors->size() > 0) {
            list = CSSValueList::createCommaSeparated();
            for (unsigned i = 0; i < cursors->size(); ++i) {
                if (StyleImage* image = cursors->at(i).image())
                    list->append(image->computedStyleValue(style));
            }
        }
        auto value = CSSPrimitiveValue::create(style.cursor());
        if (list) {
            list->append(WTFMove(value));
            return list;
        }
        return value;
    }
#if ENABLE(CURSOR_VISIBILITY)
    case CSSPropertyWebkitCursorVisibility:
        return CSSPrimitiveValue::create(style.cursorVisibility());
#endif
    case CSSPropertyDirection:  {
        auto direction = [&] {
            if (m_element == m_element->document().documentElement() && !style.hasExplicitlySetDirection())
                return RenderStyle::initialDirection();
            return style.direction();
        }();
        return CSSPrimitiveValue::create(direction);
    }
    case CSSPropertyDisplay:
        return CSSPrimitiveValue::create(style.display());
    case CSSPropertyEmptyCells:
        return CSSPrimitiveValue::create(style.emptyCells());
    case CSSPropertyAlignContent:
        return valueForContentPositionAndDistributionWithOverflowAlignment(style.alignContent());
    case CSSPropertyAlignItems:
        return valueForItemPositionWithOverflowAlignment(style.alignItems());
    case CSSPropertyAlignSelf:
        return valueForItemPositionWithOverflowAlignment(style.alignSelf());
    case CSSPropertyFlex:
        return getCSSPropertyValuesForShorthandProperties(flexShorthand());
    case CSSPropertyFlexBasis:
        return CSSPrimitiveValue::create(style.flexBasis(), style);
    case CSSPropertyFlexDirection:
        return CSSPrimitiveValue::create(style.flexDirection());
    case CSSPropertyFlexFlow:
        return getCSSPropertyValuesForShorthandProperties(flexFlowShorthand());
    case CSSPropertyFlexGrow:
        return CSSPrimitiveValue::create(style.flexGrow(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyFlexShrink:
        return CSSPrimitiveValue::create(style.flexShrink(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyFlexWrap:
        return CSSPrimitiveValue::create(style.flexWrap());
    case CSSPropertyJustifyContent:
        return valueForContentPositionAndDistributionWithOverflowAlignment(style.justifyContent());
    case CSSPropertyJustifyItems:
        return valueForItemPositionWithOverflowAlignment(style.justifyItems());
    case CSSPropertyJustifySelf:
        return valueForItemPositionWithOverflowAlignment(style.justifySelf());
    case CSSPropertyPlaceContent:
        return getCSSPropertyValuesForShorthandProperties(placeContentShorthand());
    case CSSPropertyPlaceItems:
        return getCSSPropertyValuesForShorthandProperties(placeItemsShorthand());
    case CSSPropertyPlaceSelf:
        return getCSSPropertyValuesForShorthandProperties(placeSelfShorthand());
    case CSSPropertyOrder:
        return CSSPrimitiveValue::create(style.order(), CSSUnitType::CSS_INTEGER);
    case CSSPropertyFloat:
        if (style.display() != DisplayType::None && style.hasOutOfFlowPosition())
            return CSSPrimitiveValue::create(CSSValueNone);
        return CSSPrimitiveValue::create(style.floating());
    case CSSPropertyFont:
        return fontShorthandValue(style, valueType);
    case CSSPropertyFontFamily:
        return fontFamily(style);
    case CSSPropertyFontSize:
        return fontSize(style);
    case CSSPropertyFontSizeAdjust:
        return fontSizeAdjustFromStyle(style);
    case CSSPropertyFontStyle:
        return fontStyle(style);
    case CSSPropertyFontStretch:
        return fontStretch(style);
    case CSSPropertyFontVariant:
        return fontVariantShorthandValue();
    case CSSPropertyFontWeight:
        return fontWeight(style);
    case CSSPropertyFontPalette:
        return fontPalette(style);
    case CSSPropertyFontSynthesis:
        return fontSynthesis(style);
    case CSSPropertyFontSynthesisWeight:
        return fontSynthesisWeight(style);
    case CSSPropertyFontSynthesisStyle:
        return fontSynthesisStyle(style);
    case CSSPropertyFontSynthesisSmallCaps:
        return fontSynthesisSmallCaps(style);
    case CSSPropertyFontFeatureSettings: {
        const FontFeatureSettings& featureSettings = style.fontDescription().featureSettings();
        if (!featureSettings.size())
            return CSSPrimitiveValue::create(CSSValueNormal);
        auto list = CSSValueList::createCommaSeparated();
        for (auto& feature : featureSettings)
            list->append(CSSFontFeatureValue::create(FontTag(feature.tag()), feature.value()));
        return list;
    }
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyFontVariationSettings: {
        auto& variationSettings = style.fontDescription().variationSettings();
        if (variationSettings.isEmpty())
            return CSSPrimitiveValue::create(CSSValueNormal);
        HashCountedSet<FontTag, FourCharacterTagHash, FourCharacterTagHashTraits> duplicateTagChecker;
        for (auto& feature : variationSettings)
            duplicateTagChecker.add(feature.tag());
        auto list = CSSValueList::createCommaSeparated();
        for (auto& feature : variationSettings) {
            if (duplicateTagChecker.remove(feature.tag()))
                list->append(CSSFontVariationValue::create(feature.tag(), feature.value()));
        }
        return list;
    }
    case CSSPropertyFontOpticalSizing:
        return CSSPrimitiveValue::create(style.fontDescription().opticalSizing());
#endif
    case CSSPropertyGridAutoFlow: {
        auto list = CSSValueList::createSpaceSeparated();
        ASSERT(style.isGridAutoFlowDirectionRow() || style.isGridAutoFlowDirectionColumn());
        if (style.isGridAutoFlowDirectionColumn())
            list->append(CSSPrimitiveValue::create(CSSValueColumn));
        else if (!style.isGridAutoFlowAlgorithmDense())
            list->append(CSSPrimitiveValue::create(CSSValueRow));

        if (style.isGridAutoFlowAlgorithmDense())
            list->append(CSSPrimitiveValue::create(CSSValueDense));

        return list;
    }
    case CSSPropertyMasonryAutoFlow: {
        auto list = CSSValueList::createSpaceSeparated();
        // MasonryAutoFlow information is stored in a struct that should always 
        // hold 2 pieces of information. It should contain both Pack/Next inside
        // the MasonryAutoFlowPlacementAlgorithm enum class and DefiniteFirst/Ordered
        // inside the MasonryAutoFlowPlacementOrder enum class
        ASSERT((style.masonryAutoFlow().placementAlgorithm == MasonryAutoFlowPlacementAlgorithm::Pack || style.masonryAutoFlow().placementAlgorithm == MasonryAutoFlowPlacementAlgorithm::Next) && (style.masonryAutoFlow().placementOrder == MasonryAutoFlowPlacementOrder::DefiniteFirst || style.masonryAutoFlow().placementOrder == MasonryAutoFlowPlacementOrder::Ordered));

        if (style.masonryAutoFlow().placementAlgorithm == MasonryAutoFlowPlacementAlgorithm::Next)
            list->append(CSSPrimitiveValue::create(CSSValueNext));
        // Since we know that placementAlgorithm is not Next, it must be Packed. If the PlacementOrder
        // is DefiniteFirst, then the canonical form of the computed style is just Pack (DefiniteFirst is implicit)
        else if (style.masonryAutoFlow().placementOrder == MasonryAutoFlowPlacementOrder::DefiniteFirst)
            list->append(CSSPrimitiveValue::create(CSSValuePack));

        if (style.masonryAutoFlow().placementOrder == MasonryAutoFlowPlacementOrder::Ordered)
            list->append(CSSPrimitiveValue::create(CSSValueOrdered));
        return list;
    }

    // Specs mention that getComputedStyle() should return the used value of the property instead of the computed
    // one for grid-template-{rows|columns} but not for the grid-auto-{rows|columns} as things like
    // grid-auto-columns: 2fr; cannot be resolved to a value in pixels as the '2fr' means very different things
    // depending on the size of the explicit grid or the number of implicit tracks added to the grid. See
    // http://lists.w3.org/Archives/Public/www-style/2013Nov/0014.html
    case CSSPropertyGridAutoColumns:
        return valueForGridTrackSizeList(ForColumns, style);
    case CSSPropertyGridAutoRows:
        return valueForGridTrackSizeList(ForRows, style);

    case CSSPropertyGridTemplateColumns:
        return valueForGridTrackList(ForColumns, renderer, style);
    case CSSPropertyGridTemplateRows:
        return valueForGridTrackList(ForRows, renderer, style);

    case CSSPropertyGridColumnStart:
        return valueForGridPosition(style.gridItemColumnStart());
    case CSSPropertyGridColumnEnd:
        return valueForGridPosition(style.gridItemColumnEnd());
    case CSSPropertyGridRowStart:
        return valueForGridPosition(style.gridItemRowStart());
    case CSSPropertyGridRowEnd:
        return valueForGridPosition(style.gridItemRowEnd());
    case CSSPropertyGridArea:
        return getCSSPropertyValuesForGridShorthand(gridAreaShorthand());
    case CSSPropertyGridTemplate:
        return getCSSPropertyValuesForGridShorthand(gridTemplateShorthand());
    case CSSPropertyGrid:
        return getCSSPropertyValuesForGridShorthand(gridShorthand());
    case CSSPropertyGridColumn:
        return getCSSPropertyValuesForGridShorthand(gridColumnShorthand());
    case CSSPropertyGridRow:
        return getCSSPropertyValuesForGridShorthand(gridRowShorthand());
    case CSSPropertyGridTemplateAreas:
        if (!style.namedGridAreaRowCount()) {
            ASSERT(!style.namedGridAreaColumnCount());
            return CSSPrimitiveValue::create(CSSValueNone);
        }
        return CSSGridTemplateAreasValue::create(style.namedGridArea(), style.namedGridAreaRowCount(), style.namedGridAreaColumnCount());
    case CSSPropertyGap:
        return getCSSPropertyValuesForShorthandProperties(gapShorthand());
    case CSSPropertyHeight:
        if (renderer && !renderer->isRenderOrLegacyRenderSVGModelObject()) {
            // According to http://www.w3.org/TR/CSS2/visudet.html#the-height-property,
            // the "height" property does not apply for non-replaced inline elements.
            if (!isNonReplacedInline(*renderer))
                return zoomAdjustedPixelValue(sizingBox(*renderer).height(), style);
        }
        return zoomAdjustedPixelValueForLength(style.height(), style);
    case CSSPropertyWebkitHyphens:
        return CSSPrimitiveValue::create(style.hyphens());
    case CSSPropertyWebkitHyphenateCharacter:
        if (style.hyphenationString().isNull())
            return CSSPrimitiveValue::create(CSSValueAuto);
        return CSSPrimitiveValue::create(style.hyphenationString(), CSSUnitType::CSS_STRING);
    case CSSPropertyWebkitHyphenateLimitAfter:
        if (style.hyphenationLimitAfter() < 0)
            return CSSPrimitiveValue::create(CSSValueAuto);
        return CSSPrimitiveValue::create(style.hyphenationLimitAfter(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyWebkitHyphenateLimitBefore:
        if (style.hyphenationLimitBefore() < 0)
            return CSSPrimitiveValue::create(CSSValueAuto);
        return CSSPrimitiveValue::create(style.hyphenationLimitBefore(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyWebkitHyphenateLimitLines:
        if (style.hyphenationLimitLines() < 0)
            return CSSPrimitiveValue::create(CSSValueNoLimit);
        return CSSPrimitiveValue::create(style.hyphenationLimitLines(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyImageOrientation:
        if (style.imageOrientation() == ImageOrientation::Orientation::FromImage)
            return CSSPrimitiveValue::create(CSSValueFromImage);
        return CSSPrimitiveValue::create(CSSValueNone);
    case CSSPropertyImageRendering:
        return CSSPrimitiveValue::create(style.imageRendering());
#if ENABLE(CSS_IMAGE_RESOLUTION)
    case CSSPropertyImageResolution:
        return CSSPrimitiveValue::create(style.imageResolution(), CSSUnitType::CSS_DPPX);
#endif
    case CSSPropertyInputSecurity:
        return CSSPrimitiveValue::create(style.inputSecurity());
    case CSSPropertyLeft:
        return positionOffsetValue(style, CSSPropertyLeft, renderer);
    case CSSPropertyLetterSpacing:
        if (!style.letterSpacing())
            return CSSPrimitiveValue::create(CSSValueNormal);
        return zoomAdjustedPixelValue(style.letterSpacing(), style);
    case CSSPropertyWebkitLineClamp:
        if (style.lineClamp().isNone())
            return CSSPrimitiveValue::create(CSSValueNone);
        return CSSPrimitiveValue::create(style.lineClamp().value(), style.lineClamp().isPercentage() ? CSSUnitType::CSS_PERCENTAGE : CSSUnitType::CSS_INTEGER);
    case CSSPropertyLineHeight:
        return lineHeight(style, valueType);
    case CSSPropertyListStyleImage:
        if (style.listStyleImage())
            return style.listStyleImage()->computedStyleValue(style);
        return CSSPrimitiveValue::create(CSSValueNone);
    case CSSPropertyListStylePosition:
        return CSSPrimitiveValue::create(style.listStylePosition());
    case CSSPropertyListStyleType:
        if (style.listStyleType() == ListStyleType::String)
            return CSSPrimitiveValue::create(style.listStyleStringValue(), CSSUnitType::CSS_STRING);
        return CSSPrimitiveValue::create(style.listStyleType());
    case CSSPropertyWebkitLocale:
        if (style.specifiedLocale().isNull())
            return CSSPrimitiveValue::create(CSSValueAuto);
        return CSSPrimitiveValue::createCustomIdent(style.specifiedLocale());
    case CSSPropertyMarginTop:
        return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::marginTop, &RenderBoxModelObject::marginTop>(style, renderer);
    case CSSPropertyMarginRight: {
        Length marginRight = style.marginRight();
        if (marginRight.isFixed() || !is<RenderBox>(renderer))
            return zoomAdjustedPixelValueForLength(marginRight, style);
        float value;
        if (marginRight.isPercentOrCalculated()) {
            // RenderBox gives a marginRight() that is the distance between the right-edge of the child box
            // and the right-edge of the containing box, when display == DisplayType::Block. Let's calculate the absolute
            // value of the specified margin-right % instead of relying on RenderBox's marginRight() value.
            value = minimumValueForLength(marginRight, downcast<RenderBox>(*renderer).containingBlockLogicalWidthForContent());
        } else
            value = downcast<RenderBox>(*renderer).marginRight();
        return zoomAdjustedPixelValue(value, style);
    }
    case CSSPropertyMarginBottom:
        return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::marginBottom, &RenderBoxModelObject::marginBottom>(style, renderer);
    case CSSPropertyMarginLeft:
        return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::marginLeft, &RenderBoxModelObject::marginLeft>(style, renderer);
    case CSSPropertyMarginTrim: {
        auto marginTrim = style.marginTrim();
        if (marginTrim.isEmpty())
            return CSSPrimitiveValue::create(CSSValueNone);

        // Try to serialize into one of the "block" or "inline" shorthands
        if (marginTrim.containsAll({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd }) && !marginTrim.containsAny({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd }))
            return CSSPrimitiveValue::create(CSSValueBlock);
        if (marginTrim.containsAll({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd }) && !marginTrim.containsAny({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd }))
            return CSSPrimitiveValue::create(CSSValueInline);

        auto list = CSSValueList::createSpaceSeparated();
        if (marginTrim.contains(MarginTrimType::BlockStart))
            list->append(CSSPrimitiveValue::create(CSSValueBlockStart));
        if (marginTrim.contains(MarginTrimType::InlineStart))
            list->append(CSSPrimitiveValue::create(CSSValueInlineStart));
        if (marginTrim.contains(MarginTrimType::BlockEnd))
            list->append(CSSPrimitiveValue::create(CSSValueBlockEnd));
        if (marginTrim.contains(MarginTrimType::InlineEnd))
            list->append(CSSPrimitiveValue::create(CSSValueInlineEnd));
        return list;
    }
    case CSSPropertyWebkitUserModify:
        return CSSPrimitiveValue::create(style.userModify());
    case CSSPropertyMaxHeight: {
        const Length& maxHeight = style.maxHeight();
        if (maxHeight.isUndefined())
            return CSSPrimitiveValue::create(CSSValueNone);
        return zoomAdjustedPixelValueForLength(maxHeight, style);
    }
    case CSSPropertyMaxWidth: {
        const Length& maxWidth = style.maxWidth();
        if (maxWidth.isUndefined())
            return CSSPrimitiveValue::create(CSSValueNone);
        return zoomAdjustedPixelValueForLength(maxWidth, style);
    }
    case CSSPropertyMinHeight:
        if (style.minHeight().isAuto()) {
            if (isFlexOrGridItem(renderer))
                return CSSPrimitiveValue::create(CSSValueAuto);
            return zoomAdjustedPixelValue(0, style);
        }
        return zoomAdjustedPixelValueForLength(style.minHeight(), style);
    case CSSPropertyMinWidth:
        if (style.minWidth().isAuto()) {
            if (isFlexOrGridItem(renderer))
                return CSSPrimitiveValue::create(CSSValueAuto);
            return zoomAdjustedPixelValue(0, style);
        }
        return zoomAdjustedPixelValueForLength(style.minWidth(), style);
    case CSSPropertyObjectFit:
        return CSSPrimitiveValue::create(style.objectFit());
    case CSSPropertyObjectPosition:
        return valueForPosition(style, style.objectPosition());
    case CSSPropertyOffsetPath:
        // The computed value of offset-path must only contain absolute draw commands.
        // https://github.com/w3c/fxtf-drafts/issues/225#issuecomment-334322738
        return valueForPathOperation(style, style.offsetPath(), SVGPathConversion::ForceAbsolute);
    case CSSPropertyOffsetDistance:
        return CSSPrimitiveValue::create(style.offsetDistance(), style);
    case CSSPropertyOffsetPosition:
        return valueForPositionOrAuto(style, style.offsetPosition());
    case CSSPropertyOffsetAnchor:
        return valueForPositionOrAuto(style, style.offsetAnchor());
    case CSSPropertyOffsetRotate:
        return valueForOffsetRotate(style.offsetRotate());
    case CSSPropertyOffset:
        return valueForOffsetShorthand(style);
    case CSSPropertyOpacity:
        return CSSPrimitiveValue::create(style.opacity(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyOrphans:
        if (style.hasAutoOrphans())
            return CSSPrimitiveValue::create(CSSValueAuto);
        return CSSPrimitiveValue::create(style.orphans(), CSSUnitType::CSS_INTEGER);
    case CSSPropertyOutlineColor:
        return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyOutlineColor)) : currentColorOrValidColor(style, style.outlineColor());
    case CSSPropertyOutlineOffset:
        return zoomAdjustedPixelValue(style.outlineOffset(), style);
    case CSSPropertyOutlineStyle:
        if (style.outlineStyleIsAuto() == OutlineIsAuto::On)
            return CSSPrimitiveValue::create(CSSValueAuto);
        return CSSPrimitiveValue::create(style.outlineStyle());
    case CSSPropertyOutlineWidth:
        return zoomAdjustedPixelValue(style.outlineWidth(), style);
    case CSSPropertyOverflow:
        return getCSSPropertyValuesFor2SidesShorthand(overflowShorthand());
    case CSSPropertyOverflowWrap:
        return CSSPrimitiveValue::create(style.overflowWrap());
    case CSSPropertyOverflowX:
        return CSSPrimitiveValue::create(style.overflowX());
    case CSSPropertyOverflowY:
        return CSSPrimitiveValue::create(style.overflowY());
    case CSSPropertyOverscrollBehavior:
        return CSSPrimitiveValue::create(std::max(style.overscrollBehaviorX(), style.overscrollBehaviorY()));
    case CSSPropertyOverscrollBehaviorX:
        return CSSPrimitiveValue::create(style.overscrollBehaviorX());
    case CSSPropertyOverscrollBehaviorY:
        return CSSPrimitiveValue::create(style.overscrollBehaviorY());
    case CSSPropertyPaddingTop:
        return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingTop, &RenderBoxModelObject::computedCSSPaddingTop>(style, renderer);
    case CSSPropertyPaddingRight:
        return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingRight, &RenderBoxModelObject::computedCSSPaddingRight>(style, renderer);
    case CSSPropertyPaddingBottom:
        return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingBottom, &RenderBoxModelObject::computedCSSPaddingBottom>(style, renderer);
    case CSSPropertyPaddingLeft:
        return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingLeft, &RenderBoxModelObject::computedCSSPaddingLeft>(style, renderer);
    case CSSPropertyPageBreakAfter:
        return CSSPrimitiveValue::create(convertToPageBreak(style.breakAfter()));
    case CSSPropertyPageBreakBefore:
        return CSSPrimitiveValue::create(convertToPageBreak(style.breakBefore()));
    case CSSPropertyPageBreakInside:
        return CSSPrimitiveValue::create(convertToPageBreak(style.breakInside()));
    case CSSPropertyBreakAfter:
        return CSSPrimitiveValue::create(style.breakAfter());
    case CSSPropertyBreakBefore:
        return CSSPrimitiveValue::create(style.breakBefore());
    case CSSPropertyBreakInside:
        return CSSPrimitiveValue::create(style.breakInside());
    case CSSPropertyHangingPunctuation:
        return hangingPunctuationToCSSValue(style.hangingPunctuation());
    case CSSPropertyPosition:
        return CSSPrimitiveValue::create(style.position());
    case CSSPropertyRight:
        return positionOffsetValue(style, CSSPropertyRight, renderer);
    case CSSPropertyWebkitRubyPosition:
        return CSSPrimitiveValue::create(style.rubyPosition());
    case CSSPropertyTableLayout:
        return CSSPrimitiveValue::create(style.tableLayout());
    case CSSPropertyTextAlign:
        return CSSPrimitiveValue::create(style.textAlign());
    case CSSPropertyTextAlignLast:
        return CSSPrimitiveValue::create(style.textAlignLast());
    case CSSPropertyTextDecoration:
        return renderTextDecorationLineFlagsToCSSValue(style.textDecorationLine());
    case CSSPropertyTextJustify:
        return CSSPrimitiveValue::create(style.textJustify());
    case CSSPropertyWebkitTextDecoration:
        return getCSSPropertyValuesForShorthandProperties(webkitTextDecorationShorthand());
    case CSSPropertyTextDecorationLine:
        return renderTextDecorationLineFlagsToCSSValue(style.textDecorationLine());
    case CSSPropertyTextDecorationStyle:
        return renderTextDecorationStyleFlagsToCSSValue(style.textDecorationStyle());
    case CSSPropertyTextDecorationColor:
        return currentColorOrValidColor(style, style.textDecorationColor());
    case CSSPropertyTextDecorationSkip:
        return renderTextDecorationSkipToCSSValue(style.textDecorationSkipInk());
    case CSSPropertyTextDecorationSkipInk:
        return CSSPrimitiveValue::create(style.textDecorationSkipInk());
    case CSSPropertyTextUnderlinePosition:
        return CSSPrimitiveValue::create(style.textUnderlinePosition());
    case CSSPropertyTextUnderlineOffset:
        return textUnderlineOffsetToCSSValue(style.textUnderlineOffset());
    case CSSPropertyTextDecorationThickness:
        return textDecorationThicknessToCSSValue(style.textDecorationThickness());
    case CSSPropertyWebkitTextDecorationsInEffect:
        return renderTextDecorationLineFlagsToCSSValue(style.textDecorationsInEffect());
    case CSSPropertyWebkitTextFillColor:
        return currentColorOrValidColor(style, style.textFillColor());
    case CSSPropertyTextEmphasisColor:
        return currentColorOrValidColor(style, style.textEmphasisColor());
    case CSSPropertyTextEmphasisPosition:
        return renderEmphasisPositionFlagsToCSSValue(style.textEmphasisPosition());
    case CSSPropertyTextEmphasisStyle:
        return valueForTextEmphasisStyle(style);
    case CSSPropertyTextEmphasis: {
        auto list = CSSValueList::createSpaceSeparated();
        list->append(valueForTextEmphasisStyle(style));
        list->append(currentColorOrValidColor(style, style.textEmphasisColor()));
        return list;
    }
    case CSSPropertyTextGroupAlign:
        return CSSPrimitiveValue::create(style.textGroupAlign());
    case CSSPropertyTextIndent: {
        auto textIndent = zoomAdjustedPixelValueForLength(style.textIndent(), style);
        if (style.textIndentLine() == TextIndentLine::EachLine || style.textIndentType() == TextIndentType::Hanging) {
            auto list = CSSValueList::createSpaceSeparated();
            list->append(WTFMove(textIndent));
            if (style.textIndentType() == TextIndentType::Hanging)
                list->append(CSSPrimitiveValue::create(CSSValueHanging));
            if (style.textIndentLine() == TextIndentLine::EachLine)
                list->append(CSSPrimitiveValue::create(CSSValueEachLine));
            return list;
        }
        return textIndent;
    }
    case CSSPropertyTextShadow:
        return valueForShadow(style.textShadow(), propertyID, style);
    case CSSPropertyTextRendering:
        return CSSPrimitiveValue::create(style.fontDescription().textRenderingMode());
    case CSSPropertyTextOverflow:
        if (style.textOverflow() == TextOverflow::Ellipsis)
            return CSSPrimitiveValue::create(CSSValueEllipsis);
        return CSSPrimitiveValue::create(CSSValueClip);
    case CSSPropertyWebkitTextSecurity:
        return CSSPrimitiveValue::create(style.textSecurity());
#if ENABLE(TEXT_AUTOSIZING)
    case CSSPropertyWebkitTextSizeAdjust:
        if (style.textSizeAdjust().isAuto())
            return CSSPrimitiveValue::create(CSSValueAuto);
        if (style.textSizeAdjust().isNone())
            return CSSPrimitiveValue::create(CSSValueNone);
        return CSSPrimitiveValue::create(style.textSizeAdjust().percentage(), CSSUnitType::CSS_PERCENTAGE);
#endif
    case CSSPropertyWebkitTextStrokeColor:
        return currentColorOrValidColor(style, style.textStrokeColor());
    case CSSPropertyWebkitTextStrokeWidth:
        return zoomAdjustedPixelValue(style.textStrokeWidth(), style);
    case CSSPropertyTextTransform:
        return CSSPrimitiveValue::create(style.textTransform());
    case CSSPropertyTextWrap:
        return CSSPrimitiveValue::create(style.textWrap());
    case CSSPropertyTop:
        return positionOffsetValue(style, CSSPropertyTop, renderer);
    case CSSPropertyUnicodeBidi:
        return CSSPrimitiveValue::create(style.unicodeBidi());
    case CSSPropertyVerticalAlign:
        switch (style.verticalAlign()) {
        case VerticalAlign::Baseline:
            return CSSPrimitiveValue::create(CSSValueBaseline);
        case VerticalAlign::Middle:
            return CSSPrimitiveValue::create(CSSValueMiddle);
        case VerticalAlign::Sub:
            return CSSPrimitiveValue::create(CSSValueSub);
        case VerticalAlign::Super:
            return CSSPrimitiveValue::create(CSSValueSuper);
        case VerticalAlign::TextTop:
            return CSSPrimitiveValue::create(CSSValueTextTop);
        case VerticalAlign::TextBottom:
            return CSSPrimitiveValue::create(CSSValueTextBottom);
        case VerticalAlign::Top:
            return CSSPrimitiveValue::create(CSSValueTop);
        case VerticalAlign::Bottom:
            return CSSPrimitiveValue::create(CSSValueBottom);
        case VerticalAlign::BaselineMiddle:
            return CSSPrimitiveValue::create(CSSValueWebkitBaselineMiddle);
        case VerticalAlign::Length:
            return CSSPrimitiveValue::create(style.verticalAlignLength(), style);
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    case CSSPropertyVisibility:
        return CSSPrimitiveValue::create(style.visibility());
    case CSSPropertyWhiteSpace:
        return CSSPrimitiveValue::create(style.whiteSpace());
    case CSSPropertyWidows:
        if (style.hasAutoWidows())
            return CSSPrimitiveValue::create(CSSValueAuto);
        return CSSPrimitiveValue::create(style.widows(), CSSUnitType::CSS_INTEGER);
    case CSSPropertyWidth:
        if (renderer && !renderer->isRenderOrLegacyRenderSVGModelObject()) {
            // According to http://www.w3.org/TR/CSS2/visudet.html#the-width-property,
            // the "width" property does not apply for non-replaced inline elements.
            if (!isNonReplacedInline(*renderer))
                return zoomAdjustedPixelValue(sizingBox(*renderer).width(), style);
        }
        return zoomAdjustedPixelValueForLength(style.width(), style);
    case CSSPropertyWillChange:
        return willChangePropertyValue(style.willChange());
    case CSSPropertyWordBreak:
        return CSSPrimitiveValue::create(style.wordBreak());
    case CSSPropertyWordSpacing: {
        auto& wordSpacingLength = style.wordSpacing();
        if (wordSpacingLength.isFixed() || wordSpacingLength.isAuto())
            return zoomAdjustedPixelValue(style.fontCascade().wordSpacing(), style);
        return CSSPrimitiveValue::create(wordSpacingLength, style);
    }
    case CSSPropertyLineBreak:
        return CSSPrimitiveValue::create(style.lineBreak());
    case CSSPropertyWebkitNbspMode:
        return CSSPrimitiveValue::create(style.nbspMode());
    case CSSPropertyResize:
        return CSSPrimitiveValue::create(style.resize());
    case CSSPropertyFontKerning:
        return CSSPrimitiveValue::create(style.fontDescription().kerning());
    case CSSPropertyWebkitFontSmoothing:
        return CSSPrimitiveValue::create(style.fontDescription().fontSmoothing());
    case CSSPropertyFontVariantLigatures:
        return fontVariantLigaturesPropertyValue(style.fontDescription().variantCommonLigatures(), style.fontDescription().variantDiscretionaryLigatures(), style.fontDescription().variantHistoricalLigatures(), style.fontDescription().variantContextualAlternates());
    case CSSPropertyFontVariantPosition:
        return fontVariantPositionPropertyValue(style.fontDescription().variantPosition());
    case CSSPropertyFontVariantCaps:
        return fontVariantCapsPropertyValue(style.fontDescription().variantCaps());
    case CSSPropertyFontVariantNumeric:
        return fontVariantNumericPropertyValue(style.fontDescription().variantNumericFigure(), style.fontDescription().variantNumericSpacing(), style.fontDescription().variantNumericFraction(), style.fontDescription().variantNumericOrdinal(), style.fontDescription().variantNumericSlashedZero());
    case CSSPropertyFontVariantAlternates:
        return fontVariantAlternatesPropertyValue(style.fontDescription().variantAlternates());
    case CSSPropertyFontVariantEastAsian:
        return fontVariantEastAsianPropertyValue(style.fontDescription().variantEastAsianVariant(), style.fontDescription().variantEastAsianWidth(), style.fontDescription().variantEastAsianRuby());
    case CSSPropertyZIndex:
        if (style.hasAutoSpecifiedZIndex())
            return CSSPrimitiveValue::create(CSSValueAuto);
        return CSSPrimitiveValue::create(style.specifiedZIndex(), CSSUnitType::CSS_INTEGER);
    case CSSPropertyZoom:
        return CSSPrimitiveValue::create(style.zoom(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyBoxSizing:
        if (style.boxSizing() == BoxSizing::ContentBox)
            return CSSPrimitiveValue::create(CSSValueContentBox);
        return CSSPrimitiveValue::create(CSSValueBorderBox);
    case CSSPropertyAnimation:
        return animationShorthandValue(propertyID, style.animations());
    case CSSPropertyAnimationComposition:
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyAnimationTimingFunction:
        return valueListForAnimationOrTransitionProperty(propertyID, style.animations());
    case CSSPropertyAppearance:
        return CSSPrimitiveValue::create(style.appearance());
    case CSSPropertyAspectRatio:
        switch (style.aspectRatioType()) {
        case AspectRatioType::Auto:
            return CSSPrimitiveValue::create(CSSValueAuto);
        case AspectRatioType::AutoZero:
        case AspectRatioType::AutoAndRatio:
        case AspectRatioType::Ratio: {
            auto ratioList = CSSValueList::createSlashSeparated();
            ratioList->append(CSSPrimitiveValue::create(style.aspectRatioWidth(), CSSUnitType::CSS_NUMBER));
            ratioList->append(CSSPrimitiveValue::create(style.aspectRatioHeight(), CSSUnitType::CSS_NUMBER));
            if (style.aspectRatioType() != AspectRatioType::AutoAndRatio)
                return ratioList;
            auto list = CSSValueList::createSpaceSeparated();
            list->append(CSSPrimitiveValue::create(CSSValueAuto));
            list->append(ratioList);
            return list;
        }
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    case CSSPropertyContain: {
        auto containment = style.contain();
        if (!containment)
            return CSSPrimitiveValue::create(CSSValueNone);
        if (containment == RenderStyle::strictContainment())
            return CSSPrimitiveValue::create(CSSValueStrict);
        if (containment == RenderStyle::contentContainment())
            return CSSPrimitiveValue::create(CSSValueContent);
        auto list = CSSValueList::createSpaceSeparated();
        if (containment & Containment::Size)
            list->append(CSSPrimitiveValue::create(CSSValueSize));
        if (containment & Containment::InlineSize)
            list->append(CSSPrimitiveValue::create(CSSValueInlineSize));
        if (containment & Containment::Layout)
            list->append(CSSPrimitiveValue::create(CSSValueLayout));
        if (containment & Containment::Style)
            list->append(CSSPrimitiveValue::create(CSSValueStyle));
        if (containment & Containment::Paint)
            list->append(CSSPrimitiveValue::create(CSSValuePaint));
        return list;
    }
    case CSSPropertyContainer: {
        auto list = CSSValueList::createSlashSeparated();
        if (style.containerNames().isEmpty())
            list->append(CSSPrimitiveValue::create(CSSValueNone));
        else
            list->append(propertyValue(CSSPropertyContainerName, UpdateLayout::No).releaseNonNull());
        if (style.containerType() != ContainerType::Normal)
            list->append(propertyValue(CSSPropertyContainerType, UpdateLayout::No).releaseNonNull());
        return list;
    }
    case CSSPropertyContainerType:
        return CSSPrimitiveValue::create(style.containerType());
    case CSSPropertyContainerName: {
        if (style.containerNames().isEmpty())
            return CSSPrimitiveValue::create(CSSValueNone);
        auto list = CSSValueList::createSpaceSeparated();
        for (auto& name : style.containerNames())
            list->append(CSSPrimitiveValue::createCustomIdent(name));
        return list;
    }
    case CSSPropertyContainIntrinsicSize:
        return getCSSPropertyValuesFor2SidesShorthand(containIntrinsicSizeShorthand());
    case CSSPropertyContainIntrinsicWidth:
        return valueForContainIntrinsicSize(style, style.containIntrinsicWidthType(), style.containIntrinsicWidth());
    case CSSPropertyContainIntrinsicHeight:
        return valueForContainIntrinsicSize(style, style.containIntrinsicHeightType(), style.containIntrinsicHeight());
    case CSSPropertyContentVisibility:
        return CSSPrimitiveValue::create(toCSSValueID(style.contentVisibility()));
    case CSSPropertyBackfaceVisibility:
        return CSSPrimitiveValue::create((style.backfaceVisibility() == BackfaceVisibility::Hidden) ? CSSValueHidden : CSSValueVisible);
    case CSSPropertyBorderImage:
    case CSSPropertyWebkitBorderImage:
        return valueForNinePieceImage(propertyID, style.borderImage(), style);
    case CSSPropertyBorderImageOutset:
        return valueForNinePieceImageQuad(style.borderImage().outset(), style);
    case CSSPropertyBorderImageRepeat:
        return valueForNinePieceImageRepeat(style.borderImage());
    case CSSPropertyBorderImageSlice:
        return valueForNinePieceImageSlice(style.borderImage());
    case CSSPropertyBorderImageWidth:
        if (style.borderImage().overridesBorderWidths())
            return nullptr;
        return valueForNinePieceImageQuad(style.borderImage().borderSlices(), style);
    case CSSPropertyWebkitMaskBoxImage:
        return valueForNinePieceImage(propertyID, style.maskBoxImage(), style);
    case CSSPropertyWebkitMaskBoxImageOutset:
        return valueForNinePieceImageQuad(style.maskBoxImage().outset(), style);
    case CSSPropertyWebkitMaskBoxImageRepeat:
        return valueForNinePieceImageRepeat(style.maskBoxImage());
    case CSSPropertyWebkitMaskBoxImageSlice:
        return valueForNinePieceImageSlice(style.maskBoxImage());
    case CSSPropertyWebkitMaskBoxImageWidth:
        return valueForNinePieceImageQuad(style.maskBoxImage().borderSlices(), style);
    case CSSPropertyWebkitMaskBoxImageSource:
        if (style.maskBoxImageSource())
            return style.maskBoxImageSource()->computedStyleValue(style);
        return CSSPrimitiveValue::create(CSSValueNone);
    case CSSPropertyWebkitInitialLetter: {
        auto drop = !style.initialLetterDrop() ? CSSPrimitiveValue::create(CSSValueNormal) : CSSPrimitiveValue::create(style.initialLetterDrop(), CSSUnitType::CSS_NUMBER);
        auto size = !style.initialLetterHeight() ? CSSPrimitiveValue::create(CSSValueNormal) : CSSPrimitiveValue::create(style.initialLetterHeight(), CSSUnitType::CSS_NUMBER);
        return CSSPrimitiveValue::create(Pair::create(WTFMove(drop), WTFMove(size)));
    }
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    case CSSPropertyWebkitOverflowScrolling:
        if (!style.useTouchOverflowScrolling())
            return CSSPrimitiveValue::create(CSSValueAuto);
        return CSSPrimitiveValue::create(CSSValueTouch);
#endif
    case CSSPropertyScrollBehavior:
        if (!style.useSmoothScrolling())
            return CSSPrimitiveValue::create(CSSValueAuto);
        return CSSPrimitiveValue::create(CSSValueSmooth);
    case CSSPropertyPerspective:
    case CSSPropertyWebkitPerspective:
        if (!style.hasPerspective())
            return CSSPrimitiveValue::create(CSSValueNone);
        return zoomAdjustedPixelValue(style.perspective(), style);
    case CSSPropertyPerspectiveOrigin: {
        auto list = CSSValueList::createSpaceSeparated();
        if (renderer) {
            auto box = renderer->transformReferenceBoxRect(style);
            list->append(zoomAdjustedPixelValue(minimumValueForLength(style.perspectiveOriginX(), box.width()), style));
            list->append(zoomAdjustedPixelValue(minimumValueForLength(style.perspectiveOriginY(), box.height()), style));
        } else {
            list->append(zoomAdjustedPixelValueForLength(style.perspectiveOriginX(), style));
            list->append(zoomAdjustedPixelValueForLength(style.perspectiveOriginY(), style));
        }
        return list;
    }
    case CSSPropertyWebkitRtlOrdering:
        return CSSPrimitiveValue::create(style.rtlOrdering() == Order::Visual ? CSSValueVisual : CSSValueLogical);
#if ENABLE(TOUCH_EVENTS)
    case CSSPropertyWebkitTapHighlightColor:
        return currentColorOrValidColor(style, style.tapHighlightColor());
#endif
    case CSSPropertyTouchAction:
        return touchActionFlagsToCSSValue(style.touchActions());
#if PLATFORM(IOS_FAMILY)
    case CSSPropertyWebkitTouchCallout:
        return CSSPrimitiveValue::create(style.touchCalloutEnabled() ? CSSValueDefault : CSSValueNone);
#endif
    case CSSPropertyWebkitUserDrag:
        return CSSPrimitiveValue::create(style.userDrag());
    case CSSPropertyWebkitUserSelect:
        return CSSPrimitiveValue::create(style.userSelect());
    case CSSPropertyBorderBottomLeftRadius:
        return borderRadiusCornerValue(style.borderBottomLeftRadius(), style);
    case CSSPropertyBorderBottomRightRadius:
        return borderRadiusCornerValue(style.borderBottomRightRadius(), style);
    case CSSPropertyBorderTopLeftRadius:
        return borderRadiusCornerValue(style.borderTopLeftRadius(), style);
    case CSSPropertyBorderTopRightRadius:
        return borderRadiusCornerValue(style.borderTopRightRadius(), style);
    case CSSPropertyClip: {
        if (!style.hasClip())
            return CSSPrimitiveValue::create(CSSValueAuto);
        if (style.clip().top().isAuto() && style.clip().right().isAuto()
            && style.clip().top().isAuto() && style.clip().right().isAuto())
            return CSSPrimitiveValue::create(CSSValueAuto);

        auto rect = Rect::create();
        rect->setTop(autoOrZoomAdjustedValue(style.clip().top(), style));
        rect->setRight(autoOrZoomAdjustedValue(style.clip().right(), style));
        rect->setBottom(autoOrZoomAdjustedValue(style.clip().bottom(), style));
        rect->setLeft(autoOrZoomAdjustedValue(style.clip().left(), style));
        return CSSPrimitiveValue::create(WTFMove(rect));
    }
    case CSSPropertySpeakAs:
        return speakAsToCSSValue(style.speakAs());
    case CSSPropertyTransform:
        return computedTransform(renderer, style, valueType);
    case CSSPropertyTransformBox:
        return CSSPrimitiveValue::create(style.transformBox());
    case CSSPropertyTransformOrigin: {
        auto list = CSSValueList::createSpaceSeparated();
        if (renderer) {
            auto box = renderer->transformReferenceBoxRect(style);
            list->append(zoomAdjustedPixelValue(minimumValueForLength(style.transformOriginX(), box.width()), style));
            list->append(zoomAdjustedPixelValue(minimumValueForLength(style.transformOriginY(), box.height()), style));
            if (style.transformOriginZ())
                list->append(zoomAdjustedPixelValue(style.transformOriginZ(), style));
        } else {
            list->append(zoomAdjustedPixelValueForLength(style.transformOriginX(), style));
            list->append(zoomAdjustedPixelValueForLength(style.transformOriginY(), style));
            if (style.transformOriginZ())
                list->append(zoomAdjustedPixelValue(style.transformOriginZ(), style));
        }
        return list;
    }
    case CSSPropertyTransformStyle:
        switch (style.transformStyle3D()) {
        case TransformStyle3D::Flat:
            return CSSPrimitiveValue::create(CSSValueFlat);
        case TransformStyle3D::Preserve3D:
            return CSSPrimitiveValue::create(CSSValuePreserve3d);
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
        case TransformStyle3D::Optimized3D:
            return CSSPrimitiveValue::create(CSSValueOptimized3d);
#endif
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    case CSSPropertyTranslate:
        return computedTranslate(renderer, style);
    case CSSPropertyScale:
        return computedScale(renderer, style);
    case CSSPropertyRotate:
        return computedRotate(renderer, style);
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionTimingFunction:
    case CSSPropertyTransitionProperty:
        return valueListForAnimationOrTransitionProperty(propertyID, style.transitions());
    case CSSPropertyTransition:
        return animationShorthandValue(propertyID, style.transitions());
    case CSSPropertyPointerEvents:
        return CSSPrimitiveValue::create(style.pointerEvents());
    case CSSPropertyWebkitLineGrid:
        if (style.lineGrid().isNull())
            return CSSPrimitiveValue::create(CSSValueNone);
        return CSSPrimitiveValue::createCustomIdent(style.lineGrid());
    case CSSPropertyWebkitLineSnap:
        return CSSPrimitiveValue::create(style.lineSnap());
    case CSSPropertyWebkitLineAlign:
        return CSSPrimitiveValue::create(style.lineAlign());
    case CSSPropertyWritingMode: {
        auto writingMode = [&] {
            if (m_element == m_element->document().documentElement() && !style.hasExplicitlySetWritingMode())
                return RenderStyle::initialWritingMode();
            return style.writingMode();
        }();
        return CSSPrimitiveValue::create(writingMode);
    }
    case CSSPropertyWebkitTextCombine:
        if (style.textCombine() == TextCombine::All)
            return CSSPrimitiveValue::create(CSSValueHorizontal);
        return CSSPrimitiveValue::create(style.textCombine());
    case CSSPropertyTextCombineUpright:
        return CSSPrimitiveValue::create(style.textCombine());
    case CSSPropertyWebkitTextOrientation:
        return CSSPrimitiveValue::create(style.textOrientation());
    case CSSPropertyTextOrientation:
        return CSSPrimitiveValue::create(style.textOrientation());
    case CSSPropertyWebkitLineBoxContain:
        return createLineBoxContainValue(style.lineBoxContain());
    case CSSPropertyAlt:
        return altTextToCSSValue(style);
    case CSSPropertyContent:
        return contentToCSSValue(style);
    case CSSPropertyCounterIncrement:
        return counterToCSSValue(style, propertyID);
    case CSSPropertyCounterReset:
        return counterToCSSValue(style, propertyID);
    case CSSPropertyClipPath:
        return valueForPathOperation(style, style.clipPath());
    case CSSPropertyShapeMargin:
        return CSSPrimitiveValue::create(style.shapeMargin(), style);
    case CSSPropertyShapeImageThreshold:
        return CSSPrimitiveValue::create(style.shapeImageThreshold(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyShapeOutside:
        return shapePropertyValue(style, style.shapeOutside());
    case CSSPropertyFilter:
        return valueForFilter(style, style.filter());
    case CSSPropertyAppleColorFilter:
        return valueForFilter(style, style.appleColorFilter());
#if ENABLE(FILTERS_LEVEL_2)
    case CSSPropertyWebkitBackdropFilter:
        return valueForFilter(style, style.backdropFilter());
#endif
    case CSSPropertyMathStyle:
        return CSSPrimitiveValue::create(style.mathStyle());
#if ENABLE(CSS_COMPOSITING)
    case CSSPropertyMixBlendMode:
        return CSSPrimitiveValue::create(style.blendMode());
    case CSSPropertyIsolation:
        return CSSPrimitiveValue::create(style.isolation());
#endif
    case CSSPropertyBackgroundBlendMode: {
        auto& layers = style.backgroundLayers();
        if (!layers.next())
            return CSSPrimitiveValue::create(layers.blendMode());
        auto list = CSSValueList::createCommaSeparated();
        for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
            list->append(CSSPrimitiveValue::create(currLayer->blendMode()));
        return list;
    }
    case CSSPropertyBackground:
        return getBackgroundShorthandValue();
    case CSSPropertyMask:
        return getMaskShorthandValue();
    case CSSPropertyBorder: {
        auto value = propertyValue(CSSPropertyBorderTop, UpdateLayout::No);
        const CSSPropertyID properties[3] = { CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
        for (auto& property : properties) {
            if (!compareCSSValuePtr<CSSValue>(value, propertyValue(property, UpdateLayout::No)))
                return nullptr;
        }
        return value;
    }
    case CSSPropertyBorderBlock: {
        auto value = propertyValue(CSSPropertyBorderBlockStart, UpdateLayout::No);
        if (!compareCSSValuePtr<CSSValue>(value, propertyValue(CSSPropertyBorderBlockEnd, UpdateLayout::No)))
            return nullptr;
        return value;
    }
    case CSSPropertyBorderBlockColor:
        return getCSSPropertyValuesFor2SidesShorthand(borderBlockColorShorthand());
    case CSSPropertyBorderBlockEnd:
        return getCSSPropertyValuesForShorthandProperties(borderBlockEndShorthand());
    case CSSPropertyBorderBlockStart:
        return getCSSPropertyValuesForShorthandProperties(borderBlockStartShorthand());
    case CSSPropertyBorderBlockStyle:
        return getCSSPropertyValuesFor2SidesShorthand(borderBlockStyleShorthand());
    case CSSPropertyBorderBlockWidth:
        return getCSSPropertyValuesFor2SidesShorthand(borderBlockWidthShorthand());
    case CSSPropertyBorderBottom:
        return getCSSPropertyValuesForShorthandProperties(borderBottomShorthand());
    case CSSPropertyBorderColor:
        return getCSSPropertyValuesFor4SidesShorthand(borderColorShorthand());
    case CSSPropertyBorderLeft:
        return getCSSPropertyValuesForShorthandProperties(borderLeftShorthand());
    case CSSPropertyBorderInline: {
        auto value = propertyValue(CSSPropertyBorderInlineStart, UpdateLayout::No);
        if (!compareCSSValuePtr<CSSValue>(value, propertyValue(CSSPropertyBorderInlineEnd, UpdateLayout::No)))
            return nullptr;
        return value;
    }
    case CSSPropertyBorderInlineColor:
        return getCSSPropertyValuesFor2SidesShorthand(borderInlineColorShorthand());
    case CSSPropertyBorderInlineEnd:
        return getCSSPropertyValuesForShorthandProperties(borderInlineEndShorthand());
    case CSSPropertyBorderInlineStart:
        return getCSSPropertyValuesForShorthandProperties(borderInlineStartShorthand());
    case CSSPropertyBorderInlineStyle:
        return getCSSPropertyValuesFor2SidesShorthand(borderInlineStyleShorthand());
    case CSSPropertyBorderInlineWidth:
        return getCSSPropertyValuesFor2SidesShorthand(borderInlineWidthShorthand());
    case CSSPropertyBorderRadius:
    case CSSPropertyWebkitBorderRadius:
        return borderRadiusShorthandValue(style, propertyID);
    case CSSPropertyBorderRight:
        return getCSSPropertyValuesForShorthandProperties(borderRightShorthand());
    case CSSPropertyBorderStyle:
        return getCSSPropertyValuesFor4SidesShorthand(borderStyleShorthand());
    case CSSPropertyBorderTop:
        return getCSSPropertyValuesForShorthandProperties(borderTopShorthand());
    case CSSPropertyBorderWidth:
        return getCSSPropertyValuesFor4SidesShorthand(borderWidthShorthand());
    case CSSPropertyColumnRule:
        return getCSSPropertyValuesForShorthandProperties(columnRuleShorthand());
    case CSSPropertyColumns:
        return getCSSPropertyValuesForShorthandProperties(columnsShorthand());
    case CSSPropertyInset:
        return getCSSPropertyValuesFor4SidesShorthand(insetShorthand());
    case CSSPropertyInsetBlock:
        return getCSSPropertyValuesFor2SidesShorthand(insetBlockShorthand());
    case CSSPropertyInsetInline:
        return getCSSPropertyValuesFor2SidesShorthand(insetInlineShorthand());
    case CSSPropertyListStyle:
        return getCSSPropertyValuesForShorthandProperties(listStyleShorthand());
    case CSSPropertyMargin:
        return getCSSPropertyValuesFor4SidesShorthand(marginShorthand());
    case CSSPropertyMarginBlock:
        return getCSSPropertyValuesFor2SidesShorthand(marginBlockShorthand());
    case CSSPropertyMarginInline:
        return getCSSPropertyValuesFor2SidesShorthand(marginInlineShorthand());
    case CSSPropertyOutline:
        return getCSSPropertyValuesForShorthandProperties(outlineShorthand());
    case CSSPropertyPadding:
        return getCSSPropertyValuesFor4SidesShorthand(paddingShorthand());
    case CSSPropertyPaddingBlock:
        return getCSSPropertyValuesFor2SidesShorthand(paddingBlockShorthand());
    case CSSPropertyPaddingInline:
        return getCSSPropertyValuesFor2SidesShorthand(paddingInlineShorthand());
    case CSSPropertyScrollMargin:
        return getCSSPropertyValuesFor4SidesShorthand(scrollMarginShorthand());
    case CSSPropertyScrollMarginBottom:
        return zoomAdjustedPixelValueForLength(style.scrollMarginBottom(), style);
    case CSSPropertyScrollMarginTop:
        return zoomAdjustedPixelValueForLength(style.scrollMarginTop(), style);
    case CSSPropertyScrollMarginRight:
        return zoomAdjustedPixelValueForLength(style.scrollMarginRight(), style);
    case CSSPropertyScrollMarginLeft:
        return zoomAdjustedPixelValueForLength(style.scrollMarginLeft(), style);
    case CSSPropertyScrollMarginBlock:
        return getCSSPropertyValuesFor2SidesShorthand(scrollMarginBlockShorthand());
    case CSSPropertyScrollMarginInline:
        return getCSSPropertyValuesFor2SidesShorthand(scrollMarginInlineShorthand());
    case CSSPropertyScrollPadding:
        return getCSSPropertyValuesFor4SidesShorthand(scrollPaddingShorthand());
    case CSSPropertyScrollPaddingBottom:
        return zoomAdjustedPixelValueForLength(style.scrollPaddingBottom(), style);
    case CSSPropertyScrollPaddingTop:
        return zoomAdjustedPixelValueForLength(style.scrollPaddingTop(), style);
    case CSSPropertyScrollPaddingRight:
        return zoomAdjustedPixelValueForLength(style.scrollPaddingRight(), style);
    case CSSPropertyScrollPaddingLeft:
        return zoomAdjustedPixelValueForLength(style.scrollPaddingLeft(), style);
    case CSSPropertyScrollPaddingBlock:
        return getCSSPropertyValuesFor2SidesShorthand(scrollPaddingBlockShorthand());
    case CSSPropertyScrollPaddingInline:
        return getCSSPropertyValuesFor2SidesShorthand(scrollPaddingInlineShorthand());
    case CSSPropertyScrollSnapAlign:
        return valueForScrollSnapAlignment(style.scrollSnapAlign());
    case CSSPropertyScrollSnapStop:
        return CSSPrimitiveValue::create(style.scrollSnapStop());
    case CSSPropertyScrollSnapType:
        return valueForScrollSnapType(style.scrollSnapType());
    case CSSPropertyOverflowAnchor:
        return CSSPrimitiveValue::create(style.overflowAnchor());
    case CSSPropertyTextEdge:
        return valueForTextEdge(style.textEdge());

#if ENABLE(APPLE_PAY)
    case CSSPropertyApplePayButtonStyle:
        return CSSPrimitiveValue::create(style.applePayButtonStyle());
    case CSSPropertyApplePayButtonType:
        return CSSPrimitiveValue::create(style.applePayButtonType());
#endif

#if ENABLE(DARK_MODE_CSS)
    case CSSPropertyColorScheme: {
        auto colorScheme = style.colorScheme();
        if (colorScheme.isNormal())
            return CSSPrimitiveValue::create(CSSValueNormal);

        auto list = CSSValueList::createSpaceSeparated();
        if (colorScheme.contains(ColorScheme::Light))
            list->append(CSSPrimitiveValue::create(CSSValueLight));
        if (colorScheme.contains(ColorScheme::Dark))
            list->append(CSSPrimitiveValue::create(CSSValueDark));
        if (!colorScheme.allowsTransformations())
            list->append(CSSPrimitiveValue::create(CSSValueOnly));
        ASSERT(list->length());
        return list;
    }
#endif

    // Length properties for SVG.
    case CSSPropertyCx:
        return zoomAdjustedPixelValueForLength(style.svgStyle().cx(), style);
    case CSSPropertyCy:
        return zoomAdjustedPixelValueForLength(style.svgStyle().cy(), style);
    case CSSPropertyR:
        return zoomAdjustedPixelValueForLength(style.svgStyle().r(), style);
    case CSSPropertyRx:
        return zoomAdjustedPixelValueForLength(style.svgStyle().rx(), style);
    case CSSPropertyRy:
        return zoomAdjustedPixelValueForLength(style.svgStyle().ry(), style);
    case CSSPropertyStrokeDashoffset:
        return zoomAdjustedPixelValueForLength(style.svgStyle().strokeDashOffset(), style);
    case CSSPropertyX:
        return zoomAdjustedPixelValueForLength(style.svgStyle().x(), style);
    case CSSPropertyY:
        return zoomAdjustedPixelValueForLength(style.svgStyle().y(), style);
    case CSSPropertyWebkitTextZoom:
        return CSSPrimitiveValue::create(style.textZoom());

    case CSSPropertyPaintOrder:
        return paintOrder(style.paintOrder());
    case CSSPropertyStrokeLinecap:
        return CSSPrimitiveValue::create(style.capStyle());
    case CSSPropertyStrokeLinejoin:
        return CSSPrimitiveValue::create(style.joinStyle());
    case CSSPropertyStrokeWidth:
        return zoomAdjustedPixelValueForLength(style.strokeWidth(), style);
    case CSSPropertyStrokeColor:
        return currentColorOrValidColor(style, style.strokeColor());
    case CSSPropertyStrokeMiterlimit:
        return CSSPrimitiveValue::create(style.strokeMiterLimit(), CSSUnitType::CSS_NUMBER);

    case CSSPropertyQuotes:
        return valueForQuotes(style.quotes());

    /* Unimplemented CSS 3 properties (including CSS3 shorthand properties) */
    case CSSPropertyAll:
        return nullptr;

    /* Directional properties are resolved by resolveDirectionAwareProperty() before the switch. */
    case CSSPropertyBorderBlockEndColor:
    case CSSPropertyBorderBlockEndStyle:
    case CSSPropertyBorderBlockEndWidth:
    case CSSPropertyBorderBlockStartColor:
    case CSSPropertyBorderBlockStartStyle:
    case CSSPropertyBorderBlockStartWidth:
    case CSSPropertyBorderEndEndRadius:
    case CSSPropertyBorderEndStartRadius:
    case CSSPropertyBorderInlineEndColor:
    case CSSPropertyBorderInlineEndStyle:
    case CSSPropertyBorderInlineEndWidth:
    case CSSPropertyBorderInlineStartColor:
    case CSSPropertyBorderInlineStartStyle:
    case CSSPropertyBorderInlineStartWidth:
    case CSSPropertyBorderStartEndRadius:
    case CSSPropertyBorderStartStartRadius:
    case CSSPropertyInsetBlockEnd:
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetInlineEnd:
    case CSSPropertyInsetInlineStart:
    case CSSPropertyMarginBlockEnd:
    case CSSPropertyMarginBlockStart:
    case CSSPropertyMarginInlineEnd:
    case CSSPropertyMarginInlineStart:
    case CSSPropertyOverscrollBehaviorInline:
    case CSSPropertyOverscrollBehaviorBlock:
    case CSSPropertyPaddingBlockEnd:
    case CSSPropertyPaddingBlockStart:
    case CSSPropertyPaddingInlineEnd:
    case CSSPropertyPaddingInlineStart:
    case CSSPropertyBlockSize:
    case CSSPropertyInlineSize:
    case CSSPropertyMaxBlockSize:
    case CSSPropertyMaxInlineSize:
    case CSSPropertyMinBlockSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyScrollMarginBlockEnd:
    case CSSPropertyScrollMarginBlockStart:
    case CSSPropertyScrollMarginInlineEnd:
    case CSSPropertyScrollMarginInlineStart:
    case CSSPropertyScrollPaddingBlockEnd:
    case CSSPropertyScrollPaddingBlockStart:
    case CSSPropertyScrollPaddingInlineEnd:
    case CSSPropertyScrollPaddingInlineStart:
    case CSSPropertyContainIntrinsicBlockSize:
    case CSSPropertyContainIntrinsicInlineSize:
        ASSERT_NOT_REACHED();
        return nullptr;

    // Internal properties should be handled by isExposed above.
    case CSSPropertyWebkitFontSizeDelta:
    case CSSPropertyWebkitMarqueeDirection:
    case CSSPropertyWebkitMarqueeIncrement:
    case CSSPropertyWebkitMarqueeRepetition:
    case CSSPropertyWebkitMarqueeStyle:
    case CSSPropertyWebkitMarqueeSpeed:
#if ENABLE(TEXT_AUTOSIZING)
    case CSSPropertyInternalTextAutosizingStatus:
#endif
        ASSERT_NOT_REACHED();
        return nullptr;

    // These are intentionally unimplemented because they are actually descriptors for @counter-style.
    case CSSPropertySystem:
    case CSSPropertyNegative:
    case CSSPropertyPrefix:
    case CSSPropertySuffix:
    case CSSPropertyRange:
    case CSSPropertyPad:
    case CSSPropertyFallback:
    case CSSPropertySymbols:
    case CSSPropertyAdditiveSymbols:
        return nullptr;

    // @property descriptors.
    case CSSPropertyInherits:
    case CSSPropertyInitialValue:
    case CSSPropertySyntax:
        return nullptr;

    /* Unimplemented @font-face properties */
    case CSSPropertySrc:
    case CSSPropertyUnicodeRange:
    case CSSPropertyFontDisplay:
        return nullptr;

    // Unimplemented @font-palette-values properties
    case CSSPropertyBasePalette:
    case CSSPropertyOverrideColors:
        return nullptr;

    /* Other unimplemented properties */
    case CSSPropertyPage: // for @page
    case CSSPropertySize: // for @page
        return nullptr;

    /* Unimplemented -webkit- properties */
    case CSSPropertyWebkitMask:
    case CSSPropertyPerspectiveOriginX:
    case CSSPropertyPerspectiveOriginY:
    case CSSPropertyWebkitTextStroke:
    case CSSPropertyTransformOriginX:
    case CSSPropertyTransformOriginY:
    case CSSPropertyTransformOriginZ:
        return nullptr;

    case CSSPropertyBufferedRendering:
    case CSSPropertyClipRule:
    case CSSPropertyFloodColor:
    case CSSPropertyFloodOpacity:
    case CSSPropertyLightingColor:
    case CSSPropertyStopColor:
    case CSSPropertyStopOpacity:
    case CSSPropertyColorInterpolation:
    case CSSPropertyColorInterpolationFilters:
    case CSSPropertyFill:
    case CSSPropertyFillOpacity:
    case CSSPropertyFillRule:
    case CSSPropertyMarker:
    case CSSPropertyMarkerEnd:
    case CSSPropertyMarkerMid:
    case CSSPropertyMarkerStart:
    case CSSPropertyMaskType:
    case CSSPropertyShapeRendering:
    case CSSPropertyStroke:
    case CSSPropertyStrokeDasharray:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyAlignmentBaseline:
    case CSSPropertyBaselineShift:
    case CSSPropertyDominantBaseline:
    case CSSPropertyGlyphOrientationHorizontal:
    case CSSPropertyGlyphOrientationVertical:
    case CSSPropertyKerning:
    case CSSPropertyTextAnchor:
    case CSSPropertyVectorEffect:
        return svgPropertyValue(propertyID);
    case CSSPropertyCustom:
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

bool ComputedStyleExtractor::propertyMatches(CSSPropertyID propertyID, const CSSValue* value)
{
    if (!m_element)
        return false;
    if (propertyID == CSSPropertyFontSize && is<CSSPrimitiveValue>(*value)) {
        m_element->document().updateLayoutIgnorePendingStylesheets();
        if (auto* style = m_element->computedStyle(m_pseudoElementSpecifier)) {
            if (CSSValueID sizeIdentifier = style->fontDescription().keywordSizeAsIdentifier()) {
                auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
                if (primitiveValue.isValueID() && primitiveValue.valueID() == sizeIdentifier)
                    return true;
            }
        }
    }
    RefPtr<CSSValue> computedValue = propertyValue(propertyID);
    return computedValue && value && computedValue->equals(*value);
}

Ref<CSSValueList> ComputedStyleExtractor::getCSSPropertyValuesForShorthandProperties(const StylePropertyShorthand& shorthand)
{
    auto list = CSSValueList::createSpaceSeparated();
    for (auto longhand : shorthand)
        list->append(propertyValue(longhand, UpdateLayout::No).releaseNonNull());
    return list;
}

RefPtr<CSSValueList> ComputedStyleExtractor::getCSSPropertyValuesFor2SidesShorthand(const StylePropertyShorthand& shorthand)
{
    auto list = CSSValueList::createSpaceSeparated();

    // Assume the properties are in the usual order start, end.
    auto startValue = propertyValue(shorthand.properties()[0], UpdateLayout::No);
    auto endValue = propertyValue(shorthand.properties()[1], UpdateLayout::No);

    // All 2 properties must be specified.
    if (!startValue || !endValue)
        return nullptr;

    bool showEnd = !compareCSSValuePtr(startValue, endValue);

    list->append(startValue.releaseNonNull());
    if (showEnd)
        list->append(endValue.releaseNonNull());

    return list;
}

RefPtr<CSSValueList> ComputedStyleExtractor::getCSSPropertyValuesFor4SidesShorthand(const StylePropertyShorthand& shorthand)
{
    auto list = CSSValueList::createSpaceSeparated();

    // Assume the properties are in the usual order top, right, bottom, left.
    auto topValue = propertyValue(shorthand.properties()[0], UpdateLayout::No);
    auto rightValue = propertyValue(shorthand.properties()[1], UpdateLayout::No);
    auto bottomValue = propertyValue(shorthand.properties()[2], UpdateLayout::No);
    auto leftValue = propertyValue(shorthand.properties()[3], UpdateLayout::No);

    // All 4 properties must be specified.
    if (!topValue || !rightValue || !bottomValue || !leftValue)
        return nullptr;

    bool showLeft = !compareCSSValuePtr(rightValue, leftValue);
    bool showBottom = !compareCSSValuePtr(topValue, bottomValue) || showLeft;
    bool showRight = !compareCSSValuePtr(topValue, rightValue) || showBottom;

    list->append(topValue.releaseNonNull());
    if (showRight)
        list->append(rightValue.releaseNonNull());
    if (showBottom)
        list->append(bottomValue.releaseNonNull());
    if (showLeft)
        list->append(leftValue.releaseNonNull());

    return list;
}

Ref<CSSValueList> ComputedStyleExtractor::getCSSPropertyValuesForGridShorthand(const StylePropertyShorthand& shorthand)
{
    auto list = CSSValueList::createSlashSeparated();
    for (auto longhand : shorthand)
        list->append(propertyValue(longhand, UpdateLayout::No).releaseNonNull());
    return list;
}

Ref<MutableStyleProperties> ComputedStyleExtractor::copyProperties(Span<const CSSPropertyID> properties)
{
    Vector<CSSProperty> vector;
    vector.reserveInitialCapacity(properties.size());
    for (auto property : properties) {
        if (auto value = propertyValue(property))
            vector.uncheckedAppend(CSSProperty(property, WTFMove(value), false));
    }
    vector.shrinkToFit();
    return MutableStyleProperties::create(WTFMove(vector));
}

Ref<MutableStyleProperties> ComputedStyleExtractor::copyProperties()
{
    return MutableStyleProperties::create(WTF::compactMap(allLonghandCSSProperties(), [this] (auto property) -> std::optional<CSSProperty> {
        auto value = propertyValue(property);
        if (!value)
            return std::nullopt;
        return { { property, WTFMove(value) } };
    }));
}

size_t ComputedStyleExtractor::getLayerCount(CSSPropertyID property)
{
    ASSERT(property == CSSPropertyBackground || property == CSSPropertyMask);
    if (!m_element)
        return 0;

    std::unique_ptr<RenderStyle> ownedStyle;
    const RenderStyle* style = computeRenderStyleForProperty(*m_element, m_pseudoElementSpecifier, property, ownedStyle, nullptr);
    if (!style)
        return 0;

    auto& layers = property == CSSPropertyMask ? style->maskLayers() : style->backgroundLayers();

    size_t layerCount = 0;
    for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
        layerCount++;
    if (layerCount == 1 && property == CSSPropertyMask && !layers.image())
        return 0;
    return layerCount;
}

Ref<CSSValue> ComputedStyleExtractor::getFillLayerPropertyShorthandValue(CSSPropertyID property, const StylePropertyShorthand& propertiesBeforeSlashSeparator, const StylePropertyShorthand& propertiesAfterSlashSeparator, CSSPropertyID lastLayerProperty)
{
    ASSERT(property == CSSPropertyBackground || property == CSSPropertyMask);
    size_t layerCount = getLayerCount(property);
    if (!layerCount) {
        ASSERT(property == CSSPropertyMask);
        return CSSPrimitiveValue::create(CSSValueNone);
    }

    auto lastValue = lastLayerProperty != CSSPropertyInvalid ? propertyValue(lastLayerProperty, UpdateLayout::No) : nullptr;
    auto before = getCSSPropertyValuesForShorthandProperties(propertiesBeforeSlashSeparator);
    auto after = getCSSPropertyValuesForShorthandProperties(propertiesAfterSlashSeparator);

    // The computed properties are returned as lists of properties, with a list of layers in each.
    // We want to swap that around to have a list of layers, with a list of properties in each.

    auto layers = CSSValueList::createCommaSeparated();

    for (size_t i = 0; i < layerCount; i++) {
        auto list = CSSValueList::createSlashSeparated();
        auto beforeList = CSSValueList::createSpaceSeparated();

        if (i == layerCount - 1 && lastValue)
            beforeList->append(*lastValue);

        for (size_t j = 0; j < propertiesBeforeSlashSeparator.length(); j++) {
            auto& value = *before->item(j);
            beforeList->append(layerCount == 1 ? value : *downcast<CSSValueList>(value).item(i));
        }
        list->append(beforeList);

        auto afterList = CSSValueList::createSpaceSeparated();
        for (size_t j = 0; j < propertiesAfterSlashSeparator.length(); j++) {
            auto& value = *after->item(j);
            afterList->append(layerCount == 1 ? value : *downcast<CSSValueList>(value).item(i));
        }
        list->append(afterList);

        if (layerCount == 1)
            return list;

        layers->append(list);
    }

    return layers;
}


Ref<CSSValue> ComputedStyleExtractor::getBackgroundShorthandValue()
{
    static const CSSPropertyID propertiesBeforeSlashSeparator[] = { CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat, CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition };
    static const CSSPropertyID propertiesAfterSlashSeparator[] = { CSSPropertyBackgroundSize, CSSPropertyBackgroundOrigin, CSSPropertyBackgroundClip };

    return getFillLayerPropertyShorthandValue(CSSPropertyBackground, StylePropertyShorthand(CSSPropertyBackground, propertiesBeforeSlashSeparator), StylePropertyShorthand(CSSPropertyBackground, propertiesAfterSlashSeparator), CSSPropertyBackgroundColor);
}

Ref<CSSValue> ComputedStyleExtractor::getMaskShorthandValue()
{
    static const CSSPropertyID propertiesBeforeSlashSeperator[2] = { CSSPropertyMaskImage, CSSPropertyMaskPosition };
    static const CSSPropertyID propertiesAfterSlashSeperator[6] = { CSSPropertyMaskSize, CSSPropertyMaskRepeat, CSSPropertyMaskOrigin, CSSPropertyMaskClip, CSSPropertyMaskComposite, CSSPropertyMaskMode };

    return getFillLayerPropertyShorthandValue(CSSPropertyMask, StylePropertyShorthand(CSSPropertyMask, propertiesBeforeSlashSeperator), StylePropertyShorthand(CSSPropertyMask, propertiesAfterSlashSeperator), CSSPropertyInvalid);
}

} // namespace WebCore
