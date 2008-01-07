/**
 *
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
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
    CSS_PROP_BACKGROUND_ATTACHMENT,
    CSS_PROP_BACKGROUND_COLOR,
    CSS_PROP_BACKGROUND_IMAGE,
    // more specific background-position-x/y are non-standard
    CSS_PROP_BACKGROUND_POSITION,
    CSS_PROP_BACKGROUND_REPEAT,
    CSS_PROP_BORDER_BOTTOM_COLOR,
    CSS_PROP_BORDER_BOTTOM_STYLE,
    CSS_PROP_BORDER_BOTTOM_WIDTH,
    CSS_PROP_BORDER_COLLAPSE,
    CSS_PROP_BORDER_LEFT_COLOR,
    CSS_PROP_BORDER_LEFT_STYLE,
    CSS_PROP_BORDER_LEFT_WIDTH,
    CSS_PROP_BORDER_RIGHT_COLOR,
    CSS_PROP_BORDER_RIGHT_STYLE,
    CSS_PROP_BORDER_RIGHT_WIDTH,
    CSS_PROP_BORDER_TOP_COLOR,
    CSS_PROP_BORDER_TOP_STYLE,
    CSS_PROP_BORDER_TOP_WIDTH,
    CSS_PROP_BOTTOM,
    CSS_PROP_CAPTION_SIDE,
    CSS_PROP_CLEAR,
    CSS_PROP_COLOR,
    CSS_PROP_CURSOR,
    CSS_PROP_DIRECTION,
    CSS_PROP_DISPLAY,
    CSS_PROP_EMPTY_CELLS,
    CSS_PROP_FLOAT,
    CSS_PROP_FONT_FAMILY,
    CSS_PROP_FONT_SIZE,
    CSS_PROP_FONT_STYLE,
    CSS_PROP_FONT_VARIANT,
    CSS_PROP_FONT_WEIGHT,
    CSS_PROP_HEIGHT,
    CSS_PROP_LEFT,
    CSS_PROP_LETTER_SPACING,
    CSS_PROP_LINE_HEIGHT,
    CSS_PROP_LIST_STYLE_IMAGE,
    CSS_PROP_LIST_STYLE_POSITION,
    CSS_PROP_LIST_STYLE_TYPE,
    CSS_PROP_MARGIN_BOTTOM,
    CSS_PROP_MARGIN_LEFT,
    CSS_PROP_MARGIN_RIGHT,
    CSS_PROP_MARGIN_TOP,
    CSS_PROP_MAX_HEIGHT,
    CSS_PROP_MAX_WIDTH,
    CSS_PROP_MIN_HEIGHT,
    CSS_PROP_MIN_WIDTH,
    CSS_PROP_OPACITY,
    CSS_PROP_ORPHANS,
    CSS_PROP_OUTLINE_COLOR,
    CSS_PROP_OUTLINE_STYLE,
    CSS_PROP_OUTLINE_WIDTH,
    CSS_PROP_OVERFLOW_X,
    CSS_PROP_OVERFLOW_Y,
    CSS_PROP_PADDING_BOTTOM,
    CSS_PROP_PADDING_LEFT,
    CSS_PROP_PADDING_RIGHT,
    CSS_PROP_PADDING_TOP,
    CSS_PROP_PAGE_BREAK_AFTER,
    CSS_PROP_PAGE_BREAK_BEFORE,
    CSS_PROP_PAGE_BREAK_INSIDE,
    CSS_PROP_POSITION,
    CSS_PROP_RESIZE,
    CSS_PROP_RIGHT,
    CSS_PROP_TABLE_LAYOUT,
    CSS_PROP_TEXT_ALIGN,
    CSS_PROP_TEXT_DECORATION,
    CSS_PROP_TEXT_INDENT,
    CSS_PROP_TEXT_SHADOW,
    CSS_PROP_TEXT_TRANSFORM,
    CSS_PROP_TOP,
    CSS_PROP_UNICODE_BIDI,
    CSS_PROP_VERTICAL_ALIGN,
    CSS_PROP_VISIBILITY,
    CSS_PROP_WHITE_SPACE,
    CSS_PROP_WIDOWS,
    CSS_PROP_WIDTH,
    CSS_PROP_WORD_SPACING,
    CSS_PROP_WORD_WRAP,
    CSS_PROP_Z_INDEX,

    CSS_PROP__WEBKIT_APPEARANCE,
    CSS_PROP__WEBKIT_BACKGROUND_CLIP,
    CSS_PROP__WEBKIT_BACKGROUND_COMPOSITE,
    CSS_PROP__WEBKIT_BACKGROUND_ORIGIN,
    CSS_PROP__WEBKIT_BACKGROUND_SIZE,
    CSS_PROP__WEBKIT_BORDER_FIT,
    CSS_PROP__WEBKIT_BORDER_HORIZONTAL_SPACING,
    CSS_PROP__WEBKIT_BORDER_VERTICAL_SPACING,
    CSS_PROP__WEBKIT_BOX_ALIGN,
    CSS_PROP__WEBKIT_BOX_DIRECTION,
    CSS_PROP__WEBKIT_BOX_FLEX,
    CSS_PROP__WEBKIT_BOX_FLEX_GROUP,
    CSS_PROP__WEBKIT_BOX_LINES,
    CSS_PROP__WEBKIT_BOX_ORDINAL_GROUP,
    CSS_PROP__WEBKIT_BOX_ORIENT,
    CSS_PROP__WEBKIT_BOX_PACK,
    CSS_PROP__WEBKIT_BOX_SHADOW,
    CSS_PROP__WEBKIT_BOX_SIZING,
    CSS_PROP__WEBKIT_COLUMN_BREAK_AFTER,
    CSS_PROP__WEBKIT_COLUMN_BREAK_BEFORE,
    CSS_PROP__WEBKIT_COLUMN_BREAK_INSIDE,
    CSS_PROP__WEBKIT_COLUMN_COUNT,
    CSS_PROP__WEBKIT_COLUMN_GAP,
    CSS_PROP__WEBKIT_COLUMN_RULE_COLOR,
    CSS_PROP__WEBKIT_COLUMN_RULE_STYLE,
    CSS_PROP__WEBKIT_COLUMN_RULE_WIDTH,
    CSS_PROP__WEBKIT_COLUMN_WIDTH,
    CSS_PROP__WEBKIT_HIGHLIGHT,
    CSS_PROP__WEBKIT_LINE_BREAK,
    CSS_PROP__WEBKIT_LINE_CLAMP,
    CSS_PROP__WEBKIT_MARGIN_BOTTOM_COLLAPSE,
    CSS_PROP__WEBKIT_MARGIN_TOP_COLLAPSE,
    CSS_PROP__WEBKIT_MARQUEE_DIRECTION,
    CSS_PROP__WEBKIT_MARQUEE_INCREMENT,
    CSS_PROP__WEBKIT_MARQUEE_REPETITION,
    CSS_PROP__WEBKIT_MARQUEE_STYLE,
    CSS_PROP__WEBKIT_NBSP_MODE,
    CSS_PROP__WEBKIT_RTL_ORDERING,
    CSS_PROP__WEBKIT_TEXT_DECORATIONS_IN_EFFECT,
    CSS_PROP__WEBKIT_TEXT_FILL_COLOR,
    CSS_PROP__WEBKIT_TEXT_SECURITY,
    CSS_PROP__WEBKIT_TEXT_STROKE_COLOR,
    CSS_PROP__WEBKIT_TEXT_STROKE_WIDTH,
    CSS_PROP__WEBKIT_USER_DRAG,
    CSS_PROP__WEBKIT_USER_MODIFY,
    CSS_PROP__WEBKIT_USER_SELECT,
    CSS_PROP__WEBKIT_DASHBOARD_REGION,
    CSS_PROP__WEBKIT_BORDER_BOTTOM_LEFT_RADIUS,
    CSS_PROP__WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS,
    CSS_PROP__WEBKIT_BORDER_TOP_LEFT_RADIUS,
    CSS_PROP__WEBKIT_BORDER_TOP_RIGHT_RADIUS
    
#if ENABLE(SVG)
    ,
    CSS_PROP_CLIP_PATH,
    CSS_PROP_CLIP_RULE,
    CSS_PROP_MASK,
    CSS_PROP_FILTER,
    CSS_PROP_FLOOD_COLOR,
    CSS_PROP_FLOOD_OPACITY,
    CSS_PROP_LIGHTING_COLOR,
    CSS_PROP_STOP_COLOR,
    CSS_PROP_STOP_OPACITY,
    CSS_PROP_POINTER_EVENTS,
    CSS_PROP_COLOR_INTERPOLATION,
    CSS_PROP_COLOR_INTERPOLATION_FILTERS,
    CSS_PROP_COLOR_RENDERING,
    CSS_PROP_FILL,
    CSS_PROP_FILL_OPACITY,
    CSS_PROP_FILL_RULE,
    CSS_PROP_IMAGE_RENDERING,
    CSS_PROP_MARKER_END,
    CSS_PROP_MARKER_MID,
    CSS_PROP_MARKER_START,
    CSS_PROP_SHAPE_RENDERING,
    CSS_PROP_STROKE,
    CSS_PROP_STROKE_DASHARRAY,
    CSS_PROP_STROKE_DASHOFFSET,
    CSS_PROP_STROKE_LINECAP,
    CSS_PROP_STROKE_LINEJOIN,
    CSS_PROP_STROKE_MITERLIMIT,
    CSS_PROP_STROKE_OPACITY,
    CSS_PROP_STROKE_WIDTH,
    CSS_PROP_TEXT_RENDERING,
    CSS_PROP_ALIGNMENT_BASELINE,
    CSS_PROP_BASELINE_SHIFT,
    CSS_PROP_DOMINANT_BASELINE,
    CSS_PROP_KERNING,
    CSS_PROP_TEXT_ANCHOR,
    CSS_PROP_WRITING_MODE,
    CSS_PROP_GLYPH_ORIENTATION_HORIZONTAL,
    CSS_PROP_GLYPH_ORIENTATION_VERTICAL
#endif
};

const unsigned numComputedProperties = sizeof(computedProperties) / sizeof(computedProperties[0]);

static PassRefPtr<CSSValue> valueForShadow(const ShadowData* shadow)
{
    if (!shadow)
        return new CSSPrimitiveValue(CSS_VAL_NONE);

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

static PassRefPtr<CSSValue> getPositionOffsetValue(RenderStyle* style, int propertyID)
{
    if (!style)
        return 0;

    Length l;
    switch (propertyID) {
        case CSS_PROP_LEFT:
            l = style->left();
            break;
        case CSS_PROP_RIGHT:
            l = style->right();
            break;
        case CSS_PROP_TOP:
            l = style->top();
            break;
        case CSS_PROP_BOTTOM:
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

    return new CSSPrimitiveValue(CSS_VAL_AUTO);
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
        case CSS_PROP_INVALID:
            break;

        case CSS_PROP_BACKGROUND_COLOR:
            return new CSSPrimitiveValue(style->backgroundColor().rgb());
        case CSS_PROP_BACKGROUND_IMAGE:
            if (style->backgroundImage())
                return new CSSPrimitiveValue(style->backgroundImage()->url(), CSSPrimitiveValue::CSS_URI);
            return new CSSPrimitiveValue(CSS_VAL_NONE);
        case CSS_PROP__WEBKIT_BACKGROUND_SIZE: {
            RefPtr<CSSValueList> list = new CSSValueList(true);
            list->append(new CSSPrimitiveValue(style->backgroundSize().width));
            list->append(new CSSPrimitiveValue(style->backgroundSize().height));
            return list.release();
        }  
        case CSS_PROP_BACKGROUND_REPEAT:
            return new CSSPrimitiveValue(style->backgroundRepeat());
        case CSS_PROP__WEBKIT_BACKGROUND_COMPOSITE:
            return new CSSPrimitiveValue(style->backgroundComposite());
        case CSS_PROP_BACKGROUND_ATTACHMENT:
            if (style->backgroundAttachment())
                return new CSSPrimitiveValue(CSS_VAL_SCROLL);
            return new CSSPrimitiveValue(CSS_VAL_FIXED);
        case CSS_PROP__WEBKIT_BACKGROUND_CLIP:
        case CSS_PROP__WEBKIT_BACKGROUND_ORIGIN: {
            EBackgroundBox box = (propertyID == CSS_PROP__WEBKIT_BACKGROUND_CLIP ? style->backgroundClip() : style->backgroundOrigin());
            return new CSSPrimitiveValue(box);
        }
        case CSS_PROP_BACKGROUND_POSITION: {
            RefPtr<CSSValueList> list = new CSSValueList(true);

            list->append(new CSSPrimitiveValue(style->backgroundXPosition()));
            list->append(new CSSPrimitiveValue(style->backgroundYPosition()));

            return list.release();
        }
        case CSS_PROP_BACKGROUND_POSITION_X:
            return new CSSPrimitiveValue(style->backgroundXPosition());
        case CSS_PROP_BACKGROUND_POSITION_Y:
            return new CSSPrimitiveValue(style->backgroundYPosition());
        case CSS_PROP_BORDER_COLLAPSE:
            if (style->borderCollapse())
                return new CSSPrimitiveValue(CSS_VAL_COLLAPSE);
            return new CSSPrimitiveValue(CSS_VAL_SEPARATE);
        case CSS_PROP_BORDER_SPACING: {
            RefPtr<CSSValueList> list = new CSSValueList(true);
            list->append(new CSSPrimitiveValue(style->horizontalBorderSpacing(), CSSPrimitiveValue::CSS_PX));
            list->append(new CSSPrimitiveValue(style->verticalBorderSpacing(), CSSPrimitiveValue::CSS_PX));
            return list.release();
        }  
        case CSS_PROP__WEBKIT_BORDER_HORIZONTAL_SPACING:
            return new CSSPrimitiveValue(style->horizontalBorderSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP__WEBKIT_BORDER_VERTICAL_SPACING:
            return new CSSPrimitiveValue(style->verticalBorderSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP_BORDER_TOP_COLOR:
            return currentColorOrValidColor(style, style->borderTopColor());
        case CSS_PROP_BORDER_RIGHT_COLOR:
            return currentColorOrValidColor(style, style->borderRightColor());
        case CSS_PROP_BORDER_BOTTOM_COLOR:
            return currentColorOrValidColor(style, style->borderBottomColor());
        case CSS_PROP_BORDER_LEFT_COLOR:
            return currentColorOrValidColor(style, style->borderLeftColor());
        case CSS_PROP_BORDER_TOP_STYLE:
            return new CSSPrimitiveValue(style->borderTopStyle());
        case CSS_PROP_BORDER_RIGHT_STYLE:
            return new CSSPrimitiveValue(style->borderRightStyle());
        case CSS_PROP_BORDER_BOTTOM_STYLE:
            return new CSSPrimitiveValue(style->borderBottomStyle());
        case CSS_PROP_BORDER_LEFT_STYLE:
            return new CSSPrimitiveValue(style->borderLeftStyle());
        case CSS_PROP_BORDER_TOP_WIDTH:
            return new CSSPrimitiveValue(style->borderTopWidth(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP_BORDER_RIGHT_WIDTH:
            return new CSSPrimitiveValue(style->borderRightWidth(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP_BORDER_BOTTOM_WIDTH:
            return new CSSPrimitiveValue(style->borderBottomWidth(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP_BORDER_LEFT_WIDTH:
            return new CSSPrimitiveValue(style->borderLeftWidth(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP_BOTTOM:
            return getPositionOffsetValue(style, CSS_PROP_BOTTOM);
        case CSS_PROP__WEBKIT_BOX_ALIGN:
            return new CSSPrimitiveValue(style->boxAlign());
        case CSS_PROP__WEBKIT_BOX_DIRECTION:
            return new CSSPrimitiveValue(style->boxDirection());
        case CSS_PROP__WEBKIT_BOX_FLEX:
            return new CSSPrimitiveValue(style->boxFlex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP__WEBKIT_BOX_FLEX_GROUP:
            return new CSSPrimitiveValue(style->boxFlexGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP__WEBKIT_BOX_LINES:
            return new CSSPrimitiveValue(style->boxLines());
        case CSS_PROP__WEBKIT_BOX_ORDINAL_GROUP:
            return new CSSPrimitiveValue(style->boxOrdinalGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP__WEBKIT_BOX_ORIENT:
            return new CSSPrimitiveValue(style->boxOrient());
        case CSS_PROP__WEBKIT_BOX_PACK: {
            EBoxAlignment boxPack = style->boxPack();
            ASSERT(boxPack != BSTRETCH);
            ASSERT(boxPack != BBASELINE);
            if (boxPack == BJUSTIFY || boxPack== BBASELINE)
                return 0;
            return new CSSPrimitiveValue(boxPack);
        }
        case CSS_PROP__WEBKIT_BOX_SHADOW:
            return valueForShadow(style->boxShadow());
        case CSS_PROP_CAPTION_SIDE:
            return new CSSPrimitiveValue(style->captionSide());
        case CSS_PROP_CLEAR:
            return new CSSPrimitiveValue(style->clear());
        case CSS_PROP_COLOR:
            return new CSSPrimitiveValue(style->color().rgb());
        case CSS_PROP__WEBKIT_COLUMN_COUNT:
            if (style->hasAutoColumnCount())
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            return new CSSPrimitiveValue(style->columnCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP__WEBKIT_COLUMN_GAP:
            if (style->hasNormalColumnGap())
                return new CSSPrimitiveValue(CSS_VAL_NORMAL);
            return new CSSPrimitiveValue(style->columnGap(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP__WEBKIT_COLUMN_RULE_COLOR:
            return currentColorOrValidColor(style, style->columnRuleColor());
        case CSS_PROP__WEBKIT_COLUMN_RULE_STYLE:
            return new CSSPrimitiveValue(style->columnRuleStyle());
        case CSS_PROP__WEBKIT_COLUMN_RULE_WIDTH:
            return new CSSPrimitiveValue(style->columnRuleWidth(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP__WEBKIT_COLUMN_BREAK_AFTER:
            return new CSSPrimitiveValue(style->columnBreakAfter());
        case CSS_PROP__WEBKIT_COLUMN_BREAK_BEFORE:
            return new CSSPrimitiveValue(style->columnBreakBefore());
        case CSS_PROP__WEBKIT_COLUMN_BREAK_INSIDE:
            return new CSSPrimitiveValue(style->columnBreakInside());
        case CSS_PROP__WEBKIT_COLUMN_WIDTH:
            if (style->hasAutoColumnWidth())
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            return new CSSPrimitiveValue(style->columnWidth(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_CURSOR: {
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
        case CSS_PROP_DIRECTION:
            return new CSSPrimitiveValue(style->direction());
        case CSS_PROP_DISPLAY:
            return new CSSPrimitiveValue(style->display());
        case CSS_PROP_EMPTY_CELLS:
            return new CSSPrimitiveValue(style->emptyCells());
        case CSS_PROP_FLOAT:
            return new CSSPrimitiveValue(style->floating());
        case CSS_PROP_FONT_FAMILY:
            // FIXME: This only returns the first family.
            return new CSSPrimitiveValue(style->fontDescription().family().family().domString(), CSSPrimitiveValue::CSS_STRING);
        case CSS_PROP_FONT_SIZE:
            return new CSSPrimitiveValue(style->fontDescription().computedPixelSize(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP__WEBKIT_BINDING:
            break;
        case CSS_PROP_FONT_STYLE:
            if (style->fontDescription().italic())
                return new CSSPrimitiveValue(CSS_VAL_ITALIC);
            return new CSSPrimitiveValue(CSS_VAL_NORMAL);
        case CSS_PROP_FONT_VARIANT:
            if (style->fontDescription().smallCaps())
                return new CSSPrimitiveValue(CSS_VAL_SMALL_CAPS);
            return new CSSPrimitiveValue(CSS_VAL_NORMAL);
        case CSS_PROP_FONT_WEIGHT:
            // FIXME: this does not reflect the full range of weights
            // that can be expressed with CSS
            if (style->fontDescription().weight() == cBoldWeight)
                return new CSSPrimitiveValue(CSS_VAL_BOLD);
            return new CSSPrimitiveValue(CSS_VAL_NORMAL);
        case CSS_PROP_HEIGHT:
            if (renderer)
                return new CSSPrimitiveValue(sizingBox(renderer).height(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->height());
        case CSS_PROP__WEBKIT_HIGHLIGHT:
            if (style->highlight() == nullAtom)
                return new CSSPrimitiveValue(CSS_VAL_NONE);
            return new CSSPrimitiveValue(style->highlight(), CSSPrimitiveValue::CSS_STRING);
        case CSS_PROP__WEBKIT_BORDER_FIT:
            if (style->borderFit() == BorderFitBorder)
                return new CSSPrimitiveValue(CSS_VAL_BORDER);
            return new CSSPrimitiveValue(CSS_VAL_LINES);
        case CSS_PROP_LEFT:
            return getPositionOffsetValue(style, CSS_PROP_LEFT);
        case CSS_PROP_LETTER_SPACING:
            if (!style->letterSpacing())
                return new CSSPrimitiveValue(CSS_VAL_NORMAL);
            return new CSSPrimitiveValue(style->letterSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP__WEBKIT_LINE_CLAMP:
            if (style->lineClamp() == -1)
                return new CSSPrimitiveValue(CSS_VAL_NONE);
            return new CSSPrimitiveValue(style->lineClamp(), CSSPrimitiveValue::CSS_PERCENTAGE);
        case CSS_PROP_LINE_HEIGHT: {
            Length length = style->lineHeight();
            if (length.isNegative())
                return new CSSPrimitiveValue(CSS_VAL_NORMAL);
            if (length.isPercent())
                // This is imperfect, because it doesn't include the zoom factor and the real computation
                // for how high to be in pixels does include things like minimum font size and the zoom factor.
                // On the other hand, since font-size doesn't include the zoom factor, we really can't do
                // that here either.
                return new CSSPrimitiveValue(static_cast<int>(length.percent() * style->fontDescription().specifiedSize()) / 100, CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(length.value(), CSSPrimitiveValue::CSS_PX);
        }
        case CSS_PROP_LIST_STYLE_IMAGE:
            if (style->listStyleImage())
                return new CSSPrimitiveValue(style->listStyleImage()->url(), CSSPrimitiveValue::CSS_URI);
            return new CSSPrimitiveValue(CSS_VAL_NONE);
        case CSS_PROP_LIST_STYLE_POSITION:
            return new CSSPrimitiveValue(style->listStylePosition());
        case CSS_PROP_LIST_STYLE_TYPE:
            return new CSSPrimitiveValue(style->listStyleType());
        case CSS_PROP_MARGIN_TOP:
            if (renderer)
                // FIXME: Supposed to return the percentage if percentage was specified.
                return new CSSPrimitiveValue(renderer->marginTop(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->marginTop());
        case CSS_PROP_MARGIN_RIGHT:
            if (renderer)
                // FIXME: Supposed to return the percentage if percentage was specified.
                return new CSSPrimitiveValue(renderer->marginRight(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->marginRight());
        case CSS_PROP_MARGIN_BOTTOM:
            if (renderer)
                // FIXME: Supposed to return the percentage if percentage was specified.
                return new CSSPrimitiveValue(renderer->marginBottom(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->marginBottom());
        case CSS_PROP_MARGIN_LEFT:
            if (renderer)
                // FIXME: Supposed to return the percentage if percentage was specified.
                return new CSSPrimitiveValue(renderer->marginLeft(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->marginLeft());
        case CSS_PROP__WEBKIT_MARQUEE_DIRECTION:
            return new CSSPrimitiveValue(style->marqueeDirection());
        case CSS_PROP__WEBKIT_MARQUEE_INCREMENT:
            return new CSSPrimitiveValue(style->marqueeIncrement());
        case CSS_PROP__WEBKIT_MARQUEE_REPETITION:
            if (style->marqueeLoopCount() < 0)
                return new CSSPrimitiveValue(CSS_VAL_INFINITE);
            return new CSSPrimitiveValue(style->marqueeLoopCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP__WEBKIT_MARQUEE_STYLE:
            return new CSSPrimitiveValue(style->marqueeBehavior());
        case CSS_PROP__WEBKIT_USER_MODIFY:
            return new CSSPrimitiveValue(style->userModify());
        case CSS_PROP_MAX_HEIGHT: {
            const Length& maxHeight = style->maxHeight();
            if (maxHeight.isFixed() && maxHeight.value() == undefinedLength)
                return new CSSPrimitiveValue(CSS_VAL_NONE);
            return new CSSPrimitiveValue(maxHeight);
        }
        case CSS_PROP_MAX_WIDTH: {
            const Length& maxWidth = style->maxHeight();
            if (maxWidth.isFixed() && maxWidth.value() == undefinedLength)
                return new CSSPrimitiveValue(CSS_VAL_NONE);
            return new CSSPrimitiveValue(maxWidth);
        }
        case CSS_PROP_MIN_HEIGHT:
            return new CSSPrimitiveValue(style->minHeight());
        case CSS_PROP_MIN_WIDTH:
            return new CSSPrimitiveValue(style->minWidth());
        case CSS_PROP_OPACITY:
            return new CSSPrimitiveValue(style->opacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_ORPHANS:
            return new CSSPrimitiveValue(style->orphans(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_OUTLINE_COLOR:
            return currentColorOrValidColor(style, style->outlineColor());
        case CSS_PROP_OUTLINE_STYLE:
            if (style->outlineStyleIsAuto())
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            return new CSSPrimitiveValue(style->outlineStyle());
        case CSS_PROP_OUTLINE_WIDTH:
            return new CSSPrimitiveValue(style->outlineWidth(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP_OVERFLOW:
            return new CSSPrimitiveValue(max(style->overflowX(), style->overflowY()));
        case CSS_PROP_OVERFLOW_X:
            return new CSSPrimitiveValue(style->overflowX());
        case CSS_PROP_OVERFLOW_Y:
            return new CSSPrimitiveValue(style->overflowY());
        case CSS_PROP_PADDING_TOP:
            if (renderer)
                return new CSSPrimitiveValue(renderer->paddingTop(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->paddingTop());
        case CSS_PROP_PADDING_RIGHT:
            if (renderer)
                return new CSSPrimitiveValue(renderer->paddingRight(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->paddingRight());
        case CSS_PROP_PADDING_BOTTOM:
            if (renderer)
                return new CSSPrimitiveValue(renderer->paddingBottom(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->paddingBottom());
        case CSS_PROP_PADDING_LEFT:
            if (renderer)
                return new CSSPrimitiveValue(renderer->paddingLeft(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->paddingLeft());
        case CSS_PROP_PAGE_BREAK_AFTER:
            return new CSSPrimitiveValue(style->pageBreakAfter());
        case CSS_PROP_PAGE_BREAK_BEFORE:
            return new CSSPrimitiveValue(style->pageBreakBefore());
        case CSS_PROP_PAGE_BREAK_INSIDE: {
            EPageBreak pageBreak = style->pageBreakInside();
            ASSERT(pageBreak != PBALWAYS);
            if (pageBreak == PBALWAYS)
                return 0;
            return new CSSPrimitiveValue(style->pageBreakInside());
        }
        case CSS_PROP_POSITION:
            return new CSSPrimitiveValue(style->position());
        case CSS_PROP_RIGHT:
            return getPositionOffsetValue(style, CSS_PROP_RIGHT);
        case CSS_PROP_TABLE_LAYOUT:
            return new CSSPrimitiveValue(style->tableLayout());
        case CSS_PROP_TEXT_ALIGN:
            return new CSSPrimitiveValue(style->textAlign());
        case CSS_PROP_TEXT_DECORATION: {
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
                return new CSSPrimitiveValue(CSS_VAL_NONE);
            return new CSSPrimitiveValue(string, CSSPrimitiveValue::CSS_STRING);
        }
        case CSS_PROP__WEBKIT_TEXT_DECORATIONS_IN_EFFECT: {
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
                return new CSSPrimitiveValue(CSS_VAL_NONE);
            return new CSSPrimitiveValue(string, CSSPrimitiveValue::CSS_STRING);
        }
        case CSS_PROP__WEBKIT_TEXT_FILL_COLOR:
            return currentColorOrValidColor(style, style->textFillColor());
        case CSS_PROP_TEXT_INDENT:
            return new CSSPrimitiveValue(style->textIndent());
        case CSS_PROP_TEXT_SHADOW:
            return valueForShadow(style->textShadow());
        case CSS_PROP__WEBKIT_TEXT_SECURITY:
            return new CSSPrimitiveValue(style->textSecurity());
        case CSS_PROP__WEBKIT_TEXT_SIZE_ADJUST:
            if (style->textSizeAdjust())
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            return new CSSPrimitiveValue(CSS_VAL_NONE);
        case CSS_PROP__WEBKIT_TEXT_STROKE_COLOR:
            return currentColorOrValidColor(style, style->textStrokeColor());
        case CSS_PROP__WEBKIT_TEXT_STROKE_WIDTH:
            return new CSSPrimitiveValue(style->textStrokeWidth(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_TEXT_TRANSFORM:
            return new CSSPrimitiveValue(style->textTransform());
        case CSS_PROP_TOP:
            return getPositionOffsetValue(style, CSS_PROP_TOP);
        case CSS_PROP_UNICODE_BIDI:
            return new CSSPrimitiveValue(style->unicodeBidi());
        case CSS_PROP_VERTICAL_ALIGN:
            switch (style->verticalAlign()) {
                case BASELINE:
                    return new CSSPrimitiveValue(CSS_VAL_BASELINE);
                case MIDDLE:
                    return new CSSPrimitiveValue(CSS_VAL_MIDDLE);
                case SUB:
                    return new CSSPrimitiveValue(CSS_VAL_SUB);
                case SUPER:
                    return new CSSPrimitiveValue(CSS_VAL_SUPER);
                case TEXT_TOP:
                    return new CSSPrimitiveValue(CSS_VAL_TEXT_TOP);
                case TEXT_BOTTOM:
                    return new CSSPrimitiveValue(CSS_VAL_TEXT_BOTTOM);
                case TOP:
                    return new CSSPrimitiveValue(CSS_VAL_TOP);
                case BOTTOM:
                    return new CSSPrimitiveValue(CSS_VAL_BOTTOM);
                case BASELINE_MIDDLE:
                    return new CSSPrimitiveValue(CSS_VAL__WEBKIT_BASELINE_MIDDLE);
                case LENGTH:
                    return new CSSPrimitiveValue(style->verticalAlignLength());
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_VISIBILITY:
            return new CSSPrimitiveValue(style->visibility());
        case CSS_PROP_WHITE_SPACE:
            return new CSSPrimitiveValue(style->whiteSpace());
        case CSS_PROP_WIDOWS:
            return new CSSPrimitiveValue(style->widows(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_WIDTH:
            if (renderer)
                return new CSSPrimitiveValue(sizingBox(renderer).width(), CSSPrimitiveValue::CSS_PX);
            return new CSSPrimitiveValue(style->width());
        case CSS_PROP_WORD_BREAK:
            return new CSSPrimitiveValue(style->wordBreak());
        case CSS_PROP_WORD_SPACING:
            return new CSSPrimitiveValue(style->wordSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP_WORD_WRAP:
            return new CSSPrimitiveValue(style->wordWrap());
        case CSS_PROP__WEBKIT_LINE_BREAK:
            return new CSSPrimitiveValue(style->khtmlLineBreak());
        case CSS_PROP__WEBKIT_NBSP_MODE:
            return new CSSPrimitiveValue(style->nbspMode());
        case CSS_PROP__WEBKIT_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR:
            return new CSSPrimitiveValue(style->matchNearestMailBlockquoteColor());
        case CSS_PROP_RESIZE:
            return new CSSPrimitiveValue(style->resize());
        case CSS_PROP_Z_INDEX:
            if (style->hasAutoZIndex())
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            return new CSSPrimitiveValue(style->zIndex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP__WEBKIT_BOX_SIZING:
            if (style->boxSizing() == CONTENT_BOX)
                return new CSSPrimitiveValue(CSS_VAL_CONTENT_BOX);
            return new CSSPrimitiveValue(CSS_VAL_BORDER_BOX);
        case CSS_PROP__WEBKIT_DASHBOARD_REGION:
        {
            const Vector<StyleDashboardRegion>& regions = style->dashboardRegions();
            unsigned count = regions.size();
            if (count == 1 && regions[0].type == StyleDashboardRegion::None)
                return new CSSPrimitiveValue(CSS_VAL_NONE);

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
        case CSS_PROP__WEBKIT_APPEARANCE:
            return new CSSPrimitiveValue(style->appearance());
        case CSS_PROP__WEBKIT_FONT_SIZE_DELTA:
            // Not a real style property -- used by the editing engine -- so has no computed value.
            break;
        case CSS_PROP__WEBKIT_MARGIN_BOTTOM_COLLAPSE:
            return new CSSPrimitiveValue(style->marginBottomCollapse());
        case CSS_PROP__WEBKIT_MARGIN_TOP_COLLAPSE:
            return new CSSPrimitiveValue(style->marginTopCollapse());
        case CSS_PROP__WEBKIT_RTL_ORDERING:
            if (style->visuallyOrdered())
                return new CSSPrimitiveValue(CSS_VAL_VISUAL);
            return new CSSPrimitiveValue(CSS_VAL_LOGICAL);
        case CSS_PROP__WEBKIT_USER_DRAG:
            return new CSSPrimitiveValue(style->userDrag());
        case CSS_PROP__WEBKIT_USER_SELECT:
            return new CSSPrimitiveValue(style->userSelect());
        case CSS_PROP__WEBKIT_BORDER_BOTTOM_LEFT_RADIUS:
            return getBorderRadiusCornerValue(style->borderBottomLeftRadius());
        case CSS_PROP__WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS:
            return getBorderRadiusCornerValue(style->borderBottomRightRadius());
        case CSS_PROP__WEBKIT_BORDER_TOP_LEFT_RADIUS:
            return getBorderRadiusCornerValue(style->borderTopLeftRadius());
        case CSS_PROP__WEBKIT_BORDER_TOP_RIGHT_RADIUS:
            return getBorderRadiusCornerValue(style->borderTopRightRadius());
        case CSS_PROP_BACKGROUND:
        case CSS_PROP_BORDER:
        case CSS_PROP_BORDER_BOTTOM:
        case CSS_PROP_BORDER_COLOR:
        case CSS_PROP_BORDER_LEFT:
        case CSS_PROP_BORDER_RIGHT:
        case CSS_PROP_BORDER_STYLE:
        case CSS_PROP_BORDER_TOP:
        case CSS_PROP_BORDER_WIDTH:
        case CSS_PROP_CLIP:
        case CSS_PROP_CONTENT:
        case CSS_PROP_COUNTER_INCREMENT:
        case CSS_PROP_COUNTER_RESET:
        case CSS_PROP_FONT:
        case CSS_PROP_FONT_STRETCH:
        case CSS_PROP_LIST_STYLE:
        case CSS_PROP_MARGIN:
        case CSS_PROP_OUTLINE:
        case CSS_PROP_OUTLINE_OFFSET:
        case CSS_PROP_PADDING:
        case CSS_PROP_PAGE:
        case CSS_PROP_QUOTES:
        case CSS_PROP_SCROLLBAR_3DLIGHT_COLOR:
        case CSS_PROP_SCROLLBAR_ARROW_COLOR:
        case CSS_PROP_SCROLLBAR_DARKSHADOW_COLOR:
        case CSS_PROP_SCROLLBAR_FACE_COLOR:
        case CSS_PROP_SCROLLBAR_HIGHLIGHT_COLOR:
        case CSS_PROP_SCROLLBAR_SHADOW_COLOR:
        case CSS_PROP_SCROLLBAR_TRACK_COLOR:
        case CSS_PROP_SRC: // Only used in @font-face rules.
        case CSS_PROP_SIZE:
        case CSS_PROP_TEXT_LINE_THROUGH:
        case CSS_PROP_TEXT_LINE_THROUGH_COLOR:
        case CSS_PROP_TEXT_LINE_THROUGH_MODE:
        case CSS_PROP_TEXT_LINE_THROUGH_STYLE:
        case CSS_PROP_TEXT_LINE_THROUGH_WIDTH:
        case CSS_PROP_TEXT_OVERFLOW:
        case CSS_PROP_TEXT_OVERLINE:
        case CSS_PROP_TEXT_OVERLINE_COLOR:
        case CSS_PROP_TEXT_OVERLINE_MODE:
        case CSS_PROP_TEXT_OVERLINE_STYLE:
        case CSS_PROP_TEXT_OVERLINE_WIDTH:
        case CSS_PROP_TEXT_UNDERLINE:
        case CSS_PROP_TEXT_UNDERLINE_COLOR:
        case CSS_PROP_TEXT_UNDERLINE_MODE:
        case CSS_PROP_TEXT_UNDERLINE_STYLE:
        case CSS_PROP_TEXT_UNDERLINE_WIDTH:
        case CSS_PROP_UNICODE_RANGE: // Only used in @font-face rules.
        case CSS_PROP__WEBKIT_BORDER_IMAGE:
        case CSS_PROP__WEBKIT_BORDER_RADIUS:
        case CSS_PROP__WEBKIT_COLUMNS:
        case CSS_PROP__WEBKIT_COLUMN_RULE:
        case CSS_PROP__WEBKIT_MARGIN_COLLAPSE:
        case CSS_PROP__WEBKIT_MARGIN_START:
        case CSS_PROP__WEBKIT_MARQUEE:
        case CSS_PROP__WEBKIT_MARQUEE_SPEED:
        case CSS_PROP__WEBKIT_PADDING_START:
        case CSS_PROP__WEBKIT_TEXT_STROKE:
        case CSS_PROP__WEBKIT_TRANSFORM:
        case CSS_PROP__WEBKIT_TRANSFORM_ORIGIN:
        case CSS_PROP__WEBKIT_TRANSFORM_ORIGIN_X:
        case CSS_PROP__WEBKIT_TRANSFORM_ORIGIN_Y:
        case CSS_PROP__WEBKIT_TRANSITION:
        case CSS_PROP__WEBKIT_TRANSITION_DURATION:
        case CSS_PROP__WEBKIT_TRANSITION_PROPERTY:
        case CSS_PROP__WEBKIT_TRANSITION_REPEAT_COUNT:
        case CSS_PROP__WEBKIT_TRANSITION_TIMING_FUNCTION:
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
    CSS_PROP_BORDER_COLLAPSE,
    CSS_PROP_COLOR,
    CSS_PROP_FONT_FAMILY,
    CSS_PROP_FONT_SIZE,
    CSS_PROP_FONT_STYLE,
    CSS_PROP_FONT_VARIANT,
    CSS_PROP_FONT_WEIGHT,
    CSS_PROP_LETTER_SPACING,
    CSS_PROP_LINE_HEIGHT,
    CSS_PROP_ORPHANS,
    CSS_PROP_TEXT_ALIGN,
    CSS_PROP_TEXT_INDENT,
    CSS_PROP_TEXT_TRANSFORM,
    CSS_PROP_WHITE_SPACE,
    CSS_PROP_WIDOWS,
    CSS_PROP_WORD_SPACING,
    CSS_PROP__WEBKIT_BORDER_HORIZONTAL_SPACING,
    CSS_PROP__WEBKIT_BORDER_VERTICAL_SPACING,
    CSS_PROP__WEBKIT_TEXT_DECORATIONS_IN_EFFECT,
    CSS_PROP__WEBKIT_TEXT_FILL_COLOR,
    CSS_PROP__WEBKIT_TEXT_SIZE_ADJUST,
    CSS_PROP__WEBKIT_TEXT_STROKE_COLOR,
    CSS_PROP__WEBKIT_TEXT_STROKE_WIDTH,
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
            style->removeProperty(CSS_PROP__WEBKIT_TEXT_FILL_COLOR, ec);
        if (!m_node->computedStyle()->textStrokeColor().isValid())
            style->removeProperty(CSS_PROP__WEBKIT_TEXT_STROKE_COLOR, ec);
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
