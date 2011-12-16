/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
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
#include "CSSAspectRatioValue.h"
#include "CSSBorderImageValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSReflectValue.h"
#include "CSSSelector.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "ContentData.h"
#include "CounterContent.h"
#include "CursorList.h"
#if ENABLE(CSS_SHADERS)
#include "CustomFilterOperation.h"
#endif
#include "Document.h"
#include "ExceptionCode.h"
#include "FontFeatureSettings.h"
#include "FontFeatureValue.h"
#include "FontValue.h"
#include "Pair.h"
#include "Rect.h"
#include "RenderBox.h"
#include "RenderLayer.h"
#include "RenderStyle.h"
#include "ShadowValue.h"
#if ENABLE(CSS_FILTERS)
#include "WebKitCSSFilterValue.h"
#endif
#include "WebKitCSSTransformValue.h"
#include "WebKitFontFamilyNames.h"

#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

namespace WebCore {

// List of all properties we know how to compute, omitting shorthands.
static const int computedProperties[] = {
    CSSPropertyBackgroundAttachment,
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
    CSSPropertyImageRendering,
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
    CSSPropertyOutlineStyle,
    CSSPropertyOutlineWidth,
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
    CSSPropertyTextAlign,
    CSSPropertyTextDecoration,
    CSSPropertyTextIndent,
    CSSPropertyTextRendering,
    CSSPropertyTextShadow,
    CSSPropertyTextOverflow,
    CSSPropertyTextTransform,
    CSSPropertyTop,
    CSSPropertyUnicodeBidi,
    CSSPropertyVerticalAlign,
    CSSPropertyVisibility,
    CSSPropertyWhiteSpace,
    CSSPropertyWidows,
    CSSPropertyWidth,
    CSSPropertyWordBreak,
    CSSPropertyWordSpacing,
    CSSPropertyWordWrap,
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
    CSSPropertyWebkitAppearance,
    CSSPropertyWebkitBackfaceVisibility,
    CSSPropertyWebkitBackgroundClip,
    CSSPropertyWebkitBackgroundComposite,
    CSSPropertyWebkitBackgroundOrigin,
    CSSPropertyWebkitBackgroundSize,
    CSSPropertyWebkitBorderFit,
    CSSPropertyWebkitBorderHorizontalSpacing,
    CSSPropertyWebkitBorderImage,
    CSSPropertyWebkitBorderVerticalSpacing,
    CSSPropertyWebkitBoxAlign,
    CSSPropertyWebkitBoxDirection,
    CSSPropertyWebkitBoxFlex,
    CSSPropertyWebkitBoxFlexGroup,
    CSSPropertyWebkitBoxLines,
    CSSPropertyWebkitBoxOrdinalGroup,
    CSSPropertyWebkitBoxOrient,
    CSSPropertyWebkitBoxPack,
    CSSPropertyWebkitBoxReflect,
    CSSPropertyWebkitBoxShadow,
    CSSPropertyWebkitColorCorrection,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyWebkitColumnAxis,
    CSSPropertyWebkitColumnCount,
    CSSPropertyWebkitColumnGap,
    CSSPropertyWebkitColumnRuleColor,
    CSSPropertyWebkitColumnRuleStyle,
    CSSPropertyWebkitColumnRuleWidth,
    CSSPropertyWebkitColumnSpan,
    CSSPropertyWebkitColumnWidth,
#if ENABLE(DASHBOARD_SUPPORT)
    CSSPropertyWebkitDashboardRegion,
#endif
    CSSPropertyWebkitFlexOrder,
    CSSPropertyWebkitFlexPack,
    CSSPropertyWebkitFlexAlign,
    CSSPropertyWebkitFlexDirection,
    CSSPropertyWebkitFlexFlow,
    CSSPropertyWebkitFlexWrap,
    CSSPropertyWebkitFontSmoothing,
#if ENABLE(CSS_GRID_LAYOUT)
    CSSPropertyWebkitGridColumns,
    CSSPropertyWebkitGridRows,
#endif
    CSSPropertyWebkitHighlight,
    CSSPropertyWebkitHyphenateCharacter,
    CSSPropertyWebkitHyphenateLimitAfter,
    CSSPropertyWebkitHyphenateLimitBefore,
    CSSPropertyWebkitHyphenateLimitLines,
    CSSPropertyWebkitHyphens,
    CSSPropertyWebkitLineBoxContain,
    CSSPropertyWebkitLineBreak,
    CSSPropertyWebkitLineClamp,
    CSSPropertyWebkitLineGrid,
    CSSPropertyWebkitLineGridSnap,
    CSSPropertyWebkitLocale,
    CSSPropertyWebkitMarginBeforeCollapse,
    CSSPropertyWebkitMarginAfterCollapse,
    CSSPropertyWebkitMarqueeDirection,
    CSSPropertyWebkitMarqueeIncrement,
    CSSPropertyWebkitMarqueeRepetition,
    CSSPropertyWebkitMarqueeStyle,
    CSSPropertyWebkitMaskAttachment,
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
    CSSPropertyWebkitNbspMode,
    CSSPropertyWebkitPerspective,
    CSSPropertyWebkitPerspectiveOrigin,
    CSSPropertyWebkitPrintColorAdjust,
    CSSPropertyWebkitRtlOrdering,
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
    CSSPropertyWebkitFlowInto,
    CSSPropertyWebkitFlowFrom,
    CSSPropertyWebkitRegionOverflow,
    CSSPropertyWebkitRegionBreakAfter,
    CSSPropertyWebkitRegionBreakBefore,
    CSSPropertyWebkitRegionBreakInside,
    CSSPropertyWebkitWrapFlow,
    CSSPropertyWebkitWrapMargin,
    CSSPropertyWebkitWrapPadding,
    CSSPropertyWebkitWrapThrough
#if ENABLE(SVG)
    ,
    CSSPropertyClipPath,
    CSSPropertyClipRule,
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
    CSSPropertyVectorEffect
#endif
};

const unsigned numComputedProperties = WTF_ARRAY_LENGTH(computedProperties);

static int valueForRepeatRule(int rule)
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

static PassRefPtr<CSSBorderImageSliceValue> valueForNinePieceImageSlice(const NinePieceImage& image, CSSValuePool* cssValuePool)
{
    // Create the slices.
    RefPtr<CSSPrimitiveValue> top;
    RefPtr<CSSPrimitiveValue> right;
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;

    if (image.imageSlices().top().isPercent())
        top = cssValuePool->createValue(image.imageSlices().top().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        top = cssValuePool->createValue(image.imageSlices().top().value(), CSSPrimitiveValue::CSS_NUMBER);

    if (image.imageSlices().right() == image.imageSlices().top() && image.imageSlices().bottom() == image.imageSlices().top()
        && image.imageSlices().left() == image.imageSlices().top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (image.imageSlices().right().isPercent())
            right = cssValuePool->createValue(image.imageSlices().right().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
        else
            right = cssValuePool->createValue(image.imageSlices().right().value(), CSSPrimitiveValue::CSS_NUMBER);

        if (image.imageSlices().bottom() == image.imageSlices().top() && image.imageSlices().right() == image.imageSlices().left()) {
            bottom = top;
            left = right;
        } else {
            if (image.imageSlices().bottom().isPercent())
                bottom = cssValuePool->createValue(image.imageSlices().bottom().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
            else
                bottom = cssValuePool->createValue(image.imageSlices().bottom().value(), CSSPrimitiveValue::CSS_NUMBER);

            if (image.imageSlices().left() == image.imageSlices().right())
                left = right;
            else {
                if (image.imageSlices().left().isPercent())
                    left = cssValuePool->createValue(image.imageSlices().left().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
                else
                    left = cssValuePool->createValue(image.imageSlices().left().value(), CSSPrimitiveValue::CSS_NUMBER);
            }
        }
    }

    RefPtr<Quad> quad = Quad::create();
    quad->setTop(top);
    quad->setRight(right);
    quad->setBottom(bottom);
    quad->setLeft(left);

    return CSSBorderImageSliceValue::create(cssValuePool->createValue(quad.release()), image.fill());
}

static PassRefPtr<CSSPrimitiveValue> valueForNinePieceImageQuad(const LengthBox& box, CSSValuePool* cssValuePool)
{
    // Create the slices.
    RefPtr<CSSPrimitiveValue> top;
    RefPtr<CSSPrimitiveValue> right;
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;

    if (box.top().isRelative())
        top = cssValuePool->createValue(box.top().value(), CSSPrimitiveValue::CSS_NUMBER);
    else
        top = cssValuePool->createValue(box.top());

    if (box.right() == box.top() && box.bottom() == box.top() && box.left() == box.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (box.right().isRelative())
            right = cssValuePool->createValue(box.right().value(), CSSPrimitiveValue::CSS_NUMBER);
        else
            right = cssValuePool->createValue(box.right());

        if (box.bottom() == box.top() && box.right() == box.left()) {
            bottom = top;
            left = right;
        } else {
            if (box.bottom().isRelative())
                bottom = cssValuePool->createValue(box.bottom().value(), CSSPrimitiveValue::CSS_NUMBER);
            else
                bottom = cssValuePool->createValue(box.bottom());

            if (box.left() == box.right())
                left = right;
            else {
                if (box.left().isRelative())
                    left = cssValuePool->createValue(box.left().value(), CSSPrimitiveValue::CSS_NUMBER);
                else
                    left = cssValuePool->createValue(box.left());
            }
        }
    }

    RefPtr<Quad> quad = Quad::create();
    quad->setTop(top);
    quad->setRight(right);
    quad->setBottom(bottom);
    quad->setLeft(left);

    return cssValuePool->createValue(quad.release());
}

static PassRefPtr<CSSValue> valueForNinePieceImageRepeat(const NinePieceImage& image, CSSValuePool* cssValuePool)
{
    RefPtr<CSSPrimitiveValue> horizontalRepeat;
    RefPtr<CSSPrimitiveValue> verticalRepeat;

    horizontalRepeat = cssValuePool->createIdentifierValue(valueForRepeatRule(image.horizontalRule()));
    if (image.horizontalRule() == image.verticalRule())
        verticalRepeat = horizontalRepeat;
    else
        verticalRepeat = cssValuePool->createIdentifierValue(valueForRepeatRule(image.verticalRule()));
    return cssValuePool->createValue(Pair::create(horizontalRepeat.release(), verticalRepeat.release()));
}

static PassRefPtr<CSSValue> valueForNinePieceImage(const NinePieceImage& image, CSSValuePool* cssValuePool)
{
    if (!image.hasImage())
        return cssValuePool->createIdentifierValue(CSSValueNone);

    // Image first.
    RefPtr<CSSValue> imageValue;
    if (image.image())
        imageValue = image.image()->cssValue();

    // Create the image slice.
    RefPtr<CSSBorderImageSliceValue> imageSlices = valueForNinePieceImageSlice(image, cssValuePool);

    // Create the border area slices.
    RefPtr<CSSValue> borderSlices = valueForNinePieceImageQuad(image.borderSlices(), cssValuePool);

    // Create the border outset.
    RefPtr<CSSValue> outset = valueForNinePieceImageQuad(image.outset(), cssValuePool);

    // Create the repeat rules.
    RefPtr<CSSValue> repeat = valueForNinePieceImageRepeat(image, cssValuePool);

    return CSSBorderImageValue::create(imageValue.release(), imageSlices.release(), borderSlices.release(), outset.release(), repeat);
}

inline static PassRefPtr<CSSPrimitiveValue> zoomAdjustedPixelValue(int value, const RenderStyle* style, CSSValuePool* cssValuePool)
{
    return cssValuePool->createValue(adjustForAbsoluteZoom(value, style), CSSPrimitiveValue::CSS_PX);
}

inline static PassRefPtr<CSSPrimitiveValue> zoomAdjustedNumberValue(double value, const RenderStyle* style, CSSValuePool* cssValuePool)
{
    return cssValuePool->createValue(value / style->effectiveZoom(), CSSPrimitiveValue::CSS_NUMBER);
}

static PassRefPtr<CSSValue> zoomAdjustedPixelValueForLength(const Length& length, const RenderStyle* style, CSSValuePool* cssValuePool)
{
    if (length.isFixed())
        return zoomAdjustedPixelValue(length.value(), style, cssValuePool);
    return cssValuePool->createValue(length);
}

static PassRefPtr<CSSValue> valueForReflection(const StyleReflection* reflection, const RenderStyle* style, CSSValuePool* cssValuePool)
{
    if (!reflection)
        return cssValuePool->createIdentifierValue(CSSValueNone);

    RefPtr<CSSPrimitiveValue> offset;
    if (reflection->offset().isPercent())
        offset = cssValuePool->createValue(reflection->offset().percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        offset = zoomAdjustedPixelValue(reflection->offset().value(), style, cssValuePool);

    return CSSReflectValue::create(reflection->direction(), offset.release(), valueForNinePieceImage(reflection->mask(), cssValuePool));
}

static PassRefPtr<CSSValue> getPositionOffsetValue(RenderStyle* style, int propertyID, CSSValuePool* cssValuePool)
{
    if (!style)
        return 0;

    Length l;
    switch (propertyID) {
        case CSSPropertyLeft:
            l = style->left();
            break;
        case CSSPropertyRight:
            l = style->right();
            break;
        case CSSPropertyTop:
            l = style->top();
            break;
        case CSSPropertyBottom:
            l = style->bottom();
            break;
        default:
            return 0;
    }

    if (style->position() == AbsolutePosition || style->position() == FixedPosition) {
        if (l.type() == WebCore::Fixed)
            return zoomAdjustedPixelValue(l.value(), style, cssValuePool);
        return cssValuePool->createValue(l);
    }

    if (style->position() == RelativePosition)
        // FIXME: It's not enough to simply return "auto" values for one offset if the other side is defined.
        // In other words if left is auto and right is not auto, then left's computed value is negative right().
        // So we should get the opposite length unit and see if it is auto.
        return cssValuePool->createValue(l);

    return cssValuePool->createIdentifierValue(CSSValueAuto);
}

PassRefPtr<CSSPrimitiveValue> CSSComputedStyleDeclaration::currentColorOrValidColor(RenderStyle* style, const Color& color) const
{
    // This function does NOT look at visited information, so that computed style doesn't expose that.
    CSSValuePool* cssValuePool = m_node->document()->cssValuePool().get();
    if (!color.isValid())
        return cssValuePool->createColorValue(style->color().rgb());
    return cssValuePool->createColorValue(color.rgb());
}

static PassRefPtr<CSSValue> getBorderRadiusCornerValue(LengthSize radius, const RenderStyle* style, CSSValuePool* cssValuePool)
{
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (radius.width() == radius.height()) {
        if (radius.width().type() == Percent)
            return cssValuePool->createValue(radius.width().percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
        return zoomAdjustedPixelValue(radius.width().value(), style, cssValuePool);
    }
    if (radius.width().type() == Percent)
        list->append(cssValuePool->createValue(radius.width().percent(), CSSPrimitiveValue::CSS_PERCENTAGE));
    else
        list->append(zoomAdjustedPixelValue(radius.width().value(), style, cssValuePool));
    if (radius.height().type() == Percent)
        list->append(cssValuePool->createValue(radius.height().percent(), CSSPrimitiveValue::CSS_PERCENTAGE));
    else
        list->append(zoomAdjustedPixelValue(radius.height().value(), style, cssValuePool));
    return list.release();
}

static LayoutRect sizingBox(RenderObject* renderer)
{
    if (!renderer->isBox())
        return LayoutRect();

    RenderBox* box = toRenderBox(renderer);
    return box->style()->boxSizing() == CONTENT_BOX ? box->contentBoxRect() : box->borderBoxRect();
}

static inline bool hasCompositedLayer(RenderObject* renderer)
{
    return renderer && renderer->hasLayer() && toRenderBoxModelObject(renderer)->layer()->isComposited();
}

static PassRefPtr<CSSValue> computedTransform(RenderObject* renderer, const RenderStyle* style, CSSValuePool* cssValuePool)
{
    if (!renderer || style->transform().operations().isEmpty())
        return cssValuePool->createIdentifierValue(CSSValueNone);

    IntRect box = sizingBox(renderer);

    TransformationMatrix transform;
    style->applyTransform(transform, box.size(), RenderStyle::ExcludeTransformOrigin);
    // Note that this does not flatten to an affine transform if ENABLE(3D_RENDERING) is off, by design.

    RefPtr<WebKitCSSTransformValue> transformVal;

    // FIXME: Need to print out individual functions (https://bugs.webkit.org/show_bug.cgi?id=23924)
    if (transform.isAffine()) {
        transformVal = WebKitCSSTransformValue::create(WebKitCSSTransformValue::MatrixTransformOperation);

        transformVal->append(cssValuePool->createValue(transform.a(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(cssValuePool->createValue(transform.b(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(cssValuePool->createValue(transform.c(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(cssValuePool->createValue(transform.d(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(zoomAdjustedNumberValue(transform.e(), style, cssValuePool));
        transformVal->append(zoomAdjustedNumberValue(transform.f(), style, cssValuePool));
    } else {
        transformVal = WebKitCSSTransformValue::create(WebKitCSSTransformValue::Matrix3DTransformOperation);

        transformVal->append(cssValuePool->createValue(transform.m11(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(cssValuePool->createValue(transform.m12(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(cssValuePool->createValue(transform.m13(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(cssValuePool->createValue(transform.m14(), CSSPrimitiveValue::CSS_NUMBER));

        transformVal->append(cssValuePool->createValue(transform.m21(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(cssValuePool->createValue(transform.m22(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(cssValuePool->createValue(transform.m23(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(cssValuePool->createValue(transform.m24(), CSSPrimitiveValue::CSS_NUMBER));

        transformVal->append(cssValuePool->createValue(transform.m31(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(cssValuePool->createValue(transform.m32(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(cssValuePool->createValue(transform.m33(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(cssValuePool->createValue(transform.m34(), CSSPrimitiveValue::CSS_NUMBER));

        transformVal->append(zoomAdjustedNumberValue(transform.m41(), style, cssValuePool));
        transformVal->append(zoomAdjustedNumberValue(transform.m42(), style, cssValuePool));
        transformVal->append(zoomAdjustedNumberValue(transform.m43(), style, cssValuePool));
        transformVal->append(cssValuePool->createValue(transform.m44(), CSSPrimitiveValue::CSS_NUMBER));
    }

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(transformVal);

    return list.release();
}

#if ENABLE(CSS_FILTERS)
PassRefPtr<CSSValue> CSSComputedStyleDeclaration::valueForFilter(RenderStyle* style) const
{
    CSSValuePool* cssValuePool = m_node->document()->cssValuePool().get();

    if (style->filter().operations().isEmpty())
        return cssValuePool->createIdentifierValue(CSSValueNone);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    RefPtr<WebKitCSSFilterValue> filterValue;

    Vector<RefPtr<FilterOperation> >::const_iterator end = style->filter().operations().end();
    for (Vector<RefPtr<FilterOperation> >::const_iterator it = style->filter().operations().begin(); it != end; ++it) {
        FilterOperation* filterOperation = (*it).get();
        switch (filterOperation->getOperationType()) {
        case FilterOperation::REFERENCE: {
            ReferenceFilterOperation* referenceOperation = static_cast<ReferenceFilterOperation*>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::ReferenceFilterOperation);
            filterValue->append(cssValuePool->createValue(referenceOperation->reference(), CSSPrimitiveValue::CSS_STRING));
            break;
        }
        case FilterOperation::GRAYSCALE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = static_cast<BasicColorMatrixFilterOperation*>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::GrayscaleFilterOperation);
            filterValue->append(cssValuePool->createValue(colorMatrixOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::SEPIA: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = static_cast<BasicColorMatrixFilterOperation*>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::SepiaFilterOperation);
            filterValue->append(cssValuePool->createValue(colorMatrixOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::SATURATE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = static_cast<BasicColorMatrixFilterOperation*>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::SaturateFilterOperation);
            filterValue->append(cssValuePool->createValue(colorMatrixOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = static_cast<BasicColorMatrixFilterOperation*>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::HueRotateFilterOperation);
            filterValue->append(cssValuePool->createValue(colorMatrixOperation->amount(), CSSPrimitiveValue::CSS_DEG));
            break;
        }
        case FilterOperation::INVERT: {
            BasicComponentTransferFilterOperation* componentTransferOperation = static_cast<BasicComponentTransferFilterOperation*>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::InvertFilterOperation);
            filterValue->append(cssValuePool->createValue(componentTransferOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::OPACITY: {
            BasicComponentTransferFilterOperation* componentTransferOperation = static_cast<BasicComponentTransferFilterOperation*>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::OpacityFilterOperation);
            filterValue->append(cssValuePool->createValue(componentTransferOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::GAMMA: {
            GammaFilterOperation* gammaOperation = static_cast<GammaFilterOperation*>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::GammaFilterOperation);
            filterValue->append(cssValuePool->createValue(gammaOperation->amplitude(), CSSPrimitiveValue::CSS_NUMBER));
            filterValue->append(cssValuePool->createValue(gammaOperation->exponent(), CSSPrimitiveValue::CSS_NUMBER));
            filterValue->append(cssValuePool->createValue(gammaOperation->offset(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::BLUR: {
            BlurFilterOperation* blurOperation = static_cast<BlurFilterOperation*>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::BlurFilterOperation);
            filterValue->append(zoomAdjustedPixelValue(blurOperation->stdDeviationX().value(), style, cssValuePool));
            filterValue->append(zoomAdjustedPixelValue(blurOperation->stdDeviationY().value(), style, cssValuePool));
            break;
        }
        case FilterOperation::SHARPEN: {
            SharpenFilterOperation* sharpenOperation = static_cast<SharpenFilterOperation*>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::SharpenFilterOperation);
            filterValue->append(cssValuePool->createValue(sharpenOperation->amount(), CSSPrimitiveValue::CSS_NUMBER));
            filterValue->append(zoomAdjustedPixelValue(sharpenOperation->radius().value(), style, cssValuePool));
            filterValue->append(cssValuePool->createValue(sharpenOperation->threshold(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            DropShadowFilterOperation* dropShadowOperation = static_cast<DropShadowFilterOperation*>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::DropShadowFilterOperation);
            // We want our computed style to look like that of a text shadow (has neither spread nor inset style).
            ShadowData shadowData = ShadowData(dropShadowOperation->x(), dropShadowOperation->y(), dropShadowOperation->stdDeviation(), 0, Normal, false, dropShadowOperation->color());
            filterValue->append(valueForShadow(&shadowData, CSSPropertyTextShadow, style));
            break;
        }
#if ENABLE(CSS_SHADERS)
        case FilterOperation::CUSTOM: {
            CustomFilterOperation* customOperation = static_cast<CustomFilterOperation*>(filterOperation);
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::CustomFilterOperation);
            
            // The output should be verbose, even if the values are the default ones.
            
            RefPtr<CSSValueList> shadersList = CSSValueList::createSpaceSeparated();
            if (customOperation->vertexShader())
                shadersList->append(customOperation->vertexShader()->cssValue());
            else
                shadersList->append(cssValuePool->createIdentifierValue(CSSValueNone));
            if (customOperation->fragmentShader())
                shadersList->append(customOperation->fragmentShader()->cssValue());
            else
                shadersList->append(cssValuePool->createIdentifierValue(CSSValueNone));
            filterValue->append(shadersList.release());
            
            RefPtr<CSSValueList> meshParameters = CSSValueList::createSpaceSeparated();
            meshParameters->append(cssValuePool->createValue(customOperation->meshRows(), CSSPrimitiveValue::CSS_NUMBER));
            meshParameters->append(cssValuePool->createValue(customOperation->meshColumns(), CSSPrimitiveValue::CSS_NUMBER));
            meshParameters->append(cssValuePool->createValue(customOperation->meshBoxType()));
            
            // FIXME: The specification doesn't have any "attached" identifier. Should we add one?
            // https://bugs.webkit.org/show_bug.cgi?id=72700
            if (customOperation->meshType() == CustomFilterOperation::DETACHED)
                meshParameters->append(cssValuePool->createIdentifierValue(CSSValueDetached));
            
            filterValue->append(meshParameters.release());
            
            break;
        }
#endif
        default:
            filterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::UnknownFilterOperation);
            break;
        }
        list->append(filterValue);
    }

    return list.release();
}
#endif

#if ENABLE(CSS_GRID_LAYOUT)
static PassRefPtr<CSSValue> valueForGridTrackBreadth(const Length& trackLength, const RenderStyle* style, CSSValuePool* cssValuePool)
{
    if (trackLength.isPercent())
        return cssValuePool->createValue(trackLength);
    if (trackLength.isAuto())
        return cssValuePool->createIdentifierValue(CSSValueAuto);
    return zoomAdjustedPixelValue(trackLength.value(), style, cssValuePool);
}

static PassRefPtr<CSSValue> valueForGridTrackList(const Vector<Length>& trackLengths, const RenderStyle* style, CSSValuePool* cssValuePool)
{
    // We should have at least an element!
    ASSERT(trackLengths.size());

    // Handle the 'none' case here.
    if (trackLengths.size() == 1 && trackLengths[0].isUndefined())
        return cssValuePool->createIdentifierValue(CSSValueNone);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (size_t i = 0; i < trackLengths.size(); ++i)
        list->append(valueForGridTrackBreadth(trackLengths[i], style, cssValuePool));
    return list.release();
}
#endif

static PassRefPtr<CSSValue> getDelayValue(const AnimationList* animList, CSSValuePool* cssValuePool)
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list->append(cssValuePool->createValue(animList->animation(i)->delay(), CSSPrimitiveValue::CSS_S));
    } else {
        // Note that initialAnimationDelay() is used for both transitions and animations
        list->append(cssValuePool->createValue(Animation::initialAnimationDelay(), CSSPrimitiveValue::CSS_S));
    }
    return list.release();
}

static PassRefPtr<CSSValue> getDurationValue(const AnimationList* animList, CSSValuePool* cssValuePool)
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list->append(cssValuePool->createValue(animList->animation(i)->duration(), CSSPrimitiveValue::CSS_S));
    } else {
        // Note that initialAnimationDuration() is used for both transitions and animations
        list->append(cssValuePool->createValue(Animation::initialAnimationDuration(), CSSPrimitiveValue::CSS_S));
    }
    return list.release();
}

static PassRefPtr<CSSValue> getTimingFunctionValue(const AnimationList* animList)
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i) {
            const TimingFunction* tf = animList->animation(i)->timingFunction().get();
            if (tf->isCubicBezierTimingFunction()) {
                const CubicBezierTimingFunction* ctf = static_cast<const CubicBezierTimingFunction*>(tf);
                list->append(CSSCubicBezierTimingFunctionValue::create(ctf->x1(), ctf->y1(), ctf->x2(), ctf->y2()));
            } else if (tf->isStepsTimingFunction()) {
                const StepsTimingFunction* stf = static_cast<const StepsTimingFunction*>(tf);
                list->append(CSSStepsTimingFunctionValue::create(stf->numberOfSteps(), stf->stepAtStart()));
            } else {
                list->append(CSSLinearTimingFunctionValue::create());
            }
        }
    } else {
        // Note that initialAnimationTimingFunction() is used for both transitions and animations
        RefPtr<TimingFunction> tf = Animation::initialAnimationTimingFunction();
        if (tf->isCubicBezierTimingFunction()) {
            const CubicBezierTimingFunction* ctf = static_cast<const CubicBezierTimingFunction*>(tf.get());
            list->append(CSSCubicBezierTimingFunctionValue::create(ctf->x1(), ctf->y1(), ctf->x2(), ctf->y2()));
        } else if (tf->isStepsTimingFunction()) {
            const StepsTimingFunction* stf = static_cast<const StepsTimingFunction*>(tf.get());
            list->append(CSSStepsTimingFunctionValue::create(stf->numberOfSteps(), stf->stepAtStart()));
        } else {
            list->append(CSSLinearTimingFunctionValue::create());
        }
    }
    return list.release();
}

static PassRefPtr<CSSValue> createLineBoxContainValue(CSSValuePool* cssValuePool, unsigned lineBoxContain)
{
    if (!lineBoxContain)
        return cssValuePool->createIdentifierValue(CSSValueNone);
    return CSSLineBoxContainValue::create(lineBoxContain);
}

CSSComputedStyleDeclaration::CSSComputedStyleDeclaration(PassRefPtr<Node> n, bool allowVisitedStyle, const String& pseudoElementName)
    : m_node(n)
    , m_allowVisitedStyle(allowVisitedStyle)
{
    unsigned nameWithoutColonsStart = pseudoElementName[0] == ':' ? (pseudoElementName[1] == ':' ? 2 : 1) : 0;
    m_pseudoElementSpecifier = CSSSelector::pseudoId(CSSSelector::parsePseudoType(
        AtomicString(pseudoElementName.substring(nameWithoutColonsStart))));
}

CSSComputedStyleDeclaration::~CSSComputedStyleDeclaration()
{
}

String CSSComputedStyleDeclaration::cssText() const
{
    String result("");

    for (unsigned i = 0; i < numComputedProperties; i++) {
        if (i)
            result += " ";
        result += getPropertyName(static_cast<CSSPropertyID>(computedProperties[i]));
        result += ": ";
        result += getPropertyValue(computedProperties[i]);
        result += ";";
    }

    return result;
}

void CSSComputedStyleDeclaration::setCssText(const String&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

static int cssIdentifierForFontSizeKeyword(int keywordSize)
{
    ASSERT_ARG(keywordSize, keywordSize);
    ASSERT_ARG(keywordSize, keywordSize <= 8);
    return CSSValueXxSmall + keywordSize - 1;
}

PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getFontSizeCSSValuePreferringKeyword() const
{
    if (!m_node)
        return 0;

    m_node->document()->updateLayoutIgnorePendingStylesheets();

    RefPtr<RenderStyle> style = m_node->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return 0;

    CSSValuePool* cssValuePool = m_node->document()->cssValuePool().get();

    if (int keywordSize = style->fontDescription().keywordSize())
        return cssValuePool->createIdentifierValue(cssIdentifierForFontSizeKeyword(keywordSize));


    return zoomAdjustedPixelValue(style->fontDescription().computedPixelSize(), style.get(), cssValuePool);
}

bool CSSComputedStyleDeclaration::useFixedFontDefaultSize() const
{
    if (!m_node)
        return false;

    RefPtr<RenderStyle> style = m_node->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return false;

    return style->fontDescription().useFixedDefaultSize();
}

PassRefPtr<CSSValue> CSSComputedStyleDeclaration::valueForShadow(const ShadowData* shadow, int id, RenderStyle* style) const
{
    CSSValuePool* cssValuePool = m_node->document()->cssValuePool().get();
    if (!shadow)
        return cssValuePool->createIdentifierValue(CSSValueNone);

    CSSPropertyID propertyID = static_cast<CSSPropertyID>(id);

    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    for (const ShadowData* s = shadow; s; s = s->next()) {
        RefPtr<CSSPrimitiveValue> x = zoomAdjustedPixelValue(s->x(), style, cssValuePool);
        RefPtr<CSSPrimitiveValue> y = zoomAdjustedPixelValue(s->y(), style, cssValuePool);
        RefPtr<CSSPrimitiveValue> blur = zoomAdjustedPixelValue(s->blur(), style, cssValuePool);
        RefPtr<CSSPrimitiveValue> spread = propertyID == CSSPropertyTextShadow ? PassRefPtr<CSSPrimitiveValue>() : zoomAdjustedPixelValue(s->spread(), style, cssValuePool);
        RefPtr<CSSPrimitiveValue> style = propertyID == CSSPropertyTextShadow || s->style() == Normal ? PassRefPtr<CSSPrimitiveValue>() : cssValuePool->createIdentifierValue(CSSValueInset);
        RefPtr<CSSPrimitiveValue> color = cssValuePool->createColorValue(s->color().rgb());
        list->prepend(ShadowValue::create(x.release(), y.release(), blur.release(), spread.release(), style.release(), color.release()));
    }
    return list.release();
}

PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(int propertyID) const
{
    return getPropertyCSSValue(propertyID, UpdateLayout);
}

static int identifierForFamily(const AtomicString& family)
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
    return 0;
}

static PassRefPtr<CSSPrimitiveValue> valueForFamily(const AtomicString& family, CSSValuePool* cssValuePool)
{
    if (int familyIdentifier = identifierForFamily(family))
        return cssValuePool->createIdentifierValue(familyIdentifier);
    return cssValuePool->createValue(family.string(), CSSPrimitiveValue::CSS_STRING);
}

static PassRefPtr<CSSValue> renderTextDecorationFlagsToCSSValue(int textDecoration, CSSValuePool* cssValuePool)
{
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (textDecoration & UNDERLINE)
        list->append(cssValuePool->createIdentifierValue(CSSValueUnderline));
    if (textDecoration & OVERLINE)
        list->append(cssValuePool->createIdentifierValue(CSSValueOverline));
    if (textDecoration & LINE_THROUGH)
        list->append(cssValuePool->createIdentifierValue(CSSValueLineThrough));
    if (textDecoration & BLINK)
        list->append(cssValuePool->createIdentifierValue(CSSValueBlink));

    if (!list->length())
        return cssValuePool->createIdentifierValue(CSSValueNone);
    return list;
}

static PassRefPtr<CSSValue> fillRepeatToCSSValue(EFillRepeat xRepeat, EFillRepeat yRepeat, CSSValuePool* cssValuePool)
{
    // For backwards compatibility, if both values are equal, just return one of them. And
    // if the two values are equivalent to repeat-x or repeat-y, just return the shorthand.
    if (xRepeat == yRepeat)
        return cssValuePool->createValue(xRepeat);
    if (xRepeat == RepeatFill && yRepeat == NoRepeatFill)
        return cssValuePool->createIdentifierValue(CSSValueRepeatX);
    if (xRepeat == NoRepeatFill && yRepeat == RepeatFill)
        return cssValuePool->createIdentifierValue(CSSValueRepeatY);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(cssValuePool->createValue(xRepeat));
    list->append(cssValuePool->createValue(yRepeat));
    return list.release();
}

static PassRefPtr<CSSValue> fillSizeToCSSValue(const FillSize& fillSize, CSSValuePool* cssValuePool)
{
    if (fillSize.type == Contain)
        return cssValuePool->createIdentifierValue(CSSValueContain);

    if (fillSize.type == Cover)
        return cssValuePool->createIdentifierValue(CSSValueCover);

    if (fillSize.size.height().isAuto())
        return cssValuePool->createValue(fillSize.size.width());

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(cssValuePool->createValue(fillSize.size.width()));
    list->append(cssValuePool->createValue(fillSize.size.height()));
    return list.release();
}

static PassRefPtr<CSSValue> contentToCSSValue(const RenderStyle* style, CSSValuePool* cssValuePool)
{
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (const ContentData* contentData = style->contentData(); contentData; contentData = contentData->next()) {
        if (contentData->isCounter()) {
            const CounterContent* counter = static_cast<const CounterContentData*>(contentData)->counter();
            ASSERT(counter);
            list->append(cssValuePool->createValue(counter->identifier(), CSSPrimitiveValue::CSS_COUNTER_NAME));
        } else if (contentData->isImage()) {
            const StyleImage* image = static_cast<const ImageContentData*>(contentData)->image();
            ASSERT(image);
            list->append(image->cssValue());
        } else if (contentData->isText())
            list->append(cssValuePool->createValue(static_cast<const TextContentData*>(contentData)->text(), CSSPrimitiveValue::CSS_STRING));
    }
    if (!style->regionThread().isNull())
        list->append(cssValuePool->createValue(style->regionThread(), CSSPrimitiveValue::CSS_STRING));
    return list.release();
}

static PassRefPtr<CSSValue> counterToCSSValue(const RenderStyle* style, int propertyID, CSSValuePool* cssValuePool)
{
    const CounterDirectiveMap* map = style->counterDirectives();
    if (!map)
        return 0;

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (CounterDirectiveMap::const_iterator it = map->begin(); it != map->end(); ++it) {
        list->append(cssValuePool->createValue(it->first.get(), CSSPrimitiveValue::CSS_STRING));
        short number = propertyID == CSSPropertyCounterIncrement ? it->second.m_incrementValue : it->second.m_resetValue;
        list->append(cssValuePool->createValue((double)number, CSSPrimitiveValue::CSS_NUMBER));
    }
    return list.release();
}

static void logUnimplementedPropertyID(int propertyID)
{
    DEFINE_STATIC_LOCAL(HashSet<int>, propertyIDSet, ());
    if (!propertyIDSet.add(propertyID).second)
        return;

    LOG_ERROR("WebKit does not yet implement getComputedStyle for '%s'.", getPropertyName(static_cast<CSSPropertyID>(propertyID)));
}

static PassRefPtr<CSSValueList> fontFamilyFromStyle(RenderStyle* style, CSSValuePool* cssValuePool)
{
    const FontFamily& firstFamily = style->fontDescription().family();
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    for (const FontFamily* family = &firstFamily; family; family = family->next())
        list->append(valueForFamily(family->family(), cssValuePool));
    return list.release();
}

static PassRefPtr<CSSPrimitiveValue> lineHeightFromStyle(RenderStyle* style, CSSValuePool* cssValuePool)
{
    Length length = style->lineHeight();
    if (length.isNegative())
        return cssValuePool->createIdentifierValue(CSSValueNormal);
    if (length.isPercent())
        // This is imperfect, because it doesn't include the zoom factor and the real computation
        // for how high to be in pixels does include things like minimum font size and the zoom factor.
        // On the other hand, since font-size doesn't include the zoom factor, we really can't do
        // that here either.
        return zoomAdjustedPixelValue(static_cast<int>(length.percent() * style->fontDescription().specifiedSize()) / 100, style, cssValuePool);
    return zoomAdjustedPixelValue(length.value(), style, cssValuePool);
}

static PassRefPtr<CSSPrimitiveValue> fontSizeFromStyle(RenderStyle* style, CSSValuePool* cssValuePool)
{
    return zoomAdjustedPixelValue(style->fontDescription().computedPixelSize(), style, cssValuePool);
}

static PassRefPtr<CSSPrimitiveValue> fontStyleFromStyle(RenderStyle* style, CSSValuePool* cssValuePool)
{
    if (style->fontDescription().italic())
        return cssValuePool->createIdentifierValue(CSSValueItalic);
    return cssValuePool->createIdentifierValue(CSSValueNormal);
}

static PassRefPtr<CSSPrimitiveValue> fontVariantFromStyle(RenderStyle* style, CSSValuePool* cssValuePool)
{
    if (style->fontDescription().smallCaps())
        return cssValuePool->createIdentifierValue(CSSValueSmallCaps);
    return cssValuePool->createIdentifierValue(CSSValueNormal);
}

static PassRefPtr<CSSPrimitiveValue> fontWeightFromStyle(RenderStyle* style, CSSValuePool* cssValuePool)
{
    switch (style->fontDescription().weight()) {
    case FontWeight100:
        return cssValuePool->createIdentifierValue(CSSValue100);
    case FontWeight200:
        return cssValuePool->createIdentifierValue(CSSValue200);
    case FontWeight300:
        return cssValuePool->createIdentifierValue(CSSValue300);
    case FontWeightNormal:
        return cssValuePool->createIdentifierValue(CSSValueNormal);
    case FontWeight500:
        return cssValuePool->createIdentifierValue(CSSValue500);
    case FontWeight600:
        return cssValuePool->createIdentifierValue(CSSValue600);
    case FontWeightBold:
        return cssValuePool->createIdentifierValue(CSSValueBold);
    case FontWeight800:
        return cssValuePool->createIdentifierValue(CSSValue800);
    case FontWeight900:
        return cssValuePool->createIdentifierValue(CSSValue900);
    }
    ASSERT_NOT_REACHED();
    return cssValuePool->createIdentifierValue(CSSValueNormal);
}

PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(int propertyID, EUpdateLayout updateLayout) const
{
    Node* node = m_node.get();
    if (!node)
        return 0;

    // Make sure our layout is up to date before we allow a query on these attributes.
    if (updateLayout)
        node->document()->updateLayoutIgnorePendingStylesheets();

    RenderObject* renderer = node->renderer();

    RefPtr<RenderStyle> style;
    if (renderer && hasCompositedLayer(renderer) && AnimationController::supportsAcceleratedAnimationOfProperty(static_cast<CSSPropertyID>(propertyID))) {
        style = renderer->animation()->getAnimatedStyleForRenderer(renderer);
        if (m_pseudoElementSpecifier) {
            // FIXME: This cached pseudo style will only exist if the animation has been run at least once.
            style = style->getCachedPseudoStyle(m_pseudoElementSpecifier);
        }
    } else
        style = node->computedStyle(m_pseudoElementSpecifier);

    if (!style)
        return 0;

    CSSValuePool* cssValuePool = node->document()->cssValuePool().get();

    propertyID = CSSProperty::resolveDirectionAwareProperty(propertyID, style->direction(), style->writingMode());

    switch (static_cast<CSSPropertyID>(propertyID)) {
        case CSSPropertyInvalid:
            break;

        case CSSPropertyBackgroundColor:
            return cssValuePool->createColorValue(m_allowVisitedStyle? style->visitedDependentColor(CSSPropertyBackgroundColor).rgb() : style->backgroundColor().rgb());
        case CSSPropertyBackgroundImage:
        case CSSPropertyWebkitMaskImage: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskImage ? style->maskLayers() : style->backgroundLayers();
            if (!layers)
                return cssValuePool->createIdentifierValue(CSSValueNone);

            if (!layers->next()) {
                if (layers->image())
                    return layers->image()->cssValue();

                return cssValuePool->createIdentifierValue(CSSValueNone);
            }

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next()) {
                if (currLayer->image())
                    list->append(currLayer->image()->cssValue());
                else
                    list->append(cssValuePool->createIdentifierValue(CSSValueNone));
            }
            return list.release();
        }
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize:
        case CSSPropertyWebkitMaskSize: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskSize ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return fillSizeToCSSValue(layers->size(), cssValuePool);

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(fillSizeToCSSValue(currLayer->size(), cssValuePool));

            return list.release();
        }
        case CSSPropertyBackgroundRepeat:
        case CSSPropertyWebkitMaskRepeat: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskRepeat ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return fillRepeatToCSSValue(layers->repeatX(), layers->repeatY(), cssValuePool);

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(fillRepeatToCSSValue(currLayer->repeatX(), currLayer->repeatY(), cssValuePool));

            return list.release();
        }
        case CSSPropertyWebkitBackgroundComposite:
        case CSSPropertyWebkitMaskComposite: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskComposite ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool->createValue(layers->composite());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool->createValue(currLayer->composite()));

            return list.release();
        }
        case CSSPropertyBackgroundAttachment:
        case CSSPropertyWebkitMaskAttachment: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskAttachment ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool->createValue(layers->attachment());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool->createValue(currLayer->attachment()));

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
                return cssValuePool->createValue(box);
            }

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next()) {
                EFillBox box = isClip ? currLayer->clip() : currLayer->origin();
                list->append(cssValuePool->createValue(box));
            }

            return list.release();
        }
        case CSSPropertyBackgroundPosition:
        case CSSPropertyWebkitMaskPosition: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskPosition ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next()) {
                RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
                list->append(cssValuePool->createValue(layers->xPosition()));
                list->append(cssValuePool->createValue(layers->yPosition()));
                return list.release();
            }

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next()) {
                RefPtr<CSSValueList> positionList = CSSValueList::createSpaceSeparated();
                positionList->append(cssValuePool->createValue(currLayer->xPosition()));
                positionList->append(cssValuePool->createValue(currLayer->yPosition()));
                list->append(positionList);
            }

            return list.release();
        }
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskPositionX ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool->createValue(layers->xPosition());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool->createValue(currLayer->xPosition()));

            return list.release();
        }
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskPositionY ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool->createValue(layers->yPosition());

            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool->createValue(currLayer->yPosition()));

            return list.release();
        }
        case CSSPropertyBorderCollapse:
            if (style->borderCollapse())
                return cssValuePool->createIdentifierValue(CSSValueCollapse);
            return cssValuePool->createIdentifierValue(CSSValueSeparate);
        case CSSPropertyBorderSpacing: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            list->append(zoomAdjustedPixelValue(style->horizontalBorderSpacing(), style.get(), cssValuePool));
            list->append(zoomAdjustedPixelValue(style->verticalBorderSpacing(), style.get(), cssValuePool));
            return list.release();
        }
        case CSSPropertyWebkitBorderHorizontalSpacing:
            return zoomAdjustedPixelValue(style->horizontalBorderSpacing(), style.get(), cssValuePool);
        case CSSPropertyWebkitBorderVerticalSpacing:
            return zoomAdjustedPixelValue(style->verticalBorderSpacing(), style.get(), cssValuePool);
        case CSSPropertyBorderImageSource:
            if (style->borderImageSource())
                return style->borderImageSource()->cssValue();
            return cssValuePool->createIdentifierValue(CSSValueNone);
        case CSSPropertyBorderTopColor:
            return m_allowVisitedStyle ? cssValuePool->createColorValue(style->visitedDependentColor(CSSPropertyBorderTopColor).rgb()) : currentColorOrValidColor(style.get(), style->borderTopColor());
        case CSSPropertyBorderRightColor:
            return m_allowVisitedStyle ? cssValuePool->createColorValue(style->visitedDependentColor(CSSPropertyBorderRightColor).rgb()) : currentColorOrValidColor(style.get(), style->borderRightColor());
        case CSSPropertyBorderBottomColor:
            return m_allowVisitedStyle ? cssValuePool->createColorValue(style->visitedDependentColor(CSSPropertyBorderBottomColor).rgb()) : currentColorOrValidColor(style.get(), style->borderBottomColor());
        case CSSPropertyBorderLeftColor:
            return m_allowVisitedStyle ? cssValuePool->createColorValue(style->visitedDependentColor(CSSPropertyBorderLeftColor).rgb()) : currentColorOrValidColor(style.get(), style->borderLeftColor());
        case CSSPropertyBorderTopStyle:
            return cssValuePool->createValue(style->borderTopStyle());
        case CSSPropertyBorderRightStyle:
            return cssValuePool->createValue(style->borderRightStyle());
        case CSSPropertyBorderBottomStyle:
            return cssValuePool->createValue(style->borderBottomStyle());
        case CSSPropertyBorderLeftStyle:
            return cssValuePool->createValue(style->borderLeftStyle());
        case CSSPropertyBorderTopWidth:
            return zoomAdjustedPixelValue(style->borderTopWidth(), style.get(), cssValuePool);
        case CSSPropertyBorderRightWidth:
            return zoomAdjustedPixelValue(style->borderRightWidth(), style.get(), cssValuePool);
        case CSSPropertyBorderBottomWidth:
            return zoomAdjustedPixelValue(style->borderBottomWidth(), style.get(), cssValuePool);
        case CSSPropertyBorderLeftWidth:
            return zoomAdjustedPixelValue(style->borderLeftWidth(), style.get(), cssValuePool);
        case CSSPropertyBottom:
            return getPositionOffsetValue(style.get(), CSSPropertyBottom, cssValuePool);
        case CSSPropertyWebkitBoxAlign:
            return cssValuePool->createValue(style->boxAlign());
        case CSSPropertyWebkitBoxDirection:
            return cssValuePool->createValue(style->boxDirection());
        case CSSPropertyWebkitBoxFlex:
            return cssValuePool->createValue(style->boxFlex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxFlexGroup:
            return cssValuePool->createValue(style->boxFlexGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxLines:
            return cssValuePool->createValue(style->boxLines());
        case CSSPropertyWebkitBoxOrdinalGroup:
            return cssValuePool->createValue(style->boxOrdinalGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxOrient:
            return cssValuePool->createValue(style->boxOrient());
        case CSSPropertyWebkitBoxPack: {
            EBoxAlignment boxPack = style->boxPack();
            ASSERT(boxPack != BSTRETCH);
            ASSERT(boxPack != BBASELINE);
            if (boxPack == BJUSTIFY || boxPack== BBASELINE)
                return 0;
            return cssValuePool->createValue(boxPack);
        }
        case CSSPropertyWebkitBoxReflect:
            return valueForReflection(style->boxReflect(), style.get(), cssValuePool);
        case CSSPropertyBoxShadow:
        case CSSPropertyWebkitBoxShadow:
            return valueForShadow(style->boxShadow(), propertyID, style.get());
        case CSSPropertyCaptionSide:
            return cssValuePool->createValue(style->captionSide());
        case CSSPropertyClear:
            return cssValuePool->createValue(style->clear());
        case CSSPropertyColor:
            return cssValuePool->createColorValue(m_allowVisitedStyle ? style->visitedDependentColor(CSSPropertyColor).rgb() : style->color().rgb());
        case CSSPropertyWebkitPrintColorAdjust:
            return cssValuePool->createValue(style->printColorAdjust());
        case CSSPropertyWebkitColumnAxis:
            return cssValuePool->createValue(style->columnAxis());
        case CSSPropertyWebkitColumnCount:
            if (style->hasAutoColumnCount())
                return cssValuePool->createIdentifierValue(CSSValueAuto);
            return cssValuePool->createValue(style->columnCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitColumnGap:
            if (style->hasNormalColumnGap())
                return cssValuePool->createIdentifierValue(CSSValueNormal);
            return zoomAdjustedPixelValue(style->columnGap(), style.get(), cssValuePool);
        case CSSPropertyWebkitColumnRuleColor:
            return m_allowVisitedStyle ? cssValuePool->createColorValue(style->visitedDependentColor(CSSPropertyOutlineColor).rgb()) : currentColorOrValidColor(style.get(), style->columnRuleColor());
        case CSSPropertyWebkitColumnRuleStyle:
            return cssValuePool->createValue(style->columnRuleStyle());
        case CSSPropertyWebkitColumnRuleWidth:
            return zoomAdjustedPixelValue(style->columnRuleWidth(), style.get(), cssValuePool);
        case CSSPropertyWebkitColumnSpan:
            if (style->columnSpan())
                return cssValuePool->createIdentifierValue(CSSValueAll);
            return cssValuePool->createValue(1, CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitColumnBreakAfter:
            return cssValuePool->createValue(style->columnBreakAfter());
        case CSSPropertyWebkitColumnBreakBefore:
            return cssValuePool->createValue(style->columnBreakBefore());
        case CSSPropertyWebkitColumnBreakInside:
            return cssValuePool->createValue(style->columnBreakInside());
        case CSSPropertyWebkitColumnWidth:
            if (style->hasAutoColumnWidth())
                return cssValuePool->createIdentifierValue(CSSValueAuto);
            return zoomAdjustedPixelValue(style->columnWidth(), style.get(), cssValuePool);
        case CSSPropertyWebkitRegionBreakAfter:
            return cssValuePool->createValue(style->regionBreakAfter());
        case CSSPropertyWebkitRegionBreakBefore:
            return cssValuePool->createValue(style->regionBreakBefore());
        case CSSPropertyWebkitRegionBreakInside:
            return cssValuePool->createValue(style->regionBreakInside());
        case CSSPropertyCursor: {
            RefPtr<CSSValueList> list;
            CursorList* cursors = style->cursors();
            if (cursors && cursors->size() > 0) {
                list = CSSValueList::createCommaSeparated();
                for (unsigned i = 0; i < cursors->size(); ++i)
                    if (StyleImage* image = cursors->at(i).image())
                        list->append(image->cssValue());
            }
            RefPtr<CSSValue> value = cssValuePool->createValue(style->cursor());
            if (list) {
                list->append(value);
                return list.release();
            }
            return value.release();
        }
        case CSSPropertyDirection:
            return cssValuePool->createValue(style->direction());
        case CSSPropertyDisplay:
            return cssValuePool->createValue(style->display());
        case CSSPropertyEmptyCells:
            return cssValuePool->createValue(style->emptyCells());
        case CSSPropertyWebkitFlexOrder:
            return cssValuePool->createValue(style->flexOrder(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitFlexPack:
            return cssValuePool->createValue(style->flexPack());
        case CSSPropertyWebkitFlexAlign:
            return cssValuePool->createValue(style->flexAlign());
        case CSSPropertyWebkitFlexDirection:
            return cssValuePool->createValue(style->flexDirection());
        case CSSPropertyWebkitFlexWrap:
            return cssValuePool->createValue(style->flexWrap());
        case CSSPropertyWebkitFlexFlow: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            list->append(cssValuePool->createValue(style->flexDirection()));
            list->append(cssValuePool->createValue(style->flexWrap()));
            return list.release();
        }
        case CSSPropertyFloat:
            return cssValuePool->createValue(style->floating());
        case CSSPropertyFont: {
            RefPtr<FontValue> computedFont = FontValue::create();
            computedFont->style = fontStyleFromStyle(style.get(), cssValuePool);
            computedFont->variant = fontVariantFromStyle(style.get(), cssValuePool);
            computedFont->weight = fontWeightFromStyle(style.get(), cssValuePool);
            computedFont->size = fontSizeFromStyle(style.get(), cssValuePool);
            computedFont->lineHeight = lineHeightFromStyle(style.get(), cssValuePool);
            computedFont->family = fontFamilyFromStyle(style.get(), cssValuePool);
            return computedFont.release();
        }
        case CSSPropertyFontFamily: {
            RefPtr<CSSValueList> fontFamilyList = fontFamilyFromStyle(style.get(), cssValuePool);
            // If there's only a single family, return that as a CSSPrimitiveValue.
            // NOTE: Gecko always returns this as a comma-separated CSSPrimitiveValue string.
            if (fontFamilyList->length() == 1)
                return fontFamilyList->item(0);
            return fontFamilyList.release();
        }
        case CSSPropertyFontSize:
            return fontSizeFromStyle(style.get(), cssValuePool);
        case CSSPropertyFontStyle:
            return fontStyleFromStyle(style.get(), cssValuePool);
        case CSSPropertyFontVariant:
            return fontVariantFromStyle(style.get(), cssValuePool);
        case CSSPropertyFontWeight:
            return fontWeightFromStyle(style.get(), cssValuePool);
        case CSSPropertyWebkitFontFeatureSettings: {
            const FontFeatureSettings* featureSettings = style->fontDescription().featureSettings();
            if (!featureSettings || !featureSettings->size())
                return cssValuePool->createIdentifierValue(CSSValueNormal);
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (unsigned i = 0; i < featureSettings->size(); ++i) {
                const FontFeature& feature = featureSettings->at(i);
                RefPtr<FontFeatureValue> featureValue = FontFeatureValue::create(feature.tag(), feature.value());
                list->append(featureValue.release());
            }
            return list.release();
        }
#if ENABLE(CSS_GRID_LAYOUT)
        case CSSPropertyWebkitGridColumns: {
            return valueForGridTrackList(style->gridColumns(), style.get(), cssValuePool);
        }
        case CSSPropertyWebkitGridRows: {
            return valueForGridTrackList(style->gridRows(), style.get(), cssValuePool);
        }
#endif
        case CSSPropertyHeight:
            if (renderer) {
                // According to http://www.w3.org/TR/CSS2/visudet.html#the-height-property,
                // the "height" property does not apply for non-replaced inline elements.
                if (!renderer->isReplaced() && renderer->isInline())
                    return cssValuePool->createIdentifierValue(CSSValueAuto);
                return zoomAdjustedPixelValue(sizingBox(renderer).height(), style.get(), cssValuePool);
            }
            return zoomAdjustedPixelValueForLength(style->height(), style.get(), cssValuePool);
        case CSSPropertyWebkitHighlight:
            if (style->highlight() == nullAtom)
                return cssValuePool->createIdentifierValue(CSSValueNone);
            return cssValuePool->createValue(style->highlight(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitHyphens:
            return cssValuePool->createValue(style->hyphens());
        case CSSPropertyWebkitHyphenateCharacter:
            if (style->hyphenationString().isNull())
                return cssValuePool->createIdentifierValue(CSSValueAuto);
            return cssValuePool->createValue(style->hyphenationString(), CSSPrimitiveValue::CSS_STRING);
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
                return cssValuePool->createIdentifierValue(CSSValueBorder);
            return cssValuePool->createIdentifierValue(CSSValueLines);
        case CSSPropertyImageRendering:
            return CSSPrimitiveValue::create(style->imageRendering());
        case CSSPropertyLeft:
            return getPositionOffsetValue(style.get(), CSSPropertyLeft, cssValuePool);
        case CSSPropertyLetterSpacing:
            if (!style->letterSpacing())
                return cssValuePool->createIdentifierValue(CSSValueNormal);
            return zoomAdjustedPixelValue(style->letterSpacing(), style.get(), cssValuePool);
        case CSSPropertyWebkitLineClamp:
            if (style->lineClamp().isNone())
                return cssValuePool->createIdentifierValue(CSSValueNone);
            return cssValuePool->createValue(style->lineClamp().value(), style->lineClamp().isPercentage() ? CSSPrimitiveValue::CSS_PERCENTAGE : CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyLineHeight:
            return lineHeightFromStyle(style.get(), cssValuePool);
        case CSSPropertyListStyleImage:
            if (style->listStyleImage())
                return style->listStyleImage()->cssValue();
            return cssValuePool->createIdentifierValue(CSSValueNone);
        case CSSPropertyListStylePosition:
            return cssValuePool->createValue(style->listStylePosition());
        case CSSPropertyListStyleType:
            return cssValuePool->createValue(style->listStyleType());
        case CSSPropertyWebkitLocale:
            if (style->locale().isNull())
                return cssValuePool->createIdentifierValue(CSSValueAuto);
            return cssValuePool->createValue(style->locale(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyMarginTop: {
            Length marginTop = style->marginTop();
            if (marginTop.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(marginTop, style.get(), cssValuePool);
            return zoomAdjustedPixelValue(toRenderBox(renderer)->marginTop(), style.get(), cssValuePool);
        }
        case CSSPropertyMarginRight: {
            Length marginRight = style->marginRight();
            if (marginRight.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(marginRight, style.get(), cssValuePool);
            int value;
            if (marginRight.isPercent())
                // RenderBox gives a marginRight() that is the distance between the right-edge of the child box
                // and the right-edge of the containing box, when display == BLOCK. Let's calculate the absolute
                // value of the specified margin-right % instead of relying on RenderBox's marginRight() value.
                value = marginRight.calcMinValue(toRenderBox(renderer)->containingBlockLogicalWidthForContent());
            else
                value = toRenderBox(renderer)->marginRight();
            return zoomAdjustedPixelValue(value, style.get(), cssValuePool);
        }
        case CSSPropertyMarginBottom: {
            Length marginBottom = style->marginBottom();
            if (marginBottom.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(marginBottom, style.get(), cssValuePool);
            return zoomAdjustedPixelValue(toRenderBox(renderer)->marginBottom(), style.get(), cssValuePool);
        }
        case CSSPropertyMarginLeft: {
            Length marginLeft = style->marginLeft();
            if (marginLeft.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(marginLeft, style.get(), cssValuePool);
            return zoomAdjustedPixelValue(toRenderBox(renderer)->marginLeft(), style.get(), cssValuePool);
        }
        case CSSPropertyWebkitMarqueeDirection:
            return cssValuePool->createValue(style->marqueeDirection());
        case CSSPropertyWebkitMarqueeIncrement:
            return cssValuePool->createValue(style->marqueeIncrement());
        case CSSPropertyWebkitMarqueeRepetition:
            if (style->marqueeLoopCount() < 0)
                return cssValuePool->createIdentifierValue(CSSValueInfinite);
            return cssValuePool->createValue(style->marqueeLoopCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitMarqueeStyle:
            return cssValuePool->createValue(style->marqueeBehavior());
        case CSSPropertyWebkitUserModify:
            return cssValuePool->createValue(style->userModify());
        case CSSPropertyMaxHeight: {
            const Length& maxHeight = style->maxHeight();
            if (maxHeight.isUndefined())
                return cssValuePool->createIdentifierValue(CSSValueNone);
            return cssValuePool->createValue(maxHeight);
        }
        case CSSPropertyMaxWidth: {
            const Length& maxWidth = style->maxWidth();
            if (maxWidth.isUndefined())
                return cssValuePool->createIdentifierValue(CSSValueNone);
            return cssValuePool->createValue(maxWidth);
        }
        case CSSPropertyMinHeight:
            return cssValuePool->createValue(style->minHeight());
        case CSSPropertyMinWidth:
            return cssValuePool->createValue(style->minWidth());
        case CSSPropertyOpacity:
            return cssValuePool->createValue(style->opacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOrphans:
            return cssValuePool->createValue(style->orphans(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOutlineColor:
            return m_allowVisitedStyle ? cssValuePool->createColorValue(style->visitedDependentColor(CSSPropertyOutlineColor).rgb()) : currentColorOrValidColor(style.get(), style->outlineColor());
        case CSSPropertyOutlineOffset:
            return zoomAdjustedPixelValue(style->outlineOffset(), style.get(), cssValuePool);
        case CSSPropertyOutlineStyle:
            if (style->outlineStyleIsAuto())
                return cssValuePool->createIdentifierValue(CSSValueAuto);
            return cssValuePool->createValue(style->outlineStyle());
        case CSSPropertyOutlineWidth:
            return zoomAdjustedPixelValue(style->outlineWidth(), style.get(), cssValuePool);
        case CSSPropertyOverflow:
            return cssValuePool->createValue(max(style->overflowX(), style->overflowY()));
        case CSSPropertyOverflowX:
            return cssValuePool->createValue(style->overflowX());
        case CSSPropertyOverflowY:
            return cssValuePool->createValue(style->overflowY());
        case CSSPropertyPaddingTop:
            if (renderer && renderer->isBox())
                return zoomAdjustedPixelValue(toRenderBox(renderer)->paddingTop(false), style.get(), cssValuePool);
            return cssValuePool->createValue(style->paddingTop());
        case CSSPropertyPaddingRight:
            if (renderer && renderer->isBox())
                return zoomAdjustedPixelValue(toRenderBox(renderer)->paddingRight(false), style.get(), cssValuePool);
            return cssValuePool->createValue(style->paddingRight());
        case CSSPropertyPaddingBottom:
            if (renderer && renderer->isBox())
                return zoomAdjustedPixelValue(toRenderBox(renderer)->paddingBottom(false), style.get(), cssValuePool);
            return cssValuePool->createValue(style->paddingBottom());
        case CSSPropertyPaddingLeft:
            if (renderer && renderer->isBox())
                return zoomAdjustedPixelValue(toRenderBox(renderer)->paddingLeft(false), style.get(), cssValuePool);
            return cssValuePool->createValue(style->paddingLeft());
        case CSSPropertyPageBreakAfter:
            return cssValuePool->createValue(style->pageBreakAfter());
        case CSSPropertyPageBreakBefore:
            return cssValuePool->createValue(style->pageBreakBefore());
        case CSSPropertyPageBreakInside: {
            EPageBreak pageBreak = style->pageBreakInside();
            ASSERT(pageBreak != PBALWAYS);
            if (pageBreak == PBALWAYS)
                return 0;
            return cssValuePool->createValue(style->pageBreakInside());
        }
        case CSSPropertyPosition:
            return cssValuePool->createValue(style->position());
        case CSSPropertyRight:
            return getPositionOffsetValue(style.get(), CSSPropertyRight, cssValuePool);
        case CSSPropertyTableLayout:
            return cssValuePool->createValue(style->tableLayout());
        case CSSPropertyTextAlign:
            return cssValuePool->createValue(style->textAlign());
        case CSSPropertyTextDecoration:
            return renderTextDecorationFlagsToCSSValue(style->textDecoration(), cssValuePool);
        case CSSPropertyWebkitTextDecorationsInEffect:
            return renderTextDecorationFlagsToCSSValue(style->textDecorationsInEffect(), cssValuePool);
        case CSSPropertyWebkitTextFillColor:
            return currentColorOrValidColor(style.get(), style->textFillColor());
        case CSSPropertyWebkitTextEmphasisColor:
            return currentColorOrValidColor(style.get(), style->textEmphasisColor());
        case CSSPropertyWebkitTextEmphasisPosition:
            return cssValuePool->createValue(style->textEmphasisPosition());
        case CSSPropertyWebkitTextEmphasisStyle:
            switch (style->textEmphasisMark()) {
            case TextEmphasisMarkNone:
                return cssValuePool->createIdentifierValue(CSSValueNone);
            case TextEmphasisMarkCustom:
                return cssValuePool->createValue(style->textEmphasisCustomMark(), CSSPrimitiveValue::CSS_STRING);
            case TextEmphasisMarkAuto:
                ASSERT_NOT_REACHED();
                // Fall through
            case TextEmphasisMarkDot:
            case TextEmphasisMarkCircle:
            case TextEmphasisMarkDoubleCircle:
            case TextEmphasisMarkTriangle:
            case TextEmphasisMarkSesame: {
                RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
                list->append(cssValuePool->createValue(style->textEmphasisFill()));
                list->append(cssValuePool->createValue(style->textEmphasisMark()));
                return list.release();
            }
            }
        case CSSPropertyTextIndent:
            return cssValuePool->createValue(style->textIndent());
        case CSSPropertyTextShadow:
            return valueForShadow(style->textShadow(), propertyID, style.get());
        case CSSPropertyTextRendering:
            return cssValuePool->createValue(style->fontDescription().textRenderingMode());
        case CSSPropertyTextOverflow:
            if (style->textOverflow())
                return cssValuePool->createIdentifierValue(CSSValueEllipsis);
            return cssValuePool->createIdentifierValue(CSSValueClip);
        case CSSPropertyWebkitTextSecurity:
            return cssValuePool->createValue(style->textSecurity());
        case CSSPropertyWebkitTextSizeAdjust:
            if (style->textSizeAdjust())
                return cssValuePool->createIdentifierValue(CSSValueAuto);
            return cssValuePool->createIdentifierValue(CSSValueNone);
        case CSSPropertyWebkitTextStrokeColor:
            return currentColorOrValidColor(style.get(), style->textStrokeColor());
        case CSSPropertyWebkitTextStrokeWidth:
            return zoomAdjustedPixelValue(style->textStrokeWidth(), style.get(), cssValuePool);
        case CSSPropertyTextTransform:
            return cssValuePool->createValue(style->textTransform());
        case CSSPropertyTop:
            return getPositionOffsetValue(style.get(), CSSPropertyTop, cssValuePool);
        case CSSPropertyUnicodeBidi:
            return cssValuePool->createValue(style->unicodeBidi());
        case CSSPropertyVerticalAlign:
            switch (style->verticalAlign()) {
                case BASELINE:
                    return cssValuePool->createIdentifierValue(CSSValueBaseline);
                case MIDDLE:
                    return cssValuePool->createIdentifierValue(CSSValueMiddle);
                case SUB:
                    return cssValuePool->createIdentifierValue(CSSValueSub);
                case SUPER:
                    return cssValuePool->createIdentifierValue(CSSValueSuper);
                case TEXT_TOP:
                    return cssValuePool->createIdentifierValue(CSSValueTextTop);
                case TEXT_BOTTOM:
                    return cssValuePool->createIdentifierValue(CSSValueTextBottom);
                case TOP:
                    return cssValuePool->createIdentifierValue(CSSValueTop);
                case BOTTOM:
                    return cssValuePool->createIdentifierValue(CSSValueBottom);
                case BASELINE_MIDDLE:
                    return cssValuePool->createIdentifierValue(CSSValueWebkitBaselineMiddle);
                case LENGTH:
                    return cssValuePool->createValue(style->verticalAlignLength());
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSSPropertyVisibility:
            return cssValuePool->createValue(style->visibility());
        case CSSPropertyWhiteSpace:
            return cssValuePool->createValue(style->whiteSpace());
        case CSSPropertyWidows:
            return cssValuePool->createValue(style->widows(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWidth:
            if (renderer) {
                // According to http://www.w3.org/TR/CSS2/visudet.html#the-width-property,
                // the "width" property does not apply for non-replaced inline elements.
                if (!renderer->isReplaced() && renderer->isInline())
                    return cssValuePool->createIdentifierValue(CSSValueAuto);
                return zoomAdjustedPixelValue(sizingBox(renderer).width(), style.get(), cssValuePool);
            }
            return zoomAdjustedPixelValueForLength(style->width(), style.get(), cssValuePool);
        case CSSPropertyWordBreak:
            return cssValuePool->createValue(style->wordBreak());
        case CSSPropertyWordSpacing:
            return zoomAdjustedPixelValue(style->wordSpacing(), style.get(), cssValuePool);
        case CSSPropertyWordWrap:
            return cssValuePool->createValue(style->wordWrap());
        case CSSPropertyWebkitLineBreak:
            return cssValuePool->createValue(style->khtmlLineBreak());
        case CSSPropertyWebkitNbspMode:
            return cssValuePool->createValue(style->nbspMode());
        case CSSPropertyWebkitMatchNearestMailBlockquoteColor:
            return cssValuePool->createValue(style->matchNearestMailBlockquoteColor());
        case CSSPropertyResize:
            return cssValuePool->createValue(style->resize());
        case CSSPropertyWebkitFontSmoothing:
            return cssValuePool->createValue(style->fontDescription().fontSmoothing());
        case CSSPropertyZIndex:
            if (style->hasAutoZIndex())
                return cssValuePool->createIdentifierValue(CSSValueAuto);
            return cssValuePool->createValue(style->zIndex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyZoom:
            return cssValuePool->createValue(style->zoom(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyBoxSizing:
            if (style->boxSizing() == CONTENT_BOX)
                return cssValuePool->createIdentifierValue(CSSValueContentBox);
            return cssValuePool->createIdentifierValue(CSSValueBorderBox);
#if ENABLE(DASHBOARD_SUPPORT)
        case CSSPropertyWebkitDashboardRegion:
        {
            const Vector<StyleDashboardRegion>& regions = style->dashboardRegions();
            unsigned count = regions.size();
            if (count == 1 && regions[0].type == StyleDashboardRegion::None)
                return cssValuePool->createIdentifierValue(CSSValueNone);

            RefPtr<DashboardRegion> firstRegion;
            DashboardRegion* previousRegion = 0;
            for (unsigned i = 0; i < count; i++) {
                RefPtr<DashboardRegion> region = DashboardRegion::create();
                StyleDashboardRegion styleRegion = regions[i];

                region->m_label = styleRegion.label;
                LengthBox offset = styleRegion.offset;
                region->setTop(zoomAdjustedPixelValue(offset.top().value(), style.get(), cssValuePool));
                region->setRight(zoomAdjustedPixelValue(offset.right().value(), style.get(), cssValuePool));
                region->setBottom(zoomAdjustedPixelValue(offset.bottom().value(), style.get(), cssValuePool));
                region->setLeft(zoomAdjustedPixelValue(offset.left().value(), style.get(), cssValuePool));
                region->m_isRectangle = (styleRegion.type == StyleDashboardRegion::Rectangle);
                region->m_isCircle = (styleRegion.type == StyleDashboardRegion::Circle);

                if (previousRegion)
                    previousRegion->m_next = region;
                else
                    firstRegion = region;
                previousRegion = region.get();
            }
            return cssValuePool->createValue(firstRegion.release());
        }
#endif
        case CSSPropertyWebkitAnimationDelay:
            return getDelayValue(style->animations(), cssValuePool);
        case CSSPropertyWebkitAnimationDirection: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    if (t->animation(i)->direction())
                        list->append(cssValuePool->createIdentifierValue(CSSValueAlternate));
                    else
                        list->append(cssValuePool->createIdentifierValue(CSSValueNormal));
                }
            } else
                list->append(cssValuePool->createIdentifierValue(CSSValueNormal));
            return list.release();
        }
        case CSSPropertyWebkitAnimationDuration:
            return getDurationValue(style->animations(), cssValuePool);
        case CSSPropertyWebkitAnimationFillMode: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    switch (t->animation(i)->fillMode()) {
                    case AnimationFillModeNone:
                        list->append(cssValuePool->createIdentifierValue(CSSValueNone));
                        break;
                    case AnimationFillModeForwards:
                        list->append(cssValuePool->createIdentifierValue(CSSValueForwards));
                        break;
                    case AnimationFillModeBackwards:
                        list->append(cssValuePool->createIdentifierValue(CSSValueBackwards));
                        break;
                    case AnimationFillModeBoth:
                        list->append(cssValuePool->createIdentifierValue(CSSValueBoth));
                        break;
                    }
                }
            } else
                list->append(cssValuePool->createIdentifierValue(CSSValueNone));
            return list.release();
        }
        case CSSPropertyWebkitAnimationIterationCount: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    int iterationCount = t->animation(i)->iterationCount();
                    if (iterationCount == Animation::IterationCountInfinite)
                        list->append(cssValuePool->createIdentifierValue(CSSValueInfinite));
                    else
                        list->append(cssValuePool->createValue(iterationCount, CSSPrimitiveValue::CSS_NUMBER));
                }
            } else
                list->append(cssValuePool->createValue(Animation::initialAnimationIterationCount(), CSSPrimitiveValue::CSS_NUMBER));
            return list.release();
        }
        case CSSPropertyWebkitAnimationName: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i)
                    list->append(cssValuePool->createValue(t->animation(i)->name(), CSSPrimitiveValue::CSS_STRING));
            } else
                list->append(cssValuePool->createIdentifierValue(CSSValueNone));
            return list.release();
        }
        case CSSPropertyWebkitAnimationPlayState: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    int prop = t->animation(i)->playState();
                    if (prop == AnimPlayStatePlaying)
                        list->append(cssValuePool->createIdentifierValue(CSSValueRunning));
                    else
                        list->append(cssValuePool->createIdentifierValue(CSSValuePaused));
                }
            } else
                list->append(cssValuePool->createIdentifierValue(CSSValueRunning));
            return list.release();
        }
        case CSSPropertyWebkitAnimationTimingFunction:
            return getTimingFunctionValue(style->animations());
        case CSSPropertyWebkitAppearance:
            return cssValuePool->createValue(style->appearance());
        case CSSPropertyWebkitAspectRatio:
            if (!style->hasAspectRatio())
                return cssValuePool->createIdentifierValue(CSSValueNone);
            return CSSAspectRatioValue::create(style->aspectRatioNumerator(), style->aspectRatioDenominator());
        case CSSPropertyWebkitBackfaceVisibility:
            return cssValuePool->createIdentifierValue((style->backfaceVisibility() == BackfaceVisibilityHidden) ? CSSValueHidden : CSSValueVisible);
        case CSSPropertyWebkitBorderImage:
            return valueForNinePieceImage(style->borderImage(), cssValuePool);
        case CSSPropertyBorderImageOutset:
            return valueForNinePieceImageQuad(style->borderImage().outset(), cssValuePool);
        case CSSPropertyBorderImageRepeat:
            return valueForNinePieceImageRepeat(style->borderImage(), cssValuePool);
        case CSSPropertyBorderImageSlice:
            return valueForNinePieceImageSlice(style->borderImage(), cssValuePool);
        case CSSPropertyBorderImageWidth:
            return valueForNinePieceImageQuad(style->borderImage().borderSlices(), cssValuePool);
        case CSSPropertyWebkitMaskBoxImage:
            return valueForNinePieceImage(style->maskBoxImage(), cssValuePool);
        case CSSPropertyWebkitMaskBoxImageOutset:
            return valueForNinePieceImageQuad(style->maskBoxImage().outset(), cssValuePool);
        case CSSPropertyWebkitMaskBoxImageRepeat:
            return valueForNinePieceImageRepeat(style->maskBoxImage(), cssValuePool);
        case CSSPropertyWebkitMaskBoxImageSlice:
            return valueForNinePieceImageSlice(style->maskBoxImage(), cssValuePool);
        case CSSPropertyWebkitMaskBoxImageWidth:
            return valueForNinePieceImageQuad(style->maskBoxImage().borderSlices(), cssValuePool);
        case CSSPropertyWebkitMaskBoxImageSource:
            if (style->maskBoxImageSource())
                return style->maskBoxImageSource()->cssValue();
            return cssValuePool->createIdentifierValue(CSSValueNone);
        case CSSPropertyWebkitFontSizeDelta:
            // Not a real style property -- used by the editing engine -- so has no computed value.
            break;
        case CSSPropertyWebkitMarginBottomCollapse:
        case CSSPropertyWebkitMarginAfterCollapse:
            return cssValuePool->createValue(style->marginAfterCollapse());
        case CSSPropertyWebkitMarginTopCollapse:
        case CSSPropertyWebkitMarginBeforeCollapse:
            return cssValuePool->createValue(style->marginBeforeCollapse());
        case CSSPropertyWebkitPerspective:
            if (!style->hasPerspective())
                return cssValuePool->createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValue(style->perspective(), style.get(), cssValuePool);
        case CSSPropertyWebkitPerspectiveOrigin: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                IntRect box = sizingBox(renderer);
                list->append(zoomAdjustedPixelValue(style->perspectiveOriginX().calcMinValue(box.width()), style.get(), cssValuePool));
                list->append(zoomAdjustedPixelValue(style->perspectiveOriginY().calcMinValue(box.height()), style.get(), cssValuePool));
            }
            else {
                list->append(zoomAdjustedPixelValueForLength(style->perspectiveOriginX(), style.get(), cssValuePool));
                list->append(zoomAdjustedPixelValueForLength(style->perspectiveOriginY(), style.get(), cssValuePool));

            }
            return list.release();
        }
        case CSSPropertyWebkitRtlOrdering:
            return cssValuePool->createIdentifierValue(style->rtlOrdering() ? CSSValueVisual : CSSValueLogical);
#if ENABLE(TOUCH_EVENTS)
        case CSSPropertyWebkitTapHighlightColor:
            return currentColorOrValidColor(style.get(), style->tapHighlightColor());
#endif
        case CSSPropertyWebkitUserDrag:
            return cssValuePool->createValue(style->userDrag());
        case CSSPropertyWebkitUserSelect:
            return cssValuePool->createValue(style->userSelect());
        case CSSPropertyBorderBottomLeftRadius:
            return getBorderRadiusCornerValue(style->borderBottomLeftRadius(), style.get(), cssValuePool);
        case CSSPropertyBorderBottomRightRadius:
            return getBorderRadiusCornerValue(style->borderBottomRightRadius(), style.get(), cssValuePool);
        case CSSPropertyBorderTopLeftRadius:
            return getBorderRadiusCornerValue(style->borderTopLeftRadius(), style.get(), cssValuePool);
        case CSSPropertyBorderTopRightRadius:
            return getBorderRadiusCornerValue(style->borderTopRightRadius(), style.get(), cssValuePool);
        case CSSPropertyClip: {
            if (!style->hasClip())
                return cssValuePool->createIdentifierValue(CSSValueAuto);
            RefPtr<Rect> rect = Rect::create();
            rect->setTop(zoomAdjustedPixelValue(style->clip().top().value(), style.get(), cssValuePool));
            rect->setRight(zoomAdjustedPixelValue(style->clip().right().value(), style.get(), cssValuePool));
            rect->setBottom(zoomAdjustedPixelValue(style->clip().bottom().value(), style.get(), cssValuePool));
            rect->setLeft(zoomAdjustedPixelValue(style->clip().left().value(), style.get(), cssValuePool));
            return cssValuePool->createValue(rect.release());
        }
        case CSSPropertySpeak:
            return cssValuePool->createValue(style->speak());
        case CSSPropertyWebkitTransform:
            return computedTransform(renderer, style.get(), cssValuePool);
        case CSSPropertyWebkitTransformOrigin: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                IntRect box = sizingBox(renderer);
                list->append(zoomAdjustedPixelValue(style->transformOriginX().calcMinValue(box.width()), style.get(), cssValuePool));
                list->append(zoomAdjustedPixelValue(style->transformOriginY().calcMinValue(box.height()), style.get(), cssValuePool));
                if (style->transformOriginZ() != 0)
                    list->append(zoomAdjustedPixelValue(style->transformOriginZ(), style.get(), cssValuePool));
            } else {
                list->append(zoomAdjustedPixelValueForLength(style->transformOriginX(), style.get(), cssValuePool));
                list->append(zoomAdjustedPixelValueForLength(style->transformOriginY(), style.get(), cssValuePool));
                if (style->transformOriginZ() != 0)
                    list->append(zoomAdjustedPixelValue(style->transformOriginZ(), style.get(), cssValuePool));
            }
            return list.release();
        }
        case CSSPropertyWebkitTransformStyle:
            return cssValuePool->createIdentifierValue((style->transformStyle3D() == TransformStyle3DPreserve3D) ? CSSValuePreserve3d : CSSValueFlat);
        case CSSPropertyWebkitTransitionDelay:
            return getDelayValue(style->transitions(), cssValuePool);
        case CSSPropertyWebkitTransitionDuration:
            return getDurationValue(style->transitions(), cssValuePool);
        case CSSPropertyWebkitTransitionProperty: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->transitions();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    int prop = t->animation(i)->property();
                    RefPtr<CSSValue> propertyValue;
                    if (prop == cAnimateNone)
                        propertyValue = cssValuePool->createIdentifierValue(CSSValueNone);
                    else if (prop == cAnimateAll)
                        propertyValue = cssValuePool->createIdentifierValue(CSSValueAll);
                    else
                        propertyValue = cssValuePool->createValue(getPropertyName(static_cast<CSSPropertyID>(prop)), CSSPrimitiveValue::CSS_STRING);
                    list->append(propertyValue);
                }
            } else
                list->append(cssValuePool->createIdentifierValue(CSSValueAll));
            return list.release();
        }
        case CSSPropertyWebkitTransitionTimingFunction:
            return getTimingFunctionValue(style->transitions());
        case CSSPropertyPointerEvents:
            return cssValuePool->createValue(style->pointerEvents());
        case CSSPropertyWebkitColorCorrection:
            return cssValuePool->createValue(style->colorSpace());
        case CSSPropertyWebkitLineGrid:
            if (style->lineGrid().isNull())
                return cssValuePool->createIdentifierValue(CSSValueNone);
            return cssValuePool->createValue(style->lineGrid(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitLineGridSnap:
            return CSSPrimitiveValue::create(style->lineGridSnap());
        case CSSPropertyWebkitWritingMode:
            return cssValuePool->createValue(style->writingMode());
        case CSSPropertyWebkitTextCombine:
            return cssValuePool->createValue(style->textCombine());
        case CSSPropertyWebkitTextOrientation:
            return CSSPrimitiveValue::create(style->fontDescription().textOrientation());
        case CSSPropertyWebkitLineBoxContain:
            return createLineBoxContainValue(cssValuePool, style->lineBoxContain());
        case CSSPropertyContent:
            return contentToCSSValue(style.get(), cssValuePool);
        case CSSPropertyCounterIncrement:
            return counterToCSSValue(style.get(), propertyID, cssValuePool);
        case CSSPropertyCounterReset:
            return counterToCSSValue(style.get(), propertyID, cssValuePool);
        case CSSPropertyWebkitFlowInto:
            if (style->flowThread().isNull())
                return cssValuePool->createIdentifierValue(CSSValueAuto);
            return cssValuePool->createValue(style->flowThread(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitFlowFrom:
            if (style->regionThread().isNull())
                return cssValuePool->createIdentifierValue(CSSValueNone);
            return cssValuePool->createValue(style->regionThread(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitRegionOverflow:
            return cssValuePool->createValue(style->regionOverflow());
        case CSSPropertyWebkitWrapFlow:
            return cssValuePool->createValue(style->wrapFlow());
        case CSSPropertyWebkitWrapMargin:
            return cssValuePool->createValue(style->wrapMargin());
        case CSSPropertyWebkitWrapPadding:
            return cssValuePool->createValue(style->wrapPadding());
        case CSSPropertyWebkitWrapShapeInside:
            if (!style->wrapShapeInside())
                return cssValuePool->createIdentifierValue(CSSValueAuto);
            return cssValuePool->createValue(style->wrapShapeInside());
        case CSSPropertyWebkitWrapShapeOutside:
            if (!style->wrapShapeOutside())
                return cssValuePool->createIdentifierValue(CSSValueAuto);
            return cssValuePool->createValue(style->wrapShapeOutside());
        case CSSPropertyWebkitWrapThrough:
            return cssValuePool->createValue(style->wrapThrough());
#if ENABLE(CSS_FILTERS)
        case CSSPropertyWebkitFilter:
            return valueForFilter(style.get());
#endif
        /* Shorthand properties, currently not supported see bug 13658*/
        case CSSPropertyBackground:
        case CSSPropertyBorder:
        case CSSPropertyBorderBottom:
        case CSSPropertyBorderColor:
        case CSSPropertyBorderLeft:
        case CSSPropertyBorderImage:
        case CSSPropertyBorderRadius:
        case CSSPropertyBorderRight:
        case CSSPropertyBorderStyle:
        case CSSPropertyBorderTop:
        case CSSPropertyBorderWidth:
        case CSSPropertyListStyle:
        case CSSPropertyMargin:
        case CSSPropertyOutline:
        case CSSPropertyPadding:
            break;

        /* Individual properties not part of the spec */
        case CSSPropertyBackgroundRepeatX:
        case CSSPropertyBackgroundRepeatY:
            break;

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
        case CSSPropertyWebkitColumns:
        case CSSPropertyWebkitColumnRule:
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
        case CSSPropertyWebkitTransition:
        case CSSPropertyWebkitWrap:
            break;

#if ENABLE(SVG)
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
        case CSSPropertyShapeRendering:
        case CSSPropertyStroke:
        case CSSPropertyStrokeDasharray:
        case CSSPropertyStrokeDashoffset:
        case CSSPropertyStrokeLinecap:
        case CSSPropertyStrokeLinejoin:
        case CSSPropertyStrokeMiterlimit:
        case CSSPropertyStrokeOpacity:
        case CSSPropertyStrokeWidth:
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
            return getSVGPropertyCSSValue(propertyID, DoNotUpdateLayout);
#endif
    }

    logUnimplementedPropertyID(propertyID);
    return 0;
}

String CSSComputedStyleDeclaration::getPropertyValue(int propertyID) const
{
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    if (value)
        return value->cssText();
    return "";
}

bool CSSComputedStyleDeclaration::getPropertyPriority(int /*propertyID*/) const
{
    // All computed styles have a priority of false (not "important").
    return false;
}

String CSSComputedStyleDeclaration::removeProperty(int /*propertyID*/, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
    return String();
}

void CSSComputedStyleDeclaration::setProperty(int /*propertyID*/, const String& /*value*/, bool /*important*/, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

unsigned CSSComputedStyleDeclaration::virtualLength() const
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
        return "";

    return getPropertyName(static_cast<CSSPropertyID>(computedProperties[i]));
}

bool CSSComputedStyleDeclaration::cssPropertyMatches(const CSSProperty* property) const
{
    if (property->id() == CSSPropertyFontSize && property->value()->isPrimitiveValue() && m_node) {
        m_node->document()->updateLayoutIgnorePendingStylesheets();
        RenderStyle* style = m_node->computedStyle(m_pseudoElementSpecifier);
        if (style && style->fontDescription().keywordSize()) {
            int sizeValue = cssIdentifierForFontSizeKeyword(style->fontDescription().keywordSize());
            CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(property->value());
            if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_IDENT && primitiveValue->getIdent() == sizeValue)
                return true;
        }
    }

    return CSSStyleDeclaration::cssPropertyMatches(property);
}

PassRefPtr<CSSMutableStyleDeclaration> CSSComputedStyleDeclaration::copy() const
{
    return copyPropertiesInSet(computedProperties, numComputedProperties);
}

PassRefPtr<CSSMutableStyleDeclaration> CSSComputedStyleDeclaration::makeMutable()
{
    return copy();
}

} // namespace WebCore
