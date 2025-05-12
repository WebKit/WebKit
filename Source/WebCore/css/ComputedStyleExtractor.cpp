/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSAppleColorFilterPropertyValue.h"
#include "CSSBasicShapeValue.h"
#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBoxShadowPropertyValue.h"
#include "CSSColorSchemeValue.h"
#include "CSSCounterValue.h"
#include "CSSDynamicRangeLimitValue.h"
#include "CSSEasingFunctionValue.h"
#include "CSSFilterPropertyValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSPathValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSProperty.h"
#include "CSSPropertyParserConsumer+Anchor.h"
#include "CSSQuadValue.h"
#include "CSSRatioValue.h"
#include "CSSRayValue.h"
#include "CSSRectValue.h"
#include "CSSReflectValue.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSScrollValue.h"
#include "CSSSerializationContext.h"
#include "CSSTextShadowPropertyValue.h"
#include "CSSTransformListValue.h"
#include "CSSURLValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "CSSViewValue.h"
#include "ComposedTreeAncestorIterator.h"
#include "ContainerNodeInlines.h"
#include "ContentData.h"
#include "CursorList.h"
#include "CustomPropertyRegistry.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "FontCascade.h"
#include "FontSelectionValueInlines.h"
#include "GridPositionsResolver.h"
#include "HTMLFrameOwnerElement.h"
#include "NodeRenderStyle.h"
#include "PerspectiveTransformOperation.h"
#include "PseudoElementIdentifier.h"
#include "QuotesData.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderObjectInlines.h"
#include "RotateTransformOperation.h"
#include "SVGElement.h"
#include "SVGRenderStyle.h"
#include "ScaleTransformOperation.h"
#include "ScrollTimeline.h"
#include "SkewTransformOperation.h"
#include "StyleAppleColorFilterProperty.h"
#include "StyleBoxShadow.h"
#include "StyleColorScheme.h"
#include "StyleCornerShapeValue.h"
#include "StyleDynamicRangeLimit.h"
#include "StyleEasingFunction.h"
#include "StyleFilterProperty.h"
#include "StyleInterpolation.h"
#include "StyleLineBoxContain.h"
#include "StylePathData.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"
#include "StyleReflection.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleScrollMargin.h"
#include "StyleScrollPadding.h"
#include "StyleTextShadow.h"
#include "Styleable.h"
#include "TimelineRange.h"
#include "TransformOperationData.h"
#include "TranslateTransformOperation.h"
#include "ViewTimeline.h"
#include "WebAnimationUtilities.h"

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ComputedStyleExtractor);

enum class AdjustPixelValuesForComputedStyle : bool { No, Yes };
enum class ForcedLayout : uint8_t { No, Yes, ParentDocument };

using PhysicalDirection = BoxSide;
using FlowRelativeDirection = LogicalBoxSide;

static const RenderStyle& formattingContextRootStyle(const RenderBox& renderer)
{
    if (auto* ancestorToUse = (renderer.isFlexItem() || renderer.isGridItem()) ? renderer.parent() : renderer.containingBlock())
        return ancestorToUse->style();
    ASSERT_NOT_REACHED();
    return renderer.style();
};

template<typename ConvertibleType> Ref<CSSPrimitiveValue> valueForConvertibleType(const ConvertibleType& value)
{
    return CSSPrimitiveValue::create(toCSSValueID(value));
}

static Ref<CSSPrimitiveValue> valueForZoomAdjustedFloatLength(const RenderStyle& style, double value)
{
    return CSSPrimitiveValue::create(adjustFloatForAbsoluteZoom(value, style), CSSUnitType::CSS_PX);
}

static Ref<CSSPrimitiveValue> valueForLength(const RenderStyle& style, const Length& value)
{
    return CSSPrimitiveValue::create(value, style);
}

static Ref<CSSPrimitiveValue> valueForNumber(float value)
{
    return CSSPrimitiveValue::create(value);
}

static Ref<CSSPrimitiveValue> valueForInteger(int value)
{
    return CSSPrimitiveValue::createInteger(value);
}

template<auto isKeywordAccessor, CSSValueID keyword, auto valueIDAccessor>
static Ref<CSSPrimitiveValue> valueForKeywordOrCSSValueID(const RenderStyle& style)
{
    if ((style.*isKeywordAccessor)())
        return CSSPrimitiveValue::create(keyword);
    return valueForConvertibleType((style.*valueIDAccessor)());
}

template<auto isKeywordAccessor, CSSValueID keyword, auto numberAccessor>
static Ref<CSSPrimitiveValue> valueForKeywordOrNumber(const RenderStyle& style)
{
    if ((style.*isKeywordAccessor)())
        return CSSPrimitiveValue::create(keyword);
    return CSSPrimitiveValue::create((style.*numberAccessor)());
}

template<auto isKeywordAccessor, CSSValueID keyword, auto integerAccessor>
static Ref<CSSPrimitiveValue> valueForKeywordOrInteger(const RenderStyle& style)
{
    if ((style.*isKeywordAccessor)())
        return CSSPrimitiveValue::create(keyword);
    return CSSPrimitiveValue::createInteger((style.*integerAccessor)());
}

template<auto isKeywordAccessor, CSSValueID keyword, auto floatLengthAccessor>
static Ref<CSSPrimitiveValue> valueForKeywordOrZoomAdjustedFloatLength(const RenderStyle& style)
{
    if ((style.*isKeywordAccessor)())
        return CSSPrimitiveValue::create(keyword);
    return valueForZoomAdjustedFloatLength(style, (style.*floatLengthAccessor)());
}

template<auto isKeywordAccessor, CSSValueID keyword, auto floatLengthAccessor>
static Ref<CSSPrimitiveValue> valueForKeywordIfNegatedOrZoomAdjustedFloatLength(const RenderStyle& style)
{
    if (!(style.*isKeywordAccessor)())
        return CSSPrimitiveValue::create(keyword);
    return valueForZoomAdjustedFloatLength(style, (style.*floatLengthAccessor)());
}

template<auto isKeywordAccessor, CSSValueID keyword, auto lengthAccessor>
static Ref<CSSPrimitiveValue> valueForKeywordOrZoomAdjustedLength(const RenderStyle& style)
{
    if ((style.*isKeywordAccessor)())
        return CSSPrimitiveValue::create(keyword);
    return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, (style.*lengthAccessor)());
}

static Ref<CSSPrimitiveValue> valueForPercentageOrZoomAdjustedLength(const RenderStyle& style, const Length& length)
{
    if (length.isPercent())
        return CSSPrimitiveValue::create(length.percent(), CSSUnitType::CSS_PERCENTAGE);
    return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, length);
}

static Ref<CSSPrimitiveValue> valueForAutoOrZoomAdjustedLength(const RenderStyle& style, const Length& length)
{
    if (length.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, length);
}

static Ref<CSSPrimitiveValue> valueForAutoOrString(const RenderStyle&, const AtomString& string)
{
    if (string.isNull())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return CSSPrimitiveValue::create(string);
}

template<CSSValueID keyword> static Ref<CSSPrimitiveValue> valueForKeywordOrCustomIdent(const RenderStyle&, const AtomString& string)
{
    if (string.isNull())
        return CSSPrimitiveValue::create(keyword);
    return CSSPrimitiveValue::createCustomIdent(string);
}

class OrderedNamedLinesCollector {
    WTF_MAKE_NONCOPYABLE(OrderedNamedLinesCollector);
public:
    OrderedNamedLinesCollector(const RenderStyle& style, bool isRowAxis)
        : m_orderedNamedGridLines(isRowAxis ? style.orderedNamedGridColumnLines() : style.orderedNamedGridRowLines())
        , m_orderedNamedAutoRepeatGridLines(isRowAxis ? style.autoRepeatOrderedNamedGridColumnLines() : style.autoRepeatOrderedNamedGridRowLines())
    {
    }
    virtual ~OrderedNamedLinesCollector() = default;

    bool isEmpty() const { return m_orderedNamedGridLines.map.isEmpty() && m_orderedNamedAutoRepeatGridLines.map.isEmpty(); }
    virtual void collectLineNamesForIndex(Vector<String>&, unsigned index) const = 0;

    virtual int namedGridLineCount() const { return m_orderedNamedGridLines.map.size(); }

protected:

    enum class NamedLinesType : bool { NamedLines, AutoRepeatNamedLines };
    void appendLines(Vector<String>&, unsigned index, NamedLinesType) const;

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

    void collectLineNamesForIndex(Vector<String>&, unsigned index) const override;

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
        , m_autoRepeatLineSetListLength((isRowAxis ? style.autoRepeatOrderedNamedGridColumnLines() : style.autoRepeatOrderedNamedGridRowLines()).map.size())
        , m_totalLines(totalTracksCount + 1)
    {
        if (!m_autoRepeatLineSetListLength) {
            m_autoRepeatTotalLineSets = 0;
            return;
        }
        unsigned named = (isRowAxis ? style.orderedNamedGridColumnLines() : style.orderedNamedGridRowLines()).map.size();
        if (named >= m_totalLines) {
            m_autoRepeatTotalLineSets = 0;
            return;
        }
        m_autoRepeatTotalLineSets = (m_totalLines - named) / m_autoRepeatLineSetListLength;
        m_autoRepeatTotalLineSets *= m_autoRepeatLineSetListLength;
    }

    void collectLineNamesForIndex(Vector<String>&, unsigned index) const override;

    int namedGridLineCount() const override { return m_totalLines; }
private:
    unsigned m_insertionPoint;
    unsigned m_autoRepeatTotalLineSets;
    unsigned m_autoRepeatLineSetListLength;
    unsigned m_totalLines;
};

void OrderedNamedLinesCollector::appendLines(Vector<String>& lineNames, unsigned index, NamedLinesType type) const
{
    auto& map = (type == NamedLinesType::NamedLines ? m_orderedNamedGridLines : m_orderedNamedAutoRepeatGridLines).map;
    auto it = map.find(index);
    if (it == map.end())
        return;
    for (auto& name : it->value)
        lineNames.append(name);
}

void OrderedNamedLinesCollectorInGridLayout::collectLineNamesForIndex(Vector<String>& lineNamesValue, unsigned i) const
{
    ASSERT(!isEmpty());
    if (!m_autoRepeatTrackListLength || i < m_insertionPoint) {
        appendLines(lineNamesValue, i, NamedLinesType::NamedLines);
        return;
    }

    ASSERT(m_autoRepeatTotalTracks);

    if (i > m_insertionPoint + m_autoRepeatTotalTracks) {
        appendLines(lineNamesValue, i - (m_autoRepeatTotalTracks - 1), NamedLinesType::NamedLines);
        return;
    }

    if (i == m_insertionPoint) {
        appendLines(lineNamesValue, i, NamedLinesType::NamedLines);
        appendLines(lineNamesValue, 0, NamedLinesType::AutoRepeatNamedLines);
        return;
    }

    if (i == m_insertionPoint + m_autoRepeatTotalTracks) {
        appendLines(lineNamesValue, m_autoRepeatTrackListLength, NamedLinesType::AutoRepeatNamedLines);
        appendLines(lineNamesValue, m_insertionPoint + 1, NamedLinesType::NamedLines);
        return;
    }

    unsigned autoRepeatIndexInFirstRepetition = (i - m_insertionPoint) % m_autoRepeatTrackListLength;
    if (!autoRepeatIndexInFirstRepetition && i > m_insertionPoint)
        appendLines(lineNamesValue, m_autoRepeatTrackListLength, NamedLinesType::AutoRepeatNamedLines);
    appendLines(lineNamesValue, autoRepeatIndexInFirstRepetition, NamedLinesType::AutoRepeatNamedLines);
}

void OrderedNamedLinesCollectorInSubgridLayout::collectLineNamesForIndex(Vector<String>& lineNamesValue, unsigned i) const
{
    if (!m_autoRepeatLineSetListLength || i < m_insertionPoint) {
        appendLines(lineNamesValue, i, NamedLinesType::NamedLines);
        return;
    }

    if (i >= m_insertionPoint + m_autoRepeatTotalLineSets) {
        appendLines(lineNamesValue, i - m_autoRepeatTotalLineSets, NamedLinesType::NamedLines);
        return;
    }

    unsigned autoRepeatIndexInFirstRepetition = (i - m_insertionPoint) % m_autoRepeatLineSetListLength;
    appendLines(lineNamesValue, autoRepeatIndexInFirstRepetition, NamedLinesType::AutoRepeatNamedLines);
}

static Ref<CSSValue> valueForFilter(const RenderStyle& style, const FilterOperations& filterOperations)
{
    return CSSFilterPropertyValue::create(Style::toCSSFilterProperty(filterOperations, style));
}

static Ref<CSSValue> valueForAppleColorFilter(const RenderStyle& style, const FilterOperations& filterOperations)
{
    return CSSAppleColorFilterPropertyValue::create(Style::toCSSAppleColorFilterProperty(filterOperations, style));
}

static Ref<CSSValue> valueForScrollMarginEdge(const RenderStyle& style, const Style::ScrollMarginEdge& edge)
{
    return edge.toCSS(style);
}

static Ref<CSSValue> valueForScrollPaddingEdge(const RenderStyle& style, const Style::ScrollPaddingEdge& edge)
{
    return edge.toCSS(style);
}

static Ref<CSSValue> valueForCornerShape(const RenderStyle& style, const Style::CornerShapeValue& cornerShape)
{
    return Style::toCSSValue(cornerShape, style);
}

static Ref<CSSValue> valueForDynamicRangeLimit(const RenderStyle& style)
{
    return CSSDynamicRangeLimitValue::create(Style::toCSS(style.dynamicRangeLimit(), style));
}

#if ENABLE(DARK_MODE_CSS)
static Ref<CSSValue> valueForColorScheme(const RenderStyle& style)
{
    return CSSColorSchemeValue::create(Style::toCSS(style.colorScheme(), style));
}
#endif

static RefPtr<CSSPrimitiveValue> valueForGlyphOrientation(GlyphOrientation orientation)
{
    switch (orientation) {
    case GlyphOrientation::Degrees0:
        return CSSPrimitiveValue::create(0.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Degrees90:
        return CSSPrimitiveValue::create(90.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Degrees180:
        return CSSPrimitiveValue::create(180.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Degrees270:
        return CSSPrimitiveValue::create(270.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Auto:
        return nullptr;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static RefPtr<CSSPrimitiveValue> valueForGlyphOrientationHorizontal(const RenderStyle& style)
{
    return valueForGlyphOrientation(style.svgStyle().glyphOrientationHorizontal());
}

static RefPtr<CSSPrimitiveValue> valueForGlyphOrientationVertical(const RenderStyle& style)
{
    auto glyphOrientationVertical = style.svgStyle().glyphOrientationVertical();
    if (auto value = valueForGlyphOrientation(glyphOrientationVertical))
        return value;

    if (glyphOrientationVertical == GlyphOrientation::Auto)
        return CSSPrimitiveValue::create(CSSValueAuto);

    return nullptr;
}

static Ref<CSSValue> valueForStrokeDasharray(const RenderStyle& style)
{
    auto& dashes = style.svgStyle().strokeDashArray();
    if (dashes.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& length : dashes) {
        auto primitiveValue = length.toCSSPrimitiveValue();
        // Computed lengths should always be in 'px' unit.
        if (primitiveValue->isLength() && primitiveValue->primitiveType() != CSSUnitType::CSS_PX)
            list.append(CSSPrimitiveValue::create(primitiveValue->resolveAsLengthDeprecated(), CSSUnitType::CSS_PX));
        else
            list.append(WTFMove(primitiveValue));
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static RefPtr<CSSValue> valueForBaselineShift(const RenderStyle& style, RefPtr<Element> element)
{
    switch (style.svgStyle().baselineShift()) {
    case BaselineShift::Baseline:
        return CSSPrimitiveValue::create(CSSValueBaseline);
    case BaselineShift::Super:
        return CSSPrimitiveValue::create(CSSValueSuper);
    case BaselineShift::Sub:
        return CSSPrimitiveValue::create(CSSValueSub);
    case BaselineShift::Length: {
        auto computedValue = style.svgStyle().baselineShiftValue().toCSSPrimitiveValue(element.get());
        if (computedValue->isLength() && computedValue->primitiveType() != CSSUnitType::CSS_PX)
            return CSSPrimitiveValue::create(computedValue->resolveAsLengthDeprecated(), CSSUnitType::CSS_PX);
        return computedValue;
    }
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

static RefPtr<CSSValue> valueForMarkerURL(const RenderStyle& style, const Style::URL& marker)
{
    if (marker.isNone())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSURLValue::create(Style::toCSS(marker, style));
}

static Ref<CSSColorValue> valueForColor(const RenderStyle& style, CSSValuePool& pool, const Style::Color& color)
{
    // This function does NOT look at visited information, so that computed style doesn't expose that.
    return pool.createColorValue(style.colorResolvingCurrentColor(color));
}

static Ref<CSSValue> valueForSVGPaint(const RenderStyle& style, CSSValuePool& pool, SVGPaintType paintType, const Style::URL& url, const Style::Color& color)
{
    if (paintType >= SVGPaintType::URINone) {
        CSSValueListBuilder values;
        values.append(CSSURLValue::create(Style::toCSS(url, style)));
        if (paintType == SVGPaintType::URINone)
            values.append(CSSPrimitiveValue::create(CSSValueNone));
        else if (paintType == SVGPaintType::URICurrentColor || paintType == SVGPaintType::URIRGBColor)
            values.append(valueForColor(style, pool, color));
        return CSSValueList::createSpaceSeparated(WTFMove(values));
    }
    if (paintType == SVGPaintType::None)
        return CSSPrimitiveValue::create(CSSValueNone);
    return valueForColor(style, pool, color);
}

static RefPtr<CSSValue> valueForAccentColor(const RenderStyle& style, CSSValuePool& pool)
{
    if (style.hasAutoAccentColor())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return valueForColor(style, pool, style.accentColor());
}

template<CSSPropertyID colorPropertyID, auto colorAccessor>
static RefPtr<CSSValue> valueForColor(const RenderStyle& style, bool allowVisitedStyle, CSSValuePool& pool)
{
    if (allowVisitedStyle)
        return pool.createColorValue(style.visitedDependentColor(colorPropertyID));
    return valueForColor(style, pool, (style.*colorAccessor)());
}

template<typename MappingFunctor> static RefPtr<CSSValue> valueForFillLayerProperty(const RenderStyle& style, const FillLayer& layers, MappingFunctor&& mapper)
{
    if (!layers.next())
        return mapper(style, layers);
    CSSValueListBuilder list;
    for (auto* layer = &layers; layer; layer = layer->next())
        list.append(mapper(style, *layer));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

template<typename MappingFunctor> static RefPtr<CSSValue> valueForBackgroundLayerProperty(const RenderStyle& style, MappingFunctor&& mapper)
{
    return valueForFillLayerProperty(style, style.backgroundLayers(), mapper);
}

template<typename MappingFunctor> static RefPtr<CSSValue> valueForMaskLayerProperty(const RenderStyle& style, MappingFunctor&& mapper)
{
    return valueForFillLayerProperty(style, style.maskLayers(), mapper);
}

static RefPtr<CSSValue> valueForBackgroundOrMaskImage(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto& style, auto& layer) -> Ref<CSSValue> {
        if (layer.image())
            return layer.image()->computedStyleValue(style);
        return CSSPrimitiveValue::create(CSSValueNone);
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForBackgroundSize(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto& style, auto& layer) -> Ref<CSSValue> {
        auto fillSize = layer.size();

        if (fillSize.type == FillSizeType::Contain)
            return CSSPrimitiveValue::create(CSSValueContain);

        if (fillSize.type == FillSizeType::Cover)
            return CSSPrimitiveValue::create(CSSValueCover);

        if (fillSize.size.height.isAuto() && fillSize.size.width.isAuto())
            return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, fillSize.size.width);

        return CSSValueList::createSpaceSeparated(
            ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, fillSize.size.width),
            ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, fillSize.size.height)
        );
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForMaskSize(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto& style, auto& layer) -> Ref<CSSValue> {
        auto fillSize = layer.size();

        if (fillSize.type == FillSizeType::Contain)
            return CSSPrimitiveValue::create(CSSValueContain);

        if (fillSize.type == FillSizeType::Cover)
            return CSSPrimitiveValue::create(CSSValueCover);

        if (fillSize.size.height.isAuto())
            return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, fillSize.size.width);

        return CSSValueList::createSpaceSeparated(
            ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, fillSize.size.width),
            ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, fillSize.size.height)
        );
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForBackgroundOrMaskRepeat(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto&, auto& layer) -> Ref<CSSValue> {
        auto repeat = layer.repeat();

        if (repeat.x == repeat.y)
            return valueForConvertibleType(repeat.x);

        if (repeat.x == FillRepeat::Repeat && repeat.y == FillRepeat::NoRepeat)
            return CSSPrimitiveValue::create(CSSValueRepeatX);

        if (repeat.x == FillRepeat::NoRepeat && repeat.y == FillRepeat::Repeat)
            return CSSPrimitiveValue::create(CSSValueRepeatY);

        return CSSValueList::createSpaceSeparated(
            valueForConvertibleType(repeat.x),
            valueForConvertibleType(repeat.y)
        );
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForWebkitMaskSourceType(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto&, auto& layer) -> Ref<CSSValue> {
        auto maskMode = layer.maskMode();

        switch (maskMode) {
        case MaskMode::Alpha:
            return CSSPrimitiveValue::create(CSSValueAlpha);
        case MaskMode::Luminance:
            return CSSPrimitiveValue::create(CSSValueLuminance);
        case MaskMode::MatchSource:
            // MatchSource is only available in the mask-mode property.
            return CSSPrimitiveValue::create(CSSValueAlpha);
        }
        ASSERT_NOT_REACHED();
        return CSSPrimitiveValue::create(CSSValueAlpha);
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForMaskMode(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto&, auto& layer) -> Ref<CSSValue> {
        auto maskMode = layer.maskMode();

        switch (maskMode) {
        case MaskMode::Alpha:
            return CSSPrimitiveValue::create(CSSValueAlpha);
        case MaskMode::Luminance:
            return CSSPrimitiveValue::create(CSSValueLuminance);
        case MaskMode::MatchSource:
            return CSSPrimitiveValue::create(CSSValueMatchSource);
        }
        ASSERT_NOT_REACHED();
        return CSSPrimitiveValue::create(CSSValueMatchSource);
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForWebkitMaskComposite(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto&, auto& layer) -> Ref<CSSValue> {
        return CSSPrimitiveValue::create(toCSSValueID(layer.composite(), CSSPropertyWebkitMaskComposite));
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForMaskComposite(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto&, auto& layer) -> Ref<CSSValue> {
        return CSSPrimitiveValue::create(toCSSValueID(layer.composite(), CSSPropertyMaskComposite));
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForBackgroundAttachment(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto&, auto& layer) -> Ref<CSSValue> {
        return valueForConvertibleType(layer.attachment());
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForBackgroundBlendMode(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto&, auto& layer) -> Ref<CSSValue> {
        return valueForConvertibleType(layer.blendMode());
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForBackgroundOrMaskClip(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto&, auto& layer) -> Ref<CSSValue> {
        return valueForConvertibleType(layer.clip());
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForBackgroundOrMaskOrigin(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto&, auto& layer) -> Ref<CSSValue> {
        return valueForConvertibleType(layer.origin());
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForBackgroundOrMaskPosition(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto& style, auto& layer) -> Ref<CSSValue> {
        return CSSValueList::createSpaceSeparated(CSSValueListBuilder {
            ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, layer.xPosition()),
            ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, layer.yPosition()),
        });
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForBackgroundOrMaskPositionX(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto& style, auto& layer) -> Ref<CSSValue> {
        return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, layer.xPosition());
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForBackgroundOrMaskPositionY(const RenderStyle& style, const FillLayer& layers)
{
    auto mapper = [](auto& style, auto& layer) -> Ref<CSSValue> {
        return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, layer.yPosition());
    };

    return valueForFillLayerProperty(style, layers, mapper);
}

static RefPtr<CSSValue> valueForBlockEllipsis(const RenderStyle& style)
{
    switch (style.blockEllipsis().type) {
    case BlockEllipsis::Type::None:
        return CSSPrimitiveValue::create(CSSValueNone);
    case BlockEllipsis::Type::Auto:
        return CSSPrimitiveValue::create(CSSValueAuto);
    case BlockEllipsis::Type::String:
        return CSSPrimitiveValue::create(style.blockEllipsis().string);
    default:
        ASSERT_NOT_REACHED();
    }
    return CSSPrimitiveValue::create(CSSValueNone);
}

static RefPtr<CSSValue> valueForBlockStepShorthandValue(const RenderStyle& style)
{
    CSSValueListBuilder list;
    if (style.blockStepSize())
        list.append(ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, *style.blockStepSize()));

    if (style.blockStepInsert() != RenderStyle::initialBlockStepInsert())
        list.append(valueForConvertibleType(style.blockStepInsert()));

    if (style.blockStepAlign() != RenderStyle::initialBlockStepAlign())
        list.append(valueForConvertibleType(style.blockStepAlign()));

    if (style.blockStepRound() != RenderStyle::initialBlockStepRound())
        list.append(valueForConvertibleType(style.blockStepRound()));

    if (!list.isEmpty())
        return CSSValueList::createSpaceSeparated(list);

    return CSSPrimitiveValue::create(CSSValueNone);
}

static RefPtr<CSSValue> valueForBlockStepSize(const RenderStyle& style)
{
    if (auto blockStepSize = style.blockStepSize())
        return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, *blockStepSize);
    return CSSPrimitiveValue::create(CSSValueNone);
}

static RefPtr<CSSValue> valueForBorderSpacing(const RenderStyle& style)
{
    return CSSValuePair::create(
        valueForZoomAdjustedFloatLength(style, style.horizontalBorderSpacing()),
        valueForZoomAdjustedFloatLength(style, style.verticalBorderSpacing())
    );
}

static RefPtr<CSSValue> valueForStyleImage(const RenderStyle& style, StyleImage* styleImage)
{
    if (styleImage)
        return styleImage->computedStyleValue(style);
    return CSSPrimitiveValue::create(CSSValueNone);
}

static RefPtr<CSSValue> valueForGapLength(const RenderStyle& style, const GapLength& gapLength)
{
    if (gapLength.isNormal())
        return CSSPrimitiveValue::create(CSSValueNormal);
    return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, gapLength.length());
}

static RefPtr<CSSValue> valueForTabSize(const RenderStyle& style)
{
    return CSSPrimitiveValue::create(style.tabSize().widthInPixels(1.0), style.tabSize().isSpaces() ? CSSUnitType::CSS_NUMBER : CSSUnitType::CSS_PX);
}

static RefPtr<CSSValue> valueForCursor(const RenderStyle& style)
{
    auto value = valueForConvertibleType(style.cursor());
    auto* cursors = style.cursors();
    if (!cursors || !cursors->size())
        return value;
    CSSValueListBuilder list;
    for (unsigned i = 0; i < cursors->size(); ++i) {
        if (auto* image = cursors->at(i).image())
            list.append(image->computedStyleValue(style));
    }
    list.append(WTFMove(value));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static RefPtr<CSSValue> valueForDirection(const RenderStyle& style, RefPtr<Element> element)
{
    auto direction = [&] {
        if (element == element->document().documentElement() && !style.hasExplicitlySetDirection())
            return RenderStyle::initialDirection();
        return style.writingMode().computedTextDirection();
    }();
    return valueForConvertibleType(direction);
}

static RefPtr<CSSValue> valueForWritingMode(const RenderStyle& style, RefPtr<Element> element)
{
    auto writingMode = [&] {
        if (element == element->document().documentElement() && !style.hasExplicitlySetWritingMode())
            return RenderStyle::initialWritingMode();
        return style.writingMode().computedWritingMode();
    }();
    return valueForConvertibleType(writingMode);
}

static RefPtr<CSSValue> valueForGridAutoFlow(const RenderStyle& style)
{
    CSSValueListBuilder list;
    ASSERT(style.isGridAutoFlowDirectionRow() || style.isGridAutoFlowDirectionColumn());
    if (style.isGridAutoFlowDirectionColumn())
        list.append(CSSPrimitiveValue::create(CSSValueColumn));
    else if (!style.isGridAutoFlowAlgorithmDense())
        list.append(CSSPrimitiveValue::create(CSSValueRow));

    if (style.isGridAutoFlowAlgorithmDense())
        list.append(CSSPrimitiveValue::create(CSSValueDense));

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static RefPtr<CSSValue> valueForGridTemplateAreas(const RenderStyle& style)
{
    if (!style.namedGridAreaRowCount()) {
        ASSERT(!style.namedGridAreaColumnCount());
        return CSSPrimitiveValue::create(CSSValueNone);
    }
    return CSSGridTemplateAreasValue::create(style.namedGridArea(), style.namedGridAreaRowCount(), style.namedGridAreaColumnCount());
}

static RefPtr<CSSValue> valueForWebkitTextCombine(const RenderStyle& style)
{
    if (style.textCombine() == TextCombine::All)
        return CSSPrimitiveValue::create(CSSValueHorizontal);
    return valueForConvertibleType(style.textCombine());
}

template<CSSValueID specialCase> static RefPtr<CSSValue> valueForWebkitHyphenateLimit(const RenderStyle&, short limit)
{
    if (limit < 0)
        return CSSPrimitiveValue::create(specialCase);
    return CSSPrimitiveValue::create(limit);
}

static RefPtr<CSSValue> valueForImageOrientation(const RenderStyle& style)
{
    if (style.imageOrientation() == ImageOrientation::Orientation::FromImage)
        return CSSPrimitiveValue::create(CSSValueFromImage);
    return CSSPrimitiveValue::create(CSSValueNone);
}

static RefPtr<CSSValue> valueForLetterSpacing(const RenderStyle& style)
{
    auto& spacing = style.computedLetterSpacing();
    if (spacing.isFixed()) {
        if (spacing.isZero())
            return CSSPrimitiveValue::create(CSSValueNormal);
        return valueForZoomAdjustedFloatLength(style, spacing.value());
    }
    return CSSPrimitiveValue::create(spacing, style);
}

static RefPtr<CSSValue> valueForWordSpacing(const RenderStyle& style)
{
    auto& spacing = style.computedWordSpacing();
    if (spacing.isFixed())
        return valueForZoomAdjustedFloatLength(style, spacing.value());
    return CSSPrimitiveValue::create(spacing, style);
}

static RefPtr<CSSValue> valueForWebkitLineClamp(const RenderStyle& style)
{
    if (style.lineClamp().isNone())
        return CSSPrimitiveValue::create(CSSValueNone);
    if (style.lineClamp().isPercentage())
        return CSSPrimitiveValue::create(style.lineClamp().value(), CSSUnitType::CSS_PERCENTAGE);
    return CSSPrimitiveValue::createInteger(style.lineClamp().value());
}

static RefPtr<CSSValue> valueForListStyleType(const RenderStyle& style)
{
    if (style.listStyleType().type == ListStyleType::Type::String)
        return CSSPrimitiveValue::create(style.listStyleType().identifier);
    if (style.listStyleType().type == ListStyleType::Type::CounterStyle)
        return CSSPrimitiveValue::createCustomIdent(style.listStyleType().identifier);
    return valueForConvertibleType(style.listStyleType().type);
}

static RefPtr<CSSValue> valueForBoxSizing(const RenderStyle& style)
{
    if (style.boxSizing() == BoxSizing::ContentBox)
        return CSSPrimitiveValue::create(CSSValueContentBox);
    return CSSPrimitiveValue::create(CSSValueBorderBox);
}

static RefPtr<CSSValue> valueForAspectRatio(const RenderStyle& style)
{
    switch (style.aspectRatioType()) {
    case AspectRatioType::Auto:
        return CSSPrimitiveValue::create(CSSValueAuto);
    case AspectRatioType::AutoZero:
    case AspectRatioType::Ratio:
        return CSSRatioValue::create(CSS::Ratio { style.aspectRatioWidth(), style.aspectRatioHeight() });
    case AspectRatioType::AutoAndRatio:
        return CSSValueList::createSpaceSeparated(
            CSSPrimitiveValue::create(CSSValueAuto),
            CSSRatioValue::create(CSS::Ratio { style.aspectRatioWidth(), style.aspectRatioHeight() })
        );
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

static RefPtr<CSSValue> valueForContain(const RenderStyle& style)
{
    auto containment = style.contain();
    if (!containment)
        return CSSPrimitiveValue::create(CSSValueNone);
    if (containment == RenderStyle::strictContainment())
        return CSSPrimitiveValue::create(CSSValueStrict);
    if (containment == RenderStyle::contentContainment())
        return CSSPrimitiveValue::create(CSSValueContent);
    CSSValueListBuilder list;
    if (containment & Containment::Size)
        list.append(CSSPrimitiveValue::create(CSSValueSize));
    if (containment & Containment::InlineSize)
        list.append(CSSPrimitiveValue::create(CSSValueInlineSize));
    if (containment & Containment::Layout)
        list.append(CSSPrimitiveValue::create(CSSValueLayout));
    if (containment & Containment::Style)
        list.append(CSSPrimitiveValue::create(CSSValueStyle));
    if (containment & Containment::Paint)
        list.append(CSSPrimitiveValue::create(CSSValuePaint));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static RefPtr<CSSValue> valueForMaxLines(const RenderStyle& style)
{
    if (!style.maxLines())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSPrimitiveValue::create(style.maxLines());
}

static RefPtr<CSSValue> valueForScrollBehavior(const RenderStyle& style)
{
    if (!style.useSmoothScrolling())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return CSSPrimitiveValue::create(CSSValueSmooth);
}

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
static RefPtr<CSSValue> valueForWebkitOverflowScrolling(const RenderStyle& style)
{
    if (!style.useTouchOverflowScrolling())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return CSSPrimitiveValue::create(CSSValueTouch);
}
#endif

static RefPtr<CSSValue> valueForWebkitInitialLetter(const RenderStyle& style)
{
    auto drop = !style.initialLetterDrop() ? CSSPrimitiveValue::create(CSSValueNormal) : CSSPrimitiveValue::create(style.initialLetterDrop());
    auto size = !style.initialLetterHeight() ? CSSPrimitiveValue::create(CSSValueNormal) : CSSPrimitiveValue::create(style.initialLetterHeight());
    return CSSValuePair::create(WTFMove(drop), WTFMove(size));
}

static RefPtr<CSSValue> valueForClip(const RenderStyle& style)
{
    if (!style.hasClip())
        return CSSPrimitiveValue::create(CSSValueAuto);

    if (style.clip().top().isAuto() && style.clip().right().isAuto() && style.clip().top().isAuto() && style.clip().right().isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);

    return CSSRectValue::create({
        valueForAutoOrZoomAdjustedLength(style, style.clip().top()),
        valueForAutoOrZoomAdjustedLength(style, style.clip().right()),
        valueForAutoOrZoomAdjustedLength(style, style.clip().bottom()),
        valueForAutoOrZoomAdjustedLength(style, style.clip().left())
    });
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
    // These values can be percentages or numbers.
    if (length.isPercent())
        return CSSPrimitiveValue::create(length.percent(), CSSUnitType::CSS_PERCENTAGE);
    ASSERT(length.isFixed());
    return CSSPrimitiveValue::create(length.value());
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

    return CSSBorderImageSliceValue::create({ top.releaseNonNull(), right.releaseNonNull(), bottom.releaseNonNull(), left.releaseNonNull() }, image.fill());
}

static Ref<CSSValue> valueForNinePieceImageQuad(const LengthBox& box, const RenderStyle& style)
{
    RefPtr<CSSPrimitiveValue> top;
    RefPtr<CSSPrimitiveValue> right;
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;

    if (box.top().isRelative())
        top = CSSPrimitiveValue::create(box.top().value());
    else
        top = CSSPrimitiveValue::create(box.top(), style);

    if (box.right() == box.top() && box.bottom() == box.top() && box.left() == box.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (box.right().isRelative())
            right = CSSPrimitiveValue::create(box.right().value());
        else
            right = CSSPrimitiveValue::create(box.right(), style);

        if (box.bottom() == box.top() && box.right() == box.left()) {
            bottom = top;
            left = right;
        } else {
            if (box.bottom().isRelative())
                bottom = CSSPrimitiveValue::create(box.bottom().value());
            else
                bottom = CSSPrimitiveValue::create(box.bottom(), style);

            if (box.left() == box.right())
                left = right;
            else {
                if (box.left().isRelative())
                    left = CSSPrimitiveValue::create(box.left().value());
                else
                    left = CSSPrimitiveValue::create(box.left(), style);
            }
        }
    }

    return CSSQuadValue::create({
        top.releaseNonNull(),
        right.releaseNonNull(),
        bottom.releaseNonNull(),
        left.releaseNonNull()
    });
}

static Ref<CSSValue> valueForNinePieceImageRepeat(const NinePieceImage& image)
{
    auto horizontalRepeat = CSSPrimitiveValue::create(valueForRepeatRule(image.horizontalRule()));
    RefPtr<CSSPrimitiveValue> verticalRepeat;
    if (image.horizontalRule() == image.verticalRule())
        verticalRepeat = horizontalRepeat.copyRef();
    else
        verticalRepeat = CSSPrimitiveValue::create(valueForRepeatRule(image.verticalRule()));
    return CSSValuePair::create(WTFMove(horizontalRepeat), verticalRepeat.releaseNonNull());
}

static RefPtr<CSSValue> valueForNinePieceImage(CSSPropertyID propertyID, const NinePieceImage& image, const RenderStyle& style)
{
    RefPtr imageSource = image.image();
    if (!imageSource)
        return CSSPrimitiveValue::create(CSSValueNone);

    // -webkit-border-image has a legacy behavior that makes fixed border slices also set the border widths.
    auto& borderSlices = image.borderSlices();
    bool overridesBorderWidths = propertyID == CSSPropertyWebkitBorderImage && (borderSlices.top().isFixed() || borderSlices.right().isFixed() || borderSlices.bottom().isFixed() || borderSlices.left().isFixed());
    if (overridesBorderWidths != image.overridesBorderWidths())
        return nullptr;

    return createBorderImageValue({
        .source = imageSource->computedStyleValue(style),
        .slice = valueForNinePieceImageSlice(image),
        .width = valueForNinePieceImageQuad(borderSlices, style),
        .outset = valueForNinePieceImageQuad(image.outset(), style),
        .repeat = valueForNinePieceImageRepeat(image),
    });
}

static RefPtr<CSSValue> valueForBorderImageWidth(const RenderStyle& style)
{
    if (style.borderImage().overridesBorderWidths())
        return nullptr;
    return valueForNinePieceImageQuad(style.borderImage().borderSlices(), style);
}

static Ref<CSSValue> valueForFontSizeAdjust(const RenderStyle& style)
{
    auto fontSizeAdjust = style.fontSizeAdjust();
    if (fontSizeAdjust.isNone())
        return CSSPrimitiveValue::create(CSSValueNone);

    auto metric = fontSizeAdjust.metric;
    auto value = fontSizeAdjust.shouldResolveFromFont() ? fontSizeAdjust.resolve(style.computedFontSize(), style.metricsOfPrimaryFont()) : fontSizeAdjust.value.asOptional();

    if (!value)
        return CSSPrimitiveValue::create(CSSValueNone);

    if (metric == FontSizeAdjust::Metric::ExHeight)
        return CSSPrimitiveValue::create(*value);

    return CSSValuePair::create(valueForConvertibleType(metric), CSSPrimitiveValue::create(*value));
}

static Ref<CSSPrimitiveValue> valueForTextSpacingTrim(const RenderStyle& style)
{
    // FIXME: add support for remaining values once spec is stable and we are parsing them.
    auto textSpacingTrim = style.textSpacingTrim();
    switch (textSpacingTrim.type()) {
    case TextSpacingTrim::TrimType::SpaceAll:
        return CSSPrimitiveValue::create(CSSValueSpaceAll);
    case TextSpacingTrim::TrimType::Auto:
        return CSSPrimitiveValue::create(CSSValueAuto);
    case TextSpacingTrim::TrimType::TrimAll:
        return CSSPrimitiveValue::create(CSSValueTrimAll);
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return CSSPrimitiveValue::create(CSSValueSpaceAll);
}

static Ref<CSSValue> valueForTextAutospace(const RenderStyle& style)
{
    // FIXME: add support for remaining values once spec is stable and we are parsing them.
    auto textAutospace = style.textAutospace();
    if (textAutospace.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    if (textAutospace.isNoAutospace())
        return CSSPrimitiveValue::create(CSSValueNoAutospace);
    if (textAutospace.isNormal())
        return CSSPrimitiveValue::create(CSSValueNormal);

    CSSValueListBuilder list;
    if (textAutospace.hasIdeographAlpha())
        list.append(CSSPrimitiveValue::create(CSSValueIdeographAlpha));
    if (textAutospace.hasIdeographNumeric())
        list.append(CSSPrimitiveValue::create(CSSValueIdeographNumeric));

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static inline Ref<CSSValue> valueForWebkitBoxReflect(const RenderStyle& style, const StyleReflection* reflection)
{
    if (!reflection)
        return CSSPrimitiveValue::create(CSSValueNone);

    // FIXME: Consider omitting 0px when the mask is null.
    RefPtr<CSSPrimitiveValue> offset;
    if (reflection->offset().isPercentOrCalculated())
        offset = CSSPrimitiveValue::create(reflection->offset().percent(), CSSUnitType::CSS_PERCENTAGE);
    else
        offset = valueForZoomAdjustedFloatLength(style, reflection->offset().value());

    return CSSReflectValue::create(
        toCSSValueID(reflection->direction()),
        offset.releaseNonNull(),
        valueForNinePieceImage(CSSPropertyWebkitBoxReflect, reflection->mask(), style)
    );
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

static RefPtr<CSSValue> valueForInset(const RenderStyle& style, CSSPropertyID propertyID, RenderObject* renderer)
{
    auto offset = getOffsetComputedLength(style, propertyID);

    // If the element is not displayed; return the "computed value".
    CheckedPtr box = dynamicDowncast<RenderBox>(renderer);
    if (!box)
        return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, offset);

    auto* containingBlock = box->containingBlock();

    // Resolve a "computed value" percentage if the element is positioned.
    if (containingBlock && offset.isPercentOrCalculated() && box->isPositioned()) {
        bool isVerticalProperty;
        if (propertyID == CSSPropertyTop || propertyID == CSSPropertyBottom)
            isVerticalProperty = true;
        else {
            ASSERT(propertyID == CSSPropertyLeft || propertyID == CSSPropertyRight);
            isVerticalProperty = false;
        }
        LayoutUnit containingBlockSize;
        if (box->isStickilyPositioned()) {
            auto& enclosingClippingBox = box->enclosingClippingBoxForStickyPosition().first;
            if (isVerticalProperty == enclosingClippingBox.isHorizontalWritingMode())
                containingBlockSize = enclosingClippingBox.contentBoxLogicalHeight();
            else
                containingBlockSize = enclosingClippingBox.contentBoxLogicalWidth();
        } else {
            if (isVerticalProperty == containingBlock->isHorizontalWritingMode()) {
                containingBlockSize = box->isOutOfFlowPositioned()
                    ? box->containingBlockLogicalHeightForPositioned(*containingBlock, false)
                    : box->containingBlockLogicalHeightForContent(AvailableLogicalHeightType::ExcludeMarginBorderPadding);
            } else {
                containingBlockSize = box->isOutOfFlowPositioned()
                    ? box->containingBlockLogicalWidthForPositioned(*containingBlock, false)
                    : box->containingBlockLogicalWidthForContent();
            }
        }
        return valueForZoomAdjustedFloatLength(style, floatValueForLength(offset, containingBlockSize));
    }

    // Return a "computed value" length.
    if (!offset.isAuto())
        return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, offset);

    // The property won't be overconstrained if its computed value is "auto", so the "used value" can be returned.
    if (box->isRelativelyPositioned())
        return valueForZoomAdjustedFloatLength(style, getOffsetUsedStyleRelative(*box, propertyID));

    if (containingBlock && box->isOutOfFlowPositioned())
        return valueForZoomAdjustedFloatLength(style, getOffsetUsedStyleOutOfFlowPositioned(*containingBlock, *box, propertyID));

    return CSSPrimitiveValue::create(CSSValueAuto);
}

static Ref<CSSValue> valueForTextEdge(CSSPropertyID property, const TextEdge& textEdge)
{
    if (property == CSSPropertyTextBoxEdge && textEdge.over == TextEdgeType::Auto && textEdge.under == TextEdgeType::Auto)
        return valueForConvertibleType(textEdge.over);

    if (property == CSSPropertyLineFitEdge && textEdge.over == TextEdgeType::Leading && textEdge.under == TextEdgeType::Leading)
        return valueForConvertibleType(textEdge.over);

    // https://www.w3.org/TR/css-inline-3/#text-edges
    // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
    auto shouldSerializeUnderEdge = [&]() {
        if (textEdge.over == TextEdgeType::CapHeight || textEdge.over == TextEdgeType::ExHeight)
            return textEdge.under != TextEdgeType::Text;
        return textEdge.over != textEdge.under;
    }();

    if (!shouldSerializeUnderEdge)
        return valueForConvertibleType(textEdge.over);

    return CSSValuePair::create(valueForConvertibleType(textEdge.over),
        valueForConvertibleType(textEdge.under));
}

static Ref<CSSValue> valueForQuotes(const RenderStyle& style)
{
    auto quotes = style.quotes();
    if (!quotes)
        return CSSPrimitiveValue::create(CSSValueAuto);
    unsigned size = quotes->size();
    if (!size)
        return CSSPrimitiveValue::create(CSSValueNone);
    CSSValueListBuilder list;
    for (unsigned i = 0; i < size; ++i) {
        list.append(CSSPrimitiveValue::create(quotes->openQuote(i)));
        list.append(CSSPrimitiveValue::create(quotes->closeQuote(i)));
    }
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>> valueForBorderRadiusCornerValues(const RenderStyle& style, const LengthSize& radius)
{
    auto x = valueForPercentageOrZoomAdjustedLength(style, radius.width);
    auto y = radius.width == radius.height ? x.copyRef() : valueForPercentageOrZoomAdjustedLength(style, radius.height);
    return { WTFMove(x), WTFMove(y) };
}

static Ref<CSSValue> valueForBorderRadiusCornerValue(const RenderStyle& style, const LengthSize& radius)
{
    auto [x, y] = valueForBorderRadiusCornerValues(style, radius);
    return CSSValuePair::create(WTFMove(x), WTFMove(y));
}

static bool itemsEqual(const CSSValueListBuilder& a, const CSSValueListBuilder& b)
{
    auto size = a.size();
    if (size != b.size())
        return false;
    for (unsigned i = 0; i < size; ++i) {
        if (!a[i]->equals(b[i]))
            return false;
    }
    return true;
}

static RefPtr<CSSValue> valueForBorderRadiusShorthand(const RenderStyle& style, CSSPropertyID propertyID)
{
    bool showHorizontalBottomLeft = style.borderTopRightRadius().width != style.borderBottomLeftRadius().width;
    bool showHorizontalBottomRight = showHorizontalBottomLeft || (style.borderBottomRightRadius().width != style.borderTopLeftRadius().width);
    bool showHorizontalTopRight = showHorizontalBottomRight || (style.borderTopRightRadius().width != style.borderTopLeftRadius().width);

    bool showVerticalBottomLeft = style.borderTopRightRadius().height != style.borderBottomLeftRadius().height;
    bool showVerticalBottomRight = showVerticalBottomLeft || (style.borderBottomRightRadius().height != style.borderTopLeftRadius().height);
    bool showVerticalTopRight = showVerticalBottomRight || (style.borderTopRightRadius().height != style.borderTopLeftRadius().height);

    auto [topLeftRadiusX, topLeftRadiusY] = valueForBorderRadiusCornerValues(style, style.borderTopLeftRadius());
    auto [topRightRadiusX, topRightRadiusY] = valueForBorderRadiusCornerValues(style, style.borderTopRightRadius());
    auto [bottomRightRadiusX, bottomRightRadiusY] = valueForBorderRadiusCornerValues(style, style.borderBottomRightRadius());
    auto [bottomLeftRadiusX, bottomLeftRadiusY] = valueForBorderRadiusCornerValues(style, style.borderBottomLeftRadius());

    CSSValueListBuilder horizontalRadii;
    horizontalRadii.append(WTFMove(topLeftRadiusX));
    if (showHorizontalTopRight)
        horizontalRadii.append(WTFMove(topRightRadiusX));
    if (showHorizontalBottomRight)
        horizontalRadii.append(WTFMove(bottomRightRadiusX));
    if (showHorizontalBottomLeft)
        horizontalRadii.append(WTFMove(bottomLeftRadiusX));

    CSSValueListBuilder verticalRadii;
    verticalRadii.append(WTFMove(topLeftRadiusY));
    if (showVerticalTopRight)
        verticalRadii.append(WTFMove(topRightRadiusY));
    if (showVerticalBottomRight)
        verticalRadii.append(WTFMove(bottomRightRadiusY));
    if (showVerticalBottomLeft)
        verticalRadii.append(WTFMove(bottomLeftRadiusY));

    bool includeVertical = false;
    if (!itemsEqual(horizontalRadii, verticalRadii))
        includeVertical = true;
    else if (propertyID == CSSPropertyWebkitBorderRadius && showHorizontalTopRight && !showHorizontalBottomRight)
        horizontalRadii.append(WTFMove(bottomRightRadiusX));

    if (!includeVertical)
        return CSSValueList::createSlashSeparated(CSSValueList::createSpaceSeparated(WTFMove(horizontalRadii)));
    return CSSValueList::createSlashSeparated(CSSValueList::createSpaceSeparated(WTFMove(horizontalRadii)),
        CSSValueList::createSpaceSeparated(WTFMove(verticalRadii)));
}

static Ref<CSSValue> valueForTransform(const RenderStyle& style, RenderElement* renderer, ComputedStyleExtractor::PropertyValueType valueType)
{
    if (!style.hasTransform())
        return CSSPrimitiveValue::create(CSSValueNone);

    if (renderer) {
        TransformationMatrix transform;
        style.applyTransform(transform, TransformOperationData(renderer->transformReferenceBoxRect(style), renderer), { });
        return CSSTransformListValue::create(ComputedStyleExtractor::valueForTransformationMatrix(style, transform));
    }

    // https://w3c.github.io/csswg-drafts/css-transforms-1/#serialization-of-the-computed-value
    // If we don't have a renderer, then the value should be "none" if we're asking for the
    // resolved value (such as when calling getComputedStyle()).
    if (valueType == ComputedStyleExtractor::PropertyValueType::Resolved)
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& operation : style.transform()) {
        if (auto functionValue = ComputedStyleExtractor::valueForTransformOperation(style, operation))
            list.append(functionValue.releaseNonNull());
    }
    if (!list.isEmpty())
        return CSSTransformListValue::create(WTFMove(list));

    return CSSPrimitiveValue::create(CSSValueNone);
}

// https://drafts.csswg.org/css-transforms-2/#propdef-translate
// Computed value: the keyword none or a pair of computed <length-percentage> values and an absolute length
static Ref<CSSValue> valueForTranslate(const RenderStyle& style, RenderElement* renderer)
{
    auto* translate = style.translate();
    if (!translate || is<RenderInline>(renderer))
        return CSSPrimitiveValue::create(CSSValueNone);

    auto includeLength = [](const Length& length) {
        return !length.isZero() || length.isPercent();
    };

    auto value = [&](const Length& length) {
        return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, length);
    };

    if (includeLength(translate->z()))
        return CSSValueList::createSpaceSeparated(value(translate->x()), value(translate->y()), value(translate->z()));
    if (includeLength(translate->y()))
        return CSSValueList::createSpaceSeparated(value(translate->x()), value(translate->y()));
    if (!translate->x().isUndefined() && !translate->x().isEmptyValue())
        return CSSValueList::createSpaceSeparated(value(translate->x()));

    return CSSPrimitiveValue::create(CSSValueNone);
}

static Ref<CSSValue> valueForScale(const RenderStyle& style, RenderElement* renderer)
{
    auto* scale = style.scale();
    if (!scale || is<RenderInline>(renderer))
        return CSSPrimitiveValue::create(CSSValueNone);

    auto value = [](double number) {
        return CSSPrimitiveValue::create(number);
    };

    if (scale->z() != 1)
        return CSSValueList::createSpaceSeparated(value(scale->x()), value(scale->y()), value(scale->z()));
    if (scale->x() != scale->y())
        return CSSValueList::createSpaceSeparated(value(scale->x()), value(scale->y()));
    return CSSValueList::createSpaceSeparated(value(scale->x()));
}

static Ref<CSSValue> valueForRotate(const RenderStyle& style, RenderElement* renderer)
{
    auto* rotate = style.rotate();
    if (!rotate || is<RenderInline>(renderer))
        return CSSPrimitiveValue::create(CSSValueNone);

    auto angle = CSSPrimitiveValue::create(rotate->angle(), CSSUnitType::CSS_DEG);
    if (!rotate->is3DOperation() || (!rotate->x() && !rotate->y() && rotate->z()))
        return angle;
    if (rotate->x() && !rotate->y() && !rotate->z())
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueX), WTFMove(angle));
    if (!rotate->x() && rotate->y() && !rotate->z())
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueY), WTFMove(angle));
    return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(rotate->x()),
        CSSPrimitiveValue::create(rotate->y()), CSSPrimitiveValue::create(rotate->z()), WTFMove(angle));
}

static Ref<CSSValue> valueForTransformOrigin(const RenderStyle& style, RenderElement* renderer)
{
    CSSValueListBuilder list;
    if (renderer) {
        auto box = renderer->transformReferenceBoxRect(style);
        list.append(valueForZoomAdjustedFloatLength(style, minimumValueForLength(style.transformOriginX(), box.width())));
        list.append(valueForZoomAdjustedFloatLength(style, minimumValueForLength(style.transformOriginY(), box.height())));
        if (style.transformOriginZ())
            list.append(valueForZoomAdjustedFloatLength(style, style.transformOriginZ()));
    } else {
        list.append(ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, style.transformOriginX()));
        list.append(ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, style.transformOriginY()));
        if (style.transformOriginZ())
            list.append(valueForZoomAdjustedFloatLength(style, style.transformOriginZ()));
    }
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSPrimitiveValue> valueForScopedName(const Style::ScopedName& scopedName)
{
    if (scopedName.isIdentifier)
        return CSSPrimitiveValue::createCustomIdent(scopedName.name);
    return CSSPrimitiveValue::create(scopedName.name);
}

static RefPtr<CSSValue> valueForContainerName(const RenderStyle& style)
{
    if (style.containerNames().isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);
    CSSValueListBuilder list;
    for (auto& name : style.containerNames())
        list.append(valueForScopedName(name));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static RefPtr<CSSValue> valueForViewTransitionClass(const RenderStyle& style)
{
    auto classList = style.viewTransitionClasses();
    if (classList.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& name : classList)
        list.append(valueForScopedName(name));

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static RefPtr<CSSValue> valueForViewTransitionName(const RenderStyle& style)
{
    auto viewTransitionName = style.viewTransitionName();
    if (viewTransitionName.isNone())
        return CSSPrimitiveValue::create(CSSValueNone);
    if (viewTransitionName.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return CSSPrimitiveValue::createCustomIdent(viewTransitionName.customIdent());
}

static RefPtr<CSSValue> valueForTextIndent(const RenderStyle& style)
{
    auto textIndent = ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, style.textIndent());
    if (style.textIndentLine() == TextIndentLine::EachLine || style.textIndentType() == TextIndentType::Hanging) {
        CSSValueListBuilder list;
        list.append(WTFMove(textIndent));
        if (style.textIndentType() == TextIndentType::Hanging)
            list.append(CSSPrimitiveValue::create(CSSValueHanging));
        if (style.textIndentLine() == TextIndentLine::EachLine)
            list.append(CSSPrimitiveValue::create(CSSValueEachLine));
        return CSSValueList::createSpaceSeparated(WTFMove(list));
    }
    return textIndent;
}

#if ENABLE(TEXT_AUTOSIZING)
static RefPtr<CSSValue> valueForWebkitTextSizeAdjust(const RenderStyle& style)
{
    if (style.textSizeAdjust().isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    if (style.textSizeAdjust().isNone())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSPrimitiveValue::create(style.textSizeAdjust().percentage(), CSSUnitType::CSS_PERCENTAGE);
}
#endif

static RefPtr<CSSValue> valueForVerticalAlign(const RenderStyle& style)
{
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
}

static Ref<CSSValue> valueForBoxShadow(const RenderStyle& style, const ShadowData* shadow)
{
    if (!shadow)
        return CSSPrimitiveValue::create(CSSValueNone);

    CSS::BoxShadowProperty::List list;

    for (const auto* currentShadowData = shadow; currentShadowData; currentShadowData = currentShadowData->next())
        list.value.append(Style::toCSS(currentShadowData->asBoxShadow(), style));

    list.value.reverse();

    return CSSBoxShadowPropertyValue::create(CSS::BoxShadowProperty { WTFMove(list) });
}

static Ref<CSSValue> valueForTextShadow(const RenderStyle& style, const ShadowData* shadow)
{
    if (!shadow)
        return CSSPrimitiveValue::create(CSSValueNone);

    CSS::TextShadowProperty::List list;

    for (const auto* currentShadowData = shadow; currentShadowData; currentShadowData = currentShadowData->next())
        list.value.append(Style::toCSS(currentShadowData->asTextShadow(), style));

    list.value.reverse();

    return CSSTextShadowPropertyValue::create(CSS::TextShadowProperty { WTFMove(list) });
}

static Ref<CSSValue> valueForPositionTryFallbacks(const Vector<Style::PositionTryFallback>& fallbacks)
{
    if (fallbacks.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& fallback : fallbacks) {
        if (fallback.positionAreaProperties) {
            auto areaValue = fallback.positionAreaProperties->getPropertyCSSValue(CSSPropertyPositionArea);
            if (areaValue)
                list.append(*areaValue);
            continue;
        }

        CSSValueListBuilder singleFallbackList;
        if (fallback.positionTryRuleName)
            singleFallbackList.append(valueForScopedName(*fallback.positionTryRuleName));
        for (auto& tactic : fallback.tactics)
            singleFallbackList.append(valueForConvertibleType(tactic));
        list.append(CSSValueList::createSpaceSeparated(singleFallbackList));
    }

    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static Ref<CSSValue> specifiedValueForGridTrackBreadth(const GridLength& trackBreadth, const RenderStyle& style)
{
    if (!trackBreadth.isLength())
        return CSSPrimitiveValue::create(trackBreadth.flex(), CSSUnitType::CSS_FR);

    auto& trackBreadthLength = trackBreadth.length();
    if (trackBreadthLength.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, trackBreadthLength);
}

static Ref<CSSValue> specifiedValueForGridTrackSize(const GridTrackSize& trackSize, const RenderStyle& style)
{
    switch (trackSize.type()) {
    case LengthTrackSizing:
        return specifiedValueForGridTrackBreadth(trackSize.minTrackBreadth(), style);
    case FitContentTrackSizing:
        return CSSFunctionValue::create(CSSValueFitContent, ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, trackSize.fitContentTrackBreadth().length()));
    default:
        ASSERT(trackSize.type() == MinMaxTrackSizing);
        if (trackSize.minTrackBreadth().isAuto() && trackSize.maxTrackBreadth().isFlex())
            return CSSPrimitiveValue::create(trackSize.maxTrackBreadth().flex(), CSSUnitType::CSS_FR);
        return CSSFunctionValue::create(CSSValueMinmax, specifiedValueForGridTrackBreadth(trackSize.minTrackBreadth(), style),
            specifiedValueForGridTrackBreadth(trackSize.maxTrackBreadth(), style));
    }
}

static void addValuesForNamedGridLinesAtIndex(OrderedNamedLinesCollector& collector, unsigned i, CSSValueListBuilder& list, bool renderEmpty = false)
{
    if (collector.isEmpty() && !renderEmpty)
        return;

    Vector<String> lineNames;
    collector.collectLineNamesForIndex(lineNames, i);
    if (!lineNames.isEmpty() || renderEmpty)
        list.append(CSSGridLineNamesValue::create(lineNames));
}

// Specs mention that getComputedStyle() should return the used value of the property instead of the computed
// one for grid-template-{rows|columns} but not for the grid-auto-{rows|columns} as things like
// grid-auto-columns: 2fr; cannot be resolved to a value in pixels as the '2fr' means very different things
// depending on the size of the explicit grid or the number of implicit tracks added to the grid. See
// http://lists.w3.org/Archives/Public/www-style/2013Nov/0014.html

static Ref<CSSValueList> valueForGridTrackSizeList(GridTrackSizingDirection direction, const RenderStyle& style)
{
    auto& autoTrackSizes = direction == GridTrackSizingDirection::ForColumns ? style.gridAutoColumns() : style.gridAutoRows();

    CSSValueListBuilder list;
    for (auto& trackSize : autoTrackSizes)
        list.append(specifiedValueForGridTrackSize(trackSize, style));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

template <typename T, typename F>
void populateGridTrackList(CSSValueListBuilder& list, OrderedNamedLinesCollector& collector, const Vector<T>& tracks, F getTrackSize, int offset = 0)
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

static void populateSubgridLineNameList(CSSValueListBuilder& list, OrderedNamedLinesCollector& collector)
{
    for (int i = 0; i < collector.namedGridLineCount(); i++)
        addValuesForNamedGridLinesAtIndex(collector, i, list, true);
}

static Ref<CSSValue> valueForGridTrackList(GridTrackSizingDirection direction, RenderObject* renderer, const RenderStyle& style)
{
    bool isRowAxis = direction == GridTrackSizingDirection::ForColumns;
    auto* renderGrid = dynamicDowncast<RenderGrid>(renderer);
    bool isSubgrid = isRowAxis ? style.gridSubgridColumns() : style.gridSubgridRows();
    auto& trackSizes = isRowAxis ? style.gridColumnTrackSizes() : style.gridRowTrackSizes();
    auto& autoRepeatTrackSizes = isRowAxis ? style.gridAutoRepeatColumns() : style.gridAutoRepeatRows();

    if ((direction == GridTrackSizingDirection::ForRows && style.gridMasonryRows())
        || (direction == GridTrackSizingDirection::ForColumns && style.gridMasonryColumns()))
        return CSSPrimitiveValue::create(CSSValueMasonry);

    // Handle the 'none' case.
    bool trackListIsEmpty = trackSizes.isEmpty() && autoRepeatTrackSizes.isEmpty();
    if (renderGrid && trackListIsEmpty) {
        // For grids we should consider every listed track, whether implicitly or explicitly
        // created. Empty grids have a sole grid line per axis.
        auto& positions = isRowAxis ? renderGrid->columnPositions() : renderGrid->rowPositions();
        trackListIsEmpty = positions.size() == 1;
    }

    if (trackListIsEmpty && !isSubgrid)
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;

    // If the element is a grid container, the resolved value is the used value,
    // specifying track sizes in pixels and expanding the repeat() notation.
    // If subgrid was specified, but the element isn't a subgrid (due to not having
    // an appropriate grid parent), then we fall back to using the specified value.
    if (renderGrid && (!isSubgrid || renderGrid->isSubgrid(direction))) {
        if (isSubgrid) {
            list.append(CSSPrimitiveValue::create(CSSValueSubgrid));

            OrderedNamedLinesCollectorInSubgridLayout collector(style, isRowAxis, renderGrid->numTracks(direction));
            populateSubgridLineNameList(list, collector);
            return CSSValueList::createSpaceSeparated(WTFMove(list));
        }
        OrderedNamedLinesCollectorInGridLayout collector(style, isRowAxis, renderGrid->autoRepeatCountForDirection(direction), autoRepeatTrackSizes.size());
        // Named grid line indices are relative to the explicit grid, but we are including all tracks.
        // So we need to subtract the number of leading implicit tracks in order to get the proper line index.
        int offset = -renderGrid->explicitGridStartForDirection(direction);
        populateGridTrackList(list, collector, renderGrid->trackSizesForComputedStyle(direction), [&](const LayoutUnit& v) {
            return valueForZoomAdjustedFloatLength(style, v);
        }, offset);
        return CSSValueList::createSpaceSeparated(WTFMove(list));
    }

    // Otherwise, the resolved value is the computed value, preserving repeat().
    auto& computedTracks = (isRowAxis ? style.gridColumnList() : style.gridRowList()).list;

    auto repeatVisitor = [&](CSSValueListBuilder& list, const RepeatEntry& entry) {
        if (std::holds_alternative<Vector<String>>(entry)) {
            const auto& names = std::get<Vector<String>>(entry);
            if (names.isEmpty() && !isSubgrid)
                return;
            list.append(CSSGridLineNamesValue::create(names));
        } else
            list.append(specifiedValueForGridTrackSize(std::get<GridTrackSize>(entry), style));
    };

    auto trackEntryVisitor = WTF::makeVisitor(
        [&](const GridTrackSize& size) {
            list.append(specifiedValueForGridTrackSize(size, style));
        },
        [&](const Vector<String>& names) {
            // Subgrids don't have track sizes specified, so empty line names sets
            // need to be serialized, as they are meaningful placeholders.
            if (names.isEmpty() && !isSubgrid)
                return;
            list.append(CSSGridLineNamesValue::create(names));
        },
        [&](const GridTrackEntryRepeat& repeat) {
            CSSValueListBuilder repeatedValues;
            for (auto& entry : repeat.list)
                repeatVisitor(repeatedValues, entry);
            list.append(CSSGridIntegerRepeatValue::create(CSSPrimitiveValue::createInteger(repeat.repeats), WTFMove(repeatedValues)));
        },
        [&](const GridTrackEntryAutoRepeat& repeat) {
            CSSValueListBuilder repeatedValues;
            for (auto& entry : repeat.list)
                repeatVisitor(repeatedValues, entry);
            list.append(CSSGridAutoRepeatValue::create(repeat.type == AutoRepeatType::Fill ? CSSValueAutoFill : CSSValueAutoFit, WTFMove(repeatedValues)));
        },
        [&](const GridTrackEntrySubgrid&) {
            list.append(CSSPrimitiveValue::create(CSSValueSubgrid));
        },
        [&](const GridTrackEntryMasonry&) {
            list.append(CSSPrimitiveValue::create(CSSValueMasonry));
        }
    );

    for (auto& entry : computedTracks)
        WTF::visit(trackEntryVisitor, entry);

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForGridPosition(const GridPosition& position)
{
    if (position.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);

    if (position.isNamedGridArea())
        return CSSPrimitiveValue::createCustomIdent(position.namedGridLine());

    bool hasNamedGridLine = !position.namedGridLine().isNull();
    CSSValueListBuilder list;
    if (position.isSpan()) {
        list.append(CSSPrimitiveValue::create(CSSValueSpan));
        if (!hasNamedGridLine || position.spanPosition() != 1)
            list.append(CSSPrimitiveValue::createInteger(position.spanPosition()));
    } else
        list.append(CSSPrimitiveValue::createInteger(position.integerPosition()));

    if (hasNamedGridLine)
        list.append(CSSPrimitiveValue::createCustomIdent(position.namedGridLine()));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForTransitionProperty(const Animation& animation)
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
    if (type.strictness == ScrollSnapStrictness::None)
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueNone));
    if (type.strictness == ScrollSnapStrictness::Proximity)
        return CSSValueList::createSpaceSeparated(valueForConvertibleType(type.axis));
    return CSSValueList::createSpaceSeparated(valueForConvertibleType(type.axis),
        valueForConvertibleType(type.strictness));
}

static Ref<CSSValue> valueForScrollSnapAlignment(const ScrollSnapAlign& alignment)
{
    return CSSValuePair::create(
        valueForConvertibleType(alignment.blockAlign),
        valueForConvertibleType(alignment.inlineAlign)
    );
}

static Ref<CSSValue> valueForScrollbarColor(const RenderStyle& style, CSSValuePool& pool)
{
    if (!style.scrollbarColor())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return CSSValuePair::createNoncoalescing(
        valueForColor(style, pool, style.scrollbarColor().value().thumbColor),
        valueForColor(style, pool, style.scrollbarColor().value().trackColor)
    );
}

static Ref<CSSValue> valueForScrollbarGutter(const ScrollbarGutter& gutter)
{
    if (!gutter.bothEdges)
        return CSSPrimitiveValue::create(gutter.isAuto ? CSSValueAuto : CSSValueStable);
    return CSSValuePair::create(
        CSSPrimitiveValue::create(CSSValueStable),
        CSSPrimitiveValue::create(CSSValueBothEdges)
    );
}

static Ref<CSSValue> valueForWillChange(const RenderStyle& style)
{
    auto willChangeData = style.willChange();
    if (!willChangeData || !willChangeData->numFeatures())
        return CSSPrimitiveValue::create(CSSValueAuto);

    CSSValueListBuilder list;
    for (size_t i = 0; i < willChangeData->numFeatures(); ++i) {
        auto feature = willChangeData->featureAt(i);
        switch (feature.first) {
        case WillChangeData::Feature::ScrollPosition:
            list.append(CSSPrimitiveValue::create(CSSValueScrollPosition));
            break;
        case WillChangeData::Feature::Contents:
            list.append(CSSPrimitiveValue::create(CSSValueContents));
            break;
        case WillChangeData::Feature::Property:
            list.append(CSSPrimitiveValue::create(feature.second));
            break;
        case WillChangeData::Feature::Invalid:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForFontVariantLigaturesPropertyValue(FontVariantLigatures common, FontVariantLigatures discretionary, FontVariantLigatures historical, FontVariantLigatures contextualAlternates)
{
    if (common == FontVariantLigatures::No && discretionary == FontVariantLigatures::No && historical == FontVariantLigatures::No && contextualAlternates == FontVariantLigatures::No)
        return CSSPrimitiveValue::create(CSSValueNone);
    if (common == FontVariantLigatures::Normal && discretionary == FontVariantLigatures::Normal && historical == FontVariantLigatures::Normal && contextualAlternates == FontVariantLigatures::Normal)
        return CSSPrimitiveValue::create(CSSValueNormal);

    auto appendLigaturesValue = [](auto& list, auto value, auto yesValue, auto noValue) {
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
    };

    CSSValueListBuilder valueList;
    appendLigaturesValue(valueList, common, CSSValueCommonLigatures, CSSValueNoCommonLigatures);
    appendLigaturesValue(valueList, discretionary, CSSValueDiscretionaryLigatures, CSSValueNoDiscretionaryLigatures);
    appendLigaturesValue(valueList, historical, CSSValueHistoricalLigatures, CSSValueNoHistoricalLigatures);
    appendLigaturesValue(valueList, contextualAlternates, CSSValueContextual, CSSValueNoContextual);
    return CSSValueList::createSpaceSeparated(WTFMove(valueList));
}

static Ref<CSSValue> valueForFontVariantNumericPropertyValue(FontVariantNumericFigure figure, FontVariantNumericSpacing spacing, FontVariantNumericFraction fraction, FontVariantNumericOrdinal ordinal, FontVariantNumericSlashedZero slashedZero)
{
    if (figure == FontVariantNumericFigure::Normal && spacing == FontVariantNumericSpacing::Normal && fraction == FontVariantNumericFraction::Normal && ordinal == FontVariantNumericOrdinal::Normal && slashedZero == FontVariantNumericSlashedZero::Normal)
        return CSSPrimitiveValue::create(CSSValueNormal);

    CSSValueListBuilder valueList;
    switch (figure) {
    case FontVariantNumericFigure::Normal:
        break;
    case FontVariantNumericFigure::LiningNumbers:
        valueList.append(CSSPrimitiveValue::create(CSSValueLiningNums));
        break;
    case FontVariantNumericFigure::OldStyleNumbers:
        valueList.append(CSSPrimitiveValue::create(CSSValueOldstyleNums));
        break;
    }

    switch (spacing) {
    case FontVariantNumericSpacing::Normal:
        break;
    case FontVariantNumericSpacing::ProportionalNumbers:
        valueList.append(CSSPrimitiveValue::create(CSSValueProportionalNums));
        break;
    case FontVariantNumericSpacing::TabularNumbers:
        valueList.append(CSSPrimitiveValue::create(CSSValueTabularNums));
        break;
    }

    switch (fraction) {
    case FontVariantNumericFraction::Normal:
        break;
    case FontVariantNumericFraction::DiagonalFractions:
        valueList.append(CSSPrimitiveValue::create(CSSValueDiagonalFractions));
        break;
    case FontVariantNumericFraction::StackedFractions:
        valueList.append(CSSPrimitiveValue::create(CSSValueStackedFractions));
        break;
    }

    if (ordinal == FontVariantNumericOrdinal::Yes)
        valueList.append(CSSPrimitiveValue::create(CSSValueOrdinal));
    if (slashedZero == FontVariantNumericSlashedZero::Yes)
        valueList.append(CSSPrimitiveValue::create(CSSValueSlashedZero));

    return CSSValueList::createSpaceSeparated(WTFMove(valueList));
}

static Ref<CSSValue> valueForFontVariantAlternatesPropertyValue(const FontVariantAlternates& alternates)
{
    if (alternates.isNormal())
        return CSSPrimitiveValue::create(CSSValueNormal);

    CSSValueListBuilder valueList;

    if (!alternates.values().stylistic.isNull())
        valueList.append(CSSFunctionValue::create(CSSValueStylistic, CSSPrimitiveValue::createCustomIdent(alternates.values().stylistic)));

    if (alternates.values().historicalForms)
        valueList.append(CSSPrimitiveValue::create(CSSValueHistoricalForms));

    if (!alternates.values().styleset.isEmpty()) {
        CSSValueListBuilder stylesetArguments;
        for (auto& argument : alternates.values().styleset)
            stylesetArguments.append(CSSPrimitiveValue::createCustomIdent(argument));
        valueList.append(CSSFunctionValue::create(CSSValueStyleset, WTFMove(stylesetArguments)));
    }

    if (!alternates.values().characterVariant.isEmpty()) {
        CSSValueListBuilder characterVariantArguments;
        for (auto& argument : alternates.values().characterVariant)
            characterVariantArguments.append(CSSPrimitiveValue::createCustomIdent(argument));
        valueList.append(CSSFunctionValue::create(CSSValueCharacterVariant, WTFMove(characterVariantArguments)));
    }

    if (!alternates.values().swash.isNull())
        valueList.append(CSSFunctionValue::create(CSSValueSwash, CSSPrimitiveValue::createCustomIdent(alternates.values().swash)));

    if (!alternates.values().ornaments.isNull())
        valueList.append(CSSFunctionValue::create(CSSValueOrnaments, CSSPrimitiveValue::createCustomIdent(alternates.values().ornaments)));

    if (!alternates.values().annotation.isNull())
        valueList.append(CSSFunctionValue::create(CSSValueAnnotation, CSSPrimitiveValue::createCustomIdent(alternates.values().annotation)));

    if (valueList.size() == 1)
        return WTFMove(valueList[0]);

    return CSSValueList::createSpaceSeparated(WTFMove(valueList));
}

static Ref<CSSValue> valueForFontVariantEastAsianPropertyValue(FontVariantEastAsianVariant variant, FontVariantEastAsianWidth width, FontVariantEastAsianRuby ruby)
{
    if (variant == FontVariantEastAsianVariant::Normal && width == FontVariantEastAsianWidth::Normal && ruby == FontVariantEastAsianRuby::Normal)
        return CSSPrimitiveValue::create(CSSValueNormal);

    CSSValueListBuilder valueList;
    switch (variant) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        valueList.append(CSSPrimitiveValue::create(CSSValueJis78));
        break;
    case FontVariantEastAsianVariant::Jis83:
        valueList.append(CSSPrimitiveValue::create(CSSValueJis83));
        break;
    case FontVariantEastAsianVariant::Jis90:
        valueList.append(CSSPrimitiveValue::create(CSSValueJis90));
        break;
    case FontVariantEastAsianVariant::Jis04:
        valueList.append(CSSPrimitiveValue::create(CSSValueJis04));
        break;
    case FontVariantEastAsianVariant::Simplified:
        valueList.append(CSSPrimitiveValue::create(CSSValueSimplified));
        break;
    case FontVariantEastAsianVariant::Traditional:
        valueList.append(CSSPrimitiveValue::create(CSSValueTraditional));
        break;
    }

    switch (width) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        valueList.append(CSSPrimitiveValue::create(CSSValueFullWidth));
        break;
    case FontVariantEastAsianWidth::Proportional:
        valueList.append(CSSPrimitiveValue::create(CSSValueProportionalWidth));
        break;
    }

    if (ruby == FontVariantEastAsianRuby::Yes)
        valueList.append(CSSPrimitiveValue::create(CSSValueRuby));

    return CSSValueList::createSpaceSeparated(WTFMove(valueList));
}

static Ref<CSSPrimitiveValue> valueForTransitionBehavior(bool allowsDiscreteTransitions)
{
    return CSSPrimitiveValue::create(allowsDiscreteTransitions ? CSSValueAllowDiscrete : CSSValueNormal);
}

static Ref<CSSPrimitiveValue> valueForAnimationDuration(MarkableDouble duration, const Animation* animation = nullptr, const AnimationList* animationList = nullptr)
{
    auto animationListHasMultipleExplicitTimelines = [&]() {
        if (!animationList || animationList->size() <= 1)
            return false;
        auto explicitTimelines = 0;
        for (auto& animation : *animationList) {
            if (animation->isTimelineSet())
                ++explicitTimelines;
            if (explicitTimelines > 1)
                return true;
        }
        return false;
    };

    auto animationHasExplicitNonAutoTimeline = [&]() {
        if (!animation || !animation->isTimelineSet())
            return false;
        auto* timelineKeyword = std::get_if<Animation::TimelineKeyword>(&animation->timeline());
        return !timelineKeyword || *timelineKeyword != Animation::TimelineKeyword::Auto;
    };

    // https://drafts.csswg.org/css-animations-2/#animation-duration
    // For backwards-compatibility with Level 1, when the computed value of animation-timeline is auto
    // (i.e. only one list value, and that value being auto), the resolved value of auto for
    // animation-duration is 0s whenever its used value would also be 0s.
    if (!duration && (animationListHasMultipleExplicitTimelines() || animationHasExplicitNonAutoTimeline()))
        return CSSPrimitiveValue::create(CSSValueAuto);
    return CSSPrimitiveValue::create(duration.value_or(0), CSSUnitType::CSS_S);
}

static Ref<CSSPrimitiveValue> valueForAnimationDelay(double delay)
{
    return CSSPrimitiveValue::create(delay, CSSUnitType::CSS_S);
}

static Ref<CSSPrimitiveValue> valueForAnimationIterationCount(double iterationCount)
{
    if (iterationCount == Animation::IterationCountInfinite)
        return CSSPrimitiveValue::create(CSSValueInfinite);
    return CSSPrimitiveValue::create(iterationCount);
}

static Ref<CSSPrimitiveValue> valueForAnimationDirection(Animation::Direction direction)
{
    switch (direction) {
    case Animation::Direction::Normal:
        return CSSPrimitiveValue::create(CSSValueNormal);
    case Animation::Direction::Alternate:
        return CSSPrimitiveValue::create(CSSValueAlternate);
    case Animation::Direction::Reverse:
        return CSSPrimitiveValue::create(CSSValueReverse);
    case Animation::Direction::AlternateReverse:
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

static Ref<CSSValue> valueForAnimationTimeline(const RenderStyle& style, const Animation::Timeline& timeline)
{
    auto valueForAnonymousScrollTimeline = [](auto& anonymousScrollTimeline) {
        auto scroller = [&]() {
            switch (anonymousScrollTimeline.scroller) {
            case Scroller::Nearest:
                return CSSValueNearest;
            case Scroller::Root:
                return CSSValueRoot;
            case Scroller::Self:
                return CSSValueSelf;
            default:
                ASSERT_NOT_REACHED();
                return CSSValueNearest;
            }
        }();
        return CSSScrollValue::create(
            CSSPrimitiveValue::create(scroller),
            valueForConvertibleType(anonymousScrollTimeline.axis)
        );
    };

    auto valueForAnonymousViewTimeline = [&](auto& anonymousViewTimeline) {
        auto insetCSSValue = [&](auto& inset) -> RefPtr<CSSValue> {
            if (!inset)
                return nullptr;
            return CSSPrimitiveValue::create(*inset, style);
        };
        return CSSViewValue::create(
            valueForConvertibleType(anonymousViewTimeline.axis),
            insetCSSValue(anonymousViewTimeline.insets.start),
            insetCSSValue(anonymousViewTimeline.insets.end)
        );
    };

    return WTF::switchOn(timeline,
        [&](Animation::TimelineKeyword keyword) -> Ref<CSSValue> {
            return CSSPrimitiveValue::create(keyword == Animation::TimelineKeyword::None ? CSSValueNone : CSSValueAuto);
        },
        [&](const AtomString& customIdent) -> Ref<CSSValue> {
            return CSSPrimitiveValue::createCustomIdent(customIdent);
        },
        [&](const Animation::AnonymousScrollTimeline& anonymousScrollTimeline) -> Ref<CSSValue> {
            return valueForAnonymousScrollTimeline(anonymousScrollTimeline);
        },
        [&](const Animation::AnonymousViewTimeline& anonymousViewTimeline) -> Ref<CSSValue> {
            return valueForAnonymousViewTimeline(anonymousViewTimeline);
        }
    );
}

static Ref<CSSValue> valueForAnimationTimingFunction(const RenderStyle& style, const TimingFunction& timingFunction)
{
    return CSSEasingFunctionValue::create(Style::toCSSEasingFunction(timingFunction, style));
}

static Ref<CSSValue> valueForSingleAnimationRange(const RenderStyle& style, const SingleTimelineRange& range, SingleTimelineRange::Type type)
{
    CSSValueListBuilder list;
    if (range.name != SingleTimelineRange::Name::Omitted)
        list.append(CSSPrimitiveValue::create(SingleTimelineRange::valueID(range.name)));
    if (!SingleTimelineRange::isDefault(range.offset, type))
        list.append(ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, range.offset));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForAnimationRange(const RenderStyle& style, const TimelineRange& range)
{
    CSSValueListBuilder list;
    auto rangeStart = range.start;
    auto rangeEnd = range.end;

    RefPtr startValue = dynamicDowncast<CSSValueList>(valueForSingleAnimationRange(style, rangeStart, SingleTimelineRange::Type::Start));
    if (startValue && startValue->length())
        list.append(*startValue);

    RefPtr endValue = dynamicDowncast<CSSValueList>(valueForSingleAnimationRange(style, rangeEnd, SingleTimelineRange::Type::End));
    bool endValueEqualsStart = startValue && endValue && startValue->equals(*endValue);
    bool isNormal = rangeEnd.name == SingleTimelineRange::Name::Normal;
    bool isDefaultAndSameNameAsStart = rangeStart.name == rangeEnd.name && SingleTimelineRange::isDefault(rangeEnd.offset, SingleTimelineRange::Type::End);
    if (endValue && endValue->length() && !endValueEqualsStart && !isNormal && !isDefaultAndSameNameAsStart)
        list.append(*endValue);

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static void addValueForAnimationPropertyToList(const RenderStyle& style, CSSValueListBuilder& list, CSSPropertyID property, const Animation* animation, const AnimationList* animationList)
{
    switch (property) {
    case CSSPropertyTransitionBehavior:
        if (!animation || !animation->isAllowsDiscreteTransitionsFilled())
            list.append(valueForTransitionBehavior(animation ? animation->allowsDiscreteTransitions() : Animation::initialAllowsDiscreteTransitions()));
        break;
    case CSSPropertyAnimationDuration:
    case CSSPropertyTransitionDuration:
        if (!animation || !animation->isDurationFilled())
            list.append(valueForAnimationDuration(animation ? animation->duration() : Animation::initialDuration(), animation, animationList));
        break;
    case CSSPropertyAnimationDelay:
    case CSSPropertyTransitionDelay:
        if (!animation || !animation->isDelayFilled())
            list.append(valueForAnimationDelay(animation ? animation->delay() : Animation::initialDelay()));
        break;
    case CSSPropertyAnimationIterationCount:
        if (!animation || !animation->isIterationCountFilled())
            list.append(valueForAnimationIterationCount(animation ? animation->iterationCount() : Animation::initialIterationCount()));
        break;
    case CSSPropertyAnimationDirection:
        if (!animation || !animation->isDirectionFilled())
            list.append(valueForAnimationDirection(animation ? animation->direction() : Animation::initialDirection()));
        break;
    case CSSPropertyAnimationFillMode:
        if (!animation || !animation->isFillModeFilled())
            list.append(valueForAnimationFillMode(animation ? animation->fillMode() : Animation::initialFillMode()));
        break;
    case CSSPropertyAnimationPlayState:
        if (!animation || !animation->isPlayStateFilled())
            list.append(valueForAnimationPlayState(animation ? animation->playState() : Animation::initialPlayState()));
        break;
    case CSSPropertyAnimationName:
        list.append(valueForScopedName(animation ? animation->name() : Animation::initialName()));
        break;
    case CSSPropertyAnimationComposition:
        if (!animation || !animation->isCompositeOperationFilled())
            list.append(valueForAnimationComposition(animation ? animation->compositeOperation() : Animation::initialCompositeOperation()));
        break;
    case CSSPropertyAnimationTimeline:
        if (!animation || !animation->isTimelineFilled())
            list.append(valueForAnimationTimeline(style, animation ? animation->timeline() : Animation::initialTimeline()));
        break;
    case CSSPropertyTransitionProperty:
        if (animation) {
            if (!animation->isPropertyFilled())
                list.append(valueForTransitionProperty(*animation));
        } else
            list.append(CSSPrimitiveValue::create(CSSValueAll));
        break;
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
        if (animation) {
            if (!animation->isTimingFunctionFilled())
                list.append(valueForAnimationTimingFunction(style, *animation->timingFunction()));
        } else
            list.append(valueForAnimationTimingFunction(style, CubicBezierTimingFunction::defaultTimingFunction()));
        break;
    case CSSPropertyAnimationRangeStart:
        if (!animation || !animation->isRangeStartFilled())
            list.append(valueForSingleAnimationRange(style, animation ? animation->rangeStart() : Animation::initialRangeStart(), SingleTimelineRange::Type::Start));
        break;
    case CSSPropertyAnimationRangeEnd:
        if (!animation || !animation->isRangeEndFilled())
            list.append(valueForSingleAnimationRange(style, animation ? animation->rangeEnd() : Animation::initialRangeEnd(), SingleTimelineRange::Type::End));
        break;
    case CSSPropertyAnimationRange:
        if (!animation || !animation->isRangeFilled())
            list.append(valueForAnimationRange(style, animation ? animation->range() : Animation::initialRange()));
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

static Ref<CSSValueList> valueForAnimationOrTransition(const RenderStyle& style, CSSPropertyID property, const AnimationList* animationList)
{
    CSSValueListBuilder list;
    if (animationList) {
        for (auto& animation : *animationList)
            addValueForAnimationPropertyToList(style, list, property, animation.ptr(), animationList);
    } else
        addValueForAnimationPropertyToList(style, list, property, nullptr, nullptr);
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static Ref<CSSValue> singleAnimationValue(const RenderStyle& style, const Animation& animation)
{
    static NeverDestroyed<Ref<TimingFunction>> initialTimingFunction(Animation::initialTimingFunction());

    static NeverDestroyed<String> alternate { "alternate"_s };
    static NeverDestroyed<String> alternateReverse { "alternate-reverse"_s };
    static NeverDestroyed<String> backwards { "backwards"_s };
    static NeverDestroyed<String> both { "both"_s };
    static NeverDestroyed<String> ease { "ease"_s };
    static NeverDestroyed<String> easeIn { "ease-in"_s };
    static NeverDestroyed<String> easeInOut { "ease-in-out"_s };
    static NeverDestroyed<String> easeOut { "ease-out"_s };
    static NeverDestroyed<String> forwards { "forwards"_s };
    static NeverDestroyed<String> infinite { "infinite"_s };
    static NeverDestroyed<String> linear { "linear"_s };
    static NeverDestroyed<String> normal { "normal"_s };
    static NeverDestroyed<String> paused { "paused"_s };
    static NeverDestroyed<String> reverse { "reverse"_s };
    static NeverDestroyed<String> running { "running"_s };
    static NeverDestroyed<String> stepEnd { "step-end"_s };
    static NeverDestroyed<String> stepStart { "step-start"_s };

    // If we have an animation-delay but no animation-duration set, we must serialze
    // the animation-duration because they're both <time> values and animation-delay
    // comes first.
    auto showsDelay = animation.delay() != Animation::initialDelay();
    auto showsDuration = showsDelay || animation.duration() != Animation::initialDuration();

    auto showsTimingFunction = [&]() {
        auto* timingFunction = animation.timingFunction();
        if (timingFunction && *timingFunction != initialTimingFunction.get())
            return true;
        auto& name = animation.name().name;
        return name == ease || name == easeIn || name == easeInOut || name == easeOut || name == linear || name == stepEnd || name == stepStart;
    };

    auto showsIterationCount = [&]() {
        if (animation.iterationCount() != Animation::initialIterationCount())
            return true;
        return animation.name().name == infinite;
    };

    auto showsDirection = [&]() {
        if (animation.direction() != Animation::initialDirection())
            return true;
        auto& name = animation.name().name;
        return name == normal || name == reverse || name == alternate || name == alternateReverse;
    };

    auto showsFillMode = [&]() {
        if (animation.fillMode() != Animation::initialFillMode())
            return true;
        auto& name = animation.name().name;
        return name == forwards || name == backwards || name == both;
    };

    auto showsPlaysState = [&]() {
        if (animation.playState() != Animation::initialPlayState())
            return true;
        auto& name = animation.name().name;
        return name == running || name == paused;
    };

    CSSValueListBuilder list;
    if (showsDuration)
        list.append(valueForAnimationDuration(animation.duration()));
    if (showsTimingFunction())
        list.append(valueForAnimationTimingFunction(style, *animation.timingFunction()));
    if (showsDelay)
        list.append(valueForAnimationDelay(animation.delay()));
    if (showsIterationCount())
        list.append(valueForAnimationIterationCount(animation.iterationCount()));
    if (showsDirection())
        list.append(valueForAnimationDirection(animation.direction()));
    if (showsFillMode())
        list.append(valueForAnimationFillMode(animation.fillMode()));
    if (showsPlaysState())
        list.append(valueForAnimationPlayState(animation.playState()));
    if (animation.name() != Animation::initialName())
        list.append(valueForScopedName(animation.name()));
    if (animation.timeline() != Animation::initialTimeline())
        list.append(valueForAnimationTimeline(style, animation.timeline()));
    if (animation.compositeOperation() != Animation::initialCompositeOperation())
        list.append(valueForAnimationComposition(animation.compositeOperation()));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForAnimationShorthand(const RenderStyle& style, const AnimationList* animations)
{
    if (!animations || animations->isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& animation : *animations) {
        // If any of the reset-only longhands are set, we cannot serialize this value.
        if (animation->isTimelineSet() || animation->isRangeStartSet() || animation->isRangeEndSet()) {
            list.clear();
            break;
        }
        list.append(singleAnimationValue(style, animation));
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static Ref<CSSValue> singleTransitionValue(const RenderStyle& style, const Animation& transition)
{
    static NeverDestroyed<Ref<TimingFunction>> initialTimingFunction(Animation::initialTimingFunction());

    // If we have a transition-delay but no transition-duration set, we must serialze
    // the transition-duration because they're both <time> values and transition-delay
    // comes first.
    auto showsDelay = transition.delay() != Animation::initialDelay();
    auto showsDuration = showsDelay || transition.duration() != Animation::initialDuration();

    CSSValueListBuilder list;
    if (transition.property() != Animation::initialProperty())
        list.append(valueForTransitionProperty(transition));
    if (showsDuration)
        list.append(valueForAnimationDuration(transition.duration()));
    if (auto* timingFunction = transition.timingFunction(); *timingFunction != initialTimingFunction.get())
        list.append(valueForAnimationTimingFunction(style, *timingFunction));
    if (showsDelay)
        list.append(valueForAnimationDelay(transition.delay()));
    if (transition.allowsDiscreteTransitions() != Animation::initialAllowsDiscreteTransitions())
        list.append(valueForTransitionBehavior(transition.allowsDiscreteTransitions()));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueAll);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForTransitionShorthand(const RenderStyle& style)
{
    auto transitions = style.transitions();
    if (!transitions || transitions->isEmpty())
        return CSSPrimitiveValue::create(CSSValueAll);

    CSSValueListBuilder list;
    for (auto& transition : *transitions)
        list.append(singleTransitionValue(style, transition));
    ASSERT(!list.isEmpty());
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForWebkitLineBoxContain(const RenderStyle& style)
{
    auto lineBoxContain = style.lineBoxContain();
    if (!lineBoxContain)
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    if (lineBoxContain.contains(Style::LineBoxContain::Block))
        list.append(CSSPrimitiveValue::create(CSSValueBlock));
    if (lineBoxContain.contains(Style::LineBoxContain::Inline))
        list.append(CSSPrimitiveValue::create(CSSValueInline));
    if (lineBoxContain.contains(Style::LineBoxContain::Font))
        list.append(CSSPrimitiveValue::create(CSSValueFont));
    if (lineBoxContain.contains(Style::LineBoxContain::Glyphs))
        list.append(CSSPrimitiveValue::create(CSSValueGlyphs));
    if (lineBoxContain.contains(Style::LineBoxContain::Replaced))
        list.append(CSSPrimitiveValue::create(CSSValueReplaced));
    if (lineBoxContain.contains(Style::LineBoxContain::InlineBox))
        list.append(CSSPrimitiveValue::create(CSSValueInlineBox));
    if (lineBoxContain.contains(Style::LineBoxContain::InitialLetter))
        list.append(CSSPrimitiveValue::create(CSSValueInitialLetter));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForWebkitRubyPosition(RubyPosition position)
{
    return CSSPrimitiveValue::create([&] {
        switch (position) {
        case RubyPosition::Over:
            return CSSValueBefore;
        case RubyPosition::Under:
            return CSSValueAfter;
        case RubyPosition::InterCharacter:
        case RubyPosition::LegacyInterCharacter:
            return CSSValueInterCharacter;
        }
        return CSSValueBefore;
    }());
}

static Ref<CSSValue> valueForPosition(const RenderStyle& style, const LengthPoint& position)
{
    return CSSValueList::createSpaceSeparated(
        ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, position.x),
        ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, position.y)
    );
}

static bool isAuto(const LengthPoint& position)
{
    return position.x.isAuto() && position.y.isAuto();
}

static bool isNormal(const LengthPoint& position)
{
    return position.x.isNormal();
}

static Ref<CSSValue> valueForPositionOrAuto(const RenderStyle& style, const LengthPoint& position)
{
    if (isAuto(position))
        return CSSPrimitiveValue::create(CSSValueAuto);
    return valueForPosition(style, position);
}

static Ref<CSSValue> valueForPositionOrAutoOrNormal(const RenderStyle& style, const LengthPoint& position)
{
    if (isAuto(position))
        return CSSPrimitiveValue::create(CSSValueAuto);
    if (isNormal(position))
        return CSSPrimitiveValue::create(CSSValueNormal);
    return valueForPosition(style, position);
}

static Ref<CSSValue> valueForOutlineStyle(const RenderStyle& style)
{
    if (style.outlineStyleIsAuto() == OutlineIsAuto::On)
        return CSSPrimitiveValue::create(CSSValueAuto);
    return valueForConvertibleType(style.outlineStyle());
}

static Ref<CSSValue> valueForD(const RenderStyle& style)
{
    auto path = style.d();
    if (!path)
        return CSSPrimitiveValue::create(CSSValueNone);
    Ref protectedPath = *path;
    return CSSPathValue::create(Style::overrideToCSS(protectedPath->path(), style, Style::PathConversion::ForceAbsolute));
}

static Ref<CSSValue> valueForBasicShape(const RenderStyle& style, const Style::BasicShape& basicShape, Style::PathConversion conversion)
{
    return CSSBasicShapeValue::create(
        WTF::switchOn(basicShape,
            [&](const auto& shape) {
                return CSS::BasicShape { Style::toCSS(shape, style) };
            },
            [&](const Style::PathFunction& path) {
                return CSS::BasicShape { Style::overrideToCSS(path, style, conversion) };
            }
        )
    );
}

static Ref<CSSValue> valueForPathOperation(const RenderStyle& style, const PathOperation* operation, Style::PathConversion conversion = Style::PathConversion::None)
{
    if (!operation)
        return CSSPrimitiveValue::create(CSSValueNone);

    switch (operation->type()) {
    case PathOperation::Type::Reference:
        return CSSURLValue::create(Style::toCSS(uncheckedDowncast<ReferencePathOperation>(*operation).url(), style));

    case PathOperation::Type::Shape: {
        auto& shapeOperation = uncheckedDowncast<ShapePathOperation>(*operation);
        if (shapeOperation.referenceBox() == CSSBoxType::BoxMissing)
            return CSSValueList::createSpaceSeparated(valueForBasicShape(style, shapeOperation.shape(), conversion));
        return CSSValueList::createSpaceSeparated(valueForBasicShape(style, shapeOperation.shape(), conversion),
            valueForConvertibleType(shapeOperation.referenceBox()));
    }

    case PathOperation::Type::Box:
        return valueForConvertibleType(uncheckedDowncast<BoxPathOperation>(*operation).referenceBox());

    case PathOperation::Type::Ray: {
        auto& ray = uncheckedDowncast<RayPathOperation>(*operation);
        return CSSRayValue::create(Style::toCSS(ray.ray(), style), ray.referenceBox());
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
    case ContainIntrinsicSizeType::Length:
        return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, containIntrinsicLength.value());
    case ContainIntrinsicSizeType::AutoAndLength:
        return CSSValuePair::create(CSSPrimitiveValue::create(CSSValueAuto),
            ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, containIntrinsicLength.value()));
    case ContainIntrinsicSizeType::AutoAndNone:
        return CSSValuePair::create(CSSPrimitiveValue::create(CSSValueAuto), CSSPrimitiveValue::create(CSSValueNone));
    }
    RELEASE_ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueNone);
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

static Ref<CSSPrimitiveValue> valueForFamily(const AtomString& family, CSSValuePool& pool)
{
    if (auto familyIdentifier = identifierForFamily(family))
        return CSSPrimitiveValue::create(familyIdentifier);
    return pool.createFontFamilyValue(family);
}

static Ref<CSSValue> valueForTouchAction(const RenderStyle& style)
{
    auto touchActions = style.touchActions();

    if (touchActions & TouchAction::Auto)
        return CSSPrimitiveValue::create(CSSValueAuto);
    if (touchActions & TouchAction::None)
        return CSSPrimitiveValue::create(CSSValueNone);
    if (touchActions & TouchAction::Manipulation)
        return CSSPrimitiveValue::create(CSSValueManipulation);

    CSSValueListBuilder list;
    if (touchActions & TouchAction::PanX)
        list.append(CSSPrimitiveValue::create(CSSValuePanX));
    if (touchActions & TouchAction::PanY)
        list.append(CSSPrimitiveValue::create(CSSValuePanY));
    if (touchActions & TouchAction::PinchZoom)
        list.append(CSSPrimitiveValue::create(CSSValuePinchZoom));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

#if PLATFORM(IOS_FAMILY)
static Ref<CSSValue> valueForWebkitTouchCallout(const RenderStyle& style)
{
    return CSSPrimitiveValue::create(style.touchCalloutEnabled() ? CSSValueDefault : CSSValueNone);
}
#endif

static Ref<CSSValue> valueForTextTransform(const RenderStyle& style)
{
    auto textTransform = style.textTransform();

    CSSValueListBuilder list;
    if (textTransform.contains(TextTransform::Capitalize))
        list.append(CSSPrimitiveValue::create(CSSValueCapitalize));
    else if (textTransform.contains(TextTransform::Uppercase))
        list.append(CSSPrimitiveValue::create(CSSValueUppercase));
    else if (textTransform.contains(TextTransform::Lowercase))
        list.append(CSSPrimitiveValue::create(CSSValueLowercase));

    if (textTransform.contains(TextTransform::FullWidth))
        list.append(CSSPrimitiveValue::create(CSSValueFullWidth));

    if (textTransform.contains(TextTransform::FullSizeKana))
        list.append(CSSPrimitiveValue::create(CSSValueFullSizeKana));

    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForTextDecorationLine(OptionSet<TextDecorationLine> textDecorationLine)
{
    // Blink value is ignored.
    CSSValueListBuilder list;
    if (textDecorationLine & TextDecorationLine::Underline)
        list.append(CSSPrimitiveValue::create(CSSValueUnderline));
    if (textDecorationLine & TextDecorationLine::Overline)
        list.append(CSSPrimitiveValue::create(CSSValueOverline));
    if (textDecorationLine & TextDecorationLine::LineThrough)
        list.append(CSSPrimitiveValue::create(CSSValueLineThrough));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static RefPtr<CSSValue> valueForTextDecorationSkipShorthand(TextDecorationSkipInk textDecorationSkipInk)
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

static Ref<CSSValue> valueForTextUnderlineOffset(const RenderStyle& style, const TextUnderlineOffset& textUnderlineOffset)
{
    if (textUnderlineOffset.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    ASSERT(textUnderlineOffset.isLength());
    auto& length = textUnderlineOffset.length();
    if (length.isPercent())
        return CSSPrimitiveValue::create(length.percent(), CSSUnitType::CSS_PERCENTAGE);
    return CSSPrimitiveValue::create(length, style);
}

static Ref<CSSValue> valueForTextDecorationThickness(const RenderStyle& style, const TextDecorationThickness& textDecorationThickness)
{
    if (textDecorationThickness.isAuto())
        return CSSPrimitiveValue::create(CSSValueAuto);
    if (textDecorationThickness.isFromFont())
        return CSSPrimitiveValue::create(CSSValueFromFont);

    ASSERT(textDecorationThickness.isLength());
    auto& length = textDecorationThickness.length();
    if (length.isPercent())
        return CSSPrimitiveValue::create(length.percent(), CSSUnitType::CSS_PERCENTAGE);
    return CSSPrimitiveValue::create(length, style);
}

static Ref<CSSValue> valueForTextEmphasisPosition(OptionSet<TextEmphasisPosition> textEmphasisPosition)
{
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Over) && (textEmphasisPosition & TextEmphasisPosition::Under)));
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Left) && (textEmphasisPosition & TextEmphasisPosition::Right)));
    ASSERT((textEmphasisPosition & TextEmphasisPosition::Over) || (textEmphasisPosition & TextEmphasisPosition::Under));

    CSSValueListBuilder list;
    if (textEmphasisPosition & TextEmphasisPosition::Over)
        list.append(CSSPrimitiveValue::create(CSSValueOver));
    if (textEmphasisPosition & TextEmphasisPosition::Under)
        list.append(CSSPrimitiveValue::create(CSSValueUnder));
    if (textEmphasisPosition & TextEmphasisPosition::Left)
        list.append(CSSPrimitiveValue::create(CSSValueLeft));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForTextEmphasisStyle(const RenderStyle& style)
{
    switch (style.textEmphasisMark()) {
    case TextEmphasisMark::None:
        return CSSPrimitiveValue::create(CSSValueNone);
    case TextEmphasisMark::Custom:
        return CSSPrimitiveValue::create(style.textEmphasisCustomMark());
    case TextEmphasisMark::Auto:
        ASSERT_NOT_REACHED();
#if !ASSERT_ENABLED
        [[fallthrough]];
#endif
    case TextEmphasisMark::Dot:
    case TextEmphasisMark::Circle:
    case TextEmphasisMark::DoubleCircle:
    case TextEmphasisMark::Triangle:
    case TextEmphasisMark::Sesame:
        if (style.textEmphasisFill() == TextEmphasisFill::Filled)
            return CSSValueList::createSpaceSeparated(valueForConvertibleType(style.textEmphasisMark()));
        return CSSValueList::createSpaceSeparated(valueForConvertibleType(style.textEmphasisFill()),
            valueForConvertibleType(style.textEmphasisMark()));
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Ref<CSSValue> valueForTextEmphasisShorthand(const RenderStyle& style, CSSValuePool& pool)
{
    return CSSValueList::createSpaceSeparated(
        valueForTextEmphasisStyle(style),
        valueForColor(style, pool, style.textEmphasisColor())
    );
}

static Ref<CSSValue> valueForTextUnderlinePosition(OptionSet<TextUnderlinePosition> textUnderlinePosition)
{
    ASSERT(!((textUnderlinePosition & TextUnderlinePosition::FromFont) && (textUnderlinePosition & TextUnderlinePosition::Under)));
    ASSERT(!((textUnderlinePosition & TextUnderlinePosition::Left) && (textUnderlinePosition & TextUnderlinePosition::Right)));

    if (textUnderlinePosition.isEmpty())
        return CSSPrimitiveValue::create(CSSValueAuto);
    bool isFromFont = textUnderlinePosition.contains(TextUnderlinePosition::FromFont);
    bool isUnder = textUnderlinePosition .contains(TextUnderlinePosition::Under);
    bool isLeft = textUnderlinePosition.contains(TextUnderlinePosition::Left);
    bool isRight = textUnderlinePosition.contains(TextUnderlinePosition::Right);

    auto metric = isUnder ? CSSValueUnder : CSSValueFromFont;
    auto side = isLeft ? CSSValueLeft : CSSValueRight;
    if (!isFromFont && !isUnder)
        return CSSPrimitiveValue::create(side);
    if (!isLeft && !isRight)
        return CSSPrimitiveValue::create(metric);
    return CSSValuePair::create(CSSPrimitiveValue::create(metric), CSSPrimitiveValue::create(side));
}

static Ref<CSSValue> valueForSpeakAs(const RenderStyle& style)
{
    auto speakAs = style.speakAs();
    CSSValueListBuilder list;
    if (speakAs & SpeakAs::SpellOut)
        list.append(CSSPrimitiveValue::create(CSSValueSpellOut));
    if (speakAs & SpeakAs::Digits)
        list.append(CSSPrimitiveValue::create(CSSValueDigits));
    if (speakAs & SpeakAs::LiteralPunctuation)
        list.append(CSSPrimitiveValue::create(CSSValueLiteralPunctuation));
    if (speakAs & SpeakAs::NoPunctuation)
        list.append(CSSPrimitiveValue::create(CSSValueNoPunctuation));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNormal);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForHangingPunctuation(OptionSet<HangingPunctuation> hangingPunctuation)
{
    CSSValueListBuilder list;
    if (hangingPunctuation & HangingPunctuation::First)
        list.append(CSSPrimitiveValue::create(CSSValueFirst));
    if (hangingPunctuation & HangingPunctuation::AllowEnd)
        list.append(CSSPrimitiveValue::create(CSSValueAllowEnd));
    if (hangingPunctuation & HangingPunctuation::ForceEnd)
        list.append(CSSPrimitiveValue::create(CSSValueForceEnd));
    if (hangingPunctuation & HangingPunctuation::Last)
        list.append(CSSPrimitiveValue::create(CSSValueLast));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForContent(const RenderStyle& style)
{
    CSSValueListBuilder list;
    for (auto* contentData = style.contentData(); contentData; contentData = contentData->next()) {
        if (auto* counterContentData = dynamicDowncast<CounterContentData>(*contentData)) {
            RefPtr counterStyle = CSSPrimitiveValue::createCustomIdent(counterContentData->counter().listStyleType().identifier);
            list.append(CSSCounterValue::create(counterContentData->counter().identifier(), counterContentData->counter().separator(), WTFMove(counterStyle)));
        } else if (auto* imageContentData = dynamicDowncast<ImageContentData>(*contentData))
            list.append(imageContentData->image().computedStyleValue(style));
        else if (auto* quoteContentData = dynamicDowncast<QuoteContentData>(*contentData))
            list.append(valueForConvertibleType(quoteContentData->quote()));
        else if (auto* textContentData = dynamicDowncast<TextContentData>(*contentData))
            list.append(CSSPrimitiveValue::create(textContentData->text()));
        else {
            ASSERT_NOT_REACHED();
            continue;
        }
    }
    if (list.isEmpty())
        list.append(CSSPrimitiveValue::create(style.hasUsedContentNone() ? CSSValueNone : CSSValueNormal));
    else if (auto& altText = style.contentAltText(); !altText.isNull())
        return CSSValuePair::createSlashSeparated(CSSValueList::createSpaceSeparated(WTFMove(list)), CSSPrimitiveValue::create(altText));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForCounter(const RenderStyle& style, CSSPropertyID propertyID)
{
    auto& map = style.counterDirectives().map;
    if (map.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& keyValue : map) {
        auto number = [&]() -> std::optional<int> {
            switch (propertyID) {
            case CSSPropertyCounterIncrement:
                return keyValue.value.incrementValue;
            case CSSPropertyCounterReset:
                return keyValue.value.resetValue;
            case CSSPropertyCounterSet:
                return keyValue.value.setValue;
            default:
                ASSERT_NOT_REACHED();
                return std::nullopt;
            }
        }();
        if (number) {
            list.append(CSSPrimitiveValue::createCustomIdent(keyValue.key));
            list.append(CSSPrimitiveValue::createInteger(*number));
        }
    }
    if (!list.isEmpty())
        return CSSValueList::createSpaceSeparated(WTFMove(list));
    return CSSPrimitiveValue::create(CSSValueNone);
}

static Ref<CSSValueList> valueForFontFamilyList(const RenderStyle& style, CSSValuePool& pool)
{
    CSSValueListBuilder list;
    for (unsigned i = 0; i < style.fontCascade().familyCount(); ++i)
        list.append(valueForFamily(style.fontCascade().familyAt(i), pool));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForFontFamily(const RenderStyle& style, CSSValuePool& pool)
{
    if (style.fontCascade().familyCount() == 1)
        return valueForFamily(style.fontCascade().familyAt(0), pool);
    return valueForFontFamilyList(style, pool);
}

static RefPtr<CSSPrimitiveValue> valueForOptionalLineHeight(const RenderStyle& style, ComputedStyleExtractor::PropertyValueType valueType)
{
    Length length = style.lineHeight();
    if (length.isNormal())
        return nullptr;
    if (length.isPercent()) {
        // BuilderConverter::convertLineHeight() will convert a percentage value to a fixed value,
        // and a number value to a percentage value. To be able to roundtrip a number value, we thus
        // look for a percent value and convert it back to a number.
        if (valueType == ComputedStyleExtractor::PropertyValueType::Computed)
            return CSSPrimitiveValue::create(length.value() / 100);

        // This is imperfect, because it doesn't include the zoom factor and the real computation
        // for how high to be in pixels does include things like minimum font size and the zoom factor.
        // On the other hand, since font-size doesn't include the zoom factor, we really can't do
        // that here either.
        return valueForZoomAdjustedFloatLength(style, static_cast<double>(length.percent() * style.fontDescription().computedSize()) / 100);
    }
    return valueForZoomAdjustedFloatLength(style, floatValueForLength(length, 0));
}

static Ref<CSSPrimitiveValue> valueForLineHeight(const RenderStyle& style, ComputedStyleExtractor::PropertyValueType valueType)
{
    if (auto lineHeight = valueForOptionalLineHeight(style, valueType))
        return lineHeight.releaseNonNull();
    return CSSPrimitiveValue::create(CSSValueNormal);
}

static Ref<CSSPrimitiveValue> valueForFontSize(const RenderStyle& style)
{
    return valueForZoomAdjustedFloatLength(style, style.fontDescription().computedSize());
}

static Ref<CSSPrimitiveValue> valueForFontPalette(const RenderStyle& style)
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

static Ref<CSSPrimitiveValue> valueForFontWeight(FontSelectionValue weight)
{
    return CSSPrimitiveValue::create(static_cast<float>(weight));
}

static Ref<CSSPrimitiveValue> valueForFontWeight(const RenderStyle& style)
{
    return valueForFontWeight(style.fontDescription().weight());
}

static Ref<CSSPrimitiveValue> valueForFontWidth(FontSelectionValue width)
{
    return CSSPrimitiveValue::create(static_cast<float>(width), CSSUnitType::CSS_PERCENTAGE);
}

static Ref<CSSPrimitiveValue> valueForFontWidth(const RenderStyle& style)
{
    return valueForFontWidth(style.fontDescription().width());
}

static Ref<CSSValue> valueFromFontStyle(std::optional<FontSelectionValue> italic, FontStyleAxis axis)
{
    if (auto keyword = fontStyleKeyword(italic, axis))
        return CSSPrimitiveValue::create(keyword.value());
    float angle = *italic;
    return CSSFontStyleWithAngleValue::create(CSSFontStyleWithAngleValue::ObliqueAngle { CSS::AngleUnit::Deg, angle });
}

static Ref<CSSValue> valueFromFontStyle(const RenderStyle& style)
{
    return valueFromFontStyle(style.fontDescription().italic(), style.fontDescription().fontStyleAxis());
}

static Ref<CSSValue> valueForFontSynthesis(const RenderStyle& style)
{
    CSSValueListBuilder list;
    if (style.fontDescription().hasAutoFontSynthesisWeight())
        list.append(CSSPrimitiveValue::create(CSSValueWeight));
    if (style.fontDescription().hasAutoFontSynthesisStyle())
        list.append(CSSPrimitiveValue::create(CSSValueStyle));
    if (style.fontDescription().hasAutoFontSynthesisSmallCaps())
        list.append(CSSPrimitiveValue::create(CSSValueSmallCaps));
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForFontSynthesisLonghand(FontSynthesisLonghandValue value)
{
    return CSSPrimitiveValue::create(value == FontSynthesisLonghandValue::Auto ? CSSValueAuto : CSSValueNone);
}

static Ref<CSSValue> valueForFontSynthesisWeight(const RenderStyle& style)
{
    return valueForFontSynthesisLonghand(style.fontDescription().fontSynthesisWeight());
}

static Ref<CSSValue> valueForFontSynthesisStyle(const RenderStyle& style)
{
    return valueForFontSynthesisLonghand(style.fontDescription().fontSynthesisStyle());
}

static Ref<CSSValue> valueForFontSynthesisSmallCaps(const RenderStyle& style)
{
    return valueForFontSynthesisLonghand(style.fontDescription().fontSynthesisSmallCaps());
}

static Ref<CSSValue> valueForFontFeatureSettings(const RenderStyle& style)
{
    auto& featureSettings = style.fontDescription().featureSettings();
    if (!featureSettings.size())
        return CSSPrimitiveValue::create(CSSValueNormal);
    CSSValueListBuilder list;
    for (auto& feature : featureSettings)
        list.append(CSSFontFeatureValue::create(FontTag(feature.tag()), CSSPrimitiveValue::createInteger(feature.value())));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

#if ENABLE(VARIATION_FONTS)
static Ref<CSSValue> valueForFontVariationSettings(const RenderStyle& style)
{
    auto& variationSettings = style.fontDescription().variationSettings();
    if (variationSettings.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNormal);
    CSSValueListBuilder list;
    for (auto& feature : variationSettings)
        list.append(CSSFontVariationValue::create(feature.tag(), CSSPrimitiveValue::create(feature.value())));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}
#endif

typedef const Length& (RenderStyle::*RenderStyleLengthGetter)() const;
typedef LayoutUnit (RenderBoxModelObject::*RenderBoxComputedCSSValueGetter)() const;

template<RenderStyleLengthGetter lengthGetter, RenderBoxComputedCSSValueGetter computedCSSValueGetter>
static RefPtr<CSSValue> valueForZoomAdjustedPaddingPixel(const RenderStyle& style, RenderObject* renderer)
{
    Length unzoomedLength = (style.*lengthGetter)();
    auto* renderBox = dynamicDowncast<RenderBox>(renderer);
    if (!renderBox || unzoomedLength.isFixed())
        return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, unzoomedLength);
    return valueForZoomAdjustedFloatLength(style, (renderBox->*computedCSSValueGetter)());
}

template<RenderStyleLengthGetter lengthGetter, RenderBoxComputedCSSValueGetter computedCSSValueGetter>
static RefPtr<CSSValue> valueForZoomAdjustedMarginPixel(const RenderStyle& style, RenderObject* renderer)
{
    auto* renderBox = dynamicDowncast<RenderBox>(renderer);
    if (!renderBox) {
        Length unzoomedLength = (style.*lengthGetter)();
        return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, unzoomedLength);
    }
    return valueForZoomAdjustedFloatLength(style, (renderBox->*computedCSSValueGetter)());
}

static inline bool isNonReplacedInline(RenderObject& renderer)
{
    return renderer.isInline() && !renderer.isReplacedOrAtomicInline();
}

static RefPtr<CSSValue> valueForPageBreak(const RenderStyle&, BreakBetween value)
{
    if (value == BreakBetween::Page || value == BreakBetween::LeftPage || value == BreakBetween::RightPage
        || value == BreakBetween::RectoPage || value == BreakBetween::VersoPage)
        return CSSPrimitiveValue::create(CSSValueAlways); // CSS 2.1 allows us to map these to always.
    if (value == BreakBetween::Avoid || value == BreakBetween::AvoidPage)
        return CSSPrimitiveValue::create(CSSValueAvoid);
    return CSSPrimitiveValue::create(CSSValueAuto);
}

static RefPtr<CSSValue> valueForPageBreak(const RenderStyle&, BreakInside value)
{
    if (value == BreakInside::Avoid || value == BreakInside::AvoidPage)
        return CSSPrimitiveValue::create(CSSValueAvoid);
    return CSSPrimitiveValue::create(CSSValueAuto);
}

static RefPtr<CSSValue> valueForColumnBreak(const RenderStyle&, BreakBetween value)
{
    if (value == BreakBetween::Column)
        return CSSPrimitiveValue::create(CSSValueAlways);
    if (value == BreakBetween::Avoid || value == BreakBetween::AvoidColumn)
        return CSSPrimitiveValue::create(CSSValueAvoid);
    return CSSPrimitiveValue::create(CSSValueAuto);
}

static RefPtr<CSSValue> valueForColumnBreak(const RenderStyle&, BreakInside value)
{
    if (value == BreakInside::Avoid || value == BreakInside::AvoidColumn)
        return CSSPrimitiveValue::create(CSSValueAvoid);
    return CSSPrimitiveValue::create(CSSValueAuto);
}

static LayoutRect sizingBox(RenderObject& renderer)
{
    auto* box = dynamicDowncast<RenderBox>(renderer);
    if (!box)
        return LayoutRect();

    return box->style().boxSizing() == BoxSizing::BorderBox ? box->borderBoxRect() : box->computedCSSContentBoxRect();
}

static RefPtr<CSSValue> valueForHeight(const RenderStyle& style, RenderElement* renderer)
{
    if (renderer && !renderer->isRenderOrLegacyRenderSVGModelObject()) {
        // According to http://www.w3.org/TR/CSS2/visudet.html#the-height-property,
        // the "height" property does not apply for non-replaced inline elements.
        if (!isNonReplacedInline(*renderer))
            return valueForZoomAdjustedFloatLength(style, sizingBox(*renderer).height());
    }
    return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, style.height());
}

static RefPtr<CSSValue> valueForWidth(const RenderStyle& style, RenderElement* renderer)
{
    if (renderer && !renderer->isRenderOrLegacyRenderSVGModelObject()) {
        // According to http://www.w3.org/TR/CSS2/visudet.html#the-width-property,
        // the "width" property does not apply for non-replaced inline elements.
        if (!isNonReplacedInline(*renderer))
            return valueForZoomAdjustedFloatLength(style, sizingBox(*renderer).width());
    }
    return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, style.width());
}

static Ref<CSSValue> valueForMaxSize(const RenderStyle& style, const Length& length)
{
    if (length.isUndefined())
        return CSSPrimitiveValue::create(CSSValueNone);
    return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, length);
}

static Ref<CSSValue> valueForMinSize(const RenderStyle& style, const Length& length, RenderElement* renderer)
{
    auto isFlexOrGridItem = [](auto renderer) {
        auto* box = dynamicDowncast<RenderBox>(renderer);
        return box && (box->isFlexItem() || box->isGridItem());
    };

    if (length.isAuto()) {
        if (isFlexOrGridItem(renderer))
            return CSSPrimitiveValue::create(CSSValueAuto);
        return valueForZoomAdjustedFloatLength(style, 0);
    }
    return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, length);
}

static RefPtr<CSSValue> valueForPerspectiveOrigin(const RenderStyle& style, RenderElement* renderer)
{
    if (renderer) {
        auto box = renderer->transformReferenceBoxRect(style);
        return CSSValueList::createSpaceSeparated(
            valueForZoomAdjustedFloatLength(style, minimumValueForLength(style.perspectiveOriginX(), box.width())),
            valueForZoomAdjustedFloatLength(style, minimumValueForLength(style.perspectiveOriginY(), box.height()))
        );
    }
    return CSSValueList::createSpaceSeparated(
        ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, style.perspectiveOriginX()),
        ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, style.perspectiveOriginY())
    );
}

static bool rendererCanHaveTrimmedMargin(const RenderBox& renderer, MarginTrimType marginTrimType)
{
    // A renderer will have a specific margin marked as trimmed by setting its rare data bit if:
    // 1.) The layout system the box is in has this logic (setting the rare data bit for this
    // specific margin) implemented
    // 2.) The block container/flexbox/grid has this margin specified in its margin-trim style
    // If marginTrimType is empty we will check if any of the supported margins are in the style
    if (renderer.isFlexItem() || renderer.isGridItem())
        return renderer.parent()->style().marginTrim().contains(marginTrimType);

    // Even though margin-trim is not inherited, it is possible for nested block level boxes
    // to get placed at the block-start of an containing block ancestor which does have margin-trim.
    // In this case it is not enough to simply check the immediate containing block of the child. It is
    // also probably too expensive to perform an arbitrary walk up the tree to check for the existence
    // of an ancestor containing block with the property, so we will just return true and let
    // the rest of the logic in RenderBox::hasTrimmedMargin to determine if the rare data bit
    // were set at some point during layout
    if (renderer.isBlockLevelBox()) {
        auto containingBlock = renderer.containingBlock();
        return containingBlock && containingBlock->isHorizontalWritingMode();
    }
    return false;
}

static MarginTrimType toMarginTrimType(const RenderBox& renderer, PhysicalDirection direction)
{
    switch (mapSidePhysicalToLogical(formattingContextRootStyle(renderer).writingMode(), direction)) {
    case FlowRelativeDirection::BlockStart:
        return MarginTrimType::BlockStart;
    case FlowRelativeDirection::BlockEnd:
        return MarginTrimType::BlockEnd;
    case FlowRelativeDirection::InlineStart:
        return MarginTrimType::InlineStart;
    case FlowRelativeDirection::InlineEnd:
        return MarginTrimType::InlineEnd;
    default:
        ASSERT_NOT_REACHED();
        return MarginTrimType::BlockStart;
    }
}

static RefPtr<CSSValue> valueForMarginTop(const RenderStyle& style, RenderElement* renderer)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::BlockStart) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Top)))
        return valueForZoomAdjustedFloatLength(style, box->marginTop());
    return valueForZoomAdjustedMarginPixel<&RenderStyle::marginTop, &RenderBoxModelObject::marginTop>(style, renderer);
}

static RefPtr<CSSValue> valueForMarginRight(const RenderStyle& style, RenderElement* renderer)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::InlineEnd) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Right)))
        return valueForZoomAdjustedFloatLength(style, box->marginRight());

    auto& marginRight = style.marginRight();
    if (marginRight.isFixed() || !box)
        return ComputedStyleExtractor::valueForZoomAdjustedPixelLength(style, marginRight);

    float value;
    if (marginRight.isPercentOrCalculated()) {
        // RenderBox gives a marginRight() that is the distance between the right-edge of the child box
        // and the right-edge of the containing box, when display == DisplayType::Block. Let's calculate the absolute
        // value of the specified margin-right % instead of relying on RenderBox's marginRight() value.
        value = minimumValueForLength(marginRight, box->containingBlockLogicalWidthForContent());
    } else
        value = box->marginRight();
    return valueForZoomAdjustedFloatLength(style, value);
}

static RefPtr<CSSValue> valueForMarginBottom(const RenderStyle& style, RenderElement* renderer)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::BlockEnd) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Bottom)))
        return valueForZoomAdjustedFloatLength(style, box->marginBottom());
    return valueForZoomAdjustedMarginPixel<&RenderStyle::marginBottom, &RenderBoxModelObject::marginBottom>(style, renderer);
}

static RefPtr<CSSValue> valueForMarginLeft(const RenderStyle& style, RenderElement* renderer)
{
    CheckedPtr box = dynamicDowncast<RenderBox>(renderer);
    if (box && rendererCanHaveTrimmedMargin(*box, MarginTrimType::InlineStart) && box->hasTrimmedMargin(toMarginTrimType(*box, PhysicalDirection::Left)))
        return valueForZoomAdjustedFloatLength(style, box->marginLeft());
    return valueForZoomAdjustedMarginPixel<&RenderStyle::marginLeft, &RenderBoxModelObject::marginLeft>(style, renderer);
}

static RefPtr<CSSValue> valueForMarginTrim(const RenderStyle& style)
{
    auto marginTrim = style.marginTrim();
    if (marginTrim.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    // Try to serialize into one of the "block" or "inline" shorthands
    if (marginTrim.containsAll({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd }) && !marginTrim.containsAny({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd }))
        return CSSPrimitiveValue::create(CSSValueBlock);
    if (marginTrim.containsAll({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd }) && !marginTrim.containsAny({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd }))
        return CSSPrimitiveValue::create(CSSValueInline);
    if (marginTrim.containsAll({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd, MarginTrimType::InlineStart, MarginTrimType::InlineEnd })) {
        CSSValueListBuilder list;
        list.append(CSSPrimitiveValue::create(CSSValueBlock));
        list.append(CSSPrimitiveValue::create(CSSValueInline));
        return CSSValueList::createSpaceSeparated(WTFMove(list));
    }

    CSSValueListBuilder list;
    if (marginTrim.contains(MarginTrimType::BlockStart))
        list.append(CSSPrimitiveValue::create(CSSValueBlockStart));
    if (marginTrim.contains(MarginTrimType::InlineStart))
        list.append(CSSPrimitiveValue::create(CSSValueInlineStart));
    if (marginTrim.contains(MarginTrimType::BlockEnd))
        list.append(CSSPrimitiveValue::create(CSSValueBlockEnd));
    if (marginTrim.contains(MarginTrimType::InlineEnd))
        list.append(CSSPrimitiveValue::create(CSSValueInlineEnd));
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForShapeOutside(const RenderStyle& style)
{
    auto shapeValue = style.shapeOutside();
    if (!shapeValue)
        return CSSPrimitiveValue::create(CSSValueNone);

    if (shapeValue->type() == ShapeValue::Type::Box)
        return valueForConvertibleType(shapeValue->cssBox());

    if (shapeValue->type() == ShapeValue::Type::Image) {
        if (shapeValue->image())
            return shapeValue->image()->computedStyleValue(style);
        return CSSPrimitiveValue::create(CSSValueNone);
    }

    ASSERT(shapeValue->type() == ShapeValue::Type::Shape);

    if (shapeValue->cssBox() == CSSBoxType::BoxMissing)
        return CSSValueList::createSpaceSeparated(valueForBasicShape(style, *shapeValue->shape(), Style::PathConversion::None));
    return CSSValueList::createSpaceSeparated(valueForBasicShape(style, *shapeValue->shape(), Style::PathConversion::None),
        valueForConvertibleType(shapeValue->cssBox()));
}

static Ref<CSSValueList> valueForItemPositionWithOverflowAlignment(const StyleSelfAlignmentData& data)
{
    CSSValueListBuilder list;
    if (data.positionType() == ItemPositionType::Legacy)
        list.append(CSSPrimitiveValue::create(CSSValueLegacy));
    if (data.position() == ItemPosition::Baseline)
        list.append(CSSPrimitiveValue::create(CSSValueBaseline));
    else if (data.position() == ItemPosition::LastBaseline) {
        list.append(CSSPrimitiveValue::create(CSSValueLast));
        list.append(CSSPrimitiveValue::create(CSSValueBaseline));
    } else {
        if (data.position() >= ItemPosition::Center && data.overflow() != OverflowAlignment::Default)
            list.append(valueForConvertibleType(data.overflow()));
        if (data.position() == ItemPosition::Legacy)
            list.append(CSSPrimitiveValue::create(CSSValueNormal));
        else
            list.append(valueForConvertibleType(data.position()));
    }
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValueList> valueForContentPositionAndDistributionWithOverflowAlignment(const StyleContentAlignmentData& data)
{
    CSSValueListBuilder list;

    // Handle content-distribution values
    if (data.distribution() != ContentDistribution::Default)
        list.append(valueForConvertibleType(data.distribution()));

    // Handle content-position values (either as fallback or actual value)
    switch (data.position()) {
    case ContentPosition::Normal:
        // Handle 'normal' value, not valid as content-distribution fallback.
        if (data.distribution() == ContentDistribution::Default)
            list.append(CSSPrimitiveValue::create(CSSValueNormal));
        break;
    case ContentPosition::LastBaseline:
        list.append(CSSPrimitiveValue::create(CSSValueLast));
        list.append(CSSPrimitiveValue::create(CSSValueBaseline));
        break;
    default:
        // Handle overflow-alignment (only allowed for content-position values)
        if ((data.position() >= ContentPosition::Center || data.distribution() != ContentDistribution::Default) && data.overflow() != OverflowAlignment::Default)
            list.append(valueForConvertibleType(data.overflow()));
        list.append(valueForConvertibleType(data.position()));
    }

    ASSERT(list.size() > 0);
    ASSERT(list.size() <= 3);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSValueList> valueForOffsetRotate(const OffsetRotation& rotation)
{
    auto angle = CSSPrimitiveValue::create(rotation.angle(), CSSUnitType::CSS_DEG);
    if (rotation.hasAuto())
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueAuto), WTFMove(angle));
    return CSSValueList::createSpaceSeparated(WTFMove(angle));
}

static Ref<CSSValue> valueForOffsetShorthand(const RenderStyle& style)
{
    // [ <'offset-position'>? [ <'offset-path'> [ <'offset-distance'> || <'offset-rotate'> ]? ]? ]! [ / <'offset-anchor'> ]?

    // The first four elements are serialized in a space separated CSSValueList.
    // This is then combined with offset-anchor in a slash separated CSSValueList.

    CSSValueListBuilder innerList;

    if (!isAuto(style.offsetPosition()) && !isNormal(style.offsetPosition()))
        innerList.append(valueForPosition(style, style.offsetPosition()));

    bool nonInitialDistance = !style.offsetDistance().isZero();
    bool nonInitialRotate = style.offsetRotate() != style.initialOffsetRotate();

    if (style.offsetPath() || nonInitialDistance || nonInitialRotate)
        innerList.append(valueForPathOperation(style, style.offsetPath(), Style::PathConversion::ForceAbsolute));

    if (nonInitialDistance)
        innerList.append(CSSPrimitiveValue::create(style.offsetDistance(), style));
    if (nonInitialRotate)
        innerList.append(valueForOffsetRotate(style.offsetRotate()));

    auto inner = innerList.isEmpty()
        ? Ref<CSSValue> { CSSPrimitiveValue::create(CSSValueAuto) }
        : Ref<CSSValue> { CSSValueList::createSpaceSeparated(WTFMove(innerList)) };

    if (isAuto(style.offsetAnchor()))
        return inner;

    return CSSValueList::createSlashSeparated(WTFMove(inner), valueForPosition(style, style.offsetAnchor()));
}

static Ref<CSSValue> valueForPaintOrder(const RenderStyle& style)
{
    auto paintOrder = style.paintOrder();
    if (paintOrder == PaintOrder::Normal)
        return CSSPrimitiveValue::create(CSSValueNormal);

    CSSValueListBuilder paintOrderList;
    switch (paintOrder) {
    case PaintOrder::Normal:
        ASSERT_NOT_REACHED();
        break;
    case PaintOrder::Fill:
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueFill));
        break;
    case PaintOrder::FillMarkers:
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueFill));
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueMarkers));
        break;
    case PaintOrder::Stroke:
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueStroke));
        break;
    case PaintOrder::StrokeMarkers:
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueStroke));
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueMarkers));
        break;
    case PaintOrder::Markers:
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueMarkers));
        break;
    case PaintOrder::MarkersStroke:
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueMarkers));
        paintOrderList.append(CSSPrimitiveValue::create(CSSValueStroke));
        break;
    }
    return CSSValueList::createSpaceSeparated(WTFMove(paintOrderList));
}

static Ref<CSSValue> valueForScrollTimelineAxis(const Vector<ScrollAxis>& axes)
{
    if (axes.isEmpty())
        return CSSPrimitiveValue::create(CSSValueBlock);

    CSSValueListBuilder list;
    for (auto axis : axes)
        list.append(valueForConvertibleType(axis));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForScrollTimelineName(const Vector<AtomString>& names)
{
    if (names.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& name : names) {
        if (name.isNull())
            list.append(CSSPrimitiveValue::create(CSSValueNone));
        else
            list.append(CSSPrimitiveValue::createCustomIdent(name));
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForAnchorName(const RenderStyle& style)
{
    auto& scopedNames = style.anchorNames();
    if (scopedNames.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& scopedName : scopedNames)
        list.append(valueForScopedName(scopedName));

    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForPositionAnchor(const RenderStyle& style)
{
    if (!style.positionAnchor())
        return CSSPrimitiveValue::create(CSSValueAuto);
    return valueForScopedName(*style.positionAnchor());
}

static CSSValueID keywordForPositionAreaSpan(const PositionAreaSpan span)
{
    auto axis = span.axis();
    auto track = span.track();
    auto self = span.self();

    switch (axis) {
    case PositionAreaAxis::Horizontal:
        ASSERT(self == PositionAreaSelf::No);
        switch (track) {
        case PositionAreaTrack::Start:
            return CSSValueLeft;
        case PositionAreaTrack::SpanStart:
            return CSSValueSpanLeft;
        case PositionAreaTrack::End:
            return CSSValueRight;
        case PositionAreaTrack::SpanEnd:
            return CSSValueSpanRight;
        case PositionAreaTrack::Center:
            return CSSValueCenter;
        case PositionAreaTrack::SpanAll:
            return CSSValueSpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueLeft;
        }

    case PositionAreaAxis::Vertical:
        ASSERT(self == PositionAreaSelf::No);
        switch (track) {
        case PositionAreaTrack::Start:
            return CSSValueTop;
        case PositionAreaTrack::SpanStart:
            return CSSValueSpanTop;
        case PositionAreaTrack::End:
            return CSSValueBottom;
        case PositionAreaTrack::SpanEnd:
            return CSSValueSpanBottom;
        case PositionAreaTrack::Center:
            return CSSValueCenter;
        case PositionAreaTrack::SpanAll:
            return CSSValueSpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueTop;
        }

    case PositionAreaAxis::X:
        switch (track) {
        case PositionAreaTrack::Start:
            return self == PositionAreaSelf::No ? CSSValueXStart : CSSValueXSelfStart;
        case PositionAreaTrack::SpanStart:
            return self == PositionAreaSelf::No ? CSSValueSpanXStart : CSSValueSpanXSelfStart;
        case PositionAreaTrack::End:
            return self == PositionAreaSelf::No ? CSSValueXEnd : CSSValueXSelfEnd;
        case PositionAreaTrack::SpanEnd:
            return self == PositionAreaSelf::No ? CSSValueSpanXEnd : CSSValueSpanXSelfEnd;
        case PositionAreaTrack::Center:
            return CSSValueCenter;
        case PositionAreaTrack::SpanAll:
            return CSSValueSpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueXStart;
        }

    case PositionAreaAxis::Y:
        switch (track) {
        case PositionAreaTrack::Start:
            return self == PositionAreaSelf::No ? CSSValueYStart : CSSValueYSelfStart;
        case PositionAreaTrack::SpanStart:
            return self == PositionAreaSelf::No ? CSSValueSpanYStart : CSSValueSpanYSelfStart;
        case PositionAreaTrack::End:
            return self == PositionAreaSelf::No ? CSSValueYEnd : CSSValueYSelfEnd;
        case PositionAreaTrack::SpanEnd:
            return self == PositionAreaSelf::No ? CSSValueSpanYEnd : CSSValueSpanYSelfEnd;
        case PositionAreaTrack::Center:
            return CSSValueCenter;
        case PositionAreaTrack::SpanAll:
            return CSSValueSpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueYStart;
        }

    case PositionAreaAxis::Block:
        switch (track) {
        case PositionAreaTrack::Start:
            return self == PositionAreaSelf::No ? CSSValueBlockStart : CSSValueSelfBlockStart;
        case PositionAreaTrack::SpanStart:
            return self == PositionAreaSelf::No ? CSSValueSpanBlockStart : CSSValueSpanSelfBlockStart;
        case PositionAreaTrack::End:
            return self == PositionAreaSelf::No ? CSSValueBlockEnd : CSSValueSelfBlockEnd;
        case PositionAreaTrack::SpanEnd:
            return self == PositionAreaSelf::No ? CSSValueSpanBlockEnd : CSSValueSpanSelfBlockEnd;
        case PositionAreaTrack::Center:
            return CSSValueCenter;
        case PositionAreaTrack::SpanAll:
            return CSSValueSpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueBlockStart;
        }

    case PositionAreaAxis::Inline:
        switch (track) {
        case PositionAreaTrack::Start:
            return self == PositionAreaSelf::No ? CSSValueInlineStart : CSSValueSelfInlineStart;
        case PositionAreaTrack::SpanStart:
            return self == PositionAreaSelf::No ? CSSValueSpanInlineStart : CSSValueSpanSelfInlineStart;
        case PositionAreaTrack::End:
            return self == PositionAreaSelf::No ? CSSValueInlineEnd : CSSValueSelfInlineEnd;
        case PositionAreaTrack::SpanEnd:
            return self == PositionAreaSelf::No ? CSSValueSpanInlineEnd : CSSValueSpanSelfInlineEnd;
        case PositionAreaTrack::Center:
            return CSSValueCenter;
        case PositionAreaTrack::SpanAll:
            return CSSValueSpanAll;
        default:
            ASSERT_NOT_REACHED();
            return CSSValueInlineStart;
        }
    }

    ASSERT_NOT_REACHED();
    return CSSValueLeft;
}

static Ref<CSSValue> valueForPositionArea(const RenderStyle& style)
{
    auto positionArea = style.positionArea();
    if (!positionArea)
        return CSSPrimitiveValue::create(CSSValueNone);

    auto blockOrXAxisKeyword = keywordForPositionAreaSpan(positionArea->blockOrXAxis());
    auto inlineOrYAxisKeyword = keywordForPositionAreaSpan(positionArea->inlineOrYAxis());

    return CSSPropertyParserHelpers::valueForPositionArea(blockOrXAxisKeyword, inlineOrYAxisKeyword).releaseNonNull();
}

static Ref<CSSValue> valueForNameScope(const NameScope& scope)
{
    switch (scope.type) {
    case NameScope::Type::None:
        return CSSPrimitiveValue::create(CSSValueNone);

    case NameScope::Type::All:
        return CSSPrimitiveValue::create(CSSValueAll);

    case NameScope::Type::Ident:
        if (scope.names.isEmpty())
            return CSSPrimitiveValue::create(CSSValueNone);

        CSSValueListBuilder list;
        for (auto& name : scope.names) {
            ASSERT(!name.isNull());
            list.append(CSSPrimitiveValue::createCustomIdent(name));
        }

        return CSSValueList::createCommaSeparated(WTFMove(list));
    }

    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueNone);
}

static Ref<CSSValue> valueForScrollTimelineShorthandValue(const Vector<Ref<ScrollTimeline>>& timelines)
{
    if (timelines.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& timeline : timelines) {
        auto& name = timeline->name();
        auto axis = timeline->axis();

        ASSERT(!name.isNull());
        auto nameCSSValue = CSSPrimitiveValue::createCustomIdent(name);

        if (axis == ScrollAxis::Block)
            list.append(WTFMove(nameCSSValue));
        else
            list.append(CSSValuePair::createNoncoalescing(nameCSSValue, valueForConvertibleType(axis)));
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForSingleViewTimelineInset(const RenderStyle& style, const ViewTimelineInsets& insets)
{
    ASSERT(insets.start);
    if (insets.end && insets.start != insets.end)
        return CSSValuePair::createNoncoalescing(CSSPrimitiveValue::create(*insets.start, style), CSSPrimitiveValue::create(*insets.end, style));
    return CSSPrimitiveValue::create(*insets.start, style);
}

static Ref<CSSValue> valueForViewTimelineInset(const RenderStyle& style)
{
    auto& insets = style.viewTimelineInsets();
    if (insets.isEmpty())
        return CSSPrimitiveValue::create(CSSValueAuto);

    CSSValueListBuilder list;
    for (auto& singleInsets : insets)
        list.append(valueForSingleViewTimelineInset(style, singleInsets));
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForViewTimelineShorthand(const RenderStyle& style)
{
    auto& timelines = style.viewTimelines();
    if (timelines.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNone);

    CSSValueListBuilder list;
    for (auto& timeline : timelines) {
        auto& name = timeline->name();
        auto axis = timeline->axis();
        auto& insets = timeline->insets();

        auto hasDefaultAxis = axis == ScrollAxis::Block;
        auto hasDefaultInsets = [insets]() {
            if (!insets.start && !insets.end)
                return true;
            if (insets.start->isAuto())
                return true;
            return false;
        }();

        ASSERT(!name.isNull());
        auto nameCSSValue = CSSPrimitiveValue::createCustomIdent(name);

        if (hasDefaultAxis && hasDefaultInsets)
            list.append(WTFMove(nameCSSValue));
        else if (hasDefaultAxis)
            list.append(CSSValuePair::createNoncoalescing(nameCSSValue, valueForSingleViewTimelineInset(style, insets)));
        else if (hasDefaultInsets)
            list.append(CSSValuePair::createNoncoalescing(nameCSSValue, valueForConvertibleType(axis)));
        else {
            list.append(CSSValueList::createSpaceSeparated(
                WTFMove(nameCSSValue),
                valueForConvertibleType(axis),
                valueForSingleViewTimelineInset(style, insets)
            ));
        }
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

static Ref<CSSValue> valueForPositionVisibility(OptionSet<PositionVisibility> positionVisibility)
{
    CSSValueListBuilder list;
    if (positionVisibility & PositionVisibility::AnchorsValid)
        list.append(CSSPrimitiveValue::create(CSSValueAnchorsValid));
    if (positionVisibility & PositionVisibility::AnchorsVisible)
        list.append(CSSPrimitiveValue::create(CSSValueAnchorsVisible));
    if (positionVisibility & PositionVisibility::NoOverflow)
        list.append(CSSPrimitiveValue::create(CSSValueNoOverflow));

    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueAlways);

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

static Ref<CSSFontValue> valueForFontShorthand(const RenderStyle& style, ComputedStyleExtractor::PropertyValueType valueType, CSSValuePool& pool)
{
    auto& description = style.fontDescription();
    auto fontWidth = fontWidthKeyword(description.width());
    auto fontStyle = fontStyleKeyword(description.italic(), description.fontStyleAxis());

    auto propertiesResetByShorthandAreExpressible = [&] {
        // The font shorthand can express "font-variant-caps: small-caps". Overwrite with "normal" so we can use isAllNormal to check that all the other settings are normal.
        auto variantSettingsOmittingExpressible = description.variantSettings();
        if (variantSettingsOmittingExpressible.caps == FontVariantCaps::Small)
            variantSettingsOmittingExpressible.caps = FontVariantCaps::Normal;

        // When we add font-language-override, also add code to check for non-expressible values for it here.
        return variantSettingsOmittingExpressible.isAllNormal()
            && fontWidth
            && fontStyle
            && description.fontSizeAdjust().isNone()
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
        computedFont->weight = CSSPrimitiveValue::create(weight);
    if (*fontWidth != CSSValueNormal)
        computedFont->width = CSSPrimitiveValue::create(*fontWidth);
    if (*fontStyle != CSSValueNormal)
        computedFont->style = CSSPrimitiveValue::create(*fontStyle);
    computedFont->size = valueForFontSize(style);
    computedFont->lineHeight = valueForOptionalLineHeight(style, valueType);
    computedFont->family = valueForFontFamilyList(style, pool);

    return computedFont;
}

static Element* styleElementForNode(Node* node)
{
    if (!node)
        return nullptr;
    if (auto* element = dynamicDowncast<Element>(*node))
        return element;
    return composedTreeAncestors(*node).first();
}

ComputedStyleExtractor::ComputedStyleExtractor(Node* node, bool allowVisitedStyle, const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
    : ComputedStyleExtractor(styleElementForNode(node), allowVisitedStyle, pseudoElementIdentifier)
{
}

ComputedStyleExtractor::ComputedStyleExtractor(Node* node, bool allowVisitedStyle)
    : ComputedStyleExtractor(node, allowVisitedStyle, std::nullopt)
{
}

ComputedStyleExtractor::ComputedStyleExtractor(Element* element, bool allowVisitedStyle, const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
    : m_element(element)
    , m_pseudoElementIdentifier(pseudoElementIdentifier)
    , m_allowVisitedStyle(allowVisitedStyle)
{
}

ComputedStyleExtractor::ComputedStyleExtractor(Element* element, bool allowVisitedStyle)
    : ComputedStyleExtractor(element, allowVisitedStyle, std::nullopt)
{
}

RefPtr<CSSPrimitiveValue> ComputedStyleExtractor::getFontSizeCSSValuePreferringKeyword() const
{
    RefPtr element = m_element;
    if (!element)
        return nullptr;

    element->protectedDocument()->updateLayoutIgnorePendingStylesheets();

    auto* style = element->computedStyle(m_pseudoElementIdentifier);
    if (!style)
        return nullptr;

    if (auto sizeIdentifier = style->fontDescription().keywordSizeAsIdentifier())
        return CSSPrimitiveValue::create(sizeIdentifier);

    return valueForZoomAdjustedFloatLength(*style, style->fontDescription().computedSize());
}

bool ComputedStyleExtractor::useFixedFontDefaultSize() const
{
    RefPtr element = m_element;
    if (!element)
        return false;

    auto* style = element->computedStyle(m_pseudoElementIdentifier);
    if (!style)
        return false;

    return style->fontDescription().useFixedDefaultSize();
}

RenderElement* ComputedStyleExtractor::styledRenderer() const
{
    RefPtr element = m_element;
    if (!element)
        return nullptr;
    if (m_pseudoElementIdentifier)
        return Styleable(*element, m_pseudoElementIdentifier).renderer();
    if (element->hasDisplayContents())
        return nullptr;
    return element->renderer();
}

static inline bool hasValidStyleForProperty(Element& element, CSSPropertyID propertyID)
{
    if (element.styleValidity() != Style::Validity::Valid)
        return false;
    if (element.document().hasPendingFullStyleRebuild())
        return false;
    if (!element.document().childNeedsStyleRecalc())
        return true;

    if (auto* keyframeEffectStack = Styleable(element, { }).keyframeEffectStack()) {
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
    Ref document = element.document();

    document->styleScope().flushPendingUpdate();

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

    document->updateStyleIfNeeded();
    return true;
}

static inline const RenderStyle* computeRenderStyleForProperty(Element& element, const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier, CSSPropertyID propertyID, std::unique_ptr<RenderStyle>& ownedStyle, SingleThreadWeakPtr<RenderElement> renderer)
{
    if (!renderer)
        renderer = element.renderer();

    if (renderer && renderer->isComposited() && Style::Interpolation::isAccelerated(propertyID, element.document().settings())) {
        ownedStyle = renderer->animatedStyle();
        if (pseudoElementIdentifier) {
            // FIXME: This cached pseudo style will only exist if the animation has been run at least once.
            return ownedStyle->getCachedPseudoStyle(*pseudoElementIdentifier);
        }
        return ownedStyle.get();
    }

    return element.computedStyle(pseudoElementIdentifier);
}

RefPtr<CSSValue> ComputedStyleExtractor::customPropertyValue(const AtomString& propertyName) const
{
    RefPtr element = m_element;
    if (!element)
        return nullptr;

    updateStyleIfNeededForProperty(*element, CSSPropertyCustom);

    std::unique_ptr<RenderStyle> ownedStyle;
    auto* style = computeRenderStyleForProperty(*element, m_pseudoElementIdentifier, CSSPropertyCustom, ownedStyle, nullptr);
    if (!style)
        return nullptr;

    Ref document = element->document();

    if (document->hasStyleWithViewportUnits()) {
        if (RefPtr owner = document->ownerElement()) {
            owner->document().updateLayout();
            style = computeRenderStyleForProperty(*element, m_pseudoElementIdentifier, CSSPropertyCustom, ownedStyle, nullptr);
        }
    }

    return const_cast<CSSCustomPropertyValue*>(style->customPropertyValue(propertyName));
}

String ComputedStyleExtractor::customPropertyText(const AtomString& propertyName) const
{
    RefPtr propertyValue = customPropertyValue(propertyName);
    return propertyValue ? propertyValue->cssText(CSS::defaultSerializationContext()) : emptyString();
}

template<RenderStyleLengthGetter lengthGetter>
static bool paddingIsLayoutDependent(const RenderStyle* style, RenderObject* renderer)
{
    return renderer && style && renderer->isRenderBox() && !(style->*lengthGetter)().isFixed();
}

static bool isLayoutDependent(CSSPropertyID propertyID, const RenderStyle* style, RenderObject* renderer)
{
    auto mapLogicalToPhysicalPaddingProperty = [](auto direction, auto& renderer) -> CSSPropertyID {
        switch (mapSideLogicalToPhysical(formattingContextRootStyle(renderer).writingMode(), direction)) {
        case PhysicalDirection::Top:
            return CSSPropertyPaddingTop;
        case PhysicalDirection::Right:
            return CSSPropertyPaddingRight;
        case PhysicalDirection::Bottom:
            return CSSPropertyPaddingBottom;
        case PhysicalDirection::Left:
            return CSSPropertyPaddingLeft;
        default:
            ASSERT_NOT_REACHED();
            return CSSPropertyInvalid;
        }
    };

    switch (propertyID) {
    case CSSPropertyTop:
    case CSSPropertyBottom:
    case CSSPropertyLeft:
    case CSSPropertyRight:
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetBlockEnd:
    case CSSPropertyInsetInlineStart:
    case CSSPropertyInsetInlineEnd:
        return renderer && style && renderer->isRenderBox();
    case CSSPropertyWidth:
    case CSSPropertyHeight:
    case CSSPropertyInlineSize:
    case CSSPropertyBlockSize:
        return renderer && !renderer->isRenderOrLegacyRenderSVGModelObject() && !isNonReplacedInline(*renderer);
    case CSSPropertyMargin:
    case CSSPropertyMarginBlock:
    case CSSPropertyMarginBlockStart:
    case CSSPropertyMarginBlockEnd:
    case CSSPropertyMarginInline:
    case CSSPropertyMarginInlineStart:
    case CSSPropertyMarginInlineEnd:
    case CSSPropertyMarginTop:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
        return renderer && renderer->isRenderBox();
    case CSSPropertyPerspectiveOrigin:
    case CSSPropertyTransformOrigin:
    case CSSPropertyTransform:
    case CSSPropertyFilter: // Why are filters layout-dependent?
    case CSSPropertyBackdropFilter: // Why are backdrop-filters layout-dependent?
    case CSSPropertyWebkitBackdropFilter: // Why are backdrop-filters layout-dependent?
        return true;
    case CSSPropertyPadding:
        return isLayoutDependent(CSSPropertyPaddingBlock, style, renderer) || isLayoutDependent(CSSPropertyPaddingInline, style, renderer);
    case CSSPropertyPaddingBlock:
        return isLayoutDependent(CSSPropertyPaddingBlockStart, style, renderer) || isLayoutDependent(CSSPropertyPaddingBlockEnd, style, renderer);
    case CSSPropertyPaddingInline:
        return isLayoutDependent(CSSPropertyPaddingInlineStart, style, renderer) || isLayoutDependent(CSSPropertyPaddingInlineEnd, style, renderer);
    case CSSPropertyPaddingBlockStart:
        if (auto* renderBox = dynamicDowncast<RenderBox>(renderer))
            return isLayoutDependent(mapLogicalToPhysicalPaddingProperty(FlowRelativeDirection::BlockStart, *renderBox), style, renderBox);
        return false;
    case CSSPropertyPaddingBlockEnd:
        if (auto* renderBox = dynamicDowncast<RenderBox>(renderer))
            return isLayoutDependent(mapLogicalToPhysicalPaddingProperty(FlowRelativeDirection::BlockEnd, *renderBox), style, renderBox);
        return false;
    case CSSPropertyPaddingInlineStart:
        if (auto* renderBox = dynamicDowncast<RenderBox>(renderer))
            return isLayoutDependent(mapLogicalToPhysicalPaddingProperty(FlowRelativeDirection::InlineStart, *renderBox), style, renderBox);
        return false;
    case CSSPropertyPaddingInlineEnd:
        if (auto* renderBox = dynamicDowncast<RenderBox>(renderer))
            return isLayoutDependent(mapLogicalToPhysicalPaddingProperty(FlowRelativeDirection::InlineEnd, *renderBox), style, renderBox);
        return false;
    case CSSPropertyPaddingTop:
        return paddingIsLayoutDependent<&RenderStyle::paddingTop>(style, renderer);
    case CSSPropertyPaddingRight:
        return paddingIsLayoutDependent<&RenderStyle::paddingRight>(style, renderer);
    case CSSPropertyPaddingBottom:
        return paddingIsLayoutDependent<&RenderStyle::paddingBottom>(style, renderer);
    case CSSPropertyPaddingLeft:
        return paddingIsLayoutDependent<&RenderStyle::paddingLeft>(style, renderer);
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
    case CSSPropertyGridTemplate:
    case CSSPropertyGrid:
        return renderer && renderer->isRenderGrid();
    default:
        return false;
    }
}

RefPtr<CSSValue> ComputedStyleExtractor::propertyValue(CSSPropertyID propertyID, UpdateLayout updateLayout, PropertyValueType valueType) const
{
    RefPtr element = m_element.get();
    if (!element)
        return nullptr;

    if (!isExposed(propertyID, element->document().settings())) {
        // Exit quickly, and avoid us ever having to update layout in this case.
        return nullptr;
    }

    std::unique_ptr<RenderStyle> ownedStyle;
    const RenderStyle* style = nullptr;
    auto forcedLayout = ForcedLayout::No;

    if (updateLayout == UpdateLayout::Yes) {
        Ref document = element->document();

        updateStyleIfNeededForProperty(*element, propertyID);
        if (propertyID == CSSPropertyDisplay && !styledRenderer()) {
            RefPtr svgElement = dynamicDowncast<SVGElement>(*element);
            if (svgElement && !svgElement->isValid())
                return nullptr;
        }

        style = computeRenderStyleForProperty(*element, m_pseudoElementIdentifier, propertyID, ownedStyle, styledRenderer());

        forcedLayout = [&] {
            // FIXME: Some of these cases could be narrowed down or optimized better.
            if (isLayoutDependent(propertyID, style, styledRenderer()))
                return ForcedLayout::Yes;
            // FIXME: Why?
            if (element->isInShadowTree())
                return ForcedLayout::Yes;
            if (!document->ownerElement())
                return ForcedLayout::No;
            if (!document->styleScope().resolverIfExists())
                return ForcedLayout::No;
            if (auto& ruleSets = document->styleScope().resolverIfExists()->ruleSets(); ruleSets.hasViewportDependentMediaQueries() || ruleSets.hasContainerQueries())
                return ForcedLayout::Yes;
            // FIXME: Can we limit this to properties whose computed length value derived from a viewport unit?
            if (document->hasStyleWithViewportUnits())
                return ForcedLayout::ParentDocument;
            return ForcedLayout::No;
        }();

        if (forcedLayout == ForcedLayout::Yes)
            document->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, element.get());
        else if (forcedLayout == ForcedLayout::ParentDocument) {
            if (RefPtr owner = document->ownerElement())
                owner->protectedDocument()->updateLayout();
            else
                forcedLayout = ForcedLayout::No;
        }
    }

    if (updateLayout == UpdateLayout::No || forcedLayout != ForcedLayout::No)
        style = computeRenderStyleForProperty(*element, m_pseudoElementIdentifier, propertyID, ownedStyle, styledRenderer());

    if (!style)
        return nullptr;

    return valueForPropertyInStyle(*style, propertyID, CSSValuePool::singleton(), valueType == PropertyValueType::Resolved ? styledRenderer() : nullptr, valueType);
}

bool ComputedStyleExtractor::hasProperty(CSSPropertyID propertyID) const
{
    return propertyValue(propertyID);
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForPropertyInStyle(const RenderStyle& style, CSSPropertyID propertyID, CSSValuePool& cssValuePool, RenderElement* renderer, PropertyValueType valueType) const
{
    ASSERT(isExposed(propertyID, m_element->document().settings()));

    switch (propertyID) {
    case CSSPropertyInvalid:
        return nullptr;
    case CSSPropertyCustom:
        ASSERT_NOT_REACHED();
        return nullptr;

    case CSSPropertyAccentColor:
        return valueForAccentColor(style, cssValuePool);
    case CSSPropertyBackgroundColor:
        return valueForColor<CSSPropertyBackgroundColor, &RenderStyle::backgroundColor>(style, m_allowVisitedStyle, cssValuePool);
    case CSSPropertyBackgroundImage:
        return valueForBackgroundOrMaskImage(style, style.backgroundLayers());
    case CSSPropertyMaskImage:
        return valueForBackgroundOrMaskImage(style, style.maskLayers());
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitBackgroundSize:
        return valueForBackgroundSize(style, style.backgroundLayers());
    case CSSPropertyMaskSize:
        return valueForMaskSize(style, style.maskLayers());
    case CSSPropertyBackgroundRepeat:
        return valueForBackgroundOrMaskRepeat(style, style.backgroundLayers());
    case CSSPropertyMaskRepeat:
        return valueForBackgroundOrMaskRepeat(style, style.maskLayers());
    case CSSPropertyWebkitMaskSourceType:
        return valueForWebkitMaskSourceType(style, style.maskLayers());
    case CSSPropertyMaskMode:
        return valueForMaskMode(style, style.maskLayers());
    case CSSPropertyWebkitMaskComposite:
        return valueForWebkitMaskComposite(style, style.maskLayers());
    case CSSPropertyMaskComposite:
        return valueForMaskComposite(style, style.maskLayers());
    case CSSPropertyBackgroundAttachment:
        return valueForBackgroundAttachment(style, style.backgroundLayers());
    case CSSPropertyBackgroundBlendMode:
        return valueForBackgroundBlendMode(style, style.backgroundLayers());
    case CSSPropertyBackgroundClip:
    case CSSPropertyWebkitBackgroundClip:
        return valueForBackgroundOrMaskClip(style, style.backgroundLayers());
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyWebkitBackgroundOrigin:
        return valueForBackgroundOrMaskOrigin(style, style.backgroundLayers());
    case CSSPropertyMaskClip:
    case CSSPropertyWebkitMaskClip:
        return valueForBackgroundOrMaskClip(style, style.maskLayers());
    case CSSPropertyMaskOrigin:
        return valueForBackgroundOrMaskOrigin(style, style.maskLayers());
    case CSSPropertyBackgroundPosition:
        return valueForBackgroundOrMaskPosition(style, style.backgroundLayers());
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyMaskPosition:
        return valueForBackgroundOrMaskPosition(style, style.maskLayers());
    case CSSPropertyBackgroundPositionX:
        return valueForBackgroundOrMaskPositionX(style, style.backgroundLayers());
    case CSSPropertyWebkitMaskPositionX:
        return valueForBackgroundOrMaskPositionX(style, style.maskLayers());
    case CSSPropertyBackgroundPositionY:
        return valueForBackgroundOrMaskPositionY(style, style.backgroundLayers());
    case CSSPropertyWebkitMaskPositionY:
        return valueForBackgroundOrMaskPositionY(style, style.maskLayers());
    case CSSPropertyBlockEllipsis:
        return valueForBlockEllipsis(style);
    case CSSPropertyBlockStep:
        return valueForBlockStepShorthandValue(style);
    case CSSPropertyBlockStepAlign:
        return valueForConvertibleType(style.blockStepAlign());
    case CSSPropertyBlockStepInsert:
        return valueForConvertibleType(style.blockStepInsert());
    case CSSPropertyBlockStepRound:
        return valueForConvertibleType(style.blockStepRound());
    case CSSPropertyBlockStepSize:
        return valueForBlockStepSize(style);
    case CSSPropertyBorderCollapse:
        return valueForConvertibleType(style.borderCollapse());
    case CSSPropertyBorderSpacing:
        return valueForBorderSpacing(style);
    case CSSPropertyWebkitBorderHorizontalSpacing:
        return valueForZoomAdjustedFloatLength(style, style.horizontalBorderSpacing());
    case CSSPropertyWebkitBorderVerticalSpacing:
        return valueForZoomAdjustedFloatLength(style, style.verticalBorderSpacing());
    case CSSPropertyBorderImageSource:
        return valueForStyleImage(style, style.borderImageSource());
    case CSSPropertyBorderTopColor:
        return valueForColor<CSSPropertyBorderTopColor, &RenderStyle::borderTopColor>(style, m_allowVisitedStyle, cssValuePool);
    case CSSPropertyBorderRightColor:
        return valueForColor<CSSPropertyBorderRightColor, &RenderStyle::borderRightColor>(style, m_allowVisitedStyle, cssValuePool);
    case CSSPropertyBorderBottomColor:
        return valueForColor<CSSPropertyBorderBottomColor, &RenderStyle::borderBottomColor>(style, m_allowVisitedStyle, cssValuePool);
    case CSSPropertyBorderLeftColor:
        return valueForColor<CSSPropertyBorderLeftColor, &RenderStyle::borderLeftColor>(style, m_allowVisitedStyle, cssValuePool);
    case CSSPropertyBorderTopStyle:
        return valueForConvertibleType(style.borderTopStyle());
    case CSSPropertyBorderRightStyle:
        return valueForConvertibleType(style.borderRightStyle());
    case CSSPropertyBorderBottomStyle:
        return valueForConvertibleType(style.borderBottomStyle());
    case CSSPropertyBorderLeftStyle:
        return valueForConvertibleType(style.borderLeftStyle());
    case CSSPropertyBorderTopWidth:
        return valueForZoomAdjustedFloatLength(style, style.borderTopWidth());
    case CSSPropertyBorderRightWidth:
        return valueForZoomAdjustedFloatLength(style, style.borderRightWidth());
    case CSSPropertyBorderBottomWidth:
        return valueForZoomAdjustedFloatLength(style, style.borderBottomWidth());
    case CSSPropertyBorderLeftWidth:
        return valueForZoomAdjustedFloatLength(style, style.borderLeftWidth());
    case CSSPropertyBottom:
        return valueForInset(style, CSSPropertyBottom, renderer);
    case CSSPropertyWebkitBoxAlign:
        return valueForConvertibleType(style.boxAlign());
    case CSSPropertyWebkitBoxDecorationBreak:
        return valueForConvertibleType(style.boxDecorationBreak());
    case CSSPropertyWebkitBoxDirection:
        return valueForConvertibleType(style.boxDirection());
    case CSSPropertyWebkitBoxFlex:
        return valueForNumber(style.boxFlex());
    case CSSPropertyWebkitBoxFlexGroup:
        return valueForInteger(style.boxFlexGroup());
    case CSSPropertyWebkitBoxLines:
        return valueForConvertibleType(style.boxLines());
    case CSSPropertyWebkitBoxOrdinalGroup:
        return valueForInteger(style.boxOrdinalGroup());
    case CSSPropertyWebkitBoxOrient:
        return valueForConvertibleType(style.boxOrient());
    case CSSPropertyWebkitBoxPack:
        return valueForConvertibleType(style.boxPack());
    case CSSPropertyWebkitBoxReflect:
        return valueForWebkitBoxReflect(style, style.boxReflect());
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow:
        return valueForBoxShadow(style, style.boxShadow());
    case CSSPropertyCaptionSide:
        return valueForConvertibleType(style.captionSide());
    case CSSPropertyCaretColor:
        return valueForColor<CSSPropertyCaretColor, &RenderStyle::caretColor>(style, m_allowVisitedStyle, cssValuePool);
    case CSSPropertyClear:
        return valueForConvertibleType(style.clear());
    case CSSPropertyTextBoxTrim:
        return valueForConvertibleType(style.textBoxTrim());
    case CSSPropertyColor:
        return cssValuePool.createColorValue(m_allowVisitedStyle ? style.visitedDependentColor(CSSPropertyColor) : style.color());
    case CSSPropertyPrintColorAdjust:
        return valueForConvertibleType(style.printColorAdjust());
    case CSSPropertyWebkitColumnAxis:
        return valueForConvertibleType(style.columnAxis());
    case CSSPropertyColumnCount:
        return valueForKeywordOrNumber<&RenderStyle::hasAutoColumnCount, CSSValueAuto, &RenderStyle::columnCount>(style);
    case CSSPropertyColumnFill:
        return valueForConvertibleType(style.columnFill());
    case CSSPropertyColumnGap:
        return valueForGapLength(style, style.columnGap());
    case CSSPropertyRowGap:
        return valueForGapLength(style, style.rowGap());
    case CSSPropertyWebkitColumnProgression:
        return valueForConvertibleType(style.columnProgression());
    case CSSPropertyColumnRuleColor:
        // FIXME: Was using style.visitedDependentColor(CSSPropertyOutlineColor). Was this a bug?
        return valueForColor<CSSPropertyColumnRuleColor, &RenderStyle::columnRuleColor>(style, m_allowVisitedStyle, cssValuePool);
    case CSSPropertyColumnRuleStyle:
        return valueForConvertibleType(style.columnRuleStyle());
    case CSSPropertyColumnRuleWidth:
        return valueForZoomAdjustedFloatLength(style, style.columnRuleWidth());
    case CSSPropertyColumnSpan:
        return valueForConvertibleType(style.columnSpan());
    case CSSPropertyWebkitColumnBreakAfter:
        return valueForColumnBreak(style, style.breakAfter());
    case CSSPropertyWebkitColumnBreakBefore:
        return valueForColumnBreak(style, style.breakBefore());
    case CSSPropertyWebkitColumnBreakInside:
        return valueForColumnBreak(style, style.breakInside());
    case CSSPropertyColumnWidth:
        return valueForKeywordOrZoomAdjustedFloatLength<&RenderStyle::hasAutoColumnWidth, CSSValueAuto, &RenderStyle::columnWidth>(style);
    case CSSPropertyContinue:
        return valueForConvertibleType(style.overflowContinue());
    case CSSPropertyTabSize:
        return valueForTabSize(style);
    case CSSPropertyCursor:
        return valueForCursor(style);
#if ENABLE(CURSOR_VISIBILITY)
    case CSSPropertyWebkitCursorVisibility:
        return valueForConvertibleType(style.cursorVisibility());
#endif
    case CSSPropertyDirection:
        return valueForDirection(style, m_element);
    case CSSPropertyDisplay:
        return valueForConvertibleType(style.display());
    case CSSPropertyDynamicRangeLimit:
        return valueForDynamicRangeLimit(style);
    case CSSPropertyEmptyCells:
        return valueForConvertibleType(style.emptyCells());
    case CSSPropertyAlignContent:
        return valueForContentPositionAndDistributionWithOverflowAlignment(style.alignContent());
    case CSSPropertyAlignItems:
        return valueForItemPositionWithOverflowAlignment(style.alignItems());
    case CSSPropertyAlignSelf:
        return valueForItemPositionWithOverflowAlignment(style.alignSelf());
    case CSSPropertyFlex:
        return valueForShorthandProperties(flexShorthand());
    case CSSPropertyFlexBasis:
        return valueForLength(style, style.flexBasis());
    case CSSPropertyFlexDirection:
        return valueForConvertibleType(style.flexDirection());
    case CSSPropertyFlexFlow:
        return valueForFlexFlowShorthand(style);
    case CSSPropertyFlexGrow:
        return valueForNumber(style.flexGrow());
    case CSSPropertyFlexShrink:
        return valueForNumber(style.flexShrink());
    case CSSPropertyFlexWrap:
        return valueForConvertibleType(style.flexWrap());
    case CSSPropertyJustifyContent:
        return valueForContentPositionAndDistributionWithOverflowAlignment(style.justifyContent());
    case CSSPropertyJustifyItems:
        return valueForItemPositionWithOverflowAlignment(style.justifyItems());
    case CSSPropertyJustifySelf:
        return valueForItemPositionWithOverflowAlignment(style.justifySelf());
    case CSSPropertyPlaceContent:
        return valueFor2SidesShorthand(placeContentShorthand());
    case CSSPropertyPlaceItems:
        return valueFor2SidesShorthand(placeItemsShorthand());
    case CSSPropertyPlaceSelf:
        return valueFor2SidesShorthand(placeSelfShorthand());
    case CSSPropertyOrder:
        return valueForInteger(style.order());
    case CSSPropertyFloat:
        return valueForKeywordOrCSSValueID<&RenderStyle::hasOutOfFlowPosition, CSSValueNone, &RenderStyle::floating>(style);
    case CSSPropertyFieldSizing:
        return valueForConvertibleType(style.fieldSizing());
    case CSSPropertyFont:
        return valueForFontShorthand(style, valueType, cssValuePool);
    case CSSPropertyFontFamily:
        return valueForFontFamily(style, cssValuePool);
    case CSSPropertyFontSize:
        return valueForFontSize(style);
    case CSSPropertyFontSizeAdjust:
        return valueForFontSizeAdjust(style);
    case CSSPropertyFontStyle:
        return valueFromFontStyle(style);
    case CSSPropertyFontWidth:
        return valueForFontWidth(style);
    case CSSPropertyFontVariant:
        return valueForFontVariantShorthand();
    case CSSPropertyFontWeight:
        return valueForFontWeight(style);
    case CSSPropertyFontPalette:
        return valueForFontPalette(style);
    case CSSPropertyFontSynthesis:
        return valueForFontSynthesis(style);
    case CSSPropertyFontSynthesisWeight:
        return valueForFontSynthesisWeight(style);
    case CSSPropertyFontSynthesisStyle:
        return valueForFontSynthesisStyle(style);
    case CSSPropertyFontSynthesisSmallCaps:
        return valueForFontSynthesisSmallCaps(style);
    case CSSPropertyFontFeatureSettings:
        return valueForFontFeatureSettings(style);
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyFontVariationSettings:
        return valueForFontVariationSettings(style);
    case CSSPropertyFontOpticalSizing:
        return valueForConvertibleType(style.fontDescription().opticalSizing());
#endif
    case CSSPropertyGridAutoFlow:
        return valueForGridAutoFlow(style);
    case CSSPropertyGridAutoColumns:
        return valueForGridTrackSizeList(GridTrackSizingDirection::ForColumns, style);
    case CSSPropertyGridAutoRows:
        return valueForGridTrackSizeList(GridTrackSizingDirection::ForRows, style);
    case CSSPropertyGridTemplateColumns:
        return valueForGridTrackList(GridTrackSizingDirection::ForColumns, renderer, style);
    case CSSPropertyGridTemplateRows:
        return valueForGridTrackList(GridTrackSizingDirection::ForRows, renderer, style);
    case CSSPropertyGridColumnStart:
        return valueForGridPosition(style.gridItemColumnStart());
    case CSSPropertyGridColumnEnd:
        return valueForGridPosition(style.gridItemColumnEnd());
    case CSSPropertyGridRowStart:
        return valueForGridPosition(style.gridItemRowStart());
    case CSSPropertyGridRowEnd:
        return valueForGridPosition(style.gridItemRowEnd());
    case CSSPropertyGridArea:
        return valueForGridShorthand(gridAreaShorthand());
    case CSSPropertyGridTemplate:
        return valueForGridShorthand(gridTemplateShorthand());
    case CSSPropertyGrid:
        return valueForGridShorthand(gridShorthand());
    case CSSPropertyGridColumn:
        return valueForGridShorthand(gridColumnShorthand());
    case CSSPropertyGridRow:
        return valueForGridShorthand(gridRowShorthand());
    case CSSPropertyGridTemplateAreas:
        return valueForGridTemplateAreas(style);
    case CSSPropertyGap:
        return valueFor2SidesShorthand(gapShorthand());
    case CSSPropertyHeight:
        return valueForHeight(style, renderer);
    case CSSPropertyHyphens:
        return valueForConvertibleType(style.hyphens());
    case CSSPropertyHyphenateCharacter:
        return valueForAutoOrString(style, style.hyphenationString());
    case CSSPropertyWebkitHyphenateLimitAfter:
        return valueForWebkitHyphenateLimit<CSSValueAuto>(style, style.hyphenationLimitAfter());
    case CSSPropertyWebkitHyphenateLimitBefore:
        return valueForWebkitHyphenateLimit<CSSValueAuto>(style, style.hyphenationLimitBefore());
    case CSSPropertyWebkitHyphenateLimitLines:
        return valueForWebkitHyphenateLimit<CSSValueNoLimit>(style, style.hyphenationLimitLines());
    case CSSPropertyImageOrientation:
        return valueForImageOrientation(style);
    case CSSPropertyImageRendering:
        return valueForConvertibleType(style.imageRendering());
    case CSSPropertyInputSecurity:
        return valueForConvertibleType(style.inputSecurity());
    case CSSPropertyLeft:
        return valueForInset(style, CSSPropertyLeft, renderer);
    case CSSPropertyLetterSpacing:
        return valueForLetterSpacing(style);
    case CSSPropertyLineClamp:
        return valueForLineClampShorthand(style);
    case CSSPropertyWebkitLineClamp:
        return valueForWebkitLineClamp(style);
    case CSSPropertyLineHeight:
        return valueForLineHeight(style, valueType);
    case CSSPropertyListStyleImage:
        return valueForStyleImage(style, style.listStyleImage());
    case CSSPropertyListStylePosition:
        return valueForConvertibleType(style.listStylePosition());
    case CSSPropertyListStyleType:
        return valueForListStyleType(style);
    case CSSPropertyWebkitLocale:
        return valueForKeywordOrCustomIdent<CSSValueAuto>(style, style.specifiedLocale());
    case CSSPropertyMarginTop:
        return valueForMarginTop(style, renderer);
    case CSSPropertyMarginRight:
        return valueForMarginRight(style, renderer);
    case CSSPropertyMarginBottom:
        return valueForMarginBottom(style, renderer);
    case CSSPropertyMarginLeft:
        return valueForMarginLeft(style, renderer);
    case CSSPropertyMarginTrim:
        return valueForMarginTrim(style);
    case CSSPropertyWebkitUserModify:
        return valueForConvertibleType(style.userModify());
    case CSSPropertyMaxHeight:
        return valueForMaxSize(style, style.maxHeight());
    case CSSPropertyMaxWidth:
        return valueForMaxSize(style, style.maxWidth());
    case CSSPropertyMinHeight:
        return valueForMinSize(style, style.minHeight(), renderer);
    case CSSPropertyMinWidth:
        return valueForMinSize(style, style.minWidth(), renderer);
    case CSSPropertyObjectFit:
        return valueForConvertibleType(style.objectFit());
    case CSSPropertyObjectPosition:
        return valueForPosition(style, style.objectPosition());
    case CSSPropertyOffsetPath:
        // The computed value of offset-path must only contain absolute draw commands.
        // https://github.com/w3c/fxtf-drafts/issues/225#issuecomment-334322738
        return valueForPathOperation(style, style.offsetPath(), Style::PathConversion::ForceAbsolute);
    case CSSPropertyOffsetDistance:
        return valueForLength(style, style.offsetDistance());
    case CSSPropertyOffsetPosition:
        return valueForPositionOrAutoOrNormal(style, style.offsetPosition());
    case CSSPropertyOffsetAnchor:
        return valueForPositionOrAuto(style, style.offsetAnchor());
    case CSSPropertyOffsetRotate:
        return valueForOffsetRotate(style.offsetRotate());
    case CSSPropertyOffset:
        return valueForOffsetShorthand(style);
    case CSSPropertyOpacity:
        return valueForNumber(style.opacity());
    case CSSPropertyOrphans:
        return valueForKeywordOrInteger<&RenderStyle::hasAutoOrphans, CSSValueAuto, &RenderStyle::orphans>(style);
    case CSSPropertyOutlineColor:
        return valueForColor<CSSPropertyOutlineColor, &RenderStyle::outlineColor>(style, m_allowVisitedStyle, cssValuePool);
    case CSSPropertyOutlineOffset:
        return valueForZoomAdjustedFloatLength(style, style.outlineOffset());
    case CSSPropertyOutlineStyle:
        return valueForOutlineStyle(style);
    case CSSPropertyOutlineWidth:
        return valueForZoomAdjustedFloatLength(style, style.outlineWidth());
    case CSSPropertyOverflow:
        return valueFor2SidesShorthand(overflowShorthand());
    case CSSPropertyOverflowWrap:
        return valueForConvertibleType(style.overflowWrap());
    case CSSPropertyOverflowX:
        return valueForConvertibleType(style.overflowX());
    case CSSPropertyOverflowY:
        return valueForConvertibleType(style.overflowY());
    case CSSPropertyOverscrollBehavior:
        return valueForConvertibleType(std::max(style.overscrollBehaviorX(), style.overscrollBehaviorY()));
    case CSSPropertyOverscrollBehaviorX:
        return valueForConvertibleType(style.overscrollBehaviorX());
    case CSSPropertyOverscrollBehaviorY:
        return valueForConvertibleType(style.overscrollBehaviorY());
    case CSSPropertyPaddingTop:
        return valueForZoomAdjustedPaddingPixel<&RenderStyle::paddingTop, &RenderBoxModelObject::computedCSSPaddingTop>(style, renderer);
    case CSSPropertyPaddingRight:
        return valueForZoomAdjustedPaddingPixel<&RenderStyle::paddingRight, &RenderBoxModelObject::computedCSSPaddingRight>(style, renderer);
    case CSSPropertyPaddingBottom:
        return valueForZoomAdjustedPaddingPixel<&RenderStyle::paddingBottom, &RenderBoxModelObject::computedCSSPaddingBottom>(style, renderer);
    case CSSPropertyPaddingLeft:
        return valueForZoomAdjustedPaddingPixel<&RenderStyle::paddingLeft, &RenderBoxModelObject::computedCSSPaddingLeft>(style, renderer);
    case CSSPropertyPageBreakAfter:
        return valueForPageBreak(style, style.breakAfter());
    case CSSPropertyPageBreakBefore:
        return valueForPageBreak(style, style.breakBefore());
    case CSSPropertyPageBreakInside:
        return valueForPageBreak(style, style.breakInside());
    case CSSPropertyBreakAfter:
        return valueForConvertibleType(style.breakAfter());
    case CSSPropertyBreakBefore:
        return valueForConvertibleType(style.breakBefore());
    case CSSPropertyBreakInside:
        return valueForConvertibleType(style.breakInside());
    case CSSPropertyHangingPunctuation:
        return valueForHangingPunctuation(style.hangingPunctuation());
    case CSSPropertyPosition:
        return valueForConvertibleType(style.position());
    case CSSPropertyRight:
        return valueForInset(style, CSSPropertyRight, renderer);
    case CSSPropertyRubyPosition:
        return valueForConvertibleType(style.rubyPosition());
    case CSSPropertyWebkitRubyPosition:
        return valueForWebkitRubyPosition(style.rubyPosition());
    case CSSPropertyRubyAlign:
        return valueForConvertibleType(style.rubyAlign());
    case CSSPropertyRubyOverhang:
        return valueForConvertibleType(style.rubyOverhang());
    case CSSPropertyTableLayout:
        return valueForConvertibleType(style.tableLayout());
    case CSSPropertyTextAlign:
        return valueForConvertibleType(style.textAlign());
    case CSSPropertyTextAlignLast:
        return valueForConvertibleType(style.textAlignLast());
    case CSSPropertyTextDecoration:
        return valueForTextDecorationLine(style.textDecorationLine());
    case CSSPropertyTextJustify:
        return valueForConvertibleType(style.textJustify());
    case CSSPropertyWebkitTextDecoration:
        return valueForShorthandProperties(webkitTextDecorationShorthand());
    case CSSPropertyTextDecorationLine:
        return valueForTextDecorationLine(style.textDecorationLine());
    case CSSPropertyTextDecorationStyle:
        return valueForConvertibleType(style.textDecorationStyle());
    case CSSPropertyTextDecorationColor:
        return valueForColor(style, cssValuePool, style.textDecorationColor());
    case CSSPropertyTextDecorationSkip:
        return valueForTextDecorationSkipShorthand(style.textDecorationSkipInk());
    case CSSPropertyTextDecorationSkipInk:
        return valueForConvertibleType(style.textDecorationSkipInk());
    case CSSPropertyTextUnderlinePosition:
        return valueForTextUnderlinePosition(style.textUnderlinePosition());
    case CSSPropertyTextUnderlineOffset:
        return valueForTextUnderlineOffset(style, style.textUnderlineOffset());
    case CSSPropertyTextDecorationThickness:
        return valueForTextDecorationThickness(style, style.textDecorationThickness());
    case CSSPropertyWebkitTextDecorationsInEffect:
        return valueForTextDecorationLine(style.textDecorationLineInEffect());
    case CSSPropertyWebkitTextFillColor:
        return valueForColor(style, cssValuePool, style.textFillColor());
    case CSSPropertyTextEmphasisColor:
        return valueForColor(style, cssValuePool, style.textEmphasisColor());
    case CSSPropertyTextEmphasisPosition:
        return valueForTextEmphasisPosition(style.textEmphasisPosition());
    case CSSPropertyTextEmphasisStyle:
        return valueForTextEmphasisStyle(style);
    case CSSPropertyTextEmphasis:
        return valueForTextEmphasisShorthand(style, cssValuePool);
    case CSSPropertyTextGroupAlign:
        return valueForConvertibleType(style.textGroupAlign());
    case CSSPropertyTextIndent:
        return valueForTextIndent(style);
    case CSSPropertyTextShadow:
        return valueForTextShadow(style, style.textShadow());
    case CSSPropertyTextSpacingTrim:
        return valueForTextSpacingTrim(style);
    case CSSPropertyTextAutospace:
        return valueForTextAutospace(style);
    case CSSPropertyTextRendering:
        return valueForConvertibleType(style.fontDescription().textRenderingMode());
    case CSSPropertyTextOverflow:
        return valueForConvertibleType(style.textOverflow());
    case CSSPropertyWebkitTextSecurity:
        return valueForConvertibleType(style.textSecurity());
#if ENABLE(TEXT_AUTOSIZING)
    case CSSPropertyWebkitTextSizeAdjust:
        return valueForWebkitTextSizeAdjust(style);
#endif
    case CSSPropertyWebkitTextStrokeColor:
        return valueForColor(style, cssValuePool, style.textStrokeColor());
    case CSSPropertyWebkitTextStrokeWidth:
        return valueForZoomAdjustedFloatLength(style, style.textStrokeWidth());
    case CSSPropertyTextBox:
        return valueForTextBoxShorthand(style);
    case CSSPropertyTextTransform:
        return valueForTextTransform(style);
    case CSSPropertyTextWrap:
        return valueForTextWrapShorthand(style);
    case CSSPropertyTextWrapMode:
        return valueForConvertibleType(style.textWrapMode());
    case CSSPropertyTextWrapStyle:
        return valueForConvertibleType(style.textWrapStyle());
    case CSSPropertyTop:
        return valueForInset(style, CSSPropertyTop, renderer);
    case CSSPropertyUnicodeBidi:
        return valueForConvertibleType(style.unicodeBidi());
    case CSSPropertyVerticalAlign:
        return valueForVerticalAlign(style);
    case CSSPropertyViewTransitionClass:
        return valueForViewTransitionClass(style);
    case CSSPropertyViewTransitionName:
        return valueForViewTransitionName(style);
    case CSSPropertyVisibility:
        return valueForConvertibleType(style.visibility());
    case CSSPropertyWhiteSpace:
        return valueForWhiteSpaceShorthand(style);
    case CSSPropertyWhiteSpaceCollapse:
        return valueForConvertibleType(style.whiteSpaceCollapse());
    case CSSPropertyWidows:
        return valueForKeywordOrInteger<&RenderStyle::hasAutoWidows, CSSValueAuto, &RenderStyle::widows>(style);
    case CSSPropertyWidth:
        return valueForWidth(style, renderer);
    case CSSPropertyWillChange:
        return valueForWillChange(style);
    case CSSPropertyWordBreak:
        return valueForConvertibleType(style.wordBreak());
    case CSSPropertyWordSpacing:
        return valueForWordSpacing(style);
    case CSSPropertyLineBreak:
        return valueForConvertibleType(style.lineBreak());
    case CSSPropertyWebkitNbspMode:
        return valueForConvertibleType(style.nbspMode());
    case CSSPropertyResize:
        return valueForConvertibleType(style.resize());
    case CSSPropertyFontKerning:
        return valueForConvertibleType(style.fontDescription().kerning());
    case CSSPropertyWebkitFontSmoothing:
        return valueForConvertibleType(style.fontDescription().fontSmoothing());
    case CSSPropertyFontVariantLigatures:
        return valueForFontVariantLigaturesPropertyValue(style.fontDescription().variantCommonLigatures(), style.fontDescription().variantDiscretionaryLigatures(), style.fontDescription().variantHistoricalLigatures(), style.fontDescription().variantContextualAlternates());
    case CSSPropertyFontVariantPosition:
        return valueForConvertibleType(style.fontDescription().variantPosition());
    case CSSPropertyFontVariantCaps:
        return valueForConvertibleType(style.fontDescription().variantCaps());
    case CSSPropertyFontVariantNumeric:
        return valueForFontVariantNumericPropertyValue(style.fontDescription().variantNumericFigure(), style.fontDescription().variantNumericSpacing(), style.fontDescription().variantNumericFraction(), style.fontDescription().variantNumericOrdinal(), style.fontDescription().variantNumericSlashedZero());
    case CSSPropertyFontVariantAlternates:
        return valueForFontVariantAlternatesPropertyValue(style.fontDescription().variantAlternates());
    case CSSPropertyFontVariantEastAsian:
        return valueForFontVariantEastAsianPropertyValue(style.fontDescription().variantEastAsianVariant(), style.fontDescription().variantEastAsianWidth(), style.fontDescription().variantEastAsianRuby());
    case CSSPropertyFontVariantEmoji:
        return valueForConvertibleType(style.fontDescription().variantEmoji());
    case CSSPropertyZIndex:
        return valueForKeywordOrInteger<&RenderStyle::hasAutoSpecifiedZIndex, CSSValueAuto, &RenderStyle::specifiedZIndex>(style);
    case CSSPropertyZoom:
        return valueForNumber(style.zoom());
    case CSSPropertyBoxSizing:
        return valueForBoxSizing(style);
    case CSSPropertyAnimation:
        return valueForAnimationShorthand(style, style.animations());
    case CSSPropertyAnimationComposition:
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyAnimationRangeEnd:
    case CSSPropertyAnimationRangeStart:
    case CSSPropertyAnimationRange:
    case CSSPropertyAnimationTimeline:
    case CSSPropertyAnimationTimingFunction:
        return valueForAnimationOrTransition(style, propertyID, style.animations());
    case CSSPropertyAppearance:
        return valueForConvertibleType(style.appearance());
    case CSSPropertyAspectRatio:
        return valueForAspectRatio(style);
    case CSSPropertyContain:
        return valueForContain(style);
    case CSSPropertyContainer:
        return valueForContainerShorthand(style);
    case CSSPropertyContainerType:
        return valueForConvertibleType(style.containerType());
    case CSSPropertyContainerName:
        return valueForContainerName(style);
    case CSSPropertyContainIntrinsicSize:
        return valueFor2SidesShorthand(containIntrinsicSizeShorthand());
    case CSSPropertyContainIntrinsicWidth:
        return valueForContainIntrinsicSize(style, style.containIntrinsicWidthType(), style.containIntrinsicWidth());
    case CSSPropertyContainIntrinsicHeight:
        return valueForContainIntrinsicSize(style, style.containIntrinsicHeightType(), style.containIntrinsicHeight());
    case CSSPropertyContentVisibility:
        return valueForConvertibleType(style.contentVisibility());
    case CSSPropertyBackfaceVisibility:
        return valueForConvertibleType(style.backfaceVisibility());
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
        return valueForBorderImageWidth(style);
    case CSSPropertyWebkitMaskBoxImage:
    case CSSPropertyMaskBorder:
        return valueForNinePieceImage(propertyID, style.maskBorder(), style);
    case CSSPropertyMaskBorderOutset:
        return valueForNinePieceImageQuad(style.maskBorder().outset(), style);
    case CSSPropertyMaskBorderRepeat:
        return valueForNinePieceImageRepeat(style.maskBorder());
    case CSSPropertyMaskBorderSlice:
        return valueForNinePieceImageSlice(style.maskBorder());
    case CSSPropertyMaskBorderWidth:
        return valueForNinePieceImageQuad(style.maskBorder().borderSlices(), style);
    case CSSPropertyMaskBorderSource:
        return valueForStyleImage(style, style.maskBorderSource());
    case CSSPropertyMaxLines:
        return valueForMaxLines(style);
    case CSSPropertyWebkitInitialLetter:
        return valueForWebkitInitialLetter(style);
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    case CSSPropertyWebkitOverflowScrolling:
        return valueForWebkitOverflowScrolling(style);
#endif
    case CSSPropertyScrollBehavior:
        return valueForScrollBehavior(style);
    case CSSPropertyPerspective:
    case CSSPropertyWebkitPerspective:
        return valueForKeywordIfNegatedOrZoomAdjustedFloatLength<&RenderStyle::hasPerspective, CSSValueNone, &RenderStyle::perspective>(style);
    case CSSPropertyPerspectiveOrigin:
        return valueForPerspectiveOrigin(style, renderer);
    case CSSPropertyWebkitRtlOrdering:
        return valueForConvertibleType(style.rtlOrdering());
#if ENABLE(TOUCH_EVENTS)
    case CSSPropertyWebkitTapHighlightColor:
        return valueForColor(style, cssValuePool, style.tapHighlightColor());
#endif
    case CSSPropertyTouchAction:
        return valueForTouchAction(style);
#if PLATFORM(IOS_FAMILY)
    case CSSPropertyWebkitTouchCallout:
        return valueForWebkitTouchCallout(style);
#endif
    case CSSPropertyWebkitUserDrag:
        return valueForConvertibleType(style.userDrag());
    case CSSPropertyWebkitUserSelect:
        return valueForConvertibleType(style.userSelect());
    case CSSPropertyBorderBottomLeftRadius:
        return valueForBorderRadiusCornerValue(style, style.borderBottomLeftRadius());
    case CSSPropertyBorderBottomRightRadius:
        return valueForBorderRadiusCornerValue(style, style.borderBottomRightRadius());
    case CSSPropertyBorderTopLeftRadius:
        return valueForBorderRadiusCornerValue(style, style.borderTopLeftRadius());
    case CSSPropertyBorderTopRightRadius:
        return valueForBorderRadiusCornerValue(style, style.borderTopRightRadius());
    case CSSPropertyClip:
        return valueForClip(style);
    case CSSPropertySpeakAs:
        return valueForSpeakAs(style);
    case CSSPropertyTransform:
        return valueForTransform(style, renderer, valueType);
    case CSSPropertyTransformBox:
        return valueForConvertibleType(style.transformBox());
    case CSSPropertyTransformOrigin:
        return valueForTransformOrigin(style, renderer);
    case CSSPropertyTransformStyle:
        return valueForConvertibleType(style.transformStyle3D());
    case CSSPropertyTranslate:
        return valueForTranslate(style, renderer);
    case CSSPropertyScale:
        return valueForScale(style, renderer);
    case CSSPropertyRotate:
        return valueForRotate(style, renderer);
    case CSSPropertyTransitionBehavior:
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionTimingFunction:
    case CSSPropertyTransitionProperty:
        return valueForAnimationOrTransition(style, propertyID, style.transitions());
    case CSSPropertyTransition:
        return valueForTransitionShorthand(style);
    case CSSPropertyPointerEvents:
        return valueForConvertibleType(style.pointerEvents());
    case CSSPropertyWebkitLineGrid:
        return valueForKeywordOrCustomIdent<CSSValueNone>(style, style.lineGrid());
    case CSSPropertyWebkitLineSnap:
        return valueForConvertibleType(style.lineSnap());
    case CSSPropertyWebkitLineAlign:
        return valueForConvertibleType(style.lineAlign());
    case CSSPropertyWritingMode:
        return valueForWritingMode(style, m_element);
    case CSSPropertyWebkitTextCombine:
        return valueForWebkitTextCombine(style);
    case CSSPropertyTextCombineUpright:
        return valueForConvertibleType(style.textCombine());
    case CSSPropertyWebkitTextOrientation:
        return valueForConvertibleType(style.writingMode().computedTextOrientation());
    case CSSPropertyTextOrientation:
        return valueForConvertibleType(style.writingMode().computedTextOrientation());
    case CSSPropertyWebkitLineBoxContain:
        return valueForWebkitLineBoxContain(style);
    case CSSPropertyContent:
        return valueForContent(style);
    case CSSPropertyCounterIncrement:
        return valueForCounter(style, propertyID);
    case CSSPropertyCounterReset:
        return valueForCounter(style, propertyID);
    case CSSPropertyCounterSet:
        return valueForCounter(style, propertyID);
    case CSSPropertyClipPath:
        return valueForPathOperation(style, style.clipPath());
    case CSSPropertyShapeMargin:
        return valueForLength(style, style.shapeMargin());
    case CSSPropertyShapeImageThreshold:
        return valueForNumber(style.shapeImageThreshold());
    case CSSPropertyShapeOutside:
        return valueForShapeOutside(style);
    case CSSPropertyFilter:
        return valueForFilter(style, style.filter());
    case CSSPropertyAppleColorFilter:
        return valueForAppleColorFilter(style, style.appleColorFilter());
    case CSSPropertyWebkitBackdropFilter:
    case CSSPropertyBackdropFilter:
        return valueForFilter(style, style.backdropFilter());
    case CSSPropertyMathStyle:
        return valueForConvertibleType(style.mathStyle());
    case CSSPropertyMixBlendMode:
        return valueForConvertibleType(style.blendMode());
    case CSSPropertyIsolation:
        return valueForConvertibleType(style.isolation());
    case CSSPropertyBackground:
        return valueForBackgroundShorthand();
    case CSSPropertyMask:
        return valueForMaskShorthand();
    case CSSPropertyBorder:
        return valueForBorderShorthand();
    case CSSPropertyBorderBlock:
        return valueForBorderBlockShorthand();
    case CSSPropertyBorderBlockColor:
        return valueFor2SidesShorthand(borderBlockColorShorthand());
    case CSSPropertyBorderBlockEnd:
        return valueForShorthandProperties(borderBlockEndShorthand());
    case CSSPropertyBorderBlockStart:
        return valueForShorthandProperties(borderBlockStartShorthand());
    case CSSPropertyBorderBlockStyle:
        return valueFor2SidesShorthand(borderBlockStyleShorthand());
    case CSSPropertyBorderBlockWidth:
        return valueFor2SidesShorthand(borderBlockWidthShorthand());
    case CSSPropertyBorderBottom:
        return valueForShorthandProperties(borderBottomShorthand());
    case CSSPropertyBorderColor:
        return valueFor4SidesShorthand(borderColorShorthand());
    case CSSPropertyBorderLeft:
        return valueForShorthandProperties(borderLeftShorthand());
    case CSSPropertyBorderInline:
        return valueForBorderInlineShorthand();
    case CSSPropertyBorderInlineColor:
        return valueFor2SidesShorthand(borderInlineColorShorthand());
    case CSSPropertyBorderInlineEnd:
        return valueForShorthandProperties(borderInlineEndShorthand());
    case CSSPropertyBorderInlineStart:
        return valueForShorthandProperties(borderInlineStartShorthand());
    case CSSPropertyBorderInlineStyle:
        return valueFor2SidesShorthand(borderInlineStyleShorthand());
    case CSSPropertyBorderInlineWidth:
        return valueFor2SidesShorthand(borderInlineWidthShorthand());
    case CSSPropertyBorderRadius:
    case CSSPropertyWebkitBorderRadius:
        return valueForBorderRadiusShorthand(style, propertyID);
    case CSSPropertyBorderRight:
        return valueForShorthandProperties(borderRightShorthand());
    case CSSPropertyBorderStyle:
        return valueFor4SidesShorthand(borderStyleShorthand());
    case CSSPropertyBorderTop:
        return valueForShorthandProperties(borderTopShorthand());
    case CSSPropertyBorderWidth:
        return valueFor4SidesShorthand(borderWidthShorthand());
    case CSSPropertyColumnRule:
        return valueForShorthandProperties(columnRuleShorthand());
    case CSSPropertyColumns:
        return valueForColumnsShorthand(style);
    case CSSPropertyCornerShape:
        return valueFor4SidesShorthand(cornerShapeShorthand());
    case CSSPropertyCornerTopLeftShape:
        return valueForCornerShape(style, style.cornerTopLeftShape());
    case CSSPropertyCornerTopRightShape:
        return valueForCornerShape(style, style.cornerTopRightShape());
    case CSSPropertyCornerBottomRightShape:
        return valueForCornerShape(style, style.cornerBottomRightShape());
    case CSSPropertyCornerBottomLeftShape:
        return valueForCornerShape(style, style.cornerBottomLeftShape());
    case CSSPropertyInset:
        return valueFor4SidesShorthand(insetShorthand());
    case CSSPropertyInsetBlock:
        return valueFor2SidesShorthand(insetBlockShorthand());
    case CSSPropertyInsetInline:
        return valueFor2SidesShorthand(insetInlineShorthand());
    case CSSPropertyListStyle:
        return valueForShorthandProperties(listStyleShorthand());
    case CSSPropertyMargin:
        return valueFor4SidesShorthand(marginShorthand());
    case CSSPropertyMarginBlock:
        return valueFor2SidesShorthand(marginBlockShorthand());
    case CSSPropertyMarginInline:
        return valueFor2SidesShorthand(marginInlineShorthand());
    case CSSPropertyOutline:
        return valueForShorthandProperties(outlineShorthand());
    case CSSPropertyPadding:
        return valueFor4SidesShorthand(paddingShorthand());
    case CSSPropertyPaddingBlock:
        return valueFor2SidesShorthand(paddingBlockShorthand());
    case CSSPropertyPaddingInline:
        return valueFor2SidesShorthand(paddingInlineShorthand());
    case CSSPropertyScrollMargin:
        return valueFor4SidesShorthand(scrollMarginShorthand());
    case CSSPropertyScrollMarginBottom:
        return valueForScrollMarginEdge(style, style.scrollMarginBottom());
    case CSSPropertyScrollMarginTop:
        return valueForScrollMarginEdge(style, style.scrollMarginTop());
    case CSSPropertyScrollMarginRight:
        return valueForScrollMarginEdge(style, style.scrollMarginRight());
    case CSSPropertyScrollMarginLeft:
        return valueForScrollMarginEdge(style, style.scrollMarginLeft());
    case CSSPropertyScrollMarginBlock:
        return valueFor2SidesShorthand(scrollMarginBlockShorthand());
    case CSSPropertyScrollMarginInline:
        return valueFor2SidesShorthand(scrollMarginInlineShorthand());
    case CSSPropertyScrollPadding:
        return valueFor4SidesShorthand(scrollPaddingShorthand());
    case CSSPropertyScrollPaddingBottom:
        return valueForScrollPaddingEdge(style, style.scrollPaddingBottom());
    case CSSPropertyScrollPaddingTop:
        return valueForScrollPaddingEdge(style, style.scrollPaddingTop());
    case CSSPropertyScrollPaddingRight:
        return valueForScrollPaddingEdge(style, style.scrollPaddingRight());
    case CSSPropertyScrollPaddingLeft:
        return valueForScrollPaddingEdge(style, style.scrollPaddingLeft());
    case CSSPropertyScrollPaddingBlock:
        return valueFor2SidesShorthand(scrollPaddingBlockShorthand());
    case CSSPropertyScrollPaddingInline:
        return valueFor2SidesShorthand(scrollPaddingInlineShorthand());
    case CSSPropertyScrollSnapAlign:
        return valueForScrollSnapAlignment(style.scrollSnapAlign());
    case CSSPropertyScrollSnapStop:
        return valueForConvertibleType(style.scrollSnapStop());
    case CSSPropertyScrollSnapType:
        return valueForScrollSnapType(style.scrollSnapType());
    case CSSPropertyScrollTimelineAxis:
        return valueForScrollTimelineAxis(style.scrollTimelineAxes());
    case CSSPropertyScrollTimelineName:
        return valueForScrollTimelineName(style.scrollTimelineNames());
    case CSSPropertyScrollTimeline:
        return valueForScrollTimelineShorthandValue(style.scrollTimelines());
    case CSSPropertyViewTimelineAxis:
        return valueForScrollTimelineAxis(style.viewTimelineAxes());
    case CSSPropertyViewTimelineInset:
        return valueForViewTimelineInset(style);
    case CSSPropertyViewTimelineName:
        return valueForScrollTimelineName(style.viewTimelineNames());
    case CSSPropertyViewTimeline:
        return valueForViewTimelineShorthand(style);
    case CSSPropertyScrollbarColor:
        return valueForScrollbarColor(style, cssValuePool);
    case CSSPropertyScrollbarGutter:
        return valueForScrollbarGutter(style.scrollbarGutter());
    case CSSPropertyScrollbarWidth:
        return valueForConvertibleType(style.scrollbarWidth());
    case CSSPropertyOverflowAnchor:
        return valueForConvertibleType(style.overflowAnchor());
    case CSSPropertyTextBoxEdge:
        return valueForTextEdge(propertyID, style.textBoxEdge());
    case CSSPropertyLineFitEdge:
        return valueForTextEdge(propertyID, style.lineFitEdge());
#if ENABLE(APPLE_PAY)
    case CSSPropertyApplePayButtonStyle:
        return valueForConvertibleType(style.applePayButtonStyle());
    case CSSPropertyApplePayButtonType:
        return valueForConvertibleType(style.applePayButtonType());
#endif
#if HAVE(CORE_MATERIAL)
    case CSSPropertyAppleVisualEffect:
        return valueForConvertibleType(style.appleVisualEffect());
#endif
#if ENABLE(DARK_MODE_CSS)
    case CSSPropertyColorScheme:
        return valueForColorScheme(style);
#endif
    case CSSPropertyWebkitTextZoom:
        return valueForConvertibleType(style.textZoom());
    case CSSPropertyD:
        return valueForD(style);
    case CSSPropertyPaintOrder:
        return valueForPaintOrder(style);
    case CSSPropertyStrokeLinecap:
        return valueForConvertibleType(style.capStyle());
    case CSSPropertyStrokeLinejoin:
        return valueForConvertibleType(style.joinStyle());
    case CSSPropertyStrokeWidth:
        return valueForZoomAdjustedPixelLength(style, style.strokeWidth());
    case CSSPropertyStrokeColor:
        return valueForColor(style, cssValuePool, style.strokeColor());
    case CSSPropertyStrokeMiterlimit:
        return valueForNumber(style.strokeMiterLimit());
    case CSSPropertyQuotes:
        return valueForQuotes(style);
    case CSSPropertyAnchorName:
        return valueForAnchorName(style);
    case CSSPropertyAnchorScope:
        return valueForNameScope(style.anchorScope());
    case CSSPropertyPositionAnchor:
        return valueForPositionAnchor(style);
    case CSSPropertyPositionArea:
        return valueForPositionArea(style);
    case CSSPropertyPositionTry:
        return valueForPositionTryShorthand(style);
    case CSSPropertyPositionTryFallbacks:
        return valueForPositionTryFallbacks(style.positionTryFallbacks());
    case CSSPropertyPositionTryOrder:
        return valueForConvertibleType(style.positionTryOrder());
    case CSSPropertyPositionVisibility:
        return valueForPositionVisibility(style.positionVisibility());
    case CSSPropertyTimelineScope:
        return valueForNameScope(style.timelineScope());
    case CSSPropertyCx:
        return valueForZoomAdjustedPixelLength(style, style.svgStyle().cx());
    case CSSPropertyCy:
        return valueForZoomAdjustedPixelLength(style, style.svgStyle().cy());
    case CSSPropertyR:
        return valueForZoomAdjustedPixelLength(style, style.svgStyle().r());
    case CSSPropertyRx:
        return valueForZoomAdjustedPixelLength(style, style.svgStyle().rx());
    case CSSPropertyRy:
        return valueForZoomAdjustedPixelLength(style, style.svgStyle().ry());
    case CSSPropertyStrokeDashoffset:
        return valueForZoomAdjustedPixelLength(style, style.svgStyle().strokeDashOffset());
    case CSSPropertyX:
        return valueForZoomAdjustedPixelLength(style, style.svgStyle().x());
    case CSSPropertyY:
        return valueForZoomAdjustedPixelLength(style, style.svgStyle().y());
    case CSSPropertyClipRule:
        return valueForConvertibleType(style.svgStyle().clipRule());
    case CSSPropertyFloodOpacity:
        return valueForNumber(style.svgStyle().floodOpacity());
    case CSSPropertyStopOpacity:
        return valueForNumber(style.svgStyle().stopOpacity());
    case CSSPropertyColorInterpolation:
        return valueForConvertibleType(style.svgStyle().colorInterpolation());
    case CSSPropertyColorInterpolationFilters:
        return valueForConvertibleType(style.svgStyle().colorInterpolationFilters());
    case CSSPropertyFillOpacity:
        return valueForNumber(style.svgStyle().fillOpacity());
    case CSSPropertyFillRule:
        return valueForConvertibleType(style.svgStyle().fillRule());
    case CSSPropertyShapeRendering:
        return valueForConvertibleType(style.svgStyle().shapeRendering());
    case CSSPropertyStrokeOpacity:
        return valueForNumber(style.svgStyle().strokeOpacity());
    case CSSPropertyAlignmentBaseline:
        return valueForConvertibleType(style.svgStyle().alignmentBaseline());
    case CSSPropertyDominantBaseline:
        return valueForConvertibleType(style.svgStyle().dominantBaseline());
    case CSSPropertyTextAnchor:
        return valueForConvertibleType(style.svgStyle().textAnchor());
    case CSSPropertyFloodColor:
        return valueForColor(style, cssValuePool, style.svgStyle().floodColor());
    case CSSPropertyLightingColor:
        return valueForColor(style, cssValuePool, style.svgStyle().lightingColor());
    case CSSPropertyStopColor:
        return valueForColor(style, cssValuePool, style.svgStyle().stopColor());
    case CSSPropertyFill:
        return valueForSVGPaint(style, cssValuePool, style.svgStyle().fillPaintType(), style.svgStyle().fillPaintUri(), style.svgStyle().fillPaintColor());
    case CSSPropertyMarkerEnd:
        return valueForMarkerURL(style, style.svgStyle().markerEndResource());
    case CSSPropertyMarkerMid:
        return valueForMarkerURL(style, style.svgStyle().markerMidResource());
    case CSSPropertyMarkerStart:
        return valueForMarkerURL(style, style.svgStyle().markerStartResource());
    case CSSPropertyStroke:
        return valueForSVGPaint(style, cssValuePool, style.svgStyle().strokePaintType(), style.svgStyle().strokePaintUri(), style.svgStyle().strokePaintColor());
    case CSSPropertyStrokeDasharray:
        return valueForStrokeDasharray(style);
    case CSSPropertyBaselineShift:
        return valueForBaselineShift(style, m_element);
    case CSSPropertyBufferedRendering:
        return valueForConvertibleType(style.svgStyle().bufferedRendering());
    case CSSPropertyGlyphOrientationHorizontal:
        return valueForGlyphOrientationHorizontal(style);
    case CSSPropertyGlyphOrientationVertical:
        return valueForGlyphOrientationVertical(style);
    case CSSPropertyVectorEffect:
        return valueForConvertibleType(style.svgStyle().vectorEffect());
    case CSSPropertyMaskType:
        return valueForConvertibleType(style.svgStyle().maskType());

    // Directional properties are handled by recursing using the direction resolved property.
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
    case CSSPropertyCornerEndEndShape:
    case CSSPropertyCornerEndStartShape:
    case CSSPropertyCornerStartEndShape:
    case CSSPropertyCornerStartStartShape:
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
    case CSSPropertyOverflowBlock:
    case CSSPropertyOverflowInline:
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
        return valueForPropertyInStyle(style, CSSProperty::resolveDirectionAwareProperty(propertyID, style.writingMode()), cssValuePool, renderer, valueType);

    // Unimplemented properties.
    case CSSPropertyAll:
    case CSSPropertyPage:
    case CSSPropertyMarker:
    case CSSPropertyWebkitMask:
    case CSSPropertyPerspectiveOriginX:
    case CSSPropertyPerspectiveOriginY:
    case CSSPropertyWebkitTextStroke:
    case CSSPropertyTransformOriginX:
    case CSSPropertyTransformOriginY:
    case CSSPropertyTransformOriginZ:
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

    // The rest are intentionally unimplemented because they are descriptors only.

    // @counter-style descriptors.
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

    // @font-face descriptors.
    case CSSPropertySrc:
    case CSSPropertyUnicodeRange:
    case CSSPropertyFontDisplay:
    case CSSPropertySizeAdjust:
        return nullptr;

    // @view-transition descriptors.
    case CSSPropertyNavigation:
    case CSSPropertyTypes:
        return nullptr;

    // @font-palette-values descriptors.
    case CSSPropertyBasePalette:
    case CSSPropertyOverrideColors:
        return nullptr;

    // @page descriptors.
    case CSSPropertySize:
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

bool ComputedStyleExtractor::propertyMatches(CSSPropertyID propertyID, const CSSValue* value) const
{
    if (!m_element)
        return false;
    if (propertyID == CSSPropertyFontSize) {
        if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(*value)) {
            m_element->protectedDocument()->updateLayoutIgnorePendingStylesheets();
            if (auto* style = m_element->computedStyle(m_pseudoElementIdentifier)) {
                if (CSSValueID sizeIdentifier = style->fontDescription().keywordSizeAsIdentifier()) {
                    if (primitiveValue->isValueID() && primitiveValue->valueID() == sizeIdentifier)
                        return true;
                }
            }
        }
    }
    RefPtr<CSSValue> computedValue = propertyValue(propertyID);
    return computedValue && value && computedValue->equals(*value);
}


Ref<MutableStyleProperties> ComputedStyleExtractor::copyProperties(std::span<const CSSPropertyID> properties) const
{
    auto vector = WTF::compactMap(properties, [&](auto& property) -> std::optional<CSSProperty> {
        if (auto value = propertyValue(property))
            return CSSProperty(property, value.releaseNonNull());
        return std::nullopt;
    });
    return MutableStyleProperties::create(WTFMove(vector));
}

Ref<MutableStyleProperties> ComputedStyleExtractor::copyProperties() const
{
    return MutableStyleProperties::create(WTF::compactMap(allLonghandCSSProperties(), [this] (auto property) -> std::optional<CSSProperty> {
        auto value = propertyValue(property);
        if (!value)
            return std::nullopt;
        return { { property, value.releaseNonNull() } };
    }).span());
}

Ref<CSSFunctionValue> ComputedStyleExtractor::valueForTransformationMatrix(const RenderStyle& style, const TransformationMatrix& transform)
{
    auto zoom = style.usedZoom();
    if (transform.isAffine()) {
        double values[] = { transform.a(), transform.b(), transform.c(), transform.d(), transform.e() / zoom, transform.f() / zoom };
        CSSValueListBuilder arguments;
        for (auto value : values)
            arguments.append(CSSPrimitiveValue::create(value));
        return CSSFunctionValue::create(CSSValueMatrix, WTFMove(arguments));
    }

    double values[] = {
        transform.m11(), transform.m12(), transform.m13(), transform.m14() * zoom,
        transform.m21(), transform.m22(), transform.m23(), transform.m24() * zoom,
        transform.m31(), transform.m32(), transform.m33(), transform.m34() * zoom,
        transform.m41() / zoom, transform.m42() / zoom, transform.m43() / zoom, transform.m44()
    };
    CSSValueListBuilder arguments;
    for (auto value : values)
        arguments.append(CSSPrimitiveValue::create(value));
    return CSSFunctionValue::create(CSSValueMatrix3d, WTFMove(arguments));
}

RefPtr<CSSFunctionValue> ComputedStyleExtractor::valueForTransformOperation(const RenderStyle& style, const TransformOperation& operation)
{
    auto translateLengthAsCSSValue = [&](const Length& length) {
        if (length.isZero())
            return CSSPrimitiveValue::create(0, CSSUnitType::CSS_PX);
        return valueForZoomAdjustedPixelLength(style, length);
    };

    auto includeLength = [](const Length& length) -> bool {
        return !length.isZero() || length.isPercent();
    };

    switch (operation.type()) {
    // translate
    case TransformOperation::Type::TranslateX:
        return CSSFunctionValue::create(CSSValueTranslateX, translateLengthAsCSSValue(uncheckedDowncast<TranslateTransformOperation>(operation).x()));
    case TransformOperation::Type::TranslateY:
        return CSSFunctionValue::create(CSSValueTranslateY, translateLengthAsCSSValue(uncheckedDowncast<TranslateTransformOperation>(operation).y()));
    case TransformOperation::Type::TranslateZ:
        return CSSFunctionValue::create(CSSValueTranslateZ, translateLengthAsCSSValue(uncheckedDowncast<TranslateTransformOperation>(operation).z()));
    case TransformOperation::Type::Translate:
    case TransformOperation::Type::Translate3D: {
        auto& translate = uncheckedDowncast<TranslateTransformOperation>(operation);
        if (!translate.is3DOperation()) {
            if (!includeLength(translate.y()))
                return CSSFunctionValue::create(CSSValueTranslate, translateLengthAsCSSValue(translate.x()));
            return CSSFunctionValue::create(CSSValueTranslate, translateLengthAsCSSValue(translate.x()),
                translateLengthAsCSSValue(translate.y()));
        }
        return CSSFunctionValue::create(CSSValueTranslate3d,
            translateLengthAsCSSValue(translate.x()),
            translateLengthAsCSSValue(translate.y()),
            translateLengthAsCSSValue(translate.z()));
    }
    // scale
    case TransformOperation::Type::ScaleX:
        return CSSFunctionValue::create(CSSValueScaleX, CSSPrimitiveValue::create(uncheckedDowncast<ScaleTransformOperation>(operation).x()));
    case TransformOperation::Type::ScaleY:
        return CSSFunctionValue::create(CSSValueScaleY, CSSPrimitiveValue::create(uncheckedDowncast<ScaleTransformOperation>(operation).y()));
    case TransformOperation::Type::ScaleZ:
        return CSSFunctionValue::create(CSSValueScaleZ, CSSPrimitiveValue::create(uncheckedDowncast<ScaleTransformOperation>(operation).z()));
    case TransformOperation::Type::Scale:
    case TransformOperation::Type::Scale3D: {
        auto& scale = uncheckedDowncast<ScaleTransformOperation>(operation);
        if (!scale.is3DOperation()) {
            if (scale.x() == scale.y())
                return CSSFunctionValue::create(CSSValueScale, CSSPrimitiveValue::create(scale.x()));
            return CSSFunctionValue::create(CSSValueScale, CSSPrimitiveValue::create(scale.x()),
                CSSPrimitiveValue::create(scale.y()));
        }
        return CSSFunctionValue::create(CSSValueScale3d,
            CSSPrimitiveValue::create(scale.x()),
            CSSPrimitiveValue::create(scale.y()),
            CSSPrimitiveValue::create(scale.z()));
    }
    // rotate
    case TransformOperation::Type::RotateX:
        return CSSFunctionValue::create(CSSValueRotateX, CSSPrimitiveValue::create(uncheckedDowncast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::RotateY:
        return CSSFunctionValue::create(CSSValueRotateX, CSSPrimitiveValue::create(uncheckedDowncast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::RotateZ:
        return CSSFunctionValue::create(CSSValueRotateZ, CSSPrimitiveValue::create(uncheckedDowncast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::Rotate:
        return CSSFunctionValue::create(CSSValueRotate, CSSPrimitiveValue::create(uncheckedDowncast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::Rotate3D: {
        auto& rotate = uncheckedDowncast<RotateTransformOperation>(operation);
        return CSSFunctionValue::create(CSSValueRotate3d, CSSPrimitiveValue::create(rotate.x()), CSSPrimitiveValue::create(rotate.y()), CSSPrimitiveValue::create(rotate.z()), CSSPrimitiveValue::create(rotate.angle(), CSSUnitType::CSS_DEG));
    }
    // skew
    case TransformOperation::Type::SkewX:
        return CSSFunctionValue::create(CSSValueSkewX, CSSPrimitiveValue::create(uncheckedDowncast<SkewTransformOperation>(operation).angleX(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::SkewY:
        return CSSFunctionValue::create(CSSValueSkewY, CSSPrimitiveValue::create(uncheckedDowncast<SkewTransformOperation>(operation).angleY(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::Skew: {
        auto& skew = uncheckedDowncast<SkewTransformOperation>(operation);
        if (!skew.angleY())
            return CSSFunctionValue::create(CSSValueSkew, CSSPrimitiveValue::create(skew.angleX(), CSSUnitType::CSS_DEG));
        return CSSFunctionValue::create(CSSValueSkew, CSSPrimitiveValue::create(skew.angleX(), CSSUnitType::CSS_DEG),
            CSSPrimitiveValue::create(skew.angleY(), CSSUnitType::CSS_DEG));
    }
    // perspective
    case TransformOperation::Type::Perspective:
        if (auto perspective = uncheckedDowncast<PerspectiveTransformOperation>(operation).perspective())
            return CSSFunctionValue::create(CSSValuePerspective, valueForZoomAdjustedPixelLength(style, *perspective));
        return CSSFunctionValue::create(CSSValuePerspective, CSSPrimitiveValue::create(CSSValueNone));
    // matrix
    case TransformOperation::Type::Matrix:
    case TransformOperation::Type::Matrix3D: {
        TransformationMatrix transform;
        operation.apply(transform, { });
        return valueForTransformationMatrix(style, transform);
    }
    case TransformOperation::Type::Identity:
    case TransformOperation::Type::None:
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

Ref<CSSPrimitiveValue> ComputedStyleExtractor::valueForZoomAdjustedPixelLength(const RenderStyle& style, const Length& length)
{
    if (length.isFixed())
        return valueForZoomAdjustedFloatLength(style, length.value());
    return CSSPrimitiveValue::create(length, style);
}

RefPtr<CSSValueList> ComputedStyleExtractor::valueForShorthandProperties(const StylePropertyShorthand& shorthand) const
{
    CSSValueListBuilder list;
    for (auto longhand : shorthand)
        list.append(propertyValue(longhand, UpdateLayout::No).releaseNonNull());
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValueList> ComputedStyleExtractor::valueFor2SidesShorthand(const StylePropertyShorthand& shorthand) const
{
    // Assume the properties are in the usual order start, end.
    auto longhands = shorthand.properties();
    auto startValue = propertyValue(longhands[0], UpdateLayout::No);
    auto endValue = propertyValue(longhands[1], UpdateLayout::No);

    // All 2 properties must be specified.
    if (!startValue || !endValue)
        return nullptr;

    if (compareCSSValuePtr(startValue, endValue))
        return CSSValueList::createSpaceSeparated(startValue.releaseNonNull());
    return CSSValueList::createSpaceSeparated(startValue.releaseNonNull(), endValue.releaseNonNull());
}

RefPtr<CSSValueList> ComputedStyleExtractor::valueFor4SidesShorthand(const StylePropertyShorthand& shorthand) const
{
    // Assume the properties are in the usual order top, right, bottom, left.
    auto longhands = shorthand.properties();
    auto topValue = propertyValue(longhands[0], UpdateLayout::No);
    auto rightValue = propertyValue(longhands[1], UpdateLayout::No);
    auto bottomValue = propertyValue(longhands[2], UpdateLayout::No);
    auto leftValue = propertyValue(longhands[3], UpdateLayout::No);

    // All 4 properties must be specified.
    if (!topValue || !rightValue || !bottomValue || !leftValue)
        return nullptr;

    bool showLeft = !compareCSSValuePtr(rightValue, leftValue);
    bool showBottom = !compareCSSValuePtr(topValue, bottomValue) || showLeft;
    bool showRight = !compareCSSValuePtr(topValue, rightValue) || showBottom;

    CSSValueListBuilder list;
    list.append(topValue.releaseNonNull());
    if (showRight)
        list.append(rightValue.releaseNonNull());
    if (showBottom)
        list.append(bottomValue.releaseNonNull());
    if (showLeft)
        list.append(leftValue.releaseNonNull());
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForGridShorthand(const StylePropertyShorthand& shorthand) const
{
    CSSValueListBuilder builder;
    for (auto longhand : shorthand)
        builder.append(propertyValue(longhand, UpdateLayout::No).releaseNonNull());
    return CSSValueList::createSlashSeparated(WTFMove(builder));
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForBackgroundShorthand() const
{
    static constexpr std::array propertiesBeforeSlashSeparator { CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat, CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition };
    static constexpr std::array propertiesAfterSlashSeparator { CSSPropertyBackgroundSize, CSSPropertyBackgroundOrigin, CSSPropertyBackgroundClip };

    return fillLayerPropertyShorthandValue(CSSPropertyBackground, StylePropertyShorthand(CSSPropertyBackground, std::span { propertiesBeforeSlashSeparator }), StylePropertyShorthand(CSSPropertyBackground, std::span { propertiesAfterSlashSeparator }), CSSPropertyBackgroundColor);
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForMaskShorthand() const
{
    static constexpr std::array propertiesBeforeSlashSeparator { CSSPropertyMaskImage, CSSPropertyMaskPosition };
    static constexpr std::array propertiesAfterSlashSeparator { CSSPropertyMaskSize, CSSPropertyMaskRepeat, CSSPropertyMaskOrigin, CSSPropertyMaskClip, CSSPropertyMaskComposite, CSSPropertyMaskMode };

    return fillLayerPropertyShorthandValue(CSSPropertyMask, StylePropertyShorthand(CSSPropertyMask, std::span { propertiesBeforeSlashSeparator }), StylePropertyShorthand(CSSPropertyMask, std::span { propertiesAfterSlashSeparator }), CSSPropertyInvalid);
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForBorderShorthand() const
{
    auto value = propertyValue(CSSPropertyBorderTop, UpdateLayout::No);
    const CSSPropertyID properties[3] = { CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
    for (auto& property : properties) {
        if (!compareCSSValuePtr<CSSValue>(value, propertyValue(property, UpdateLayout::No)))
            return nullptr;
    }
    return value;
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForBorderBlockShorthand() const
{
    auto value = propertyValue(CSSPropertyBorderBlockStart, UpdateLayout::No);
    if (!compareCSSValuePtr<CSSValue>(value, propertyValue(CSSPropertyBorderBlockEnd, UpdateLayout::No)))
        return nullptr;
    return value;
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForBorderInlineShorthand() const
{
    auto value = propertyValue(CSSPropertyBorderInlineStart, UpdateLayout::No);
    if (!compareCSSValuePtr<CSSValue>(value, propertyValue(CSSPropertyBorderInlineEnd, UpdateLayout::No)))
        return nullptr;
    return value;
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForFontVariantShorthand() const
{
    CSSValueListBuilder list;
    for (auto longhand : fontVariantShorthand()) {
        auto value = propertyValue(longhand, UpdateLayout::No);
        // We may not have a value if the longhand is disabled.
        if (!value || isValueID(value, CSSValueNormal))
            continue;
        list.append(value.releaseNonNull());
    }
    if (list.isEmpty())
        return CSSPrimitiveValue::create(CSSValueNormal);
    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForTextBoxShorthand(const RenderStyle& style) const
{
    auto textBoxTrim = style.textBoxTrim();
    auto textBoxEdge = style.textBoxEdge();
    auto textBoxEdgeIsAuto = textBoxEdge == TextEdge { TextEdgeType::Auto, TextEdgeType::Auto };

    if (textBoxTrim == TextBoxTrim::None && textBoxEdgeIsAuto)
        return CSSPrimitiveValue::create(CSSValueNormal);
    if (textBoxEdgeIsAuto)
        return valueForConvertibleType(textBoxTrim);
    if (textBoxTrim == TextBoxTrim::TrimBoth)
        return valueForTextEdge(CSSPropertyTextBoxEdge, textBoxEdge);

    return CSSValuePair::create(valueForConvertibleType(textBoxTrim), valueForTextEdge(CSSPropertyTextBoxEdge, textBoxEdge));
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForLineClampShorthand(const RenderStyle& style) const
{
    auto maxLines = style.maxLines();
    if (!maxLines)
        return CSSPrimitiveValue::create(CSSValueNone);

    Ref maxLineCount = CSSPrimitiveValue::create(maxLines, CSSUnitType::CSS_INTEGER);
    auto blockEllipsisType = style.blockEllipsis().type;

    if (blockEllipsisType == BlockEllipsis::Type::None)
        return CSSValuePair::create(WTFMove(maxLineCount), CSSPrimitiveValue::create(CSSValueNone));

    if (blockEllipsisType == BlockEllipsis::Type::Auto)
        return CSSValuePair::create(WTFMove(maxLineCount), CSSPrimitiveValue::create(CSSValueAuto));

    if (blockEllipsisType == BlockEllipsis::Type::String)
        return CSSValuePair::create(WTFMove(maxLineCount), CSSPrimitiveValue::createCustomIdent(style.blockEllipsis().string));

    ASSERT_NOT_REACHED();
    return { };
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForContainerShorthand(const RenderStyle& style) const
{
    auto name = [&]() -> Ref<CSSValue> {
        if (style.containerNames().isEmpty())
            return CSSPrimitiveValue::create(CSSValueNone);
        return propertyValue(CSSPropertyContainerName, UpdateLayout::No).releaseNonNull();
    }();
    if (style.containerType() == ContainerType::Normal)
        return CSSValueList::createSlashSeparated(WTFMove(name));
    return CSSValueList::createSlashSeparated(WTFMove(name),
        propertyValue(CSSPropertyContainerType, UpdateLayout::No).releaseNonNull());
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForColumnsShorthand(const RenderStyle& style) const
{
    if (style.hasAutoColumnCount())
        return style.hasAutoColumnWidth() ? CSSPrimitiveValue::create(CSSValueAuto) : valueForZoomAdjustedFloatLength(style, style.columnWidth());
    if (style.hasAutoColumnWidth())
        return style.hasAutoColumnCount() ? CSSPrimitiveValue::create(CSSValueAuto) : CSSPrimitiveValue::create(style.columnCount());
    return valueForShorthandProperties(columnsShorthand());
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForFlexFlowShorthand(const RenderStyle& style) const
{
    if (style.flexWrap() == RenderStyle::initialFlexWrap())
        return valueForConvertibleType(style.flexDirection());
    if (style.flexDirection() == RenderStyle::initialFlexDirection())
        return valueForConvertibleType(style.flexWrap());
    return valueForShorthandProperties(flexFlowShorthand());
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForTextWrapShorthand(const RenderStyle& style) const
{
    auto textWrapMode = style.textWrapMode();
    auto textWrapStyle = style.textWrapStyle();

    if (textWrapStyle == TextWrapStyle::Auto)
        return valueForConvertibleType(textWrapMode);
    if (textWrapMode == TextWrapMode::Wrap)
        return valueForConvertibleType(textWrapStyle);

    return CSSValuePair::create(valueForConvertibleType(textWrapMode), valueForConvertibleType(textWrapStyle));
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForWhiteSpaceShorthand(const RenderStyle& style) const
{
    auto whiteSpaceCollapse = style.whiteSpaceCollapse();
    auto textWrapMode = style.textWrapMode();

    // Convert to backwards-compatible keywords if possible.
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Collapse && textWrapMode == TextWrapMode::Wrap)
        return CSSPrimitiveValue::create(CSSValueNormal);
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Preserve && textWrapMode == TextWrapMode::NoWrap)
        return CSSPrimitiveValue::create(CSSValuePre);
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Preserve && textWrapMode == TextWrapMode::Wrap)
        return CSSPrimitiveValue::create(CSSValuePreWrap);
    if (whiteSpaceCollapse == WhiteSpaceCollapse::PreserveBreaks && textWrapMode == TextWrapMode::Wrap)
        return CSSPrimitiveValue::create(CSSValuePreLine);

    // Omit default longhand values.
    if (whiteSpaceCollapse == WhiteSpaceCollapse::Collapse)
        return valueForConvertibleType(textWrapMode);
    if (textWrapMode == TextWrapMode::Wrap)
        return valueForConvertibleType(whiteSpaceCollapse);

    return CSSValuePair::create(valueForConvertibleType(whiteSpaceCollapse), valueForConvertibleType(textWrapMode));
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForPositionTryShorthand(const RenderStyle& style) const
{
    if (style.positionTryOrder() == RenderStyle::initialPositionTryOrder())
        return valueForPositionTryFallbacks(style.positionTryFallbacks());
    return valueForShorthandProperties(positionTryShorthand());
}

size_t ComputedStyleExtractor::layerCount(CSSPropertyID property) const
{
    ASSERT(property == CSSPropertyBackground || property == CSSPropertyMask);

    if (!m_element)
        return 0;

    std::unique_ptr<RenderStyle> ownedStyle;
    auto style = computeRenderStyleForProperty(*m_element, m_pseudoElementIdentifier, property, ownedStyle, nullptr);
    if (!style)
        return 0;

    auto& layers = property == CSSPropertyMask ? style->maskLayers() : style->backgroundLayers();

    size_t layerCount = 0;
    for (auto* layer = &layers; layer; layer = layer->next())
        layerCount++;
    if (layerCount == 1 && property == CSSPropertyMask && !layers.image())
        return 0;
    return layerCount;
}

Ref<CSSValue> ComputedStyleExtractor::fillLayerPropertyShorthandValue(CSSPropertyID property, const StylePropertyShorthand& propertiesBeforeSlashSeparator, const StylePropertyShorthand& propertiesAfterSlashSeparator, CSSPropertyID lastLayerProperty) const
{
    ASSERT(property == CSSPropertyBackground || property == CSSPropertyMask);
    size_t layerCount = this->layerCount(property);
    if (!layerCount) {
        ASSERT(property == CSSPropertyMask);
        return CSSPrimitiveValue::create(CSSValueNone);
    }

    auto lastValue = lastLayerProperty != CSSPropertyInvalid ? propertyValue(lastLayerProperty, UpdateLayout::No) : nullptr;
    auto before = valueForShorthandProperties(propertiesBeforeSlashSeparator);
    auto after = valueForShorthandProperties(propertiesAfterSlashSeparator);

    // The computed properties are returned as lists of properties, with a list of layers in each.
    // We want to swap that around to have a list of layers, with a list of properties in each.

    CSSValueListBuilder layers;
    for (size_t i = 0; i < layerCount; i++) {
        CSSValueListBuilder beforeList;
        if (i == layerCount - 1 && lastValue)
            beforeList.append(*lastValue);
        for (size_t j = 0; j < propertiesBeforeSlashSeparator.length(); j++) {
            auto& value = *before->item(j);
            beforeList.append(const_cast<CSSValue&>(layerCount == 1 ? value : *downcast<CSSValueList>(value).item(i)));
        }
        CSSValueListBuilder afterList;
        for (size_t j = 0; j < propertiesAfterSlashSeparator.length(); j++) {
            auto& value = *after->item(j);
            afterList.append(const_cast<CSSValue&>(layerCount == 1 ? value : *downcast<CSSValueList>(value).item(i)));
        }
        auto list = CSSValueList::createSlashSeparated(CSSValueList::createSpaceSeparated(WTFMove(beforeList)),
            CSSValueList::createSpaceSeparated(WTFMove(afterList)));
        if (layerCount == 1)
            return list;
        layers.append(WTFMove(list));
    }
    return CSSValueList::createCommaSeparated(WTFMove(layers));
}

} // namespace WebCore
