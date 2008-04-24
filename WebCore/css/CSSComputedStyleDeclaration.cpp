/**
 *
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "CSSBorderImageValue.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSValueList.h"
#include "CachedImage.h"
#include "DashboardRegion.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Pair.h"
#include "RenderObject.h"
#include "ShadowValue.h"

namespace WebCore {

// List of all properties we know how to compute, omitting shorthands.
static const int computedProperties[] = {
    CSSPropertyBackgroundAttachment,
    CSSPropertyBackgroundColor,
    CSSPropertyBackgroundImage,
    // more specific background-position-x/y are non-standard
    CSSPropertyBackgroundPosition,
    CSSPropertyBackgroundRepeat,
    CSSPropertyBorderBottomColor,
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
    CSSPropertyBorderTopStyle,
    CSSPropertyBorderTopWidth,
    CSSPropertyBottom,
    CSSPropertyCaptionSide,
    CSSPropertyClear,
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
    CSSPropertyPosition,
    CSSPropertyResize,
    CSSPropertyRight,
    CSSPropertyTableLayout,
    CSSPropertyTextAlign,
    CSSPropertyTextDecoration,
    CSSPropertyTextIndent,
    CSSPropertyTextShadow,
    CSSPropertyTextTransform,
    CSSPropertyTop,
    CSSPropertyUnicodeBidi,
    CSSPropertyVerticalAlign,
    CSSPropertyVisibility,
    CSSPropertyWhiteSpace,
    CSSPropertyWidows,
    CSSPropertyWidth,
    CSSPropertyWordSpacing,
    CSSPropertyWordWrap,
    CSSPropertyZIndex,
    CSSPropertyZoom,

    CSSPropertyWebkitAppearance,
    CSSPropertyWebkitBackgroundClip,
    CSSPropertyWebkitBackgroundComposite,
    CSSPropertyWebkitBackgroundOrigin,
    CSSPropertyWebkitBackgroundSize,
    CSSPropertyWebkitBorderFit,
    CSSPropertyWebkitBorderImage,
    CSSPropertyWebkitBorderHorizontalSpacing,
    CSSPropertyWebkitBorderVerticalSpacing,
    CSSPropertyWebkitBoxAlign,
    CSSPropertyWebkitBoxDirection,
    CSSPropertyWebkitBoxFlex,
    CSSPropertyWebkitBoxFlexGroup,
    CSSPropertyWebkitBoxLines,
    CSSPropertyWebkitBoxOrdinalGroup,
    CSSPropertyWebkitBoxOrient,
    CSSPropertyWebkitBoxPack,
    CSSPropertyWebkitBoxShadow,
    CSSPropertyWebkitBoxSizing,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyWebkitColumnCount,
    CSSPropertyWebkitColumnGap,
    CSSPropertyWebkitColumnRuleColor,
    CSSPropertyWebkitColumnRuleStyle,
    CSSPropertyWebkitColumnRuleWidth,
    CSSPropertyWebkitColumnWidth,
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
    CSSPropertyWebkitMaskImage,
    CSSPropertyWebkitMaskPosition,
    CSSPropertyWebkitMaskRepeat,
    CSSPropertyWebkitMaskClip,
    CSSPropertyWebkitMaskComposite,
    CSSPropertyWebkitMaskOrigin,
    CSSPropertyWebkitMaskSize,
    CSSPropertyWebkitNbspMode,
    CSSPropertyWebkitRtlOrdering,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextFillColor,
    CSSPropertyWebkitTextSecurity,
    CSSPropertyWebkitTextStrokeColor,
    CSSPropertyWebkitTextStrokeWidth,
    CSSPropertyWebkitUserDrag,
    CSSPropertyWebkitUserModify,
    CSSPropertyWebkitUserSelect,
    CSSPropertyWebkitDashboardRegion,
    CSSPropertyWebkitBorderBottomLeftRadius,
    CSSPropertyWebkitBorderBottomRightRadius,
    CSSPropertyWebkitBorderTopLeftRadius,
    CSSPropertyWebkitBorderTopRightRadius
    
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
    CSSPropertyPointerEvents,
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
    CSSPropertyTextRendering,
    CSSPropertyAlignmentBaseline,
    CSSPropertyBaselineShift,
    CSSPropertyDominantBaseline,
    CSSPropertyKerning,
    CSSPropertyTextAnchor,
    CSSPropertyWritingMode,
    CSSPropertyGlyphOrientationHorizontal,
    CSSPropertyGlyphOrientationVertical
#endif
};

const unsigned numComputedProperties = sizeof(computedProperties) / sizeof(computedProperties[0]);

static PassRefPtr<CSSValue> valueForShadow(const ShadowData* shadow)
{
    if (!shadow)
        return new CSSPrimitiveValue(CSSValueNone);

    RefPtr<CSSValueList> list = new CSSValueList;
    for (const ShadowData* s = shadow; s; s = s->next) {
        RefPtr<CSSPrimitiveValue> x = new CSSPrimitiveValue(s->x, CSSPrimitiveValue::CSS_PX);
        RefPtr<CSSPrimitiveValue> y = new CSSPrimitiveValue(s->y, CSSPrimitiveValue::CSS_PX);
        RefPtr<CSSPrimitiveValue> blur = new CSSPrimitiveValue(s->blur, CSSPrimitiveValue::CSS_PX);
        RefPtr<CSSPrimitiveValue> color = new CSSPrimitiveValue(s->color.rgb());
        list->append(new ShadowValue(x.release(), y.release(), blur.release(), color.release()));
    }
    return list.release();
}

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
    
    ASSERT_NOT_REACHED();
    return CSSValueStretch;
}
        
static PassRefPtr<CSSValue> valueForNinePieceImage(const NinePieceImage& image)
{
    if (!image.hasImage())
        return new CSSPrimitiveValue(CSSValueNone);
    
    // Image first.
    RefPtr<CSSValue> imageValue;
    if (image.image())
        imageValue = image.image()->cssValue();
    
    // Create the slices.
    RefPtr<CSSPrimitiveValue> top;
    if (image.m_slices.top.isPercent())
        top = new CSSPrimitiveValue(image.m_slices.top.value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        top = new CSSPrimitiveValue(image.m_slices.top.value(), CSSPrimitiveValue::CSS_NUMBER);
        
    RefPtr<CSSPrimitiveValue> right;
    if (image.m_slices.right.isPercent())
        right = new CSSPrimitiveValue(image.m_slices.right.value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        right = new CSSPrimitiveValue(image.m_slices.right.value(), CSSPrimitiveValue::CSS_NUMBER);
        
    RefPtr<CSSPrimitiveValue> bottom;
    if (image.m_slices.bottom.isPercent())
        bottom = new CSSPrimitiveValue(image.m_slices.bottom.value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        bottom = new CSSPrimitiveValue(image.m_slices.bottom.value(), CSSPrimitiveValue::CSS_NUMBER);
    
    RefPtr<CSSPrimitiveValue> left;
    if (image.m_slices.left.isPercent())
        left = new CSSPrimitiveValue(image.m_slices.left.value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        left = new CSSPrimitiveValue(image.m_slices.left.value(), CSSPrimitiveValue::CSS_NUMBER);

    RefPtr<Rect> rect = new Rect();
    rect->setTop(top);
    rect->setRight(right);
    rect->setBottom(bottom);
    rect->setLeft(left);

    return new CSSBorderImageValue(imageValue, rect, valueForRepeatRule(image.m_horizontalRule), valueForRepeatRule(image.m_verticalRule));
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

    if (style->position() == AbsolutePosition || style->position() == FixedPosition)
        return new CSSPrimitiveValue(l);

    if (style->position() == RelativePosition)
        // FIXME: It's not enough to simply return "auto" values for one offset if the other side is defined.
        // In other words if left is auto and right is not auto, then left's computed value is negative right.
        // So we should get the opposite length unit and see if it is auto.
        return new CSSPrimitiveValue(l);

    return new CSSPrimitiveValue(CSSValueAuto);
}

static PassRefPtr<CSSPrimitiveValue> currentColorOrValidColor(RenderStyle* style, const Color& color)
{
    if (!color.isValid())
        return new CSSPrimitiveValue(style->color().rgb());
    return new CSSPrimitiveValue(color.rgb());
}

static PassRefPtr<CSSValue> getBorderRadiusCornerValue(IntSize radius)
{
    if (radius.width() == radius.height())
        return new CSSPrimitiveValue(radius.width(), CSSPrimitiveValue::CSS_PX);

    RefPtr<CSSValueList> list = new CSSValueList(true);
    list->append(new CSSPrimitiveValue(radius.width(), CSSPrimitiveValue::CSS_PX));
    list->append(new CSSPrimitiveValue(radius.height(), CSSPrimitiveValue::CSS_PX));
    return list.release();
}

static IntRect sizingBox(RenderObject* renderer)
{
    return renderer->style()->boxSizing() == CONTENT_BOX ? renderer->contentBox() : renderer->borderBox();
}

CSSComputedStyleDeclaration::CSSComputedStyleDeclaration(PassRefPtr<Node> n)
    : m_node(n)
{
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

PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(int propertyID) const
{
    return getPropertyCSSValue(propertyID, UpdateLayout);
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

    RenderStyle* style = node->computedStyle();
    if (!style)
        return 0;

    switch (static_cast<CSSPropertyID>(propertyID)) {
        case CSSPropertyInvalid:
            break;

        case CSSPropertyBackgroundColor:
            return new CSSPrimitiveValue(style->backgroundColor().rgb());
        case CSSPropertyBackgroundImage:
            if (style->backgroundImage())
                return style->backgroundImage()->cssValue();
            return new CSSPrimitiveValue(CSSValueNone);
        case CSSPropertyWebkitBackgroundSize: {
            RefPtr<CSSValueList> list = new CSSValueList(true);
            list->append(new CSSPrimitiveValue(style->backgroundSize().width));
            list->append(new CSSPrimitiveValue(style->backgroundSize().height));
            return list.release();
        }  
        case CSSPropertyBackgroundRepeat:
            return new CSSPrimitiveValue(style->backgroundRepeat());
        case CSSPropertyWebkitBackgroundComposite:
            return new CSSPrimitiveValue(style->backgroundComposite());
        case CSSPropertyBackgroundAttachment:
            if (style->backgroundAttachment())
                return new CSSPrimitiveValue(CSSValueScroll);
            return new CSSPrimitiveValue(CSSValueFixed);
        case CSSPropertyWebkitBackgroundClip:
        case CSSPropertyWebkitBackgroundOrigin: {
            EFillBox box = (propertyID == CSSPropertyWebkitBackgroundClip ? style->backgroundClip() : style->backgroundOrigin());
            return new CSSPrimitiveValue(box);
        }
        case CSSPropertyBackgroundPosition: {
            RefPtr<CSSValueList> list = new CSSValueList(true);

            list->append(new CSSPrimitiveValue(style->backgroundXPosition()));
            list->append(new CSSPrimitiveValue(style->backgroundYPosition()));

            return list.release();
        }
        case CSSPropertyBackgroundPositionX:
            return new CSSPrimitiveValue(style->backgroundXPosition());
        case CSSPropertyBackgroundPositionY:
            return new CSSPrimitiveValue(style->backgroundYPosition());
        case CSSPropertyBorderCollapse:
            if (style->borderCollapse())
                return new CSSPrimitiveValue(CSSValueCollapse);
            return new CSSPrimitiveValue(CSSValueSeparate);
        case CSSPropertyBorderSpacing: {
            RefPtr<CSSValueList> list = new CSSValueList(true);
            list->append(new CSSPrimitiveValue(style->horizontalBorderSpacing(), CSSPrimitiveValue::CSS_PX));
            list->append(new CSSPrimitiveValue(style->verticalBorderSpacing(), CSSPrimitiveValue::CSS_PX));
            return list.release();
        }  
        case CSSPropertyWebkitBorderHorizontalSpacing:
            return new CSSPrimitiveValue(style->horizontalBorderSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyWebkitBorderVerticalSpacing:
            return new CSSPrimitiveValue(style->verticalBorderSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyBorderTopColor:
            return currentColorOrValidColor(style, style->borderTopColor());
        case CSSPropertyBorderRightColor:
            return currentColorOrValidColor(style, style->borderRightColor());
        case CSSPropertyBorderBottomColor:
            return currentColorOrValidColor(style, style->borderBottomColor());
        case CSSPropertyBorderLeftColor:
            return currentColorOrValidColor(style, style->borderLeftColor());
        case CSSPropertyBorderTopStyle:
            return new CSSPrimitiveValue(style->borderTopStyle());
        case CSSPropertyBorderRightStyle:
            return new CSSPrimitiveValue(style->borderRightStyle());
        case CSSPropertyBorderBottomStyle:
            return new CSSPrimitiveValue(style->borderBottomStyle());
        case CSSPropertyBorderLeftStyle:
            return new CSSPrimitiveValue(style->borderLeftStyle());
        case CSSPropertyBorderTopWidth:
            return new CSSPrimitiveValue(style->borderTopWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyBorderRightWidth:
            return new CSSPrimitiveValue(style->borderRightWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyBorderBottomWidth:
            return new CSSPrimitiveValue(style->borderBottomWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyBorderLeftWidth:
            return new CSSPrimitiveValue(style->borderLeftWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyBottom:
            return getPositionOffsetValue(style, CSSPropertyBottom);
        case CSSPropertyWebkitBoxAlign:
            return new CSSPrimitiveValue(style->boxAlign());
        case CSSPropertyWebkitBoxDirection:
            return new CSSPrimitiveValue(style->boxDirection());
        case CSSPropertyWebkitBoxFlex:
            return new CSSPrimitiveValue(style->boxFlex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxFlexGroup:
            return new CSSPrimitiveValue(style->boxFlexGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxLines:
            return new CSSPrimitiveValue(style->boxLines());
        case CSSPropertyWebkitBoxOrdinalGroup:
            return new CSSPrimitiveValue(style->boxOrdinalGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxOrient:
            return new CSSPrimitiveValue(style->boxOrient());
        case CSSPropertyWebkitBoxPack: {
            EBoxAlignment boxPack = style->boxPack();
            ASSERT(boxPack != BSTRETCH);
            ASSERT(boxPack != BBASELINE);
            if (boxPack == BJUSTIFY || boxPack== BBASELINE)
                return 0;
            return new CSSPrimitiveValue(boxPack);
        }
        case CSSPropertyWebkitBoxShadow:
            return valueForShadow(style->boxShadow());
        case CSSPropertyCaptionSide:
            return new CSSPrimitiveValue(style->captionSide());
        case CSSPropertyClear:
            return new CSSPrimitiveValue(style->clear());
        case CSSPropertyColor:
            return new CSSPrimitiveValue(style->color().rgb());
        case CSSPropertyWebkitColumnCount:
            if (style->hasAutoColumnCount())
                return new CSSPrimitiveValue(CSSValueAuto);
            return new CSSPrimitiveValue(style->columnCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitColumnGap:
            if (style->hasNormalColumnGap())
                return new CSSPrimitiveValue(CSSValueNormal);
            return new CSSPrimitiveValue(style->columnGap(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitColumnRuleColor:
            return currentColorOrValidColor(style, style->columnRuleColor());
        case CSSPropertyWebkitColumnRuleStyle:
            return new CSSPrimitiveValue(style->columnRuleStyle());
        case CSSPropertyWebkitColumnRuleWidth:
            return new CSSPrimitiveValue(style->columnRuleWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyWebkitColumnBreakAfter:
            return new CSSPrimitiveValue(style->columnBreakAfter());
        case CSSPropertyWebkitColumnBreakBefore:
            return new CSSPrimitiveValue(style->columnBreakBefore());
        case CSSPropertyWebkitColumnBreakInside:
            return new CSSPrimitiveValue(style->columnBreakInside());
        case CSSPropertyWebkitColumnWidth:
            if (style->hasAutoColumnWidth())
                return new CSSPrimitiveValue(CSSValueAuto);
            return new CSSPrimitiveValue(style->columnWidth(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyCursor: {
            RefPtr<CSSValueList> list;
            CursorList* cursors = style->cursors();
            if (cursors && cursors->size() > 0) {
                list = new CSSValueList;
                for (unsigned i = 0; i < cursors->size(); ++i)
                    list->append(new CSSPrimitiveValue((*cursors)[i].cursorImage->url(), CSSPrimitiveValue::CSS_URI));
            }
            RefPtr<CSSValue> value = new CSSPrimitiveValue(style->cursor());
            if (list) {
                list->append(value);
                return list.release();
            }
            return value.release();
        }
        case CSSPropertyDirection:
            return new CSSPrimitiveValue(style->direction());
        case CSSPropertyDisplay:
            return new CSSPrimitiveValue(style->display());
        case CSSPropertyEmptyCells:
            return new CSSPrimitiveValue(style->emptyCells());
        case CSSPropertyFloat:
            return new CSSPrimitiveValue(style->floating());
        case CSSPropertyFontFamily:
            // FIXME: This only returns the first family.
            return new CSSPrimitiveValue(style->fontDescription().family().family().string(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyFontSize:
            return new CSSPrimitiveValue(style->fontDescription().computedPixelSize(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyWebkitBinding:
            break;
        case CSSPropertyFontStyle:
            if (style->fontDescription().italic())
                return new CSSPrimitiveValue(CSSValueItalic);
            return new CSSPrimitiveValue(CSSValueNormal);
        case CSSPropertyFontVariant:
            if (style->fontDescription().smallCaps())
                return new CSSPrimitiveValue(CSSValueSmallCaps);
            return new CSSPrimitiveValue(CSSValueNormal);
        case CSSPropertyFontWeight:
            switch (style->fontDescription().weight()) {
                case FontWeight100:
                    return new CSSPrimitiveValue(CSSValue100);
                case FontWeight200:
                    return new CSSPrimitiveValue(CSSValue200);
                case FontWeight300:
                    return new CSSPrimitiveValue(CSSValue300);
                case FontWeightNormal:
                    return new CSSPrimitiveValue(CSSValueNormal);
                case FontWeight500:
                    return new CSSPrimitiveValue(CSSValue500);
                case FontWeight600:
                    return new CSSPrimitiveValue(CSSValue600);
                case FontWeightBold:
                    return new CSSPrimitiveValue(CSSValueBold);
                case FontWeight800:
                    return new CSSPrimitiveValue(CSSValue800);
                case FontWeight900:
                    return new CSSPrimitiveValue(CSSValue900);
            }
            ASSERT_NOT_REACHED();
            return new CSSPrimitiveValue(CSSValueNormal);
        case CSSPropertyHeight:
            if (renderer)
                return new CSSPrimitiveValue(sizingBox(renderer).height(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->height());
        case CSSPropertyWebkitHighlight:
            if (style->highlight() == nullAtom)
                return new CSSPrimitiveValue(CSSValueNone);
            return new CSSPrimitiveValue(style->highlight(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitBorderFit:
            if (style->borderFit() == BorderFitBorder)
                return new CSSPrimitiveValue(CSSValueBorder);
            return new CSSPrimitiveValue(CSSValueLines);
        case CSSPropertyLeft:
            return getPositionOffsetValue(style, CSSPropertyLeft);
        case CSSPropertyLetterSpacing:
            if (!style->letterSpacing())
                return new CSSPrimitiveValue(CSSValueNormal);
            return new CSSPrimitiveValue(style->letterSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyWebkitLineClamp:
            if (style->lineClamp() == -1)
                return new CSSPrimitiveValue(CSSValueNone);
            return new CSSPrimitiveValue(style->lineClamp(), CSSPrimitiveValue::CSS_PERCENTAGE);
        case CSSPropertyLineHeight: {
            Length length = style->lineHeight();
            if (length.isNegative())
                return new CSSPrimitiveValue(CSSValueNormal);
            if (length.isPercent())
                // This is imperfect, because it doesn't include the zoom factor and the real computation
                // for how high to be in pixels does include things like minimum font size and the zoom factor.
                // On the other hand, since font-size doesn't include the zoom factor, we really can't do
                // that here either.
                return new CSSPrimitiveValue(static_cast<int>(length.percent() * style->fontDescription().specifiedSize()) / 100, CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(length.value(), CSSPrimitiveValue::CSS_PX);
        }
        case CSSPropertyListStyleImage:
            if (style->listStyleImage())
                return style->listStyleImage()->cssValue();
            return new CSSPrimitiveValue(CSSValueNone);
        case CSSPropertyListStylePosition:
            return new CSSPrimitiveValue(style->listStylePosition());
        case CSSPropertyListStyleType:
            return new CSSPrimitiveValue(style->listStyleType());
        case CSSPropertyMarginTop:
            if (renderer)
                // FIXME: Supposed to return the percentage if percentage was specified.
                return new CSSPrimitiveValue(renderer->marginTop(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->marginTop());
        case CSSPropertyMarginRight:
            if (renderer)
                // FIXME: Supposed to return the percentage if percentage was specified.
                return new CSSPrimitiveValue(renderer->marginRight(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->marginRight());
        case CSSPropertyMarginBottom:
            if (renderer)
                // FIXME: Supposed to return the percentage if percentage was specified.
                return new CSSPrimitiveValue(renderer->marginBottom(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->marginBottom());
        case CSSPropertyMarginLeft:
            if (renderer)
                // FIXME: Supposed to return the percentage if percentage was specified.
                return new CSSPrimitiveValue(renderer->marginLeft(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->marginLeft());
        case CSSPropertyWebkitMarqueeDirection:
            return new CSSPrimitiveValue(style->marqueeDirection());
        case CSSPropertyWebkitMarqueeIncrement:
            return new CSSPrimitiveValue(style->marqueeIncrement());
        case CSSPropertyWebkitMarqueeRepetition:
            if (style->marqueeLoopCount() < 0)
                return new CSSPrimitiveValue(CSSValueInfinite);
            return new CSSPrimitiveValue(style->marqueeLoopCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitMarqueeStyle:
            return new CSSPrimitiveValue(style->marqueeBehavior());
        case CSSPropertyWebkitMaskImage:
            if (style->maskImage())
                return style->maskImage()->cssValue();
            return new CSSPrimitiveValue(CSSValueNone);
        case CSSPropertyWebkitMaskSize: {
            RefPtr<CSSValueList> list = new CSSValueList(true);
            list->append(new CSSPrimitiveValue(style->maskSize().width));
            list->append(new CSSPrimitiveValue(style->maskSize().height));
            return list.release();
        }  
        case CSSPropertyWebkitMaskRepeat:
            return new CSSPrimitiveValue(style->maskRepeat());
        case CSSPropertyWebkitMaskAttachment:
            if (style->maskAttachment())
                return new CSSPrimitiveValue(CSSValueScroll);
            return new CSSPrimitiveValue(CSSValueFixed);
        case CSSPropertyWebkitMaskComposite:
            return new CSSPrimitiveValue(style->maskComposite());
        case CSSPropertyWebkitMaskClip:
        case CSSPropertyWebkitMaskOrigin: {
            EFillBox box = (propertyID == CSSPropertyWebkitMaskClip ? style->maskClip() : style->maskOrigin());
            return new CSSPrimitiveValue(box);
        }
        case CSSPropertyWebkitMaskPosition: {
            RefPtr<CSSValueList> list = new CSSValueList(true);

            list->append(new CSSPrimitiveValue(style->maskXPosition()));
            list->append(new CSSPrimitiveValue(style->maskYPosition()));

            return list.release();
        }
        case CSSPropertyWebkitMaskPositionX:
            return new CSSPrimitiveValue(style->maskXPosition());
        case CSSPropertyWebkitMaskPositionY:
            return new CSSPrimitiveValue(style->maskYPosition());
        case CSSPropertyWebkitUserModify:
            return new CSSPrimitiveValue(style->userModify());
        case CSSPropertyMaxHeight: {
            const Length& maxHeight = style->maxHeight();
            if (maxHeight.isFixed() && maxHeight.value() == undefinedLength)
                return new CSSPrimitiveValue(CSSValueNone);
            return new CSSPrimitiveValue(maxHeight);
        }
        case CSSPropertyMaxWidth: {
            const Length& maxWidth = style->maxHeight();
            if (maxWidth.isFixed() && maxWidth.value() == undefinedLength)
                return new CSSPrimitiveValue(CSSValueNone);
            return new CSSPrimitiveValue(maxWidth);
        }
        case CSSPropertyMinHeight:
            return new CSSPrimitiveValue(style->minHeight());
        case CSSPropertyMinWidth:
            return new CSSPrimitiveValue(style->minWidth());
        case CSSPropertyOpacity:
            return new CSSPrimitiveValue(style->opacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOrphans:
            return new CSSPrimitiveValue(style->orphans(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOutlineColor:
            return currentColorOrValidColor(style, style->outlineColor());
        case CSSPropertyOutlineStyle:
            if (style->outlineStyleIsAuto())
                return new CSSPrimitiveValue(CSSValueAuto);
            return new CSSPrimitiveValue(style->outlineStyle());
        case CSSPropertyOutlineWidth:
            return new CSSPrimitiveValue(style->outlineWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyOverflow:
            return new CSSPrimitiveValue(max(style->overflowX(), style->overflowY()));
        case CSSPropertyOverflowX:
            return new CSSPrimitiveValue(style->overflowX());
        case CSSPropertyOverflowY:
            return new CSSPrimitiveValue(style->overflowY());
        case CSSPropertyPaddingTop:
            if (renderer)
                return new CSSPrimitiveValue(renderer->paddingTop(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->paddingTop());
        case CSSPropertyPaddingRight:
            if (renderer)
                return new CSSPrimitiveValue(renderer->paddingRight(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->paddingRight());
        case CSSPropertyPaddingBottom:
            if (renderer)
                return new CSSPrimitiveValue(renderer->paddingBottom(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->paddingBottom());
        case CSSPropertyPaddingLeft:
            if (renderer)
                return new CSSPrimitiveValue(renderer->paddingLeft(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->paddingLeft());
        case CSSPropertyPageBreakAfter:
            return new CSSPrimitiveValue(style->pageBreakAfter());
        case CSSPropertyPageBreakBefore:
            return new CSSPrimitiveValue(style->pageBreakBefore());
        case CSSPropertyPageBreakInside: {
            EPageBreak pageBreak = style->pageBreakInside();
            ASSERT(pageBreak != PBALWAYS);
            if (pageBreak == PBALWAYS)
                return 0;
            return new CSSPrimitiveValue(style->pageBreakInside());
        }
        case CSSPropertyPosition:
            return new CSSPrimitiveValue(style->position());
        case CSSPropertyRight:
            return getPositionOffsetValue(style, CSSPropertyRight);
        case CSSPropertyTableLayout:
            return new CSSPrimitiveValue(style->tableLayout());
        case CSSPropertyTextAlign:
            return new CSSPrimitiveValue(style->textAlign());
        case CSSPropertyTextDecoration: {
            String string;
            if (style->textDecoration() & UNDERLINE)
                string += "underline";
            if (style->textDecoration() & OVERLINE) {
                if (string.length())
                    string += " ";
                string += "overline";
            }
            if (style->textDecoration() & LINE_THROUGH) {
                if (string.length())
                    string += " ";
                string += "line-through";
            }
            if (style->textDecoration() & BLINK) {
                if (string.length())
                    string += " ";
                string += "blink";
            }
            if (!string.length())
                return new CSSPrimitiveValue(CSSValueNone);
            return new CSSPrimitiveValue(string, CSSPrimitiveValue::CSS_STRING);
        }
        case CSSPropertyWebkitTextDecorationsInEffect: {
            String string;
            if (style->textDecorationsInEffect() & UNDERLINE)
                string += "underline";
            if (style->textDecorationsInEffect() & OVERLINE) {
                if (string.length())
                    string += " ";
                string += "overline";
            }
            if (style->textDecorationsInEffect() & LINE_THROUGH) {
                if (string.length())
                    string += " ";
                string += "line-through";
            }
            if (style->textDecorationsInEffect() & BLINK) {
                if (string.length())
                    string += " ";
                string += "blink";
            }
            if (!string.length())
                return new CSSPrimitiveValue(CSSValueNone);
            return new CSSPrimitiveValue(string, CSSPrimitiveValue::CSS_STRING);
        }
        case CSSPropertyWebkitTextFillColor:
            return currentColorOrValidColor(style, style->textFillColor());
        case CSSPropertyTextIndent:
            return new CSSPrimitiveValue(style->textIndent());
        case CSSPropertyTextShadow:
            return valueForShadow(style->textShadow());
        case CSSPropertyWebkitTextSecurity:
            return new CSSPrimitiveValue(style->textSecurity());
        case CSSPropertyWebkitTextSizeAdjust:
            if (style->textSizeAdjust())
                return new CSSPrimitiveValue(CSSValueAuto);
            return new CSSPrimitiveValue(CSSValueNone);
        case CSSPropertyWebkitTextStrokeColor:
            return currentColorOrValidColor(style, style->textStrokeColor());
        case CSSPropertyWebkitTextStrokeWidth:
            return new CSSPrimitiveValue(style->textStrokeWidth(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyTextTransform:
            return new CSSPrimitiveValue(style->textTransform());
        case CSSPropertyTop:
            return getPositionOffsetValue(style, CSSPropertyTop);
        case CSSPropertyUnicodeBidi:
            return new CSSPrimitiveValue(style->unicodeBidi());
        case CSSPropertyVerticalAlign:
            switch (style->verticalAlign()) {
                case BASELINE:
                    return new CSSPrimitiveValue(CSSValueBaseline);
                case MIDDLE:
                    return new CSSPrimitiveValue(CSSValueMiddle);
                case SUB:
                    return new CSSPrimitiveValue(CSSValueSub);
                case SUPER:
                    return new CSSPrimitiveValue(CSSValueSuper);
                case TEXT_TOP:
                    return new CSSPrimitiveValue(CSSValueTextTop);
                case TEXT_BOTTOM:
                    return new CSSPrimitiveValue(CSSValueTextBottom);
                case TOP:
                    return new CSSPrimitiveValue(CSSValueTop);
                case BOTTOM:
                    return new CSSPrimitiveValue(CSSValueBottom);
                case BASELINE_MIDDLE:
                    return new CSSPrimitiveValue(CSSValueWebkitBaselineMiddle);
                case LENGTH:
                    return new CSSPrimitiveValue(style->verticalAlignLength());
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSSPropertyVisibility:
            return new CSSPrimitiveValue(style->visibility());
        case CSSPropertyWhiteSpace:
            return new CSSPrimitiveValue(style->whiteSpace());
        case CSSPropertyWidows:
            return new CSSPrimitiveValue(style->widows(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWidth:
            if (renderer)
                return new CSSPrimitiveValue(sizingBox(renderer).width(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->width());
        case CSSPropertyWordBreak:
            return new CSSPrimitiveValue(style->wordBreak());
        case CSSPropertyWordSpacing:
            return new CSSPrimitiveValue(style->wordSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSSPropertyWordWrap:
            return new CSSPrimitiveValue(style->wordWrap());
        case CSSPropertyWebkitLineBreak:
            return new CSSPrimitiveValue(style->khtmlLineBreak());
        case CSSPropertyWebkitNbspMode:
            return new CSSPrimitiveValue(style->nbspMode());
        case CSSPropertyWebkitMatchNearestMailBlockquoteColor:
            return new CSSPrimitiveValue(style->matchNearestMailBlockquoteColor());
        case CSSPropertyResize:
            return new CSSPrimitiveValue(style->resize());
        case CSSPropertyZIndex:
            if (style->hasAutoZIndex())
                return new CSSPrimitiveValue(CSSValueAuto);
            return new CSSPrimitiveValue(style->zIndex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyZoom:
            return new CSSPrimitiveValue(style->zoom(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxSizing:
            if (style->boxSizing() == CONTENT_BOX)
                return new CSSPrimitiveValue(CSSValueContentBox);
            return new CSSPrimitiveValue(CSSValueBorderBox);
        case CSSPropertyWebkitDashboardRegion:
        {
            const Vector<StyleDashboardRegion>& regions = style->dashboardRegions();
            unsigned count = regions.size();
            if (count == 1 && regions[0].type == StyleDashboardRegion::None)
                return new CSSPrimitiveValue(CSSValueNone);

            RefPtr<DashboardRegion> firstRegion;
            DashboardRegion* previousRegion = 0;
            for (unsigned i = 0; i < count; i++) {
                RefPtr<DashboardRegion> region = new DashboardRegion;
                StyleDashboardRegion styleRegion = regions[i];

                region->m_label = styleRegion.label;
                LengthBox offset = styleRegion.offset;
                region->setTop(new CSSPrimitiveValue(offset.top.value(), CSSPrimitiveValue::CSS_PX));
                region->setRight(new CSSPrimitiveValue(offset.right.value(), CSSPrimitiveValue::CSS_PX));
                region->setBottom(new CSSPrimitiveValue(offset.bottom.value(), CSSPrimitiveValue::CSS_PX));
                region->setLeft(new CSSPrimitiveValue(offset.left.value(), CSSPrimitiveValue::CSS_PX));
                region->m_isRectangle = (styleRegion.type == StyleDashboardRegion::Rectangle);
                region->m_isCircle = (styleRegion.type == StyleDashboardRegion::Circle);

                if (previousRegion)
                    previousRegion->m_next = region;
                else
                    firstRegion = region;
                previousRegion = region.get();
            }
            return new CSSPrimitiveValue(firstRegion.release());
        }
        case CSSPropertyWebkitAppearance:
            return new CSSPrimitiveValue(style->appearance());
        case CSSPropertyWebkitBorderImage:
            return valueForNinePieceImage(style->borderImage());
        case CSSPropertyWebkitMaskBoxImage:
            return valueForNinePieceImage(style->maskBoxImage());
        case CSSPropertyWebkitFontSizeDelta:
            // Not a real style property -- used by the editing engine -- so has no computed value.
            break;
        case CSSPropertyWebkitMarginBottomCollapse:
            return new CSSPrimitiveValue(style->marginBottomCollapse());
        case CSSPropertyWebkitMarginTopCollapse:
            return new CSSPrimitiveValue(style->marginTopCollapse());
        case CSSPropertyWebkitRtlOrdering:
            if (style->visuallyOrdered())
                return new CSSPrimitiveValue(CSSValueVisual);
            return new CSSPrimitiveValue(CSSValueLogical);
        case CSSPropertyWebkitUserDrag:
            return new CSSPrimitiveValue(style->userDrag());
        case CSSPropertyWebkitUserSelect:
            return new CSSPrimitiveValue(style->userSelect());
        case CSSPropertyWebkitBorderBottomLeftRadius:
            return getBorderRadiusCornerValue(style->borderBottomLeftRadius());
        case CSSPropertyWebkitBorderBottomRightRadius:
            return getBorderRadiusCornerValue(style->borderBottomRightRadius());
        case CSSPropertyWebkitBorderTopLeftRadius:
            return getBorderRadiusCornerValue(style->borderTopLeftRadius());
        case CSSPropertyWebkitBorderTopRightRadius:
            return getBorderRadiusCornerValue(style->borderTopRightRadius());
        case CSSPropertyBackground:
        case CSSPropertyBorder:
        case CSSPropertyBorderBottom:
        case CSSPropertyBorderColor:
        case CSSPropertyBorderLeft:
        case CSSPropertyBorderRight:
        case CSSPropertyBorderStyle:
        case CSSPropertyBorderTop:
        case CSSPropertyBorderWidth:
        case CSSPropertyClip:
        case CSSPropertyContent:
        case CSSPropertyCounterIncrement:
        case CSSPropertyCounterReset:
        case CSSPropertyFont:
        case CSSPropertyFontStretch:
        case CSSPropertyListStyle:
        case CSSPropertyMargin:
        case CSSPropertyOutline:
        case CSSPropertyOutlineOffset:
        case CSSPropertyPadding:
        case CSSPropertyPage:
        case CSSPropertyQuotes:
        case CSSPropertyScrollbar3dlightColor:
        case CSSPropertyScrollbarArrowColor:
        case CSSPropertyScrollbarDarkshadowColor:
        case CSSPropertyScrollbarFaceColor:
        case CSSPropertyScrollbarHighlightColor:
        case CSSPropertyScrollbarShadowColor:
        case CSSPropertyScrollbarTrackColor:
        case CSSPropertySrc: // Only used in @font-face rules.
        case CSSPropertySize:
        case CSSPropertyTextLineThrough:
        case CSSPropertyTextLineThroughColor:
        case CSSPropertyTextLineThroughMode:
        case CSSPropertyTextLineThroughStyle:
        case CSSPropertyTextLineThroughWidth:
        case CSSPropertyTextOverflow:
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
        case CSSPropertyUnicodeRange: // Only used in @font-face rules.
        case CSSPropertyWebkitBorderRadius:
        case CSSPropertyWebkitColumns:
        case CSSPropertyWebkitColumnRule:
        case CSSPropertyWebkitMarginCollapse:
        case CSSPropertyWebkitMarginStart:
        case CSSPropertyWebkitMarquee:
        case CSSPropertyWebkitMarqueeSpeed:
        case CSSPropertyWebkitPaddingStart:
        case CSSPropertyWebkitTextStroke:
        case CSSPropertyWebkitTransform:
        case CSSPropertyWebkitTransformOrigin:
        case CSSPropertyWebkitTransformOriginX:
        case CSSPropertyWebkitTransformOriginY:
        case CSSPropertyWebkitTransition:
        case CSSPropertyWebkitTransitionDuration:
        case CSSPropertyWebkitTransitionProperty:
        case CSSPropertyWebkitTransitionRepeatCount:
        case CSSPropertyWebkitTransitionTimingFunction:
            // FIXME: The above are unimplemented.
            break;
#if ENABLE(SVG)
        default:
            return getSVGPropertyCSSValue(propertyID, DoNotUpdateLayout);
#endif
    }

    LOG_ERROR("unimplemented propertyID: %d", propertyID);
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

unsigned CSSComputedStyleDeclaration::length() const
{
    Node* node = m_node.get();
    if (!node)
        return 0;

    RenderStyle* style = node->computedStyle();
    if (!style)
        return 0;

    return numComputedProperties;
}

String CSSComputedStyleDeclaration::item(unsigned i) const
{
    if (i >= length())
        return String();

    return getPropertyName(static_cast<CSSPropertyID>(computedProperties[i]));
}

// This is the list of properties we want to copy in the copyInheritableProperties() function.
// It is the intersection of the list of inherited CSS properties and the
// properties for which we have a computed implementation in this file.
const int inheritableProperties[] = {
    CSSPropertyBorderCollapse,
    CSSPropertyColor,
    CSSPropertyFontFamily,
    CSSPropertyFontSize,
    CSSPropertyFontStyle,
    CSSPropertyFontVariant,
    CSSPropertyFontWeight,
    CSSPropertyLetterSpacing,
    CSSPropertyLineHeight,
    CSSPropertyOrphans,
    CSSPropertyTextAlign,
    CSSPropertyTextIndent,
    CSSPropertyTextTransform,
    CSSPropertyWhiteSpace,
    CSSPropertyWidows,
    CSSPropertyWordSpacing,
    CSSPropertyWebkitBorderHorizontalSpacing,
    CSSPropertyWebkitBorderVerticalSpacing,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextFillColor,
    CSSPropertyWebkitTextSizeAdjust,
    CSSPropertyWebkitTextStrokeColor,
    CSSPropertyWebkitTextStrokeWidth,
};

const unsigned numInheritableProperties = sizeof(inheritableProperties) / sizeof(inheritableProperties[0]);

void CSSComputedStyleDeclaration::removeComputedInheritablePropertiesFrom(CSSMutableStyleDeclaration* declaration)
{
    declaration->removePropertiesInSet(inheritableProperties, numInheritableProperties);
}

PassRefPtr<CSSMutableStyleDeclaration> CSSComputedStyleDeclaration::copyInheritableProperties() const
{
    RefPtr<CSSMutableStyleDeclaration> style = copyPropertiesInSet(inheritableProperties, numInheritableProperties);
    if (style && m_node && m_node->computedStyle()) {
        // If a node's text fill color is invalid, then its children use 
        // their font-color as their text fill color (they don't
        // inherit it).  Likewise for stroke color.
        ExceptionCode ec = 0;
        if (!m_node->computedStyle()->textFillColor().isValid())
            style->removeProperty(CSSPropertyWebkitTextFillColor, ec);
        if (!m_node->computedStyle()->textStrokeColor().isValid())
            style->removeProperty(CSSPropertyWebkitTextStrokeColor, ec);
        ASSERT(ec == 0);
    }
    return style.release();
}

PassRefPtr<CSSMutableStyleDeclaration> CSSComputedStyleDeclaration::copy() const
{
    return copyPropertiesInSet(computedProperties, numComputedProperties);
}

PassRefPtr<CSSMutableStyleDeclaration> CSSComputedStyleDeclaration::makeMutable()
{
    return copy();
}

PassRefPtr<CSSComputedStyleDeclaration> computedStyle(Node* node)
{
    return new CSSComputedStyleDeclaration(node);
}

} // namespace WebCore
