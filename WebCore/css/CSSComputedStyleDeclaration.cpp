/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
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
#include "CSSBorderImageValue.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSReflectValue.h"
#include "CSSSelector.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueList.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Rect.h"
#include "RenderBox.h"
#include "RenderLayer.h"
#include "ShadowValue.h"
#include "WebKitCSSTransformValue.h"

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
    CSSPropertyWebkitBlockFlow,
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
    CSSPropertyWebkitBoxSizing,
    CSSPropertyWebkitColorCorrection,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakInside,
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
    CSSPropertyWebkitFontSmoothing,
    CSSPropertyWebkitHighlight,
    CSSPropertyWebkitLineBreak,
    CSSPropertyWebkitLineClamp,
    CSSPropertyWebkitMarginBottomCollapse,
    CSSPropertyWebkitMarginTopCollapse,
    CSSPropertyWebkitMarqueeDirection,
    CSSPropertyWebkitMarqueeIncrement,
    CSSPropertyWebkitMarqueeRepetition,
    CSSPropertyWebkitMarqueeStyle,
    CSSPropertyWebkitMaskAttachment,
    CSSPropertyWebkitMaskBoxImage,
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
    CSSPropertyWebkitRtlOrdering,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextFillColor,
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
    CSSPropertyWebkitUserSelect

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
    CSSPropertyImageRendering,
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

const unsigned numComputedProperties = sizeof(computedProperties) / sizeof(computedProperties[0]);

static int valueForRepeatRule(int rule)
{
    switch (rule) {
        case RepeatImageRule:
            return CSSValueRepeat;
        case RoundImageRule:
            return CSSValueRound;
        default:
            return CSSValueStretch;
    }
}
        
static PassRefPtr<CSSValue> valueForNinePieceImage(const NinePieceImage& image)
{
    if (!image.hasImage())
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    
    // Image first.
    RefPtr<CSSValue> imageValue;
    if (image.image())
        imageValue = image.image()->cssValue();
    
    // Create the slices.
    RefPtr<CSSPrimitiveValue> top;
    if (image.slices().top().isPercent())
        top = CSSPrimitiveValue::create(image.slices().top().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        top = CSSPrimitiveValue::create(image.slices().top().value(), CSSPrimitiveValue::CSS_NUMBER);
        
    RefPtr<CSSPrimitiveValue> right;
    if (image.slices().right().isPercent())
        right = CSSPrimitiveValue::create(image.slices().right().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        right = CSSPrimitiveValue::create(image.slices().right().value(), CSSPrimitiveValue::CSS_NUMBER);
        
    RefPtr<CSSPrimitiveValue> bottom;
    if (image.slices().bottom().isPercent())
        bottom = CSSPrimitiveValue::create(image.slices().bottom().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        bottom = CSSPrimitiveValue::create(image.slices().bottom().value(), CSSPrimitiveValue::CSS_NUMBER);
    
    RefPtr<CSSPrimitiveValue> left;
    if (image.slices().left().isPercent())
        left = CSSPrimitiveValue::create(image.slices().left().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        left = CSSPrimitiveValue::create(image.slices().left().value(), CSSPrimitiveValue::CSS_NUMBER);

    RefPtr<Rect> rect = Rect::create();
    rect->setTop(top);
    rect->setRight(right);
    rect->setBottom(bottom);
    rect->setLeft(left);

    return CSSBorderImageValue::create(imageValue, rect, valueForRepeatRule(image.horizontalRule()), valueForRepeatRule(image.verticalRule()));
}

inline static PassRefPtr<CSSPrimitiveValue> zoomAdjustedPixelValue(int value, const RenderStyle* style)
{
    return CSSPrimitiveValue::create(adjustForAbsoluteZoom(value, style), CSSPrimitiveValue::CSS_PX);
}

inline static PassRefPtr<CSSPrimitiveValue> zoomAdjustedNumberValue(double value, const RenderStyle* style)
{
    return CSSPrimitiveValue::create(value / style->effectiveZoom(), CSSPrimitiveValue::CSS_NUMBER);
}

static PassRefPtr<CSSValue> valueForReflection(const StyleReflection* reflection, const RenderStyle* style)
{
    if (!reflection)
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);

    RefPtr<CSSPrimitiveValue> offset;
    if (reflection->offset().isPercent())
        offset = CSSPrimitiveValue::create(reflection->offset().percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        offset = zoomAdjustedPixelValue(reflection->offset().value(), style);
    
    return CSSReflectValue::create(reflection->direction(), offset.release(), valueForNinePieceImage(reflection->mask()));
}

static PassRefPtr<CSSValue> getPositionOffsetValue(RenderStyle* style, int propertyID)
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
            return zoomAdjustedPixelValue(l.value(), style);
        return CSSPrimitiveValue::create(l);
    }

    if (style->position() == RelativePosition)
        // FIXME: It's not enough to simply return "auto" values for one offset if the other side is defined.
        // In other words if left is auto and right is not auto, then left's computed value is negative right().
        // So we should get the opposite length unit and see if it is auto.
        return CSSPrimitiveValue::create(l);

    return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
}

PassRefPtr<CSSPrimitiveValue> CSSComputedStyleDeclaration::currentColorOrValidColor(RenderStyle* style, const Color& color) const
{
    // This function does NOT look at visited information, so that computed style doesn't expose that.
    if (!color.isValid())
        return CSSPrimitiveValue::createColor(style->color().rgb());
    return CSSPrimitiveValue::createColor(color.rgb());
}

static PassRefPtr<CSSValue> getBorderRadiusCornerValue(LengthSize radius, const RenderStyle* style)
{
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (radius.width() == radius.height()) {
        if (radius.width().type() == Percent)
            return CSSPrimitiveValue::create(radius.width().percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
        return zoomAdjustedPixelValue(radius.width().value(), style);
    }
    if (radius.width().type() == Percent)
        list->append(CSSPrimitiveValue::create(radius.width().percent(), CSSPrimitiveValue::CSS_PERCENTAGE));
    else
        list->append(zoomAdjustedPixelValue(radius.width().value(), style));
    if (radius.height().type() == Percent)
        list->append(CSSPrimitiveValue::create(radius.height().percent(), CSSPrimitiveValue::CSS_PERCENTAGE));
    else
        list->append(zoomAdjustedPixelValue(radius.height().value(), style));
    return list.release();
}

static IntRect sizingBox(RenderObject* renderer)
{
    if (!renderer->isBox())
        return IntRect();
    
    RenderBox* box = toRenderBox(renderer);
    return box->style()->boxSizing() == CONTENT_BOX ? box->contentBoxRect() : box->borderBoxRect();
}

static inline bool hasCompositedLayer(RenderObject* renderer)
{
    return renderer && renderer->hasLayer() && toRenderBoxModelObject(renderer)->layer()->isComposited();
}

static PassRefPtr<CSSValue> computedTransform(RenderObject* renderer, const RenderStyle* style)
{
    if (!renderer || style->transform().operations().isEmpty())
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    
    IntRect box = sizingBox(renderer);

    TransformationMatrix transform;
    style->applyTransform(transform, box.size(), RenderStyle::ExcludeTransformOrigin);
    // Note that this does not flatten to an affine transform if ENABLE(3D_RENDERING) is off, by design.

    RefPtr<WebKitCSSTransformValue> transformVal;

    // FIXME: Need to print out individual functions (https://bugs.webkit.org/show_bug.cgi?id=23924)
    if (transform.isAffine()) {
        transformVal = WebKitCSSTransformValue::create(WebKitCSSTransformValue::MatrixTransformOperation);

        transformVal->append(CSSPrimitiveValue::create(transform.a(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.b(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.c(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.d(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(zoomAdjustedNumberValue(transform.e(), style));
        transformVal->append(zoomAdjustedNumberValue(transform.f(), style));
    } else {
        transformVal = WebKitCSSTransformValue::create(WebKitCSSTransformValue::Matrix3DTransformOperation);

        transformVal->append(CSSPrimitiveValue::create(transform.m11(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m12(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m13(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m14(), CSSPrimitiveValue::CSS_NUMBER));

        transformVal->append(CSSPrimitiveValue::create(transform.m21(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m22(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m23(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m24(), CSSPrimitiveValue::CSS_NUMBER));

        transformVal->append(CSSPrimitiveValue::create(transform.m31(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m32(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m33(), CSSPrimitiveValue::CSS_NUMBER));
        transformVal->append(CSSPrimitiveValue::create(transform.m34(), CSSPrimitiveValue::CSS_NUMBER));

        transformVal->append(zoomAdjustedNumberValue(transform.m41(), style));
        transformVal->append(zoomAdjustedNumberValue(transform.m42(), style));
        transformVal->append(zoomAdjustedNumberValue(transform.m43(), style));
        transformVal->append(CSSPrimitiveValue::create(transform.m44(), CSSPrimitiveValue::CSS_NUMBER));
    }

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(transformVal);

    return list.release();
}

static PassRefPtr<CSSValue> getDelayValue(const AnimationList* animList)
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list->append(CSSPrimitiveValue::create(animList->animation(i)->delay(), CSSPrimitiveValue::CSS_S));
    } else {
        // Note that initialAnimationDelay() is used for both transitions and animations
        list->append(CSSPrimitiveValue::create(Animation::initialAnimationDelay(), CSSPrimitiveValue::CSS_S));
    }
    return list.release();
}

static PassRefPtr<CSSValue> getDurationValue(const AnimationList* animList)
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list->append(CSSPrimitiveValue::create(animList->animation(i)->duration(), CSSPrimitiveValue::CSS_S));
    } else {
        // Note that initialAnimationDuration() is used for both transitions and animations
        list->append(CSSPrimitiveValue::create(Animation::initialAnimationDuration(), CSSPrimitiveValue::CSS_S));
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

    if (int keywordSize = style->fontDescription().keywordSize())
        return CSSPrimitiveValue::createIdentifier(cssIdentifierForFontSizeKeyword(keywordSize));


    return zoomAdjustedPixelValue(style->fontDescription().computedPixelSize(), style.get());
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
    if (!shadow)
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);

    CSSPropertyID propertyID = static_cast<CSSPropertyID>(id);

    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    for (const ShadowData* s = shadow; s; s = s->next()) {
        RefPtr<CSSPrimitiveValue> x = zoomAdjustedPixelValue(s->x(), style);
        RefPtr<CSSPrimitiveValue> y = zoomAdjustedPixelValue(s->y(), style);
        RefPtr<CSSPrimitiveValue> blur = zoomAdjustedPixelValue(s->blur(), style);
        RefPtr<CSSPrimitiveValue> spread = propertyID == CSSPropertyTextShadow ? 0 : zoomAdjustedPixelValue(s->spread(), style);
        RefPtr<CSSPrimitiveValue> style = propertyID == CSSPropertyTextShadow || s->style() == Normal ? 0 : CSSPrimitiveValue::createIdentifier(CSSValueInset);
        RefPtr<CSSPrimitiveValue> color = CSSPrimitiveValue::createColor(s->color().rgb());
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
    DEFINE_STATIC_LOCAL(AtomicString, cursiveFamily, ("-webkit-cursive")); 
    DEFINE_STATIC_LOCAL(AtomicString, fantasyFamily, ("-webkit-fantasy")); 
    DEFINE_STATIC_LOCAL(AtomicString, monospaceFamily, ("-webkit-monospace")); 
    DEFINE_STATIC_LOCAL(AtomicString, sansSerifFamily, ("-webkit-sans-serif")); 
    DEFINE_STATIC_LOCAL(AtomicString, serifFamily, ("-webkit-serif")); 
    if (family == cursiveFamily)
        return CSSValueCursive;
    if (family == fantasyFamily)
        return CSSValueFantasy;
    if (family == monospaceFamily)
        return CSSValueMonospace;
    if (family == sansSerifFamily)
        return CSSValueSansSerif;
    if (family == serifFamily)
        return CSSValueSerif;
    return 0;
}

static PassRefPtr<CSSPrimitiveValue> valueForFamily(const AtomicString& family)
{
    if (int familyIdentifier = identifierForFamily(family))
        return CSSPrimitiveValue::createIdentifier(familyIdentifier);
    return CSSPrimitiveValue::create(family.string(), CSSPrimitiveValue::CSS_STRING);
}

static PassRefPtr<CSSValue> renderTextDecorationFlagsToCSSValue(int textDecoration)
{
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (textDecoration & UNDERLINE)
        list->append(CSSPrimitiveValue::createIdentifier(CSSValueUnderline));
    if (textDecoration & OVERLINE)
        list->append(CSSPrimitiveValue::createIdentifier(CSSValueOverline));
    if (textDecoration & LINE_THROUGH)
        list->append(CSSPrimitiveValue::createIdentifier(CSSValueLineThrough));
    if (textDecoration & BLINK)
        list->append(CSSPrimitiveValue::createIdentifier(CSSValueBlink));

    if (!list->length())
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    return list;
}

static PassRefPtr<CSSValue> fillRepeatToCSSValue(EFillRepeat xRepeat, EFillRepeat yRepeat)
{
    // For backwards compatibility, if both values are equal, just return one of them. And
    // if the two values are equivalent to repeat-x or repeat-y, just return the shorthand.
    if (xRepeat == yRepeat)
        return CSSPrimitiveValue::create(xRepeat);
    if (xRepeat == RepeatFill && yRepeat == NoRepeatFill)
        return CSSPrimitiveValue::createIdentifier(CSSValueRepeatX);
    if (xRepeat == NoRepeatFill && yRepeat == RepeatFill)
        return CSSPrimitiveValue::createIdentifier(CSSValueRepeatY);

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(CSSPrimitiveValue::create(xRepeat));
    list->append(CSSPrimitiveValue::create(yRepeat));
    return list.release();
}

static void logUnimplementedPropertyID(int propertyID)
{
    DEFINE_STATIC_LOCAL(HashSet<int>, propertyIDSet, ());
    if (!propertyIDSet.add(propertyID).second)
        return;

    LOG_ERROR("WebKit does not yet implement getComputedStyle for '%s'.", getPropertyName(static_cast<CSSPropertyID>(propertyID)));
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

    propertyID = CSSProperty::resolveDirectionAwareProperty(propertyID, style->direction());

    switch (static_cast<CSSPropertyID>(propertyID)) {
        case CSSPropertyInvalid:
            break;

        case CSSPropertyBackgroundColor:
            return CSSPrimitiveValue::createColor(m_allowVisitedStyle? style->visitedDependentColor(CSSPropertyBackgroundColor).rgb() : style->backgroundColor().rgb());
        case CSSPropertyBackgroundImage:
            // FIXME: Broken for multiple backgrounds. https://bugs.webkit.org/show_bug.cgi?id=44853
            if (style->backgroundImage())
                return style->backgroundImage()->cssValue();
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize: {
            EFillSizeType size = style->backgroundSizeType();
            if (size == Contain)
                return CSSPrimitiveValue::createIdentifier(CSSValueContain);
            if (size == Cover)
                return CSSPrimitiveValue::createIdentifier(CSSValueCover);
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            list->append(CSSPrimitiveValue::create(style->backgroundSizeLength().width()));
            list->append(CSSPrimitiveValue::create(style->backgroundSizeLength().height()));
            return list.release();
        }  
        case CSSPropertyBackgroundRepeat:
            return fillRepeatToCSSValue(style->backgroundRepeatX(), style->backgroundRepeatY());
        case CSSPropertyWebkitBackgroundComposite:
            return CSSPrimitiveValue::create(style->backgroundComposite());
        case CSSPropertyBackgroundAttachment:
            return CSSPrimitiveValue::create(style->backgroundAttachment());
        case CSSPropertyBackgroundClip:
        case CSSPropertyBackgroundOrigin:
        case CSSPropertyWebkitBackgroundClip:
        case CSSPropertyWebkitBackgroundOrigin: {
            EFillBox box = (propertyID == CSSPropertyWebkitBackgroundClip || propertyID == CSSPropertyBackgroundClip) ? style->backgroundClip() : style->backgroundOrigin();
            return CSSPrimitiveValue::create(box);
        }
        case CSSPropertyBackgroundPosition: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

            list->append(CSSPrimitiveValue::create(style->backgroundXPosition()));
            list->append(CSSPrimitiveValue::create(style->backgroundYPosition()));

            return list.release();
        }
        case CSSPropertyBackgroundPositionX:
            return CSSPrimitiveValue::create(style->backgroundXPosition());
        case CSSPropertyBackgroundPositionY:
            return CSSPrimitiveValue::create(style->backgroundYPosition());
        case CSSPropertyBorderCollapse:
            if (style->borderCollapse())
                return CSSPrimitiveValue::createIdentifier(CSSValueCollapse);
            return CSSPrimitiveValue::createIdentifier(CSSValueSeparate);
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
        case CSSPropertyBorderTopColor:
            return m_allowVisitedStyle ? CSSPrimitiveValue::createColor(style->visitedDependentColor(CSSPropertyBorderTopColor).rgb()) : currentColorOrValidColor(style.get(), style->borderTopColor());
        case CSSPropertyBorderRightColor:
            return m_allowVisitedStyle ? CSSPrimitiveValue::createColor(style->visitedDependentColor(CSSPropertyBorderRightColor).rgb()) : currentColorOrValidColor(style.get(), style->borderRightColor());
        case CSSPropertyBorderBottomColor:
            return m_allowVisitedStyle ? CSSPrimitiveValue::createColor(style->visitedDependentColor(CSSPropertyBorderBottomColor).rgb()) : currentColorOrValidColor(style.get(), style->borderBottomColor());
        case CSSPropertyBorderLeftColor:
            return m_allowVisitedStyle ? CSSPrimitiveValue::createColor(style->visitedDependentColor(CSSPropertyBorderLeftColor).rgb()) : currentColorOrValidColor(style.get(), style->borderLeftColor());
        case CSSPropertyBorderTopStyle:
            return CSSPrimitiveValue::create(style->borderTopStyle());
        case CSSPropertyBorderRightStyle:
            return CSSPrimitiveValue::create(style->borderRightStyle());
        case CSSPropertyBorderBottomStyle:
            return CSSPrimitiveValue::create(style->borderBottomStyle());
        case CSSPropertyBorderLeftStyle:
            return CSSPrimitiveValue::create(style->borderLeftStyle());
        case CSSPropertyBorderTopWidth:
            return zoomAdjustedPixelValue(style->borderTopWidth(), style.get());
        case CSSPropertyBorderRightWidth:
            return zoomAdjustedPixelValue(style->borderRightWidth(), style.get());
        case CSSPropertyBorderBottomWidth:
            return zoomAdjustedPixelValue(style->borderBottomWidth(), style.get());
        case CSSPropertyBorderLeftWidth:
            return zoomAdjustedPixelValue(style->borderLeftWidth(), style.get());
        case CSSPropertyBottom:
            return getPositionOffsetValue(style.get(), CSSPropertyBottom);
        case CSSPropertyWebkitBoxAlign:
            return CSSPrimitiveValue::create(style->boxAlign());
        case CSSPropertyWebkitBoxDirection:
            return CSSPrimitiveValue::create(style->boxDirection());
        case CSSPropertyWebkitBoxFlex:
            return CSSPrimitiveValue::create(style->boxFlex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxFlexGroup:
            return CSSPrimitiveValue::create(style->boxFlexGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxLines:
            return CSSPrimitiveValue::create(style->boxLines());
        case CSSPropertyWebkitBoxOrdinalGroup:
            return CSSPrimitiveValue::create(style->boxOrdinalGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxOrient:
            return CSSPrimitiveValue::create(style->boxOrient());
        case CSSPropertyWebkitBoxPack: {
            EBoxAlignment boxPack = style->boxPack();
            ASSERT(boxPack != BSTRETCH);
            ASSERT(boxPack != BBASELINE);
            if (boxPack == BJUSTIFY || boxPack== BBASELINE)
                return 0;
            return CSSPrimitiveValue::create(boxPack);
        }
        case CSSPropertyWebkitBoxReflect:
            return valueForReflection(style->boxReflect(), style.get());
        case CSSPropertyWebkitBoxShadow:
            return valueForShadow(style->boxShadow(), propertyID, style.get());
        case CSSPropertyCaptionSide:
            return CSSPrimitiveValue::create(style->captionSide());
        case CSSPropertyClear:
            return CSSPrimitiveValue::create(style->clear());
        case CSSPropertyColor:
            return CSSPrimitiveValue::createColor(m_allowVisitedStyle ? style->visitedDependentColor(CSSPropertyColor).rgb() : style->color().rgb());
        case CSSPropertyWebkitColumnCount:
            if (style->hasAutoColumnCount())
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style->columnCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitColumnGap:
            if (style->hasNormalColumnGap())
                return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
            return CSSPrimitiveValue::create(style->columnGap(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitColumnRuleColor:
            return m_allowVisitedStyle ? CSSPrimitiveValue::createColor(style->visitedDependentColor(CSSPropertyOutlineColor).rgb()) : currentColorOrValidColor(style.get(), style->columnRuleColor());
        case CSSPropertyWebkitColumnRuleStyle:
            return CSSPrimitiveValue::create(style->columnRuleStyle());
        case CSSPropertyWebkitColumnRuleWidth:
            return zoomAdjustedPixelValue(style->columnRuleWidth(), style.get());
        case CSSPropertyWebkitColumnSpan:
            if (style->columnSpan())
                return CSSPrimitiveValue::createIdentifier(CSSValueAll);
            return CSSPrimitiveValue::create(1, CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitColumnBreakAfter:
            return CSSPrimitiveValue::create(style->columnBreakAfter());
        case CSSPropertyWebkitColumnBreakBefore:
            return CSSPrimitiveValue::create(style->columnBreakBefore());
        case CSSPropertyWebkitColumnBreakInside:
            return CSSPrimitiveValue::create(style->columnBreakInside());
        case CSSPropertyWebkitColumnWidth:
            if (style->hasAutoColumnWidth())
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style->columnWidth(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyCursor: {
            RefPtr<CSSValueList> list;
            CursorList* cursors = style->cursors();
            if (cursors && cursors->size() > 0) {
                list = CSSValueList::createCommaSeparated();
                for (unsigned i = 0; i < cursors->size(); ++i)
                    list->append((*cursors)[i].image()->cssValue());
            }
            RefPtr<CSSValue> value = CSSPrimitiveValue::create(style->cursor());
            if (list) {
                list->append(value);
                return list.release();
            }
            return value.release();
        }
        case CSSPropertyDirection:
            return CSSPrimitiveValue::create(style->direction());
        case CSSPropertyDisplay:
            return CSSPrimitiveValue::create(style->display());
        case CSSPropertyEmptyCells:
            return CSSPrimitiveValue::create(style->emptyCells());
        case CSSPropertyFloat:
            return CSSPrimitiveValue::create(style->floating());
        case CSSPropertyFontFamily: {
            const FontFamily& firstFamily = style->fontDescription().family();
            if (!firstFamily.next())
                return valueForFamily(firstFamily.family());
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FontFamily* family = &firstFamily; family; family = family->next())
                list->append(valueForFamily(family->family()));
            return list.release();
        }
        case CSSPropertyFontSize:
            return zoomAdjustedPixelValue(style->fontDescription().computedPixelSize(), style.get());
        case CSSPropertyFontStyle:
            if (style->fontDescription().italic())
                return CSSPrimitiveValue::createIdentifier(CSSValueItalic);
            return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
        case CSSPropertyFontVariant:
            if (style->fontDescription().smallCaps())
                return CSSPrimitiveValue::createIdentifier(CSSValueSmallCaps);
            return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
        case CSSPropertyFontWeight:
            switch (style->fontDescription().weight()) {
                case FontWeight100:
                    return CSSPrimitiveValue::createIdentifier(CSSValue100);
                case FontWeight200:
                    return CSSPrimitiveValue::createIdentifier(CSSValue200);
                case FontWeight300:
                    return CSSPrimitiveValue::createIdentifier(CSSValue300);
                case FontWeightNormal:
                    return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
                case FontWeight500:
                    return CSSPrimitiveValue::createIdentifier(CSSValue500);
                case FontWeight600:
                    return CSSPrimitiveValue::createIdentifier(CSSValue600);
                case FontWeightBold:
                    return CSSPrimitiveValue::createIdentifier(CSSValueBold);
                case FontWeight800:
                    return CSSPrimitiveValue::createIdentifier(CSSValue800);
                case FontWeight900:
                    return CSSPrimitiveValue::createIdentifier(CSSValue900);
            }
            ASSERT_NOT_REACHED();
            return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
        case CSSPropertyHeight:
            if (renderer)
                return zoomAdjustedPixelValue(sizingBox(renderer).height(), style.get());
            return CSSPrimitiveValue::create(style->height());
        case CSSPropertyWebkitHighlight:
            if (style->highlight() == nullAtom)
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            return CSSPrimitiveValue::create(style->highlight(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitHyphens:
            return CSSPrimitiveValue::create(style->hyphens());
        case CSSPropertyWebkitHyphenateCharacter:
            if (style->hyphenationString().isNull())
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style->hyphenationString(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitHyphenateLocale:
            if (style->hyphenationLocale().isNull())
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style->hyphenationLocale(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitBorderFit:
            if (style->borderFit() == BorderFitBorder)
                return CSSPrimitiveValue::createIdentifier(CSSValueBorder);
            return CSSPrimitiveValue::createIdentifier(CSSValueLines);
        case CSSPropertyLeft:
            return getPositionOffsetValue(style.get(), CSSPropertyLeft);
        case CSSPropertyLetterSpacing:
            if (!style->letterSpacing())
                return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
            return zoomAdjustedPixelValue(style->letterSpacing(), style.get());
        case CSSPropertyWebkitLineClamp:
            if (style->lineClamp().isNone())
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            return CSSPrimitiveValue::create(style->lineClamp().value(), style->lineClamp().isPercentage() ? CSSPrimitiveValue::CSS_PERCENTAGE : CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyLineHeight: {
            Length length = style->lineHeight();
            if (length.isNegative())
                return CSSPrimitiveValue::createIdentifier(CSSValueNormal);
            if (length.isPercent())
                // This is imperfect, because it doesn't include the zoom factor and the real computation
                // for how high to be in pixels does include things like minimum font size and the zoom factor.
                // On the other hand, since font-size doesn't include the zoom factor, we really can't do
                // that here either.
                return zoomAdjustedPixelValue(static_cast<int>(length.percent() * style->fontDescription().specifiedSize()) / 100, style.get());
            return zoomAdjustedPixelValue(length.value(), style.get());
        }
        case CSSPropertyListStyleImage:
            if (style->listStyleImage())
                return style->listStyleImage()->cssValue();
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyListStylePosition:
            return CSSPrimitiveValue::create(style->listStylePosition());
        case CSSPropertyListStyleType:
            return CSSPrimitiveValue::create(style->listStyleType());
        case CSSPropertyMarginTop:
            if (renderer && renderer->isBox())
                // FIXME: Supposed to return the percentage if percentage was specified.
                return zoomAdjustedPixelValue(toRenderBox(renderer)->marginTop(), style.get());
            return CSSPrimitiveValue::create(style->marginTop());
        case CSSPropertyMarginRight:
            if (renderer && renderer->isBox())
                // FIXME: Supposed to return the percentage if percentage was specified.
                return zoomAdjustedPixelValue(toRenderBox(renderer)->marginRight(), style.get());
            return CSSPrimitiveValue::create(style->marginRight());
        case CSSPropertyMarginBottom:
            if (renderer && renderer->isBox())
                // FIXME: Supposed to return the percentage if percentage was specified.
                return zoomAdjustedPixelValue(toRenderBox(renderer)->marginBottom(), style.get());
            return CSSPrimitiveValue::create(style->marginBottom());
        case CSSPropertyMarginLeft:
            if (renderer && renderer->isBox())
                // FIXME: Supposed to return the percentage if percentage was specified.
                return zoomAdjustedPixelValue(toRenderBox(renderer)->marginLeft(), style.get());
            return CSSPrimitiveValue::create(style->marginLeft());
        case CSSPropertyWebkitMarqueeDirection:
            return CSSPrimitiveValue::create(style->marqueeDirection());
        case CSSPropertyWebkitMarqueeIncrement:
            return CSSPrimitiveValue::create(style->marqueeIncrement());
        case CSSPropertyWebkitMarqueeRepetition:
            if (style->marqueeLoopCount() < 0)
                return CSSPrimitiveValue::createIdentifier(CSSValueInfinite);
            return CSSPrimitiveValue::create(style->marqueeLoopCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitMarqueeStyle:
            return CSSPrimitiveValue::create(style->marqueeBehavior());
        case CSSPropertyWebkitMaskImage:
            if (style->maskImage())
                return style->maskImage()->cssValue();
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyWebkitMaskSize: {
            EFillSizeType size = style->maskSizeType();
            if (size == Contain)
                return CSSPrimitiveValue::createIdentifier(CSSValueContain);
            if (size == Cover)
                return CSSPrimitiveValue::createIdentifier(CSSValueCover);
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            list->append(CSSPrimitiveValue::create(style->maskSizeLength().width()));
            list->append(CSSPrimitiveValue::create(style->maskSizeLength().height()));
            return list.release();
        }  
        case CSSPropertyWebkitMaskRepeat:
            return fillRepeatToCSSValue(style->maskRepeatX(), style->maskRepeatY());
        case CSSPropertyWebkitMaskAttachment:
            return CSSPrimitiveValue::create(style->maskAttachment());
        case CSSPropertyWebkitMaskComposite:
            return CSSPrimitiveValue::create(style->maskComposite());
        case CSSPropertyWebkitMaskClip:
        case CSSPropertyWebkitMaskOrigin: {
            EFillBox box = (propertyID == CSSPropertyWebkitMaskClip ? style->maskClip() : style->maskOrigin());
            return CSSPrimitiveValue::create(box);
        }
        case CSSPropertyWebkitMaskPosition: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

            list->append(CSSPrimitiveValue::create(style->maskXPosition()));
            list->append(CSSPrimitiveValue::create(style->maskYPosition()));

            return list.release();
        }
        case CSSPropertyWebkitMaskPositionX:
            return CSSPrimitiveValue::create(style->maskXPosition());
        case CSSPropertyWebkitMaskPositionY:
            return CSSPrimitiveValue::create(style->maskYPosition());
        case CSSPropertyWebkitUserModify:
            return CSSPrimitiveValue::create(style->userModify());
        case CSSPropertyMaxHeight: {
            const Length& maxHeight = style->maxHeight();
            if (maxHeight.isFixed() && maxHeight.value() == undefinedLength)
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            return CSSPrimitiveValue::create(maxHeight);
        }
        case CSSPropertyMaxWidth: {
            const Length& maxWidth = style->maxWidth();
            if (maxWidth.isFixed() && maxWidth.value() == undefinedLength)
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            return CSSPrimitiveValue::create(maxWidth);
        }
        case CSSPropertyMinHeight:
            return CSSPrimitiveValue::create(style->minHeight());
        case CSSPropertyMinWidth:
            return CSSPrimitiveValue::create(style->minWidth());
        case CSSPropertyOpacity:
            return CSSPrimitiveValue::create(style->opacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOrphans:
            return CSSPrimitiveValue::create(style->orphans(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOutlineColor:
            return m_allowVisitedStyle ? CSSPrimitiveValue::createColor(style->visitedDependentColor(CSSPropertyOutlineColor).rgb()) : currentColorOrValidColor(style.get(), style->outlineColor());
        case CSSPropertyOutlineStyle:
            if (style->outlineStyleIsAuto())
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style->outlineStyle());
        case CSSPropertyOutlineWidth:
            return zoomAdjustedPixelValue(style->outlineWidth(), style.get());
        case CSSPropertyOverflow:
            return CSSPrimitiveValue::create(max(style->overflowX(), style->overflowY()));
        case CSSPropertyOverflowX:
            return CSSPrimitiveValue::create(style->overflowX());
        case CSSPropertyOverflowY:
            return CSSPrimitiveValue::create(style->overflowY());
        case CSSPropertyPaddingTop:
            if (renderer && renderer->isBox())
                return zoomAdjustedPixelValue(toRenderBox(renderer)->paddingTop(false), style.get());
            return CSSPrimitiveValue::create(style->paddingTop());
        case CSSPropertyPaddingRight:
            if (renderer && renderer->isBox())
                return zoomAdjustedPixelValue(toRenderBox(renderer)->paddingRight(false), style.get());
            return CSSPrimitiveValue::create(style->paddingRight());
        case CSSPropertyPaddingBottom:
            if (renderer && renderer->isBox())
                return zoomAdjustedPixelValue(toRenderBox(renderer)->paddingBottom(false), style.get());
            return CSSPrimitiveValue::create(style->paddingBottom());
        case CSSPropertyPaddingLeft:
            if (renderer && renderer->isBox())
                return zoomAdjustedPixelValue(toRenderBox(renderer)->paddingLeft(false), style.get());
            return CSSPrimitiveValue::create(style->paddingLeft());
        case CSSPropertyPageBreakAfter:
            return CSSPrimitiveValue::create(style->pageBreakAfter());
        case CSSPropertyPageBreakBefore:
            return CSSPrimitiveValue::create(style->pageBreakBefore());
        case CSSPropertyPageBreakInside: {
            EPageBreak pageBreak = style->pageBreakInside();
            ASSERT(pageBreak != PBALWAYS);
            if (pageBreak == PBALWAYS)
                return 0;
            return CSSPrimitiveValue::create(style->pageBreakInside());
        }
        case CSSPropertyPosition:
            return CSSPrimitiveValue::create(style->position());
        case CSSPropertyRight:
            return getPositionOffsetValue(style.get(), CSSPropertyRight);
        case CSSPropertyTableLayout:
            return CSSPrimitiveValue::create(style->tableLayout());
        case CSSPropertyTextAlign:
            return CSSPrimitiveValue::create(style->textAlign());
        case CSSPropertyTextDecoration:
            return renderTextDecorationFlagsToCSSValue(style->textDecoration());
        case CSSPropertyWebkitTextDecorationsInEffect:
            return renderTextDecorationFlagsToCSSValue(style->textDecorationsInEffect());
        case CSSPropertyWebkitTextFillColor:
            return currentColorOrValidColor(style.get(), style->textFillColor());
        case CSSPropertyTextIndent:
            return CSSPrimitiveValue::create(style->textIndent());
        case CSSPropertyTextShadow:
            return valueForShadow(style->textShadow(), propertyID, style.get());
        case CSSPropertyTextRendering:
            return CSSPrimitiveValue::create(style->fontDescription().textRenderingMode());
        case CSSPropertyTextOverflow:
            if (style->textOverflow())
                return CSSPrimitiveValue::createIdentifier(CSSValueEllipsis);
            return CSSPrimitiveValue::createIdentifier(CSSValueClip);
        case CSSPropertyWebkitTextSecurity:
            return CSSPrimitiveValue::create(style->textSecurity());
        case CSSPropertyWebkitTextSizeAdjust:
            if (style->textSizeAdjust())
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        case CSSPropertyWebkitTextStrokeColor:
            return currentColorOrValidColor(style.get(), style->textStrokeColor());
        case CSSPropertyWebkitTextStrokeWidth:
            return zoomAdjustedPixelValue(style->textStrokeWidth(), style.get());
        case CSSPropertyTextTransform:
            return CSSPrimitiveValue::create(style->textTransform());
        case CSSPropertyTop:
            return getPositionOffsetValue(style.get(), CSSPropertyTop);
        case CSSPropertyUnicodeBidi:
            return CSSPrimitiveValue::create(style->unicodeBidi());
        case CSSPropertyVerticalAlign:
            switch (style->verticalAlign()) {
                case BASELINE:
                    return CSSPrimitiveValue::createIdentifier(CSSValueBaseline);
                case MIDDLE:
                    return CSSPrimitiveValue::createIdentifier(CSSValueMiddle);
                case SUB:
                    return CSSPrimitiveValue::createIdentifier(CSSValueSub);
                case SUPER:
                    return CSSPrimitiveValue::createIdentifier(CSSValueSuper);
                case TEXT_TOP:
                    return CSSPrimitiveValue::createIdentifier(CSSValueTextTop);
                case TEXT_BOTTOM:
                    return CSSPrimitiveValue::createIdentifier(CSSValueTextBottom);
                case TOP:
                    return CSSPrimitiveValue::createIdentifier(CSSValueTop);
                case BOTTOM:
                    return CSSPrimitiveValue::createIdentifier(CSSValueBottom);
                case BASELINE_MIDDLE:
                    return CSSPrimitiveValue::createIdentifier(CSSValueWebkitBaselineMiddle);
                case LENGTH:
                    return CSSPrimitiveValue::create(style->verticalAlignLength());
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSSPropertyVisibility:
            return CSSPrimitiveValue::create(style->visibility());
        case CSSPropertyWhiteSpace:
            return CSSPrimitiveValue::create(style->whiteSpace());
        case CSSPropertyWidows:
            return CSSPrimitiveValue::create(style->widows(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWidth:
            if (renderer)
                return zoomAdjustedPixelValue(sizingBox(renderer).width(), style.get());
            return CSSPrimitiveValue::create(style->width());
        case CSSPropertyWordBreak:
            return CSSPrimitiveValue::create(style->wordBreak());
        case CSSPropertyWordSpacing:
            return zoomAdjustedPixelValue(style->wordSpacing(), style.get());
        case CSSPropertyWordWrap:
            return CSSPrimitiveValue::create(style->wordWrap());
        case CSSPropertyWebkitLineBreak:
            return CSSPrimitiveValue::create(style->khtmlLineBreak());
        case CSSPropertyWebkitNbspMode:
            return CSSPrimitiveValue::create(style->nbspMode());
        case CSSPropertyWebkitMatchNearestMailBlockquoteColor:
            return CSSPrimitiveValue::create(style->matchNearestMailBlockquoteColor());
        case CSSPropertyResize:
            return CSSPrimitiveValue::create(style->resize());
        case CSSPropertyWebkitFontSmoothing:
            return CSSPrimitiveValue::create(style->fontDescription().fontSmoothing());
        case CSSPropertyZIndex:
            if (style->hasAutoZIndex())
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            return CSSPrimitiveValue::create(style->zIndex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyZoom:
            return CSSPrimitiveValue::create(style->zoom(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxSizing:
            if (style->boxSizing() == CONTENT_BOX)
                return CSSPrimitiveValue::createIdentifier(CSSValueContentBox);
            return CSSPrimitiveValue::createIdentifier(CSSValueBorderBox);
#if ENABLE(DASHBOARD_SUPPORT)
        case CSSPropertyWebkitDashboardRegion:
        {
            const Vector<StyleDashboardRegion>& regions = style->dashboardRegions();
            unsigned count = regions.size();
            if (count == 1 && regions[0].type == StyleDashboardRegion::None)
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);

            RefPtr<DashboardRegion> firstRegion;
            DashboardRegion* previousRegion = 0;
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
            return CSSPrimitiveValue::create(firstRegion.release());
        }
#endif
        case CSSPropertyWebkitAnimationDelay:
            return getDelayValue(style->animations());
        case CSSPropertyWebkitAnimationDirection: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    if (t->animation(i)->direction())
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValueAlternate));
                    else
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValueNormal));
                }
            } else
                list->append(CSSPrimitiveValue::createIdentifier(CSSValueNormal));
            return list.release();
        }
        case CSSPropertyWebkitAnimationDuration:
            return getDurationValue(style->animations());
        case CSSPropertyWebkitAnimationFillMode: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    switch (t->animation(i)->fillMode()) {
                    case AnimationFillModeNone:
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValueNone));
                        break;
                    case AnimationFillModeForwards:
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValueForwards));
                        break;
                    case AnimationFillModeBackwards:
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValueBackwards));
                        break;
                    case AnimationFillModeBoth:
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValueBoth));
                        break;
                    }
                }
            } else
                list->append(CSSPrimitiveValue::createIdentifier(CSSValueNone));
            return list.release();
        }
        case CSSPropertyWebkitAnimationIterationCount: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    int iterationCount = t->animation(i)->iterationCount();
                    if (iterationCount == Animation::IterationCountInfinite)
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValueInfinite));
                    else
                        list->append(CSSPrimitiveValue::create(iterationCount, CSSPrimitiveValue::CSS_NUMBER));
                }
            } else
                list->append(CSSPrimitiveValue::create(Animation::initialAnimationIterationCount(), CSSPrimitiveValue::CSS_NUMBER));
            return list.release();
        }
        case CSSPropertyWebkitAnimationName: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    list->append(CSSPrimitiveValue::create(t->animation(i)->name(), CSSPrimitiveValue::CSS_STRING));
                }
            } else
                list->append(CSSPrimitiveValue::createIdentifier(CSSValueNone));
            return list.release();
        }
        case CSSPropertyWebkitAnimationPlayState: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    int prop = t->animation(i)->playState();
                    if (prop == AnimPlayStatePlaying)
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValueRunning));
                    else
                        list->append(CSSPrimitiveValue::createIdentifier(CSSValuePaused));
                }
            } else
                list->append(CSSPrimitiveValue::createIdentifier(CSSValueRunning));
            return list.release();
        }
        case CSSPropertyWebkitAnimationTimingFunction:
            return getTimingFunctionValue(style->animations());
        case CSSPropertyWebkitAppearance:
            return CSSPrimitiveValue::create(style->appearance());
        case CSSPropertyWebkitBackfaceVisibility:
            return CSSPrimitiveValue::createIdentifier((style->backfaceVisibility() == BackfaceVisibilityHidden) ? CSSValueHidden : CSSValueVisible);
        case CSSPropertyWebkitBlockFlow:
            return CSSPrimitiveValue::create(style->blockFlow());
        case CSSPropertyWebkitBorderImage:
            return valueForNinePieceImage(style->borderImage());
        case CSSPropertyWebkitMaskBoxImage:
            return valueForNinePieceImage(style->maskBoxImage());
        case CSSPropertyWebkitFontSizeDelta:
            // Not a real style property -- used by the editing engine -- so has no computed value.
            break;
        case CSSPropertyWebkitMarginBottomCollapse:
            return CSSPrimitiveValue::create(style->marginBottomCollapse());
        case CSSPropertyWebkitMarginTopCollapse:
            return CSSPrimitiveValue::create(style->marginTopCollapse());
        case CSSPropertyWebkitPerspective:
            if (!style->hasPerspective())
                return CSSPrimitiveValue::createIdentifier(CSSValueNone);
            return CSSPrimitiveValue::create(style->perspective(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitPerspectiveOrigin: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                IntRect box = sizingBox(renderer);
                list->append(zoomAdjustedPixelValue(style->perspectiveOriginX().calcMinValue(box.width()), style.get()));
                list->append(zoomAdjustedPixelValue(style->perspectiveOriginY().calcMinValue(box.height()), style.get()));
            }
            else {
                list->append(CSSPrimitiveValue::create(style->perspectiveOriginX()));
                list->append(CSSPrimitiveValue::create(style->perspectiveOriginY()));
            }
            return list.release();
        }
        case CSSPropertyWebkitRtlOrdering:
            if (style->visuallyOrdered())
                return CSSPrimitiveValue::createIdentifier(CSSValueVisual);
            return CSSPrimitiveValue::createIdentifier(CSSValueLogical);
        case CSSPropertyWebkitUserDrag:
            return CSSPrimitiveValue::create(style->userDrag());
        case CSSPropertyWebkitUserSelect:
            return CSSPrimitiveValue::create(style->userSelect());
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
                return CSSPrimitiveValue::createIdentifier(CSSValueAuto);
            RefPtr<Rect> rect = Rect::create();
            rect->setTop(zoomAdjustedPixelValue(style->clip().top().value(), style.get()));
            rect->setRight(zoomAdjustedPixelValue(style->clip().right().value(), style.get()));
            rect->setBottom(zoomAdjustedPixelValue(style->clip().bottom().value(), style.get()));
            rect->setLeft(zoomAdjustedPixelValue(style->clip().left().value(), style.get()));
            return CSSPrimitiveValue::create(rect.release());
        }
        case CSSPropertyWebkitTransform:
            return computedTransform(renderer, style.get());
        case CSSPropertyWebkitTransformOrigin: {
            RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                IntRect box = sizingBox(renderer);
                list->append(zoomAdjustedPixelValue(style->transformOriginX().calcMinValue(box.width()), style.get()));
                list->append(zoomAdjustedPixelValue(style->transformOriginY().calcMinValue(box.height()), style.get()));
                if (style->transformOriginZ() != 0)
                    list->append(zoomAdjustedPixelValue(style->transformOriginZ(), style.get()));
            } else {
                list->append(CSSPrimitiveValue::create(style->transformOriginX()));
                list->append(CSSPrimitiveValue::create(style->transformOriginY()));
                if (style->transformOriginZ() != 0)
                    list->append(zoomAdjustedPixelValue(style->transformOriginZ(), style.get()));
            }
            return list.release();
        }
        case CSSPropertyWebkitTransformStyle:
            return CSSPrimitiveValue::createIdentifier((style->transformStyle3D() == TransformStyle3DPreserve3D) ? CSSValuePreserve3d : CSSValueFlat);
        case CSSPropertyWebkitTransitionDelay:
            return getDelayValue(style->transitions());
        case CSSPropertyWebkitTransitionDuration:
            return getDurationValue(style->transitions());
        case CSSPropertyWebkitTransitionProperty: {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const AnimationList* t = style->transitions();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    int prop = t->animation(i)->property();
                    RefPtr<CSSValue> propertyValue;
                    if (prop == cAnimateNone)
                        propertyValue = CSSPrimitiveValue::createIdentifier(CSSValueNone);
                    else if (prop == cAnimateAll)
                        propertyValue = CSSPrimitiveValue::createIdentifier(CSSValueAll);
                    else
                        propertyValue = CSSPrimitiveValue::create(getPropertyName(static_cast<CSSPropertyID>(prop)), CSSPrimitiveValue::CSS_STRING);
                    list->append(propertyValue);
                }
            } else
                list->append(CSSPrimitiveValue::createIdentifier(CSSValueAll));
            return list.release();
        }
        case CSSPropertyWebkitTransitionTimingFunction:
            return getTimingFunctionValue(style->transitions());
        case CSSPropertyPointerEvents:
            return CSSPrimitiveValue::create(style->pointerEvents());
        case CSSPropertyWebkitColorCorrection:
            return CSSPrimitiveValue::create(style->colorSpace());

        /* Shorthand properties, currently not supported see bug 13658*/
        case CSSPropertyBackground:
        case CSSPropertyBorder:
        case CSSPropertyBorderBottom:
        case CSSPropertyBorderColor:
        case CSSPropertyBorderLeft:
        case CSSPropertyBorderRadius:
        case CSSPropertyBorderRight:
        case CSSPropertyBorderStyle:
        case CSSPropertyBorderTop:
        case CSSPropertyBorderWidth:
        case CSSPropertyFont:
        case CSSPropertyListStyle:
        case CSSPropertyMargin:
        case CSSPropertyPadding:
        case CSSPropertyWebkitWritingMode:
            break;

        /* Unimplemented CSS 3 properties (including CSS3 shorthand properties) */
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
        case CSSPropertyWebkitMarginEnd:
        case CSSPropertyWebkitMarginStart:
        case CSSPropertyWebkitPaddingEnd:
        case CSSPropertyWebkitPaddingStart:
            ASSERT_NOT_REACHED();
            break;

        /* Unimplemented @font-face properties */
        case CSSPropertyFontStretch:
        case CSSPropertySrc:
        case CSSPropertyUnicodeRange:
            break;

        /* Other unimplemented properties */
        case CSSPropertyBackgroundRepeatX:
        case CSSPropertyBackgroundRepeatY:
        case CSSPropertyContent: // FIXME: needs implementation, bug 23668
        case CSSPropertyCounterIncrement:
        case CSSPropertyCounterReset:
        case CSSPropertyOutline: // FIXME: needs implementation
        case CSSPropertyOutlineOffset: // FIXME: needs implementation
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
        case CSSPropertyWebkitVariableDeclarationBlock:
            break;
#if ENABLE(SVG)
        // FIXME: This default case ruins the point of using an enum for
        // properties -- it prevents us from getting a warning when we
        // forget to list a property above.
        default:
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
