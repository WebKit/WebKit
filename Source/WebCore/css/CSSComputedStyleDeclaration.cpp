/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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
#include "CSSComputedStyleDeclaration.h"

#include "BasicShapeFunctions.h"
#include "CSSBasicShapes.h"
#include "CSSBorderImage.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleValue.h"
#include "CSSFontValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyAnimation.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSReflectValue.h"
#include "CSSSelector.h"
#include "CSSShadowValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "ComposedTreeAncestorIterator.h"
#include "ContentData.h"
#include "CursorList.h"
#include "DeprecatedCSSOMValue.h"
#include "Document.h"
#include "DocumentTimeline.h"
#include "FontCascade.h"
#include "FontSelectionValueInlines.h"
#include "FontTaggedSettings.h"
#include "NodeRenderStyle.h"
#include "Pair.h"
#include "QuotesData.h"
#include "Rect.h"
#include "RenderBlock.h"
#include "RenderBox.h"
#include "RenderInline.h"
#include "RenderStyle.h"
#include "SVGElement.h"
#include "Settings.h"
#include "ShapeValue.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StylePropertyShorthandFunctions.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleScrollSnapPoints.h"
#include "TouchAction.h"
#include "WebKitFontFamilyNames.h"
#include "WillChangeData.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "RenderGrid.h"

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSComputedStyleDeclaration);

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
        return CSSValuePool::singleton().createValue(length.percent(), CSSUnitType::CSS_PERCENTAGE);
    if (length.isAuto() || length.isFixed())
        return CSSValuePool::singleton().createValue(length.value(), CSSUnitType::CSS_NUMBER);

    // Calculating the actual length currently in use would require most of the code from RenderBoxModelObject::paintNinePieceImage.
    // And even if we could do that, it's not clear if that's exactly what we'd want during animation.
    // FIXME: For now, just return 0.
    ASSERT(length.isCalculated());
    return CSSValuePool::singleton().createValue(0, CSSUnitType::CSS_NUMBER);
}

static Ref<CSSBorderImageSliceValue> valueForNinePieceImageSlice(const NinePieceImage& image)
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

    return CSSBorderImageSliceValue::create(CSSValuePool::singleton().createValue(WTFMove(quad)), image.fill());
}

static Ref<CSSPrimitiveValue> valueForNinePieceImageQuad(const LengthBox& box)
{
    RefPtr<CSSPrimitiveValue> top;
    RefPtr<CSSPrimitiveValue> right;
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;

    auto& cssValuePool = CSSValuePool::singleton();

    if (box.top().isRelative())
        top = cssValuePool.createValue(box.top().value(), CSSUnitType::CSS_NUMBER);
    else
        top = cssValuePool.createValue(box.top());

    if (box.right() == box.top() && box.bottom() == box.top() && box.left() == box.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (box.right().isRelative())
            right = cssValuePool.createValue(box.right().value(), CSSUnitType::CSS_NUMBER);
        else
            right = cssValuePool.createValue(box.right());

        if (box.bottom() == box.top() && box.right() == box.left()) {
            bottom = top;
            left = right;
        } else {
            if (box.bottom().isRelative())
                bottom = cssValuePool.createValue(box.bottom().value(), CSSUnitType::CSS_NUMBER);
            else
                bottom = cssValuePool.createValue(box.bottom());

            if (box.left() == box.right())
                left = right;
            else {
                if (box.left().isRelative())
                    left = cssValuePool.createValue(box.left().value(), CSSUnitType::CSS_NUMBER);
                else
                    left = cssValuePool.createValue(box.left());
            }
        }
    }

    auto quad = Quad::create();
    quad->setTop(WTFMove(top));
    quad->setRight(WTFMove(right));
    quad->setBottom(WTFMove(bottom));
    quad->setLeft(WTFMove(left));

    return cssValuePool.createValue(WTFMove(quad));
}

static Ref<CSSValue> valueForNinePieceImageRepeat(const NinePieceImage& image)
{
    auto& cssValuePool = CSSValuePool::singleton();
    auto horizontalRepeat = cssValuePool.createIdentifierValue(valueForRepeatRule(image.horizontalRule()));
    RefPtr<CSSPrimitiveValue> verticalRepeat;
    if (image.horizontalRule() == image.verticalRule())
        verticalRepeat = horizontalRepeat.copyRef();
    else
        verticalRepeat = cssValuePool.createIdentifierValue(valueForRepeatRule(image.verticalRule()));
    return cssValuePool.createValue(Pair::create(WTFMove(horizontalRepeat), WTFMove(verticalRepeat)));
}

static Ref<CSSValue> valueForNinePieceImage(const NinePieceImage& image)
{
    if (!image.hasImage())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    RefPtr<CSSValue> imageValue;
    if (image.image())
        imageValue = image.image()->cssValue();

    auto imageSlices = valueForNinePieceImageSlice(image);
    auto borderSlices = valueForNinePieceImageQuad(image.borderSlices());
    auto outset = valueForNinePieceImageQuad(image.outset());
    auto repeat = valueForNinePieceImageRepeat(image);
    return createBorderImageValue(WTFMove(imageValue), WTFMove(imageSlices), WTFMove(borderSlices), WTFMove(outset), WTFMove(repeat));
}

inline static Ref<CSSPrimitiveValue> zoomAdjustedPixelValue(double value, const RenderStyle& style)
{
    return CSSValuePool::singleton().createValue(adjustFloatForAbsoluteZoom(value, style), CSSUnitType::CSS_PX);
}

inline static Ref<CSSPrimitiveValue> zoomAdjustedNumberValue(double value, const RenderStyle& style)
{
    return CSSValuePool::singleton().createValue(value / style.effectiveZoom(), CSSUnitType::CSS_NUMBER);
}

static Ref<CSSPrimitiveValue> zoomAdjustedPixelValueForLength(const Length& length, const RenderStyle& style)
{
    if (length.isFixed())
        return zoomAdjustedPixelValue(length.value(), style);
    return CSSValuePool::singleton().createValue(length, style);
}

static Ref<CSSValue> valueForReflection(const StyleReflection* reflection, const RenderStyle& style)
{
    if (!reflection)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    RefPtr<CSSPrimitiveValue> offset;
    if (reflection->offset().isPercentOrCalculated())
        offset = CSSValuePool::singleton().createValue(reflection->offset().percent(), CSSUnitType::CSS_PERCENTAGE);
    else
        offset = zoomAdjustedPixelValue(reflection->offset().value(), style);

    RefPtr<CSSPrimitiveValue> direction;
    switch (reflection->direction()) {
    case ReflectionDirection::Below:
        direction = CSSValuePool::singleton().createIdentifierValue(CSSValueBelow);
        break;
    case ReflectionDirection::Above:
        direction = CSSValuePool::singleton().createIdentifierValue(CSSValueAbove);
        break;
    case ReflectionDirection::Left:
        direction = CSSValuePool::singleton().createIdentifierValue(CSSValueLeft);
        break;
    case ReflectionDirection::Right:
        direction = CSSValuePool::singleton().createIdentifierValue(CSSValueRight);
        break;
    }

    return CSSReflectValue::create(direction.releaseNonNull(), offset.releaseNonNull(), valueForNinePieceImage(reflection->mask()));
}

static Ref<CSSValueList> createPositionListForLayer(CSSPropertyID propertyID, const FillLayer& layer, const RenderStyle& style)
{
    auto list = CSSValueList::createSpaceSeparated();
    if (layer.isBackgroundXOriginSet()) {
        ASSERT_UNUSED(propertyID, propertyID == CSSPropertyBackgroundPosition || propertyID == CSSPropertyWebkitMaskPosition);
        list->append(CSSValuePool::singleton().createValue(layer.backgroundXOrigin()));
    }
    list->append(zoomAdjustedPixelValueForLength(layer.xPosition(), style));
    if (layer.isBackgroundYOriginSet()) {
        ASSERT(propertyID == CSSPropertyBackgroundPosition || propertyID == CSSPropertyWebkitMaskPosition);
        list->append(CSSValuePool::singleton().createValue(layer.backgroundYOrigin()));
    }
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
            auto& enclosingClippingBox = box.enclosingClippingBoxForStickyPosition();
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

    return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
}

Ref<CSSPrimitiveValue> ComputedStyleExtractor::currentColorOrValidColor(const RenderStyle* style, const Color& color) const
{
    // This function does NOT look at visited information, so that computed style doesn't expose that.
    if (!color.isValid())
        return CSSValuePool::singleton().createColorValue(style->color());
    return CSSValuePool::singleton().createColorValue(color);
}

static Ref<CSSPrimitiveValue> percentageOrZoomAdjustedValue(Length length, const RenderStyle& style)
{
    if (length.isPercent())
        return CSSValuePool::singleton().createValue(length.percent(), CSSUnitType::CSS_PERCENTAGE);
    
    return zoomAdjustedPixelValueForLength(length, style);
}

static Ref<CSSPrimitiveValue> autoOrZoomAdjustedValue(Length length, const RenderStyle& style)
{
    if (length.isAuto())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);

    return zoomAdjustedPixelValueForLength(length, style);
}

static Ref<CSSValueList> borderRadiusCornerValues(const LengthSize& radius, const RenderStyle& style)
{
    auto list = CSSValueList::createSpaceSeparated();
    list->append(percentageOrZoomAdjustedValue(radius.width, style));
    list->append(percentageOrZoomAdjustedValue(radius.height, style));
    return list;
}

static Ref<CSSValue> valueForQuotes(const QuotesData* quotes)
{
    if (!quotes)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    unsigned size = quotes->size();
    if (!size)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    auto list = CSSValueList::createSpaceSeparated();
    for (unsigned i = 0; i < size; ++i) {
        list->append(CSSValuePool::singleton().createValue(quotes->openQuote(i), CSSUnitType::CSS_STRING));
        list->append(CSSValuePool::singleton().createValue(quotes->closeQuote(i), CSSUnitType::CSS_STRING));
    }
    return list;
}

static Ref<CSSValue> borderRadiusCornerValue(const LengthSize& radius, const RenderStyle& style)
{
    if (radius.width == radius.height)
        return percentageOrZoomAdjustedValue(radius.width, style);
    return borderRadiusCornerValues(radius, style);
}

static Ref<CSSValueList> borderRadiusShorthandValue(const RenderStyle& style)
{
    auto list = CSSValueList::createSlashSeparated();
    bool showHorizontalBottomLeft = style.borderTopRightRadius().width != style.borderBottomLeftRadius().width;
    bool showHorizontalBottomRight = showHorizontalBottomLeft || (style.borderBottomRightRadius().width != style.borderTopLeftRadius().width);
    bool showHorizontalTopRight = showHorizontalBottomRight || (style.borderTopRightRadius().width != style.borderTopLeftRadius().width);

    bool showVerticalBottomLeft = style.borderTopRightRadius().height != style.borderBottomLeftRadius().height;
    bool showVerticalBottomRight = showVerticalBottomLeft || (style.borderBottomRightRadius().height != style.borderTopLeftRadius().height);
    bool showVerticalTopRight = showVerticalBottomRight || (style.borderTopRightRadius().height != style.borderTopLeftRadius().height);

    auto topLeftRadius = borderRadiusCornerValues(style.borderTopLeftRadius(), style);
    auto topRightRadius = borderRadiusCornerValues(style.borderTopRightRadius(), style);
    auto bottomRightRadius = borderRadiusCornerValues(style.borderBottomRightRadius(), style);
    auto bottomLeftRadius = borderRadiusCornerValues(style.borderBottomLeftRadius(), style);

    auto horizontalRadii = CSSValueList::createSpaceSeparated();
    horizontalRadii->append(*topLeftRadius->item(0));
    if (showHorizontalTopRight)
        horizontalRadii->append(*topRightRadius->item(0));
    if (showHorizontalBottomRight)
        horizontalRadii->append(*bottomRightRadius->item(0));
    if (showHorizontalBottomLeft)
        horizontalRadii->append(*bottomLeftRadius->item(0));

    list->append(WTFMove(horizontalRadii));

    auto verticalRadiiList = CSSValueList::createSpaceSeparated();
    verticalRadiiList->append(*topLeftRadius->item(1));
    if (showVerticalTopRight)
        verticalRadiiList->append(*topRightRadius->item(1));
    if (showVerticalBottomRight)
        verticalRadiiList->append(*bottomRightRadius->item(1));
    if (showVerticalBottomLeft)
        verticalRadiiList->append(*bottomLeftRadius->item(1));

    if (!verticalRadiiList->equals(downcast<CSSValueList>(*list->item(0))))
        list->append(WTFMove(verticalRadiiList));

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
    auto& cssValuePool = CSSValuePool::singleton();
    if (transform.isAffine()) {
        transformValue = CSSFunctionValue::create(CSSValueMatrix);

        transformValue->append(cssValuePool.createValue(transform.a(), CSSUnitType::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.b(), CSSUnitType::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.c(), CSSUnitType::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.d(), CSSUnitType::CSS_NUMBER));
        transformValue->append(zoomAdjustedNumberValue(transform.e(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.f(), style));
    } else {
        transformValue = CSSFunctionValue::create(CSSValueMatrix3d);

        transformValue->append(cssValuePool.createValue(transform.m11(), CSSUnitType::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m12(), CSSUnitType::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m13(), CSSUnitType::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m14(), CSSUnitType::CSS_NUMBER));

        transformValue->append(cssValuePool.createValue(transform.m21(), CSSUnitType::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m22(), CSSUnitType::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m23(), CSSUnitType::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m24(), CSSUnitType::CSS_NUMBER));

        transformValue->append(cssValuePool.createValue(transform.m31(), CSSUnitType::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m32(), CSSUnitType::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m33(), CSSUnitType::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m34(), CSSUnitType::CSS_NUMBER));

        transformValue->append(zoomAdjustedNumberValue(transform.m41(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.m42(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.m43(), style));
        transformValue->append(cssValuePool.createValue(transform.m44(), CSSUnitType::CSS_NUMBER));
    }

    return transformValue.releaseNonNull();
}

static bool rendererCanBeTransformed(RenderObject* renderer)
{
    // Inline renderers do not support transforms.
    return renderer && !is<RenderInline>(*renderer);
}

static Ref<CSSValue> computedTransform(RenderObject* renderer, const RenderStyle& style)
{
    if (!rendererCanBeTransformed(renderer) || !style.hasTransform())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    FloatRect pixelSnappedRect;
    if (is<RenderBox>(*renderer))
        pixelSnappedRect = snapRectToDevicePixels(downcast<RenderBox>(*renderer).borderBoxRect(), renderer->document().deviceScaleFactor());

    TransformationMatrix transform;
    style.applyTransform(transform, pixelSnappedRect, { });
    // Note that this does not flatten to an affine transform if ENABLE(3D_TRANSFORMS) is off, by design.

    // FIXME: Need to print out individual functions (https://bugs.webkit.org/show_bug.cgi?id=23924)
    auto list = CSSValueList::createSpaceSeparated();
    list->append(matrixTransformValue(transform, style));
    return list;
}

static Ref<CSSValue> computedTranslate(RenderObject* renderer, const RenderStyle& style)
{
    auto* translate = style.translate();
    if (!translate || !rendererCanBeTransformed(renderer) || translate->isIdentity())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    FloatRect pixelSnappedRect;
    if (is<RenderBox>(*renderer))
        pixelSnappedRect = snapRectToDevicePixels(downcast<RenderBox>(*renderer).borderBoxRect(), renderer->document().deviceScaleFactor());

    TransformationMatrix transform;
    translate->apply(transform, pixelSnappedRect.size());

    auto list = CSSValueList::createSpaceSeparated();
    if (transform.isAffine()) {
        list->append(zoomAdjustedPixelValue(transform.e(), style));
        if (transform.f())
            list->append(zoomAdjustedPixelValue(transform.f(), style));
    } else {
        list->append(zoomAdjustedPixelValue(transform.m41(), style));
        list->append(zoomAdjustedPixelValue(transform.m42(), style));
        list->append(zoomAdjustedNumberValue(transform.m43(), style));
    }

    return list;
}

static Ref<CSSValue> computedScale(RenderObject* renderer, const RenderStyle& style)
{
    auto* scale = style.scale();
    auto& cssValuePool = CSSValuePool::singleton();
    if (!scale || !rendererCanBeTransformed(renderer) || scale->isIdentity())
        return cssValuePool.createIdentifierValue(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();
    list->append(cssValuePool.createValue(scale->x(), CSSUnitType::CSS_NUMBER));
    if (scale->z() != 1) {
        list->append(cssValuePool.createValue(scale->y(), CSSUnitType::CSS_NUMBER));
        list->append(cssValuePool.createValue(scale->z(), CSSUnitType::CSS_NUMBER));
    } else if (scale->x() != scale->y())
        list->append(cssValuePool.createValue(scale->y(), CSSUnitType::CSS_NUMBER));
    return list;
}

static Ref<CSSValue> computedRotate(RenderObject* renderer, const RenderStyle& style)
{
    auto* rotate = style.rotate();
    auto& cssValuePool = CSSValuePool::singleton();
    if (!rotate || !rendererCanBeTransformed(renderer) || rotate->isIdentity())
        return cssValuePool.createIdentifierValue(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();

    bool hasImplicitX = !rotate->x();
    bool hasImplicitY = !rotate->y();
    bool hasImplicitZ = rotate->z() == 1;

    if (!hasImplicitX && hasImplicitY && hasImplicitZ)
        list->append(cssValuePool.createIdentifierValue(CSSValueX));
    else if (hasImplicitX && !hasImplicitY && hasImplicitZ)
        list->append(cssValuePool.createIdentifierValue(CSSValueY));
    else if (hasImplicitX && hasImplicitY && !hasImplicitZ)
        list->append(cssValuePool.createIdentifierValue(CSSValueZ));
    else if (!hasImplicitX || !hasImplicitY || !hasImplicitZ) {
        list->append(cssValuePool.createValue(rotate->x(), CSSUnitType::CSS_NUMBER));
        list->append(cssValuePool.createValue(rotate->y(), CSSUnitType::CSS_NUMBER));
        list->append(cssValuePool.createValue(rotate->z(), CSSUnitType::CSS_NUMBER));
    }

    list->append(cssValuePool.createValue(rotate->angle(), CSSUnitType::CSS_DEG));

    return list;
}

static inline Ref<CSSPrimitiveValue> adjustLengthForZoom(double length, const RenderStyle& style, AdjustPixelValuesForComputedStyle adjust)
{
    return adjust == AdjustPixelValues ? zoomAdjustedPixelValue(length, style) : CSSValuePool::singleton().createValue(length, CSSUnitType::CSS_PX);
}

static inline Ref<CSSPrimitiveValue> adjustLengthForZoom(const Length& length, const RenderStyle& style, AdjustPixelValuesForComputedStyle adjust)
{
    return adjust == AdjustPixelValues ? zoomAdjustedPixelValue(length.value(), style) : CSSValuePool::singleton().createValue(length);
}

Ref<CSSValue> ComputedStyleExtractor::valueForShadow(const ShadowData* shadow, CSSPropertyID propertyID, const RenderStyle& style, AdjustPixelValuesForComputedStyle adjust)
{
    auto& cssValuePool = CSSValuePool::singleton();
    if (!shadow)
        return cssValuePool.createIdentifierValue(CSSValueNone);

    auto list = CSSValueList::createCommaSeparated();
    for (const ShadowData* currShadowData = shadow; currShadowData; currShadowData = currShadowData->next()) {
        auto x = adjustLengthForZoom(currShadowData->x(), style, adjust);
        auto y = adjustLengthForZoom(currShadowData->y(), style, adjust);
        auto blur = adjustLengthForZoom(currShadowData->radius(), style, adjust);
        auto spread = propertyID == CSSPropertyTextShadow ? RefPtr<CSSPrimitiveValue>() : adjustLengthForZoom(currShadowData->spread(), style, adjust);
        auto style = propertyID == CSSPropertyTextShadow || currShadowData->style() == ShadowStyle::Normal ? RefPtr<CSSPrimitiveValue>() : cssValuePool.createIdentifierValue(CSSValueInset);
        auto color = cssValuePool.createColorValue(currShadowData->color());
        list->prepend(CSSShadowValue::create(WTFMove(x), WTFMove(y), WTFMove(blur), WTFMove(spread), WTFMove(style), WTFMove(color)));
    }
    return list;
}

Ref<CSSValue> ComputedStyleExtractor::valueForFilter(const RenderStyle& style, const FilterOperations& filterOperations, AdjustPixelValuesForComputedStyle adjust)
{
    auto& cssValuePool = CSSValuePool::singleton();
    if (filterOperations.operations().isEmpty())
        return cssValuePool.createIdentifierValue(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();

    Vector<RefPtr<FilterOperation>>::const_iterator end = filterOperations.operations().end();
    for (Vector<RefPtr<FilterOperation>>::const_iterator it = filterOperations.operations().begin(); it != end; ++it) {
        FilterOperation& filterOperation = **it;

        if (filterOperation.type() == FilterOperation::REFERENCE) {
            ReferenceFilterOperation& referenceOperation = downcast<ReferenceFilterOperation>(filterOperation);
            list->append(cssValuePool.createValue(referenceOperation.url(), CSSUnitType::CSS_URI));
        } else {
            RefPtr<CSSFunctionValue> filterValue;
            switch (filterOperation.type()) {
            case FilterOperation::GRAYSCALE: {
                filterValue = CSSFunctionValue::create(CSSValueGrayscale);
                filterValue->append(cssValuePool.createValue(downcast<BasicColorMatrixFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::SEPIA: {
                filterValue = CSSFunctionValue::create(CSSValueSepia);
                filterValue->append(cssValuePool.createValue(downcast<BasicColorMatrixFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::SATURATE: {
                filterValue = CSSFunctionValue::create(CSSValueSaturate);
                filterValue->append(cssValuePool.createValue(downcast<BasicColorMatrixFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::HUE_ROTATE: {
                filterValue = CSSFunctionValue::create(CSSValueHueRotate);
                filterValue->append(cssValuePool.createValue(downcast<BasicColorMatrixFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_DEG));
                break;
            }
            case FilterOperation::INVERT: {
                filterValue = CSSFunctionValue::create(CSSValueInvert);
                filterValue->append(cssValuePool.createValue(downcast<BasicComponentTransferFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::APPLE_INVERT_LIGHTNESS: {
                filterValue = CSSFunctionValue::create(CSSValueAppleInvertLightness);
                break;
            }
            case FilterOperation::OPACITY: {
                filterValue = CSSFunctionValue::create(CSSValueOpacity);
                filterValue->append(cssValuePool.createValue(downcast<BasicComponentTransferFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::BRIGHTNESS: {
                filterValue = CSSFunctionValue::create(CSSValueBrightness);
                filterValue->append(cssValuePool.createValue(downcast<BasicComponentTransferFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::CONTRAST: {
                filterValue = CSSFunctionValue::create(CSSValueContrast);
                filterValue->append(cssValuePool.createValue(downcast<BasicComponentTransferFilterOperation>(filterOperation).amount(), CSSUnitType::CSS_NUMBER));
                break;
            }
            case FilterOperation::BLUR: {
                filterValue = CSSFunctionValue::create(CSSValueBlur);
                filterValue->append(adjustLengthForZoom(downcast<BlurFilterOperation>(filterOperation).stdDeviation(), style, adjust));
                break;
            }
            case FilterOperation::DROP_SHADOW: {
                DropShadowFilterOperation& dropShadowOperation = downcast<DropShadowFilterOperation>(filterOperation);
                filterValue = CSSFunctionValue::create(CSSValueDropShadow);
                // We want our computed style to look like that of a text shadow (has neither spread nor inset style).
                ShadowData shadowData = ShadowData(dropShadowOperation.location(), dropShadowOperation.stdDeviation(), 0, ShadowStyle::Normal, false, dropShadowOperation.color());
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
        return CSSValuePool::singleton().createValue(trackBreadth.flex(), CSSUnitType::CSS_FR);

    const Length& trackBreadthLength = trackBreadth.length();
    if (trackBreadthLength.isAuto())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
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
            return CSSValuePool::singleton().createValue(trackSize.maxTrackBreadth().flex(), CSSUnitType::CSS_FR);

        auto minMaxTrackBreadths = CSSFunctionValue::create(CSSValueMinmax);
        minMaxTrackBreadths->append(specifiedValueForGridTrackBreadth(trackSize.minTrackBreadth(), style));
        minMaxTrackBreadths->append(specifiedValueForGridTrackBreadth(trackSize.maxTrackBreadth(), style));
        return minMaxTrackBreadths;
    }
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

    bool isEmpty() const { return m_orderedNamedGridLines.isEmpty() && m_orderedNamedAutoRepeatGridLines.isEmpty(); }
    virtual void collectLineNamesForIndex(CSSGridLineNamesValue&, unsigned index) const;

protected:

    enum NamedLinesType { NamedLines, AutoRepeatNamedLines };
    void appendLines(CSSGridLineNamesValue&, unsigned index, NamedLinesType) const;

    const OrderedNamedGridLinesMap& m_orderedNamedGridLines;
    const OrderedNamedGridLinesMap& m_orderedNamedAutoRepeatGridLines;
};

class OrderedNamedLinesCollectorInsideRepeat : public OrderedNamedLinesCollector {
public:
    OrderedNamedLinesCollectorInsideRepeat(const RenderStyle& style, bool isRowAxis)
        : OrderedNamedLinesCollector(style, isRowAxis)
    {
    }

    void collectLineNamesForIndex(CSSGridLineNamesValue&, unsigned index) const override;
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

void OrderedNamedLinesCollector::appendLines(CSSGridLineNamesValue& lineNamesValue, unsigned index, NamedLinesType type) const
{
    auto iter = type == NamedLines ? m_orderedNamedGridLines.find(index) : m_orderedNamedAutoRepeatGridLines.find(index);
    auto endIter = type == NamedLines ? m_orderedNamedGridLines.end() : m_orderedNamedAutoRepeatGridLines.end();
    if (iter == endIter)
        return;

    auto& cssValuePool = CSSValuePool::singleton();
    for (const auto& lineName : iter->value)
        lineNamesValue.append(cssValuePool.createValue(lineName, CSSUnitType::CSS_STRING));
}

void OrderedNamedLinesCollector::collectLineNamesForIndex(CSSGridLineNamesValue& lineNamesValue, unsigned i) const
{
    ASSERT(!isEmpty());
    appendLines(lineNamesValue, i, NamedLines);
}

void OrderedNamedLinesCollectorInsideRepeat::collectLineNamesForIndex(CSSGridLineNamesValue& lineNamesValue, unsigned i) const
{
    ASSERT(!isEmpty());
    appendLines(lineNamesValue, i, AutoRepeatNamedLines);
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

static void addValuesForNamedGridLinesAtIndex(OrderedNamedLinesCollector& collector, unsigned i, CSSValueList& list)
{
    if (collector.isEmpty())
        return;

    auto lineNames = CSSGridLineNamesValue::create();
    collector.collectLineNamesForIndex(lineNames.get(), i);
    if (lineNames->length())
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
void populateGridTrackList(CSSValueList& list, OrderedNamedLinesCollector& collector, const Vector<T>& tracks, F getTrackSize, int start, int end, int offset = 0)
{
    ASSERT(0 <= start);
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

template <typename T, typename F>
void populateGridTrackList(CSSValueList& list, OrderedNamedLinesCollector& collector, const Vector<T>& tracks, F getTrackSize, int offset = 0)
{
    populateGridTrackList<T>(list, collector, tracks, getTrackSize, 0, tracks.size(), offset);
}

static Ref<CSSValue> valueForGridTrackList(GridTrackSizingDirection direction, RenderObject* renderer, const RenderStyle& style)
{
    bool isRowAxis = direction == ForColumns;
    bool isRenderGrid = is<RenderGrid>(renderer);
    auto& trackSizes = isRowAxis ? style.gridColumns() : style.gridRows();
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

    if (trackListIsEmpty)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();

    // If the element is a grid container, the resolved value is the used value,
    // specifying track sizes in pixels and expanding the repeat() notation.
    if (isRenderGrid) {
        auto* grid = downcast<RenderGrid>(renderer);
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
    OrderedNamedLinesCollector collector(style, isRowAxis);
    auto getTrackSize = [&](const GridTrackSize& v) {
        return specifiedValueForGridTrackSize(v, style);
    };

    if (autoRepeatTrackSizes.isEmpty()) {
        // If there's no auto repeat(), just add all the line names and track sizes.
        populateGridTrackList(list.get(), collector, trackSizes, getTrackSize);
        return list;
    }

    // Add the line names and track sizes that precede the auto repeat().
    int autoRepeatInsertionPoint = std::clamp<int>(isRowAxis ? style.gridAutoRepeatColumnsInsertionPoint() : style.gridAutoRepeatRowsInsertionPoint(), 0, trackSizes.size());
    populateGridTrackList(list.get(), collector, trackSizes, getTrackSize, 0, autoRepeatInsertionPoint);

    // Add a CSSGridAutoRepeatValue with the contents of the auto repeat().
    AutoRepeatType autoRepeatType = isRowAxis ? style.gridAutoRepeatColumnsType() : style.gridAutoRepeatRowsType();
    auto repeatedValues = CSSGridAutoRepeatValue::create(autoRepeatType == AutoRepeatType::Fill ? CSSValueAutoFill : CSSValueAutoFit);
    OrderedNamedLinesCollectorInsideRepeat repeatCollector(style, isRowAxis);
    populateGridTrackList(repeatedValues.get(), repeatCollector, autoRepeatTrackSizes, getTrackSize);
    list->append(repeatedValues.get());

    // Add the line names and track sizes that follow the auto repeat().
    populateGridTrackList(list.get(), collector, trackSizes, getTrackSize, autoRepeatInsertionPoint, trackSizes.size(), 1);
    return list;
}

static Ref<CSSValue> valueForGridPosition(const GridPosition& position)
{
    auto& cssValuePool = CSSValuePool::singleton();
    if (position.isAuto())
        return cssValuePool.createIdentifierValue(CSSValueAuto);

    if (position.isNamedGridArea())
        return cssValuePool.createValue(position.namedGridLine(), CSSUnitType::CSS_STRING);

    auto list = CSSValueList::createSpaceSeparated();
    if (position.isSpan()) {
        list->append(cssValuePool.createIdentifierValue(CSSValueSpan));
        list->append(cssValuePool.createValue(position.spanPosition(), CSSUnitType::CSS_NUMBER));
    } else
        list->append(cssValuePool.createValue(position.integerPosition(), CSSUnitType::CSS_NUMBER));

    if (!position.namedGridLine().isNull())
        list->append(cssValuePool.createValue(position.namedGridLine(), CSSUnitType::CSS_STRING));
    return list;
}

static Ref<CSSValue> createTransitionPropertyValue(const Animation& animation)
{
    switch (animation.property().mode) {
    case Animation::TransitionMode::None:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    case Animation::TransitionMode::All:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAll);
    case Animation::TransitionMode::SingleProperty:
        return CSSValuePool::singleton().createValue(getPropertyNameString(animation.property().id), CSSUnitType::CSS_STRING);
    case Animation::TransitionMode::UnknownProperty:
        return CSSValuePool::singleton().createValue(animation.unknownProperty(), CSSUnitType::CSS_STRING);
    }
    ASSERT_NOT_REACHED();
    return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
}

static Ref<CSSValueList> transitionPropertyValue(const AnimationList* animationList)
{
    auto list = CSSValueList::createCommaSeparated();
    if (animationList) {
        for (size_t i = 0; i < animationList->size(); ++i)
            list->append(createTransitionPropertyValue(animationList->animation(i)));
    } else
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueAll));
    return list;
}

#if ENABLE(CSS_SCROLL_SNAP)

static Ref<CSSValueList> valueForScrollSnapType(const ScrollSnapType& type)
{
    auto value = CSSValueList::createSpaceSeparated();
    if (type.strictness == ScrollSnapStrictness::None)
        value->append(CSSValuePool::singleton().createValue(CSSValueNone));
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
    value->append(CSSPrimitiveValue::create(alignment.y));
    if (alignment.x != alignment.y)
        value->append(CSSPrimitiveValue::create(alignment.x));
    return value;
}

#endif

static Ref<CSSValue> willChangePropertyValue(const WillChangeData* willChangeData)
{
    auto& cssValuePool = CSSValuePool::singleton();
    if (!willChangeData || !willChangeData->numFeatures())
        return cssValuePool.createIdentifierValue(CSSValueAuto);

    auto list = CSSValueList::createCommaSeparated();
    for (size_t i = 0; i < willChangeData->numFeatures(); ++i) {
        WillChangeData::FeaturePropertyPair feature = willChangeData->featureAt(i);
        switch (feature.first) {
        case WillChangeData::ScrollPosition:
            list->append(cssValuePool.createIdentifierValue(CSSValueScrollPosition));
            break;
        case WillChangeData::Contents:
            list->append(cssValuePool.createIdentifierValue(CSSValueContents));
            break;
        case WillChangeData::Property:
            list->append(cssValuePool.createIdentifierValue(feature.second));
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
        list.append(CSSValuePool::singleton().createIdentifierValue(noValue));
        return;
    case FontVariantLigatures::Yes:
        list.append(CSSValuePool::singleton().createIdentifierValue(yesValue));
        return;
    }
    ASSERT_NOT_REACHED();
}

static Ref<CSSValue> fontVariantLigaturesPropertyValue(FontVariantLigatures common, FontVariantLigatures discretionary, FontVariantLigatures historical, FontVariantLigatures contextualAlternates)
{
    auto& cssValuePool = CSSValuePool::singleton();
    if (common == FontVariantLigatures::No && discretionary == FontVariantLigatures::No && historical == FontVariantLigatures::No && contextualAlternates == FontVariantLigatures::No)
        return cssValuePool.createIdentifierValue(CSSValueNone);
    if (common == FontVariantLigatures::Normal && discretionary == FontVariantLigatures::Normal && historical == FontVariantLigatures::Normal && contextualAlternates == FontVariantLigatures::Normal)
        return cssValuePool.createIdentifierValue(CSSValueNormal);

    auto valueList = CSSValueList::createSpaceSeparated();
    appendLigaturesValue(valueList, common, CSSValueCommonLigatures, CSSValueNoCommonLigatures);
    appendLigaturesValue(valueList, discretionary, CSSValueDiscretionaryLigatures, CSSValueNoDiscretionaryLigatures);
    appendLigaturesValue(valueList, historical, CSSValueHistoricalLigatures, CSSValueNoHistoricalLigatures);
    appendLigaturesValue(valueList, contextualAlternates, CSSValueContextual, CSSValueNoContextual);
    return valueList;
}

static Ref<CSSValue> fontVariantPositionPropertyValue(FontVariantPosition position)
{
    auto& cssValuePool = CSSValuePool::singleton();
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
    return cssValuePool.createIdentifierValue(valueID);
}

static Ref<CSSValue> fontVariantCapsPropertyValue(FontVariantCaps caps)
{
    auto& cssValuePool = CSSValuePool::singleton();
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
    return cssValuePool.createIdentifierValue(valueID);
}

static Ref<CSSValue> fontVariantNumericPropertyValue(FontVariantNumericFigure figure, FontVariantNumericSpacing spacing, FontVariantNumericFraction fraction, FontVariantNumericOrdinal ordinal, FontVariantNumericSlashedZero slashedZero)
{
    auto& cssValuePool = CSSValuePool::singleton();
    if (figure == FontVariantNumericFigure::Normal && spacing == FontVariantNumericSpacing::Normal && fraction == FontVariantNumericFraction::Normal && ordinal == FontVariantNumericOrdinal::Normal && slashedZero == FontVariantNumericSlashedZero::Normal)
        return cssValuePool.createIdentifierValue(CSSValueNormal);

    auto valueList = CSSValueList::createSpaceSeparated();
    switch (figure) {
    case FontVariantNumericFigure::Normal:
        break;
    case FontVariantNumericFigure::LiningNumbers:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueLiningNums));
        break;
    case FontVariantNumericFigure::OldStyleNumbers:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueOldstyleNums));
        break;
    }

    switch (spacing) {
    case FontVariantNumericSpacing::Normal:
        break;
    case FontVariantNumericSpacing::ProportionalNumbers:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueProportionalNums));
        break;
    case FontVariantNumericSpacing::TabularNumbers:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueTabularNums));
        break;
    }

    switch (fraction) {
    case FontVariantNumericFraction::Normal:
        break;
    case FontVariantNumericFraction::DiagonalFractions:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueDiagonalFractions));
        break;
    case FontVariantNumericFraction::StackedFractions:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueStackedFractions));
        break;
    }

    if (ordinal == FontVariantNumericOrdinal::Yes)
        valueList->append(cssValuePool.createIdentifierValue(CSSValueOrdinal));
    if (slashedZero == FontVariantNumericSlashedZero::Yes)
        valueList->append(cssValuePool.createIdentifierValue(CSSValueSlashedZero));

    return valueList;
}

static Ref<CSSValue> fontVariantAlternatesPropertyValue(FontVariantAlternates alternates)
{
    auto& cssValuePool = CSSValuePool::singleton();
    CSSValueID valueID = CSSValueNormal;
    switch (alternates) {
    case FontVariantAlternates::Normal:
        break;
    case FontVariantAlternates::HistoricalForms:
        valueID = CSSValueHistoricalForms;
        break;
    }
    return cssValuePool.createIdentifierValue(valueID);
}

static Ref<CSSValue> fontVariantEastAsianPropertyValue(FontVariantEastAsianVariant variant, FontVariantEastAsianWidth width, FontVariantEastAsianRuby ruby)
{
    auto& cssValuePool = CSSValuePool::singleton();
    if (variant == FontVariantEastAsianVariant::Normal && width == FontVariantEastAsianWidth::Normal && ruby == FontVariantEastAsianRuby::Normal)
        return cssValuePool.createIdentifierValue(CSSValueNormal);

    auto valueList = CSSValueList::createSpaceSeparated();
    switch (variant) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueJis78));
        break;
    case FontVariantEastAsianVariant::Jis83:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueJis83));
        break;
    case FontVariantEastAsianVariant::Jis90:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueJis90));
        break;
    case FontVariantEastAsianVariant::Jis04:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueJis04));
        break;
    case FontVariantEastAsianVariant::Simplified:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueSimplified));
        break;
    case FontVariantEastAsianVariant::Traditional:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueTraditional));
        break;
    }

    switch (width) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueFullWidth));
        break;
    case FontVariantEastAsianWidth::Proportional:
        valueList->append(cssValuePool.createIdentifierValue(CSSValueProportionalWidth));
        break;
    }

    if (ruby == FontVariantEastAsianRuby::Yes)
        valueList->append(cssValuePool.createIdentifierValue(CSSValueRuby));

    return valueList;
}

static Ref<CSSValueList> delayValue(const AnimationList* animationList)
{
    auto& cssValuePool = CSSValuePool::singleton();
    auto list = CSSValueList::createCommaSeparated();
    if (animationList) {
        for (size_t i = 0; i < animationList->size(); ++i)
            list->append(cssValuePool.createValue(animationList->animation(i).delay(), CSSUnitType::CSS_S));
    } else {
        // Note that initialAnimationDelay() is used for both transitions and animations
        list->append(cssValuePool.createValue(Animation::initialDelay(), CSSUnitType::CSS_S));
    }
    return list;
}

static Ref<CSSValueList> durationValue(const AnimationList* animationList)
{
    auto& cssValuePool = CSSValuePool::singleton();
    auto list = CSSValueList::createCommaSeparated();
    if (animationList) {
        for (size_t i = 0; i < animationList->size(); ++i)
            list->append(cssValuePool.createValue(animationList->animation(i).duration(), CSSUnitType::CSS_S));
    } else {
        // Note that initialAnimationDuration() is used for both transitions and animations
        list->append(cssValuePool.createValue(Animation::initialDuration(), CSSUnitType::CSS_S));
    }
    return list;
}

static Ref<CSSValue> createTimingFunctionValue(const TimingFunction& timingFunction)
{
    switch (timingFunction.type()) {
    case TimingFunction::CubicBezierFunction: {
        auto& function = downcast<CubicBezierTimingFunction>(timingFunction);
        if (function.timingFunctionPreset() != CubicBezierTimingFunction::Custom) {
            CSSValueID valueId = CSSValueInvalid;
            switch (function.timingFunctionPreset()) {
            case CubicBezierTimingFunction::Ease:
                valueId = CSSValueEase;
                break;
            case CubicBezierTimingFunction::EaseIn:
                valueId = CSSValueEaseIn;
                break;
            case CubicBezierTimingFunction::EaseOut:
                valueId = CSSValueEaseOut;
                break;
            default:
                ASSERT(function.timingFunctionPreset() == CubicBezierTimingFunction::EaseInOut);
                valueId = CSSValueEaseInOut;
                break;
            }
            return CSSValuePool::singleton().createIdentifierValue(valueId);
        }
        return CSSCubicBezierTimingFunctionValue::create(function.x1(), function.y1(), function.x2(), function.y2());
    }
    case TimingFunction::StepsFunction: {
        auto& function = downcast<StepsTimingFunction>(timingFunction);
        return CSSStepsTimingFunctionValue::create(function.numberOfSteps(), function.stepPosition());
    }
    case TimingFunction::SpringFunction: {
        auto& function = downcast<SpringTimingFunction>(timingFunction);
        return CSSSpringTimingFunctionValue::create(function.mass(), function.stiffness(), function.damping(), function.initialVelocity());
    }
    default:
        ASSERT(timingFunction.type() == TimingFunction::LinearFunction);
        return CSSValuePool::singleton().createIdentifierValue(CSSValueLinear);
    }
}

static Ref<CSSValueList> timingFunctionValue(const AnimationList* animationList)
{
    auto list = CSSValueList::createCommaSeparated();
    if (animationList) {
        for (size_t i = 0; i < animationList->size(); ++i)
            list->append(createTimingFunctionValue(*animationList->animation(i).timingFunction()));
    } else
        // Note that initialAnimationTimingFunction() is used for both transitions and animations
        list->append(createTimingFunctionValue(Animation::initialTimingFunction()));
    return list;
}

static Ref<CSSValue> createLineBoxContainValue(OptionSet<LineBoxContain> lineBoxContain)
{
    if (!lineBoxContain)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
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

CSSComputedStyleDeclaration::CSSComputedStyleDeclaration(Element& element, bool allowVisitedStyle, StringView pseudoElementName)
    : m_element(element)
    , m_allowVisitedStyle(allowVisitedStyle)
{
    StringView name = pseudoElementName;
    if (name.startsWith(':'))
        name = name.substring(1);
    if (name.startsWith(':'))
        name = name.substring(1);
    m_pseudoElementSpecifier = CSSSelector::pseudoId(CSSSelector::parsePseudoElementType(name));
}

CSSComputedStyleDeclaration::~CSSComputedStyleDeclaration() = default;

Ref<CSSComputedStyleDeclaration> CSSComputedStyleDeclaration::create(Element& element, bool allowVisitedStyle, StringView pseudoElementName)
{
    return adoptRef(*new CSSComputedStyleDeclaration(element, allowVisitedStyle, pseudoElementName));
}

void CSSComputedStyleDeclaration::ref()
{
    ++m_refCount;
}

void CSSComputedStyleDeclaration::deref()
{
    ASSERT(m_refCount);
    if (!--m_refCount)
        delete this;
}

String CSSComputedStyleDeclaration::cssText() const
{
    StringBuilder result;
    for (unsigned i = 0; i < numComputedPropertyIDs; i++) {
        if (i)
            result.append(' ');
        result.append(getPropertyName(computedPropertyIDs[i]), ": ", getPropertyValue(computedPropertyIDs[i]), ';');
    }
    return result.toString();
}

ExceptionOr<void> CSSComputedStyleDeclaration::setCssText(const String&)
{
    return Exception { NoModificationAllowedError };
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
        return CSSValuePool::singleton().createIdentifierValue(sizeIdentifier);

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
        return CSSValuePool::singleton().createIdentifierValue(familyIdentifier);
    return CSSValuePool::singleton().createFontFamilyValue(family);
}

static Ref<CSSValue> touchActionFlagsToCSSValue(OptionSet<TouchAction> touchActions)
{
    auto& cssValuePool = CSSValuePool::singleton();

    if (touchActions & TouchAction::Auto)
        return cssValuePool.createIdentifierValue(CSSValueAuto);
    if (touchActions & TouchAction::None)
        return cssValuePool.createIdentifierValue(CSSValueNone);
    if (touchActions & TouchAction::Manipulation)
        return cssValuePool.createIdentifierValue(CSSValueManipulation);

    auto list = CSSValueList::createSpaceSeparated();
    if (touchActions & TouchAction::PanX)
        list->append(cssValuePool.createIdentifierValue(CSSValuePanX));
    if (touchActions & TouchAction::PanY)
        list->append(cssValuePool.createIdentifierValue(CSSValuePanY));
    if (touchActions & TouchAction::PinchZoom)
        list->append(cssValuePool.createIdentifierValue(CSSValuePinchZoom));

    if (!list->length())
        return cssValuePool.createIdentifierValue(CSSValueAuto);
    return list;
}

static Ref<CSSValue> renderTextDecorationFlagsToCSSValue(OptionSet<TextDecoration> textDecoration)
{
    auto& cssValuePool = CSSValuePool::singleton();
    // Blink value is ignored.
    auto list = CSSValueList::createSpaceSeparated();
    if (textDecoration & TextDecoration::Underline)
        list->append(cssValuePool.createIdentifierValue(CSSValueUnderline));
    if (textDecoration & TextDecoration::Overline)
        list->append(cssValuePool.createIdentifierValue(CSSValueOverline));
    if (textDecoration & TextDecoration::LineThrough)
        list->append(cssValuePool.createIdentifierValue(CSSValueLineThrough));
#if ENABLE(LETTERPRESS)
    if (textDecoration & TextDecoration::Letterpress)
        list->append(cssValuePool.createIdentifierValue(CSSValueWebkitLetterpress));
#endif

    if (!list->length())
        return cssValuePool.createIdentifierValue(CSSValueNone);
    return list;
}

static Ref<CSSValue> renderTextDecorationStyleFlagsToCSSValue(TextDecorationStyle textDecorationStyle)
{
    switch (textDecorationStyle) {
    case TextDecorationStyle::Solid:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueSolid);
    case TextDecorationStyle::Double:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueDouble);
    case TextDecorationStyle::Dotted:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueDotted);
    case TextDecorationStyle::Dashed:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueDashed);
    case TextDecorationStyle::Wavy:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueWavy);
    }

    ASSERT_NOT_REACHED();
    return CSSValuePool::singleton().createExplicitInitialValue();
}

static Ref<CSSValue> renderTextDecorationSkipFlagsToCSSValue(OptionSet<TextDecorationSkip> textDecorationSkip)
{
    // FIXME: This should probably return a CSSValueList with the set of all TextDecorationSkips.
    switch (static_cast<TextDecorationSkip>(textDecorationSkip.toRaw())) {
    case TextDecorationSkip::Auto:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    case TextDecorationSkip::None:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    case TextDecorationSkip::Ink:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueInk);
    case TextDecorationSkip::Objects:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueObjects);
    }

    ASSERT_NOT_REACHED();
    return CSSValuePool::singleton().createExplicitInitialValue();
}

static Ref<CSSValue> textUnderlineOffsetToCSSValue(const TextUnderlineOffset& textUnderlineOffset)
{
    if (textUnderlineOffset.isAuto())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    ASSERT(textUnderlineOffset.isLength());
    return CSSValuePool::singleton().createValue(textUnderlineOffset.lengthValue(), CSSUnitType::CSS_PX);
}

static Ref<CSSValue> textDecorationThicknessToCSSValue(const TextDecorationThickness& textDecorationThickness)
{
    if (textDecorationThickness.isAuto())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    if (textDecorationThickness.isFromFont())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueFromFont);
    ASSERT(textDecorationThickness.isLength());
    return CSSValuePool::singleton().createValue(textDecorationThickness.lengthValue(), CSSUnitType::CSS_PX);
}

static Ref<CSSValue> renderEmphasisPositionFlagsToCSSValue(OptionSet<TextEmphasisPosition> textEmphasisPosition)
{
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Over) && (textEmphasisPosition & TextEmphasisPosition::Under)));
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Left) && (textEmphasisPosition & TextEmphasisPosition::Right)));
    auto& cssValuePool = CSSValuePool::singleton();
    auto list = CSSValueList::createSpaceSeparated();
    if (textEmphasisPosition & TextEmphasisPosition::Over)
        list->append(cssValuePool.createIdentifierValue(CSSValueOver));
    if (textEmphasisPosition & TextEmphasisPosition::Under)
        list->append(cssValuePool.createIdentifierValue(CSSValueUnder));
    if (textEmphasisPosition & TextEmphasisPosition::Left)
        list->append(cssValuePool.createIdentifierValue(CSSValueLeft));
    if (textEmphasisPosition & TextEmphasisPosition::Right)
        list->append(cssValuePool.createIdentifierValue(CSSValueRight));
    if (!list->length())
        return cssValuePool.createIdentifierValue(CSSValueNone);
    return list;
}

static Ref<CSSValue> speakAsToCSSValue(OptionSet<SpeakAs> speakAs)
{
    auto& cssValuePool = CSSValuePool::singleton();
    auto list = CSSValueList::createSpaceSeparated();
    if (speakAs & SpeakAs::SpellOut)
        list->append(cssValuePool.createIdentifierValue(CSSValueSpellOut));
    if (speakAs & SpeakAs::Digits)
        list->append(cssValuePool.createIdentifierValue(CSSValueDigits));
    if (speakAs & SpeakAs::LiteralPunctuation)
        list->append(cssValuePool.createIdentifierValue(CSSValueLiteralPunctuation));
    if (speakAs & SpeakAs::NoPunctuation)
        list->append(cssValuePool.createIdentifierValue(CSSValueNoPunctuation));
    if (!list->length())
        return cssValuePool.createIdentifierValue(CSSValueNormal);
    return list;
}
    
static Ref<CSSValue> hangingPunctuationToCSSValue(OptionSet<HangingPunctuation> hangingPunctuation)
{
    auto& cssValuePool = CSSValuePool::singleton();
    auto list = CSSValueList::createSpaceSeparated();
    if (hangingPunctuation & HangingPunctuation::First)
        list->append(cssValuePool.createIdentifierValue(CSSValueFirst));
    if (hangingPunctuation & HangingPunctuation::AllowEnd)
        list->append(cssValuePool.createIdentifierValue(CSSValueAllowEnd));
    if (hangingPunctuation & HangingPunctuation::ForceEnd)
        list->append(cssValuePool.createIdentifierValue(CSSValueForceEnd));
    if (hangingPunctuation & HangingPunctuation::Last)
        list->append(cssValuePool.createIdentifierValue(CSSValueLast));
    if (!list->length())
        return cssValuePool.createIdentifierValue(CSSValueNone);
    return list;
}
    
static Ref<CSSValue> fillRepeatToCSSValue(FillRepeat xRepeat, FillRepeat yRepeat)
{
    // For backwards compatibility, if both values are equal, just return one of them. And
    // if the two values are equivalent to repeat-x or repeat-y, just return the shorthand.
    auto& cssValuePool = CSSValuePool::singleton();
    if (xRepeat == yRepeat)
        return cssValuePool.createValue(xRepeat);
    if (xRepeat == FillRepeat::Repeat && yRepeat == FillRepeat::NoRepeat)
        return cssValuePool.createIdentifierValue(CSSValueRepeatX);
    if (xRepeat == FillRepeat::NoRepeat && yRepeat == FillRepeat::Repeat)
        return cssValuePool.createIdentifierValue(CSSValueRepeatY);

    auto list = CSSValueList::createSpaceSeparated();
    list->append(cssValuePool.createValue(xRepeat));
    list->append(cssValuePool.createValue(yRepeat));
    return list;
}

static Ref<CSSValue> fillSourceTypeToCSSValue(MaskSourceType type)
{
    switch (type) {
    case MaskSourceType::Alpha:
        return CSSValuePool::singleton().createValue(CSSValueAlpha);
    default:
        ASSERT(type == MaskSourceType::Luminance);
        return CSSValuePool::singleton().createValue(CSSValueLuminance);
    }
}

static Ref<CSSValue> fillSizeToCSSValue(const FillSize& fillSize, const RenderStyle& style)
{
    if (fillSize.type == FillSizeType::Contain)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueContain);

    if (fillSize.type == FillSizeType::Cover)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueCover);

    if (fillSize.size.height.isAuto())
        return zoomAdjustedPixelValueForLength(fillSize.size.width, style);

    auto list = CSSValueList::createSpaceSeparated();
    list->append(zoomAdjustedPixelValueForLength(fillSize.size.width, style));
    list->append(zoomAdjustedPixelValueForLength(fillSize.size.height, style));
    return list;
}

static Ref<CSSValue> altTextToCSSValue(const RenderStyle& style)
{
    return CSSValuePool::singleton().createValue(style.contentAltText(), CSSUnitType::CSS_STRING);
}
    
static Ref<CSSValueList> contentToCSSValue(const RenderStyle& style)
{
    auto& cssValuePool = CSSValuePool::singleton();
    auto list = CSSValueList::createSpaceSeparated();
    for (auto* contentData = style.contentData(); contentData; contentData = contentData->next()) {
        if (is<CounterContentData>(*contentData))
            list->append(cssValuePool.createValue(downcast<CounterContentData>(*contentData).counter().identifier(), CSSUnitType::CSS_COUNTER_NAME));
        else if (is<ImageContentData>(*contentData))
            list->append(downcast<ImageContentData>(*contentData).image().cssValue());
        else if (is<TextContentData>(*contentData))
            list->append(cssValuePool.createValue(downcast<TextContentData>(*contentData).text(), CSSUnitType::CSS_STRING));
    }
    return list;
}

static Ref<CSSValue> counterToCSSValue(const RenderStyle& style, CSSPropertyID propertyID)
{
    auto* map = style.counterDirectives();
    if (!map)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    auto& cssValuePool = CSSValuePool::singleton();
    auto list = CSSValueList::createSpaceSeparated();
    for (auto& keyValue : *map) {
        list->append(cssValuePool.createValue(keyValue.key, CSSUnitType::CSS_STRING));
        double number = (propertyID == CSSPropertyCounterIncrement ? keyValue.value.incrementValue : keyValue.value.resetValue).valueOr(0);
        list->append(cssValuePool.createValue(number, CSSUnitType::CSS_NUMBER));
    }
    return list;
}

static Ref<CSSValueList> fontFamilyListFromStyle(const RenderStyle& style)
{
    auto list = CSSValueList::createCommaSeparated();
    for (unsigned i = 0; i < style.fontCascade().familyCount(); ++i)
        list->append(valueForFamily(style.fontCascade().familyAt(i)));
    return list;
}

static Ref<CSSValue> fontFamilyFromStyle(const RenderStyle& style)
{
    if (style.fontCascade().familyCount() == 1)
        return valueForFamily(style.fontCascade().familyAt(0));
    return fontFamilyListFromStyle(style);
}

static Ref<CSSPrimitiveValue> lineHeightFromStyle(const RenderStyle& style)
{
    Length length = style.lineHeight();
    if (length.isNegative())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);
    if (length.isPercent()) {
        // This is imperfect, because it doesn't include the zoom factor and the real computation
        // for how high to be in pixels does include things like minimum font size and the zoom factor.
        // On the other hand, since font-size doesn't include the zoom factor, we really can't do
        // that here either.
        return zoomAdjustedPixelValue(static_cast<double>(length.percent() * style.fontDescription().computedSize()) / 100, style);
    }
    return zoomAdjustedPixelValue(floatValueForLength(length, 0), style);
}

static Ref<CSSPrimitiveValue> fontSizeFromStyle(const RenderStyle& style)
{
    return zoomAdjustedPixelValue(style.fontDescription().computedSize(), style);
}

Ref<CSSPrimitiveValue> ComputedStyleExtractor::fontNonKeywordWeightFromStyleValue(FontSelectionValue weight)
{
    return CSSValuePool::singleton().createValue(static_cast<float>(weight), CSSUnitType::CSS_NUMBER);
}

Ref<CSSPrimitiveValue> ComputedStyleExtractor::fontWeightFromStyleValue(FontSelectionValue weight)
{
    if (auto value = fontWeightKeyword(weight))
        return CSSValuePool::singleton().createIdentifierValue(value.value());
    return fontNonKeywordWeightFromStyleValue(weight);
}

static Ref<CSSPrimitiveValue> fontWeightFromStyle(const RenderStyle& style)
{
    return ComputedStyleExtractor::fontWeightFromStyleValue(style.fontDescription().weight());
}

Ref<CSSPrimitiveValue> ComputedStyleExtractor::fontNonKeywordStretchFromStyleValue(FontSelectionValue stretch)
{
    return CSSValuePool::singleton().createValue(static_cast<float>(stretch), CSSUnitType::CSS_PERCENTAGE);
}

Ref<CSSPrimitiveValue> ComputedStyleExtractor::fontStretchFromStyleValue(FontSelectionValue stretch)
{
    if (auto keyword = fontStretchKeyword(stretch))
        return CSSValuePool::singleton().createIdentifierValue(keyword.value());
    return fontNonKeywordStretchFromStyleValue(stretch);
}

static Ref<CSSPrimitiveValue> fontStretchFromStyle(const RenderStyle& style)
{
    return ComputedStyleExtractor::fontStretchFromStyleValue(style.fontDescription().stretch());
}

Ref<CSSFontStyleValue> ComputedStyleExtractor::fontNonKeywordStyleFromStyleValue(FontSelectionValue italic)
{
    return CSSFontStyleValue::create(CSSValuePool::singleton().createIdentifierValue(CSSValueOblique), CSSValuePool::singleton().createValue(static_cast<float>(italic), CSSUnitType::CSS_DEG));
}

Ref<CSSFontStyleValue> ComputedStyleExtractor::fontStyleFromStyleValue(Optional<FontSelectionValue> italic, FontStyleAxis fontStyleAxis)
{
    if (auto keyword = fontStyleKeyword(italic, fontStyleAxis))
        return CSSFontStyleValue::create(CSSValuePool::singleton().createIdentifierValue(keyword.value()));
    return fontNonKeywordStyleFromStyleValue(italic.value());
}

static Ref<CSSFontStyleValue> fontStyleFromStyle(const RenderStyle& style)
{
    return ComputedStyleExtractor::fontStyleFromStyleValue(style.fontDescription().italic(), style.fontDescription().fontStyleAxis());
}

static Ref<CSSValue> fontVariantFromStyle(const RenderStyle& style)
{
    if (style.fontDescription().variantSettings().isAllNormal())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);

    auto list = CSSValueList::createSpaceSeparated();

    switch (style.fontDescription().variantCommonLigatures()) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueCommonLigatures));
        break;
    case FontVariantLigatures::No:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoCommonLigatures));
        break;
    }

    switch (style.fontDescription().variantDiscretionaryLigatures()) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueDiscretionaryLigatures));
        break;
    case FontVariantLigatures::No:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoDiscretionaryLigatures));
        break;
    }

    switch (style.fontDescription().variantHistoricalLigatures()) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueHistoricalLigatures));
        break;
    case FontVariantLigatures::No:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoHistoricalLigatures));
        break;
    }

    switch (style.fontDescription().variantContextualAlternates()) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueContextual));
        break;
    case FontVariantLigatures::No:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoContextual));
        break;
    }

    switch (style.fontDescription().variantPosition()) {
    case FontVariantPosition::Normal:
        break;
    case FontVariantPosition::Subscript:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueSub));
        break;
    case FontVariantPosition::Superscript:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueSuper));
        break;
    }

    switch (style.fontDescription().variantCaps()) {
    case FontVariantCaps::Normal:
        break;
    case FontVariantCaps::Small:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueSmallCaps));
        break;
    case FontVariantCaps::AllSmall:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueAllSmallCaps));
        break;
    case FontVariantCaps::Petite:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValuePetiteCaps));
        break;
    case FontVariantCaps::AllPetite:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueAllPetiteCaps));
        break;
    case FontVariantCaps::Unicase:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueUnicase));
        break;
    case FontVariantCaps::Titling:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueTitlingCaps));
        break;
    }

    switch (style.fontDescription().variantNumericFigure()) {
    case FontVariantNumericFigure::Normal:
        break;
    case FontVariantNumericFigure::LiningNumbers:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueLiningNums));
        break;
    case FontVariantNumericFigure::OldStyleNumbers:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueOldstyleNums));
        break;
    }

    switch (style.fontDescription().variantNumericSpacing()) {
    case FontVariantNumericSpacing::Normal:
        break;
    case FontVariantNumericSpacing::ProportionalNumbers:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueProportionalNums));
        break;
    case FontVariantNumericSpacing::TabularNumbers:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueTabularNums));
        break;
    }

    switch (style.fontDescription().variantNumericFraction()) {
    case FontVariantNumericFraction::Normal:
        break;
    case FontVariantNumericFraction::DiagonalFractions:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueDiagonalFractions));
        break;
    case FontVariantNumericFraction::StackedFractions:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueStackedFractions));
        break;
    }

    switch (style.fontDescription().variantNumericOrdinal()) {
    case FontVariantNumericOrdinal::Normal:
        break;
    case FontVariantNumericOrdinal::Yes:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueOrdinal));
        break;
    }

    switch (style.fontDescription().variantNumericSlashedZero()) {
    case FontVariantNumericSlashedZero::Normal:
        break;
    case FontVariantNumericSlashedZero::Yes:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueSlashedZero));
        break;
    }

    switch (style.fontDescription().variantAlternates()) {
    case FontVariantAlternates::Normal:
        break;
    case FontVariantAlternates::HistoricalForms:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueHistoricalForms));
        break;
    }

    switch (style.fontDescription().variantEastAsianVariant()) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis78));
        break;
    case FontVariantEastAsianVariant::Jis83:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis83));
        break;
    case FontVariantEastAsianVariant::Jis90:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis90));
        break;
    case FontVariantEastAsianVariant::Jis04:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis04));
        break;
    case FontVariantEastAsianVariant::Simplified:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueSimplified));
        break;
    case FontVariantEastAsianVariant::Traditional:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueTraditional));
        break;
    }

    switch (style.fontDescription().variantEastAsianWidth()) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueFullWidth));
        break;
    case FontVariantEastAsianWidth::Proportional:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueProportionalWidth));
        break;
    }

    switch (style.fontDescription().variantEastAsianRuby()) {
    case FontVariantEastAsianRuby::Normal:
        break;
    case FontVariantEastAsianRuby::Yes:
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueRuby));
        break;
    }

    return list;
}

static Ref<CSSValue> fontSynthesisFromStyle(const RenderStyle& style)
{
    if (style.fontDescription().fontSynthesis() == FontSynthesisNone)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();
    if (style.fontDescription().fontSynthesis() & FontSynthesisStyle)
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueStyle));
    if (style.fontDescription().fontSynthesis() & FontSynthesisWeight)
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueWeight));
    if (style.fontDescription().fontSynthesis() & FontSynthesisSmallCaps)
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueSmallCaps));
    return list;
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
    return renderer.isInline() && !renderer.isReplaced();
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
        return renderer && !renderer->isRenderSVGModelObject() && !isNonReplacedInline(*renderer);
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

Element* ComputedStyleExtractor::styledElement() const
{
    if (!m_element)
        return nullptr;
    PseudoElement* pseudoElement;
    if (m_pseudoElementSpecifier == PseudoId::Before && (pseudoElement = m_element->beforePseudoElement()))
        return pseudoElement;
    if (m_pseudoElementSpecifier == PseudoId::After && (pseudoElement = m_element->afterPseudoElement()))
        return pseudoElement;
    return m_element.get();
}

RenderElement* ComputedStyleExtractor::styledRenderer() const
{
    auto* element = styledElement();
    if (!element)
        return nullptr;
    if (m_pseudoElementSpecifier != PseudoId::None && element == m_element.get())
        return nullptr;
    if (element->hasDisplayContents())
        return nullptr;
    return element->renderer();
}

static bool isImplicitlyInheritedGridOrFlexProperty(CSSPropertyID propertyID)
{
    // It would be nice if grid and flex worked within normal CSS mechanisms and not invented their own inheritance system.
    switch (propertyID) {
    case CSSPropertyAlignSelf:
    case CSSPropertyJustifySelf:
    case CSSPropertyJustifyItems:
    // FIXME: In StyleResolver::adjustRenderStyle z-index is adjusted based on the parent display property for grid/flex.
    case CSSPropertyZIndex:
        return true;
    default:
        return false;
    }
}

static bool nonInheritedColorPropertyHasValueCurrentColor(CSSPropertyID propertyID, const RenderStyle* style)
{
    if (CSSProperty::isInheritedProperty(propertyID) || !CSSProperty::isColorProperty(propertyID))
        return false;

    if (!style)
        return true;

    return RenderStyle::isCurrentColor(style->unresolvedColorForProperty(propertyID));
}

// In CSS 2.1 the returned object should actually contain the "used values"
// rather then the "computed values" (despite the name saying otherwise).
//
// See;
// http://www.w3.org/TR/CSS21/cascade.html#used-value
// http://www.w3.org/TR/DOM-Level-2-Style/css.html#CSS-CSSStyleDeclaration
// https://developer.mozilla.org/en-US/docs/Web/API/Window/getComputedStyle#Notes
RefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(CSSPropertyID propertyID, EUpdateLayout updateLayout) const
{
    return ComputedStyleExtractor(m_element.ptr(), m_allowVisitedStyle, m_pseudoElementSpecifier).propertyValue(propertyID, updateLayout);
}

Ref<MutableStyleProperties> CSSComputedStyleDeclaration::copyProperties() const
{
    return ComputedStyleExtractor(m_element.ptr(), m_allowVisitedStyle, m_pseudoElementSpecifier).copyProperties();
}

const Settings* CSSComputedStyleDeclaration::settings() const
{
    return &m_element->document().settings();
}

static inline bool hasValidStyleForProperty(Element& element, CSSPropertyID propertyID)
{
    if (element.styleValidity() != Style::Validity::Valid)
        return false;
    if (element.document().hasPendingFullStyleRebuild())
        return false;
    if (!element.document().childNeedsStyleRecalc())
        return true;

    bool isInherited = CSSProperty::isInheritedProperty(propertyID) || isImplicitlyInheritedGridOrFlexProperty(propertyID);
    bool maybeExplicitlyInherited = !isInherited;

    const auto* currentElement = &element;
    for (auto& ancestor : composedTreeAncestors(element)) {
        if (ancestor.styleValidity() >= Style::Validity::SubtreeInvalid)
            return false;

        if (maybeExplicitlyInherited) {
            auto* style = currentElement->renderStyle();
            // While most color properties are not inherited, the value 'currentcolor' resolves to the value of the inherited 'color' property.
            if (nonInheritedColorPropertyHasValueCurrentColor(propertyID, style))
                isInherited = true;

            maybeExplicitlyInherited = !style || style->hasExplicitlyInheritedProperties();
        }

        if ((isInherited || maybeExplicitlyInherited) && ancestor.styleValidity() == Style::Validity::ElementInvalid)
            return false;

        if (ancestor.directChildNeedsStyleRecalc() && currentElement->styleIsAffectedByPreviousSibling())
            return false;

        currentElement = &ancestor;
    }

    return true;
}

static bool updateStyleIfNeededForProperty(Element& element, CSSPropertyID propertyID)
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

static inline const RenderStyle* computeRenderStyleForProperty(Element& element, PseudoId pseudoElementSpecifier, CSSPropertyID propertyID, std::unique_ptr<RenderStyle>& ownedStyle)
{
    auto* renderer = element.renderer();

    if (renderer && renderer->isComposited() && CSSPropertyAnimation::animationOfPropertyIsAccelerated(propertyID)) {
        ownedStyle = renderer->animatedStyle();
        if (pseudoElementSpecifier != PseudoId::None && !element.isPseudoElement()) {
            // FIXME: This cached pseudo style will only exist if the animation has been run at least once.
            return ownedStyle->getCachedPseudoStyle(pseudoElementSpecifier);
        }
        return ownedStyle.get();
    }

    return element.computedStyle(element.isPseudoElement() ? PseudoId::None : pseudoElementSpecifier);
}

static Ref<CSSValue> shapePropertyValue(const RenderStyle& style, const ShapeValue* shapeValue)
{
    if (!shapeValue)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    if (shapeValue->type() == ShapeValue::Type::Box)
        return CSSValuePool::singleton().createValue(shapeValue->cssBox());

    if (shapeValue->type() == ShapeValue::Type::Image) {
        if (shapeValue->image())
            return shapeValue->image()->cssValue();
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    }

    ASSERT(shapeValue->type() == ShapeValue::Type::Shape);

    auto list = CSSValueList::createSpaceSeparated();
    list->append(valueForBasicShape(style, *shapeValue->shape()));
    if (shapeValue->cssBox() != CSSBoxType::BoxMissing)
        list->append(CSSValuePool::singleton().createValue(shapeValue->cssBox()));
    return list;
}

static Ref<CSSValueList> valueForItemPositionWithOverflowAlignment(const StyleSelfAlignmentData& data)
{
    auto& cssValuePool = CSSValuePool::singleton();
    auto result = CSSValueList::createSpaceSeparated();
    if (data.positionType() == ItemPositionType::Legacy)
        result->append(cssValuePool.createIdentifierValue(CSSValueLegacy));
    if (data.position() == ItemPosition::Baseline)
        result->append(cssValuePool.createIdentifierValue(CSSValueBaseline));
    else if (data.position() == ItemPosition::LastBaseline) {
        result->append(cssValuePool.createIdentifierValue(CSSValueLast));
        result->append(cssValuePool.createIdentifierValue(CSSValueBaseline));
    } else {
        if (data.position() >= ItemPosition::Center && data.overflow() != OverflowAlignment::Default)
            result->append(cssValuePool.createValue(data.overflow()));
        if (data.position() == ItemPosition::Legacy)
            result->append(cssValuePool.createIdentifierValue(CSSValueNormal));
        else
            result->append(cssValuePool.createValue(data.position()));
    }
    ASSERT(result->length() <= 2);
    return result;
}

static Ref<CSSValueList> valueForContentPositionAndDistributionWithOverflowAlignment(const StyleContentAlignmentData& data)
{
    auto& cssValuePool = CSSValuePool::singleton();
    auto result = CSSValueList::createSpaceSeparated();
    // Handle content-distribution values
    if (data.distribution() != ContentDistribution::Default)
        result->append(cssValuePool.createValue(data.distribution()));

    // Handle content-position values (either as fallback or actual value)
    switch (data.position()) {
    case ContentPosition::Normal:
        // Handle 'normal' value, not valid as content-distribution fallback.
        if (data.distribution() == ContentDistribution::Default)
            result->append(cssValuePool.createIdentifierValue(CSSValueNormal));
        break;
    case ContentPosition::LastBaseline:
        result->append(cssValuePool.createIdentifierValue(CSSValueLast));
        result->append(cssValuePool.createIdentifierValue(CSSValueBaseline));
        break;
    default:
        // Handle overflow-alignment (only allowed for content-position values)
        if ((data.position() >= ContentPosition::Center || data.distribution() != ContentDistribution::Default) && data.overflow() != OverflowAlignment::Default)
            result->append(cssValuePool.createValue(data.overflow()));
        result->append(cssValuePool.createValue(data.position()));
    }

    ASSERT(result->length() > 0);
    ASSERT(result->length() <= 3);
    return result;
}

static Ref<CSSValue> paintOrder(PaintOrder paintOrder)
{
    if (paintOrder == PaintOrder::Normal)
        return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
    
    auto paintOrderList = CSSValueList::createSpaceSeparated();
    switch (paintOrder) {
    case PaintOrder::Normal:
        ASSERT_NOT_REACHED();
        break;
    case PaintOrder::Fill:
        paintOrderList->append(CSSPrimitiveValue::createIdentifier(CSSValueFill));
        break;
    case PaintOrder::FillMarkers:
        paintOrderList->append(CSSPrimitiveValue::createIdentifier(CSSValueFill));
        paintOrderList->append(CSSPrimitiveValue::createIdentifier(CSSValueMarkers));
        break;
    case PaintOrder::Stroke:
        paintOrderList->append(CSSPrimitiveValue::createIdentifier(CSSValueStroke));
        break;
    case PaintOrder::StrokeMarkers:
        paintOrderList->append(CSSPrimitiveValue::createIdentifier(CSSValueStroke));
        paintOrderList->append(CSSPrimitiveValue::createIdentifier(CSSValueMarkers));
        break;
    case PaintOrder::Markers:
        paintOrderList->append(CSSPrimitiveValue::createIdentifier(CSSValueMarkers));
        break;
    case PaintOrder::MarkersStroke:
        paintOrderList->append(CSSPrimitiveValue::createIdentifier(CSSValueMarkers));
        paintOrderList->append(CSSPrimitiveValue::createIdentifier(CSSValueStroke));
        break;
    }
    return paintOrderList;
}

inline static bool isFlexOrGridItem(RenderObject* renderer)
{
    if (!renderer || !renderer->isBox())
        return false;
    auto& box = downcast<RenderBox>(*renderer);
    return box.isFlexItem() || box.isGridItem();
}

RefPtr<CSSValue> ComputedStyleExtractor::customPropertyValue(const String& propertyName)
{
    Element* styledElement = this->styledElement();
    if (!styledElement)
        return nullptr;
    
    if (updateStyleIfNeededForProperty(*styledElement, CSSPropertyCustom)) {
        // Style update may change styledElement() to PseudoElement or back.
        styledElement = this->styledElement();
    }

    std::unique_ptr<RenderStyle> ownedStyle;
    auto* style = computeRenderStyleForProperty(*styledElement, m_pseudoElementSpecifier, CSSPropertyCustom, ownedStyle);
    if (!style)
        return nullptr;

    auto* registered = styledElement->document().getCSSRegisteredCustomPropertySet().get(propertyName);
    auto* value = style->getCustomProperty(propertyName);

    if (registered && !value)
        return registered->initialValueCopy();

    if (!value)
        return nullptr;

    return WTF::switchOn(value->value(), [&](const Ref<CSSVariableReferenceValue>&) {
        ASSERT_NOT_REACHED();
        return RefPtr<CSSValue>();
    }, [&](const CSSValueID&) {
        return CSSCustomPropertyValue::create(*value);
    }, [&](const Ref<CSSVariableData>&) {
        return CSSCustomPropertyValue::create(*value);
    }, [&](const Length& value) {
        return zoomAdjustedPixelValueForLength(value, *style);
    }, [&](const Ref<StyleImage>&) {
        return CSSCustomPropertyValue::create(*value);
    });
}

String ComputedStyleExtractor::customPropertyText(const String& propertyName)
{
    RefPtr<CSSValue> propertyValue = customPropertyValue(propertyName);
    return propertyValue ? propertyValue->cssText() : emptyString();
}

static Ref<CSSFontValue> fontShorthandValueForSelectionProperties(const FontDescription& fontDescription)
{
    auto computedFont = CSSFontValue::create();

    auto variantCaps = fontDescription.variantCaps();
    if (variantCaps == FontVariantCaps::Small)
        computedFont->variant = CSSValuePool::singleton().createIdentifierValue(CSSValueSmallCaps);
    else if (variantCaps == FontVariantCaps::Normal)
        computedFont->variant = CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);
    else
        return CSSFontValue::create();

    auto weight = fontDescription.weight();
    if (auto value = fontWeightKeyword(weight))
        computedFont->weight = CSSValuePool::singleton().createIdentifierValue(value.value());
    else if (isCSS21Weight(weight))
        computedFont->weight = CSSValuePool::singleton().createValue(static_cast<float>(weight), CSSUnitType::CSS_NUMBER);
    else
        return CSSFontValue::create();

    if (auto keyword = fontStretchKeyword(fontDescription.stretch()))
        computedFont->stretch = CSSValuePool::singleton().createIdentifierValue(keyword.value());
    else
        return CSSFontValue::create();

    if (auto italic = fontStyleKeyword(fontDescription.italic(), fontDescription.fontStyleAxis()))
        computedFont->style = CSSFontStyleValue::create(CSSValuePool::singleton().createIdentifierValue(italic.value()));
    else
        return CSSFontValue::create();

    return computedFont;
}

RefPtr<CSSValue> ComputedStyleExtractor::propertyValue(CSSPropertyID propertyID, EUpdateLayout updateLayout)
{
    auto* styledElement = this->styledElement();
    if (!styledElement)
        return nullptr;

    std::unique_ptr<RenderStyle> ownedStyle;
    const RenderStyle* style = nullptr;
    RenderElement* renderer = nullptr;
    bool forceFullLayout = false;
    if (updateLayout) {
        Document& document = m_element->document();

        if (updateStyleIfNeededForProperty(*styledElement, propertyID)) {
            // Style update may change styledElement() to PseudoElement or back.
            styledElement = this->styledElement();
        }
        renderer = styledRenderer();

        if (propertyID == CSSPropertyDisplay && !renderer && is<SVGElement>(*styledElement) && !downcast<SVGElement>(*styledElement).isValid())
            return nullptr;

        style = computeRenderStyleForProperty(*styledElement, m_pseudoElementSpecifier, propertyID, ownedStyle);

        // FIXME: Some of these cases could be narrowed down or optimized better.
        forceFullLayout = isLayoutDependent(propertyID, style, renderer)
            || styledElement->isInShadowTree()
            || (document.styleScope().resolverIfExists() && document.styleScope().resolverIfExists()->hasViewportDependentMediaQueries() && document.ownerElement());

        if (forceFullLayout) {
            document.updateLayoutIgnorePendingStylesheets();
            styledElement = this->styledElement();
        }
    }

    if (!updateLayout || forceFullLayout) {
        style = computeRenderStyleForProperty(*styledElement, m_pseudoElementSpecifier, propertyID, ownedStyle);
        renderer = styledRenderer();
    }

    if (!style)
        return nullptr;

    return valueForPropertyInStyle(*style, propertyID, renderer);
}

RefPtr<CSSValue> ComputedStyleExtractor::valueForPropertyInStyle(const RenderStyle& style, CSSPropertyID propertyID, RenderElement* renderer)
{
    auto& cssValuePool = CSSValuePool::singleton();
    propertyID = CSSProperty::resolveDirectionAwareProperty(propertyID, style.direction(), style.writingMode());

    switch (propertyID) {
        case CSSPropertyInvalid:
#if ENABLE(TEXT_AUTOSIZING)
        case CSSPropertyInternalTextAutosizingStatus:
#endif
            break;

        case CSSPropertyBackgroundColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyBackgroundColor)) : currentColorOrValidColor(&style, style.backgroundColor());
        case CSSPropertyBackgroundImage:
        case CSSPropertyWebkitMaskImage: {
            auto& layers = propertyID == CSSPropertyWebkitMaskImage ? style.maskLayers() : style.backgroundLayers();
            if (!layers.next()) {
                if (layers.image())
                    return layers.image()->cssValue();
                return cssValuePool.createIdentifierValue(CSSValueNone);
            }
            auto list = CSSValueList::createCommaSeparated();
            for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next()) {
                if (currLayer->image())
                    list->append(currLayer->image()->cssValue());
                else
                    list->append(cssValuePool.createIdentifierValue(CSSValueNone));
            }
            return list;
        }
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize:
        case CSSPropertyWebkitMaskSize: {
            auto& layers = propertyID == CSSPropertyWebkitMaskSize ? style.maskLayers() : style.backgroundLayers();
            if (!layers.next())
                return fillSizeToCSSValue(layers.size(), style);
            auto list = CSSValueList::createCommaSeparated();
            for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
                list->append(fillSizeToCSSValue(currLayer->size(), style));
            return list;
        }
        case CSSPropertyBackgroundRepeat:
        case CSSPropertyWebkitMaskRepeat: {
            auto& layers = propertyID == CSSPropertyWebkitMaskRepeat ? style.maskLayers() : style.backgroundLayers();
            if (!layers.next())
                return fillRepeatToCSSValue(layers.repeatX(), layers.repeatY());
            auto list = CSSValueList::createCommaSeparated();
            for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
                list->append(fillRepeatToCSSValue(currLayer->repeatX(), currLayer->repeatY()));
            return list;
        }
        case CSSPropertyWebkitMaskSourceType: {
            auto& layers = style.maskLayers();
            if (!layers.next())
                return fillSourceTypeToCSSValue(layers.maskSourceType());
            auto list = CSSValueList::createCommaSeparated();
            for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
                list->append(fillSourceTypeToCSSValue(currLayer->maskSourceType()));
            return list;
        }
        case CSSPropertyWebkitBackgroundComposite:
        case CSSPropertyWebkitMaskComposite: {
            auto& layers = propertyID == CSSPropertyWebkitMaskComposite ? style.maskLayers() : style.backgroundLayers();
            if (!layers.next())
                return cssValuePool.createValue(layers.composite());
            auto list = CSSValueList::createCommaSeparated();
            for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool.createValue(currLayer->composite()));
            return list;
        }
        case CSSPropertyBackgroundAttachment: {
            auto& layers = style.backgroundLayers();
            if (!layers.next())
                return cssValuePool.createValue(layers.attachment());
            auto list = CSSValueList::createCommaSeparated();
            for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool.createValue(currLayer->attachment()));
            return list;
        }
        case CSSPropertyBackgroundClip:
        case CSSPropertyBackgroundOrigin:
        case CSSPropertyWebkitBackgroundClip:
        case CSSPropertyWebkitBackgroundOrigin:
        case CSSPropertyWebkitMaskClip:
        case CSSPropertyWebkitMaskOrigin: {
            auto& layers = (propertyID == CSSPropertyWebkitMaskClip || propertyID == CSSPropertyWebkitMaskOrigin) ? style.maskLayers() : style.backgroundLayers();
            bool isClip = propertyID == CSSPropertyBackgroundClip || propertyID == CSSPropertyWebkitBackgroundClip || propertyID == CSSPropertyWebkitMaskClip;
            if (!layers.next())
                return cssValuePool.createValue(isClip ? layers.clip() : layers.origin());
            auto list = CSSValueList::createCommaSeparated();
            for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool.createValue(isClip ? currLayer->clip() : currLayer->origin()));
            return list;
        }
        case CSSPropertyBackgroundPosition:
        case CSSPropertyWebkitMaskPosition: {
            auto& layers = propertyID == CSSPropertyWebkitMaskPosition ? style.maskLayers() : style.backgroundLayers();
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
                return cssValuePool.createValue(layers.xPosition());

            auto list = CSSValueList::createCommaSeparated();
            for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool.createValue(currLayer->xPosition()));

            return list;
        }
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY: {
            auto& layers = propertyID == CSSPropertyWebkitMaskPositionY ? style.maskLayers() : style.backgroundLayers();
            if (!layers.next())
                return cssValuePool.createValue(layers.yPosition());

            auto list = CSSValueList::createCommaSeparated();
            for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool.createValue(currLayer->yPosition()));

            return list;
        }
        case CSSPropertyBorderCollapse:
            if (style.borderCollapse() == BorderCollapse::Collapse)
                return cssValuePool.createIdentifierValue(CSSValueCollapse);
            return cssValuePool.createIdentifierValue(CSSValueSeparate);
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
                return style.borderImageSource()->cssValue();
            return cssValuePool.createIdentifierValue(CSSValueNone);
        case CSSPropertyBorderTopColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyBorderTopColor)) : currentColorOrValidColor(&style, style.borderTopColor());
        case CSSPropertyBorderRightColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyBorderRightColor)) : currentColorOrValidColor(&style, style.borderRightColor());
        case CSSPropertyBorderBottomColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyBorderBottomColor)) : currentColorOrValidColor(&style, style.borderBottomColor());
        case CSSPropertyBorderLeftColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyBorderLeftColor)) : currentColorOrValidColor(&style, style.borderLeftColor());
        case CSSPropertyBorderTopStyle:
            return cssValuePool.createValue(style.borderTopStyle());
        case CSSPropertyBorderRightStyle:
            return cssValuePool.createValue(style.borderRightStyle());
        case CSSPropertyBorderBottomStyle:
            return cssValuePool.createValue(style.borderBottomStyle());
        case CSSPropertyBorderLeftStyle:
            return cssValuePool.createValue(style.borderLeftStyle());
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
            return cssValuePool.createValue(style.boxAlign());
#if ENABLE(CSS_BOX_DECORATION_BREAK)
        case CSSPropertyWebkitBoxDecorationBreak:
            if (style.boxDecorationBreak() == BoxDecorationBreak::Slice)
                return cssValuePool.createIdentifierValue(CSSValueSlice);
        return cssValuePool.createIdentifierValue(CSSValueClone);
#endif
        case CSSPropertyWebkitBoxDirection:
            return cssValuePool.createValue(style.boxDirection());
        case CSSPropertyWebkitBoxFlex:
            return cssValuePool.createValue(style.boxFlex(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyWebkitBoxFlexGroup:
            return cssValuePool.createValue(style.boxFlexGroup(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyWebkitBoxLines:
            return cssValuePool.createValue(style.boxLines());
        case CSSPropertyWebkitBoxOrdinalGroup:
            return cssValuePool.createValue(style.boxOrdinalGroup(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyWebkitBoxOrient:
            return cssValuePool.createValue(style.boxOrient());
        case CSSPropertyWebkitBoxPack:
            return cssValuePool.createValue(style.boxPack());
        case CSSPropertyWebkitBoxReflect:
            return valueForReflection(style.boxReflect(), style);
        case CSSPropertyBoxShadow:
        case CSSPropertyWebkitBoxShadow:
            return valueForShadow(style.boxShadow(), propertyID, style);
        case CSSPropertyCaptionSide:
            return cssValuePool.createValue(style.captionSide());
        case CSSPropertyCaretColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyCaretColor)) : currentColorOrValidColor(&style, style.caretColor());
        case CSSPropertyClear:
            return cssValuePool.createValue(style.clear());
        case CSSPropertyColor:
            return cssValuePool.createColorValue(m_allowVisitedStyle ? style.visitedDependentColor(CSSPropertyColor) : style.color());
        case CSSPropertyWebkitPrintColorAdjust:
            return cssValuePool.createValue(style.printColorAdjust());
        case CSSPropertyWebkitColumnAxis:
            return cssValuePool.createValue(style.columnAxis());
        case CSSPropertyColumnCount:
            if (style.hasAutoColumnCount())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style.columnCount(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyColumnFill:
            return cssValuePool.createValue(style.columnFill());
        case CSSPropertyColumnGap:
            if (style.columnGap().isNormal())
                return cssValuePool.createIdentifierValue(CSSValueNormal);
            return zoomAdjustedPixelValueForLength(style.columnGap().length(), style);
        case CSSPropertyRowGap:
            if (style.rowGap().isNormal())
                return cssValuePool.createIdentifierValue(CSSValueNormal);
            return zoomAdjustedPixelValueForLength(style.rowGap().length(), style);
        case CSSPropertyWebkitColumnProgression:
            return cssValuePool.createValue(style.columnProgression());
        case CSSPropertyColumnRuleColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyOutlineColor)) : currentColorOrValidColor(&style, style.columnRuleColor());
        case CSSPropertyColumnRuleStyle:
            return cssValuePool.createValue(style.columnRuleStyle());
        case CSSPropertyColumnRuleWidth:
            return zoomAdjustedPixelValue(style.columnRuleWidth(), style);
        case CSSPropertyColumnSpan:
            return cssValuePool.createIdentifierValue(style.columnSpan() == ColumnSpan::All ? CSSValueAll : CSSValueNone);
        case CSSPropertyWebkitColumnBreakAfter:
            return cssValuePool.createValue(convertToColumnBreak(style.breakAfter()));
        case CSSPropertyWebkitColumnBreakBefore:
            return cssValuePool.createValue(convertToColumnBreak(style.breakBefore()));
        case CSSPropertyWebkitColumnBreakInside:
            return cssValuePool.createValue(convertToColumnBreak(style.breakInside()));
        case CSSPropertyColumnWidth:
            if (style.hasAutoColumnWidth())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return zoomAdjustedPixelValue(style.columnWidth(), style);
        case CSSPropertyTabSize:
            return cssValuePool.createValue(style.tabSize().widthInPixels(1.0), style.tabSize().isSpaces() ? CSSUnitType::CSS_NUMBER : CSSUnitType::CSS_PX);
        case CSSPropertyCursor: {
            RefPtr<CSSValueList> list;
            auto* cursors = style.cursors();
            if (cursors && cursors->size() > 0) {
                list = CSSValueList::createCommaSeparated();
                for (unsigned i = 0; i < cursors->size(); ++i)
                    if (StyleImage* image = cursors->at(i).image())
                        list->append(image->cssValue());
            }
            auto value = cssValuePool.createValue(style.cursor());
            if (list) {
                list->append(WTFMove(value));
                return list;
            }
            return value;
        }
#if ENABLE(CURSOR_VISIBILITY)
        case CSSPropertyWebkitCursorVisibility:
            return cssValuePool.createValue(style.cursorVisibility());
#endif
        case CSSPropertyDirection:
            return cssValuePool.createValue(style.direction());
        case CSSPropertyDisplay:
            return cssValuePool.createValue(style.display());
        case CSSPropertyEmptyCells:
            return cssValuePool.createValue(style.emptyCells());
        case CSSPropertyAlignContent:
            return valueForContentPositionAndDistributionWithOverflowAlignment(style.alignContent());
        case CSSPropertyAlignItems:
            return valueForItemPositionWithOverflowAlignment(style.alignItems());
        case CSSPropertyAlignSelf:
            return valueForItemPositionWithOverflowAlignment(style.alignSelf());
        case CSSPropertyFlex:
            return getCSSPropertyValuesForShorthandProperties(flexShorthand());
        case CSSPropertyFlexBasis:
            return cssValuePool.createValue(style.flexBasis(), style);
        case CSSPropertyFlexDirection:
            return cssValuePool.createValue(style.flexDirection());
        case CSSPropertyFlexFlow:
            return getCSSPropertyValuesForShorthandProperties(flexFlowShorthand());
        case CSSPropertyFlexGrow:
            return cssValuePool.createValue(style.flexGrow());
        case CSSPropertyFlexShrink:
            return cssValuePool.createValue(style.flexShrink());
        case CSSPropertyFlexWrap:
            return cssValuePool.createValue(style.flexWrap());
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
            return cssValuePool.createValue(style.order(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyFloat:
            if (style.display() != DisplayType::None && style.hasOutOfFlowPosition())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return cssValuePool.createValue(style.floating());
        case CSSPropertyFont: {
            auto computedFont = fontShorthandValueForSelectionProperties(style.fontDescription());
            computedFont->size = fontSizeFromStyle(style);
            computedFont->lineHeight = lineHeightFromStyle(style);
            computedFont->family = fontFamilyListFromStyle(style);
            return computedFont;
        }
        case CSSPropertyFontFamily:
            return fontFamilyFromStyle(style);
        case CSSPropertyFontSize:
            return fontSizeFromStyle(style);
        case CSSPropertyFontStyle:
            return fontStyleFromStyle(style);
        case CSSPropertyFontStretch:
            return fontStretchFromStyle(style);
        case CSSPropertyFontVariant:
            return fontVariantFromStyle(style);
        case CSSPropertyFontWeight:
            return fontWeightFromStyle(style);
        case CSSPropertyFontSynthesis:
            return fontSynthesisFromStyle(style);
        case CSSPropertyFontFeatureSettings: {
            const FontFeatureSettings& featureSettings = style.fontDescription().featureSettings();
            if (!featureSettings.size())
                return cssValuePool.createIdentifierValue(CSSValueNormal);
            auto list = CSSValueList::createCommaSeparated();
            for (auto& feature : featureSettings)
                list->append(CSSFontFeatureValue::create(FontTag(feature.tag()), feature.value()));
            return list;
        }
#if ENABLE(VARIATION_FONTS)
        case CSSPropertyFontVariationSettings: {
            const FontVariationSettings& variationSettings = style.fontDescription().variationSettings();
            if (variationSettings.isEmpty())
                return cssValuePool.createIdentifierValue(CSSValueNormal);
            auto list = CSSValueList::createCommaSeparated();
            for (auto& feature : variationSettings)
                list->append(CSSFontVariationValue::create(feature.tag(), feature.value()));
            return list;
        }
        case CSSPropertyFontOpticalSizing:
            return cssValuePool.createValue(style.fontDescription().opticalSizing());
#endif
        case CSSPropertyGridAutoFlow: {
            auto list = CSSValueList::createSpaceSeparated();
            ASSERT(style.isGridAutoFlowDirectionRow() || style.isGridAutoFlowDirectionColumn());
            if (style.isGridAutoFlowDirectionRow())
                list->append(cssValuePool.createIdentifierValue(CSSValueRow));
            else
                list->append(cssValuePool.createIdentifierValue(CSSValueColumn));

            if (style.isGridAutoFlowAlgorithmDense())
                list->append(cssValuePool.createIdentifierValue(CSSValueDense));

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
                return cssValuePool.createIdentifierValue(CSSValueNone);
            }
            return CSSGridTemplateAreasValue::create(style.namedGridArea(), style.namedGridAreaRowCount(), style.namedGridAreaColumnCount());
        case CSSPropertyGap:
            return getCSSPropertyValuesForShorthandProperties(gapShorthand());
        case CSSPropertyHeight:
            if (renderer && !renderer->isRenderSVGModelObject()) {
                // According to http://www.w3.org/TR/CSS2/visudet.html#the-height-property,
                // the "height" property does not apply for non-replaced inline elements.
                if (!isNonReplacedInline(*renderer))
                    return zoomAdjustedPixelValue(sizingBox(*renderer).height(), style);
            }
            return zoomAdjustedPixelValueForLength(style.height(), style);
        case CSSPropertyWebkitHyphens:
            return cssValuePool.createValue(style.hyphens());
        case CSSPropertyWebkitHyphenateCharacter:
            if (style.hyphenationString().isNull())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style.hyphenationString(), CSSUnitType::CSS_STRING);
        case CSSPropertyWebkitHyphenateLimitAfter:
            if (style.hyphenationLimitAfter() < 0)
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style.hyphenationLimitAfter(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyWebkitHyphenateLimitBefore:
            if (style.hyphenationLimitBefore() < 0)
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style.hyphenationLimitBefore(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyWebkitHyphenateLimitLines:
            if (style.hyphenationLimitLines() < 0)
                return CSSPrimitiveValue::createIdentifier(CSSValueNoLimit);
            return CSSPrimitiveValue::create(style.hyphenationLimitLines(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyWebkitBorderFit:
            if (style.borderFit() == BorderFit::Border)
                return cssValuePool.createIdentifierValue(CSSValueBorder);
            return cssValuePool.createIdentifierValue(CSSValueLines);
        case CSSPropertyImageOrientation:
            if (style.imageOrientation() == ImageOrientation::FromImage)
                return cssValuePool.createIdentifierValue(CSSValueFromImage);
            return cssValuePool.createIdentifierValue(CSSValueNone);
        case CSSPropertyImageRendering:
            return CSSPrimitiveValue::create(style.imageRendering());
#if ENABLE(CSS_IMAGE_RESOLUTION)
        case CSSPropertyImageResolution:
            return cssValuePool.createValue(style.imageResolution(), CSSUnitType::CSS_DPPX);
#endif
        case CSSPropertyLeft:
            return positionOffsetValue(style, CSSPropertyLeft, renderer);
        case CSSPropertyLetterSpacing:
            if (!style.letterSpacing())
                return cssValuePool.createIdentifierValue(CSSValueNormal);
            return zoomAdjustedPixelValue(style.letterSpacing(), style);
        case CSSPropertyWebkitLineClamp:
            if (style.lineClamp().isNone())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return cssValuePool.createValue(style.lineClamp().value(), style.lineClamp().isPercentage() ? CSSUnitType::CSS_PERCENTAGE : CSSUnitType::CSS_NUMBER);
        case CSSPropertyLineHeight:
            return lineHeightFromStyle(style);
        case CSSPropertyListStyleImage:
            if (style.listStyleImage())
                return style.listStyleImage()->cssValue();
            return cssValuePool.createIdentifierValue(CSSValueNone);
        case CSSPropertyListStylePosition:
            return cssValuePool.createValue(style.listStylePosition());
        case CSSPropertyListStyleType:
            if (style.listStyleType() == ListStyleType::String)
                return cssValuePool.createValue(style.listStyleStringValue(), CSSUnitType::CSS_STRING);
            return cssValuePool.createValue(style.listStyleType());
        case CSSPropertyWebkitLocale:
            if (style.specifiedLocale().isNull())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style.specifiedLocale(), CSSUnitType::CSS_STRING);
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
        case CSSPropertyWebkitUserModify:
            return cssValuePool.createValue(style.userModify());
        case CSSPropertyMaxHeight: {
            const Length& maxHeight = style.maxHeight();
            if (maxHeight.isUndefined())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValueForLength(maxHeight, style);
        }
        case CSSPropertyMaxWidth: {
            const Length& maxWidth = style.maxWidth();
            if (maxWidth.isUndefined())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValueForLength(maxWidth, style);
        }
        case CSSPropertyMinHeight:
            if (style.minHeight().isAuto()) {
                if (isFlexOrGridItem(renderer))
                    return cssValuePool.createIdentifierValue(CSSValueAuto);
                return zoomAdjustedPixelValue(0, style);
            }
            return zoomAdjustedPixelValueForLength(style.minHeight(), style);
        case CSSPropertyMinWidth:
            if (style.minWidth().isAuto()) {
                if (isFlexOrGridItem(renderer))
                    return cssValuePool.createIdentifierValue(CSSValueAuto);
                return zoomAdjustedPixelValue(0, style);
            }
            return zoomAdjustedPixelValueForLength(style.minWidth(), style);
        case CSSPropertyObjectFit:
            return cssValuePool.createValue(style.objectFit());
        case CSSPropertyObjectPosition: {
            auto list = CSSValueList::createSpaceSeparated();
            list->append(zoomAdjustedPixelValueForLength(style.objectPosition().x(), style));
            list->append(zoomAdjustedPixelValueForLength(style.objectPosition().y(), style));
            return list;
        }
        case CSSPropertyOpacity:
            return cssValuePool.createValue(style.opacity(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyOrphans:
            if (style.hasAutoOrphans())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style.orphans(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyOutlineColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style.visitedDependentColor(CSSPropertyOutlineColor)) : currentColorOrValidColor(&style, style.outlineColor());
        case CSSPropertyOutlineOffset:
            return zoomAdjustedPixelValue(style.outlineOffset(), style);
        case CSSPropertyOutlineStyle:
            if (style.outlineStyleIsAuto() == OutlineIsAuto::On)
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style.outlineStyle());
        case CSSPropertyOutlineWidth:
            return zoomAdjustedPixelValue(style.outlineWidth(), style);
        case CSSPropertyOverflow:
            return getCSSPropertyValuesFor2SidesShorthand(overflowShorthand());
        case CSSPropertyOverflowWrap:
            return cssValuePool.createValue(style.overflowWrap());
        case CSSPropertyOverflowX:
            return cssValuePool.createValue(style.overflowX());
        case CSSPropertyOverflowY:
            return cssValuePool.createValue(style.overflowY());
        case CSSPropertyOverscrollBehavior:
            if (renderer && !renderer->settings().overscrollBehaviorEnabled())
                return nullptr;
            return cssValuePool.createValue(std::max(style.overscrollBehaviorX(), style.overscrollBehaviorY()));
        case CSSPropertyOverscrollBehaviorX:
            if (renderer && !renderer->settings().overscrollBehaviorEnabled())
                return nullptr;
            return cssValuePool.createValue(style.overscrollBehaviorX());
        case CSSPropertyOverscrollBehaviorY:
            if (renderer && !renderer->settings().overscrollBehaviorEnabled())
                return nullptr;
            return cssValuePool.createValue(style.overscrollBehaviorY());
        case CSSPropertyPaddingTop:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingTop, &RenderBoxModelObject::computedCSSPaddingTop>(style, renderer);
        case CSSPropertyPaddingRight:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingRight, &RenderBoxModelObject::computedCSSPaddingRight>(style, renderer);
        case CSSPropertyPaddingBottom:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingBottom, &RenderBoxModelObject::computedCSSPaddingBottom>(style, renderer);
        case CSSPropertyPaddingLeft:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingLeft, &RenderBoxModelObject::computedCSSPaddingLeft>(style, renderer);
        case CSSPropertyPageBreakAfter:
            return cssValuePool.createValue(convertToPageBreak(style.breakAfter()));
        case CSSPropertyPageBreakBefore:
            return cssValuePool.createValue(convertToPageBreak(style.breakBefore()));
        case CSSPropertyPageBreakInside:
            return cssValuePool.createValue(convertToPageBreak(style.breakInside()));
        case CSSPropertyBreakAfter:
            return cssValuePool.createValue(style.breakAfter());
        case CSSPropertyBreakBefore:
            return cssValuePool.createValue(style.breakBefore());
        case CSSPropertyBreakInside:
            return cssValuePool.createValue(style.breakInside());
        case CSSPropertyHangingPunctuation:
            return hangingPunctuationToCSSValue(style.hangingPunctuation());
        case CSSPropertyPosition:
            return cssValuePool.createValue(style.position());
        case CSSPropertyRight:
            return positionOffsetValue(style, CSSPropertyRight, renderer);
        case CSSPropertyWebkitRubyPosition:
            return cssValuePool.createValue(style.rubyPosition());
        case CSSPropertyTableLayout:
            return cssValuePool.createValue(style.tableLayout());
        case CSSPropertyTextAlign:
            return cssValuePool.createValue(style.textAlign());
        case CSSPropertyTextDecoration:
            return renderTextDecorationFlagsToCSSValue(style.textDecoration());
#if ENABLE(CSS3_TEXT)
        case CSSPropertyWebkitTextAlignLast:
            return cssValuePool.createValue(style.textAlignLast());
        case CSSPropertyWebkitTextJustify:
            return cssValuePool.createValue(style.textJustify());
#endif // CSS3_TEXT
        case CSSPropertyWebkitTextDecoration:
            return getCSSPropertyValuesForShorthandProperties(webkitTextDecorationShorthand());
        case CSSPropertyTextDecorationLine:
            return renderTextDecorationFlagsToCSSValue(style.textDecoration());
        case CSSPropertyTextDecorationStyle:
            return renderTextDecorationStyleFlagsToCSSValue(style.textDecorationStyle());
        case CSSPropertyTextDecorationColor:
            return currentColorOrValidColor(&style, style.textDecorationColor());
        case CSSPropertyTextDecorationSkip:
            return renderTextDecorationSkipFlagsToCSSValue(style.textDecorationSkip());
        case CSSPropertyTextUnderlinePosition:
            return cssValuePool.createValue(style.textUnderlinePosition());
        case CSSPropertyTextUnderlineOffset:
            return textUnderlineOffsetToCSSValue(style.textUnderlineOffset());
        case CSSPropertyTextDecorationThickness:
            return textDecorationThicknessToCSSValue(style.textDecorationThickness());
        case CSSPropertyWebkitTextDecorationsInEffect:
            return renderTextDecorationFlagsToCSSValue(style.textDecorationsInEffect());
        case CSSPropertyWebkitTextFillColor:
            return currentColorOrValidColor(&style, style.textFillColor());
        case CSSPropertyWebkitTextEmphasisColor:
            return currentColorOrValidColor(&style, style.textEmphasisColor());
        case CSSPropertyWebkitTextEmphasisPosition:
            return renderEmphasisPositionFlagsToCSSValue(style.textEmphasisPosition());
        case CSSPropertyWebkitTextEmphasisStyle:
            switch (style.textEmphasisMark()) {
            case TextEmphasisMark::None:
                return cssValuePool.createIdentifierValue(CSSValueNone);
            case TextEmphasisMark::Custom:
                return cssValuePool.createValue(style.textEmphasisCustomMark(), CSSUnitType::CSS_STRING);
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
                list->append(cssValuePool.createValue(style.textEmphasisFill()));
                list->append(cssValuePool.createValue(style.textEmphasisMark()));
                return list;
            }
            RELEASE_ASSERT_NOT_REACHED();
        case CSSPropertyTextIndent: {
            // If CSS3_TEXT is disabled or text-indent has only one value(<length> | <percentage>),
            // getPropertyCSSValue() returns CSSValue.
            auto textIndent = zoomAdjustedPixelValueForLength(style.textIndent(), style);
#if ENABLE(CSS3_TEXT)
            // If CSS3_TEXT is enabled and text-indent has -webkit-each-line or -webkit-hanging,
            // getPropertyCSSValue() returns CSSValueList.
            if (style.textIndentLine() == TextIndentLine::EachLine || style.textIndentType() == TextIndentType::Hanging) {
                auto list = CSSValueList::createSpaceSeparated();
                list->append(WTFMove(textIndent));
                if (style.textIndentLine() == TextIndentLine::EachLine)
                    list->append(cssValuePool.createIdentifierValue(CSSValueWebkitEachLine));
                if (style.textIndentType() == TextIndentType::Hanging)
                    list->append(cssValuePool.createIdentifierValue(CSSValueWebkitHanging));
                return list;
            }
#endif
            return textIndent;
        }
        case CSSPropertyTextShadow:
            return valueForShadow(style.textShadow(), propertyID, style);
        case CSSPropertyTextRendering:
            return cssValuePool.createValue(style.fontDescription().textRenderingMode());
        case CSSPropertyTextOverflow:
            if (style.textOverflow() == TextOverflow::Ellipsis)
                return cssValuePool.createIdentifierValue(CSSValueEllipsis);
            return cssValuePool.createIdentifierValue(CSSValueClip);
        case CSSPropertyWebkitTextSecurity:
            return cssValuePool.createValue(style.textSecurity());
#if ENABLE(TEXT_AUTOSIZING)
        case CSSPropertyWebkitTextSizeAdjust:
            if (style.textSizeAdjust().isAuto())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            if (style.textSizeAdjust().isNone())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return CSSPrimitiveValue::create(style.textSizeAdjust().percentage(), CSSUnitType::CSS_PERCENTAGE);
#endif
        case CSSPropertyWebkitTextStrokeColor:
            return currentColorOrValidColor(&style, style.textStrokeColor());
        case CSSPropertyWebkitTextStrokeWidth:
            return zoomAdjustedPixelValue(style.textStrokeWidth(), style);
        case CSSPropertyTextTransform:
            return cssValuePool.createValue(style.textTransform());
        case CSSPropertyTop:
            return positionOffsetValue(style, CSSPropertyTop, renderer);
        case CSSPropertyUnicodeBidi:
            return cssValuePool.createValue(style.unicodeBidi());
        case CSSPropertyVerticalAlign:
            switch (style.verticalAlign()) {
            case VerticalAlign::Baseline:
                return cssValuePool.createIdentifierValue(CSSValueBaseline);
            case VerticalAlign::Middle:
                return cssValuePool.createIdentifierValue(CSSValueMiddle);
            case VerticalAlign::Sub:
                return cssValuePool.createIdentifierValue(CSSValueSub);
            case VerticalAlign::Super:
                return cssValuePool.createIdentifierValue(CSSValueSuper);
            case VerticalAlign::TextTop:
                return cssValuePool.createIdentifierValue(CSSValueTextTop);
            case VerticalAlign::TextBottom:
                return cssValuePool.createIdentifierValue(CSSValueTextBottom);
            case VerticalAlign::Top:
                return cssValuePool.createIdentifierValue(CSSValueTop);
            case VerticalAlign::Bottom:
                return cssValuePool.createIdentifierValue(CSSValueBottom);
            case VerticalAlign::BaselineMiddle:
                return cssValuePool.createIdentifierValue(CSSValueWebkitBaselineMiddle);
            case VerticalAlign::Length:
                return cssValuePool.createValue(style.verticalAlignLength());
            }
            ASSERT_NOT_REACHED();
            return nullptr;
        case CSSPropertyVisibility:
            return cssValuePool.createValue(style.visibility());
        case CSSPropertyWhiteSpace:
            return cssValuePool.createValue(style.whiteSpace());
        case CSSPropertyWidows:
            if (style.hasAutoWidows())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style.widows(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyWidth:
            if (renderer && !renderer->isRenderSVGModelObject()) {
                // According to http://www.w3.org/TR/CSS2/visudet.html#the-width-property,
                // the "width" property does not apply for non-replaced inline elements.
                if (!isNonReplacedInline(*renderer))
                    return zoomAdjustedPixelValue(sizingBox(*renderer).width(), style);
            }
            return zoomAdjustedPixelValueForLength(style.width(), style);
        case CSSPropertyWillChange:
            return willChangePropertyValue(style.willChange());
        case CSSPropertyWordBreak:
            return cssValuePool.createValue(style.wordBreak());
        case CSSPropertyWordSpacing:
            return zoomAdjustedPixelValue(style.fontCascade().wordSpacing(), style);
        case CSSPropertyWordWrap:
            return cssValuePool.createValue(style.overflowWrap());
        case CSSPropertyLineBreak:
            return cssValuePool.createValue(style.lineBreak());
        case CSSPropertyWebkitNbspMode:
            return cssValuePool.createValue(style.nbspMode());
        case CSSPropertyResize:
            return cssValuePool.createValue(style.resize());
        case CSSPropertyWebkitFontKerning:
            return cssValuePool.createValue(style.fontDescription().kerning());
        case CSSPropertyWebkitFontSmoothing:
            return cssValuePool.createValue(style.fontDescription().fontSmoothing());
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
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style.specifiedZIndex(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyZoom:
            return cssValuePool.createValue(style.zoom(), CSSUnitType::CSS_NUMBER);
        case CSSPropertyBoxSizing:
            if (style.boxSizing() == BoxSizing::ContentBox)
                return cssValuePool.createIdentifierValue(CSSValueContentBox);
            return cssValuePool.createIdentifierValue(CSSValueBorderBox);
        case CSSPropertyAnimationDelay:
            return delayValue(style.animations());
        case CSSPropertyAnimationDirection: {
            auto list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style.animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    switch (t->animation(i).direction()) {
                    case Animation::AnimationDirectionNormal:
                        list->append(cssValuePool.createIdentifierValue(CSSValueNormal));
                        break;
                    case Animation::AnimationDirectionAlternate:
                        list->append(cssValuePool.createIdentifierValue(CSSValueAlternate));
                        break;
                    case Animation::AnimationDirectionReverse:
                        list->append(cssValuePool.createIdentifierValue(CSSValueReverse));
                        break;
                    case Animation::AnimationDirectionAlternateReverse:
                        list->append(cssValuePool.createIdentifierValue(CSSValueAlternateReverse));
                        break;
                    }
                }
            } else
                list->append(cssValuePool.createIdentifierValue(CSSValueNormal));
            return list;
        }
        case CSSPropertyAnimationDuration:
            return durationValue(style.animations());
        case CSSPropertyAnimationFillMode: {
            auto list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style.animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    switch (t->animation(i).fillMode()) {
                    case AnimationFillMode::None:
                        list->append(cssValuePool.createIdentifierValue(CSSValueNone));
                        break;
                    case AnimationFillMode::Forwards:
                        list->append(cssValuePool.createIdentifierValue(CSSValueForwards));
                        break;
                    case AnimationFillMode::Backwards:
                        list->append(cssValuePool.createIdentifierValue(CSSValueBackwards));
                        break;
                    case AnimationFillMode::Both:
                        list->append(cssValuePool.createIdentifierValue(CSSValueBoth));
                        break;
                    }
                }
            } else
                list->append(cssValuePool.createIdentifierValue(CSSValueNone));
            return list;
        }
        case CSSPropertyAnimationIterationCount: {
            auto list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style.animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    double iterationCount = t->animation(i).iterationCount();
                    if (iterationCount == Animation::IterationCountInfinite)
                        list->append(cssValuePool.createIdentifierValue(CSSValueInfinite));
                    else
                        list->append(cssValuePool.createValue(iterationCount, CSSUnitType::CSS_NUMBER));
                }
            } else
                list->append(cssValuePool.createValue(Animation::initialIterationCount(), CSSUnitType::CSS_NUMBER));
            return list;
        }
        case CSSPropertyAnimationName: {
            auto list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style.animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i)
                    list->append(cssValuePool.createValue(t->animation(i).name(), CSSUnitType::CSS_STRING));
            } else
                list->append(cssValuePool.createIdentifierValue(CSSValueNone));
            return list;
        }
        case CSSPropertyAnimationPlayState: {
            auto list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style.animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    switch (t->animation(i).playState()) {
                    case AnimationPlayState::Playing:
                        list->append(cssValuePool.createIdentifierValue(CSSValueRunning));
                        break;
                    case AnimationPlayState::Paused:
                        list->append(cssValuePool.createIdentifierValue(CSSValuePaused));
                        break;
                    }
                }
            } else
                list->append(cssValuePool.createIdentifierValue(CSSValueRunning));
            return list;
        }
        case CSSPropertyAnimationTimingFunction:
            return timingFunctionValue(style.animations());
        case CSSPropertyWebkitAppearance:
            return cssValuePool.createValue(style.appearance());
        case CSSPropertyAspectRatio:
            if (renderer && !renderer->settings().aspectRatioEnabled())
                return nullptr;
            switch (style.aspectRatioType()) {
            case AspectRatioType::Auto:
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            case AspectRatioType::AutoZero:
            case AspectRatioType::AutoAndRatio:
            case AspectRatioType::Ratio: {
                auto ratioList = CSSValueList::createSlashSeparated();
                ratioList->append(cssValuePool.createValue(style.aspectRatioWidth(), CSSUnitType::CSS_NUMBER));
                ratioList->append(cssValuePool.createValue(style.aspectRatioHeight(), CSSUnitType::CSS_NUMBER));
                if (style.aspectRatioType() != AspectRatioType::AutoAndRatio)
                    return ratioList;
                auto list = CSSValueList::createSpaceSeparated();
                list->append(cssValuePool.createIdentifierValue(CSSValueAuto));
                list->append(ratioList);
                return list;
            }
            }
            ASSERT_NOT_REACHED();
            return nullptr;
        case CSSPropertyWebkitBackfaceVisibility:
            return cssValuePool.createIdentifierValue((style.backfaceVisibility() == BackfaceVisibility::Hidden) ? CSSValueHidden : CSSValueVisible);
        case CSSPropertyWebkitBorderImage:
            return valueForNinePieceImage(style.borderImage());
        case CSSPropertyBorderImageOutset:
            return valueForNinePieceImageQuad(style.borderImage().outset());
        case CSSPropertyBorderImageRepeat:
            return valueForNinePieceImageRepeat(style.borderImage());
        case CSSPropertyBorderImageSlice:
            return valueForNinePieceImageSlice(style.borderImage());
        case CSSPropertyBorderImageWidth:
            return valueForNinePieceImageQuad(style.borderImage().borderSlices());
        case CSSPropertyWebkitMaskBoxImage:
            return valueForNinePieceImage(style.maskBoxImage());
        case CSSPropertyWebkitMaskBoxImageOutset:
            return valueForNinePieceImageQuad(style.maskBoxImage().outset());
        case CSSPropertyWebkitMaskBoxImageRepeat:
            return valueForNinePieceImageRepeat(style.maskBoxImage());
        case CSSPropertyWebkitMaskBoxImageSlice:
            return valueForNinePieceImageSlice(style.maskBoxImage());
        case CSSPropertyWebkitMaskBoxImageWidth:
            return valueForNinePieceImageQuad(style.maskBoxImage().borderSlices());
        case CSSPropertyWebkitMaskBoxImageSource:
            if (style.maskBoxImageSource())
                return style.maskBoxImageSource()->cssValue();
            return cssValuePool.createIdentifierValue(CSSValueNone);
        case CSSPropertyWebkitFontSizeDelta:
            // Not a real style property -- used by the editing engine -- so has no computed value.
            break;
        case CSSPropertyWebkitInitialLetter: {
            auto drop = !style.initialLetterDrop() ? cssValuePool.createIdentifierValue(CSSValueNormal) : cssValuePool.createValue(style.initialLetterDrop(), CSSUnitType::CSS_NUMBER);
            auto size = !style.initialLetterHeight() ? cssValuePool.createIdentifierValue(CSSValueNormal) : cssValuePool.createValue(style.initialLetterHeight(), CSSUnitType::CSS_NUMBER);
            return cssValuePool.createValue(Pair::create(WTFMove(drop), WTFMove(size)));
        }
        case CSSPropertyWebkitMarginBottomCollapse:
        case CSSPropertyWebkitMarginAfterCollapse:
            return cssValuePool.createValue(style.marginAfterCollapse());
        case CSSPropertyWebkitMarginTopCollapse:
        case CSSPropertyWebkitMarginBeforeCollapse:
            return cssValuePool.createValue(style.marginBeforeCollapse());
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
        case CSSPropertyWebkitOverflowScrolling:
            if (!style.useTouchOverflowScrolling())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createIdentifierValue(CSSValueTouch);
#endif
        case CSSPropertyScrollBehavior:
            if (!style.useSmoothScrolling())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createIdentifierValue(CSSValueSmooth);
        case CSSPropertyPerspective:
            if (!style.hasPerspective())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValue(style.perspective(), style);
        case CSSPropertyPerspectiveOrigin: {
            auto list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                LayoutRect box;
                if (is<RenderBox>(*renderer))
                    box = downcast<RenderBox>(*renderer).borderBoxRect();

                list->append(zoomAdjustedPixelValue(minimumValueForLength(style.perspectiveOriginX(), box.width()), style));
                list->append(zoomAdjustedPixelValue(minimumValueForLength(style.perspectiveOriginY(), box.height()), style));
            } else {
                list->append(zoomAdjustedPixelValueForLength(style.perspectiveOriginX(), style));
                list->append(zoomAdjustedPixelValueForLength(style.perspectiveOriginY(), style));
            }
            return list;
        }
        case CSSPropertyWebkitRtlOrdering:
            return cssValuePool.createIdentifierValue(style.rtlOrdering() == Order::Visual ? CSSValueVisual : CSSValueLogical);
#if ENABLE(TOUCH_EVENTS)
        case CSSPropertyWebkitTapHighlightColor:
            return currentColorOrValidColor(&style, style.tapHighlightColor());
#endif
        case CSSPropertyTouchAction:
            return touchActionFlagsToCSSValue(style.touchActions());
#if PLATFORM(IOS_FAMILY)
        case CSSPropertyWebkitTouchCallout:
            return cssValuePool.createIdentifierValue(style.touchCalloutEnabled() ? CSSValueDefault : CSSValueNone);
#endif
        case CSSPropertyWebkitUserDrag:
            return cssValuePool.createValue(style.userDrag());
        case CSSPropertyWebkitUserSelect:
            return cssValuePool.createValue(style.userSelect());
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
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            auto rect = Rect::create();
            rect->setTop(autoOrZoomAdjustedValue(style.clip().top(), style));
            rect->setRight(autoOrZoomAdjustedValue(style.clip().right(), style));
            rect->setBottom(autoOrZoomAdjustedValue(style.clip().bottom(), style));
            rect->setLeft(autoOrZoomAdjustedValue(style.clip().left(), style));
            return cssValuePool.createValue(WTFMove(rect));
        }
        case CSSPropertySpeakAs:
            return speakAsToCSSValue(style.speakAs());
        case CSSPropertyTransform:
            return computedTransform(renderer, style);
        case CSSPropertyTransformBox:
            return CSSPrimitiveValue::create(style.transformBox());
        case CSSPropertyTransformOrigin: {
            auto list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                LayoutRect box;
                if (is<RenderBox>(*renderer))
                    box = downcast<RenderBox>(*renderer).borderBoxRect();

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
        case CSSPropertyWebkitTransformStyle:
            switch (style.transformStyle3D()) {
            case TransformStyle3D::Flat:
                return cssValuePool.createIdentifierValue(CSSValueFlat);
            case TransformStyle3D::Preserve3D:
                return cssValuePool.createIdentifierValue(CSSValuePreserve3d);
#if ENABLE(CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
            case TransformStyle3D::Optimized3D:
                return cssValuePool.createIdentifierValue(CSSValueOptimized3d);
#endif
            }
            ASSERT_NOT_REACHED();
            return nullptr;
        case CSSPropertyTranslate:
            if (renderer && !renderer->settings().cssIndividualTransformPropertiesEnabled())
                return nullptr;
            return computedTranslate(renderer, style);
        case CSSPropertyScale:
            if (renderer && !renderer->settings().cssIndividualTransformPropertiesEnabled())
                return nullptr;
            return computedScale(renderer, style);
        case CSSPropertyRotate:
            if (renderer && !renderer->settings().cssIndividualTransformPropertiesEnabled())
                return nullptr;
            return computedRotate(renderer, style);
        case CSSPropertyTransitionDelay:
            return delayValue(style.transitions());
        case CSSPropertyTransitionDuration:
            return durationValue(style.transitions());
        case CSSPropertyTransitionProperty:
            return transitionPropertyValue(style.transitions());
        case CSSPropertyTransitionTimingFunction:
            return timingFunctionValue(style.transitions());
        case CSSPropertyTransition: {
            if (auto* animationList = style.transitions()) {
                auto transitionsList = CSSValueList::createCommaSeparated();
                for (size_t i = 0; i < animationList->size(); ++i) {
                    auto list = CSSValueList::createSpaceSeparated();
                    auto& animation = animationList->animation(i);
                    list->append(createTransitionPropertyValue(animation));
                    list->append(cssValuePool.createValue(animation.duration(), CSSUnitType::CSS_S));
                    list->append(createTimingFunctionValue(*animation.timingFunction()));
                    list->append(cssValuePool.createValue(animation.delay(), CSSUnitType::CSS_S));
                    transitionsList->append(WTFMove(list));
                }
                return transitionsList;
            }

            auto list = CSSValueList::createSpaceSeparated();
            // transition-property default value.
            list->append(cssValuePool.createIdentifierValue(CSSValueAll));
            list->append(cssValuePool.createValue(Animation::initialDuration(), CSSUnitType::CSS_S));
            list->append(createTimingFunctionValue(Animation::initialTimingFunction()));
            list->append(cssValuePool.createValue(Animation::initialDelay(), CSSUnitType::CSS_S));
            return list;
        }
        case CSSPropertyPointerEvents:
            return cssValuePool.createValue(style.pointerEvents());
        case CSSPropertyWebkitLineGrid:
            if (style.lineGrid().isNull())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return cssValuePool.createValue(style.lineGrid(), CSSUnitType::CSS_STRING);
        case CSSPropertyWebkitLineSnap:
            return CSSPrimitiveValue::create(style.lineSnap());
        case CSSPropertyWebkitLineAlign:
            return CSSPrimitiveValue::create(style.lineAlign());
        case CSSPropertyWritingMode:
            return cssValuePool.createValue(style.writingMode());
        case CSSPropertyWebkitTextCombine:
            return cssValuePool.createValue(style.textCombine());
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
        case CSSPropertyClipPath: {
            auto* operation = style.clipPath();
            if (!operation)
                return cssValuePool.createIdentifierValue(CSSValueNone);
            if (is<ReferenceClipPathOperation>(*operation))
                return CSSPrimitiveValue::create(downcast<ReferenceClipPathOperation>(*operation).url(), CSSUnitType::CSS_URI);
            auto list = CSSValueList::createSpaceSeparated();
            if (is<ShapeClipPathOperation>(*operation)) {
                auto& shapeOperation = downcast<ShapeClipPathOperation>(*operation);
                list->append(valueForBasicShape(style, shapeOperation.basicShape()));
                if (shapeOperation.referenceBox() != CSSBoxType::BoxMissing)
                    list->append(cssValuePool.createValue(shapeOperation.referenceBox()));
            }
            if (is<BoxClipPathOperation>(*operation))
                list->append(cssValuePool.createValue(downcast<BoxClipPathOperation>(*operation).referenceBox()));
            return list;
        }
        case CSSPropertyShapeMargin:
            return cssValuePool.createValue(style.shapeMargin(), style);
        case CSSPropertyShapeImageThreshold:
            return cssValuePool.createValue(style.shapeImageThreshold(), CSSUnitType::CSS_NUMBER);
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
            return cssValuePool.createValue(style.mathStyle());
#if ENABLE(CSS_COMPOSITING)
        case CSSPropertyMixBlendMode:
            return cssValuePool.createValue(style.blendMode());
        case CSSPropertyIsolation:
            return cssValuePool.createValue(style.isolation());
#endif
        case CSSPropertyBackgroundBlendMode: {
            auto& layers = style.backgroundLayers();
            if (!layers.next())
                return cssValuePool.createValue(layers.blendMode());
            auto list = CSSValueList::createCommaSeparated();
            for (auto* currLayer = &layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool.createValue(currLayer->blendMode()));
            return list;
        }
        case CSSPropertyBackground:
            return getBackgroundShorthandValue();
        case CSSPropertyBorder: {
            auto value = propertyValue(CSSPropertyBorderTop, DoNotUpdateLayout);
            const CSSPropertyID properties[3] = { CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
            for (auto& property : properties) {
                if (!compareCSSValuePtr<CSSValue>(value, propertyValue(property, DoNotUpdateLayout)))
                    return nullptr;
            }
            return value;
        }
        case CSSPropertyBorderBlock: {
            auto value = propertyValue(CSSPropertyBorderBlockStart, DoNotUpdateLayout);
            if (!compareCSSValuePtr<CSSValue>(value, propertyValue(CSSPropertyBorderBlockEnd, DoNotUpdateLayout)))
                return nullptr;
            return value;
        }
        case CSSPropertyBorderBlockColor:
            return getCSSPropertyValuesFor2SidesShorthand(borderBlockColorShorthand());
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
        case CSSPropertyBorderImage:
            return valueForNinePieceImage(style.borderImage());
        case CSSPropertyBorderInline: {
            auto value = propertyValue(CSSPropertyBorderInlineStart, DoNotUpdateLayout);
            if (!compareCSSValuePtr<CSSValue>(value, propertyValue(CSSPropertyBorderInlineEnd, DoNotUpdateLayout)))
                return nullptr;
            return value;
        }
        case CSSPropertyBorderInlineColor:
            return getCSSPropertyValuesFor2SidesShorthand(borderInlineColorShorthand());
        case CSSPropertyBorderInlineStyle:
            return getCSSPropertyValuesFor2SidesShorthand(borderInlineStyleShorthand());
        case CSSPropertyBorderInlineWidth:
            return getCSSPropertyValuesFor2SidesShorthand(borderInlineWidthShorthand());
        case CSSPropertyBorderRadius:
            return borderRadiusShorthandValue(style);
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
#if ENABLE(CSS_SCROLL_SNAP)
        case CSSPropertyScrollSnapAlign:
            return valueForScrollSnapAlignment(style.scrollSnapAlign());
        case CSSPropertyScrollSnapStop:
            return CSSPrimitiveValue::create(style.scrollSnapStop());
        case CSSPropertyScrollSnapType:
            return valueForScrollSnapType(style.scrollSnapType());
#endif

#if ENABLE(CSS_TRAILING_WORD)
        case CSSPropertyAppleTrailingWord:
            return cssValuePool.createValue(style.trailingWord());
#endif

#if ENABLE(APPLE_PAY)
        case CSSPropertyApplePayButtonStyle:
            return cssValuePool.createValue(style.applePayButtonStyle());
        case CSSPropertyApplePayButtonType:
            return cssValuePool.createValue(style.applePayButtonType());
#endif

#if ENABLE(DARK_MODE_CSS)
        case CSSPropertyColorScheme: {
            auto colorScheme = style.colorScheme();
            if (colorScheme.isAuto())
                return cssValuePool.createIdentifierValue(CSSValueAuto);

            auto list = CSSValueList::createSpaceSeparated();
            if (colorScheme.contains(ColorScheme::Light))
                list->append(cssValuePool.createIdentifierValue(CSSValueLight));
            if (colorScheme.contains(ColorScheme::Dark))
                list->append(cssValuePool.createIdentifierValue(CSSValueDark));
            if (!colorScheme.allowsTransformations())
                list->append(cssValuePool.createIdentifierValue(CSSValueOnly));
            ASSERT(list->length());
            return list;
        }
#endif

        /* Individual properties not part of the spec */
        case CSSPropertyBackgroundRepeatX:
        case CSSPropertyBackgroundRepeatY:
            break;

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
            return cssValuePool.createValue(style.textZoom());

        case CSSPropertyPaintOrder:
            return paintOrder(style.paintOrder());
        case CSSPropertyStrokeLinecap:
            return CSSPrimitiveValue::create(style.capStyle());
        case CSSPropertyStrokeLinejoin:
            return CSSPrimitiveValue::create(style.joinStyle());
        case CSSPropertyStrokeWidth:
            return zoomAdjustedPixelValueForLength(style.strokeWidth(), style);
        case CSSPropertyStrokeColor:
            return currentColorOrValidColor(&style, style.strokeColor());
        case CSSPropertyStrokeMiterlimit:
            return CSSPrimitiveValue::create(style.strokeMiterLimit(), CSSUnitType::CSS_NUMBER);

        case CSSPropertyQuotes:
            return valueForQuotes(style.quotes());

        /* Unimplemented CSS 3 properties (including CSS3 shorthand properties) */
        case CSSPropertyAll:
        case CSSPropertyAnimation:
        case CSSPropertyWebkitTextEmphasis:
            break;

        /* Directional properties are resolved by resolveDirectionAwareProperty() before the switch. */
        case CSSPropertyBorderBlockEnd:
        case CSSPropertyBorderBlockEndColor:
        case CSSPropertyBorderBlockEndStyle:
        case CSSPropertyBorderBlockEndWidth:
        case CSSPropertyBorderBlockStart:
        case CSSPropertyBorderBlockStartColor:
        case CSSPropertyBorderBlockStartStyle:
        case CSSPropertyBorderBlockStartWidth:
        case CSSPropertyBorderEndEndRadius:        
        case CSSPropertyBorderEndStartRadius:        
        case CSSPropertyBorderInlineEnd:
        case CSSPropertyBorderInlineEndColor:
        case CSSPropertyBorderInlineEndStyle:
        case CSSPropertyBorderInlineEndWidth:
        case CSSPropertyBorderInlineStart:
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
            ASSERT_NOT_REACHED();
            break;

        /* Unimplemented @font-face properties */
        case CSSPropertySrc:
        case CSSPropertyUnicodeRange:
        case CSSPropertyFontDisplay:
            break;

        /* Other unimplemented properties */
        case CSSPropertyPage: // for @page
        case CSSPropertySize: // for @page
            break;

        /* Unimplemented -webkit- properties */
        case CSSPropertyWebkitBorderRadius:
        case CSSPropertyWebkitMarginCollapse:
        case CSSPropertyWebkitMarqueeDirection:
        case CSSPropertyWebkitMarqueeIncrement:
        case CSSPropertyWebkitMarqueeRepetition:
        case CSSPropertyWebkitMarqueeStyle:
        case CSSPropertyWebkitMarqueeSpeed:
        case CSSPropertyWebkitMask:
        case CSSPropertyWebkitMaskRepeatX:
        case CSSPropertyWebkitMaskRepeatY:
        case CSSPropertyPerspectiveOriginX:
        case CSSPropertyPerspectiveOriginY:
        case CSSPropertyWebkitTextStroke:
        case CSSPropertyTransformOriginX:
        case CSSPropertyTransformOriginY:
        case CSSPropertyTransformOriginZ:
            break;

        case CSSPropertyBufferedRendering:
        case CSSPropertyClipRule:
        case CSSPropertyMask:
        case CSSPropertyEnableBackground:
        case CSSPropertyFloodColor:
        case CSSPropertyFloodOpacity:
        case CSSPropertyLightingColor:
        case CSSPropertyStopColor:
        case CSSPropertyStopOpacity:
        case CSSPropertyColorInterpolation:
        case CSSPropertyColorInterpolationFilters:
        case CSSPropertyColorProfile:
        case CSSPropertyColorRendering:
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

    return nullptr;
}

String CSSComputedStyleDeclaration::getPropertyValue(CSSPropertyID propertyID) const
{
    auto value = getPropertyCSSValue(propertyID);
    if (!value)
        return emptyString(); // FIXME: Should this be null instead, as it is in StyleProperties::getPropertyValue?
    return value->cssText();
}

unsigned CSSComputedStyleDeclaration::length() const
{
    updateStyleIfNeededForProperty(m_element.get(), CSSPropertyCustom);

    auto* style = m_element->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return 0;

    return numComputedPropertyIDs + style->inheritedCustomProperties().size() + style->nonInheritedCustomProperties().size();
}

String CSSComputedStyleDeclaration::item(unsigned i) const
{
    if (i >= length())
        return String();

    if (i < numComputedPropertyIDs)
        return getPropertyNameString(computedPropertyIDs[i]);

    auto* style = m_element->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return String();

    const auto& inheritedCustomProperties = style->inheritedCustomProperties();

    if (i < numComputedPropertyIDs + inheritedCustomProperties.size()) {
        auto results = copyToVector(inheritedCustomProperties.keys());
        return results.at(i - numComputedPropertyIDs);
    }

    const auto& nonInheritedCustomProperties = style->nonInheritedCustomProperties();
    auto results = copyToVector(nonInheritedCustomProperties.keys());
    return results.at(i - inheritedCustomProperties.size() - numComputedPropertyIDs);
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
    for (size_t i = 0; i < shorthand.length(); ++i)
        list->append(propertyValue(shorthand.properties()[i], DoNotUpdateLayout).releaseNonNull());
    return list;
}

RefPtr<CSSValueList> ComputedStyleExtractor::getCSSPropertyValuesFor2SidesShorthand(const StylePropertyShorthand& shorthand)
{
    auto list = CSSValueList::createSpaceSeparated();

    // Assume the properties are in the usual order start, end.
    auto startValue = propertyValue(shorthand.properties()[0], DoNotUpdateLayout);
    auto endValue = propertyValue(shorthand.properties()[1], DoNotUpdateLayout);

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
    auto topValue = propertyValue(shorthand.properties()[0], DoNotUpdateLayout);
    auto rightValue = propertyValue(shorthand.properties()[1], DoNotUpdateLayout);
    auto bottomValue = propertyValue(shorthand.properties()[2], DoNotUpdateLayout);
    auto leftValue = propertyValue(shorthand.properties()[3], DoNotUpdateLayout);

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
    for (size_t i = 0; i < shorthand.length(); ++i)
        list->append(propertyValue(shorthand.properties()[i], DoNotUpdateLayout).releaseNonNull());
    return list;
}

Ref<MutableStyleProperties> ComputedStyleExtractor::copyPropertiesInSet(const CSSPropertyID* set, unsigned length)
{
    Vector<CSSProperty> list;
    list.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        if (auto value = propertyValue(set[i]))
            list.append(CSSProperty(set[i], WTFMove(value), false));
    }
    return MutableStyleProperties::create(WTFMove(list));
}

Ref<MutableStyleProperties> ComputedStyleExtractor::copyProperties()
{
    Vector<CSSProperty> list;
    list.reserveInitialCapacity(numCSSProperties);
    for (unsigned i = firstCSSProperty; i < lastCSSProperty; ++i) {
        auto propertyID = convertToCSSPropertyID(i);
        if (auto value = propertyValue(propertyID))
            list.append(CSSProperty(propertyID, WTFMove(value)));
    }
    return MutableStyleProperties::create(WTFMove(list));
}

CSSRule* CSSComputedStyleDeclaration::parentRule() const
{
    return nullptr;
}

RefPtr<DeprecatedCSSOMValue> CSSComputedStyleDeclaration::getPropertyCSSValue(const String& propertyName)
{
    if (isCustomPropertyName(propertyName)) {
        auto value = ComputedStyleExtractor(m_element.ptr(), m_allowVisitedStyle, m_pseudoElementSpecifier).customPropertyValue(propertyName);
        if (!value)
            return nullptr;
        return value->createDeprecatedCSSOMWrapper(*this);
    }

    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!propertyID)
        return nullptr;
    auto value = getPropertyCSSValue(propertyID);
    if (!value)
        return nullptr;
    return value->createDeprecatedCSSOMWrapper(*this);
}

String CSSComputedStyleDeclaration::getPropertyValue(const String &propertyName)
{
    if (isCustomPropertyName(propertyName))
        return ComputedStyleExtractor(m_element.ptr(), m_allowVisitedStyle, m_pseudoElementSpecifier).customPropertyText(propertyName);

    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!propertyID)
        return String();
    return getPropertyValue(propertyID);
}

String CSSComputedStyleDeclaration::getPropertyPriority(const String&)
{
    // All computed styles have a priority of not "important".
    return emptyString(); // FIXME: Should this sometimes be null instead of empty, to match a normal style declaration?
}

String CSSComputedStyleDeclaration::getPropertyShorthand(const String&)
{
    return emptyString(); // FIXME: Should this sometimes be null instead of empty, to match a normal style declaration?
}

bool CSSComputedStyleDeclaration::isPropertyImplicit(const String&)
{
    return false;
}

ExceptionOr<void> CSSComputedStyleDeclaration::setProperty(const String&, const String&, const String&)
{
    return Exception { NoModificationAllowedError };
}

ExceptionOr<String> CSSComputedStyleDeclaration::removeProperty(const String&)
{
    return Exception { NoModificationAllowedError };
}
    
RefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValueInternal(CSSPropertyID propertyID)
{
    return getPropertyCSSValue(propertyID);
}

String CSSComputedStyleDeclaration::getPropertyValueInternal(CSSPropertyID propertyID)
{
    return getPropertyValue(propertyID);
}

ExceptionOr<bool> CSSComputedStyleDeclaration::setPropertyInternal(CSSPropertyID, const String&, bool)
{
    return Exception { NoModificationAllowedError };
}

Ref<CSSValueList> ComputedStyleExtractor::getBackgroundShorthandValue()
{
    static const CSSPropertyID propertiesBeforeSlashSeperator[5] = { CSSPropertyBackgroundColor, CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat, CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition };
    static const CSSPropertyID propertiesAfterSlashSeperator[3] = { CSSPropertyBackgroundSize, CSSPropertyBackgroundOrigin, CSSPropertyBackgroundClip };

    auto list = CSSValueList::createSlashSeparated();
    list->append(getCSSPropertyValuesForShorthandProperties(StylePropertyShorthand(CSSPropertyBackground, propertiesBeforeSlashSeperator)));
    list->append(getCSSPropertyValuesForShorthandProperties(StylePropertyShorthand(CSSPropertyBackground, propertiesAfterSlashSeperator)));
    return list;
}

} // namespace WebCore
