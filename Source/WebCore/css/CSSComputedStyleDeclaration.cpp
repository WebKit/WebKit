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
#include "CSSAspectRatioValue.h"
#include "CSSBasicShapes.h"
#include "CSSBorderImage.h"
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

namespace WebCore {

// List of all properties we know how to compute, omitting shorthands.
static const CSSPropertyID computedProperties[] = {
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
    CSSPropertyCursor,
    CSSPropertyDirection,
    CSSPropertyDisplay,
    CSSPropertyEmptyCells,
    CSSPropertyFloat,
    CSSPropertyFontFamily,
    CSSPropertyFontSize,
    CSSPropertyFontStyle,
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

    CSSPropertyWebkitAlt,
    CSSPropertyWebkitAnimationDelay,
    CSSPropertyWebkitAnimationDirection,
    CSSPropertyWebkitAnimationDuration,
    CSSPropertyWebkitAnimationFillMode,
    CSSPropertyWebkitAnimationIterationCount,
    CSSPropertyWebkitAnimationName,
    CSSPropertyWebkitAnimationPlayState,
    CSSPropertyWebkitAnimationTimingFunction,
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
    CSSPropertyWebkitColorCorrection,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyWebkitColumnAxis,
    CSSPropertyWebkitColumnCount,
    CSSPropertyWebkitColumnGap,
    CSSPropertyWebkitColumnProgression,
    CSSPropertyWebkitColumnRuleColor,
    CSSPropertyWebkitColumnRuleStyle,
    CSSPropertyWebkitColumnRuleWidth,
    CSSPropertyWebkitColumnSpan,
    CSSPropertyWebkitColumnWidth,
#if ENABLE(CURSOR_VISIBILITY)
    CSSPropertyWebkitCursorVisibility,
#endif
#if ENABLE(DASHBOARD_SUPPORT)
    CSSPropertyWebkitDashboardRegion,
#endif
    CSSPropertyAlignContent,
    CSSPropertyAlignItems,
    CSSPropertyAlignSelf,
    CSSPropertyFlexBasis,
    CSSPropertyFlexGrow,
    CSSPropertyFlexShrink,
    CSSPropertyFlexDirection,
    CSSPropertyFlexWrap,
    CSSPropertyJustifyContent,
    CSSPropertyWebkitJustifySelf,
    CSSPropertyWebkitFilter,
    CSSPropertyWebkitFontKerning,
    CSSPropertyWebkitFontSmoothing,
    CSSPropertyWebkitFontVariantLigatures,
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
    CSSPropertyWebkitPerspective,
    CSSPropertyWebkitPerspectiveOrigin,
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
    CSSPropertyWebkitTransform,
    CSSPropertyWebkitTransformOrigin,
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

static PassRef<CSSPrimitiveValue> valueForImageSliceSide(const Length& length)
{
    // These values can be percentages, numbers, or while an animation of mixed types is in progress,
    // a calculation that combines a percentage and a number.
    if (length.isPercentNotCalculated())
        return cssValuePool().createValue(length.percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
    if (length.isFixed())
        return cssValuePool().createValue(length.value(), CSSPrimitiveValue::CSS_NUMBER);

    // Calculating the actual length currently in use would require most of the code from RenderBoxModelObject::paintNinePieceImage.
    // And even if we could do that, it's not clear if that's exactly what we'd want during animation.
    // FIXME: For now, just return 0.
    ASSERT(length.isCalculated());
    return cssValuePool().createValue(0, CSSPrimitiveValue::CSS_NUMBER);
}

static PassRef<CSSBorderImageSliceValue> valueForNinePieceImageSlice(const NinePieceImage& image)
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

    RefPtr<Quad> quad = Quad::create();
    quad->setTop(top.release());
    quad->setRight(right.release());
    quad->setBottom(bottom.release());
    quad->setLeft(left.release());

    return CSSBorderImageSliceValue::create(cssValuePool().createValue(quad.release()), image.fill());
}

static PassRef<CSSPrimitiveValue> valueForNinePieceImageQuad(const LengthBox& box)
{
    RefPtr<CSSPrimitiveValue> top;
    RefPtr<CSSPrimitiveValue> right;
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;

    if (box.top().isRelative())
        top = cssValuePool().createValue(box.top().value(), CSSPrimitiveValue::CSS_NUMBER);
    else
        top = cssValuePool().createValue(box.top());

    if (box.right() == box.top() && box.bottom() == box.top() && box.left() == box.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (box.right().isRelative())
            right = cssValuePool().createValue(box.right().value(), CSSPrimitiveValue::CSS_NUMBER);
        else
            right = cssValuePool().createValue(box.right());

        if (box.bottom() == box.top() && box.right() == box.left()) {
            bottom = top;
            left = right;
        } else {
            if (box.bottom().isRelative())
                bottom = cssValuePool().createValue(box.bottom().value(), CSSPrimitiveValue::CSS_NUMBER);
            else
                bottom = cssValuePool().createValue(box.bottom());

            if (box.left() == box.right())
                left = right;
            else {
                if (box.left().isRelative())
                    left = cssValuePool().createValue(box.left().value(), CSSPrimitiveValue::CSS_NUMBER);
                else
                    left = cssValuePool().createValue(box.left());
            }
        }
    }

    RefPtr<Quad> quad = Quad::create();
    quad->setTop(top);
    quad->setRight(right);
    quad->setBottom(bottom);
    quad->setLeft(left);

    return cssValuePool().createValue(quad.release());
}

static PassRef<CSSValue> valueForNinePieceImageRepeat(const NinePieceImage& image)
{
    RefPtr<CSSPrimitiveValue> horizontalRepeat;
    RefPtr<CSSPrimitiveValue> verticalRepeat;

    horizontalRepeat = cssValuePool().createIdentifierValue(valueForRepeatRule(image.horizontalRule()));
    if (image.horizontalRule() == image.verticalRule())
        verticalRepeat = horizontalRepeat;
    else
        verticalRepeat = cssValuePool().createIdentifierValue(valueForRepeatRule(image.verticalRule()));
    return cssValuePool().createValue(Pair::create(horizontalRepeat.release(), verticalRepeat.release()));
}

static PassRef<CSSValue> valueForNinePieceImage(const NinePieceImage& image)
{
    if (!image.hasImage())
        return cssValuePool().createIdentifierValue(CSSValueNone);

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

inline static PassRef<CSSPrimitiveValue> zoomAdjustedPixelValue(double value, const RenderStyle* style)
{
    return cssValuePool().createValue(adjustFloatForAbsoluteZoom(value, style), CSSPrimitiveValue::CSS_PX);
}

inline static PassRef<CSSPrimitiveValue> zoomAdjustedNumberValue(double value, const RenderStyle* style)
{
    return cssValuePool().createValue(value / style->effectiveZoom(), CSSPrimitiveValue::CSS_NUMBER);
}

static PassRef<CSSValue> zoomAdjustedPixelValueForLength(const Length& length, const RenderStyle* style)
{
    if (length.isFixed())
        return zoomAdjustedPixelValue(length.value(), style);
    return cssValuePool().createValue(length, style);
}

static PassRef<CSSValue> valueForReflection(const StyleReflection* reflection, const RenderStyle* style)
{
    if (!reflection)
        return cssValuePool().createIdentifierValue(CSSValueNone);

    RefPtr<CSSPrimitiveValue> offset;
    if (reflection->offset().isPercent())
        offset = cssValuePool().createValue(reflection->offset().percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        offset = zoomAdjustedPixelValue(reflection->offset().value(), style);

    RefPtr<CSSPrimitiveValue> direction;
    switch (reflection->direction()) {
    case ReflectionBelow:
        direction = cssValuePool().createIdentifierValue(CSSValueBelow);
        break;
    case ReflectionAbove:
        direction = cssValuePool().createIdentifierValue(CSSValueAbove);
        break;
    case ReflectionLeft:
        direction = cssValuePool().createIdentifierValue(CSSValueLeft);
        break;
    case ReflectionRight:
        direction = cssValuePool().createIdentifierValue(CSSValueRight);
        break;
    }

    return CSSReflectValue::create(direction.release(), offset.release(), valueForNinePieceImage(reflection->mask()));
}

static PassRef<CSSValueList> createPositionListForLayer(CSSPropertyID propertyID, const FillLayer* layer, const RenderStyle* style)
{
    auto positionList = CSSValueList::createSpaceSeparated();
    if (layer->isBackgroundOriginSet()) {
        ASSERT_UNUSED(propertyID, propertyID == CSSPropertyBackgroundPosition || propertyID == CSSPropertyWebkitMaskPosition);
        positionList.get().append(cssValuePool().createValue(layer->backgroundXOrigin()));
    }
    positionList.get().append(zoomAdjustedPixelValueForLength(layer->xPosition(), style));
    if (layer->isBackgroundOriginSet()) {
        ASSERT(propertyID == CSSPropertyBackgroundPosition || propertyID == CSSPropertyWebkitMaskPosition);
        positionList.get().append(cssValuePool().createValue(layer->backgroundYOrigin()));
    }
    positionList.get().append(zoomAdjustedPixelValueForLength(layer->yPosition(), style));
    return positionList;
}

static PassRefPtr<CSSValue> positionOffsetValue(RenderStyle* style, CSSPropertyID propertyID)
{
    if (!style)
        return nullptr;

    Length length;
    switch (propertyID) {
        case CSSPropertyLeft:
            length = style->left();
            break;
        case CSSPropertyRight:
            length = style->right();
            break;
        case CSSPropertyTop:
            length = style->top();
            break;
        case CSSPropertyBottom:
            length = style->bottom();
            break;
        default:
            return nullptr;
    }

    if (style->hasOutOfFlowPosition()) {
        if (length.isFixed())
            return zoomAdjustedPixelValue(length.value(), style);

        return cssValuePool().createValue(length);
    }

    if (style->hasInFlowPosition()) {
        // FIXME: It's not enough to simply return "auto" values for one offset if the other side is defined.
        // In other words if left is auto and right is not auto, then left's computed value is negative right().
        // So we should get the opposite length unit and see if it is auto.
        return cssValuePool().createValue(length);
    }

    return cssValuePool().createIdentifierValue(CSSValueAuto);
}

PassRefPtr<CSSPrimitiveValue> ComputedStyleExtractor::currentColorOrValidColor(RenderStyle* style, const Color& color) const
{
    // This function does NOT look at visited information, so that computed style doesn't expose that.
    if (!color.isValid())
        return cssValuePool().createColorValue(style->color().rgb());
    return cssValuePool().createColorValue(color.rgb());
}

static PassRef<CSSPrimitiveValue> percentageOrZoomAdjustedValue(Length length, const RenderStyle* style)
{
    if (length.isPercentNotCalculated())
        return cssValuePool().createValue(length.percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
    
    return zoomAdjustedPixelValue(valueForLength(length, 0), style);
}

static PassRef<CSSPrimitiveValue> autoOrZoomAdjustedValue(Length length, const RenderStyle* style)
{
    if (length.isAuto())
        return cssValuePool().createIdentifierValue(CSSValueAuto);

    return zoomAdjustedPixelValue(valueForLength(length, 0), style);
}

static PassRef<CSSValueList> getBorderRadiusCornerValues(const LengthSize& radius, const RenderStyle* style)
{
    auto list = CSSValueList::createSpaceSeparated();
    list.get().append(percentageOrZoomAdjustedValue(radius.width(), style));
    list.get().append(percentageOrZoomAdjustedValue(radius.height(), style));
    return list;
}

static PassRef<CSSValue> getBorderRadiusCornerValue(const LengthSize& radius, const RenderStyle* style)
{
    if (radius.width() == radius.height())
        return percentageOrZoomAdjustedValue(radius.width(), style);

    return getBorderRadiusCornerValues(radius, style);
}

static PassRef<CSSValueList> getBorderRadiusShorthandValue(const RenderStyle* style)
{
    auto list = CSSValueList::createSlashSeparated();
    bool showHorizontalBottomLeft = style->borderTopRightRadius().width() != style->borderBottomLeftRadius().width();
    bool showHorizontalBottomRight = showHorizontalBottomLeft || (style->borderBottomRightRadius().width() != style->borderTopLeftRadius().width());
    bool showHorizontalTopRight = showHorizontalBottomRight || (style->borderTopRightRadius().width() != style->borderTopLeftRadius().width());

    bool showVerticalBottomLeft = style->borderTopRightRadius().height() != style->borderBottomLeftRadius().height();
    bool showVerticalBottomRight = showVerticalBottomLeft || (style->borderBottomRightRadius().height() != style->borderTopLeftRadius().height());
    bool showVerticalTopRight = showVerticalBottomRight || (style->borderTopRightRadius().height() != style->borderTopLeftRadius().height());

    RefPtr<CSSValueList> topLeftRadius = getBorderRadiusCornerValues(style->borderTopLeftRadius(), style);
    RefPtr<CSSValueList> topRightRadius = getBorderRadiusCornerValues(style->borderTopRightRadius(), style);
    RefPtr<CSSValueList> bottomRightRadius = getBorderRadiusCornerValues(style->borderBottomRightRadius(), style);
    RefPtr<CSSValueList> bottomLeftRadius = getBorderRadiusCornerValues(style->borderBottomLeftRadius(), style);

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

    if (!verticalRadiiList->equals(*toCSSValueList(list.get().item(0))))
        list.get().append(verticalRadiiList.releaseNonNull());

    return list;
}

static LayoutRect sizingBox(RenderObject* renderer)
{
    if (!renderer->isBox())
        return LayoutRect();

    RenderBox* box = toRenderBox(renderer);
    return box->style().boxSizing() == BORDER_BOX ? box->borderBoxRect() : box->computedCSSContentBoxRect();
}

static PassRef<WebKitCSSTransformValue> matrixTransformValue(const TransformationMatrix& transform, const RenderStyle* style)
{
    RefPtr<WebKitCSSTransformValue> transformValue;
    if (transform.isAffine()) {
        transformValue = WebKitCSSTransformValue::create(WebKitCSSTransformValue::MatrixTransformOperation);

        transformValue->append(cssValuePool().createValue(transform.a(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.b(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.c(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.d(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(zoomAdjustedNumberValue(transform.e(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.f(), style));
    } else {
        transformValue = WebKitCSSTransformValue::create(WebKitCSSTransformValue::Matrix3DTransformOperation);

        transformValue->append(cssValuePool().createValue(transform.m11(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m12(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m13(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m14(), CSSPrimitiveValue::CSS_NUMBER));

        transformValue->append(cssValuePool().createValue(transform.m21(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m22(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m23(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m24(), CSSPrimitiveValue::CSS_NUMBER));

        transformValue->append(cssValuePool().createValue(transform.m31(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m32(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m33(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m34(), CSSPrimitiveValue::CSS_NUMBER));

        transformValue->append(zoomAdjustedNumberValue(transform.m41(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.m42(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.m43(), style));
        transformValue->append(cssValuePool().createValue(transform.m44(), CSSPrimitiveValue::CSS_NUMBER));
    }

    return transformValue.releaseNonNull();
}

static PassRef<CSSValue> computedTransform(RenderObject* renderer, const RenderStyle* style)
{
    if (!renderer || !renderer->hasTransform() || !style->hasTransform())
        return cssValuePool().createIdentifierValue(CSSValueNone);

    FloatRect pixelSnappedRect;
    if (renderer->isBox())
        pixelSnappedRect = snapRectToDevicePixels(toRenderBox(renderer)->borderBoxRect(), renderer->document().deviceScaleFactor());

    TransformationMatrix transform;
    style->applyTransform(transform, pixelSnappedRect, RenderStyle::ExcludeTransformOrigin);
    // Note that this does not flatten to an affine transform if ENABLE(3D_RENDERING) is off, by design.

    // FIXME: Need to print out individual functions (https://bugs.webkit.org/show_bug.cgi?id=23924)
    auto list = CSSValueList::createSpaceSeparated();
    list.get().append(matrixTransformValue(transform, style));
    return WTF::move(list);
}

static inline PassRef<CSSPrimitiveValue> adjustLengthForZoom(double length, const RenderStyle* style, AdjustPixelValuesForComputedStyle adjust)
{
    return adjust == AdjustPixelValues ? zoomAdjustedPixelValue(length, style) : cssValuePool().createValue(length, CSSPrimitiveValue::CSS_PX);
}

static inline PassRef<CSSPrimitiveValue> adjustLengthForZoom(const Length& length, const RenderStyle* style, AdjustPixelValuesForComputedStyle adjust)
{
    return adjust == AdjustPixelValues ? zoomAdjustedPixelValue(length.value(), style) : cssValuePool().createValue(length);
}

PassRef<CSSValue> ComputedStyleExtractor::valueForShadow(const ShadowData* shadow, CSSPropertyID propertyID, const RenderStyle* style, AdjustPixelValuesForComputedStyle adjust)
{
    if (!shadow)
        return cssValuePool().createIdentifierValue(CSSValueNone);

    auto list = CSSValueList::createCommaSeparated();
    for (const ShadowData* currShadowData = shadow; currShadowData; currShadowData = currShadowData->next()) {
        RefPtr<CSSPrimitiveValue> x = adjustLengthForZoom(currShadowData->x(), style, adjust);
        RefPtr<CSSPrimitiveValue> y = adjustLengthForZoom(currShadowData->y(), style, adjust);
        RefPtr<CSSPrimitiveValue> blur = adjustLengthForZoom(currShadowData->radius(), style, adjust);
        RefPtr<CSSPrimitiveValue> spread = propertyID == CSSPropertyTextShadow ? PassRefPtr<CSSPrimitiveValue>() : adjustLengthForZoom(currShadowData->spread(), style, adjust);
        RefPtr<CSSPrimitiveValue> style = propertyID == CSSPropertyTextShadow || currShadowData->style() == Normal ? PassRefPtr<CSSPrimitiveValue>() : cssValuePool().createIdentifierValue(CSSValueInset);
        RefPtr<CSSPrimitiveValue> color = cssValuePool().createColorValue(currShadowData->color().rgb());
        list.get().prepend(CSSShadowValue::create(x.release(), y.release(), blur.release(), spread.release(), style.release(), color.release()));
    }
    return WTF::move(list);
}

PassRef<CSSValue> ComputedStyleExtractor::valueForFilter(const RenderStyle* style, const FilterOperations& filterOperations, AdjustPixelValuesForComputedStyle adjust)
{
    if (filterOperations.operations().isEmpty())
        return cssValuePool().createIdentifierValue(CSSValueNone);

    auto list = CSSValueList::createSpaceSeparated();

    RefPtr<WebKitCSSFilterValue> filterValue;

    Vector<RefPtr<FilterOperation>>::const_iterator end = filterOperations.operations().end();
    for (Vector<RefPtr<FilterOperation>>::const_iterator it = filterOperations.operations().begin(); it != end; ++it) {
        FilterOperation* filterOperation = (*it).get();
        switch (filterOperation->type()) {
        case FilterOperation::REFERENCE: {
            ReferenceFilterOperation* referenceOperation = toReferenceFilterOperation(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::ReferenceFilterOperation);
            filterValue->append(cssValuePool().createValue(referenceOperation->url(), CSSPrimitiveValue::CSS_URI));
            break;
        }
        case FilterOperation::GRAYSCALE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = toBasicColorMatrixFilterOperation(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::GrayscaleFilterOperation);
            filterValue->append(cssValuePool().createValue(colorMatrixOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::SEPIA: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = toBasicColorMatrixFilterOperation(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::SepiaFilterOperation);
            filterValue->append(cssValuePool().createValue(colorMatrixOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::SATURATE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = toBasicColorMatrixFilterOperation(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::SaturateFilterOperation);
            filterValue->append(cssValuePool().createValue(colorMatrixOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = toBasicColorMatrixFilterOperation(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::HueRotateFilterOperation);
            filterValue->append(cssValuePool().createValue(colorMatrixOperation->amount(), CSSPrimitiveValue::CSS_DEG));
            break;
        }
        case FilterOperation::INVERT: {
            BasicComponentTransferFilterOperation* componentTransferOperation = toBasicComponentTransferFilterOperation(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::InvertFilterOperation);
            filterValue->append(cssValuePool().createValue(componentTransferOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::OPACITY: {
            BasicComponentTransferFilterOperation* componentTransferOperation = toBasicComponentTransferFilterOperation(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::OpacityFilterOperation);
            filterValue->append(cssValuePool().createValue(componentTransferOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::BRIGHTNESS: {
            BasicComponentTransferFilterOperation* brightnessOperation = toBasicComponentTransferFilterOperation(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::BrightnessFilterOperation);
            filterValue->append(cssValuePool().createValue(brightnessOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::CONTRAST: {
            BasicComponentTransferFilterOperation* contrastOperation = toBasicComponentTransferFilterOperation(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::ContrastFilterOperation);
            filterValue->append(cssValuePool().createValue(contrastOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::BLUR: {
            BlurFilterOperation* blurOperation = toBlurFilterOperation(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::BlurFilterOperation);
            filterValue->append(adjustLengthForZoom(blurOperation->stdDeviation(), style, adjust));
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            DropShadowFilterOperation* dropShadowOperation = toDropShadowFilterOperation(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::DropShadowFilterOperation);
            // We want our computed style to look like that of a text shadow (has neither spread nor inset style).
            ShadowData shadowData = ShadowData(dropShadowOperation->location(), dropShadowOperation->stdDeviation(), 0, Normal, false, dropShadowOperation->color());
            filterValue->append(valueForShadow(&shadowData, CSSPropertyTextShadow, style, adjust));
            break;
        }
        default:
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::UnknownFilterOperation);
            break;
        }
        list.get().append(filterValue.releaseNonNull());
    }

    return WTF::move(list);
}

#if ENABLE(CSS_GRID_LAYOUT)
static PassRef<CSSValue> specifiedValueForGridTrackBreadth(const GridLength& trackBreadth, const RenderStyle* style)
{
    if (!trackBreadth.isLength())
        return cssValuePool().createValue(trackBreadth.flex(), CSSPrimitiveValue::CSS_FR);

    const Length& trackBreadthLength = trackBreadth.length();
    if (trackBreadthLength.isAuto())
        return cssValuePool().createIdentifierValue(CSSValueAuto);
    return zoomAdjustedPixelValueForLength(trackBreadthLength, style);
}

static PassRef<CSSValue> specifiedValueForGridTrackSize(const GridTrackSize& trackSize, const RenderStyle* style)
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

    RefPtr<CSSGridLineNamesValue> lineNames = CSSGridLineNamesValue::create();
    for (auto& name : namedGridLines)
        lineNames->append(cssValuePool().createValue(name, CSSPrimitiveValue::CSS_STRING));
    list.append(lineNames.releaseNonNull());
}

static PassRef<CSSValue> valueForGridTrackList(GridTrackSizingDirection direction, RenderObject* renderer, const RenderStyle* style)
{
    const Vector<GridTrackSize>& trackSizes = direction == ForColumns ? style->gridColumns() : style->gridRows();
    const OrderedNamedGridLinesMap& orderedNamedGridLines = direction == ForColumns ? style->orderedNamedGridColumnLines() : style->orderedNamedGridRowLines();
    bool isRenderGrid = renderer && renderer->isRenderGrid();

    // Handle the 'none' case.
    bool trackListIsEmpty = trackSizes.isEmpty();
    if (isRenderGrid && trackListIsEmpty) {
        // For grids we should consider every listed track, whether implicitly or explicitly created. If we don't have
        // any explicit track and there are no children then there are no implicit tracks. We cannot simply check the
        // number of rows/columns in our internal grid representation because it's always at least 1x1 (see r143331).
        trackListIsEmpty = !toRenderBlock(renderer)->firstChild();
    }

    if (trackListIsEmpty) {
        ASSERT(orderedNamedGridLines.isEmpty());
        return cssValuePool().createIdentifierValue(CSSValueNone);
    }

    auto list = CSSValueList::createSpaceSeparated();
    if (isRenderGrid) {
        const Vector<LayoutUnit>& trackPositions = direction == ForColumns ? toRenderGrid(renderer)->columnPositions() : toRenderGrid(renderer)->rowPositions();
        // There are at least #tracks + 1 grid lines (trackPositions). Apart from that, the grid container can generate implicit grid tracks,
        // so we'll have more trackPositions than trackSizes as the latter only contain the explicit grid.
        ASSERT(trackPositions.size() - 1 >= trackSizes.size());

        for (unsigned i = 0; i < trackPositions.size() - 1; ++i) {
            addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, i, list.get());
            list.get().append(zoomAdjustedPixelValue(trackPositions[i + 1] - trackPositions[i], style));
        }
    } else {
        for (unsigned i = 0; i < trackSizes.size(); ++i) {
            addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, i, list.get());
            list.get().append(specifiedValueForGridTrackSize(trackSizes[i], style));
        }
    }

    // Those are the trailing <ident>* allowed in the syntax.
    addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, trackSizes.size(), list.get());
    return WTF::move(list);
}

static PassRef<CSSValue> valueForGridPosition(const GridPosition& position)
{
    if (position.isAuto())
        return cssValuePool().createIdentifierValue(CSSValueAuto);

    if (position.isNamedGridArea())
        return cssValuePool().createValue(position.namedGridLine(), CSSPrimitiveValue::CSS_STRING);

    auto list = CSSValueList::createSpaceSeparated();
    if (position.isSpan()) {
        list.get().append(cssValuePool().createIdentifierValue(CSSValueSpan));
        list.get().append(cssValuePool().createValue(position.spanPosition(), CSSPrimitiveValue::CSS_NUMBER));
    } else
        list.get().append(cssValuePool().createValue(position.integerPosition(), CSSPrimitiveValue::CSS_NUMBER));

    if (!position.namedGridLine().isNull())
        list.get().append(cssValuePool().createValue(position.namedGridLine(), CSSPrimitiveValue::CSS_STRING));
    return WTF::move(list);
}
#endif

static PassRef<CSSValue> createTransitionPropertyValue(const Animation& animation)
{
    if (animation.animationMode() == Animation::AnimateNone)
        return cssValuePool().createIdentifierValue(CSSValueNone);
    if (animation.animationMode() == Animation::AnimateAll)
        return cssValuePool().createIdentifierValue(CSSValueAll);
    return cssValuePool().createValue(getPropertyNameString(animation.property()), CSSPrimitiveValue::CSS_STRING);
}

static PassRef<CSSValueList> getTransitionPropertyValue(const AnimationList* animList)
{
    auto list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list.get().append(createTransitionPropertyValue(animList->animation(i)));
    } else
        list.get().append(cssValuePool().createIdentifierValue(CSSValueAll));
    return list;
}

#if ENABLE(CSS_SCROLL_SNAP)

static PassRef<CSSValueList> scrollSnapDestination(RenderStyle& style, const LengthSize& destination)
{
    auto list = CSSValueList::createSpaceSeparated();
    list.get().append(percentageOrZoomAdjustedValue(destination.width(), &style));
    list.get().append(percentageOrZoomAdjustedValue(destination.height(), &style));
    return list;
}

static PassRef<CSSValue> scrollSnapPoints(RenderStyle& style, const ScrollSnapPoints& points)
{
    if (points.usesElements)
        return cssValuePool().createIdentifierValue(CSSValueElements);
    auto list = CSSValueList::createSpaceSeparated();
    for (auto& point : points.offsets)
        list.get().append(percentageOrZoomAdjustedValue(point, &style));
    if (points.hasRepeat)
        list.get().append(cssValuePool().createValue(LengthRepeat::create(percentageOrZoomAdjustedValue(points.repeatOffset, &style))));
    return WTF::move(list);
}

static PassRef<CSSValue> scrollSnapCoordinates(RenderStyle& style, const Vector<LengthSize>& coordinates)
{
    if (coordinates.isEmpty())
        return cssValuePool().createIdentifierValue(CSSValueNone);

    auto list = CSSValueList::createCommaSeparated();

    for (auto& coordinate : coordinates) {
        auto pair = CSSValueList::createSpaceSeparated();
        pair.get().append(percentageOrZoomAdjustedValue(coordinate.width(), &style));
        pair.get().append(percentageOrZoomAdjustedValue(coordinate.height(), &style));
        list.get().append(WTF::move(pair));
    }

    return WTF::move(list);
}

#endif

static PassRef<CSSValueList> getDelayValue(const AnimationList* animList)
{
    auto list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list.get().append(cssValuePool().createValue(animList->animation(i).delay(), CSSPrimitiveValue::CSS_S));
    } else {
        // Note that initialAnimationDelay() is used for both transitions and animations
        list.get().append(cssValuePool().createValue(Animation::initialAnimationDelay(), CSSPrimitiveValue::CSS_S));
    }
    return list;
}

static PassRef<CSSValueList> getDurationValue(const AnimationList* animList)
{
    auto list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list.get().append(cssValuePool().createValue(animList->animation(i).duration(), CSSPrimitiveValue::CSS_S));
    } else {
        // Note that initialAnimationDuration() is used for both transitions and animations
        list.get().append(cssValuePool().createValue(Animation::initialAnimationDuration(), CSSPrimitiveValue::CSS_S));
    }
    return list;
}

static PassRef<CSSValue> createTimingFunctionValue(const TimingFunction* timingFunction)
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
            return cssValuePool().createIdentifierValue(valueId);
        }
        return CSSCubicBezierTimingFunctionValue::create(bezierTimingFunction->x1(), bezierTimingFunction->y1(), bezierTimingFunction->x2(), bezierTimingFunction->y2());
    }
    case TimingFunction::StepsFunction: {
        const StepsTimingFunction* stepsTimingFunction = static_cast<const StepsTimingFunction*>(timingFunction);
        return CSSStepsTimingFunctionValue::create(stepsTimingFunction->numberOfSteps(), stepsTimingFunction->stepAtStart());
    }
    default:
        ASSERT(timingFunction->type() == TimingFunction::LinearFunction);
        return cssValuePool().createIdentifierValue(CSSValueLinear);
    }
}

static PassRef<CSSValueList> getTimingFunctionValue(const AnimationList* animList)
{
    auto list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list.get().append(createTimingFunctionValue(animList->animation(i).timingFunction().get()));
    } else
        // Note that initialAnimationTimingFunction() is used for both transitions and animations
        list.get().append(createTimingFunctionValue(Animation::initialAnimationTimingFunction().get()));
    return list;
}

static PassRef<CSSValue> createLineBoxContainValue(unsigned lineBoxContain)
{
    if (!lineBoxContain)
        return cssValuePool().createIdentifierValue(CSSValueNone);
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

static CSSValueID cssIdentifierForFontSizeKeyword(int keywordSize)
{
    ASSERT_ARG(keywordSize, keywordSize);
    ASSERT_ARG(keywordSize, keywordSize <= 8);
    return static_cast<CSSValueID>(CSSValueXxSmall + keywordSize - 1);
}

PassRefPtr<CSSPrimitiveValue> ComputedStyleExtractor::getFontSizeCSSValuePreferringKeyword() const
{
    if (!m_node)
        return nullptr;

    m_node->document().updateLayoutIgnorePendingStylesheets();

    RefPtr<RenderStyle> style = m_node->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return nullptr;

    if (int keywordSize = style->fontDescription().keywordSize())
        return cssValuePool().createIdentifierValue(cssIdentifierForFontSizeKeyword(keywordSize));

    return zoomAdjustedPixelValue(style->fontDescription().computedPixelSize(), style.get());
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

static PassRef<CSSPrimitiveValue> valueForFamily(const AtomicString& family)
{
    if (CSSValueID familyIdentifier = identifierForFamily(family))
        return cssValuePool().createIdentifierValue(familyIdentifier);
    return cssValuePool().createValue(family.string(), CSSPrimitiveValue::CSS_STRING);
}

static PassRef<CSSValue> renderTextDecorationFlagsToCSSValue(int textDecoration)
{
    // Blink value is ignored.
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (textDecoration & TextDecorationUnderline)
        list->append(cssValuePool().createIdentifierValue(CSSValueUnderline));
    if (textDecoration & TextDecorationOverline)
        list->append(cssValuePool().createIdentifierValue(CSSValueOverline));
    if (textDecoration & TextDecorationLineThrough)
        list->append(cssValuePool().createIdentifierValue(CSSValueLineThrough));
#if ENABLE(LETTERPRESS)
    if (textDecoration & TextDecorationLetterpress)
        list->append(cssValuePool().createIdentifierValue(CSSValueWebkitLetterpress));
#endif

    if (!list->length())
        return cssValuePool().createIdentifierValue(CSSValueNone);
    return list.releaseNonNull();
}

static PassRef<CSSValue> renderTextDecorationStyleFlagsToCSSValue(TextDecorationStyle textDecorationStyle)
{
    switch (textDecorationStyle) {
    case TextDecorationStyleSolid:
        return cssValuePool().createIdentifierValue(CSSValueSolid);
    case TextDecorationStyleDouble:
        return cssValuePool().createIdentifierValue(CSSValueDouble);
    case TextDecorationStyleDotted:
        return cssValuePool().createIdentifierValue(CSSValueDotted);
    case TextDecorationStyleDashed:
        return cssValuePool().createIdentifierValue(CSSValueDashed);
    case TextDecorationStyleWavy:
        return cssValuePool().createIdentifierValue(CSSValueWavy);
    }

    ASSERT_NOT_REACHED();
    return cssValuePool().createExplicitInitialValue();
}

static PassRef<CSSValue> renderTextDecorationSkipFlagsToCSSValue(TextDecorationSkip textDecorationSkip)
{
    switch (textDecorationSkip) {
    case TextDecorationSkipAuto:
        return cssValuePool().createIdentifierValue(CSSValueAuto);
    case TextDecorationSkipNone:
        return cssValuePool().createIdentifierValue(CSSValueNone);
    case TextDecorationSkipInk:
        return cssValuePool().createIdentifierValue(CSSValueInk);
    case TextDecorationSkipObjects:
        return cssValuePool().createIdentifierValue(CSSValueObjects);
    }

    ASSERT_NOT_REACHED();
    return cssValuePool().createExplicitInitialValue();
}

static PassRef<CSSValue> renderEmphasisPositionFlagsToCSSValue(TextEmphasisPosition textEmphasisPosition)
{
    ASSERT(!((textEmphasisPosition & TextEmphasisPositionOver) && (textEmphasisPosition & TextEmphasisPositionUnder)));
    ASSERT(!((textEmphasisPosition & TextEmphasisPositionLeft) && (textEmphasisPosition & TextEmphasisPositionRight)));
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (textEmphasisPosition & TextEmphasisPositionOver)
        list->append(cssValuePool().createIdentifierValue(CSSValueOver));
    if (textEmphasisPosition & TextEmphasisPositionUnder)
        list->append(cssValuePool().createIdentifierValue(CSSValueUnder));
    if (textEmphasisPosition & TextEmphasisPositionLeft)
        list->append(cssValuePool().createIdentifierValue(CSSValueLeft));
    if (textEmphasisPosition & TextEmphasisPositionRight)
        list->append(cssValuePool().createIdentifierValue(CSSValueRight));

    if (!list->length())
        return cssValuePool().createIdentifierValue(CSSValueNone);
    return list.releaseNonNull();
}

static PassRef<CSSValue> fillRepeatToCSSValue(EFillRepeat xRepeat, EFillRepeat yRepeat)
{
    // For backwards compatibility, if both values are equal, just return one of them. And
    // if the two values are equivalent to repeat-x or repeat-y, just return the shorthand.
    if (xRepeat == yRepeat)
        return cssValuePool().createValue(xRepeat);
    if (xRepeat == RepeatFill && yRepeat == NoRepeatFill)
        return cssValuePool().createIdentifierValue(CSSValueRepeatX);
    if (xRepeat == NoRepeatFill && yRepeat == RepeatFill)
        return cssValuePool().createIdentifierValue(CSSValueRepeatY);

    auto list = CSSValueList::createSpaceSeparated();
    list.get().append(cssValuePool().createValue(xRepeat));
    list.get().append(cssValuePool().createValue(yRepeat));
    return WTF::move(list);
}

static PassRef<CSSValue> fillSourceTypeToCSSValue(EMaskSourceType type)
{
    switch (type) {
    case MaskAlpha:
        return cssValuePool().createValue(CSSValueAlpha);
    default:
        ASSERT(type == MaskLuminance);
        return cssValuePool().createValue(CSSValueLuminance);
    }
}

static PassRef<CSSValue> fillSizeToCSSValue(const FillSize& fillSize, const RenderStyle* style)
{
    if (fillSize.type == Contain)
        return cssValuePool().createIdentifierValue(CSSValueContain);

    if (fillSize.type == Cover)
        return cssValuePool().createIdentifierValue(CSSValueCover);

    if (fillSize.size.height().isAuto())
        return zoomAdjustedPixelValueForLength(fillSize.size.width(), style);

    auto list = CSSValueList::createSpaceSeparated();
    list.get().append(zoomAdjustedPixelValueForLength(fillSize.size.width(), style));
    list.get().append(zoomAdjustedPixelValueForLength(fillSize.size.height(), style));
    return WTF::move(list);
}

static PassRef<CSSValue> altTextToCSSValue(const RenderStyle* style)
{
    return cssValuePool().createValue(style->contentAltText(), CSSPrimitiveValue::CSS_STRING);
}
    
static PassRef<CSSValueList> contentToCSSValue(const RenderStyle* style)
{
    auto list = CSSValueList::createSpaceSeparated();
    for (const ContentData* contentData = style->contentData(); contentData; contentData = contentData->next()) {
        if (contentData->isCounter())
            list.get().append(cssValuePool().createValue(toCounterContentData(contentData)->counter().identifier(), CSSPrimitiveValue::CSS_COUNTER_NAME));
        else if (contentData->isImage())
            list.get().append(*toImageContentData(contentData)->image().cssValue());
        else if (contentData->isText())
            list.get().append(cssValuePool().createValue(toTextContentData(contentData)->text(), CSSPrimitiveValue::CSS_STRING));
    }
    if (style->hasFlowFrom())
        list.get().append(cssValuePool().createValue(style->regionThread(), CSSPrimitiveValue::CSS_STRING));
    return list;
}

static PassRefPtr<CSSValue> counterToCSSValue(const RenderStyle* style, CSSPropertyID propertyID)
{
    const CounterDirectiveMap* map = style->counterDirectives();
    if (!map)
        return nullptr;

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (CounterDirectiveMap::const_iterator it = map->begin(); it != map->end(); ++it) {
        list->append(cssValuePool().createValue(it->key, CSSPrimitiveValue::CSS_STRING));
        short number = propertyID == CSSPropertyCounterIncrement ? it->value.incrementValue() : it->value.resetValue();
        list->append(cssValuePool().createValue((double)number, CSSPrimitiveValue::CSS_NUMBER));
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

static PassRef<CSSValueList> fontFamilyFromStyle(RenderStyle* style)
{
    auto list = CSSValueList::createCommaSeparated();
    for (unsigned i = 0; i < style->font().familyCount(); ++i)
        list.get().append(valueForFamily(style->font().familyAt(i)));
    return list;
}

static PassRef<CSSPrimitiveValue> lineHeightFromStyle(RenderStyle* style)
{
    Length length = style->lineHeight();
    if (length.isNegative())
        return cssValuePool().createIdentifierValue(CSSValueNormal);
    if (length.isPercentNotCalculated()) {
        // This is imperfect, because it doesn't include the zoom factor and the real computation
        // for how high to be in pixels does include things like minimum font size and the zoom factor.
        // On the other hand, since font-size doesn't include the zoom factor, we really can't do
        // that here either.
        return zoomAdjustedPixelValue(static_cast<int>(length.percent() * style->fontDescription().specifiedSize()) / 100, style);
    }
    return zoomAdjustedPixelValue(floatValueForLength(length, 0), style);
}

static PassRef<CSSPrimitiveValue> fontSizeFromStyle(RenderStyle* style)
{
    return zoomAdjustedPixelValue(style->fontDescription().computedPixelSize(), style);
}

static PassRef<CSSPrimitiveValue> fontStyleFromStyle(RenderStyle* style)
{
    if (style->fontDescription().italic())
        return cssValuePool().createIdentifierValue(CSSValueItalic);
    return cssValuePool().createIdentifierValue(CSSValueNormal);
}

static PassRef<CSSPrimitiveValue> fontVariantFromStyle(RenderStyle* style)
{
    if (style->fontDescription().smallCaps())
        return cssValuePool().createIdentifierValue(CSSValueSmallCaps);
    return cssValuePool().createIdentifierValue(CSSValueNormal);
}

static PassRef<CSSPrimitiveValue> fontWeightFromStyle(RenderStyle* style)
{
    switch (style->fontDescription().weight()) {
    case FontWeight100:
        return cssValuePool().createIdentifierValue(CSSValue100);
    case FontWeight200:
        return cssValuePool().createIdentifierValue(CSSValue200);
    case FontWeight300:
        return cssValuePool().createIdentifierValue(CSSValue300);
    case FontWeightNormal:
        return cssValuePool().createIdentifierValue(CSSValueNormal);
    case FontWeight500:
        return cssValuePool().createIdentifierValue(CSSValue500);
    case FontWeight600:
        return cssValuePool().createIdentifierValue(CSSValue600);
    case FontWeightBold:
        return cssValuePool().createIdentifierValue(CSSValueBold);
    case FontWeight800:
        return cssValuePool().createIdentifierValue(CSSValue800);
    case FontWeight900:
        return cssValuePool().createIdentifierValue(CSSValue900);
    }
    ASSERT_NOT_REACHED();
    return cssValuePool().createIdentifierValue(CSSValueNormal);
}

typedef const Length& (RenderStyle::*RenderStyleLengthGetter)() const;
typedef LayoutUnit (RenderBoxModelObject::*RenderBoxComputedCSSValueGetter)() const;

template<RenderStyleLengthGetter lengthGetter, RenderBoxComputedCSSValueGetter computedCSSValueGetter>
inline PassRefPtr<CSSValue> zoomAdjustedPaddingOrMarginPixelValue(RenderStyle* style, RenderObject* renderer)
{
    Length unzoomzedLength = (style->*lengthGetter)();
    if (!renderer || !renderer->isBox() || unzoomzedLength.isFixed())
        return zoomAdjustedPixelValueForLength(unzoomzedLength, style);
    return zoomAdjustedPixelValue((toRenderBox(renderer)->*computedCSSValueGetter)(), style);
}

template<RenderStyleLengthGetter lengthGetter>
inline bool paddingOrMarginIsRendererDependent(RenderStyle* style, RenderObject* renderer)
{
    if (!renderer || !renderer->isBox())
        return false;
    return !(style && (style->*lengthGetter)().isFixed());
}

static bool isLayoutDependent(CSSPropertyID propertyID, RenderStyle* style, RenderObject* renderer)
{
    switch (propertyID) {
    case CSSPropertyWidth:
    case CSSPropertyHeight:
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridTemplateColumns:
    case CSSPropertyWebkitGridTemplateRows:
#endif
    case CSSPropertyWebkitPerspectiveOrigin:
    case CSSPropertyWebkitTransformOrigin:
    case CSSPropertyWebkitTransform:
    case CSSPropertyWebkitFilter:
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
    default:
        return false;
    }
}

Node* ComputedStyleExtractor::styledNode() const
{
    if (!m_node)
        return nullptr;
    if (!m_node->isElementNode())
        return m_node.get();
    Element* element = toElement(m_node.get());
    PseudoElement* pseudoElement;
    if (m_pseudoElementSpecifier == BEFORE && (pseudoElement = element->beforePseudoElement()))
        return pseudoElement;
    if (m_pseudoElementSpecifier == AFTER && (pseudoElement = element->afterPseudoElement()))
        return pseudoElement;
    return element;
}

PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(CSSPropertyID propertyID, EUpdateLayout updateLayout) const
{
    return ComputedStyleExtractor(m_node, m_allowVisitedStyle, m_pseudoElementSpecifier).propertyValue(propertyID, updateLayout);
}

PassRef<MutableStyleProperties> CSSComputedStyleDeclaration::copyProperties() const
{
    return ComputedStyleExtractor(m_node, m_allowVisitedStyle, m_pseudoElementSpecifier).copyProperties();
}

static inline PassRefPtr<RenderStyle> computeRenderStyleForProperty(Node* styledNode, PseudoId pseudoElementSpecifier, CSSPropertyID propertyID)
{
    RenderObject* renderer = styledNode->renderer();

    if (renderer && renderer->isComposited() && AnimationController::supportsAcceleratedAnimationOfProperty(propertyID)) {
        AnimationUpdateBlock animationUpdateBlock(&renderer->animation());
        RefPtr<RenderStyle> style = renderer->animation().getAnimatedStyleForRenderer(toRenderElement(*renderer));
        if (pseudoElementSpecifier && !styledNode->isPseudoElement()) {
            // FIXME: This cached pseudo style will only exist if the animation has been run at least once.
            return style->getCachedPseudoStyle(pseudoElementSpecifier);
        }
        return style.release();
    }

    return styledNode->computedStyle(styledNode->isPseudoElement() ? NOPSEUDO : pseudoElementSpecifier);
}

#if ENABLE(CSS_SHAPES)
static PassRef<CSSValue> shapePropertyValue(const RenderStyle* style, const ShapeValue* shapeValue)
{
    if (!shapeValue)
        return cssValuePool().createIdentifierValue(CSSValueNone);

    if (shapeValue->type() == ShapeValue::Type::Box)
        return cssValuePool().createValue(shapeValue->cssBox());

    if (shapeValue->type() == ShapeValue::Type::Image) {
        if (shapeValue->image())
            return *shapeValue->image()->cssValue();
        return cssValuePool().createIdentifierValue(CSSValueNone);
    }

    ASSERT(shapeValue->type() == ShapeValue::Type::Shape);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(valueForBasicShape(style, shapeValue->shape()));
    if (shapeValue->cssBox() != BoxMissing)
        list->append(cssValuePool().createValue(shapeValue->cssBox()));
    return list.releaseNonNull();
}
#endif

PassRefPtr<CSSValue> ComputedStyleExtractor::propertyValue(CSSPropertyID propertyID, EUpdateLayout updateLayout) const
{
    Node* styledNode = this->styledNode();
    if (!styledNode)
        return nullptr;

    RefPtr<RenderStyle> style;
    RenderObject* renderer = nullptr;
    bool forceFullLayout = false;
    if (updateLayout) {
        Document& document = styledNode->document();

        if (document.updateStyleIfNeededForNode(*styledNode)) {
            // The style recalc could have caused the styled node to be discarded or replaced
            // if it was a PseudoElement so we need to update it.
            styledNode = this->styledNode();
        }

        renderer = styledNode->renderer();

        if (propertyID == CSSPropertyDisplay && !renderer && isSVGElement(*styledNode) && !toSVGElement(*styledNode).isValid())
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

    propertyID = CSSProperty::resolveDirectionAwareProperty(propertyID, style->direction(), style->writingMode());

    switch (propertyID) {
        case CSSPropertyInvalid:
            break;

        case CSSPropertyBackgroundColor:
            return cssValuePool().createColorValue(m_allowVisitedStyle? style->visitedDependentColor(CSSPropertyBackgroundColor).rgb() : style->backgroundColor().rgb());
        case CSSPropertyBackgroundImage:
        case CSSPropertyWebkitMaskImage: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskImage ? style->maskLayers() : style->backgroundLayers();
            if (!layers)
                return cssValuePool().createIdentifierValue(CSSValueNone);

            if (!layers->next()) {
                if (layers->image())
                    return layers->image()->cssValue();

                return cssValuePool().createIdentifierValue(CSSValueNone);
            }

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next()) {
                if (currLayer->image())
                    list->append(*currLayer->image()->cssValue());
                else
                    list->append(cssValuePool().createIdentifierValue(CSSValueNone));
            }
            return list.release();
        }
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize:
        case CSSPropertyWebkitMaskSize: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskSize ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return fillSizeToCSSValue(layers->size(), style.get());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(fillSizeToCSSValue(currLayer->size(), style.get()));

            return list.release();
        }
        case CSSPropertyBackgroundRepeat:
        case CSSPropertyWebkitMaskRepeat: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskRepeat ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return fillRepeatToCSSValue(layers->repeatX(), layers->repeatY());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(fillRepeatToCSSValue(currLayer->repeatX(), currLayer->repeatY()));

            return list.release();
        }
        case CSSPropertyWebkitMaskSourceType: {
            const FillLayer* layers = style->maskLayers();

            if (!layers)
                return cssValuePool().createIdentifierValue(CSSValueNone);

            if (!layers->next())
                return fillSourceTypeToCSSValue(layers->maskSourceType());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(fillSourceTypeToCSSValue(currLayer->maskSourceType()));

            return list.release();
        }
        case CSSPropertyWebkitBackgroundComposite:
        case CSSPropertyWebkitMaskComposite: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskComposite ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool().createValue(layers->composite());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool().createValue(currLayer->composite()));

            return list.release();
        }
        case CSSPropertyBackgroundAttachment: {
            const FillLayer* layers = style->backgroundLayers();
            if (!layers->next())
                return cssValuePool().createValue(layers->attachment());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool().createValue(currLayer->attachment()));

            return list.release();
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
                return cssValuePool().createValue(box);
            }

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next()) {
                EFillBox box = isClip ? currLayer->clip() : currLayer->origin();
                list->append(cssValuePool().createValue(box));
            }

            return list.release();
        }
        case CSSPropertyBackgroundPosition:
        case CSSPropertyWebkitMaskPosition: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskPosition ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return createPositionListForLayer(propertyID, layers, style.get());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(createPositionListForLayer(propertyID, currLayer, style.get()));
            return list.release();
        }
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskPositionX ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool().createValue(layers->xPosition());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool().createValue(currLayer->xPosition()));

            return list.release();
        }
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskPositionY ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool().createValue(layers->yPosition());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool().createValue(currLayer->yPosition()));

            return list.release();
        }
        case CSSPropertyBorderCollapse:
            if (style->borderCollapse())
                return cssValuePool().createIdentifierValue(CSSValueCollapse);
            return cssValuePool().createIdentifierValue(CSSValueSeparate);
        case CSSPropertyBorderSpacing: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            list->append(zoomAdjustedPixelValue(style->horizontalBorderSpacing(), style.get()));
            list->append(zoomAdjustedPixelValue(style->verticalBorderSpacing(), style.get()));
            return list.release();
        }
        case CSSPropertyWebkitBorderHorizontalSpacing:
            return zoomAdjustedPixelValue(style->horizontalBorderSpacing(), style.get());
        case CSSPropertyWebkitBorderVerticalSpacing:
            return zoomAdjustedPixelValue(style->verticalBorderSpacing(), style.get());
        case CSSPropertyBorderImageSource:
            if (style->borderImageSource())
                return style->borderImageSource()->cssValue();
            return cssValuePool().createIdentifierValue(CSSValueNone);
        case CSSPropertyBorderTopColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyBorderTopColor).rgb()) : currentColorOrValidColor(style.get(), style->borderTopColor());
        case CSSPropertyBorderRightColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyBorderRightColor).rgb()) : currentColorOrValidColor(style.get(), style->borderRightColor());
        case CSSPropertyBorderBottomColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyBorderBottomColor).rgb()) : currentColorOrValidColor(style.get(), style->borderBottomColor());
        case CSSPropertyBorderLeftColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyBorderLeftColor).rgb()) : currentColorOrValidColor(style.get(), style->borderLeftColor());
        case CSSPropertyBorderTopStyle:
            return cssValuePool().createValue(style->borderTopStyle());
        case CSSPropertyBorderRightStyle:
            return cssValuePool().createValue(style->borderRightStyle());
        case CSSPropertyBorderBottomStyle:
            return cssValuePool().createValue(style->borderBottomStyle());
        case CSSPropertyBorderLeftStyle:
            return cssValuePool().createValue(style->borderLeftStyle());
        case CSSPropertyBorderTopWidth:
            return zoomAdjustedPixelValue(style->borderTopWidth(), style.get());
        case CSSPropertyBorderRightWidth:
            return zoomAdjustedPixelValue(style->borderRightWidth(), style.get());
        case CSSPropertyBorderBottomWidth:
            return zoomAdjustedPixelValue(style->borderBottomWidth(), style.get());
        case CSSPropertyBorderLeftWidth:
            return zoomAdjustedPixelValue(style->borderLeftWidth(), style.get());
        case CSSPropertyBottom:
            return positionOffsetValue(style.get(), CSSPropertyBottom);
        case CSSPropertyWebkitBoxAlign:
            return cssValuePool().createValue(style->boxAlign());
#if ENABLE(CSS_BOX_DECORATION_BREAK)
        case CSSPropertyWebkitBoxDecorationBreak:
            if (style->boxDecorationBreak() == DSLICE)
                return cssValuePool().createIdentifierValue(CSSValueSlice);
        return cssValuePool().createIdentifierValue(CSSValueClone);
#endif
        case CSSPropertyWebkitBoxDirection:
            return cssValuePool().createValue(style->boxDirection());
        case CSSPropertyWebkitBoxFlex:
            return cssValuePool().createValue(style->boxFlex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxFlexGroup:
            return cssValuePool().createValue(style->boxFlexGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxLines:
            return cssValuePool().createValue(style->boxLines());
        case CSSPropertyWebkitBoxOrdinalGroup:
            return cssValuePool().createValue(style->boxOrdinalGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxOrient:
            return cssValuePool().createValue(style->boxOrient());
        case CSSPropertyWebkitBoxPack:
            return cssValuePool().createValue(style->boxPack());
        case CSSPropertyWebkitBoxReflect:
            return valueForReflection(style->boxReflect(), style.get());
        case CSSPropertyBoxShadow:
        case CSSPropertyWebkitBoxShadow:
            return valueForShadow(style->boxShadow(), propertyID, style.get());
        case CSSPropertyCaptionSide:
            return cssValuePool().createValue(style->captionSide());
        case CSSPropertyClear:
            return cssValuePool().createValue(style->clear());
        case CSSPropertyColor:
            return cssValuePool().createColorValue(m_allowVisitedStyle ? style->visitedDependentColor(CSSPropertyColor).rgb() : style->color().rgb());
        case CSSPropertyWebkitPrintColorAdjust:
            return cssValuePool().createValue(style->printColorAdjust());
        case CSSPropertyWebkitColumnAxis:
            return cssValuePool().createValue(style->columnAxis());
        case CSSPropertyWebkitColumnCount:
            if (style->hasAutoColumnCount())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->columnCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitColumnFill:
            return cssValuePool().createValue(style->columnFill());
        case CSSPropertyWebkitColumnGap:
            if (style->hasNormalColumnGap())
                return cssValuePool().createIdentifierValue(CSSValueNormal);
            return zoomAdjustedPixelValue(style->columnGap(), style.get());
        case CSSPropertyWebkitColumnProgression:
            return cssValuePool().createValue(style->columnProgression());
        case CSSPropertyWebkitColumnRuleColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyOutlineColor).rgb()) : currentColorOrValidColor(style.get(), style->columnRuleColor());
        case CSSPropertyWebkitColumnRuleStyle:
            return cssValuePool().createValue(style->columnRuleStyle());
        case CSSPropertyWebkitColumnRuleWidth:
            return zoomAdjustedPixelValue(style->columnRuleWidth(), style.get());
        case CSSPropertyWebkitColumnSpan:
            return cssValuePool().createIdentifierValue(style->columnSpan() ? CSSValueAll : CSSValueNone);
        case CSSPropertyWebkitColumnBreakAfter:
            return cssValuePool().createValue(style->columnBreakAfter());
        case CSSPropertyWebkitColumnBreakBefore:
            return cssValuePool().createValue(style->columnBreakBefore());
        case CSSPropertyWebkitColumnBreakInside:
            return cssValuePool().createValue(style->columnBreakInside());
        case CSSPropertyWebkitColumnWidth:
            if (style->hasAutoColumnWidth())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return zoomAdjustedPixelValue(style->columnWidth(), style.get());
        case CSSPropertyTabSize:
            return cssValuePool().createValue(style->tabSize(), CSSPrimitiveValue::CSS_NUMBER);
#if ENABLE(CSS_REGIONS)
        case CSSPropertyWebkitRegionBreakAfter:
            return cssValuePool().createValue(style->regionBreakAfter());
        case CSSPropertyWebkitRegionBreakBefore:
            return cssValuePool().createValue(style->regionBreakBefore());
        case CSSPropertyWebkitRegionBreakInside:
            return cssValuePool().createValue(style->regionBreakInside());
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
            auto value = cssValuePool().createValue(style->cursor());
            if (list) {
                list->append(WTF::move(value));
                return list.release();
            }
            return WTF::move(value);
        }
#if ENABLE(CURSOR_VISIBILITY)
        case CSSPropertyWebkitCursorVisibility:
            return cssValuePool().createValue(style->cursorVisibility());
#endif
        case CSSPropertyDirection:
            return cssValuePool().createValue(style->direction());
        case CSSPropertyDisplay:
            return cssValuePool().createValue(style->display());
        case CSSPropertyEmptyCells:
            return cssValuePool().createValue(style->emptyCells());
        case CSSPropertyAlignContent:
            return cssValuePool().createValue(style->alignContent());
        case CSSPropertyAlignItems:
            return cssValuePool().createValue(style->alignItems());
        case CSSPropertyAlignSelf:
            if (style->alignSelf() == AlignAuto) {
                Node* parent = styledNode->parentNode();
                if (parent && parent->computedStyle())
                    return cssValuePool().createValue(parent->computedStyle()->alignItems());
                return cssValuePool().createValue(AlignStretch);
            }
            return cssValuePool().createValue(style->alignSelf());
        case CSSPropertyFlex:
            return getCSSPropertyValuesForShorthandProperties(flexShorthand());
        case CSSPropertyFlexBasis:
            return cssValuePool().createValue(style->flexBasis());
        case CSSPropertyFlexDirection:
            return cssValuePool().createValue(style->flexDirection());
        case CSSPropertyFlexFlow:
            return getCSSPropertyValuesForShorthandProperties(flexFlowShorthand());
        case CSSPropertyFlexGrow:
            return cssValuePool().createValue(style->flexGrow());
        case CSSPropertyFlexShrink:
            return cssValuePool().createValue(style->flexShrink());
        case CSSPropertyFlexWrap:
            return cssValuePool().createValue(style->flexWrap());
        case CSSPropertyJustifyContent:
            return cssValuePool().createValue(style->justifyContent());
        case CSSPropertyWebkitJustifySelf: {
            RefPtr<CSSValueList> result = CSSValueList::createSpaceSeparated();
            result->append(CSSPrimitiveValue::create(style->justifySelf()));
            if (style->justifySelf() >= JustifySelfCenter && style->justifySelfOverflowAlignment() != JustifySelfOverflowAlignmentDefault)
                result->append(CSSPrimitiveValue::create(style->justifySelfOverflowAlignment()));
            return result.release();
        }
        case CSSPropertyOrder:
            return cssValuePool().createValue(style->order(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyFloat:
            if (style->display() != NONE && style->hasOutOfFlowPosition())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return cssValuePool().createValue(style->floating());
        case CSSPropertyFont: {
            RefPtr<CSSFontValue> computedFont = CSSFontValue::create();
            computedFont->style = fontStyleFromStyle(style.get());
            computedFont->variant = fontVariantFromStyle(style.get());
            computedFont->weight = fontWeightFromStyle(style.get());
            computedFont->size = fontSizeFromStyle(style.get());
            computedFont->lineHeight = lineHeightFromStyle(style.get());
            computedFont->family = fontFamilyFromStyle(style.get());
            return computedFont.release();
        }
        case CSSPropertyFontFamily: {
            RefPtr<CSSValueList> fontFamilyList = fontFamilyFromStyle(style.get());
            // If there's only a single family, return that as a CSSPrimitiveValue.
            // NOTE: Gecko always returns this as a comma-separated CSSPrimitiveValue string.
            if (fontFamilyList->length() == 1)
                return fontFamilyList->item(0);
            return fontFamilyList.release();
        }
        case CSSPropertyFontSize:
            return fontSizeFromStyle(style.get());
        case CSSPropertyFontStyle:
            return fontStyleFromStyle(style.get());
        case CSSPropertyFontVariant:
            return fontVariantFromStyle(style.get());
        case CSSPropertyFontWeight:
            return fontWeightFromStyle(style.get());
        case CSSPropertyWebkitFontFeatureSettings: {
            const FontFeatureSettings* featureSettings = style->fontDescription().featureSettings();
            if (!featureSettings || !featureSettings->size())
                return cssValuePool().createIdentifierValue(CSSValueNormal);
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (unsigned i = 0; i < featureSettings->size(); ++i) {
                const FontFeature& feature = featureSettings->at(i);
                list->append(CSSFontFeatureValue::create(feature.tag(), feature.value()));
            }
            return list.release();
        }
#if ENABLE(CSS_GRID_LAYOUT)
        case CSSPropertyWebkitGridAutoFlow: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            ASSERT(style->isGridAutoFlowDirectionRow() || style->isGridAutoFlowDirectionColumn());
            if (style->isGridAutoFlowDirectionRow())
                list->append(cssValuePool().createIdentifierValue(CSSValueRow));
            else
                list->append(cssValuePool().createIdentifierValue(CSSValueColumn));

            if (style->isGridAutoFlowAlgorithmDense())
                list->append(cssValuePool().createIdentifierValue(CSSValueDense));
            else if (style->isGridAutoFlowAlgorithmStack())
                list->append(cssValuePool().createIdentifierValue(CSSValueStack));

            return list.release();
        }

        // Specs mention that getComputedStyle() should return the used value of the property instead of the computed
        // one for grid-definition-{rows|columns} but not for the grid-auto-{rows|columns} as things like
        // grid-auto-columns: 2fr; cannot be resolved to a value in pixels as the '2fr' means very different things
        // depending on the size of the explicit grid or the number of implicit tracks added to the grid. See
        // http://lists.w3.org/Archives/Public/www-style/2013Nov/0014.html
        case CSSPropertyWebkitGridAutoColumns:
            return specifiedValueForGridTrackSize(style->gridAutoColumns(), style.get());
        case CSSPropertyWebkitGridAutoRows:
            return specifiedValueForGridTrackSize(style->gridAutoRows(), style.get());

        case CSSPropertyWebkitGridTemplateColumns:
            return valueForGridTrackList(ForColumns, renderer, style.get());
        case CSSPropertyWebkitGridTemplateRows:
            return valueForGridTrackList(ForRows, renderer, style.get());

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
                return cssValuePool().createIdentifierValue(CSSValueNone);
            }

            return CSSGridTemplateAreasValue::create(style->namedGridArea(), style->namedGridAreaRowCount(), style->namedGridAreaColumnCount());
#endif /* ENABLE(CSS_GRID_LAYOUT) */
        case CSSPropertyHeight:
            if (renderer && !renderer->isRenderSVGModelObject()) {
                // According to http://www.w3.org/TR/CSS2/visudet.html#the-height-property,
                // the "height" property does not apply for non-replaced inline elements.
                if (!renderer->isReplaced() && renderer->isInline())
                    return cssValuePool().createIdentifierValue(CSSValueAuto);
                return zoomAdjustedPixelValue(sizingBox(renderer).height(), style.get());
            }
            return zoomAdjustedPixelValueForLength(style->height(), style.get());
        case CSSPropertyWebkitHyphens:
            return cssValuePool().createValue(style->hyphens());
        case CSSPropertyWebkitHyphenateCharacter:
            if (style->hyphenationString().isNull())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->hyphenationString(), CSSPrimitiveValue::CSS_STRING);
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
                return cssValuePool().createIdentifierValue(CSSValueBorder);
            return cssValuePool().createIdentifierValue(CSSValueLines);
#if ENABLE(CSS_IMAGE_ORIENTATION)
        case CSSPropertyImageOrientation:
            return cssValuePool().createValue(style->imageOrientation());
#endif
        case CSSPropertyImageRendering:
            return CSSPrimitiveValue::create(style->imageRendering());
#if ENABLE(CSS_IMAGE_RESOLUTION)
        case CSSPropertyImageResolution:
            return cssValuePool().createValue(style->imageResolution(), CSSPrimitiveValue::CSS_DPPX);
#endif
        case CSSPropertyLeft:
            return positionOffsetValue(style.get(), CSSPropertyLeft);
        case CSSPropertyLetterSpacing:
            if (!style->letterSpacing())
                return cssValuePool().createIdentifierValue(CSSValueNormal);
            return zoomAdjustedPixelValue(style->letterSpacing(), style.get());
        case CSSPropertyWebkitLineClamp:
            if (style->lineClamp().isNone())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return cssValuePool().createValue(style->lineClamp().value(), style->lineClamp().isPercentage() ? CSSPrimitiveValue::CSS_PERCENTAGE : CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyLineHeight:
            return lineHeightFromStyle(style.get());
        case CSSPropertyListStyleImage:
            if (style->listStyleImage())
                return style->listStyleImage()->cssValue();
            return cssValuePool().createIdentifierValue(CSSValueNone);
        case CSSPropertyListStylePosition:
            return cssValuePool().createValue(style->listStylePosition());
        case CSSPropertyListStyleType:
            return cssValuePool().createValue(style->listStyleType());
        case CSSPropertyWebkitLocale:
            if (style->locale().isNull())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->locale(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyMarginTop:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::marginTop, &RenderBoxModelObject::marginTop>(style.get(), renderer);
        case CSSPropertyMarginRight: {
            Length marginRight = style->marginRight();
            if (marginRight.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(marginRight, style.get());
            float value;
            if (marginRight.isPercent()) {
                // RenderBox gives a marginRight() that is the distance between the right-edge of the child box
                // and the right-edge of the containing box, when display == BLOCK. Let's calculate the absolute
                // value of the specified margin-right % instead of relying on RenderBox's marginRight() value.
                value = minimumValueForLength(marginRight, toRenderBox(renderer)->containingBlockLogicalWidthForContent());
            } else
                value = toRenderBox(renderer)->marginRight();
            return zoomAdjustedPixelValue(value, style.get());
        }
        case CSSPropertyMarginBottom:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::marginBottom, &RenderBoxModelObject::marginBottom>(style.get(), renderer);
        case CSSPropertyMarginLeft:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::marginLeft, &RenderBoxModelObject::marginLeft>(style.get(), renderer);
        case CSSPropertyWebkitMarqueeDirection:
            return cssValuePool().createValue(style->marqueeDirection());
        case CSSPropertyWebkitMarqueeIncrement:
            return cssValuePool().createValue(style->marqueeIncrement());
        case CSSPropertyWebkitMarqueeRepetition:
            if (style->marqueeLoopCount() < 0)
                return cssValuePool().createIdentifierValue(CSSValueInfinite);
            return cssValuePool().createValue(style->marqueeLoopCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitMarqueeStyle:
            return cssValuePool().createValue(style->marqueeBehavior());
        case CSSPropertyWebkitUserModify:
            return cssValuePool().createValue(style->userModify());
        case CSSPropertyMaxHeight: {
            const Length& maxHeight = style->maxHeight();
            if (maxHeight.isUndefined())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValueForLength(maxHeight, style.get());
        }
        case CSSPropertyMaxWidth: {
            const Length& maxWidth = style->maxWidth();
            if (maxWidth.isUndefined())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValueForLength(maxWidth, style.get());
        }
        case CSSPropertyMinHeight:
            // FIXME: For flex-items, min-height:auto should compute to min-content.
            if (style->minHeight().isAuto())
                return zoomAdjustedPixelValue(0, style.get());
            return zoomAdjustedPixelValueForLength(style->minHeight(), style.get());
        case CSSPropertyMinWidth:
            // FIXME: For flex-items, min-width:auto should compute to min-content.
            if (style->minWidth().isAuto())
                return zoomAdjustedPixelValue(0, style.get());
            return zoomAdjustedPixelValueForLength(style->minWidth(), style.get());
        case CSSPropertyObjectFit:
            return cssValuePool().createValue(style->objectFit());
        case CSSPropertyOpacity:
            return cssValuePool().createValue(style->opacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOrphans:
            if (style->hasAutoOrphans())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->orphans(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOutlineColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyOutlineColor).rgb()) : currentColorOrValidColor(style.get(), style->outlineColor());
        case CSSPropertyOutlineOffset:
            return zoomAdjustedPixelValue(style->outlineOffset(), style.get());
        case CSSPropertyOutlineStyle:
            if (style->outlineStyleIsAuto())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->outlineStyle());
        case CSSPropertyOutlineWidth:
            return zoomAdjustedPixelValue(style->outlineWidth(), style.get());
        case CSSPropertyOverflow:
            return cssValuePool().createValue(std::max(style->overflowX(), style->overflowY()));
        case CSSPropertyOverflowWrap:
            return cssValuePool().createValue(style->overflowWrap());
        case CSSPropertyOverflowX:
            return cssValuePool().createValue(style->overflowX());
        case CSSPropertyOverflowY:
            return cssValuePool().createValue(style->overflowY());
        case CSSPropertyPaddingTop:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingTop, &RenderBoxModelObject::computedCSSPaddingTop>(style.get(), renderer);
        case CSSPropertyPaddingRight:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingRight, &RenderBoxModelObject::computedCSSPaddingRight>(style.get(), renderer);
        case CSSPropertyPaddingBottom:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingBottom, &RenderBoxModelObject::computedCSSPaddingBottom>(style.get(), renderer);
        case CSSPropertyPaddingLeft:
            return zoomAdjustedPaddingOrMarginPixelValue<&RenderStyle::paddingLeft, &RenderBoxModelObject::computedCSSPaddingLeft>(style.get(), renderer);
        case CSSPropertyPageBreakAfter:
            return cssValuePool().createValue(style->pageBreakAfter());
        case CSSPropertyPageBreakBefore:
            return cssValuePool().createValue(style->pageBreakBefore());
        case CSSPropertyPageBreakInside: {
            EPageBreak pageBreak = style->pageBreakInside();
            ASSERT(pageBreak != PBALWAYS);
            if (pageBreak == PBALWAYS)
                return nullptr;
            return cssValuePool().createValue(style->pageBreakInside());
        }
        case CSSPropertyPosition:
            return cssValuePool().createValue(style->position());
        case CSSPropertyRight:
            return positionOffsetValue(style.get(), CSSPropertyRight);
        case CSSPropertyWebkitRubyPosition:
            return cssValuePool().createValue(style->rubyPosition());
        case CSSPropertyTableLayout:
            return cssValuePool().createValue(style->tableLayout());
        case CSSPropertyTextAlign:
            return cssValuePool().createValue(style->textAlign());
        case CSSPropertyTextDecoration:
            return renderTextDecorationFlagsToCSSValue(style->textDecoration());
#if ENABLE(CSS3_TEXT)
        case CSSPropertyWebkitTextAlignLast:
            return cssValuePool().createValue(style->textAlignLast());
        case CSSPropertyWebkitTextJustify:
            return cssValuePool().createValue(style->textJustify());
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
            return cssValuePool().createValue(style->textUnderlinePosition());
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
                return cssValuePool().createIdentifierValue(CSSValueNone);
            case TextEmphasisMarkCustom:
                return cssValuePool().createValue(style->textEmphasisCustomMark(), CSSPrimitiveValue::CSS_STRING);
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
                list->append(cssValuePool().createValue(style->textEmphasisFill()));
                list->append(cssValuePool().createValue(style->textEmphasisMark()));
                return list.release();
            }
            }
        case CSSPropertyTextIndent: {
            // If CSS3_TEXT is disabled or text-indent has only one value(<length> | <percentage>),
            // getPropertyCSSValue() returns CSSValue.
            RefPtr<CSSValue> textIndent = zoomAdjustedPixelValueForLength(style->textIndent(), style.get());
#if ENABLE(CSS3_TEXT)
            // If CSS3_TEXT is enabled and text-indent has -webkit-each-line or -webkit-hanging,
            // getPropertyCSSValue() returns CSSValueList.
            if (style->textIndentLine() == TextIndentEachLine || style->textIndentType() == TextIndentHanging) {
                RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
                list->append(textIndent.releaseNonNull());
                if (style->textIndentLine() == TextIndentEachLine)
                    list->append(cssValuePool().createIdentifierValue(CSSValueWebkitEachLine));
                if (style->textIndentType() == TextIndentHanging)
                    list->append(cssValuePool().createIdentifierValue(CSSValueWebkitHanging));
                return list.release();
            }
#endif
            return textIndent.release();
        }
        case CSSPropertyTextShadow:
            return valueForShadow(style->textShadow(), propertyID, style.get());
        case CSSPropertyTextRendering:
            return cssValuePool().createValue(style->fontDescription().textRenderingMode());
        case CSSPropertyTextOverflow:
            if (style->textOverflow())
                return cssValuePool().createIdentifierValue(CSSValueEllipsis);
            return cssValuePool().createIdentifierValue(CSSValueClip);
        case CSSPropertyWebkitTextSecurity:
            return cssValuePool().createValue(style->textSecurity());
#if ENABLE(IOS_TEXT_AUTOSIZING)
        case CSSPropertyWebkitTextSizeAdjust:
            if (style->textSizeAdjust().isAuto())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            if (style->textSizeAdjust().isNone())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return CSSPrimitiveValue::create(style->textSizeAdjust().percentage(), CSSPrimitiveValue::CSS_PERCENTAGE);
#endif
        case CSSPropertyWebkitTextStrokeColor:
            return currentColorOrValidColor(style.get(), style->textStrokeColor());
        case CSSPropertyWebkitTextStrokeWidth:
            return zoomAdjustedPixelValue(style->textStrokeWidth(), style.get());
        case CSSPropertyTextTransform:
            return cssValuePool().createValue(style->textTransform());
        case CSSPropertyTop:
            return positionOffsetValue(style.get(), CSSPropertyTop);
        case CSSPropertyUnicodeBidi:
            return cssValuePool().createValue(style->unicodeBidi());
        case CSSPropertyVerticalAlign:
            switch (style->verticalAlign()) {
                case BASELINE:
                    return cssValuePool().createIdentifierValue(CSSValueBaseline);
                case MIDDLE:
                    return cssValuePool().createIdentifierValue(CSSValueMiddle);
                case SUB:
                    return cssValuePool().createIdentifierValue(CSSValueSub);
                case SUPER:
                    return cssValuePool().createIdentifierValue(CSSValueSuper);
                case TEXT_TOP:
                    return cssValuePool().createIdentifierValue(CSSValueTextTop);
                case TEXT_BOTTOM:
                    return cssValuePool().createIdentifierValue(CSSValueTextBottom);
                case TOP:
                    return cssValuePool().createIdentifierValue(CSSValueTop);
                case BOTTOM:
                    return cssValuePool().createIdentifierValue(CSSValueBottom);
                case BASELINE_MIDDLE:
                    return cssValuePool().createIdentifierValue(CSSValueWebkitBaselineMiddle);
                case LENGTH:
                    return cssValuePool().createValue(style->verticalAlignLength());
            }
            ASSERT_NOT_REACHED();
            return nullptr;
        case CSSPropertyVisibility:
            return cssValuePool().createValue(style->visibility());
        case CSSPropertyWhiteSpace:
            return cssValuePool().createValue(style->whiteSpace());
        case CSSPropertyWidows:
            if (style->hasAutoWidows())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->widows(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWidth:
            if (renderer && !renderer->isRenderSVGModelObject()) {
                // According to http://www.w3.org/TR/CSS2/visudet.html#the-width-property,
                // the "width" property does not apply for non-replaced inline elements.
                if (!renderer->isReplaced() && renderer->isInline())
                    return cssValuePool().createIdentifierValue(CSSValueAuto);
                return zoomAdjustedPixelValue(sizingBox(renderer).width(), style.get());
            }
            return zoomAdjustedPixelValueForLength(style->width(), style.get());
        case CSSPropertyWordBreak:
            return cssValuePool().createValue(style->wordBreak());
        case CSSPropertyWordSpacing:
            return zoomAdjustedPixelValue(style->font().wordSpacing(), style.get());
        case CSSPropertyWordWrap:
            return cssValuePool().createValue(style->overflowWrap());
        case CSSPropertyWebkitLineBreak:
            return cssValuePool().createValue(style->lineBreak());
        case CSSPropertyWebkitNbspMode:
            return cssValuePool().createValue(style->nbspMode());
        case CSSPropertyResize:
            return cssValuePool().createValue(style->resize());
        case CSSPropertyWebkitFontKerning:
            return cssValuePool().createValue(style->fontDescription().kerning());
        case CSSPropertyWebkitFontSmoothing:
            return cssValuePool().createValue(style->fontDescription().fontSmoothing());
        case CSSPropertyWebkitFontVariantLigatures: {
            FontDescription::LigaturesState commonLigaturesState = style->fontDescription().commonLigaturesState();
            FontDescription::LigaturesState discretionaryLigaturesState = style->fontDescription().discretionaryLigaturesState();
            FontDescription::LigaturesState historicalLigaturesState = style->fontDescription().historicalLigaturesState();
            if (commonLigaturesState == FontDescription::NormalLigaturesState && discretionaryLigaturesState == FontDescription::NormalLigaturesState
                && historicalLigaturesState == FontDescription::NormalLigaturesState)
                return cssValuePool().createIdentifierValue(CSSValueNormal);

            RefPtr<CSSValueList> valueList = CSSValueList::createSpaceSeparated();
            if (commonLigaturesState != FontDescription::NormalLigaturesState)
                valueList->append(cssValuePool().createIdentifierValue(commonLigaturesState == FontDescription::DisabledLigaturesState ? CSSValueNoCommonLigatures : CSSValueCommonLigatures));
            if (discretionaryLigaturesState != FontDescription::NormalLigaturesState)
                valueList->append(cssValuePool().createIdentifierValue(discretionaryLigaturesState == FontDescription::DisabledLigaturesState ? CSSValueNoDiscretionaryLigatures : CSSValueDiscretionaryLigatures));
            if (historicalLigaturesState != FontDescription::NormalLigaturesState)
                valueList->append(cssValuePool().createIdentifierValue(historicalLigaturesState == FontDescription::DisabledLigaturesState ? CSSValueNoHistoricalLigatures : CSSValueHistoricalLigatures));
            return valueList;
        }
        case CSSPropertyZIndex:
            if (style->hasAutoZIndex())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->zIndex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyZoom:
            return cssValuePool().createValue(style->zoom(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyBoxSizing:
            if (style->boxSizing() == CONTENT_BOX)
                return cssValuePool().createIdentifierValue(CSSValueContentBox);
            return cssValuePool().createIdentifierValue(CSSValueBorderBox);
#if ENABLE(DASHBOARD_SUPPORT)
        case CSSPropertyWebkitDashboardRegion:
        {
            const Vector<StyleDashboardRegion>& regions = style->dashboardRegions();
            unsigned count = regions.size();
            if (count == 1 && regions[0].type == StyleDashboardRegion::None)
                return cssValuePool().createIdentifierValue(CSSValueNone);

            RefPtr<DashboardRegion> firstRegion;
            DashboardRegion* previousRegion = nullptr;
            for (unsigned i = 0; i < count; i++) {
                RefPtr<DashboardRegion> region = DashboardRegion::create();
                StyleDashboardRegion styleRegion = regions[i];

                region->m_label = styleRegion.label;
                LengthBox offset = styleRegion.offset;
                region->setTop(zoomAdjustedPixelValue(offset.top().value(), style.get()));
                region->setRight(zoomAdjustedPixelValue(offset.right().value(), style.get()));
                region->setBottom(zoomAdjustedPixelValue(offset.bottom().value(), style.get()));
                region->setLeft(zoomAdjustedPixelValue(offset.left().value(), style.get()));
                region->m_isRectangle = (styleRegion.type == StyleDashboardRegion::Rectangle);
                region->m_isCircle = (styleRegion.type == StyleDashboardRegion::Circle);

                if (previousRegion)
                    previousRegion->m_next = region;
                else
                    firstRegion = region;
                previousRegion = region.get();
            }
            return cssValuePool().createValue(firstRegion.release());
        }
#endif
        case CSSPropertyWebkitAnimationDelay:
            return getDelayValue(style->animations());
        case CSSPropertyWebkitAnimationDirection: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    if (t->animation(i).direction())
                        list->append(cssValuePool().createIdentifierValue(CSSValueAlternate));
                    else
                        list->append(cssValuePool().createIdentifierValue(CSSValueNormal));
                }
            } else
                list->append(cssValuePool().createIdentifierValue(CSSValueNormal));
            return list.release();
        }
        case CSSPropertyWebkitAnimationDuration:
            return getDurationValue(style->animations());
        case CSSPropertyWebkitAnimationFillMode: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    switch (t->animation(i).fillMode()) {
                    case AnimationFillModeNone:
                        list->append(cssValuePool().createIdentifierValue(CSSValueNone));
                        break;
                    case AnimationFillModeForwards:
                        list->append(cssValuePool().createIdentifierValue(CSSValueForwards));
                        break;
                    case AnimationFillModeBackwards:
                        list->append(cssValuePool().createIdentifierValue(CSSValueBackwards));
                        break;
                    case AnimationFillModeBoth:
                        list->append(cssValuePool().createIdentifierValue(CSSValueBoth));
                        break;
                    }
                }
            } else
                list->append(cssValuePool().createIdentifierValue(CSSValueNone));
            return list.release();
        }
        case CSSPropertyWebkitAnimationIterationCount: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    double iterationCount = t->animation(i).iterationCount();
                    if (iterationCount == Animation::IterationCountInfinite)
                        list->append(cssValuePool().createIdentifierValue(CSSValueInfinite));
                    else
                        list->append(cssValuePool().createValue(iterationCount, CSSPrimitiveValue::CSS_NUMBER));
                }
            } else
                list->append(cssValuePool().createValue(Animation::initialAnimationIterationCount(), CSSPrimitiveValue::CSS_NUMBER));
            return list.release();
        }
        case CSSPropertyWebkitAnimationName: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i)
                    list->append(cssValuePool().createValue(t->animation(i).name(), CSSPrimitiveValue::CSS_STRING));
            } else
                list->append(cssValuePool().createIdentifierValue(CSSValueNone));
            return list.release();
        }
        case CSSPropertyWebkitAnimationPlayState: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    int prop = t->animation(i).playState();
                    if (prop == AnimPlayStatePlaying)
                        list->append(cssValuePool().createIdentifierValue(CSSValueRunning));
                    else
                        list->append(cssValuePool().createIdentifierValue(CSSValuePaused));
                }
            } else
                list->append(cssValuePool().createIdentifierValue(CSSValueRunning));
            return list.release();
        }
        case CSSPropertyWebkitAnimationTimingFunction:
            return getTimingFunctionValue(style->animations());
        case CSSPropertyWebkitAppearance:
            return cssValuePool().createValue(style->appearance());
        case CSSPropertyWebkitAspectRatio:
            if (style->aspectRatioType() == AspectRatioAuto)
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            if (style->aspectRatioType() == AspectRatioFromDimensions)
                return cssValuePool().createIdentifierValue(CSSValueFromDimensions);
            if (style->aspectRatioType() == AspectRatioFromIntrinsic)
                return cssValuePool().createIdentifierValue(CSSValueFromIntrinsic);
            return CSSAspectRatioValue::create(style->aspectRatioNumerator(), style->aspectRatioDenominator());
        case CSSPropertyWebkitBackfaceVisibility:
            return cssValuePool().createIdentifierValue((style->backfaceVisibility() == BackfaceVisibilityHidden) ? CSSValueHidden : CSSValueVisible);
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
            return cssValuePool().createIdentifierValue(CSSValueNone);
        case CSSPropertyWebkitFontSizeDelta:
            // Not a real style property -- used by the editing engine -- so has no computed value.
            break;
        case CSSPropertyWebkitInitialLetter: {
            RefPtr<CSSPrimitiveValue> drop = !style->initialLetterDrop() ? cssValuePool().createIdentifierValue(CSSValueNormal) : cssValuePool().createValue(style->initialLetterDrop(), CSSPrimitiveValue::CSS_NUMBER);
            RefPtr<CSSPrimitiveValue> size = !style->initialLetterHeight() ? cssValuePool().createIdentifierValue(CSSValueNormal) : cssValuePool().createValue(style->initialLetterHeight(), CSSPrimitiveValue::CSS_NUMBER);
            return cssValuePool().createValue(Pair::create(drop.release(), size.release()));
        }
        case CSSPropertyWebkitMarginBottomCollapse:
        case CSSPropertyWebkitMarginAfterCollapse:
            return cssValuePool().createValue(style->marginAfterCollapse());
        case CSSPropertyWebkitMarginTopCollapse:
        case CSSPropertyWebkitMarginBeforeCollapse:
            return cssValuePool().createValue(style->marginBeforeCollapse());
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
        case CSSPropertyWebkitOverflowScrolling:
            if (!style->useTouchOverflowScrolling())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createIdentifierValue(CSSValueTouch);
#endif
        case CSSPropertyWebkitPerspective:
            if (!style->hasPerspective())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValue(style->perspective(), style.get());
        case CSSPropertyWebkitPerspectiveOrigin: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                LayoutRect box;
                if (renderer->isBox())
                    box = toRenderBox(renderer)->borderBoxRect();

                list->append(zoomAdjustedPixelValue(minimumValueForLength(style->perspectiveOriginX(), box.width()), style.get()));
                list->append(zoomAdjustedPixelValue(minimumValueForLength(style->perspectiveOriginY(), box.height()), style.get()));
            }
            else {
                list->append(zoomAdjustedPixelValueForLength(style->perspectiveOriginX(), style.get()));
                list->append(zoomAdjustedPixelValueForLength(style->perspectiveOriginY(), style.get()));

            }
            return list.release();
        }
        case CSSPropertyWebkitRtlOrdering:
            return cssValuePool().createIdentifierValue(style->rtlOrdering() ? CSSValueVisual : CSSValueLogical);
#if ENABLE(TOUCH_EVENTS)
        case CSSPropertyWebkitTapHighlightColor:
            return currentColorOrValidColor(style.get(), style->tapHighlightColor());
#endif
#if PLATFORM(IOS)
        case CSSPropertyWebkitTouchCallout:
            return cssValuePool().createIdentifierValue(style->touchCalloutEnabled() ? CSSValueDefault : CSSValueNone);
#endif
        case CSSPropertyWebkitUserDrag:
            return cssValuePool().createValue(style->userDrag());
        case CSSPropertyWebkitUserSelect:
            return cssValuePool().createValue(style->userSelect());
        case CSSPropertyBorderBottomLeftRadius:
            return getBorderRadiusCornerValue(style->borderBottomLeftRadius(), style.get());
        case CSSPropertyBorderBottomRightRadius:
            return getBorderRadiusCornerValue(style->borderBottomRightRadius(), style.get());
        case CSSPropertyBorderTopLeftRadius:
            return getBorderRadiusCornerValue(style->borderTopLeftRadius(), style.get());
        case CSSPropertyBorderTopRightRadius:
            return getBorderRadiusCornerValue(style->borderTopRightRadius(), style.get());
        case CSSPropertyClip: {
            if (!style->hasClip())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            RefPtr<Rect> rect = Rect::create();
            rect->setTop(autoOrZoomAdjustedValue(style->clip().top(), style.get()));
            rect->setRight(autoOrZoomAdjustedValue(style->clip().right(), style.get()));
            rect->setBottom(autoOrZoomAdjustedValue(style->clip().bottom(), style.get()));
            rect->setLeft(autoOrZoomAdjustedValue(style->clip().left(), style.get()));
            return cssValuePool().createValue(rect.release());
        }
        case CSSPropertySpeak:
            return cssValuePool().createValue(style->speak());
        case CSSPropertyWebkitTransform:
            return computedTransform(renderer, style.get());
        case CSSPropertyWebkitTransformOrigin: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                LayoutRect box;
                if (renderer->isBox())
                    box = toRenderBox(renderer)->borderBoxRect();

                list->append(zoomAdjustedPixelValue(minimumValueForLength(style->transformOriginX(), box.width()), style.get()));
                list->append(zoomAdjustedPixelValue(minimumValueForLength(style->transformOriginY(), box.height()), style.get()));
                if (style->transformOriginZ() != 0)
                    list->append(zoomAdjustedPixelValue(style->transformOriginZ(), style.get()));
            } else {
                list->append(zoomAdjustedPixelValueForLength(style->transformOriginX(), style.get()));
                list->append(zoomAdjustedPixelValueForLength(style->transformOriginY(), style.get()));
                if (style->transformOriginZ() != 0)
                    list->append(zoomAdjustedPixelValue(style->transformOriginZ(), style.get()));
            }
            return list.release();
        }
        case CSSPropertyWebkitTransformStyle:
            return cssValuePool().createIdentifierValue((style->transformStyle3D() == TransformStyle3DPreserve3D) ? CSSValuePreserve3d : CSSValueFlat);
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
                    list->append(cssValuePool().createValue(animation.duration(), CSSPrimitiveValue::CSS_S));
                    list->append(createTimingFunctionValue(animation.timingFunction().get()));
                    list->append(cssValuePool().createValue(animation.delay(), CSSPrimitiveValue::CSS_S));
                    transitionsList->append(list.releaseNonNull());
                }
                return transitionsList.release();
            }

            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            // transition-property default value.
            list->append(cssValuePool().createIdentifierValue(CSSValueAll));
            list->append(cssValuePool().createValue(Animation::initialAnimationDuration(), CSSPrimitiveValue::CSS_S));
            list->append(createTimingFunctionValue(Animation::initialAnimationTimingFunction().get()));
            list->append(cssValuePool().createValue(Animation::initialAnimationDelay(), CSSPrimitiveValue::CSS_S));
            return list.release();
        }
        case CSSPropertyPointerEvents:
            return cssValuePool().createValue(style->pointerEvents());
        case CSSPropertyWebkitColorCorrection:
            return cssValuePool().createValue(style->colorSpace());
        case CSSPropertyWebkitLineGrid:
            if (style->lineGrid().isNull())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return cssValuePool().createValue(style->lineGrid(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitLineSnap:
            return CSSPrimitiveValue::create(style->lineSnap());
        case CSSPropertyWebkitLineAlign:
            return CSSPrimitiveValue::create(style->lineAlign());
        case CSSPropertyWebkitWritingMode:
            return cssValuePool().createValue(style->writingMode());
        case CSSPropertyWebkitTextCombine:
            return cssValuePool().createValue(style->textCombine());
        case CSSPropertyWebkitTextOrientation:
            return CSSPrimitiveValue::create(style->textOrientation());
        case CSSPropertyWebkitLineBoxContain:
            return createLineBoxContainValue(style->lineBoxContain());
        case CSSPropertyWebkitAlt:
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
                return cssValuePool().createIdentifierValue(CSSValueNone);
            if (operation->type() == ClipPathOperation::Reference) {
                ReferenceClipPathOperation& referenceOperation = toReferenceClipPathOperation(*operation);
                return CSSPrimitiveValue::create(referenceOperation.url(), CSSPrimitiveValue::CSS_URI);
            }
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (operation->type() == ClipPathOperation::Shape) {
                ShapeClipPathOperation& shapeOperation = toShapeClipPathOperation(*operation);
                list->append(valueForBasicShape(style.get(), shapeOperation.basicShape()));
                if (shapeOperation.referenceBox() != BoxMissing)
                    list->append(cssValuePool().createValue(shapeOperation.referenceBox()));
            }
            if (operation->type() == ClipPathOperation::Box) {
                BoxClipPathOperation& boxOperation = toBoxClipPathOperation(*operation);
                list->append(cssValuePool().createValue(boxOperation.referenceBox()));
            }
            return list.release();
        }
#if ENABLE(CSS_REGIONS)
        case CSSPropertyWebkitFlowInto:
            if (!style->hasFlowInto())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return cssValuePool().createValue(style->flowThread(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitFlowFrom:
            if (!style->hasFlowFrom())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return cssValuePool().createValue(style->regionThread(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitRegionFragment:
            return cssValuePool().createValue(style->regionFragment());
#endif
#if ENABLE(CSS_SHAPES)
        case CSSPropertyWebkitShapeMargin:
            return cssValuePool().createValue(style->shapeMargin(), style.get());
        case CSSPropertyWebkitShapeImageThreshold:
            return cssValuePool().createValue(style->shapeImageThreshold(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitShapeOutside:
            return shapePropertyValue(style.get(), style->shapeOutside());
#endif
        case CSSPropertyWebkitFilter:
            return valueForFilter(style.get(), style->filter());
#if ENABLE(CSS_COMPOSITING)
        case CSSPropertyMixBlendMode:
            return cssValuePool().createValue(style->blendMode());
        case CSSPropertyIsolation:
            return cssValuePool().createValue(style->isolation());
#endif
        case CSSPropertyBackgroundBlendMode: {
            const FillLayer* layers = style->backgroundLayers();
            if (!layers->next())
                return cssValuePool().createValue(layers->blendMode());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool().createValue(currLayer->blendMode()));

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
            return value.release();
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
            return getBorderRadiusShorthandValue(style.get());
        case CSSPropertyBorderRight:
            return getCSSPropertyValuesForShorthandProperties(borderRightShorthand());
        case CSSPropertyBorderStyle:
            return getCSSPropertyValuesForSidesShorthand(borderStyleShorthand());
        case CSSPropertyBorderTop:
            return getCSSPropertyValuesForShorthandProperties(borderTopShorthand());
        case CSSPropertyBorderWidth:
            return getCSSPropertyValuesForSidesShorthand(borderWidthShorthand());
        case CSSPropertyWebkitColumnRule:
            return getCSSPropertyValuesForShorthandProperties(webkitColumnRuleShorthand());
        case CSSPropertyWebkitColumns:
            return getCSSPropertyValuesForShorthandProperties(webkitColumnsShorthand());
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
            return cssValuePool().createValue(style->scrollSnapType());
        case CSSPropertyWebkitScrollSnapDestination:
            return scrollSnapDestination(*style, style->scrollSnapDestination());
        case CSSPropertyWebkitScrollSnapPointsX:
            return scrollSnapPoints(*style, style->scrollSnapPointsX());
        case CSSPropertyWebkitScrollSnapPointsY:
            return scrollSnapPoints(*style, style->scrollSnapPointsY());
        case CSSPropertyWebkitScrollSnapCoordinate:
            return scrollSnapCoordinates(*style, style->scrollSnapCoordinates());
#endif

        /* Individual properties not part of the spec */
        case CSSPropertyBackgroundRepeatX:
        case CSSPropertyBackgroundRepeatY:
            break;

        // Length properties for SVG.
        case CSSPropertyCx:
            return zoomAdjustedPixelValueForLength(style->svgStyle().cx(), style.get());
        case CSSPropertyCy:
            return zoomAdjustedPixelValueForLength(style->svgStyle().cy(), style.get());
        case CSSPropertyR:
            return zoomAdjustedPixelValueForLength(style->svgStyle().r(), style.get());
        case CSSPropertyRx:
            return zoomAdjustedPixelValueForLength(style->svgStyle().rx(), style.get());
        case CSSPropertyRy:
            return zoomAdjustedPixelValueForLength(style->svgStyle().ry(), style.get());
        case CSSPropertyStrokeWidth:
            return zoomAdjustedPixelValueForLength(style->svgStyle().strokeWidth(), style.get());
        case CSSPropertyStrokeDashoffset:
            return zoomAdjustedPixelValueForLength(style->svgStyle().strokeDashOffset(), style.get());
        case CSSPropertyX:
            return zoomAdjustedPixelValueForLength(style->svgStyle().x(), style.get());
        case CSSPropertyY:
            return zoomAdjustedPixelValueForLength(style->svgStyle().y(), style.get());

        /* Unimplemented CSS 3 properties (including CSS3 shorthand properties) */
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
        case CSSPropertyWebkitPerspectiveOriginX:
        case CSSPropertyWebkitPerspectiveOriginY:
        case CSSPropertyWebkitTextStroke:
        case CSSPropertyWebkitTransformOriginX:
        case CSSPropertyWebkitTransformOriginY:
        case CSSPropertyWebkitTransformOriginZ:
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
        case CSSPropertyFilter:
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

    return numComputedProperties;
}

String CSSComputedStyleDeclaration::item(unsigned i) const
{
    if (i >= length())
        return emptyString();

    return getPropertyNameString(computedProperties[i]);
}

bool ComputedStyleExtractor::propertyMatches(CSSPropertyID propertyID, const CSSValue* value) const
{
    if (propertyID == CSSPropertyFontSize && value->isPrimitiveValue() && m_node) {
        m_node->document().updateLayoutIgnorePendingStylesheets();
        RenderStyle* style = m_node->computedStyle(m_pseudoElementSpecifier);
        if (style && style->fontDescription().keywordSize()) {
            CSSValueID sizeValue = cssIdentifierForFontSizeKeyword(style->fontDescription().keywordSize());
            const CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
            if (primitiveValue->isValueID() && primitiveValue->getValueID() == sizeValue)
                return true;
        }
    }
    RefPtr<CSSValue> computedValue = propertyValue(propertyID);
    return computedValue && value && computedValue->equals(*value);
}

PassRef<MutableStyleProperties> ComputedStyleExtractor::copyProperties() const
{
    return copyPropertiesInSet(computedProperties, numComputedProperties);
}

PassRefPtr<CSSValueList> ComputedStyleExtractor::getCSSPropertyValuesForShorthandProperties(const StylePropertyShorthand& shorthand) const
{
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (size_t i = 0; i < shorthand.length(); ++i) {
        RefPtr<CSSValue> value = propertyValue(shorthand.properties()[i], DoNotUpdateLayout);
        list->append(value.releaseNonNull());
    }
    return list.release();
}

PassRefPtr<CSSValueList> ComputedStyleExtractor::getCSSPropertyValuesForSidesShorthand(const StylePropertyShorthand& shorthand) const
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

    return list.release();
}

PassRefPtr<CSSValueList> ComputedStyleExtractor::getCSSPropertyValuesForGridShorthand(const StylePropertyShorthand& shorthand) const
{
    RefPtr<CSSValueList> list = CSSValueList::createSlashSeparated();
    for (size_t i = 0; i < shorthand.length(); ++i) {
        RefPtr<CSSValue> value = propertyValue(shorthand.properties()[i], DoNotUpdateLayout);
        list->append(value.releaseNonNull());
    }
    return list.release();
}

PassRef<MutableStyleProperties> ComputedStyleExtractor::copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const
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

PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(const String& propertyName)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!propertyID)
        return nullptr;
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    return value ? value->cloneForCSSOM() : nullptr;
}

String CSSComputedStyleDeclaration::getPropertyValue(const String &propertyName)
{
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
    
PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValueInternal(CSSPropertyID propertyID)
{
    return getPropertyCSSValue(propertyID);
}

String CSSComputedStyleDeclaration::getPropertyValueInternal(CSSPropertyID propertyID)
{
    return getPropertyValue(propertyID);
}

void CSSComputedStyleDeclaration::setPropertyInternal(CSSPropertyID, const String&, bool, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

PassRefPtr<CSSValueList> ComputedStyleExtractor::getBackgroundShorthandValue() const
{
    static const CSSPropertyID propertiesBeforeSlashSeperator[5] = { CSSPropertyBackgroundColor, CSSPropertyBackgroundImage,
                                                                     CSSPropertyBackgroundRepeat, CSSPropertyBackgroundAttachment,  
                                                                     CSSPropertyBackgroundPosition };
    static const CSSPropertyID propertiesAfterSlashSeperator[3] = { CSSPropertyBackgroundSize, CSSPropertyBackgroundOrigin, 
                                                                    CSSPropertyBackgroundClip };

    RefPtr<CSSValueList> list = CSSValueList::createSlashSeparated();
    list->append(*getCSSPropertyValuesForShorthandProperties(StylePropertyShorthand(CSSPropertyBackground, propertiesBeforeSlashSeperator, WTF_ARRAY_LENGTH(propertiesBeforeSlashSeperator))));
    list->append(*getCSSPropertyValuesForShorthandProperties(StylePropertyShorthand(CSSPropertyBackground, propertiesAfterSlashSeperator, WTF_ARRAY_LENGTH(propertiesAfterSlashSeperator))));
    return list.release();
}

} // namespace WebCore
