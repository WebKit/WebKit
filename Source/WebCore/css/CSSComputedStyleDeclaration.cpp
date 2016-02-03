/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004-2014 Apple Inc. All rights reserved.
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

#include "AnimationController.h"
#include "BasicShapeFunctions.h"
#include "BasicShapes.h"
#include "CSSAnimationTriggerScrollValue.h"
#include "CSSAspectRatioValue.h"
#include "CSSBasicShapes.h"
#include "CSSBorderImage.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontValue.h"
#include "CSSFunctionValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSParser.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSReflectValue.h"
#include "CSSSelector.h"
#include "CSSShadowValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "ContentData.h"
#include "CounterContent.h"
#include "CursorList.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "FontFeatureSettings.h"
#include "HTMLFrameOwnerElement.h"
#include "Pair.h"
#include "PseudoElement.h"
#include "Rect.h"
#include "RenderBlock.h"
#include "RenderBox.h"
#include "RenderStyle.h"
#include "SVGElement.h"
#include "StyleInheritedData.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include "WebKitCSSFilterValue.h"
#include "WebKitCSSTransformValue.h"
#include "WebKitFontFamilyNames.h"
#include "WillChangeData.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(CSS_GRID_LAYOUT)
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "RenderGrid.h"
#endif

#if ENABLE(CSS_SHAPES)
#include "ShapeValue.h"
#endif

#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

#if ENABLE(CSS_SCROLL_SNAP)
#include "LengthRepeat.h"
#include "StyleScrollSnapPoints.h"
#endif

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
#include "AnimationTrigger.h"
#endif

namespace WebCore {

// List of all properties we know how to compute, omitting shorthands.
static const CSSPropertyID computedProperties[] = {
    CSSPropertyAlt,
    CSSPropertyAnimationDelay,
    CSSPropertyAnimationDirection,
    CSSPropertyAnimationDuration,
    CSSPropertyAnimationFillMode,
    CSSPropertyAnimationIterationCount,
    CSSPropertyAnimationName,
    CSSPropertyAnimationPlayState,
    CSSPropertyAnimationTimingFunction,
    CSSPropertyBackgroundAttachment,
    CSSPropertyBackgroundBlendMode,
    CSSPropertyBackgroundClip,
    CSSPropertyBackgroundColor,
    CSSPropertyBackgroundImage,
    CSSPropertyBackgroundOrigin,
    CSSPropertyBackgroundPosition, // more-specific background-position-x/y are non-standard
    CSSPropertyBackgroundRepeat,
    CSSPropertyBackgroundSize,
    CSSPropertyBorderBottomColor,
    CSSPropertyBorderBottomLeftRadius,
    CSSPropertyBorderBottomRightRadius,
    CSSPropertyBorderBottomStyle,
    CSSPropertyBorderBottomWidth,
    CSSPropertyBorderCollapse,
    CSSPropertyBorderImageOutset,
    CSSPropertyBorderImageRepeat,
    CSSPropertyBorderImageSlice,
    CSSPropertyBorderImageSource,
    CSSPropertyBorderImageWidth,
    CSSPropertyBorderLeftColor,
    CSSPropertyBorderLeftStyle,
    CSSPropertyBorderLeftWidth,
    CSSPropertyBorderRightColor,
    CSSPropertyBorderRightStyle,
    CSSPropertyBorderRightWidth,
    CSSPropertyBorderTopColor,
    CSSPropertyBorderTopLeftRadius,
    CSSPropertyBorderTopRightRadius,
    CSSPropertyBorderTopStyle,
    CSSPropertyBorderTopWidth,
    CSSPropertyBottom,
    CSSPropertyBoxShadow,
    CSSPropertyBoxSizing,
    CSSPropertyCaptionSide,
    CSSPropertyClear,
    CSSPropertyClip,
    CSSPropertyColor,
    CSSPropertyContent,
    CSSPropertyCursor,
    CSSPropertyDirection,
    CSSPropertyDisplay,
    CSSPropertyEmptyCells,
    CSSPropertyFloat,
    CSSPropertyFontFamily,
    CSSPropertyFontSize,
    CSSPropertyFontStyle,
    CSSPropertyFontSynthesis,
    CSSPropertyFontVariant,
    CSSPropertyFontWeight,
    CSSPropertyHeight,
#if ENABLE(CSS_IMAGE_ORIENTATION)
    CSSPropertyImageOrientation,
#endif
    CSSPropertyImageRendering,
#if ENABLE(CSS_IMAGE_RESOLUTION)
    CSSPropertyImageResolution,
#endif
    CSSPropertyLeft,
    CSSPropertyLetterSpacing,
    CSSPropertyLineHeight,
    CSSPropertyListStyleImage,
    CSSPropertyListStylePosition,
    CSSPropertyListStyleType,
    CSSPropertyMarginBottom,
    CSSPropertyMarginLeft,
    CSSPropertyMarginRight,
    CSSPropertyMarginTop,
    CSSPropertyMaxHeight,
    CSSPropertyMaxWidth,
    CSSPropertyMinHeight,
    CSSPropertyMinWidth,
    CSSPropertyOpacity,
    CSSPropertyOrphans,
    CSSPropertyOutlineColor,
    CSSPropertyOutlineOffset,
    CSSPropertyOutlineStyle,
    CSSPropertyOutlineWidth,
    CSSPropertyOverflowWrap,
    CSSPropertyOverflowX,
    CSSPropertyOverflowY,
    CSSPropertyPaddingBottom,
    CSSPropertyPaddingLeft,
    CSSPropertyPaddingRight,
    CSSPropertyPaddingTop,
    CSSPropertyPageBreakAfter,
    CSSPropertyPageBreakBefore,
    CSSPropertyPageBreakInside,
    CSSPropertyPointerEvents,
    CSSPropertyPosition,
    CSSPropertyResize,
    CSSPropertyRight,
    CSSPropertySpeak,
    CSSPropertyTableLayout,
    CSSPropertyTabSize,
    CSSPropertyTextAlign,
    CSSPropertyTextDecoration,
#if ENABLE(CSS3_TEXT)
    CSSPropertyWebkitTextAlignLast,
    CSSPropertyWebkitTextJustify,
#endif // CSS3_TEXT
    CSSPropertyWebkitTextDecorationLine,
    CSSPropertyWebkitTextDecorationStyle,
    CSSPropertyWebkitTextDecorationColor,
    CSSPropertyWebkitTextDecorationSkip,
    CSSPropertyWebkitTextUnderlinePosition,
    CSSPropertyTextIndent,
    CSSPropertyTextRendering,
    CSSPropertyTextShadow,
    CSSPropertyTextOverflow,
    CSSPropertyTextTransform,
    CSSPropertyTop,
    CSSPropertyTransform,
    CSSPropertyTransformOrigin,
    CSSPropertyTransformStyle,
    CSSPropertyTransitionDelay,
    CSSPropertyTransitionDuration,
    CSSPropertyTransitionProperty,
    CSSPropertyTransitionTimingFunction,
    CSSPropertyUnicodeBidi,
    CSSPropertyVerticalAlign,
    CSSPropertyVisibility,
    CSSPropertyWhiteSpace,
    CSSPropertyWidows,
    CSSPropertyWidth,
    CSSPropertyWordBreak,
    CSSPropertyWordSpacing,
    CSSPropertyWordWrap,
#if ENABLE(CSS_SCROLL_SNAP)
    CSSPropertyWebkitScrollSnapType,
    CSSPropertyWebkitScrollSnapPointsX,
    CSSPropertyWebkitScrollSnapPointsY,
    CSSPropertyWebkitScrollSnapDestination,
    CSSPropertyWebkitScrollSnapCoordinate,
#endif
    CSSPropertyZIndex,
    CSSPropertyZoom,
    CSSPropertyWebkitAnimationDelay,
    CSSPropertyWebkitAnimationDirection,
    CSSPropertyWebkitAnimationDuration,
    CSSPropertyWebkitAnimationFillMode,
    CSSPropertyWebkitAnimationIterationCount,
    CSSPropertyWebkitAnimationName,
    CSSPropertyWebkitAnimationPlayState,
    CSSPropertyWebkitAnimationTimingFunction,
#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
    CSSPropertyWebkitAnimationTrigger,
#endif
    CSSPropertyWebkitAppearance,
    CSSPropertyWebkitBackfaceVisibility,
    CSSPropertyWebkitBackgroundClip,
    CSSPropertyWebkitBackgroundComposite,
    CSSPropertyWebkitBackgroundOrigin,
    CSSPropertyWebkitBackgroundSize,
#if ENABLE(CSS_COMPOSITING)
    CSSPropertyMixBlendMode,
    CSSPropertyIsolation,
#endif
    CSSPropertyWebkitBorderFit,
    CSSPropertyWebkitBorderHorizontalSpacing,
    CSSPropertyWebkitBorderImage,
    CSSPropertyWebkitBorderVerticalSpacing,
    CSSPropertyWebkitBoxAlign,
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    CSSPropertyWebkitBoxDecorationBreak,
#endif
    CSSPropertyWebkitBoxDirection,
    CSSPropertyWebkitBoxFlex,
    CSSPropertyWebkitBoxFlexGroup,
    CSSPropertyWebkitBoxLines,
    CSSPropertyWebkitBoxOrdinalGroup,
    CSSPropertyWebkitBoxOrient,
    CSSPropertyWebkitBoxPack,
    CSSPropertyWebkitBoxReflect,
    CSSPropertyWebkitBoxShadow,
    CSSPropertyWebkitClipPath,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyWebkitColumnAxis,
    CSSPropertyColumnCount,
    CSSPropertyColumnFill,
    CSSPropertyColumnGap,
    CSSPropertyColumnProgression,
    CSSPropertyColumnRuleColor,
    CSSPropertyColumnRuleStyle,
    CSSPropertyColumnRuleWidth,
    CSSPropertyColumnSpan,
    CSSPropertyColumnWidth,
#if ENABLE(CURSOR_VISIBILITY)
    CSSPropertyWebkitCursorVisibility,
#endif
#if ENABLE(DASHBOARD_SUPPORT)
    CSSPropertyWebkitDashboardRegion,
#endif
    CSSPropertyAlignContent,
    CSSPropertyAlignItems,
    CSSPropertyAlignSelf,
    CSSPropertyFilter,
    CSSPropertyFlexBasis,
    CSSPropertyFlexGrow,
    CSSPropertyFlexShrink,
    CSSPropertyFlexDirection,
    CSSPropertyFlexWrap,
    CSSPropertyJustifyContent,
    CSSPropertyJustifySelf,
    CSSPropertyJustifyItems,
#if ENABLE(FILTERS_LEVEL_2)
    CSSPropertyWebkitBackdropFilter,
#endif
    CSSPropertyWebkitFontKerning,
    CSSPropertyWebkitFontSmoothing,
    CSSPropertyFontVariantLigatures,
    CSSPropertyFontVariantPosition,
    CSSPropertyFontVariantCaps,
    CSSPropertyFontVariantNumeric,
    CSSPropertyFontVariantAlternates,
    CSSPropertyFontVariantEastAsian,
#if ENABLE(CSS_GRID_LAYOUT)
    CSSPropertyWebkitGridAutoColumns,
    CSSPropertyWebkitGridAutoFlow,
    CSSPropertyWebkitGridAutoRows,
    CSSPropertyWebkitGridColumnEnd,
    CSSPropertyWebkitGridColumnStart,
    CSSPropertyWebkitGridTemplateAreas,
    CSSPropertyWebkitGridTemplateColumns,
    CSSPropertyWebkitGridTemplateRows,
    CSSPropertyWebkitGridRowEnd,
    CSSPropertyWebkitGridRowStart,
    CSSPropertyWebkitGridColumnGap,
    CSSPropertyWebkitGridRowGap,
#endif
    CSSPropertyWebkitHyphenateCharacter,
    CSSPropertyWebkitHyphenateLimitAfter,
    CSSPropertyWebkitHyphenateLimitBefore,
    CSSPropertyWebkitHyphenateLimitLines,
    CSSPropertyWebkitHyphens,
    CSSPropertyWebkitInitialLetter,
    CSSPropertyWebkitLineAlign,
    CSSPropertyWebkitLineBoxContain,
    CSSPropertyWebkitLineBreak,
    CSSPropertyWebkitLineClamp,
    CSSPropertyWebkitLineGrid,
    CSSPropertyWebkitLineSnap,
    CSSPropertyWebkitLocale,
    CSSPropertyWebkitMarginBeforeCollapse,
    CSSPropertyWebkitMarginAfterCollapse,
    CSSPropertyWebkitMarqueeDirection,
    CSSPropertyWebkitMarqueeIncrement,
    CSSPropertyWebkitMarqueeRepetition,
    CSSPropertyWebkitMarqueeStyle,
    CSSPropertyWebkitMaskBoxImage,
    CSSPropertyWebkitMaskBoxImageOutset,
    CSSPropertyWebkitMaskBoxImageRepeat,
    CSSPropertyWebkitMaskBoxImageSlice,
    CSSPropertyWebkitMaskBoxImageSource,
    CSSPropertyWebkitMaskBoxImageWidth,
    CSSPropertyWebkitMaskClip,
    CSSPropertyWebkitMaskComposite,
    CSSPropertyWebkitMaskImage,
    CSSPropertyWebkitMaskOrigin,
    CSSPropertyWebkitMaskPosition,
    CSSPropertyWebkitMaskRepeat,
    CSSPropertyWebkitMaskSize,
    CSSPropertyWebkitMaskSourceType,
    CSSPropertyWebkitNbspMode,
    CSSPropertyOrder,
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    CSSPropertyWebkitOverflowScrolling,
#endif
    CSSPropertyPerspective,
    CSSPropertyPerspectiveOrigin,
    CSSPropertyWebkitPrintColorAdjust,
    CSSPropertyWebkitRtlOrdering,
#if PLATFORM(IOS)
    CSSPropertyWebkitTouchCallout,
#endif
#if ENABLE(CSS_SHAPES)
    CSSPropertyWebkitShapeOutside,
#endif
#if ENABLE(TOUCH_EVENTS)
    CSSPropertyWebkitTapHighlightColor,
#endif
    CSSPropertyWebkitTextCombine,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextEmphasisColor,
    CSSPropertyWebkitTextEmphasisPosition,
    CSSPropertyWebkitTextEmphasisStyle,
    CSSPropertyWebkitTextFillColor,
    CSSPropertyWebkitTextOrientation,
    CSSPropertyWebkitTextSecurity,
#if ENABLE(IOS_TEXT_AUTOSIZING)
    CSSPropertyWebkitTextSizeAdjust,
#endif
    CSSPropertyWebkitTextStrokeColor,
    CSSPropertyWebkitTextStrokeWidth,
    CSSPropertyWebkitTextZoom,
    CSSPropertyWebkitTransformStyle,
    CSSPropertyWebkitTransitionDelay,
    CSSPropertyWebkitTransitionDuration,
    CSSPropertyWebkitTransitionProperty,
    CSSPropertyWebkitTransitionTimingFunction,
    CSSPropertyWebkitUserDrag,
    CSSPropertyWebkitUserModify,
    CSSPropertyWebkitUserSelect,
    CSSPropertyWebkitWritingMode,
#if ENABLE(CSS_REGIONS)
    CSSPropertyWebkitFlowInto,
    CSSPropertyWebkitFlowFrom,
    CSSPropertyWebkitRegionBreakAfter,
    CSSPropertyWebkitRegionBreakBefore,
    CSSPropertyWebkitRegionBreakInside,
    CSSPropertyWebkitRegionFragment,
#endif
#if ENABLE(CSS_SHAPES)
    CSSPropertyWebkitShapeMargin,
    CSSPropertyWebkitShapeImageThreshold,
#endif
    CSSPropertyBufferedRendering,
    CSSPropertyClipPath,
    CSSPropertyClipRule,
    CSSPropertyCx,
    CSSPropertyCy,
    CSSPropertyMask,
    CSSPropertyFilter,
    CSSPropertyFloodColor,
    CSSPropertyFloodOpacity,
    CSSPropertyLightingColor,
    CSSPropertyStopColor,
    CSSPropertyStopOpacity,
    CSSPropertyColorInterpolation,
    CSSPropertyColorInterpolationFilters,
    CSSPropertyColorRendering,
    CSSPropertyFill,
    CSSPropertyFillOpacity,
    CSSPropertyFillRule,
    CSSPropertyMarkerEnd,
    CSSPropertyMarkerMid,
    CSSPropertyMarkerStart,
    CSSPropertyMaskType,
    CSSPropertyPaintOrder,
    CSSPropertyR,
    CSSPropertyRx,
    CSSPropertyRy,
    CSSPropertyShapeRendering,
    CSSPropertyStroke,
    CSSPropertyStrokeDasharray,
    CSSPropertyStrokeDashoffset,
    CSSPropertyStrokeLinecap,
    CSSPropertyStrokeLinejoin,
    CSSPropertyStrokeMiterlimit,
    CSSPropertyStrokeOpacity,
    CSSPropertyStrokeWidth,
    CSSPropertyAlignmentBaseline,
    CSSPropertyBaselineShift,
    CSSPropertyDominantBaseline,
    CSSPropertyKerning,
    CSSPropertyTextAnchor,
    CSSPropertyWritingMode,
    CSSPropertyGlyphOrientationHorizontal,
    CSSPropertyGlyphOrientationVertical,
    CSSPropertyWebkitSvgShadow,
    CSSPropertyVectorEffect,
    CSSPropertyX,
    CSSPropertyY
};

const unsigned numComputedProperties = WTF_ARRAY_LENGTH(computedProperties);

static CSSValueID valueForRepeatRule(int rule)
{
    switch (rule) {
        case RepeatImageRule:
            return CSSValueRepeat;
        case RoundImageRule:
            return CSSValueRound;
        case SpaceImageRule:
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
        return CSSValuePool::singleton().createValue(length.percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
    if (length.isFixed())
        return CSSValuePool::singleton().createValue(length.value(), CSSPrimitiveValue::CSS_NUMBER);

    // Calculating the actual length currently in use would require most of the code from RenderBoxModelObject::paintNinePieceImage.
    // And even if we could do that, it's not clear if that's exactly what we'd want during animation.
    // FIXME: For now, just return 0.
    ASSERT(length.isCalculated());
    return CSSValuePool::singleton().createValue(0, CSSPrimitiveValue::CSS_NUMBER);
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
    quad->setTop(top.release());
    quad->setRight(right.release());
    quad->setBottom(bottom.release());
    quad->setLeft(left.release());

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
        top = cssValuePool.createValue(box.top().value(), CSSPrimitiveValue::CSS_NUMBER);
    else
        top = cssValuePool.createValue(box.top());

    if (box.right() == box.top() && box.bottom() == box.top() && box.left() == box.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (box.right().isRelative())
            right = cssValuePool.createValue(box.right().value(), CSSPrimitiveValue::CSS_NUMBER);
        else
            right = cssValuePool.createValue(box.right());

        if (box.bottom() == box.top() && box.right() == box.left()) {
            bottom = top;
            left = right;
        } else {
            if (box.bottom().isRelative())
                bottom = cssValuePool.createValue(box.bottom().value(), CSSPrimitiveValue::CSS_NUMBER);
            else
                bottom = cssValuePool.createValue(box.bottom());

            if (box.left() == box.right())
                left = right;
            else {
                if (box.left().isRelative())
                    left = cssValuePool.createValue(box.left().value(), CSSPrimitiveValue::CSS_NUMBER);
                else
                    left = cssValuePool.createValue(box.left());
            }
        }
    }

    auto quad = Quad::create();
    quad->setTop(top);
    quad->setRight(right);
    quad->setBottom(bottom);
    quad->setLeft(left);

    return cssValuePool.createValue(WTFMove(quad));
}

static Ref<CSSValue> valueForNinePieceImageRepeat(const NinePieceImage& image)
{
    auto& cssValuePool = CSSValuePool::singleton();
    RefPtr<CSSPrimitiveValue> horizontalRepeat = cssValuePool.createIdentifierValue(valueForRepeatRule(image.horizontalRule()));

    RefPtr<CSSPrimitiveValue> verticalRepeat;
    if (image.horizontalRule() == image.verticalRule())
        verticalRepeat = horizontalRepeat;
    else
        verticalRepeat = cssValuePool.createIdentifierValue(valueForRepeatRule(image.verticalRule()));
    return cssValuePool.createValue(Pair::create(horizontalRepeat.release(), verticalRepeat.release()));
}

static Ref<CSSValue> valueForNinePieceImage(const NinePieceImage& image)
{
    if (!image.hasImage())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    // Image first.
    RefPtr<CSSValue> imageValue;
    if (image.image())
        imageValue = image.image()->cssValue();

    // Create the image slice.
    RefPtr<CSSBorderImageSliceValue> imageSlices = valueForNinePieceImageSlice(image);

    // Create the border area slices.
    RefPtr<CSSValue> borderSlices = valueForNinePieceImageQuad(image.borderSlices());

    // Create the border outset.
    RefPtr<CSSValue> outset = valueForNinePieceImageQuad(image.outset());

    // Create the repeat rules.
    RefPtr<CSSValue> repeat = valueForNinePieceImageRepeat(image);

    return createBorderImageValue(imageValue.release(), imageSlices.release(), borderSlices.release(), outset.release(), repeat.release());
}

inline static Ref<CSSPrimitiveValue> zoomAdjustedPixelValue(double value, const RenderStyle& style)
{
    return CSSValuePool::singleton().createValue(adjustFloatForAbsoluteZoom(value, style), CSSPrimitiveValue::CSS_PX);
}

inline static Ref<CSSPrimitiveValue> zoomAdjustedNumberValue(double value, const RenderStyle& style)
{
    return CSSValuePool::singleton().createValue(value / style.effectiveZoom(), CSSPrimitiveValue::CSS_NUMBER);
}

static Ref<CSSValue> zoomAdjustedPixelValueForLength(const Length& length, const RenderStyle& style)
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
        offset = CSSValuePool::singleton().createValue(reflection->offset().percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        offset = zoomAdjustedPixelValue(reflection->offset().value(), style);

    RefPtr<CSSPrimitiveValue> direction;
    switch (reflection->direction()) {
    case ReflectionBelow:
        direction = CSSValuePool::singleton().createIdentifierValue(CSSValueBelow);
        break;
    case ReflectionAbove:
        direction = CSSValuePool::singleton().createIdentifierValue(CSSValueAbove);
        break;
    case ReflectionLeft:
        direction = CSSValuePool::singleton().createIdentifierValue(CSSValueLeft);
        break;
    case ReflectionRight:
        direction = CSSValuePool::singleton().createIdentifierValue(CSSValueRight);
        break;
    }

    return CSSReflectValue::create(direction.release(), offset.release(), valueForNinePieceImage(reflection->mask()));
}

static Ref<CSSValueList> createPositionListForLayer(CSSPropertyID propertyID, const FillLayer* layer, const RenderStyle& style)
{
    auto positionList = CSSValueList::createSpaceSeparated();
    if (layer->isBackgroundOriginSet()) {
        ASSERT_UNUSED(propertyID, propertyID == CSSPropertyBackgroundPosition || propertyID == CSSPropertyWebkitMaskPosition);
        positionList.get().append(CSSValuePool::singleton().createValue(layer->backgroundXOrigin()));
    }
    positionList.get().append(zoomAdjustedPixelValueForLength(layer->xPosition(), style));
    if (layer->isBackgroundOriginSet()) {
        ASSERT(propertyID == CSSPropertyBackgroundPosition || propertyID == CSSPropertyWebkitMaskPosition);
        positionList.get().append(CSSValuePool::singleton().createValue(layer->backgroundYOrigin()));
    }
    positionList.get().append(zoomAdjustedPixelValueForLength(layer->yPosition(), style));
    return positionList;
}

static RefPtr<CSSValue> positionOffsetValue(RenderStyle& style, CSSPropertyID propertyID)
{
    Length length;
    switch (propertyID) {
        case CSSPropertyLeft:
            length = style.left();
            break;
        case CSSPropertyRight:
            length = style.right();
            break;
        case CSSPropertyTop:
            length = style.top();
            break;
        case CSSPropertyBottom:
            length = style.bottom();
            break;
        default:
            return nullptr;
    }

    if (style.hasOutOfFlowPosition()) {
        if (length.isFixed())
            return zoomAdjustedPixelValue(length.value(), style);

        return CSSValuePool::singleton().createValue(length);
    }

    if (style.hasInFlowPosition()) {
        // FIXME: It's not enough to simply return "auto" values for one offset if the other side is defined.
        // In other words if left is auto and right is not auto, then left's computed value is negative right().
        // So we should get the opposite length unit and see if it is auto.
        return CSSValuePool::singleton().createValue(length);
    }

    return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
}

RefPtr<CSSPrimitiveValue> ComputedStyleExtractor::currentColorOrValidColor(RenderStyle* style, const Color& color) const
{
    // This function does NOT look at visited information, so that computed style doesn't expose that.
    if (!color.isValid())
        return CSSValuePool::singleton().createColorValue(style->color().rgb());
    return CSSValuePool::singleton().createColorValue(color.rgb());
}

static Ref<CSSPrimitiveValue> percentageOrZoomAdjustedValue(Length length, const RenderStyle& style)
{
    if (length.isPercent())
        return CSSValuePool::singleton().createValue(length.percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
    
    return zoomAdjustedPixelValue(valueForLength(length, 0), style);
}

static Ref<CSSPrimitiveValue> autoOrZoomAdjustedValue(Length length, const RenderStyle& style)
{
    if (length.isAuto())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);

    return zoomAdjustedPixelValue(valueForLength(length, 0), style);
}

static Ref<CSSValueList> getBorderRadiusCornerValues(const LengthSize& radius, const RenderStyle& style)
{
    auto list = CSSValueList::createSpaceSeparated();
    list.get().append(percentageOrZoomAdjustedValue(radius.width(), style));
    list.get().append(percentageOrZoomAdjustedValue(radius.height(), style));
    return list;
}

static Ref<CSSValue> getBorderRadiusCornerValue(const LengthSize& radius, const RenderStyle& style)
{
    if (radius.width() == radius.height())
        return percentageOrZoomAdjustedValue(radius.width(), style);

    return getBorderRadiusCornerValues(radius, style);
}

static Ref<CSSValueList> getBorderRadiusShorthandValue(const RenderStyle& style)
{
    auto list = CSSValueList::createSlashSeparated();
    bool showHorizontalBottomLeft = style.borderTopRightRadius().width() != style.borderBottomLeftRadius().width();
    bool showHorizontalBottomRight = showHorizontalBottomLeft || (style.borderBottomRightRadius().width() != style.borderTopLeftRadius().width());
    bool showHorizontalTopRight = showHorizontalBottomRight || (style.borderTopRightRadius().width() != style.borderTopLeftRadius().width());

    bool showVerticalBottomLeft = style.borderTopRightRadius().height() != style.borderBottomLeftRadius().height();
    bool showVerticalBottomRight = showVerticalBottomLeft || (style.borderBottomRightRadius().height() != style.borderTopLeftRadius().height());
    bool showVerticalTopRight = showVerticalBottomRight || (style.borderTopRightRadius().height() != style.borderTopLeftRadius().height());

    RefPtr<CSSValueList> topLeftRadius = getBorderRadiusCornerValues(style.borderTopLeftRadius(), style);
    RefPtr<CSSValueList> topRightRadius = getBorderRadiusCornerValues(style.borderTopRightRadius(), style);
    RefPtr<CSSValueList> bottomRightRadius = getBorderRadiusCornerValues(style.borderBottomRightRadius(), style);
    RefPtr<CSSValueList> bottomLeftRadius = getBorderRadiusCornerValues(style.borderBottomLeftRadius(), style);

    RefPtr<CSSValueList> horizontalRadii = CSSValueList::createSpaceSeparated();
    horizontalRadii->append(*topLeftRadius->item(0));
    if (showHorizontalTopRight)
        horizontalRadii->append(*topRightRadius->item(0));
    if (showHorizontalBottomRight)
        horizontalRadii->append(*bottomRightRadius->item(0));
    if (showHorizontalBottomLeft)
        horizontalRadii->append(*bottomLeftRadius->item(0));

    list.get().append(horizontalRadii.releaseNonNull());

    RefPtr<CSSValueList> verticalRadiiList = CSSValueList::createSpaceSeparated();
    verticalRadiiList->append(*topLeftRadius->item(1));
    if (showVerticalTopRight)
        verticalRadiiList->append(*topRightRadius->item(1));
    if (showVerticalBottomRight)
        verticalRadiiList->append(*bottomRightRadius->item(1));
    if (showVerticalBottomLeft)
        verticalRadiiList->append(*bottomLeftRadius->item(1));

    if (!verticalRadiiList->equals(downcast<CSSValueList>(*list.get().item(0))))
        list.get().append(verticalRadiiList.releaseNonNull());

    return list;
}

static LayoutRect sizingBox(RenderObject& renderer)
{
    if (!is<RenderBox>(renderer))
        return LayoutRect();

    auto& box = downcast<RenderBox>(renderer);
    return box.style().boxSizing() == BORDER_BOX ? box.borderBoxRect() : box.computedCSSContentBoxRect();
}

static Ref<WebKitCSSTransformValue> matrixTransformValue(const TransformationMatrix& transform, const RenderStyle& style)
{
    RefPtr<WebKitCSSTransformValue> transformValue;
    auto& cssValuePool = CSSValuePool::singleton();
    if (transform.isAffine()) {
        transformValue = WebKitCSSTransformValue::create(WebKitCSSTransformValue::MatrixTransformOperation);

        transformValue->append(cssValuePool.createValue(transform.a(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.b(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.c(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.d(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(zoomAdjustedNumberValue(transform.e(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.f(), style));
    } else {
        transformValue = WebKitCSSTransformValue::create(WebKitCSSTransformValue::Matrix3DTransformOperation);

        transformValue->append(cssValuePool.createValue(transform.m11(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m12(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m13(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m14(), CSSPrimitiveValue::CSS_NUMBER));

        transformValue->append(cssValuePool.createValue(transform.m21(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m22(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m23(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m24(), CSSPrimitiveValue::CSS_NUMBER));

        transformValue->append(cssValuePool.createValue(transform.m31(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m32(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m33(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool.createValue(transform.m34(), CSSPrimitiveValue::CSS_NUMBER));

        transformValue->append(zoomAdjustedNumberValue(transform.m41(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.m42(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.m43(), style));
        transformValue->append(cssValuePool.createValue(transform.m44(), CSSPrimitiveValue::CSS_NUMBER));
    }

    return transformValue.releaseNonNull();
}

static Ref<CSSValue> computedTransform(RenderObject* renderer, const RenderStyle& style)
{
    if (!renderer || !renderer->hasTransform())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    FloatRect pixelSnappedRect;
    if (is<RenderBox>(*renderer))
        pixelSnappedRect = snapRectToDevicePixels(downcast<RenderBox>(*renderer).borderBoxRect(), renderer->document().deviceScaleFactor());

    TransformationMatrix transform;
    style.applyTransform(transform, pixelSnappedRect, RenderStyle::ExcludeTransformOrigin);
    // Note that this does not flatten to an affine transform if ENABLE(3D_TRANSFORMS) is off, by design.

    // FIXME: Need to print out individual functions (https://bugs.webkit.org/show_bug.cgi?id=23924)
    auto list = CSSValueList::createSpaceSeparated();
    list.get().append(matrixTransformValue(transform, style));
    return WTFMove(list);
}

static inline Ref<CSSPrimitiveValue> adjustLengthForZoom(double length, const RenderStyle& style, AdjustPixelValuesForComputedStyle adjust)
{
    return adjust == AdjustPixelValues ? zoomAdjustedPixelValue(length, style) : CSSValuePool::singleton().createValue(length, CSSPrimitiveValue::CSS_PX);
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
        RefPtr<CSSPrimitiveValue> x = adjustLengthForZoom(currShadowData->x(), style, adjust);
        RefPtr<CSSPrimitiveValue> y = adjustLengthForZoom(currShadowData->y(), style, adjust);
        RefPtr<CSSPrimitiveValue> blur = adjustLengthForZoom(currShadowData->radius(), style, adjust);
        RefPtr<CSSPrimitiveValue> spread = propertyID == CSSPropertyTextShadow ? PassRefPtr<CSSPrimitiveValue>() : adjustLengthForZoom(currShadowData->spread(), style, adjust);
        RefPtr<CSSPrimitiveValue> style = propertyID == CSSPropertyTextShadow || currShadowData->style() == Normal ? PassRefPtr<CSSPrimitiveValue>() : cssValuePool.createIdentifierValue(CSSValueInset);
        RefPtr<CSSPrimitiveValue> color = cssValuePool.createColorValue(currShadowData->color().rgb());
        list.get().prepend(CSSShadowValue::create(x.release(), y.release(), blur.release(), spread.release(), style.release(), color.release()));
    }
    return WTFMove(list);
}

Ref<CSSValue> ComputedStyleExtractor::valueForFilter(const RenderStyle& style, const FilterOperations& filterOperations, AdjustPixelValuesForComputedStyle adjust)
{
    auto& cssValuePool = CSSValuePool::singleton();
    if (filterOperations.operations().isEmpty())
        return cssValuePool.createIdentifierValue(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();

    RefPtr<WebKitCSSFilterValue> filterValue;

    Vector<RefPtr<FilterOperation>>::const_iterator end = filterOperations.operations().end();
    for (Vector<RefPtr<FilterOperation>>::const_iterator it = filterOperations.operations().begin(); it != end; ++it) {
        FilterOperation& filterOperation = **it;
        switch (filterOperation.type()) {
        case FilterOperation::REFERENCE: {
            ReferenceFilterOperation& referenceOperation = downcast<ReferenceFilterOperation>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::ReferenceFilterOperation);
            filterValue->append(cssValuePool.createValue(referenceOperation.url(), CSSPrimitiveValue::CSS_URI));
            break;
        }
        case FilterOperation::GRAYSCALE: {
            BasicColorMatrixFilterOperation& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::GrayscaleFilterOperation);
            filterValue->append(cssValuePool.createValue(colorMatrixOperation.amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::SEPIA: {
            BasicColorMatrixFilterOperation& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::SepiaFilterOperation);
            filterValue->append(cssValuePool.createValue(colorMatrixOperation.amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::SATURATE: {
            BasicColorMatrixFilterOperation& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::SaturateFilterOperation);
            filterValue->append(cssValuePool.createValue(colorMatrixOperation.amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            BasicColorMatrixFilterOperation& colorMatrixOperation = downcast<BasicColorMatrixFilterOperation>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::HueRotateFilterOperation);
            filterValue->append(cssValuePool.createValue(colorMatrixOperation.amount(), CSSPrimitiveValue::CSS_DEG));
            break;
        }
        case FilterOperation::INVERT: {
            BasicComponentTransferFilterOperation& componentTransferOperation = downcast<BasicComponentTransferFilterOperation>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::InvertFilterOperation);
            filterValue->append(cssValuePool.createValue(componentTransferOperation.amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::OPACITY: {
            BasicComponentTransferFilterOperation& componentTransferOperation = downcast<BasicComponentTransferFilterOperation>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::OpacityFilterOperation);
            filterValue->append(cssValuePool.createValue(componentTransferOperation.amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::BRIGHTNESS: {
            BasicComponentTransferFilterOperation& brightnessOperation = downcast<BasicComponentTransferFilterOperation>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::BrightnessFilterOperation);
            filterValue->append(cssValuePool.createValue(brightnessOperation.amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::CONTRAST: {
            BasicComponentTransferFilterOperation& contrastOperation = downcast<BasicComponentTransferFilterOperation>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::ContrastFilterOperation);
            filterValue->append(cssValuePool.createValue(contrastOperation.amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::BLUR: {
            BlurFilterOperation& blurOperation = downcast<BlurFilterOperation>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::BlurFilterOperation);
            filterValue->append(adjustLengthForZoom(blurOperation.stdDeviation(), style, adjust));
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            DropShadowFilterOperation& dropShadowOperation = downcast<DropShadowFilterOperation>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::DropShadowFilterOperation);
            // We want our computed style to look like that of a text shadow (has neither spread nor inset style).
            ShadowData shadowData = ShadowData(dropShadowOperation.location(), dropShadowOperation.stdDeviation(), 0, Normal, false, dropShadowOperation.color());
            filterValue->append(valueForShadow(&shadowData, CSSPropertyTextShadow, style, adjust));
            break;
        }
        default:
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::UnknownFilterOperation);
            break;
        }
        list.get().append(filterValue.releaseNonNull());
    }

    return WTFMove(list);
}

#if ENABLE(CSS_GRID_LAYOUT)
static Ref<CSSValue> specifiedValueForGridTrackBreadth(const GridLength& trackBreadth, const RenderStyle& style)
{
    if (!trackBreadth.isLength())
        return CSSValuePool::singleton().createValue(trackBreadth.flex(), CSSPrimitiveValue::CSS_FR);

    const Length& trackBreadthLength = trackBreadth.length();
    if (trackBreadthLength.isAuto())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    return zoomAdjustedPixelValueForLength(trackBreadthLength, style);
}

static Ref<CSSValue> specifiedValueForGridTrackSize(const GridTrackSize& trackSize, const RenderStyle& style)
{
    switch (trackSize.type()) {
    case LengthTrackSizing:
        return specifiedValueForGridTrackBreadth(trackSize.length(), style);
    default:
        ASSERT(trackSize.type() == MinMaxTrackSizing);
        RefPtr<CSSValueList> minMaxTrackBreadths = CSSValueList::createCommaSeparated();
        minMaxTrackBreadths->append(specifiedValueForGridTrackBreadth(trackSize.minTrackBreadth(), style));
        minMaxTrackBreadths->append(specifiedValueForGridTrackBreadth(trackSize.maxTrackBreadth(), style));
        return CSSFunctionValue::create("minmax(", minMaxTrackBreadths);
    }
}

static void addValuesForNamedGridLinesAtIndex(const OrderedNamedGridLinesMap& orderedNamedGridLines, size_t i, CSSValueList& list)
{
    const Vector<String>& namedGridLines = orderedNamedGridLines.get(i);
    if (namedGridLines.isEmpty())
        return;

    auto& cssValuePool = CSSValuePool::singleton();
    RefPtr<CSSGridLineNamesValue> lineNames = CSSGridLineNamesValue::create();
    for (auto& name : namedGridLines)
        lineNames->append(cssValuePool.createValue(name, CSSPrimitiveValue::CSS_STRING));
    list.append(lineNames.releaseNonNull());
}

static Ref<CSSValue> valueForGridTrackList(GridTrackSizingDirection direction, RenderObject* renderer, const RenderStyle& style)
{
    const Vector<GridTrackSize>& trackSizes = direction == ForColumns ? style.gridColumns() : style.gridRows();
    const OrderedNamedGridLinesMap& orderedNamedGridLines = direction == ForColumns ? style.orderedNamedGridColumnLines() : style.orderedNamedGridRowLines();
    bool isRenderGrid = is<RenderGrid>(renderer);

    // Handle the 'none' case.
    bool trackListIsEmpty = trackSizes.isEmpty();
    if (isRenderGrid && trackListIsEmpty) {
        // For grids we should consider every listed track, whether implicitly or explicitly created. If we don't have
        // any explicit track and there are no children then there are no implicit tracks. We cannot simply check the
        // number of rows/columns in our internal grid representation because it's always at least 1x1 (see r143331).
        trackListIsEmpty = !downcast<RenderBlock>(*renderer).firstChild();
    }

    if (trackListIsEmpty) {
        ASSERT(orderedNamedGridLines.isEmpty());
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    }

    auto list = CSSValueList::createSpaceSeparated();
    unsigned insertionIndex;
    if (isRenderGrid) {
        const Vector<LayoutUnit>& trackPositions = direction == ForColumns ? downcast<RenderGrid>(*renderer).columnPositions() : downcast<RenderGrid>(*renderer).rowPositions();
        // There are at least #tracks + 1 grid lines (trackPositions). Apart from that, the grid container can generate implicit grid tracks,
        // so we'll have more trackPositions than trackSizes as the latter only contain the explicit grid.
        ASSERT(trackPositions.size() - 1 >= trackSizes.size());

        unsigned i = 0;
        LayoutUnit gutterSize = downcast<RenderGrid>(*renderer).guttersSize(direction, 2);
        for (; i < trackPositions.size() - 2; ++i) {
            addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, i, list.get());
            list.get().append(zoomAdjustedPixelValue(trackPositions[i + 1] - trackPositions[i] - gutterSize, style));
        }
        // Last track line does not have any gutter.
        addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, i, list.get());
        list.get().append(zoomAdjustedPixelValue(trackPositions[i + 1] - trackPositions[i], style));
        insertionIndex = trackPositions.size() - 1;
    } else {
        for (unsigned i = 0; i < trackSizes.size(); ++i) {
            addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, i, list.get());
            list.get().append(specifiedValueForGridTrackSize(trackSizes[i], style));
        }
        insertionIndex = trackSizes.size();
    }

    // Those are the trailing <ident>* allowed in the syntax.
    addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, insertionIndex, list.get());
    return WTFMove(list);
}

static Ref<CSSValue> valueForGridPosition(const GridPosition& position)
{
    auto& cssValuePool = CSSValuePool::singleton();
    if (position.isAuto())
        return cssValuePool.createIdentifierValue(CSSValueAuto);

    if (position.isNamedGridArea())
        return cssValuePool.createValue(position.namedGridLine(), CSSPrimitiveValue::CSS_STRING);

    auto list = CSSValueList::createSpaceSeparated();
    if (position.isSpan()) {
        list.get().append(cssValuePool.createIdentifierValue(CSSValueSpan));
        list.get().append(cssValuePool.createValue(position.spanPosition(), CSSPrimitiveValue::CSS_NUMBER));
    } else
        list.get().append(cssValuePool.createValue(position.integerPosition(), CSSPrimitiveValue::CSS_NUMBER));

    if (!position.namedGridLine().isNull())
        list.get().append(cssValuePool.createValue(position.namedGridLine(), CSSPrimitiveValue::CSS_STRING));
    return WTFMove(list);
}
#endif

static Ref<CSSValue> createTransitionPropertyValue(const Animation& animation)
{
    if (animation.animationMode() == Animation::AnimateNone)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    if (animation.animationMode() == Animation::AnimateAll)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAll);
    return CSSValuePool::singleton().createValue(getPropertyNameString(animation.property()), CSSPrimitiveValue::CSS_STRING);
}

static Ref<CSSValueList> getTransitionPropertyValue(const AnimationList* animList)
{
    auto list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list.get().append(createTransitionPropertyValue(animList->animation(i)));
    } else
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueAll));
    return list;
}

#if ENABLE(CSS_SCROLL_SNAP)
static Ref<CSSValueList> scrollSnapDestination(RenderStyle& style, const LengthSize& destination)
{
    auto list = CSSValueList::createSpaceSeparated();
    list.get().append(zoomAdjustedPixelValueForLength(destination.width(), style));
    list.get().append(zoomAdjustedPixelValueForLength(destination.height(), style));
    return list;
}

static Ref<CSSValue> scrollSnapPoints(RenderStyle& style, const ScrollSnapPoints* points)
{
    if (!points)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    if (points->usesElements)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueElements);
    auto list = CSSValueList::createSpaceSeparated();
    for (auto& point : points->offsets)
        list.get().append(zoomAdjustedPixelValueForLength(point, style));
    if (points->hasRepeat)
        list.get().append(CSSValuePool::singleton().createValue(LengthRepeat::create(zoomAdjustedPixelValueForLength(points->repeatOffset, style))));
    return WTFMove(list);
}

static Ref<CSSValue> scrollSnapCoordinates(RenderStyle& style, const Vector<LengthSize>& coordinates)
{
    if (coordinates.isEmpty())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    auto list = CSSValueList::createCommaSeparated();

    for (auto& coordinate : coordinates) {
        auto pair = CSSValueList::createSpaceSeparated();
        pair.get().append(zoomAdjustedPixelValueForLength(coordinate.width(), style));
        pair.get().append(zoomAdjustedPixelValueForLength(coordinate.height(), style));
        list.get().append(WTFMove(pair));
    }

    return WTFMove(list);
}
#endif

static Ref<CSSValue> getWillChangePropertyValue(const WillChangeData* willChangeData)
{
    auto& cssValuePool = CSSValuePool::singleton();
    if (!willChangeData || !willChangeData->numFeatures())
        return cssValuePool.createIdentifierValue(CSSValueAuto);

    auto list = CSSValueList::createCommaSeparated();
    for (size_t i = 0; i < willChangeData->numFeatures(); ++i) {
        WillChangeData::FeaturePropertyPair feature = willChangeData->featureAt(i);
        switch (feature.first) {
        case WillChangeData::ScrollPosition:
            list.get().append(cssValuePool.createIdentifierValue(CSSValueScrollPosition));
            break;
        case WillChangeData::Contents:
            list.get().append(cssValuePool.createIdentifierValue(CSSValueContents));
            break;
        case WillChangeData::Property:
            list.get().append(cssValuePool.createIdentifierValue(feature.second));
            break;
        case WillChangeData::Invalid:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return WTFMove(list);
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
    return WTFMove(valueList);
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

    return WTFMove(valueList);
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

    return WTFMove(valueList);
}

static Ref<CSSValueList> getDelayValue(const AnimationList* animList)
{
    auto& cssValuePool = CSSValuePool::singleton();
    auto list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list.get().append(cssValuePool.createValue(animList->animation(i).delay(), CSSPrimitiveValue::CSS_S));
    } else {
        // Note that initialAnimationDelay() is used for both transitions and animations
        list.get().append(cssValuePool.createValue(Animation::initialDelay(), CSSPrimitiveValue::CSS_S));
    }
    return list;
}

static Ref<CSSValueList> getDurationValue(const AnimationList* animList)
{
    auto& cssValuePool = CSSValuePool::singleton();
    auto list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list.get().append(cssValuePool.createValue(animList->animation(i).duration(), CSSPrimitiveValue::CSS_S));
    } else {
        // Note that initialAnimationDuration() is used for both transitions and animations
        list.get().append(cssValuePool.createValue(Animation::initialDuration(), CSSPrimitiveValue::CSS_S));
    }
    return list;
}

static Ref<CSSValue> createTimingFunctionValue(const TimingFunction* timingFunction)
{
    switch (timingFunction->type()) {
    case TimingFunction::CubicBezierFunction: {
        const CubicBezierTimingFunction* bezierTimingFunction = static_cast<const CubicBezierTimingFunction*>(timingFunction);
        if (bezierTimingFunction->timingFunctionPreset() != CubicBezierTimingFunction::Custom) {
            CSSValueID valueId = CSSValueInvalid;
            switch (bezierTimingFunction->timingFunctionPreset()) {
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
                ASSERT(bezierTimingFunction->timingFunctionPreset() == CubicBezierTimingFunction::EaseInOut);
                valueId = CSSValueEaseInOut;
                break;
            }
            return CSSValuePool::singleton().createIdentifierValue(valueId);
        }
        return CSSCubicBezierTimingFunctionValue::create(bezierTimingFunction->x1(), bezierTimingFunction->y1(), bezierTimingFunction->x2(), bezierTimingFunction->y2());
    }
    case TimingFunction::StepsFunction: {
        const StepsTimingFunction* stepsTimingFunction = static_cast<const StepsTimingFunction*>(timingFunction);
        return CSSStepsTimingFunctionValue::create(stepsTimingFunction->numberOfSteps(), stepsTimingFunction->stepAtStart());
    }
    default:
        ASSERT(timingFunction->type() == TimingFunction::LinearFunction);
        return CSSValuePool::singleton().createIdentifierValue(CSSValueLinear);
    }
}

static Ref<CSSValueList> getTimingFunctionValue(const AnimationList* animList)
{
    auto list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list.get().append(createTimingFunctionValue(animList->animation(i).timingFunction().get()));
    } else
        // Note that initialAnimationTimingFunction() is used for both transitions and animations
        list.get().append(createTimingFunctionValue(Animation::initialTimingFunction().get()));
    return list;
}

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
static Ref<CSSValue> createAnimationTriggerValue(const AnimationTrigger* trigger, const RenderStyle& style)
{
    switch (trigger->type()) {
    case AnimationTrigger::AnimationTriggerType::ScrollAnimationTriggerType: {
        auto& scrollAnimationTrigger = downcast<ScrollAnimationTrigger>(*trigger);
        if (scrollAnimationTrigger.endValue().isAuto())
            return CSSAnimationTriggerScrollValue::create(zoomAdjustedPixelValueForLength(scrollAnimationTrigger.startValue(), style));
        return CSSAnimationTriggerScrollValue::create(zoomAdjustedPixelValueForLength(scrollAnimationTrigger.startValue(), style),
                                                      zoomAdjustedPixelValueForLength(scrollAnimationTrigger.endValue(), style));
    }
    default:
        ASSERT(trigger->type() == AnimationTrigger::AnimationTriggerType::AutoAnimationTriggerType);
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    }
}

static Ref<CSSValueList> getAnimationTriggerValue(const AnimationList* animList, const RenderStyle& style)
{
    auto list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list.get().append(createAnimationTriggerValue(animList->animation(i).trigger().get(), style));
    } else
        list.get().append(createAnimationTriggerValue(Animation::initialTrigger().get(), style));

    return list;
}
#endif

static Ref<CSSValue> createLineBoxContainValue(unsigned lineBoxContain)
{
    if (!lineBoxContain)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    return CSSLineBoxContainValue::create(lineBoxContain);
}

ComputedStyleExtractor::ComputedStyleExtractor(PassRefPtr<Node> node, bool allowVisitedStyle, PseudoId pseudoElementSpecifier)
    : m_node(node)
    , m_pseudoElementSpecifier(pseudoElementSpecifier)
    , m_allowVisitedStyle(allowVisitedStyle)
{
}


CSSComputedStyleDeclaration::CSSComputedStyleDeclaration(PassRefPtr<Node> n, bool allowVisitedStyle, const String& pseudoElementName)
    : m_node(n)
    , m_allowVisitedStyle(allowVisitedStyle)
    , m_refCount(1)
{
    unsigned nameWithoutColonsStart = pseudoElementName[0] == ':' ? (pseudoElementName[1] == ':' ? 2 : 1) : 0;
    m_pseudoElementSpecifier = CSSSelector::pseudoId(CSSSelector::parsePseudoElementType(
    (pseudoElementName.substringSharingImpl(nameWithoutColonsStart))));
}

CSSComputedStyleDeclaration::~CSSComputedStyleDeclaration()
{
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

    for (unsigned i = 0; i < numComputedProperties; i++) {
        if (i)
            result.append(' ');
        result.append(getPropertyName(computedProperties[i]));
        result.appendLiteral(": ");
        result.append(getPropertyValue(computedProperties[i]));
        result.append(';');
    }

    return result.toString();
}

void CSSComputedStyleDeclaration::setCssText(const String&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

RefPtr<CSSPrimitiveValue> ComputedStyleExtractor::getFontSizeCSSValuePreferringKeyword() const
{
    if (!m_node)
        return nullptr;

    m_node->document().updateLayoutIgnorePendingStylesheets();

    RefPtr<RenderStyle> style = m_node->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return nullptr;

    if (CSSValueID sizeIdentifier = style->fontDescription().keywordSizeAsIdentifier())
        return CSSValuePool::singleton().createIdentifierValue(sizeIdentifier);

    return zoomAdjustedPixelValue(style->fontDescription().computedPixelSize(), *style);
}

bool ComputedStyleExtractor::useFixedFontDefaultSize() const
{
    if (!m_node)
        return false;

    RefPtr<RenderStyle> style = m_node->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return false;

    return style->fontDescription().useFixedDefaultSize();
}


static CSSValueID identifierForFamily(const AtomicString& family)
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
    return CSSValueInvalid;
}

static Ref<CSSPrimitiveValue> valueForFamily(const AtomicString& family)
{
    if (CSSValueID familyIdentifier = identifierForFamily(family))
        return CSSValuePool::singleton().createIdentifierValue(familyIdentifier);
    return CSSValuePool::singleton().createFontFamilyValue(family);
}

static Ref<CSSValue> renderTextDecorationFlagsToCSSValue(int textDecoration)
{
    auto& cssValuePool = CSSValuePool::singleton();
    // Blink value is ignored.
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (textDecoration & TextDecorationUnderline)
        list->append(cssValuePool.createIdentifierValue(CSSValueUnderline));
    if (textDecoration & TextDecorationOverline)
        list->append(cssValuePool.createIdentifierValue(CSSValueOverline));
    if (textDecoration & TextDecorationLineThrough)
        list->append(cssValuePool.createIdentifierValue(CSSValueLineThrough));
#if ENABLE(LETTERPRESS)
    if (textDecoration & TextDecorationLetterpress)
        list->append(cssValuePool.createIdentifierValue(CSSValueWebkitLetterpress));
#endif

    if (!list->length())
        return cssValuePool.createIdentifierValue(CSSValueNone);
    return list.releaseNonNull();
}

static Ref<CSSValue> renderTextDecorationStyleFlagsToCSSValue(TextDecorationStyle textDecorationStyle)
{
    switch (textDecorationStyle) {
    case TextDecorationStyleSolid:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueSolid);
    case TextDecorationStyleDouble:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueDouble);
    case TextDecorationStyleDotted:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueDotted);
    case TextDecorationStyleDashed:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueDashed);
    case TextDecorationStyleWavy:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueWavy);
    }

    ASSERT_NOT_REACHED();
    return CSSValuePool::singleton().createExplicitInitialValue();
}

static Ref<CSSValue> renderTextDecorationSkipFlagsToCSSValue(TextDecorationSkip textDecorationSkip)
{
    switch (textDecorationSkip) {
    case TextDecorationSkipAuto:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    case TextDecorationSkipNone:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    case TextDecorationSkipInk:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueInk);
    case TextDecorationSkipObjects:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueObjects);
    }

    ASSERT_NOT_REACHED();
    return CSSValuePool::singleton().createExplicitInitialValue();
}

static Ref<CSSValue> renderEmphasisPositionFlagsToCSSValue(TextEmphasisPosition textEmphasisPosition)
{
    ASSERT(!((textEmphasisPosition & TextEmphasisPositionOver) && (textEmphasisPosition & TextEmphasisPositionUnder)));
    ASSERT(!((textEmphasisPosition & TextEmphasisPositionLeft) && (textEmphasisPosition & TextEmphasisPositionRight)));
    auto& cssValuePool = CSSValuePool::singleton();
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (textEmphasisPosition & TextEmphasisPositionOver)
        list->append(cssValuePool.createIdentifierValue(CSSValueOver));
    if (textEmphasisPosition & TextEmphasisPositionUnder)
        list->append(cssValuePool.createIdentifierValue(CSSValueUnder));
    if (textEmphasisPosition & TextEmphasisPositionLeft)
        list->append(cssValuePool.createIdentifierValue(CSSValueLeft));
    if (textEmphasisPosition & TextEmphasisPositionRight)
        list->append(cssValuePool.createIdentifierValue(CSSValueRight));

    if (!list->length())
        return cssValuePool.createIdentifierValue(CSSValueNone);
    return list.releaseNonNull();
}

static Ref<CSSValue> hangingPunctuationToCSSValue(HangingPunctuation hangingPunctuation)
{
    auto& cssValuePool = CSSValuePool::singleton();
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (hangingPunctuation & FirstHangingPunctuation)
        list->append(cssValuePool.createIdentifierValue(CSSValueFirst));
    if (hangingPunctuation & AllowEndHangingPunctuation)
        list->append(cssValuePool.createIdentifierValue(CSSValueAllowEnd));
    if (hangingPunctuation & ForceEndHangingPunctuation)
        list->append(cssValuePool.createIdentifierValue(CSSValueForceEnd));
    if (hangingPunctuation & LastHangingPunctuation)
        list->append(cssValuePool.createIdentifierValue(CSSValueLast));
    if (!list->length())
        return cssValuePool.createIdentifierValue(CSSValueNone);
    return list.releaseNonNull();
}
    
static Ref<CSSValue> fillRepeatToCSSValue(EFillRepeat xRepeat, EFillRepeat yRepeat)
{
    // For backwards compatibility, if both values are equal, just return one of them. And
    // if the two values are equivalent to repeat-x or repeat-y, just return the shorthand.
    auto& cssValuePool = CSSValuePool::singleton();
    if (xRepeat == yRepeat)
        return cssValuePool.createValue(xRepeat);
    if (xRepeat == RepeatFill && yRepeat == NoRepeatFill)
        return cssValuePool.createIdentifierValue(CSSValueRepeatX);
    if (xRepeat == NoRepeatFill && yRepeat == RepeatFill)
        return cssValuePool.createIdentifierValue(CSSValueRepeatY);

    auto list = CSSValueList::createSpaceSeparated();
    list.get().append(cssValuePool.createValue(xRepeat));
    list.get().append(cssValuePool.createValue(yRepeat));
    return WTFMove(list);
}

static Ref<CSSValue> fillSourceTypeToCSSValue(EMaskSourceType type)
{
    switch (type) {
    case MaskAlpha:
        return CSSValuePool::singleton().createValue(CSSValueAlpha);
    default:
        ASSERT(type == MaskLuminance);
        return CSSValuePool::singleton().createValue(CSSValueLuminance);
    }
}

static Ref<CSSValue> fillSizeToCSSValue(const FillSize& fillSize, const RenderStyle& style)
{
    if (fillSize.type == Contain)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueContain);

    if (fillSize.type == Cover)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueCover);

    if (fillSize.size.height().isAuto())
        return zoomAdjustedPixelValueForLength(fillSize.size.width(), style);

    auto list = CSSValueList::createSpaceSeparated();
    list.get().append(zoomAdjustedPixelValueForLength(fillSize.size.width(), style));
    list.get().append(zoomAdjustedPixelValueForLength(fillSize.size.height(), style));
    return WTFMove(list);
}

static Ref<CSSValue> altTextToCSSValue(const RenderStyle* style)
{
    return CSSValuePool::singleton().createValue(style->contentAltText(), CSSPrimitiveValue::CSS_STRING);
}
    
static Ref<CSSValueList> contentToCSSValue(const RenderStyle* style)
{
    auto& cssValuePool = CSSValuePool::singleton();
    auto list = CSSValueList::createSpaceSeparated();
    for (const ContentData* contentData = style->contentData(); contentData; contentData = contentData->next()) {
        if (is<CounterContentData>(*contentData))
            list.get().append(cssValuePool.createValue(downcast<CounterContentData>(*contentData).counter().identifier(), CSSPrimitiveValue::CSS_COUNTER_NAME));
        else if (is<ImageContentData>(*contentData))
            list.get().append(*downcast<ImageContentData>(*contentData).image().cssValue());
        else if (is<TextContentData>(*contentData))
            list.get().append(cssValuePool.createValue(downcast<TextContentData>(*contentData).text(), CSSPrimitiveValue::CSS_STRING));
    }
    if (style->hasFlowFrom())
        list.get().append(cssValuePool.createValue(style->regionThread(), CSSPrimitiveValue::CSS_STRING));
    return list;
}

static RefPtr<CSSValue> counterToCSSValue(const RenderStyle* style, CSSPropertyID propertyID)
{
    const CounterDirectiveMap* map = style->counterDirectives();
    if (!map)
        return nullptr;

    auto& cssValuePool = CSSValuePool::singleton();
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (CounterDirectiveMap::const_iterator it = map->begin(); it != map->end(); ++it) {
        list->append(cssValuePool.createValue(it->key, CSSPrimitiveValue::CSS_STRING));
        short number = propertyID == CSSPropertyCounterIncrement ? it->value.incrementValue() : it->value.resetValue();
        list->append(cssValuePool.createValue((double)number, CSSPrimitiveValue::CSS_NUMBER));
    }
    return list.release();
}

static void logUnimplementedPropertyID(CSSPropertyID propertyID)
{
    static NeverDestroyed<HashSet<CSSPropertyID>> propertyIDSet;
    if (!propertyIDSet.get().add(propertyID).isNewEntry)
        return;

    LOG_ERROR("WebKit does not yet implement getComputedStyle for '%s'.", getPropertyName(propertyID));
}

static Ref<CSSValueList> fontFamilyFromStyle(RenderStyle* style)
{
    auto list = CSSValueList::createCommaSeparated();
    for (unsigned i = 0; i < style->fontCascade().familyCount(); ++i)
        list.get().append(valueForFamily(style->fontCascade().familyAt(i)));
    return list;
}

static Ref<CSSPrimitiveValue> lineHeightFromStyle(RenderStyle& style)
{
    Length length = style.lineHeight();
    if (length.isNegative()) // If true, line-height not set; use the font's line spacing.
        return zoomAdjustedPixelValue(style.fontMetrics().floatLineSpacing(), style);
    if (length.isPercent()) {
        // This is imperfect, because it doesn't include the zoom factor and the real computation
        // for how high to be in pixels does include things like minimum font size and the zoom factor.
        // On the other hand, since font-size doesn't include the zoom factor, we really can't do
        // that here either.
        return zoomAdjustedPixelValue(static_cast<int>(length.percent() * style.fontDescription().specifiedSize()) / 100, style);
    }
    return zoomAdjustedPixelValue(floatValueForLength(length, 0), style);
}

static Ref<CSSPrimitiveValue> fontSizeFromStyle(RenderStyle& style)
{
    return zoomAdjustedPixelValue(style.fontDescription().computedPixelSize(), style);
}

static Ref<CSSPrimitiveValue> fontStyleFromStyle(RenderStyle* style)
{
    if (style->fontDescription().italic())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueItalic);
    return CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);
}

static Ref<CSSValue> fontVariantFromStyle(RenderStyle* style)
{
    if (style->fontDescription().variantSettings().isAllNormal())
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);

    auto list = CSSValueList::createSpaceSeparated();

    switch (style->fontDescription().variantCommonLigatures()) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueCommonLigatures));
        break;
    case FontVariantLigatures::No:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoCommonLigatures));
        break;
    }

    switch (style->fontDescription().variantDiscretionaryLigatures()) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueDiscretionaryLigatures));
        break;
    case FontVariantLigatures::No:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoDiscretionaryLigatures));
        break;
    }

    switch (style->fontDescription().variantHistoricalLigatures()) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueHistoricalLigatures));
        break;
    case FontVariantLigatures::No:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoHistoricalLigatures));
        break;
    }

    switch (style->fontDescription().variantContextualAlternates()) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueContextual));
        break;
    case FontVariantLigatures::No:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoContextual));
        break;
    }

    switch (style->fontDescription().variantPosition()) {
    case FontVariantPosition::Normal:
        break;
    case FontVariantPosition::Subscript:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueSub));
        break;
    case FontVariantPosition::Superscript:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueSuper));
        break;
    }

    switch (style->fontDescription().variantCaps()) {
    case FontVariantCaps::Normal:
        break;
    case FontVariantCaps::Small:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueSmallCaps));
        break;
    case FontVariantCaps::AllSmall:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueAllSmallCaps));
        break;
    case FontVariantCaps::Petite:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValuePetiteCaps));
        break;
    case FontVariantCaps::AllPetite:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueAllPetiteCaps));
        break;
    case FontVariantCaps::Unicase:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueUnicase));
        break;
    case FontVariantCaps::Titling:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueTitlingCaps));
        break;
    }

    switch (style->fontDescription().variantNumericFigure()) {
    case FontVariantNumericFigure::Normal:
        break;
    case FontVariantNumericFigure::LiningNumbers:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueLiningNums));
        break;
    case FontVariantNumericFigure::OldStyleNumbers:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueOldstyleNums));
        break;
    }

    switch (style->fontDescription().variantNumericSpacing()) {
    case FontVariantNumericSpacing::Normal:
        break;
    case FontVariantNumericSpacing::ProportionalNumbers:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueProportionalNums));
        break;
    case FontVariantNumericSpacing::TabularNumbers:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueTabularNums));
        break;
    }

    switch (style->fontDescription().variantNumericFraction()) {
    case FontVariantNumericFraction::Normal:
        break;
    case FontVariantNumericFraction::DiagonalFractions:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueDiagonalFractions));
        break;
    case FontVariantNumericFraction::StackedFractions:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueStackedFractions));
        break;
    }

    switch (style->fontDescription().variantNumericOrdinal()) {
    case FontVariantNumericOrdinal::Normal:
        break;
    case FontVariantNumericOrdinal::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueOrdinal));
        break;
    }

    switch (style->fontDescription().variantNumericSlashedZero()) {
    case FontVariantNumericSlashedZero::Normal:
        break;
    case FontVariantNumericSlashedZero::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueSlashedZero));
        break;
    }

    switch (style->fontDescription().variantAlternates()) {
    case FontVariantAlternates::Normal:
        break;
    case FontVariantAlternates::HistoricalForms:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueHistoricalForms));
        break;
    }

    switch (style->fontDescription().variantEastAsianVariant()) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis78));
        break;
    case FontVariantEastAsianVariant::Jis83:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis83));
        break;
    case FontVariantEastAsianVariant::Jis90:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis90));
        break;
    case FontVariantEastAsianVariant::Jis04:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis04));
        break;
    case FontVariantEastAsianVariant::Simplified:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueSimplified));
        break;
    case FontVariantEastAsianVariant::Traditional:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueTraditional));
        break;
    }

    switch (style->fontDescription().variantEastAsianWidth()) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueFullWidth));
        break;
    case FontVariantEastAsianWidth::Proportional:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueProportionalWidth));
        break;
    }

    switch (style->fontDescription().variantEastAsianRuby()) {
    case FontVariantEastAsianRuby::Normal:
        break;
    case FontVariantEastAsianRuby::Yes:
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueRuby));
        break;
    }

    return WTFMove(list);
}

static Ref<CSSPrimitiveValue> fontWeightFromStyle(RenderStyle* style)
{
    switch (style->fontDescription().weight()) {
    case FontWeight100:
        return CSSValuePool::singleton().createIdentifierValue(CSSValue100);
    case FontWeight200:
        return CSSValuePool::singleton().createIdentifierValue(CSSValue200);
    case FontWeight300:
        return CSSValuePool::singleton().createIdentifierValue(CSSValue300);
    case FontWeightNormal:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);
    case FontWeight500:
        return CSSValuePool::singleton().createIdentifierValue(CSSValue500);
    case FontWeight600:
        return CSSValuePool::singleton().createIdentifierValue(CSSValue600);
    case FontWeightBold:
        return CSSValuePool::singleton().createIdentifierValue(CSSValueBold);
    case FontWeight800:
        return CSSValuePool::singleton().createIdentifierValue(CSSValue800);
    case FontWeight900:
        return CSSValuePool::singleton().createIdentifierValue(CSSValue900);
    }
    ASSERT_NOT_REACHED();
    return CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);
}

static Ref<CSSValue> fontSynthesisFromStyle(RenderStyle& style)
{
    if (style.fontDescription().fontSynthesis() == FontSynthesisNone)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();
    if (style.fontDescription().fontSynthesis() & FontSynthesisStyle)
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueStyle));
    if (style.fontDescription().fontSynthesis() & FontSynthesisWeight)
        list.get().append(CSSValuePool::singleton().createIdentifierValue(CSSValueWeight));
    return Ref<CSSValue>(list.get());
}

typedef const Length& (RenderStyle::*RenderStyleLengthGetter)() const;
typedef LayoutUnit (RenderBoxModelObject::*RenderBoxComputedCSSValueGetter)() const;

template<RenderStyleLengthGetter lengthGetter, RenderBoxComputedCSSValueGetter computedCSSValueGetter>
inline RefPtr<CSSValue> zoomAdjustedPaddingOrMarginPixelValue(RenderStyle& style, RenderObject* renderer)
{
    Length unzoomzedLength = (style.*lengthGetter)();
    if (!is<RenderBox>(renderer) || unzoomzedLength.isFixed())
        return zoomAdjustedPixelValueForLength(unzoomzedLength, style);
    return zoomAdjustedPixelValue((downcast<RenderBox>(*renderer).*computedCSSValueGetter)(), style);
}

template<RenderStyleLengthGetter lengthGetter>
inline bool paddingOrMarginIsRendererDependent(RenderStyle* style, RenderObject* renderer)
{
    if (!renderer || !renderer->isBox())
        return false;
    return !(style && (style->*lengthGetter)().isFixed());
}

static CSSValueID convertToPageBreak(BreakBetween value)
{
    if (value == PageBreakBetween || value == LeftPageBreakBetween || value == RightPageBreakBetween
        || value == RectoPageBreakBetween || value == VersoPageBreakBetween)
        return CSSValueAlways; // CSS 2.1 allows us to map these to always.
    if (value == AvoidBreakBetween || value == AvoidPageBreakBetween)
        return CSSValueAvoid;
    return CSSValueAuto;
}

static CSSValueID convertToColumnBreak(BreakBetween value)
{
    if (value == ColumnBreakBetween)
        return CSSValueAlways;
    if (value == AvoidBreakBetween || value == AvoidColumnBreakBetween)
        return CSSValueAvoid;
    return CSSValueAuto;
}

static CSSValueID convertToPageBreak(BreakInside value)
{
    if (value == AvoidBreakInside || value == AvoidPageBreakInside)
        return CSSValueAvoid;
    return CSSValueAuto;
}

static CSSValueID convertToColumnBreak(BreakInside value)
{
    if (value == AvoidBreakInside || value == AvoidColumnBreakInside)
        return CSSValueAvoid;
    return CSSValueAuto;
}

#if ENABLE(CSS_REGIONS)
static CSSValueID convertToRegionBreak(BreakBetween value)
{
    if (value == RegionBreakBetween)
        return CSSValueAlways;
    if (value == AvoidBreakBetween || value == AvoidRegionBreakBetween)
        return CSSValueAvoid;
    return CSSValueAuto;
}
    
static CSSValueID convertToRegionBreak(BreakInside value)
{
    if (value == AvoidBreakInside || value == AvoidRegionBreakInside)
        return CSSValueAvoid;
    return CSSValueAuto;
}
#endif
    
static bool isLayoutDependent(CSSPropertyID propertyID, RenderStyle* style, RenderObject* renderer)
{
    switch (propertyID) {
    case CSSPropertyWidth:
    case CSSPropertyHeight:
    case CSSPropertyPerspectiveOrigin:
    case CSSPropertyTransformOrigin:
    case CSSPropertyTransform:
    case CSSPropertyFilter:
#if ENABLE(FILTERS_LEVEL_2)
    case CSSPropertyWebkitBackdropFilter:
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
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridTemplateColumns:
    case CSSPropertyWebkitGridTemplateRows:
    case CSSPropertyWebkitGridTemplate:
    case CSSPropertyWebkitGrid:
        return renderer && renderer->isRenderGrid();
#endif
    default:
        return false;
    }
}

Node* ComputedStyleExtractor::styledNode() const
{
    if (!m_node)
        return nullptr;
    if (!is<Element>(*m_node))
        return m_node.get();
    Element& element = downcast<Element>(*m_node);
    PseudoElement* pseudoElement;
    if (m_pseudoElementSpecifier == BEFORE && (pseudoElement = element.beforePseudoElement()))
        return pseudoElement;
    if (m_pseudoElementSpecifier == AFTER && (pseudoElement = element.afterPseudoElement()))
        return pseudoElement;
    return &element;
}

static ItemPosition resolveContainerAlignmentAuto(ItemPosition position, RenderObject* element)
{
    if (position != ItemPositionAuto || !element)
        return position;

    return element->style().isDisplayFlexibleOrGridBox() ? ItemPositionStretch : ItemPositionStart;
}

static ItemPosition resolveSelfAlignmentAuto(ItemPosition position, OverflowAlignment& overflow, RenderObject* element)
{
    if (position != ItemPositionAuto || !element || element->isOutOfFlowPositioned())
        return position;

    RenderBlock* parent = element->containingBlock();
    if (!parent)
        return ItemPositionStart;

    overflow = parent->style().alignItemsOverflowAlignment();
    return resolveContainerAlignmentAuto(parent->style().alignItemsPosition(), parent);
}

static void resolveContentAlignmentAuto(ContentPosition& position, ContentDistributionType& distribution, RenderObject* element)
{
    if (position != ContentPositionAuto || distribution != ContentDistributionDefault || !element)
        return;

    // Even that both align-content and justify-content 'auto' values are resolved to 'stretch'
    // in case of flexbox containers, 'stretch' value in justify-content will behave like 'flex-start'. 
    if (element->style().isDisplayFlexibleBox())
        distribution = ContentDistributionStretch;
    else
        position = ContentPositionStart;
}

RefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(CSSPropertyID propertyID, EUpdateLayout updateLayout) const
{
    return ComputedStyleExtractor(m_node, m_allowVisitedStyle, m_pseudoElementSpecifier).propertyValue(propertyID, updateLayout);
}

Ref<MutableStyleProperties> CSSComputedStyleDeclaration::copyProperties() const
{
    return ComputedStyleExtractor(m_node, m_allowVisitedStyle, m_pseudoElementSpecifier).copyProperties();
}

static inline bool nodeOrItsAncestorNeedsStyleRecalc(const Node& node)
{
    if (node.needsStyleRecalc())
        return true;

    const Node* currentNode = &node;
    const Element* ancestor = currentNode->parentOrShadowHostElement();
    while (ancestor) {
        if (ancestor->needsStyleRecalc())
            return true;

        if (ancestor->directChildNeedsStyleRecalc() && currentNode->styleIsAffectedByPreviousSibling())
            return true;

        currentNode = ancestor;
        ancestor = currentNode->parentOrShadowHostElement();
    }
    return false;
}

static inline bool updateStyleIfNeededForNode(const Node& node)
{
    Document& document = node.document();
    if (!document.hasPendingForcedStyleRecalc() && !(document.childNeedsStyleRecalc() && nodeOrItsAncestorNeedsStyleRecalc(node)))
        return false;
    document.updateStyleIfNeeded();
    return true;
}

static inline RefPtr<RenderStyle> computeRenderStyleForProperty(Node* styledNode, PseudoId pseudoElementSpecifier, CSSPropertyID propertyID)
{
    RenderObject* renderer = styledNode->renderer();

    if (renderer && renderer->isComposited() && AnimationController::supportsAcceleratedAnimationOfProperty(propertyID)) {
        RefPtr<RenderStyle> style = renderer->animation().getAnimatedStyleForRenderer(downcast<RenderElement>(*renderer));
        if (pseudoElementSpecifier && !styledNode->isPseudoElement()) {
            // FIXME: This cached pseudo style will only exist if the animation has been run at least once.
            return style->getCachedPseudoStyle(pseudoElementSpecifier);
        }
        return style;
    }

    return styledNode->computedStyle(styledNode->isPseudoElement() ? NOPSEUDO : pseudoElementSpecifier);
}

#if ENABLE(CSS_SHAPES)
static Ref<CSSValue> shapePropertyValue(const RenderStyle& style, const ShapeValue* shapeValue)
{
    if (!shapeValue)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);

    if (shapeValue->type() == ShapeValue::Type::Box)
        return CSSValuePool::singleton().createValue(shapeValue->cssBox());

    if (shapeValue->type() == ShapeValue::Type::Image) {
        if (shapeValue->image())
            return *shapeValue->image()->cssValue();
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    }

    ASSERT(shapeValue->type() == ShapeValue::Type::Shape);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(valueForBasicShape(style, *shapeValue->shape()));
    if (shapeValue->cssBox() != BoxMissing)
        list->append(CSSValuePool::singleton().createValue(shapeValue->cssBox()));
    return list.releaseNonNull();
}
#endif

static RefPtr<CSSValueList> valueForItemPositionWithOverflowAlignment(ItemPosition itemPosition, OverflowAlignment overflowAlignment, ItemPositionType positionType)
{
    auto& cssValuePool = CSSValuePool::singleton();
    RefPtr<CSSValueList> result = CSSValueList::createSpaceSeparated();
    if (positionType == LegacyPosition)
        result->append(CSSPrimitiveValue::createIdentifier(CSSValueLegacy));
    result->append(cssValuePool.createValue(itemPosition));
    if (overflowAlignment != OverflowAlignmentDefault)
        result->append(cssValuePool.createValue(overflowAlignment));
    ASSERT(result->length() <= 2);
    return result;
}

static RefPtr<CSSValueList> valueForContentPositionAndDistributionWithOverflowAlignment(ContentPosition position, ContentDistributionType distribution, OverflowAlignment overflowAlignment)
{
    RefPtr<CSSValueList> result = CSSValueList::createSpaceSeparated();
    if (distribution != ContentDistributionDefault)
        result->append(CSSPrimitiveValue::create(distribution));
    if (distribution == ContentDistributionDefault || position != ContentPositionAuto)
        result->append(CSSPrimitiveValue::create(position));
    if ((position >= ContentPositionCenter || distribution != ContentDistributionDefault) && overflowAlignment != OverflowAlignmentDefault)
        result->append(CSSPrimitiveValue::create(overflowAlignment));
    ASSERT(result->length() > 0);
    ASSERT(result->length() <= 3);
    return result;
}

inline static bool isFlexOrGrid(ContainerNode* element)
{
    return element && element->computedStyle() && element->computedStyle()->isDisplayFlexibleOrGridBox();
}

RefPtr<CSSValue> ComputedStyleExtractor::customPropertyValue(const String& propertyName) const
{
    Node* styledNode = this->styledNode();
    if (!styledNode)
        return nullptr;
    
    if (updateStyleIfNeededForNode(*styledNode)) {
        // The style recalc could have caused the styled node to be discarded or replaced
        // if it was a PseudoElement so we need to update it.
        styledNode = this->styledNode();
    }

    RefPtr<RenderStyle> style = computeRenderStyleForProperty(styledNode, m_pseudoElementSpecifier, CSSPropertyCustom);
    if (!style || !style->hasCustomProperty(propertyName))
        return nullptr;

    return style->getCustomPropertyValue(propertyName);
}

String ComputedStyleExtractor::customPropertyText(const String& propertyName) const
{
    RefPtr<CSSValue> propertyValue = this->customPropertyValue(propertyName);
    return propertyValue ? propertyValue->cssText() : emptyString();
}

RefPtr<CSSValue> ComputedStyleExtractor::propertyValue(CSSPropertyID propertyID, EUpdateLayout updateLayout) const
{
    Node* styledNode = this->styledNode();
    if (!styledNode)
        return nullptr;

    RefPtr<RenderStyle> style;
    RenderObject* renderer = nullptr;
    bool forceFullLayout = false;
    if (updateLayout) {
        Document& document = styledNode->document();

        if (updateStyleIfNeededForNode(*styledNode)) {
            // The style recalc could have caused the styled node to be discarded or replaced
            // if it was a PseudoElement so we need to update it.
            styledNode = this->styledNode();
        }

        renderer = styledNode->renderer();

        if (propertyID == CSSPropertyDisplay && !renderer && is<SVGElement>(*styledNode) && !downcast<SVGElement>(*styledNode).isValid())
            return nullptr;

        style = computeRenderStyleForProperty(styledNode, m_pseudoElementSpecifier, propertyID);

        // FIXME: Some of these cases could be narrowed down or optimized better.
        forceFullLayout = isLayoutDependent(propertyID, style.get(), renderer)
            || styledNode->isInShadowTree()
            || (document.styleResolverIfExists() && document.styleResolverIfExists()->hasViewportDependentMediaQueries() && document.ownerElement());

        if (forceFullLayout) {
            document.updateLayoutIgnorePendingStylesheets();
            styledNode = this->styledNode();
        }
    }

    if (!updateLayout || forceFullLayout) {
        style = computeRenderStyleForProperty(styledNode, m_pseudoElementSpecifier, propertyID);
        renderer = styledNode->renderer();
    }

    if (!style)
        return nullptr;

    auto& cssValuePool = CSSValuePool::singleton();
    propertyID = CSSProperty::resolveDirectionAwareProperty(propertyID, style->direction(), style->writingMode());

    switch (propertyID) {
        case CSSPropertyInvalid:
            break;

        case CSSPropertyBackgroundColor:
            return cssValuePool.createColorValue(m_allowVisitedStyle? style->visitedDependentColor(CSSPropertyBackgroundColor).rgb() : style->backgroundColor().rgb());
        case CSSPropertyBackgroundImage:
        case CSSPropertyWebkitMaskImage: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskImage ? style->maskLayers() : style->backgroundLayers();
            if (!layers)
                return cssValuePool.createIdentifierValue(CSSValueNone);

            if (!layers->next()) {
                if (layers->image())
                    return layers->image()->cssValue();

                return cssValuePool.createIdentifierValue(CSSValueNone);
            }

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next()) {
                if (currLayer->image())
                    list->append(*currLayer->image()->cssValue());
                else
                    list->append(cssValuePool.createIdentifierValue(CSSValueNone));
            }
            return list;
        }
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize:
        case CSSPropertyWebkitMaskSize: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskSize ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return fillSizeToCSSValue(layers->size(), *style);

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(fillSizeToCSSValue(currLayer->size(), *style));

            return list;
        }
        case CSSPropertyBackgroundRepeat:
        case CSSPropertyWebkitMaskRepeat: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskRepeat ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return fillRepeatToCSSValue(layers->repeatX(), layers->repeatY());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(fillRepeatToCSSValue(currLayer->repeatX(), currLayer->repeatY()));

            return list;
        }
        case CSSPropertyWebkitMaskSourceType: {
            const FillLayer* layers = style->maskLayers();

            if (!layers)
                return cssValuePool.createIdentifierValue(CSSValueNone);

            if (!layers->next())
                return fillSourceTypeToCSSValue(layers->maskSourceType());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(fillSourceTypeToCSSValue(currLayer->maskSourceType()));

            return list;
        }
        case CSSPropertyWebkitBackgroundComposite:
        case CSSPropertyWebkitMaskComposite: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskComposite ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool.createValue(layers->composite());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool.createValue(currLayer->composite()));

            return list;
        }
        case CSSPropertyBackgroundAttachment: {
            const FillLayer* layers = style->backgroundLayers();
            if (!layers->next())
                return cssValuePool.createValue(layers->attachment());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool.createValue(currLayer->attachment()));

            return list;
        }
        case CSSPropertyBackgroundClip:
        case CSSPropertyBackgroundOrigin:
        case CSSPropertyWebkitBackgroundClip:
        case CSSPropertyWebkitBackgroundOrigin:
        case CSSPropertyWebkitMaskClip:
        case CSSPropertyWebkitMaskOrigin: {
            const FillLayer* layers = (propertyID == CSSPropertyWebkitMaskClip || propertyID == CSSPropertyWebkitMaskOrigin) ? style->maskLayers() : style->backgroundLayers();
            bool isClip = propertyID == CSSPropertyBackgroundClip || propertyID == CSSPropertyWebkitBackgroundClip || propertyID == CSSPropertyWebkitMaskClip;
            if (!layers->next()) {
                EFillBox box = isClip ? layers->clip() : layers->origin();
                return cssValuePool.createValue(box);
            }

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next()) {
                EFillBox box = isClip ? currLayer->clip() : currLayer->origin();
                list->append(cssValuePool.createValue(box));
            }

            return list;
        }
        case CSSPropertyBackgroundPosition:
        case CSSPropertyWebkitMaskPosition: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskPosition ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return createPositionListForLayer(propertyID, layers, *style);

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(createPositionListForLayer(propertyID, currLayer, *style));
            return list;
        }
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskPositionX ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool.createValue(layers->xPosition());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool.createValue(currLayer->xPosition()));

            return list;
        }
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskPositionY ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool.createValue(layers->yPosition());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool.createValue(currLayer->yPosition()));

            return list;
        }
        case CSSPropertyBorderCollapse:
            if (style->borderCollapse())
                return cssValuePool.createIdentifierValue(CSSValueCollapse);
            return cssValuePool.createIdentifierValue(CSSValueSeparate);
        case CSSPropertyBorderSpacing: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            list->append(zoomAdjustedPixelValue(style->horizontalBorderSpacing(), *style));
            list->append(zoomAdjustedPixelValue(style->verticalBorderSpacing(), *style));
            return list;
        }
        case CSSPropertyWebkitBorderHorizontalSpacing:
            return zoomAdjustedPixelValue(style->horizontalBorderSpacing(), *style);
        case CSSPropertyWebkitBorderVerticalSpacing:
            return zoomAdjustedPixelValue(style->verticalBorderSpacing(), *style);
        case CSSPropertyBorderImageSource:
            if (style->borderImageSource())
                return style->borderImageSource()->cssValue();
            return cssValuePool.createIdentifierValue(CSSValueNone);
        case CSSPropertyBorderTopColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style->visitedDependentColor(CSSPropertyBorderTopColor).rgb()) : currentColorOrValidColor(style.get(), style->borderTopColor());
        case CSSPropertyBorderRightColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style->visitedDependentColor(CSSPropertyBorderRightColor).rgb()) : currentColorOrValidColor(style.get(), style->borderRightColor());
        case CSSPropertyBorderBottomColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style->visitedDependentColor(CSSPropertyBorderBottomColor).rgb()) : currentColorOrValidColor(style.get(), style->borderBottomColor());
        case CSSPropertyBorderLeftColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style->visitedDependentColor(CSSPropertyBorderLeftColor).rgb()) : currentColorOrValidColor(style.get(), style->borderLeftColor());
        case CSSPropertyBorderTopStyle:
            return cssValuePool.createValue(style->borderTopStyle());
        case CSSPropertyBorderRightStyle:
            return cssValuePool.createValue(style->borderRightStyle());
        case CSSPropertyBorderBottomStyle:
            return cssValuePool.createValue(style->borderBottomStyle());
        case CSSPropertyBorderLeftStyle:
            return cssValuePool.createValue(style->borderLeftStyle());
        case CSSPropertyBorderTopWidth:
            return zoomAdjustedPixelValue(style->borderTopWidth(), *style);
        case CSSPropertyBorderRightWidth:
            return zoomAdjustedPixelValue(style->borderRightWidth(), *style);
        case CSSPropertyBorderBottomWidth:
            return zoomAdjustedPixelValue(style->borderBottomWidth(), *style);
        case CSSPropertyBorderLeftWidth:
            return zoomAdjustedPixelValue(style->borderLeftWidth(), *style);
        case CSSPropertyBottom:
            return positionOffsetValue(*style, CSSPropertyBottom);
        case CSSPropertyWebkitBoxAlign:
            return cssValuePool.createValue(style->boxAlign());
#if ENABLE(CSS_BOX_DECORATION_BREAK)
        case CSSPropertyWebkitBoxDecorationBreak:
            if (style->boxDecorationBreak() == DSLICE)
                return cssValuePool.createIdentifierValue(CSSValueSlice);
        return cssValuePool.createIdentifierValue(CSSValueClone);
#endif
        case CSSPropertyWebkitBoxDirection:
            return cssValuePool.createValue(style->boxDirection());
        case CSSPropertyWebkitBoxFlex:
            return cssValuePool.createValue(style->boxFlex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxFlexGroup:
            return cssValuePool.createValue(style->boxFlexGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxLines:
            return cssValuePool.createValue(style->boxLines());
        case CSSPropertyWebkitBoxOrdinalGroup:
            return cssValuePool.createValue(style->boxOrdinalGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxOrient:
            return cssValuePool.createValue(style->boxOrient());
        case CSSPropertyWebkitBoxPack:
            return cssValuePool.createValue(style->boxPack());
        case CSSPropertyWebkitBoxReflect:
            return valueForReflection(style->boxReflect(), *style);
        case CSSPropertyBoxShadow:
        case CSSPropertyWebkitBoxShadow:
            return valueForShadow(style->boxShadow(), propertyID, *style);
        case CSSPropertyCaptionSide:
            return cssValuePool.createValue(style->captionSide());
        case CSSPropertyClear:
            return cssValuePool.createValue(style->clear());
        case CSSPropertyColor:
            return cssValuePool.createColorValue(m_allowVisitedStyle ? style->visitedDependentColor(CSSPropertyColor).rgb() : style->color().rgb());
        case CSSPropertyWebkitPrintColorAdjust:
            return cssValuePool.createValue(style->printColorAdjust());
        case CSSPropertyWebkitColumnAxis:
            return cssValuePool.createValue(style->columnAxis());
        case CSSPropertyColumnCount:
            if (style->hasAutoColumnCount())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style->columnCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyColumnFill:
            return cssValuePool.createValue(style->columnFill());
        case CSSPropertyColumnGap:
            if (style->hasNormalColumnGap())
                return cssValuePool.createIdentifierValue(CSSValueNormal);
            return zoomAdjustedPixelValue(style->columnGap(), *style);
        case CSSPropertyColumnProgression:
            return cssValuePool.createValue(style->columnProgression());
        case CSSPropertyColumnRuleColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style->visitedDependentColor(CSSPropertyOutlineColor).rgb()) : currentColorOrValidColor(style.get(), style->columnRuleColor());
        case CSSPropertyColumnRuleStyle:
            return cssValuePool.createValue(style->columnRuleStyle());
        case CSSPropertyColumnRuleWidth:
            return zoomAdjustedPixelValue(style->columnRuleWidth(), *style);
        case CSSPropertyColumnSpan:
            return cssValuePool.createIdentifierValue(style->columnSpan() ? CSSValueAll : CSSValueNone);
        case CSSPropertyWebkitColumnBreakAfter:
            return cssValuePool.createValue(convertToColumnBreak(style->breakAfter()));
        case CSSPropertyWebkitColumnBreakBefore:
            return cssValuePool.createValue(convertToColumnBreak(style->breakBefore()));
        case CSSPropertyWebkitColumnBreakInside:
            return cssValuePool.createValue(convertToColumnBreak(style->breakInside()));
        case CSSPropertyColumnWidth:
            if (style->hasAutoColumnWidth())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return zoomAdjustedPixelValue(style->columnWidth(), *style);
        case CSSPropertyTabSize:
            return cssValuePool.createValue(style->tabSize(), CSSPrimitiveValue::CSS_NUMBER);
#if ENABLE(CSS_REGIONS)
        case CSSPropertyWebkitRegionBreakAfter:
            return cssValuePool.createValue(convertToRegionBreak(style->breakAfter()));
        case CSSPropertyWebkitRegionBreakBefore:
            return cssValuePool.createValue(convertToRegionBreak(style->breakBefore()));
        case CSSPropertyWebkitRegionBreakInside:
            return cssValuePool.createValue(convertToRegionBreak(style->breakInside()));
#endif
        case CSSPropertyCursor: {
            RefPtr<CSSValueList> list;
            CursorList* cursors = style->cursors();
            if (cursors && cursors->size() > 0) {
                list = CSSValueList::createCommaSeparated();
                for (unsigned i = 0; i < cursors->size(); ++i)
                    if (StyleImage* image = cursors->at(i).image())
                        list->append(*image->cssValue());
            }
            auto value = cssValuePool.createValue(style->cursor());
            if (list) {
                list->append(WTFMove(value));
                return list;
            }
            return WTFMove(value);
        }
#if ENABLE(CURSOR_VISIBILITY)
        case CSSPropertyWebkitCursorVisibility:
            return cssValuePool.createValue(style->cursorVisibility());
#endif
        case CSSPropertyDirection:
            return cssValuePool.createValue(style->direction());
        case CSSPropertyDisplay:
            return cssValuePool.createValue(style->display());
        case CSSPropertyEmptyCells:
            return cssValuePool.createValue(style->emptyCells());
        case CSSPropertyAlignContent: {
            ContentPosition position = style->alignContentPosition();
            ContentDistributionType distribution = style->alignContentDistribution();
            resolveContentAlignmentAuto(position, distribution, renderer);
            return valueForContentPositionAndDistributionWithOverflowAlignment(position, distribution, style->alignContentOverflowAlignment());
        }
        case CSSPropertyAlignItems:
            return valueForItemPositionWithOverflowAlignment(resolveContainerAlignmentAuto(style->alignItemsPosition(), renderer), style->alignItemsOverflowAlignment(), NonLegacyPosition);
        case CSSPropertyAlignSelf: {
            OverflowAlignment overflow = style->alignSelfOverflowAlignment();
            ItemPosition alignSelf = resolveSelfAlignmentAuto(style->alignSelfPosition(), overflow, renderer);
            return valueForItemPositionWithOverflowAlignment(alignSelf, overflow, NonLegacyPosition);
        }
        case CSSPropertyFlex:
            return getCSSPropertyValuesForShorthandProperties(flexShorthand());
        case CSSPropertyFlexBasis:
            return cssValuePool.createValue(style->flexBasis());
        case CSSPropertyFlexDirection:
            return cssValuePool.createValue(style->flexDirection());
        case CSSPropertyFlexFlow:
            return getCSSPropertyValuesForShorthandProperties(flexFlowShorthand());
        case CSSPropertyFlexGrow:
            return cssValuePool.createValue(style->flexGrow());
        case CSSPropertyFlexShrink:
            return cssValuePool.createValue(style->flexShrink());
        case CSSPropertyFlexWrap:
            return cssValuePool.createValue(style->flexWrap());
        case CSSPropertyJustifyContent: {
            ContentPosition position = style->justifyContentPosition();
            ContentDistributionType distribution = style->justifyContentDistribution();
            resolveContentAlignmentAuto(position, distribution, renderer);
            return valueForContentPositionAndDistributionWithOverflowAlignment(position, distribution, style->justifyContentOverflowAlignment());
        }
        case CSSPropertyJustifyItems:
            return valueForItemPositionWithOverflowAlignment(resolveContainerAlignmentAuto(style->justifyItemsPosition(), renderer), style->justifyItemsOverflowAlignment(), style->justifyItemsPositionType());
        case CSSPropertyJustifySelf: {
            OverflowAlignment overflow = style->justifySelfOverflowAlignment();
            ItemPosition justifySelf = resolveSelfAlignmentAuto(style->justifySelfPosition(), overflow, renderer);
            return valueForItemPositionWithOverflowAlignment(justifySelf, overflow, NonLegacyPosition);
        }
        case CSSPropertyOrder:
            return cssValuePool.createValue(style->order(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyFloat:
            if (style->display() != NONE && style->hasOutOfFlowPosition())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return cssValuePool.createValue(style->floating());
        case CSSPropertyFont: {
            RefPtr<CSSFontValue> computedFont = CSSFontValue::create();
            computedFont->style = fontStyleFromStyle(style.get());
            if (style->fontDescription().variantCaps() == FontVariantCaps::Small)
                computedFont->variant = CSSValuePool::singleton().createIdentifierValue(CSSValueSmallCaps);
            else
                computedFont->variant = CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);
            computedFont->weight = fontWeightFromStyle(style.get());
            computedFont->size = fontSizeFromStyle(*style);
            computedFont->lineHeight = lineHeightFromStyle(*style);
            computedFont->family = fontFamilyFromStyle(style.get());
            return computedFont;
        }
        case CSSPropertyFontFamily: {
            RefPtr<CSSValueList> fontFamilyList = fontFamilyFromStyle(style.get());
            // If there's only a single family, return that as a CSSPrimitiveValue.
            // NOTE: Gecko always returns this as a comma-separated CSSPrimitiveValue string.
            if (fontFamilyList->length() == 1)
                return fontFamilyList->item(0);
            return fontFamilyList;
        }
        case CSSPropertyFontSize:
            return fontSizeFromStyle(*style);
        case CSSPropertyFontStyle:
            return fontStyleFromStyle(style.get());
        case CSSPropertyFontVariant:
            return fontVariantFromStyle(style.get());
        case CSSPropertyFontWeight:
            return fontWeightFromStyle(style.get());
        case CSSPropertyFontSynthesis:
            return fontSynthesisFromStyle(*style);
        case CSSPropertyFontFeatureSettings: {
            const FontFeatureSettings& featureSettings = style->fontDescription().featureSettings();
            if (!featureSettings.size())
                return cssValuePool.createIdentifierValue(CSSValueNormal);
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (auto& feature : featureSettings)
                list->append(CSSFontFeatureValue::create(FontFeatureTag(feature.tag()), feature.value()));
            return list;
        }
#if ENABLE(CSS_GRID_LAYOUT)
        case CSSPropertyWebkitGridAutoFlow: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            ASSERT(style->isGridAutoFlowDirectionRow() || style->isGridAutoFlowDirectionColumn());
            if (style->isGridAutoFlowDirectionRow())
                list->append(cssValuePool.createIdentifierValue(CSSValueRow));
            else
                list->append(cssValuePool.createIdentifierValue(CSSValueColumn));

            if (style->isGridAutoFlowAlgorithmDense())
                list->append(cssValuePool.createIdentifierValue(CSSValueDense));

            return list;
        }

        // Specs mention that getComputedStyle() should return the used value of the property instead of the computed
        // one for grid-definition-{rows|columns} but not for the grid-auto-{rows|columns} as things like
        // grid-auto-columns: 2fr; cannot be resolved to a value in pixels as the '2fr' means very different things
        // depending on the size of the explicit grid or the number of implicit tracks added to the grid. See
        // http://lists.w3.org/Archives/Public/www-style/2013Nov/0014.html
        case CSSPropertyWebkitGridAutoColumns:
            return specifiedValueForGridTrackSize(style->gridAutoColumns(), *style);
        case CSSPropertyWebkitGridAutoRows:
            return specifiedValueForGridTrackSize(style->gridAutoRows(), *style);

        case CSSPropertyWebkitGridTemplateColumns:
            return valueForGridTrackList(ForColumns, renderer, *style);
        case CSSPropertyWebkitGridTemplateRows:
            return valueForGridTrackList(ForRows, renderer, *style);

        case CSSPropertyWebkitGridColumnStart:
            return valueForGridPosition(style->gridItemColumnStart());
        case CSSPropertyWebkitGridColumnEnd:
            return valueForGridPosition(style->gridItemColumnEnd());
        case CSSPropertyWebkitGridRowStart:
            return valueForGridPosition(style->gridItemRowStart());
        case CSSPropertyWebkitGridRowEnd:
            return valueForGridPosition(style->gridItemRowEnd());
        case CSSPropertyWebkitGridArea:
            return getCSSPropertyValuesForGridShorthand(webkitGridAreaShorthand());
        case CSSPropertyWebkitGridTemplate:
            return getCSSPropertyValuesForGridShorthand(webkitGridTemplateShorthand());
        case CSSPropertyWebkitGrid:
            return getCSSPropertyValuesForGridShorthand(webkitGridShorthand());
        case CSSPropertyWebkitGridColumn:
            return getCSSPropertyValuesForGridShorthand(webkitGridColumnShorthand());
        case CSSPropertyWebkitGridRow:
            return getCSSPropertyValuesForGridShorthand(webkitGridRowShorthand());

        case CSSPropertyWebkitGridTemplateAreas:
            if (!style->namedGridAreaRowCount()) {
                ASSERT(!style->namedGridAreaColumnCount());
                return cssValuePool.createIdentifierValue(CSSValueNone);
            }

            return CSSGridTemplateAreasValue::create(style->namedGridArea(), style->namedGridAreaRowCount(), style->namedGridAreaColumnCount());
        case CSSPropertyWebkitGridColumnGap:
            return zoomAdjustedPixelValueForLength(style->gridColumnGap(), *style);
        case CSSPropertyWebkitGridRowGap:
            return zoomAdjustedPixelValueForLength(style->gridRowGap(), *style);
        case CSSPropertyWebkitGridGap:
            return getCSSPropertyValuesForGridShorthand(webkitGridGapShorthand());
#endif /* ENABLE(CSS_GRID_LAYOUT) */
        case CSSPropertyHeight:
            if (renderer && !renderer->isRenderSVGModelObject()) {
                // According to http://www.w3.org/TR/CSS2/visudet.html#the-height-property,
                // the "height" property does not apply for non-replaced inline elements.
                if (!renderer->isReplaced() && renderer->isInline())
                    return cssValuePool.createIdentifierValue(CSSValueAuto);
                return zoomAdjustedPixelValue(sizingBox(*renderer).height(), *style);
            }
            return zoomAdjustedPixelValueForLength(style->height(), *style);
        case CSSPropertyWebkitHyphens:
            return cssValuePool.createValue(style->hyphens());
        case CSSPropertyWebkitHyphenateCharacter:
            if (style->hyphenationString().isNull())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style->hyphenationString(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitHyphenateLimitAfter:
            if (style->hyphenationLimitAfter() < 0)
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style->hyphenationLimitAfter(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitHyphenateLimitBefore:
            if (style->hyphenationLimitBefore() < 0)
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style->hyphenationLimitBefore(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitHyphenateLimitLines:
            if (style->hyphenationLimitLines() < 0)
                return CSSPrimitiveValue::createIdentifier(CSSValueNoLimit);
            return CSSPrimitiveValue::create(style->hyphenationLimitLines(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBorderFit:
            if (style->borderFit() == BorderFitBorder)
                return cssValuePool.createIdentifierValue(CSSValueBorder);
            return cssValuePool.createIdentifierValue(CSSValueLines);
#if ENABLE(CSS_IMAGE_ORIENTATION)
        case CSSPropertyImageOrientation:
            return cssValuePool.createValue(style->imageOrientation());
#endif
        case CSSPropertyImageRendering:
            return CSSPrimitiveValue::create(style->imageRendering());
#if ENABLE(CSS_IMAGE_RESOLUTION)
        case CSSPropertyImageResolution:
            return cssValuePool.createValue(style->imageResolution(), CSSPrimitiveValue::CSS_DPPX);
#endif
        case CSSPropertyLeft:
            return positionOffsetValue(*style, CSSPropertyLeft);
        case CSSPropertyLetterSpacing:
            if (!style->letterSpacing())
                return cssValuePool.createIdentifierValue(CSSValueNormal);
            return zoomAdjustedPixelValue(style->letterSpacing(), *style);
        case CSSPropertyWebkitLineClamp:
            if (style->lineClamp().isNone())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return cssValuePool.createValue(style->lineClamp().value(), style->lineClamp().isPercentage() ? CSSPrimitiveValue::CSS_PERCENTAGE : CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyLineHeight:
            return lineHeightFromStyle(*style);
        case CSSPropertyListStyleImage:
            if (style->listStyleImage())
                return style->listStyleImage()->cssValue();
            return cssValuePool.createIdentifierValue(CSSValueNone);
        case CSSPropertyListStylePosition:
            return cssValuePool.createValue(style->listStylePosition());
        case CSSPropertyListStyleType:
            return cssValuePool.createValue(style->listStyleType());
        case CSSPropertyWebkitLocale:
            if (style->locale().isNull())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style->locale(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyMarginTop:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::marginTop, &RenderBoxModelObject::marginTop>(*style, renderer);
        case CSSPropertyMarginRight: {
            Length marginRight = style->marginRight();
            if (marginRight.isFixed() || !is<RenderBox>(renderer))
                return zoomAdjustedPixelValueForLength(marginRight, *style);
            float value;
            if (marginRight.isPercentOrCalculated()) {
                // RenderBox gives a marginRight() that is the distance between the right-edge of the child box
                // and the right-edge of the containing box, when display == BLOCK. Let's calculate the absolute
                // value of the specified margin-right % instead of relying on RenderBox's marginRight() value.
                value = minimumValueForLength(marginRight, downcast<RenderBox>(*renderer).containingBlockLogicalWidthForContent());
            } else
                value = downcast<RenderBox>(*renderer).marginRight();
            return zoomAdjustedPixelValue(value, *style);
        }
        case CSSPropertyMarginBottom:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::marginBottom, &RenderBoxModelObject::marginBottom>(*style, renderer);
        case CSSPropertyMarginLeft:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::marginLeft, &RenderBoxModelObject::marginLeft>(*style, renderer);
        case CSSPropertyWebkitMarqueeDirection:
            return cssValuePool.createValue(style->marqueeDirection());
        case CSSPropertyWebkitMarqueeIncrement:
            return cssValuePool.createValue(style->marqueeIncrement());
        case CSSPropertyWebkitMarqueeRepetition:
            if (style->marqueeLoopCount() < 0)
                return cssValuePool.createIdentifierValue(CSSValueInfinite);
            return cssValuePool.createValue(style->marqueeLoopCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitMarqueeStyle:
            return cssValuePool.createValue(style->marqueeBehavior());
        case CSSPropertyWebkitUserModify:
            return cssValuePool.createValue(style->userModify());
        case CSSPropertyMaxHeight: {
            const Length& maxHeight = style->maxHeight();
            if (maxHeight.isUndefined())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValueForLength(maxHeight, *style);
        }
        case CSSPropertyMaxWidth: {
            const Length& maxWidth = style->maxWidth();
            if (maxWidth.isUndefined())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValueForLength(maxWidth, *style);
        }
        case CSSPropertyMinHeight:
            if (style->minHeight().isAuto()) {
                if (isFlexOrGrid(styledNode->parentNode()))
                    return cssValuePool.createIdentifierValue(CSSValueAuto);
                return zoomAdjustedPixelValue(0, *style);
            }
            return zoomAdjustedPixelValueForLength(style->minHeight(), *style);
        case CSSPropertyMinWidth:
            if (style->minWidth().isAuto()) {
                if (isFlexOrGrid(styledNode->parentNode()))
                    return cssValuePool.createIdentifierValue(CSSValueAuto);
                return zoomAdjustedPixelValue(0, *style);
            }
            return zoomAdjustedPixelValueForLength(style->minWidth(), *style);
        case CSSPropertyObjectFit:
            return cssValuePool.createValue(style->objectFit());
        case CSSPropertyOpacity:
            return cssValuePool.createValue(style->opacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOrphans:
            if (style->hasAutoOrphans())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style->orphans(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOutlineColor:
            return m_allowVisitedStyle ? cssValuePool.createColorValue(style->visitedDependentColor(CSSPropertyOutlineColor).rgb()) : currentColorOrValidColor(style.get(), style->outlineColor());
        case CSSPropertyOutlineOffset:
            return zoomAdjustedPixelValue(style->outlineOffset(), *style);
        case CSSPropertyOutlineStyle:
            if (style->outlineStyleIsAuto())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style->outlineStyle());
        case CSSPropertyOutlineWidth:
            return zoomAdjustedPixelValue(style->outlineWidth(), *style);
        case CSSPropertyOverflow:
            return cssValuePool.createValue(std::max(style->overflowX(), style->overflowY()));
        case CSSPropertyOverflowWrap:
            return cssValuePool.createValue(style->overflowWrap());
        case CSSPropertyOverflowX:
            return cssValuePool.createValue(style->overflowX());
        case CSSPropertyOverflowY:
            return cssValuePool.createValue(style->overflowY());
        case CSSPropertyPaddingTop:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingTop, &RenderBoxModelObject::computedCSSPaddingTop>(*style, renderer);
        case CSSPropertyPaddingRight:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingRight, &RenderBoxModelObject::computedCSSPaddingRight>(*style, renderer);
        case CSSPropertyPaddingBottom:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingBottom, &RenderBoxModelObject::computedCSSPaddingBottom>(*style, renderer);
        case CSSPropertyPaddingLeft:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingLeft, &RenderBoxModelObject::computedCSSPaddingLeft>(*style, renderer);
        case CSSPropertyPageBreakAfter:
            return cssValuePool.createValue(convertToPageBreak(style->breakAfter()));
        case CSSPropertyPageBreakBefore:
            return cssValuePool.createValue(convertToPageBreak(style->breakBefore()));
        case CSSPropertyPageBreakInside:
            return cssValuePool.createValue(convertToPageBreak(style->breakInside()));
        case CSSPropertyBreakAfter:
            return cssValuePool.createValue(style->breakAfter());
        case CSSPropertyBreakBefore:
            return cssValuePool.createValue(style->breakBefore());
        case CSSPropertyBreakInside:
            return cssValuePool.createValue(style->breakInside());
        case CSSPropertyHangingPunctuation:
            return hangingPunctuationToCSSValue(style->hangingPunctuation());
        case CSSPropertyPosition:
            return cssValuePool.createValue(style->position());
        case CSSPropertyRight:
            return positionOffsetValue(*style, CSSPropertyRight);
        case CSSPropertyWebkitRubyPosition:
            return cssValuePool.createValue(style->rubyPosition());
        case CSSPropertyTableLayout:
            return cssValuePool.createValue(style->tableLayout());
        case CSSPropertyTextAlign:
            return cssValuePool.createValue(style->textAlign());
        case CSSPropertyTextDecoration:
            return renderTextDecorationFlagsToCSSValue(style->textDecoration());
#if ENABLE(CSS3_TEXT)
        case CSSPropertyWebkitTextAlignLast:
            return cssValuePool.createValue(style->textAlignLast());
        case CSSPropertyWebkitTextJustify:
            return cssValuePool.createValue(style->textJustify());
#endif // CSS3_TEXT
        case CSSPropertyWebkitTextDecoration:
            return getCSSPropertyValuesForShorthandProperties(webkitTextDecorationShorthand());
        case CSSPropertyWebkitTextDecorationLine:
            return renderTextDecorationFlagsToCSSValue(style->textDecoration());
        case CSSPropertyWebkitTextDecorationStyle:
            return renderTextDecorationStyleFlagsToCSSValue(style->textDecorationStyle());
        case CSSPropertyWebkitTextDecorationColor:
            return currentColorOrValidColor(style.get(), style->textDecorationColor());
        case CSSPropertyWebkitTextDecorationSkip:
            return renderTextDecorationSkipFlagsToCSSValue(style->textDecorationSkip());
        case CSSPropertyWebkitTextUnderlinePosition:
            return cssValuePool.createValue(style->textUnderlinePosition());
        case CSSPropertyWebkitTextDecorationsInEffect:
            return renderTextDecorationFlagsToCSSValue(style->textDecorationsInEffect());
        case CSSPropertyWebkitTextFillColor:
            return currentColorOrValidColor(style.get(), style->textFillColor());
        case CSSPropertyWebkitTextEmphasisColor:
            return currentColorOrValidColor(style.get(), style->textEmphasisColor());
        case CSSPropertyWebkitTextEmphasisPosition:
            return renderEmphasisPositionFlagsToCSSValue(style->textEmphasisPosition());
        case CSSPropertyWebkitTextEmphasisStyle:
            switch (style->textEmphasisMark()) {
            case TextEmphasisMarkNone:
                return cssValuePool.createIdentifierValue(CSSValueNone);
            case TextEmphasisMarkCustom:
                return cssValuePool.createValue(style->textEmphasisCustomMark(), CSSPrimitiveValue::CSS_STRING);
            case TextEmphasisMarkAuto:
                ASSERT_NOT_REACHED();
#if ASSERT_DISABLED
                FALLTHROUGH;
#endif
            case TextEmphasisMarkDot:
            case TextEmphasisMarkCircle:
            case TextEmphasisMarkDoubleCircle:
            case TextEmphasisMarkTriangle:
            case TextEmphasisMarkSesame: {
                RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
                list->append(cssValuePool.createValue(style->textEmphasisFill()));
                list->append(cssValuePool.createValue(style->textEmphasisMark()));
                return list;
            }
            }
        case CSSPropertyTextIndent: {
            // If CSS3_TEXT is disabled or text-indent has only one value(<length> | <percentage>),
            // getPropertyCSSValue() returns CSSValue.
            RefPtr<CSSValue> textIndent = zoomAdjustedPixelValueForLength(style->textIndent(), *style);
#if ENABLE(CSS3_TEXT)
            // If CSS3_TEXT is enabled and text-indent has -webkit-each-line or -webkit-hanging,
            // getPropertyCSSValue() returns CSSValueList.
            if (style->textIndentLine() == TextIndentEachLine || style->textIndentType() == TextIndentHanging) {
                RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
                list->append(textIndent.releaseNonNull());
                if (style->textIndentLine() == TextIndentEachLine)
                    list->append(cssValuePool.createIdentifierValue(CSSValueWebkitEachLine));
                if (style->textIndentType() == TextIndentHanging)
                    list->append(cssValuePool.createIdentifierValue(CSSValueWebkitHanging));
                return list;
            }
#endif
            return textIndent;
        }
        case CSSPropertyTextShadow:
            return valueForShadow(style->textShadow(), propertyID, *style);
        case CSSPropertyTextRendering:
            return cssValuePool.createValue(style->fontDescription().textRenderingMode());
        case CSSPropertyTextOverflow:
            if (style->textOverflow())
                return cssValuePool.createIdentifierValue(CSSValueEllipsis);
            return cssValuePool.createIdentifierValue(CSSValueClip);
        case CSSPropertyWebkitTextSecurity:
            return cssValuePool.createValue(style->textSecurity());
#if ENABLE(IOS_TEXT_AUTOSIZING)
        case CSSPropertyWebkitTextSizeAdjust:
            if (style->textSizeAdjust().isAuto())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            if (style->textSizeAdjust().isNone())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return CSSPrimitiveValue::create(style->textSizeAdjust().percentage(), CSSPrimitiveValue::CSS_PERCENTAGE);
#endif
        case CSSPropertyWebkitTextStrokeColor:
            return currentColorOrValidColor(style.get(), style->textStrokeColor());
        case CSSPropertyWebkitTextStrokeWidth:
            return zoomAdjustedPixelValue(style->textStrokeWidth(), *style);
        case CSSPropertyTextTransform:
            return cssValuePool.createValue(style->textTransform());
        case CSSPropertyTop:
            return positionOffsetValue(*style, CSSPropertyTop);
        case CSSPropertyUnicodeBidi:
            return cssValuePool.createValue(style->unicodeBidi());
        case CSSPropertyVerticalAlign:
            switch (style->verticalAlign()) {
                case BASELINE:
                    return cssValuePool.createIdentifierValue(CSSValueBaseline);
                case MIDDLE:
                    return cssValuePool.createIdentifierValue(CSSValueMiddle);
                case SUB:
                    return cssValuePool.createIdentifierValue(CSSValueSub);
                case SUPER:
                    return cssValuePool.createIdentifierValue(CSSValueSuper);
                case TEXT_TOP:
                    return cssValuePool.createIdentifierValue(CSSValueTextTop);
                case TEXT_BOTTOM:
                    return cssValuePool.createIdentifierValue(CSSValueTextBottom);
                case TOP:
                    return cssValuePool.createIdentifierValue(CSSValueTop);
                case BOTTOM:
                    return cssValuePool.createIdentifierValue(CSSValueBottom);
                case BASELINE_MIDDLE:
                    return cssValuePool.createIdentifierValue(CSSValueWebkitBaselineMiddle);
                case LENGTH:
                    return cssValuePool.createValue(style->verticalAlignLength());
            }
            ASSERT_NOT_REACHED();
            return nullptr;
        case CSSPropertyVisibility:
            return cssValuePool.createValue(style->visibility());
        case CSSPropertyWhiteSpace:
            return cssValuePool.createValue(style->whiteSpace());
        case CSSPropertyWidows:
            if (style->hasAutoWidows())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style->widows(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWidth:
            if (renderer && !renderer->isRenderSVGModelObject()) {
                // According to http://www.w3.org/TR/CSS2/visudet.html#the-width-property,
                // the "width" property does not apply for non-replaced inline elements.
                if (!renderer->isReplaced() && renderer->isInline())
                    return cssValuePool.createIdentifierValue(CSSValueAuto);
                return zoomAdjustedPixelValue(sizingBox(*renderer).width(), *style);
            }
            return zoomAdjustedPixelValueForLength(style->width(), *style);
        case CSSPropertyWillChange:
            return getWillChangePropertyValue(style->willChange());
            break;
        case CSSPropertyWordBreak:
            return cssValuePool.createValue(style->wordBreak());
        case CSSPropertyWordSpacing:
            return zoomAdjustedPixelValue(style->fontCascade().wordSpacing(), *style);
        case CSSPropertyWordWrap:
            return cssValuePool.createValue(style->overflowWrap());
        case CSSPropertyWebkitLineBreak:
            return cssValuePool.createValue(style->lineBreak());
        case CSSPropertyWebkitNbspMode:
            return cssValuePool.createValue(style->nbspMode());
        case CSSPropertyResize:
            return cssValuePool.createValue(style->resize());
        case CSSPropertyWebkitFontKerning:
            return cssValuePool.createValue(style->fontDescription().kerning());
        case CSSPropertyWebkitFontSmoothing:
            return cssValuePool.createValue(style->fontDescription().fontSmoothing());
        case CSSPropertyFontVariantLigatures:
            return fontVariantLigaturesPropertyValue(style->fontDescription().variantCommonLigatures(), style->fontDescription().variantDiscretionaryLigatures(), style->fontDescription().variantHistoricalLigatures(), style->fontDescription().variantContextualAlternates());
        case CSSPropertyFontVariantPosition:
            return fontVariantPositionPropertyValue(style->fontDescription().variantPosition());
        case CSSPropertyFontVariantCaps:
            return fontVariantCapsPropertyValue(style->fontDescription().variantCaps());
        case CSSPropertyFontVariantNumeric:
            return fontVariantNumericPropertyValue(style->fontDescription().variantNumericFigure(), style->fontDescription().variantNumericSpacing(), style->fontDescription().variantNumericFraction(), style->fontDescription().variantNumericOrdinal(), style->fontDescription().variantNumericSlashedZero());
        case CSSPropertyFontVariantAlternates:
            return fontVariantAlternatesPropertyValue(style->fontDescription().variantAlternates());
        case CSSPropertyFontVariantEastAsian:
            return fontVariantEastAsianPropertyValue(style->fontDescription().variantEastAsianVariant(), style->fontDescription().variantEastAsianWidth(), style->fontDescription().variantEastAsianRuby());
        case CSSPropertyZIndex:
            if (style->hasAutoZIndex())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createValue(style->zIndex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyZoom:
            return cssValuePool.createValue(style->zoom(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyBoxSizing:
            if (style->boxSizing() == CONTENT_BOX)
                return cssValuePool.createIdentifierValue(CSSValueContentBox);
            return cssValuePool.createIdentifierValue(CSSValueBorderBox);
#if ENABLE(DASHBOARD_SUPPORT)
        case CSSPropertyWebkitDashboardRegion:
        {
            const Vector<StyleDashboardRegion>& regions = style->dashboardRegions();
            unsigned count = regions.size();
            if (count == 1 && regions[0].type == StyleDashboardRegion::None)
                return cssValuePool.createIdentifierValue(CSSValueNone);

            RefPtr<DashboardRegion> firstRegion;
            DashboardRegion* previousRegion = nullptr;
            for (unsigned i = 0; i < count; i++) {
                auto region = DashboardRegion::create();
                StyleDashboardRegion styleRegion = regions[i];

                region->m_label = styleRegion.label;
                LengthBox offset = styleRegion.offset;
                region->setTop(zoomAdjustedPixelValue(offset.top().value(), *style));
                region->setRight(zoomAdjustedPixelValue(offset.right().value(), *style));
                region->setBottom(zoomAdjustedPixelValue(offset.bottom().value(), *style));
                region->setLeft(zoomAdjustedPixelValue(offset.left().value(), *style));
                region->m_isRectangle = (styleRegion.type == StyleDashboardRegion::Rectangle);
                region->m_isCircle = (styleRegion.type == StyleDashboardRegion::Circle);

                if (previousRegion)
                    previousRegion->m_next = region.copyRef();
                else
                    firstRegion = region.copyRef();
                previousRegion = region.ptr();
            }
            return cssValuePool.createValue(WTFMove(firstRegion));
        }
#endif
        case CSSPropertyAnimationDelay:
        case CSSPropertyWebkitAnimationDelay:
            return getDelayValue(style->animations());
        case CSSPropertyAnimationDirection:
        case CSSPropertyWebkitAnimationDirection: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
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
        case CSSPropertyWebkitAnimationDuration:
            return getDurationValue(style->animations());
        case CSSPropertyAnimationFillMode:
        case CSSPropertyWebkitAnimationFillMode: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    switch (t->animation(i).fillMode()) {
                    case AnimationFillModeNone:
                        list->append(cssValuePool.createIdentifierValue(CSSValueNone));
                        break;
                    case AnimationFillModeForwards:
                        list->append(cssValuePool.createIdentifierValue(CSSValueForwards));
                        break;
                    case AnimationFillModeBackwards:
                        list->append(cssValuePool.createIdentifierValue(CSSValueBackwards));
                        break;
                    case AnimationFillModeBoth:
                        list->append(cssValuePool.createIdentifierValue(CSSValueBoth));
                        break;
                    }
                }
            } else
                list->append(cssValuePool.createIdentifierValue(CSSValueNone));
            return list;
        }
        case CSSPropertyAnimationIterationCount:
        case CSSPropertyWebkitAnimationIterationCount: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    double iterationCount = t->animation(i).iterationCount();
                    if (iterationCount == Animation::IterationCountInfinite)
                        list->append(cssValuePool.createIdentifierValue(CSSValueInfinite));
                    else
                        list->append(cssValuePool.createValue(iterationCount, CSSPrimitiveValue::CSS_NUMBER));
                }
            } else
                list->append(cssValuePool.createValue(Animation::initialIterationCount(), CSSPrimitiveValue::CSS_NUMBER));
            return list;
        }
        case CSSPropertyAnimationName:
        case CSSPropertyWebkitAnimationName: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i)
                    list->append(cssValuePool.createValue(t->animation(i).name(), CSSPrimitiveValue::CSS_STRING));
            } else
                list->append(cssValuePool.createIdentifierValue(CSSValueNone));
            return list;
        }
        case CSSPropertyAnimationPlayState:
        case CSSPropertyWebkitAnimationPlayState: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    int prop = t->animation(i).playState();
                    if (prop == AnimPlayStatePlaying)
                        list->append(cssValuePool.createIdentifierValue(CSSValueRunning));
                    else
                        list->append(cssValuePool.createIdentifierValue(CSSValuePaused));
                }
            } else
                list->append(cssValuePool.createIdentifierValue(CSSValueRunning));
            return list;
        }
        case CSSPropertyAnimationTimingFunction:
        case CSSPropertyWebkitAnimationTimingFunction:
            return getTimingFunctionValue(style->animations());
#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
        case CSSPropertyWebkitAnimationTrigger:
            return getAnimationTriggerValue(style->animations(), *style);
#endif
        case CSSPropertyWebkitAppearance:
            return cssValuePool.createValue(style->appearance());
        case CSSPropertyWebkitAspectRatio:
            if (style->aspectRatioType() == AspectRatioAuto)
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            if (style->aspectRatioType() == AspectRatioFromDimensions)
                return cssValuePool.createIdentifierValue(CSSValueFromDimensions);
            if (style->aspectRatioType() == AspectRatioFromIntrinsic)
                return cssValuePool.createIdentifierValue(CSSValueFromIntrinsic);
            return CSSAspectRatioValue::create(style->aspectRatioNumerator(), style->aspectRatioDenominator());
        case CSSPropertyWebkitBackfaceVisibility:
            return cssValuePool.createIdentifierValue((style->backfaceVisibility() == BackfaceVisibilityHidden) ? CSSValueHidden : CSSValueVisible);
        case CSSPropertyWebkitBorderImage:
            return valueForNinePieceImage(style->borderImage());
        case CSSPropertyBorderImageOutset:
            return valueForNinePieceImageQuad(style->borderImage().outset());
        case CSSPropertyBorderImageRepeat:
            return valueForNinePieceImageRepeat(style->borderImage());
        case CSSPropertyBorderImageSlice:
            return valueForNinePieceImageSlice(style->borderImage());
        case CSSPropertyBorderImageWidth:
            return valueForNinePieceImageQuad(style->borderImage().borderSlices());
        case CSSPropertyWebkitMaskBoxImage:
            return valueForNinePieceImage(style->maskBoxImage());
        case CSSPropertyWebkitMaskBoxImageOutset:
            return valueForNinePieceImageQuad(style->maskBoxImage().outset());
        case CSSPropertyWebkitMaskBoxImageRepeat:
            return valueForNinePieceImageRepeat(style->maskBoxImage());
        case CSSPropertyWebkitMaskBoxImageSlice:
            return valueForNinePieceImageSlice(style->maskBoxImage());
        case CSSPropertyWebkitMaskBoxImageWidth:
            return valueForNinePieceImageQuad(style->maskBoxImage().borderSlices());
        case CSSPropertyWebkitMaskBoxImageSource:
            if (style->maskBoxImageSource())
                return style->maskBoxImageSource()->cssValue();
            return cssValuePool.createIdentifierValue(CSSValueNone);
        case CSSPropertyWebkitFontSizeDelta:
            // Not a real style property -- used by the editing engine -- so has no computed value.
            break;
        case CSSPropertyWebkitInitialLetter: {
            RefPtr<CSSPrimitiveValue> drop = !style->initialLetterDrop() ? cssValuePool.createIdentifierValue(CSSValueNormal) : cssValuePool.createValue(style->initialLetterDrop(), CSSPrimitiveValue::CSS_NUMBER);
            RefPtr<CSSPrimitiveValue> size = !style->initialLetterHeight() ? cssValuePool.createIdentifierValue(CSSValueNormal) : cssValuePool.createValue(style->initialLetterHeight(), CSSPrimitiveValue::CSS_NUMBER);
            return cssValuePool.createValue(Pair::create(drop.release(), size.release()));
        }
        case CSSPropertyWebkitMarginBottomCollapse:
        case CSSPropertyWebkitMarginAfterCollapse:
            return cssValuePool.createValue(style->marginAfterCollapse());
        case CSSPropertyWebkitMarginTopCollapse:
        case CSSPropertyWebkitMarginBeforeCollapse:
            return cssValuePool.createValue(style->marginBeforeCollapse());
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
        case CSSPropertyWebkitOverflowScrolling:
            if (!style->useTouchOverflowScrolling())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            return cssValuePool.createIdentifierValue(CSSValueTouch);
#endif
        case CSSPropertyPerspective:
            if (!style->hasPerspective())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValue(style->perspective(), *style);
        case CSSPropertyPerspectiveOrigin: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                LayoutRect box;
                if (is<RenderBox>(*renderer))
                    box = downcast<RenderBox>(*renderer).borderBoxRect();

                list->append(zoomAdjustedPixelValue(minimumValueForLength(style->perspectiveOriginX(), box.width()), *style));
                list->append(zoomAdjustedPixelValue(minimumValueForLength(style->perspectiveOriginY(), box.height()), *style));
            }
            else {
                list->append(zoomAdjustedPixelValueForLength(style->perspectiveOriginX(), *style));
                list->append(zoomAdjustedPixelValueForLength(style->perspectiveOriginY(), *style));

            }
            return list;
        }
        case CSSPropertyWebkitRtlOrdering:
            return cssValuePool.createIdentifierValue(style->rtlOrdering() ? CSSValueVisual : CSSValueLogical);
#if ENABLE(TOUCH_EVENTS)
        case CSSPropertyWebkitTapHighlightColor:
            return currentColorOrValidColor(style.get(), style->tapHighlightColor());
        case CSSPropertyTouchAction:
            return cssValuePool.createValue(style->touchAction());
#endif
#if PLATFORM(IOS)
        case CSSPropertyWebkitTouchCallout:
            return cssValuePool.createIdentifierValue(style->touchCalloutEnabled() ? CSSValueDefault : CSSValueNone);
#endif
        case CSSPropertyWebkitUserDrag:
            return cssValuePool.createValue(style->userDrag());
        case CSSPropertyWebkitUserSelect:
            return cssValuePool.createValue(style->userSelect());
        case CSSPropertyBorderBottomLeftRadius:
            return getBorderRadiusCornerValue(style->borderBottomLeftRadius(), *style);
        case CSSPropertyBorderBottomRightRadius:
            return getBorderRadiusCornerValue(style->borderBottomRightRadius(), *style);
        case CSSPropertyBorderTopLeftRadius:
            return getBorderRadiusCornerValue(style->borderTopLeftRadius(), *style);
        case CSSPropertyBorderTopRightRadius:
            return getBorderRadiusCornerValue(style->borderTopRightRadius(), *style);
        case CSSPropertyClip: {
            if (!style->hasClip())
                return cssValuePool.createIdentifierValue(CSSValueAuto);
            auto rect = Rect::create();
            rect->setTop(autoOrZoomAdjustedValue(style->clip().top(), *style));
            rect->setRight(autoOrZoomAdjustedValue(style->clip().right(), *style));
            rect->setBottom(autoOrZoomAdjustedValue(style->clip().bottom(), *style));
            rect->setLeft(autoOrZoomAdjustedValue(style->clip().left(), *style));
            return cssValuePool.createValue(WTFMove(rect));
        }
        case CSSPropertySpeak:
            return cssValuePool.createValue(style->speak());
        case CSSPropertyTransform:
            return computedTransform(renderer, *style);
        case CSSPropertyTransformOrigin: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                LayoutRect box;
                if (is<RenderBox>(*renderer))
                    box = downcast<RenderBox>(*renderer).borderBoxRect();

                list->append(zoomAdjustedPixelValue(minimumValueForLength(style->transformOriginX(), box.width()), *style));
                list->append(zoomAdjustedPixelValue(minimumValueForLength(style->transformOriginY(), box.height()), *style));
                if (style->transformOriginZ() != 0)
                    list->append(zoomAdjustedPixelValue(style->transformOriginZ(), *style));
            } else {
                list->append(zoomAdjustedPixelValueForLength(style->transformOriginX(), *style));
                list->append(zoomAdjustedPixelValueForLength(style->transformOriginY(), *style));
                if (style->transformOriginZ() != 0)
                    list->append(zoomAdjustedPixelValue(style->transformOriginZ(), *style));
            }
            return list;
        }
        case CSSPropertyTransformStyle:
        case CSSPropertyWebkitTransformStyle:
            return cssValuePool.createIdentifierValue((style->transformStyle3D() == TransformStyle3DPreserve3D) ? CSSValuePreserve3d : CSSValueFlat);
        case CSSPropertyTransitionDelay:
        case CSSPropertyWebkitTransitionDelay:
            return getDelayValue(style->transitions());
        case CSSPropertyTransitionDuration:
        case CSSPropertyWebkitTransitionDuration:
            return getDurationValue(style->transitions());
        case CSSPropertyTransitionProperty:
        case CSSPropertyWebkitTransitionProperty:
            return getTransitionPropertyValue(style->transitions());
        case CSSPropertyTransitionTimingFunction:
        case CSSPropertyWebkitTransitionTimingFunction:
            return getTimingFunctionValue(style->transitions());
        case CSSPropertyTransition:
        case CSSPropertyWebkitTransition: {
            const AnimationList* animList = style->transitions();
            if (animList) {
                RefPtr<CSSValueList> transitionsList = CSSValueList::createCommaSeparated();
                for (size_t i = 0; i < animList->size(); ++i) {
                    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
                    const Animation& animation = animList->animation(i);
                    list->append(createTransitionPropertyValue(animation));
                    list->append(cssValuePool.createValue(animation.duration(), CSSPrimitiveValue::CSS_S));
                    list->append(createTimingFunctionValue(animation.timingFunction().get()));
                    list->append(cssValuePool.createValue(animation.delay(), CSSPrimitiveValue::CSS_S));
                    transitionsList->append(list.releaseNonNull());
                }
                return transitionsList;
            }

            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            // transition-property default value.
            list->append(cssValuePool.createIdentifierValue(CSSValueAll));
            list->append(cssValuePool.createValue(Animation::initialDuration(), CSSPrimitiveValue::CSS_S));
            list->append(createTimingFunctionValue(Animation::initialTimingFunction().get()));
            list->append(cssValuePool.createValue(Animation::initialDelay(), CSSPrimitiveValue::CSS_S));
            return list;
        }
        case CSSPropertyPointerEvents:
            return cssValuePool.createValue(style->pointerEvents());
        case CSSPropertyWebkitLineGrid:
            if (style->lineGrid().isNull())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return cssValuePool.createValue(style->lineGrid(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitLineSnap:
            return CSSPrimitiveValue::create(style->lineSnap());
        case CSSPropertyWebkitLineAlign:
            return CSSPrimitiveValue::create(style->lineAlign());
        case CSSPropertyWebkitWritingMode:
            return cssValuePool.createValue(style->writingMode());
        case CSSPropertyWebkitTextCombine:
            return cssValuePool.createValue(style->textCombine());
        case CSSPropertyWebkitTextOrientation:
            return CSSPrimitiveValue::create(style->textOrientation());
        case CSSPropertyWebkitLineBoxContain:
            return createLineBoxContainValue(style->lineBoxContain());
        case CSSPropertyAlt:
            return altTextToCSSValue(style.get());
        case CSSPropertyContent:
            return contentToCSSValue(style.get());
        case CSSPropertyCounterIncrement:
            return counterToCSSValue(style.get(), propertyID);
        case CSSPropertyCounterReset:
            return counterToCSSValue(style.get(), propertyID);
        case CSSPropertyWebkitClipPath: {
            ClipPathOperation* operation = style->clipPath();
            if (!operation)
                return cssValuePool.createIdentifierValue(CSSValueNone);
            if (is<ReferenceClipPathOperation>(*operation)) {
                const auto& referenceOperation = downcast<ReferenceClipPathOperation>(*operation);
                return CSSPrimitiveValue::create(referenceOperation.url(), CSSPrimitiveValue::CSS_URI);
            }
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (is<ShapeClipPathOperation>(*operation)) {
                const auto& shapeOperation = downcast<ShapeClipPathOperation>(*operation);
                list->append(valueForBasicShape(*style, shapeOperation.basicShape()));
                if (shapeOperation.referenceBox() != BoxMissing)
                    list->append(cssValuePool.createValue(shapeOperation.referenceBox()));
            }
            if (is<BoxClipPathOperation>(*operation)) {
                const auto& boxOperation = downcast<BoxClipPathOperation>(*operation);
                list->append(cssValuePool.createValue(boxOperation.referenceBox()));
            }
            return list;
        }
#if ENABLE(CSS_REGIONS)
        case CSSPropertyWebkitFlowInto:
            if (!style->hasFlowInto())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return cssValuePool.createValue(style->flowThread(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitFlowFrom:
            if (!style->hasFlowFrom())
                return cssValuePool.createIdentifierValue(CSSValueNone);
            return cssValuePool.createValue(style->regionThread(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitRegionFragment:
            return cssValuePool.createValue(style->regionFragment());
#endif
#if ENABLE(CSS_SHAPES)
        case CSSPropertyWebkitShapeMargin:
            return cssValuePool.createValue(style->shapeMargin(), *style);
        case CSSPropertyWebkitShapeImageThreshold:
            return cssValuePool.createValue(style->shapeImageThreshold(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitShapeOutside:
            return shapePropertyValue(*style, style->shapeOutside());
#endif
        case CSSPropertyFilter:
            return valueForFilter(*style, style->filter());
#if ENABLE(FILTERS_LEVEL_2)
        case CSSPropertyWebkitBackdropFilter:
            return valueForFilter(*style, style->backdropFilter());
#endif
#if ENABLE(CSS_COMPOSITING)
        case CSSPropertyMixBlendMode:
            return cssValuePool.createValue(style->blendMode());
        case CSSPropertyIsolation:
            return cssValuePool.createValue(style->isolation());
#endif
        case CSSPropertyBackgroundBlendMode: {
            const FillLayer* layers = style->backgroundLayers();
            if (!layers->next())
                return cssValuePool.createValue(layers->blendMode());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool.createValue(currLayer->blendMode()));

            return list.release();
        }
        case CSSPropertyBackground:
            return getBackgroundShorthandValue();
        case CSSPropertyBorder: {
            RefPtr<CSSValue> value = propertyValue(CSSPropertyBorderTop, DoNotUpdateLayout);
            const CSSPropertyID properties[3] = { CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
            for (auto& property : properties) {
                if (!compareCSSValuePtr<CSSValue>(value, propertyValue(property, DoNotUpdateLayout)))
                    return nullptr;
            }
            return value;
        }
        case CSSPropertyBorderBottom:
            return getCSSPropertyValuesForShorthandProperties(borderBottomShorthand());
        case CSSPropertyBorderColor:
            return getCSSPropertyValuesForSidesShorthand(borderColorShorthand());
        case CSSPropertyBorderLeft:
            return getCSSPropertyValuesForShorthandProperties(borderLeftShorthand());
        case CSSPropertyBorderImage:
            return valueForNinePieceImage(style->borderImage());
        case CSSPropertyBorderRadius:
            return getBorderRadiusShorthandValue(*style);
        case CSSPropertyBorderRight:
            return getCSSPropertyValuesForShorthandProperties(borderRightShorthand());
        case CSSPropertyBorderStyle:
            return getCSSPropertyValuesForSidesShorthand(borderStyleShorthand());
        case CSSPropertyBorderTop:
            return getCSSPropertyValuesForShorthandProperties(borderTopShorthand());
        case CSSPropertyBorderWidth:
            return getCSSPropertyValuesForSidesShorthand(borderWidthShorthand());
        case CSSPropertyColumnRule:
            return getCSSPropertyValuesForShorthandProperties(columnRuleShorthand());
        case CSSPropertyColumns:
            return getCSSPropertyValuesForShorthandProperties(columnsShorthand());
        case CSSPropertyListStyle:
            return getCSSPropertyValuesForShorthandProperties(listStyleShorthand());
        case CSSPropertyMargin:
            return getCSSPropertyValuesForSidesShorthand(marginShorthand());
        case CSSPropertyOutline:
            return getCSSPropertyValuesForShorthandProperties(outlineShorthand());
        case CSSPropertyPadding:
            return getCSSPropertyValuesForSidesShorthand(paddingShorthand());

#if ENABLE(CSS_SCROLL_SNAP)
        case CSSPropertyWebkitScrollSnapType:
            return cssValuePool.createValue(style->scrollSnapType());
        case CSSPropertyWebkitScrollSnapDestination:
            return scrollSnapDestination(*style, style->scrollSnapDestination());
        case CSSPropertyWebkitScrollSnapPointsX:
            return scrollSnapPoints(*style, style->scrollSnapPointsX());
        case CSSPropertyWebkitScrollSnapPointsY:
            return scrollSnapPoints(*style, style->scrollSnapPointsY());
        case CSSPropertyWebkitScrollSnapCoordinate:
            return scrollSnapCoordinates(*style, style->scrollSnapCoordinates());
#endif

#if ENABLE(CSS_TRAILING_WORD)
        case CSSPropertyAppleTrailingWord:
            return cssValuePool.createValue(style->trailingWord());
#endif

        /* Individual properties not part of the spec */
        case CSSPropertyBackgroundRepeatX:
        case CSSPropertyBackgroundRepeatY:
            break;

        // Length properties for SVG.
        case CSSPropertyCx:
            return zoomAdjustedPixelValueForLength(style->svgStyle().cx(), *style);
        case CSSPropertyCy:
            return zoomAdjustedPixelValueForLength(style->svgStyle().cy(), *style);
        case CSSPropertyR:
            return zoomAdjustedPixelValueForLength(style->svgStyle().r(), *style);
        case CSSPropertyRx:
            return zoomAdjustedPixelValueForLength(style->svgStyle().rx(), *style);
        case CSSPropertyRy:
            return zoomAdjustedPixelValueForLength(style->svgStyle().ry(), *style);
        case CSSPropertyStrokeWidth:
            return zoomAdjustedPixelValueForLength(style->svgStyle().strokeWidth(), *style);
        case CSSPropertyStrokeDashoffset:
            return zoomAdjustedPixelValueForLength(style->svgStyle().strokeDashOffset(), *style);
        case CSSPropertyX:
            return zoomAdjustedPixelValueForLength(style->svgStyle().x(), *style);
        case CSSPropertyY:
            return zoomAdjustedPixelValueForLength(style->svgStyle().y(), *style);
        case CSSPropertyWebkitTextZoom:
            return cssValuePool.createValue(style->textZoom());

        /* Unimplemented CSS 3 properties (including CSS3 shorthand properties) */
        case CSSPropertyAll:
        case CSSPropertyAnimation:
        case CSSPropertyWebkitTextEmphasis:
        case CSSPropertyTextLineThrough:
        case CSSPropertyTextLineThroughColor:
        case CSSPropertyTextLineThroughMode:
        case CSSPropertyTextLineThroughStyle:
        case CSSPropertyTextLineThroughWidth:
        case CSSPropertyTextOverline:
        case CSSPropertyTextOverlineColor:
        case CSSPropertyTextOverlineMode:
        case CSSPropertyTextOverlineStyle:
        case CSSPropertyTextOverlineWidth:
        case CSSPropertyTextUnderline:
        case CSSPropertyTextUnderlineColor:
        case CSSPropertyTextUnderlineMode:
        case CSSPropertyTextUnderlineStyle:
        case CSSPropertyTextUnderlineWidth:
            break;

        /* Directional properties are resolved by resolveDirectionAwareProperty() before the switch. */
        case CSSPropertyWebkitBorderEnd:
        case CSSPropertyWebkitBorderEndColor:
        case CSSPropertyWebkitBorderEndStyle:
        case CSSPropertyWebkitBorderEndWidth:
        case CSSPropertyWebkitBorderStart:
        case CSSPropertyWebkitBorderStartColor:
        case CSSPropertyWebkitBorderStartStyle:
        case CSSPropertyWebkitBorderStartWidth:
        case CSSPropertyWebkitBorderAfter:
        case CSSPropertyWebkitBorderAfterColor:
        case CSSPropertyWebkitBorderAfterStyle:
        case CSSPropertyWebkitBorderAfterWidth:
        case CSSPropertyWebkitBorderBefore:
        case CSSPropertyWebkitBorderBeforeColor:
        case CSSPropertyWebkitBorderBeforeStyle:
        case CSSPropertyWebkitBorderBeforeWidth:
        case CSSPropertyWebkitMarginEnd:
        case CSSPropertyWebkitMarginStart:
        case CSSPropertyWebkitMarginAfter:
        case CSSPropertyWebkitMarginBefore:
        case CSSPropertyWebkitPaddingEnd:
        case CSSPropertyWebkitPaddingStart:
        case CSSPropertyWebkitPaddingAfter:
        case CSSPropertyWebkitPaddingBefore:
        case CSSPropertyWebkitLogicalWidth:
        case CSSPropertyWebkitLogicalHeight:
        case CSSPropertyWebkitMinLogicalWidth:
        case CSSPropertyWebkitMinLogicalHeight:
        case CSSPropertyWebkitMaxLogicalWidth:
        case CSSPropertyWebkitMaxLogicalHeight:
            ASSERT_NOT_REACHED();
            break;

        /* Unimplemented @font-face properties */
        case CSSPropertyFontStretch:
        case CSSPropertySrc:
        case CSSPropertyUnicodeRange:
            break;

        /* Other unimplemented properties */
        case CSSPropertyPage: // for @page
        case CSSPropertyQuotes: // FIXME: needs implementation
        case CSSPropertySize: // for @page
            break;

        /* Unimplemented -webkit- properties */
        case CSSPropertyWebkitAnimation:
        case CSSPropertyWebkitBorderRadius:
        case CSSPropertyWebkitMarginCollapse:
        case CSSPropertyWebkitMarquee:
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

#if ENABLE(CSS_DEVICE_ADAPTATION)
        case CSSPropertyMaxZoom:
        case CSSPropertyMinZoom:
        case CSSPropertyOrientation:
        case CSSPropertyUserZoom:
            break;
#endif

        case CSSPropertyBufferedRendering:
        case CSSPropertyClipPath:
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
        case CSSPropertyPaintOrder:
        case CSSPropertyShapeRendering:
        case CSSPropertyStroke:
        case CSSPropertyStrokeDasharray:
        case CSSPropertyStrokeLinecap:
        case CSSPropertyStrokeLinejoin:
        case CSSPropertyStrokeMiterlimit:
        case CSSPropertyStrokeOpacity:
        case CSSPropertyAlignmentBaseline:
        case CSSPropertyBaselineShift:
        case CSSPropertyDominantBaseline:
        case CSSPropertyGlyphOrientationHorizontal:
        case CSSPropertyGlyphOrientationVertical:
        case CSSPropertyKerning:
        case CSSPropertyTextAnchor:
        case CSSPropertyVectorEffect:
        case CSSPropertyWritingMode:
        case CSSPropertyWebkitSvgShadow:
            return svgPropertyValue(propertyID, DoNotUpdateLayout);
        case CSSPropertyCustom:
            ASSERT_NOT_REACHED();
            return nullptr;
    }

    logUnimplementedPropertyID(propertyID);
    return nullptr;
}

String CSSComputedStyleDeclaration::getPropertyValue(CSSPropertyID propertyID) const
{
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    if (!value)
        return emptyString(); // FIXME: Should this be null instead, as it is in StyleProperties::getPropertyValue?
    return value->cssText();
}

unsigned CSSComputedStyleDeclaration::length() const
{
    Node* node = m_node.get();
    if (!node)
        return 0;

    RenderStyle* style = node->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return 0;

    return numComputedProperties + style->customProperties().size();
}

String CSSComputedStyleDeclaration::item(unsigned i) const
{
    if (i >= length())
        return emptyString();
    
    if (i < numComputedProperties)
        return getPropertyNameString(computedProperties[i]);
    
    Node* node = m_node.get();
    if (!node)
        return emptyString();

    RenderStyle* style = node->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return emptyString();
    
    unsigned index = i - numComputedProperties;
    
    const auto& customProperties = style->customProperties();
    if (index >= customProperties.size())
        return emptyString();
    
    Vector<String, 4> results;
    copyKeysToVector(customProperties, results);
    return results.at(index);
}

bool ComputedStyleExtractor::propertyMatches(CSSPropertyID propertyID, const CSSValue* value) const
{
    if (propertyID == CSSPropertyFontSize && is<CSSPrimitiveValue>(*value) && m_node) {
        m_node->document().updateLayoutIgnorePendingStylesheets();
        if (RenderStyle* style = m_node->computedStyle(m_pseudoElementSpecifier)) {
            if (CSSValueID sizeIdentifier = style->fontDescription().keywordSizeAsIdentifier()) {
                auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
                if (primitiveValue.isValueID() && primitiveValue.getValueID() == sizeIdentifier)
                    return true;
            }
        }
    }
    RefPtr<CSSValue> computedValue = propertyValue(propertyID);
    return computedValue && value && computedValue->equals(*value);
}

Ref<MutableStyleProperties> ComputedStyleExtractor::copyProperties() const
{
    return copyPropertiesInSet(computedProperties, numComputedProperties);
}

RefPtr<CSSValueList> ComputedStyleExtractor::getCSSPropertyValuesForShorthandProperties(const StylePropertyShorthand& shorthand) const
{
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (size_t i = 0; i < shorthand.length(); ++i) {
        RefPtr<CSSValue> value = propertyValue(shorthand.properties()[i], DoNotUpdateLayout);
        list->append(value.releaseNonNull());
    }
    return list;
}

RefPtr<CSSValueList> ComputedStyleExtractor::getCSSPropertyValuesForSidesShorthand(const StylePropertyShorthand& shorthand) const
{
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    // Assume the properties are in the usual order top, right, bottom, left.
    RefPtr<CSSValue> topValue = propertyValue(shorthand.properties()[0], DoNotUpdateLayout);
    RefPtr<CSSValue> rightValue = propertyValue(shorthand.properties()[1], DoNotUpdateLayout);
    RefPtr<CSSValue> bottomValue = propertyValue(shorthand.properties()[2], DoNotUpdateLayout);
    RefPtr<CSSValue> leftValue = propertyValue(shorthand.properties()[3], DoNotUpdateLayout);

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

RefPtr<CSSValueList> ComputedStyleExtractor::getCSSPropertyValuesForGridShorthand(const StylePropertyShorthand& shorthand) const
{
    RefPtr<CSSValueList> list = CSSValueList::createSlashSeparated();
    for (size_t i = 0; i < shorthand.length(); ++i) {
        RefPtr<CSSValue> value = propertyValue(shorthand.properties()[i], DoNotUpdateLayout);
        list->append(value.releaseNonNull());
    }
    return list;
}

Ref<MutableStyleProperties> ComputedStyleExtractor::copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const
{
    Vector<CSSProperty, 256> list;
    list.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        RefPtr<CSSValue> value = propertyValue(set[i]);
        if (value)
            list.append(CSSProperty(set[i], value.release(), false));
    }
    return MutableStyleProperties::create(list.data(), list.size());
}

CSSRule* CSSComputedStyleDeclaration::parentRule() const
{
    return nullptr;
}

RefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(const String& propertyName)
{
    if (isCustomPropertyName(propertyName))
        return ComputedStyleExtractor(m_node, m_allowVisitedStyle, m_pseudoElementSpecifier).customPropertyValue(propertyName);

    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!propertyID)
        return nullptr;
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    return value ? value->cloneForCSSOM() : nullptr;
}

String CSSComputedStyleDeclaration::getPropertyValue(const String &propertyName)
{
    if (isCustomPropertyName(propertyName))
        return ComputedStyleExtractor(m_node, m_allowVisitedStyle, m_pseudoElementSpecifier).customPropertyText(propertyName);

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

void CSSComputedStyleDeclaration::setProperty(const String&, const String&, const String&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

String CSSComputedStyleDeclaration::removeProperty(const String&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
    return String();
}
    
RefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValueInternal(CSSPropertyID propertyID)
{
    return getPropertyCSSValue(propertyID);
}

String CSSComputedStyleDeclaration::getPropertyValueInternal(CSSPropertyID propertyID)
{
    return getPropertyValue(propertyID);
}

bool CSSComputedStyleDeclaration::setPropertyInternal(CSSPropertyID, const String&, bool, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
    return false;
}

RefPtr<CSSValueList> ComputedStyleExtractor::getBackgroundShorthandValue() const
{
    static const CSSPropertyID propertiesBeforeSlashSeperator[5] = { CSSPropertyBackgroundColor, CSSPropertyBackgroundImage,
                                                                     CSSPropertyBackgroundRepeat, CSSPropertyBackgroundAttachment,  
                                                                     CSSPropertyBackgroundPosition };
    static const CSSPropertyID propertiesAfterSlashSeperator[3] = { CSSPropertyBackgroundSize, CSSPropertyBackgroundOrigin, 
                                                                    CSSPropertyBackgroundClip };

    RefPtr<CSSValueList> list = CSSValueList::createSlashSeparated();
    list->append(*getCSSPropertyValuesForShorthandProperties(StylePropertyShorthand(CSSPropertyBackground, propertiesBeforeSlashSeperator)));
    list->append(*getCSSPropertyValuesForShorthandProperties(StylePropertyShorthand(CSSPropertyBackground, propertiesAfterSlashSeperator)));
    return list;
}

} // namespace WebCore
