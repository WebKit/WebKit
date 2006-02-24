/**
 * css_computedstyle.cpp
 *
 * Copyright (C)  2004  Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005 Apple Computer, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307  USA
 */

#include "config.h"
#include "css_computedstyle.h"

#include "AtomicString.h"
#include "CachedImage.h"
#include "DocumentImpl.h"
#include "PlatformString.h"
#include "cssproperties.h"
#include "cssvalues.h"
#include "dom_exception.h"
#include "Font.h"
#include "render_object.h"
#include "render_style.h"
#include <kxmlcore/Assertions.h>

extern WebCore::String getPropertyName(unsigned short id);

namespace WebCore {

// List of all properties we know how to compute, omitting shorthands.
static const int computedProperties[] = {
    CSS_PROP_BACKGROUND_COLOR,
    CSS_PROP_BACKGROUND_IMAGE,
    CSS_PROP_BACKGROUND_REPEAT,
    CSS_PROP_BACKGROUND_ATTACHMENT,
    CSS_PROP_BACKGROUND_CLIP,
    CSS_PROP_BACKGROUND_ORIGIN,
    CSS_PROP_BACKGROUND_POSITION,
    CSS_PROP_BACKGROUND_POSITION_X,
    CSS_PROP_BACKGROUND_POSITION_Y,
    CSS_PROP_BORDER_COLLAPSE,
    CSS_PROP_BORDER_SPACING,
    CSS_PROP__KHTML_BORDER_HORIZONTAL_SPACING,
    CSS_PROP__KHTML_BORDER_VERTICAL_SPACING,
    CSS_PROP_BORDER_TOP_COLOR,
    CSS_PROP_BORDER_RIGHT_COLOR,
    CSS_PROP_BORDER_BOTTOM_COLOR,
    CSS_PROP_BORDER_LEFT_COLOR,
    CSS_PROP_BORDER_TOP_STYLE,
    CSS_PROP_BORDER_RIGHT_STYLE,
    CSS_PROP_BORDER_BOTTOM_STYLE,
    CSS_PROP_BORDER_LEFT_STYLE,
    CSS_PROP_BORDER_TOP_WIDTH,
    CSS_PROP_BORDER_RIGHT_WIDTH,
    CSS_PROP_BORDER_BOTTOM_WIDTH,
    CSS_PROP_BORDER_LEFT_WIDTH,
    CSS_PROP_BOTTOM,
    CSS_PROP__KHTML_BOX_ALIGN,
    CSS_PROP__KHTML_BOX_DIRECTION,
    CSS_PROP__KHTML_BOX_FLEX,
    CSS_PROP__KHTML_BOX_FLEX_GROUP,
    CSS_PROP__KHTML_BOX_LINES,
    CSS_PROP__KHTML_BOX_ORDINAL_GROUP,
    CSS_PROP__KHTML_BOX_ORIENT,
    CSS_PROP__KHTML_BOX_PACK,
    CSS_PROP_CAPTION_SIDE,
    CSS_PROP_CLEAR,
    CSS_PROP_COLOR,
    CSS_PROP_CURSOR,
#if __APPLE__
    CSS_PROP__KHTML_DASHBOARD_REGION,
#endif
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
    CSS_PROP__KHTML_LINE_BREAK,
    CSS_PROP__KHTML_LINE_CLAMP,
    CSS_PROP_LINE_HEIGHT,
    CSS_PROP_LIST_STYLE_IMAGE,
    CSS_PROP_LIST_STYLE_POSITION,
    CSS_PROP_LIST_STYLE_TYPE,
    CSS_PROP_MARGIN_TOP,
    CSS_PROP_MARGIN_RIGHT,
    CSS_PROP_MARGIN_BOTTOM,
    CSS_PROP_MARGIN_LEFT,
    CSS_PROP__KHTML_MARQUEE_DIRECTION,
    CSS_PROP__KHTML_MARQUEE_INCREMENT,
    CSS_PROP__KHTML_MARQUEE_REPETITION,
    CSS_PROP__KHTML_MARQUEE_STYLE,
    CSS_PROP_MAX_HEIGHT,
    CSS_PROP_MAX_WIDTH,
    CSS_PROP_MIN_HEIGHT,
    CSS_PROP_MIN_WIDTH,
    CSS_PROP__KHTML_NBSP_MODE,
    CSS_PROP_OPACITY,
    CSS_PROP_ORPHANS,
    CSS_PROP_OUTLINE_STYLE,
    CSS_PROP_OVERFLOW,
    CSS_PROP_PADDING_TOP,
    CSS_PROP_PADDING_RIGHT,
    CSS_PROP_PADDING_BOTTOM,
    CSS_PROP_PADDING_LEFT,
    CSS_PROP_PAGE_BREAK_AFTER,
    CSS_PROP_PAGE_BREAK_BEFORE,
    CSS_PROP_PAGE_BREAK_INSIDE,
    CSS_PROP_POSITION,
    CSS_PROP_RIGHT,
    CSS_PROP_TABLE_LAYOUT,
    CSS_PROP_TEXT_ALIGN,
    CSS_PROP_TEXT_DECORATION,
    CSS_PROP__KHTML_TEXT_DECORATIONS_IN_EFFECT,
    CSS_PROP_TEXT_INDENT,
    CSS_PROP_TEXT_SHADOW,
    CSS_PROP_TEXT_TRANSFORM,
    CSS_PROP_TOP,
    CSS_PROP_UNICODE_BIDI,
    CSS_PROP__KHTML_USER_MODIFY,
    CSS_PROP_VERTICAL_ALIGN,
    CSS_PROP_VISIBILITY,
    CSS_PROP_WHITE_SPACE,
    CSS_PROP_WIDOWS,
    CSS_PROP_WIDTH,
    CSS_PROP_WORD_SPACING,
    CSS_PROP_WORD_WRAP,
    CSS_PROP_Z_INDEX,
};

const unsigned numComputedProperties = sizeof(computedProperties) / sizeof(computedProperties[0]);

static CSSValueImpl* valueForLength(const Length &length)
{
    switch (length.type()) {
        case Percent:
            return new CSSPrimitiveValueImpl(length.value(), CSSPrimitiveValue::CSS_PERCENTAGE);
        case khtml::Fixed:
            return new CSSPrimitiveValueImpl(length.value(), CSSPrimitiveValue::CSS_PX);
        default: // FIXME: Intrinsic and MinIntrinsic should probably return keywords.
            return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
    }
}

static CSSValueImpl *valueForBorderStyle(EBorderStyle style)
{
    switch (style) {
        case BNONE:
            return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
        case BHIDDEN:
            return new CSSPrimitiveValueImpl(CSS_VAL_HIDDEN);
        case INSET:
            return new CSSPrimitiveValueImpl(CSS_VAL_INSET);
        case GROOVE:
            return new CSSPrimitiveValueImpl(CSS_VAL_GROOVE);
        case RIDGE:
            return new CSSPrimitiveValueImpl(CSS_VAL_RIDGE);
        case OUTSET:
            return new CSSPrimitiveValueImpl(CSS_VAL_OUTSET);
        case DOTTED:
            return new CSSPrimitiveValueImpl(CSS_VAL_DOTTED);
        case DASHED:
            return new CSSPrimitiveValueImpl(CSS_VAL_DASHED);
        case SOLID:
            return new CSSPrimitiveValueImpl(CSS_VAL_SOLID);
        case DOUBLE:
            return new CSSPrimitiveValueImpl(CSS_VAL_DOUBLE);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static CSSValueImpl *valueForTextAlign(ETextAlign align)
{
    switch (align) {
        case TAAUTO:
            return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
        case LEFT:
            return new CSSPrimitiveValueImpl(CSS_VAL_LEFT);
        case RIGHT:
            return new CSSPrimitiveValueImpl(CSS_VAL_RIGHT);
        case CENTER:
            return new CSSPrimitiveValueImpl(CSS_VAL_CENTER);
        case JUSTIFY:
            return new CSSPrimitiveValueImpl(CSS_VAL_JUSTIFY);
        case KHTML_LEFT:
            return new CSSPrimitiveValueImpl(CSS_VAL__KHTML_LEFT);
        case KHTML_RIGHT:
            return new CSSPrimitiveValueImpl(CSS_VAL__KHTML_RIGHT);
        case KHTML_CENTER:
            return new CSSPrimitiveValueImpl(CSS_VAL__KHTML_CENTER);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static CSSValueImpl* valueForShadow(const ShadowData *shadow)
{
    if (!shadow)
        return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
    CSSValueListImpl *list = new CSSValueListImpl;
    for (const ShadowData *s = shadow; s; s = s->next) {
        CSSPrimitiveValueImpl *x = new CSSPrimitiveValueImpl(s->x, CSSPrimitiveValue::CSS_PX);
        CSSPrimitiveValueImpl *y = new CSSPrimitiveValueImpl(s->y, CSSPrimitiveValue::CSS_PX);
        CSSPrimitiveValueImpl *blur = new CSSPrimitiveValueImpl(s->blur, CSSPrimitiveValue::CSS_PX);
        CSSPrimitiveValueImpl *color = new CSSPrimitiveValueImpl(s->color.rgb());
        list->append(new ShadowValueImpl(x, y, blur, color));
    }
    return list;
}

static CSSValueImpl *getPositionOffsetValue(RenderObject *renderer, int propertyID)
{
    if (!renderer)
        return 0;

    RenderStyle *style = renderer->style();
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

    if (renderer->isPositioned())
        return valueForLength(l);
    
    if (renderer->isRelPositioned())
        // FIXME: It's not enough to simply return "auto" values for one offset if the other side is defined.
        // In other words if left is auto and right is not auto, then left's computed value is negative right.
        // So we should get the opposite length unit and see if it is auto.
        return valueForLength(l);
    
    return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
}

CSSComputedStyleDeclarationImpl::CSSComputedStyleDeclarationImpl(PassRefPtr<NodeImpl> n)
    : m_node(n)
{
}

CSSComputedStyleDeclarationImpl::~CSSComputedStyleDeclarationImpl()
{
}

String CSSComputedStyleDeclarationImpl::cssText() const
{
    String result("");
    
    for (unsigned i = 0; i < numComputedProperties; i++) {
        if (i != 0)
            result += " ";
        result += getPropertyName(computedProperties[i]);
        result += ": ";
        result += getPropertyValue(computedProperties[i]);
        result += ";";
    }
    
    return result;
}

void CSSComputedStyleDeclarationImpl::setCssText(const String&, int &exceptionCode)
{
    exceptionCode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
}

// Display integers in integer format instead of "1.0".
static QString numberAsString(double n)
{
    long i = static_cast<long>(n);
    return i == n ? QString::number(i) : QString::number(n);
}

PassRefPtr<CSSValueImpl> CSSComputedStyleDeclarationImpl::getPropertyCSSValue(int propertyID) const
{
    return getPropertyCSSValue(propertyID, UpdateLayout);
}

PassRefPtr<CSSValueImpl> CSSComputedStyleDeclarationImpl::getPropertyCSSValue(int propertyID, EUpdateLayout updateLayout) const
{
    NodeImpl* node = m_node.get();
    if (!node)
        return 0;

    // Make sure our layout is up to date before we allow a query on these attributes.
    DocumentImpl* docimpl = node->getDocument();
    if (docimpl && updateLayout)
        docimpl->updateLayout();

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return 0;
    RenderStyle* style = renderer->style();
    if (!style)
        return 0;

    switch (propertyID)
    {
    case CSS_PROP_BACKGROUND_COLOR:
        return new CSSPrimitiveValueImpl(style->backgroundColor().rgb());
    case CSS_PROP_BACKGROUND_IMAGE:
        if (style->backgroundImage())
            return new CSSPrimitiveValueImpl(style->backgroundImage()->url(), CSSPrimitiveValue::CSS_URI);
        return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
    case CSS_PROP_BACKGROUND_REPEAT:
        switch (style->backgroundRepeat()) {
            case REPEAT:
                return new CSSPrimitiveValueImpl(CSS_VAL_REPEAT);
            case REPEAT_X:
                return new CSSPrimitiveValueImpl(CSS_VAL_REPEAT_X);
            case REPEAT_Y:
                return new CSSPrimitiveValueImpl(CSS_VAL_REPEAT_Y);
            case NO_REPEAT:
                return new CSSPrimitiveValueImpl(CSS_VAL_NO_REPEAT);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_BACKGROUND_ATTACHMENT:
        if (style->backgroundAttachment())
            return new CSSPrimitiveValueImpl(CSS_VAL_SCROLL);
        return new CSSPrimitiveValueImpl(CSS_VAL_FIXED);
    case CSS_PROP_BACKGROUND_CLIP:
    case CSS_PROP_BACKGROUND_ORIGIN: {
        EBackgroundBox box = (propertyID == CSS_PROP_BACKGROUND_CLIP ? style->backgroundClip() : style->backgroundOrigin());
        if (box == BGBORDER)
            return new CSSPrimitiveValueImpl(CSS_VAL_BORDER);
        if (box == BGPADDING)
            return new CSSPrimitiveValueImpl(CSS_VAL_PADDING);
        return new CSSPrimitiveValueImpl(CSS_VAL_CONTENT);
    }
    case CSS_PROP_BACKGROUND_POSITION:
    {
        String string;
        Length length(style->backgroundXPosition());
        if (length.isPercent())
            string = numberAsString(length.value()) + "%";
        else
            string = numberAsString(length.calcMinValue(renderer->contentWidth()));
        string += " ";
        length = style->backgroundYPosition();
        if (length.isPercent())
            string += numberAsString(length.value()) + "%";
        else
            string += numberAsString(length.calcMinValue(renderer->contentWidth()));
        return new CSSPrimitiveValueImpl(string, CSSPrimitiveValue::CSS_STRING);
    }
    case CSS_PROP_BACKGROUND_POSITION_X:
        return valueForLength(style->backgroundXPosition());
    case CSS_PROP_BACKGROUND_POSITION_Y:
        return valueForLength(style->backgroundYPosition());
#ifndef KHTML_NO_XBL
    case CSS_PROP__KHTML_BINDING:
        // FIXME: unimplemented
        break;
#endif
    case CSS_PROP_BORDER_COLLAPSE:
        if (style->borderCollapse())
            return new CSSPrimitiveValueImpl(CSS_VAL_COLLAPSE);
        else
            return new CSSPrimitiveValueImpl(CSS_VAL_SEPARATE);
    case CSS_PROP_BORDER_SPACING:
    {
        QString string(numberAsString(style->horizontalBorderSpacing()) + 
            "px " + 
            numberAsString(style->verticalBorderSpacing()) +
            "px");
        return new CSSPrimitiveValueImpl(string, CSSPrimitiveValue::CSS_STRING);
    }
    case CSS_PROP__KHTML_BORDER_HORIZONTAL_SPACING:
        return new CSSPrimitiveValueImpl(style->horizontalBorderSpacing(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP__KHTML_BORDER_VERTICAL_SPACING:
        return new CSSPrimitiveValueImpl(style->verticalBorderSpacing(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP_BORDER_TOP_COLOR:
        return new CSSPrimitiveValueImpl(style->borderLeftColor().rgb());
    case CSS_PROP_BORDER_RIGHT_COLOR:
        return new CSSPrimitiveValueImpl(style->borderRightColor().rgb());
    case CSS_PROP_BORDER_BOTTOM_COLOR:
        return new CSSPrimitiveValueImpl(style->borderBottomColor().rgb());
    case CSS_PROP_BORDER_LEFT_COLOR:
        return new CSSPrimitiveValueImpl(style->borderLeftColor().rgb());
    case CSS_PROP_BORDER_TOP_STYLE:
        return valueForBorderStyle(style->borderTopStyle());
    case CSS_PROP_BORDER_RIGHT_STYLE:
        return valueForBorderStyle(style->borderRightStyle());
    case CSS_PROP_BORDER_BOTTOM_STYLE:
        return valueForBorderStyle(style->borderBottomStyle());
    case CSS_PROP_BORDER_LEFT_STYLE:
        return valueForBorderStyle(style->borderLeftStyle());
    case CSS_PROP_BORDER_TOP_WIDTH:
        return new CSSPrimitiveValueImpl(style->borderTopWidth(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP_BORDER_RIGHT_WIDTH:
        return new CSSPrimitiveValueImpl(style->borderRightWidth(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP_BORDER_BOTTOM_WIDTH:
        return new CSSPrimitiveValueImpl(style->borderBottomWidth(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP_BORDER_LEFT_WIDTH:
        return new CSSPrimitiveValueImpl(style->borderLeftWidth(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP_BOTTOM:
        return getPositionOffsetValue(renderer, CSS_PROP_BOTTOM);
    case CSS_PROP__KHTML_BOX_ALIGN:
        switch (style->boxAlign()) {
            case BSTRETCH:
                return new CSSPrimitiveValueImpl(CSS_VAL_STRETCH);
            case BSTART:
                return new CSSPrimitiveValueImpl(CSS_VAL_START);
            case BCENTER:
                return new CSSPrimitiveValueImpl(CSS_VAL_CENTER);
            case BEND:
                return new CSSPrimitiveValueImpl(CSS_VAL_END);
            case BBASELINE:
                return new CSSPrimitiveValueImpl(CSS_VAL_BASELINE);
            case BJUSTIFY:
                break; // not allowed
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_BOX_DIRECTION:
        switch (style->boxDirection()) {
            case BNORMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case BREVERSE:
                return new CSSPrimitiveValueImpl(CSS_VAL_REVERSE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_BOX_FLEX:
        return new CSSPrimitiveValueImpl(style->boxFlex(), CSSPrimitiveValue::CSS_NUMBER);
    case CSS_PROP__KHTML_BOX_FLEX_GROUP:
        return new CSSPrimitiveValueImpl(style->boxFlexGroup(), CSSPrimitiveValue::CSS_NUMBER);
    case CSS_PROP__KHTML_BOX_LINES:
        switch (style->boxLines()) {
            case SINGLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_SINGLE);
            case MULTIPLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_MULTIPLE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_BOX_ORDINAL_GROUP:
        return new CSSPrimitiveValueImpl(style->boxOrdinalGroup(), CSSPrimitiveValue::CSS_NUMBER);
    case CSS_PROP__KHTML_BOX_ORIENT:
        switch (style->boxOrient()) {
            case HORIZONTAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_HORIZONTAL);
            case VERTICAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_VERTICAL);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_BOX_PACK:
        switch (style->boxPack()) {
            case BSTART:
                return new CSSPrimitiveValueImpl(CSS_VAL_START);
            case BEND:
                return new CSSPrimitiveValueImpl(CSS_VAL_END);
            case BCENTER:
                return new CSSPrimitiveValueImpl(CSS_VAL_CENTER);
            case BJUSTIFY:
                return new CSSPrimitiveValueImpl(CSS_VAL_JUSTIFY);
            case BSTRETCH:
            case BBASELINE:
                break; // not allowed
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_CAPTION_SIDE:
        switch (style->captionSide()) {
            case CAPLEFT:
                return new CSSPrimitiveValueImpl(CSS_VAL_LEFT);
            case CAPRIGHT:
                return new CSSPrimitiveValueImpl(CSS_VAL_RIGHT);
            case CAPTOP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TOP);
            case CAPBOTTOM:
                return new CSSPrimitiveValueImpl(CSS_VAL_BOTTOM);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_CLEAR:
        switch (style->clear()) {
            case CNONE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
            case CLEFT:
                return new CSSPrimitiveValueImpl(CSS_VAL_LEFT);
            case CRIGHT:
                return new CSSPrimitiveValueImpl(CSS_VAL_RIGHT);
            case CBOTH:
                return new CSSPrimitiveValueImpl(CSS_VAL_BOTH);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_CLIP:
        // FIXME: unimplemented
        break;
    case CSS_PROP_COLOR:
        return new CSSPrimitiveValueImpl(style->color().rgb());
    case CSS_PROP_CONTENT:
        // FIXME: unimplemented
        break;
    case CSS_PROP_COUNTER_INCREMENT:
        // FIXME: unimplemented
        break;
    case CSS_PROP_COUNTER_RESET:
        // FIXME: unimplemented
        break;
    case CSS_PROP_CURSOR:
        if (style->cursorImage())
            return new CSSPrimitiveValueImpl(style->cursorImage()->url(), CSSPrimitiveValue::CSS_URI);
        switch (style->cursor()) {
            case CURSOR_AUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case CURSOR_CROSS:
                return new CSSPrimitiveValueImpl(CSS_VAL_CROSSHAIR);
            case CURSOR_DEFAULT:
                return new CSSPrimitiveValueImpl(CSS_VAL_DEFAULT);
            case CURSOR_POINTER:
                return new CSSPrimitiveValueImpl(CSS_VAL_POINTER);
            case CURSOR_MOVE:
                return new CSSPrimitiveValueImpl(CSS_VAL_MOVE);
            case CURSOR_E_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_E_RESIZE);
            case CURSOR_NE_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NE_RESIZE);
            case CURSOR_NW_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NW_RESIZE);
            case CURSOR_N_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_N_RESIZE);
            case CURSOR_SE_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_SE_RESIZE);
            case CURSOR_SW_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_SW_RESIZE);
            case CURSOR_S_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_S_RESIZE);
            case CURSOR_W_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_W_RESIZE);
            case CURSOR_TEXT:
                return new CSSPrimitiveValueImpl(CSS_VAL_TEXT);
            case CURSOR_WAIT:
                return new CSSPrimitiveValueImpl(CSS_VAL_WAIT);
            case CURSOR_HELP:
                return new CSSPrimitiveValueImpl(CSS_VAL_HELP);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_DIRECTION:
        switch (style->direction()) {
            case LTR:
                return new CSSPrimitiveValueImpl(CSS_VAL_LTR);
            case RTL:
                return new CSSPrimitiveValueImpl(CSS_VAL_RTL);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_DISPLAY:
        switch (style->display()) {
            case INLINE:
                return new CSSPrimitiveValueImpl(CSS_VAL_INLINE);
            case BLOCK:
                return new CSSPrimitiveValueImpl(CSS_VAL_BLOCK);
            case LIST_ITEM:
                return new CSSPrimitiveValueImpl(CSS_VAL_LIST_ITEM);
            case RUN_IN:
                return new CSSPrimitiveValueImpl(CSS_VAL_RUN_IN);
            case COMPACT:
                return new CSSPrimitiveValueImpl(CSS_VAL_COMPACT);
            case INLINE_BLOCK:
                return new CSSPrimitiveValueImpl(CSS_VAL_INLINE_BLOCK);
            case TABLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE);
            case INLINE_TABLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_INLINE_TABLE);
            case TABLE_ROW_GROUP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_ROW_GROUP);
            case TABLE_HEADER_GROUP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_HEADER_GROUP);
            case TABLE_FOOTER_GROUP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_FOOTER_GROUP);
            case TABLE_ROW:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_ROW);
            case TABLE_COLUMN_GROUP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_COLUMN_GROUP);
            case TABLE_COLUMN:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_COLUMN);
            case TABLE_CELL:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_CELL);
            case TABLE_CAPTION:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_CAPTION);
            case BOX:
                return new CSSPrimitiveValueImpl(CSS_VAL__KHTML_BOX);
            case INLINE_BOX:
                return new CSSPrimitiveValueImpl(CSS_VAL__KHTML_INLINE_BOX);
            case NONE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_EMPTY_CELLS:
        switch (style->emptyCells()) {
            case SHOW:
                return new CSSPrimitiveValueImpl(CSS_VAL_SHOW);
            case HIDE:
                return new CSSPrimitiveValueImpl(CSS_VAL_HIDE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_FLOAT:
        switch (style->floating()) {
            case FNONE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
            case FLEFT:
                return new CSSPrimitiveValueImpl(CSS_VAL_LEFT);
            case FRIGHT:
                return new CSSPrimitiveValueImpl(CSS_VAL_RIGHT);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_FONT_FAMILY:
    {
        // FIXME: This only returns the first family.
        const FontDescription& desc = style->fontDescription();
        return new CSSPrimitiveValueImpl(desc.family().family().domString(), CSSPrimitiveValue::CSS_STRING);
    }
    case CSS_PROP_FONT_SIZE:
    {
        FontDescription def = style->fontDescription();
        return new CSSPrimitiveValueImpl(def.specifiedSize(), CSSPrimitiveValue::CSS_PX);
    }
    case CSS_PROP_FONT_STRETCH:
        // FIXME: unimplemented
        break;
    case CSS_PROP_FONT_STYLE:
    {
        // FIXME: handle oblique?
        const FontDescription& desc = style->fontDescription();
        if (desc.italic())
            return new CSSPrimitiveValueImpl(CSS_VAL_ITALIC);
        else
            return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
    }
    case CSS_PROP_FONT_VARIANT:
    {
        const FontDescription& desc = style->fontDescription();
        if (desc.smallCaps())
            return new CSSPrimitiveValueImpl(CSS_VAL_SMALL_CAPS);
        else
            return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
    }
    case CSS_PROP_FONT_WEIGHT:
    {
        // FIXME: this does not reflect the full range of weights
        // that can be expressed with CSS
        const FontDescription& desc = style->fontDescription();
        if (desc.weight() == QFont::Bold)
            return new CSSPrimitiveValueImpl(CSS_VAL_BOLD);
        else
            return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
    }
    case CSS_PROP_HEIGHT:
        return new CSSPrimitiveValueImpl(renderer->contentHeight(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP_LEFT:
        return getPositionOffsetValue(renderer, CSS_PROP_LEFT);
    case CSS_PROP_LETTER_SPACING:
        if (style->letterSpacing() == 0)
            return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
        return new CSSPrimitiveValueImpl(style->letterSpacing(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP__KHTML_LINE_CLAMP:
        return new CSSPrimitiveValueImpl(style->lineClamp(), CSSPrimitiveValue::CSS_PERCENTAGE);
    case CSS_PROP_LINE_HEIGHT: {
        Length length(style->lineHeight());
        if (length.value() < 0)
            return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
        if (length.isPercent()) {
            // This is imperfect, because it doesn't include the zoom factor and the real computation
            // for how high to be in pixels does include things like minimum font size and the zoom factor.
            // On the other hand, since font-size doesn't include the zoom factor, we really can't do
            // that here either.
            float fontSize = style->fontDescription().specifiedSize();
            return new CSSPrimitiveValueImpl((int)(length.value() * fontSize) / 100, CSSPrimitiveValue::CSS_PX);
        }
        else {
            return new CSSPrimitiveValueImpl(length.value(), CSSPrimitiveValue::CSS_PX);
        }
    }
    case CSS_PROP_LIST_STYLE_IMAGE:
        if (style->listStyleImage())
            return new CSSPrimitiveValueImpl(style->listStyleImage()->url(), CSSPrimitiveValue::CSS_URI);
        return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
    case CSS_PROP_LIST_STYLE_POSITION:
        switch (style->listStylePosition()) {
            case OUTSIDE:
                return new CSSPrimitiveValueImpl(CSS_VAL_OUTSIDE);
            case INSIDE:
                return new CSSPrimitiveValueImpl(CSS_VAL_INSIDE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_LIST_STYLE_TYPE:
        switch (style->listStyleType()) {
            case LNONE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
            case DISC:
                return new CSSPrimitiveValueImpl(CSS_VAL_DISC);
            case CIRCLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_CIRCLE);
            case SQUARE:
                return new CSSPrimitiveValueImpl(CSS_VAL_SQUARE);
            case LDECIMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_DECIMAL);
            case DECIMAL_LEADING_ZERO:
                return new CSSPrimitiveValueImpl(CSS_VAL_DECIMAL_LEADING_ZERO);
            case LOWER_ROMAN:
                return new CSSPrimitiveValueImpl(CSS_VAL_LOWER_ROMAN);
            case UPPER_ROMAN:
                return new CSSPrimitiveValueImpl(CSS_VAL_UPPER_ROMAN);
            case LOWER_GREEK:
                return new CSSPrimitiveValueImpl(CSS_VAL_LOWER_GREEK);
            case LOWER_ALPHA:
                return new CSSPrimitiveValueImpl(CSS_VAL_LOWER_ALPHA);
            case LOWER_LATIN:
                return new CSSPrimitiveValueImpl(CSS_VAL_LOWER_LATIN);
            case UPPER_ALPHA:
                return new CSSPrimitiveValueImpl(CSS_VAL_UPPER_ALPHA);
            case UPPER_LATIN:
                return new CSSPrimitiveValueImpl(CSS_VAL_UPPER_LATIN);
            case HEBREW:
                return new CSSPrimitiveValueImpl(CSS_VAL_HEBREW);
            case ARMENIAN:
                return new CSSPrimitiveValueImpl(CSS_VAL_ARMENIAN);
            case GEORGIAN:
                return new CSSPrimitiveValueImpl(CSS_VAL_GEORGIAN);
            case CJK_IDEOGRAPHIC:
                return new CSSPrimitiveValueImpl(CSS_VAL_CJK_IDEOGRAPHIC);
            case HIRAGANA:
                return new CSSPrimitiveValueImpl(CSS_VAL_HIRAGANA);
            case KATAKANA:
                return new CSSPrimitiveValueImpl(CSS_VAL_KATAKANA);
            case HIRAGANA_IROHA:
                return new CSSPrimitiveValueImpl(CSS_VAL_HIRAGANA_IROHA);
            case KATAKANA_IROHA:
                return new CSSPrimitiveValueImpl(CSS_VAL_KATAKANA_IROHA);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_MARGIN_TOP:
        return valueForLength(style->marginTop());
    case CSS_PROP_MARGIN_RIGHT:
        return valueForLength(style->marginRight());
    case CSS_PROP_MARGIN_BOTTOM:
        return valueForLength(style->marginBottom());
    case CSS_PROP_MARGIN_LEFT:
        return valueForLength(style->marginLeft());
    case CSS_PROP__KHTML_MARQUEE:
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_MARQUEE_DIRECTION:
        switch (style->marqueeDirection()) {
            case MFORWARD:
                return new CSSPrimitiveValueImpl(CSS_VAL_FORWARDS);
            case MBACKWARD:
                return new CSSPrimitiveValueImpl(CSS_VAL_BACKWARDS);
            case MAUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case MUP:
                return new CSSPrimitiveValueImpl(CSS_VAL_UP);
            case MDOWN:
                return new CSSPrimitiveValueImpl(CSS_VAL_DOWN);
            case MLEFT:
                return new CSSPrimitiveValueImpl(CSS_VAL_LEFT);
            case MRIGHT:
                return new CSSPrimitiveValueImpl(CSS_VAL_RIGHT);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_MARQUEE_INCREMENT:
        return valueForLength(style->marqueeIncrement());
    case CSS_PROP__KHTML_MARQUEE_REPETITION:
        if (style->marqueeLoopCount() < 0)
            return new CSSPrimitiveValueImpl(CSS_VAL_INFINITE);
        return new CSSPrimitiveValueImpl(style->marqueeLoopCount(), CSSPrimitiveValue::CSS_NUMBER);
    case CSS_PROP__KHTML_MARQUEE_SPEED:
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_MARQUEE_STYLE:
        switch (style->marqueeBehavior()) {
            case MNONE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
            case MSCROLL:
                return new CSSPrimitiveValueImpl(CSS_VAL_SCROLL);
            case MSLIDE:
                return new CSSPrimitiveValueImpl(CSS_VAL_SLIDE);
            case MALTERNATE:
                return new CSSPrimitiveValueImpl(CSS_VAL_ALTERNATE);
            case MUNFURL:
                return new CSSPrimitiveValueImpl(CSS_VAL_UNFURL);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_USER_MODIFY:
        switch (style->userModify()) {
            case READ_ONLY:
                return new CSSPrimitiveValueImpl(CSS_VAL_READ_ONLY);
            case READ_WRITE:
                return new CSSPrimitiveValueImpl(CSS_VAL_READ_WRITE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_MAX_HEIGHT:
        return valueForLength(style->maxHeight());
    case CSS_PROP_MAX_WIDTH:
        return valueForLength(style->maxWidth());
    case CSS_PROP_MIN_HEIGHT:
        return valueForLength(style->minHeight());
    case CSS_PROP_MIN_WIDTH:
        return valueForLength(style->minWidth());
    case CSS_PROP_OPACITY:
        return new CSSPrimitiveValueImpl(style->opacity(), CSSPrimitiveValue::CSS_NUMBER);
    case CSS_PROP_ORPHANS:
        return new CSSPrimitiveValueImpl(style->orphans(), CSSPrimitiveValue::CSS_NUMBER);
    case CSS_PROP_OUTLINE_COLOR:
        // FIXME: unimplemented
        break;
    case CSS_PROP_OUTLINE_OFFSET:
        // FIXME: unimplemented
        break;
    case CSS_PROP_OUTLINE_STYLE:
        if (style->outlineStyleIsAuto())
            return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
        return valueForBorderStyle(style->outlineStyle());
    case CSS_PROP_OUTLINE_WIDTH:
        // FIXME: unimplemented
        break;
    case CSS_PROP_OVERFLOW:
        switch (style->overflow()) {
            case OVISIBLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_VISIBLE);
            case OHIDDEN:
                return new CSSPrimitiveValueImpl(CSS_VAL_HIDDEN);
            case OSCROLL:
                return new CSSPrimitiveValueImpl(CSS_VAL_SCROLL);
            case OAUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case OMARQUEE:
                return new CSSPrimitiveValueImpl(CSS_VAL_MARQUEE);
            case OOVERLAY:
                return new CSSPrimitiveValueImpl(CSS_VAL_OVERLAY);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_PADDING_TOP:
        return valueForLength(style->paddingTop());
    case CSS_PROP_PADDING_RIGHT:
        return valueForLength(style->paddingRight());
    case CSS_PROP_PADDING_BOTTOM:
        return valueForLength(style->paddingBottom());
    case CSS_PROP_PADDING_LEFT:
        return valueForLength(style->paddingLeft());
    case CSS_PROP_PAGE:
        // FIXME: unimplemented
        break;
    case CSS_PROP_PAGE_BREAK_AFTER:
        switch (style->pageBreakAfter()) {
            case PBAUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case PBALWAYS:
                return new CSSPrimitiveValueImpl(CSS_VAL_ALWAYS);
            case PBAVOID:
                return new CSSPrimitiveValueImpl(CSS_VAL_AVOID);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_PAGE_BREAK_BEFORE:
        switch (style->pageBreakBefore()) {
            case PBAUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case PBALWAYS:
                return new CSSPrimitiveValueImpl(CSS_VAL_ALWAYS);
            case PBAVOID:
                return new CSSPrimitiveValueImpl(CSS_VAL_AVOID);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_PAGE_BREAK_INSIDE:
        switch (style->pageBreakInside()) {
            case PBAUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case PBAVOID:
                return new CSSPrimitiveValueImpl(CSS_VAL_AVOID);
            case PBALWAYS:
                break; // not allowed
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_POSITION:
        switch (style->position()) {
            case StaticPosition:
                return new CSSPrimitiveValueImpl(CSS_VAL_STATIC);
            case RelativePosition:
                return new CSSPrimitiveValueImpl(CSS_VAL_RELATIVE);
            case AbsolutePosition:
                return new CSSPrimitiveValueImpl(CSS_VAL_ABSOLUTE);
            case FixedPosition:
                return new CSSPrimitiveValueImpl(CSS_VAL_FIXED);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_QUOTES:
        // FIXME: unimplemented
        break;
    case CSS_PROP_RIGHT:
        return getPositionOffsetValue(renderer, CSS_PROP_RIGHT);
    case CSS_PROP_SIZE:
        // FIXME: unimplemented
        break;
    case CSS_PROP_TABLE_LAYOUT:
        switch (style->tableLayout()) {
            case TAUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case TFIXED:
                return new CSSPrimitiveValueImpl(CSS_VAL_FIXED);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_TEXT_ALIGN:
        return valueForTextAlign(style->textAlign());
    case CSS_PROP_TEXT_DECORATION:
    {
        QString string;
        if (style->textDecoration() & UNDERLINE)
            string += "underline";
        if (style->textDecoration() & OVERLINE) {
            if (string.length() > 0)
                string += " ";
            string += "overline";
        }
        if (style->textDecoration() & LINE_THROUGH) {
            if (string.length() > 0)
                string += " ";
            string += "line-through";
        }
        if (style->textDecoration() & BLINK) {
            if (string.length() > 0)
                string += " ";
            string += "blink";
        }
        if (string.length() == 0)
            return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
        return new CSSPrimitiveValueImpl(string, CSSPrimitiveValue::CSS_STRING);
    }
    case CSS_PROP__KHTML_TEXT_DECORATIONS_IN_EFFECT:
    {
        QString string;
        if (style->textDecorationsInEffect() & UNDERLINE)
            string += "underline";
        if (style->textDecorationsInEffect() & OVERLINE) {
            if (string.length() > 0)
                string += " ";
            string += "overline";
        }
        if (style->textDecorationsInEffect() & LINE_THROUGH) {
            if (string.length() > 0)
                string += " ";
            string += "line-through";
        }
        if (style->textDecorationsInEffect() & BLINK) {
            if (string.length() > 0)
                string += " ";
            string += "blink";
        }
        if (string.length() == 0)
            return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
        return new CSSPrimitiveValueImpl(string, CSSPrimitiveValue::CSS_STRING);
    }
    case CSS_PROP_TEXT_INDENT:
        return valueForLength(style->textIndent());
    case CSS_PROP_TEXT_SHADOW:
        return valueForShadow(style->textShadow());
    case CSS_PROP__KHTML_TEXT_SIZE_ADJUST:
        if (style->textSizeAdjust()) 
            return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
        else
            return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
    case CSS_PROP_TEXT_TRANSFORM:
        switch (style->textTransform()) {
            case CAPITALIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_CAPITALIZE);
            case UPPERCASE:
                return new CSSPrimitiveValueImpl(CSS_VAL_UPPERCASE);
            case LOWERCASE:
                return new CSSPrimitiveValueImpl(CSS_VAL_LOWERCASE);
            case TTNONE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_TOP:
        return getPositionOffsetValue(renderer, CSS_PROP_TOP);
    case CSS_PROP_UNICODE_BIDI:
        switch (style->unicodeBidi()) {
            case UBNormal:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case Embed:
                return new CSSPrimitiveValueImpl(CSS_VAL_EMBED);
            case Override:
                return new CSSPrimitiveValueImpl(CSS_VAL_BIDI_OVERRIDE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_VERTICAL_ALIGN:
        switch (style->verticalAlign()) {
            case BASELINE:
                return new CSSPrimitiveValueImpl(CSS_VAL_BASELINE);
            case MIDDLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_MIDDLE);
            case SUB:
                return new CSSPrimitiveValueImpl(CSS_VAL_SUB);
            case SUPER:
                return new CSSPrimitiveValueImpl(CSS_VAL_SUPER);
            case TEXT_TOP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TEXT_TOP);
            case TEXT_BOTTOM:
                return new CSSPrimitiveValueImpl(CSS_VAL_TEXT_BOTTOM);
            case TOP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TOP);
            case BOTTOM:
                return new CSSPrimitiveValueImpl(CSS_VAL_BOTTOM);
            case BASELINE_MIDDLE:
                return new CSSPrimitiveValueImpl(CSS_VAL__KHTML_BASELINE_MIDDLE);
            case LENGTH:
                return valueForLength(style->verticalAlignLength());
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_VISIBILITY:
        switch (style->visibility()) {
            case VISIBLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_VISIBLE);
            case HIDDEN:
                return new CSSPrimitiveValueImpl(CSS_VAL_HIDDEN);
            case COLLAPSE:
                return new CSSPrimitiveValueImpl(CSS_VAL_COLLAPSE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_WHITE_SPACE:
        switch (style->whiteSpace()) {
            case NORMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case PRE:
                return new CSSPrimitiveValueImpl(CSS_VAL_PRE);
            case PRE_WRAP:
                return new CSSPrimitiveValueImpl(CSS_VAL_PRE_WRAP);
            case PRE_LINE:
                return new CSSPrimitiveValueImpl(CSS_VAL_PRE_LINE);
            case NOWRAP:
                return new CSSPrimitiveValueImpl(CSS_VAL_NOWRAP);
            case KHTML_NOWRAP:
                return new CSSPrimitiveValueImpl(CSS_VAL__KHTML_NOWRAP);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_WIDOWS:
        return new CSSPrimitiveValueImpl(style->widows(), CSSPrimitiveValue::CSS_NUMBER);
    case CSS_PROP_WIDTH:
        return new CSSPrimitiveValueImpl(renderer->contentWidth(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP_WORD_SPACING:
        return new CSSPrimitiveValueImpl(style->wordSpacing(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP_WORD_WRAP:
        switch (style->wordWrap()) {
            case WBNORMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case BREAK_WORD:
                return new CSSPrimitiveValueImpl(CSS_VAL_BREAK_WORD);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_LINE_BREAK:
        switch (style->khtmlLineBreak()) {
            case LBNORMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case AFTER_WHITE_SPACE:
                return new CSSPrimitiveValueImpl(CSS_VAL_AFTER_WHITE_SPACE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_NBSP_MODE:
        switch (style->nbspMode()) {
            case NBNORMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case SPACE:
                return new CSSPrimitiveValueImpl(CSS_VAL_SPACE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR:
        switch (style->matchNearestMailBlockquoteColor()) {
            case BCNORMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case MATCH:
                return new CSSPrimitiveValueImpl(CSS_VAL_MATCH);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_Z_INDEX:
        if (style->hasAutoZIndex())
            return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
        return new CSSPrimitiveValueImpl(style->zIndex(), CSSPrimitiveValue::CSS_NUMBER);
    case CSS_PROP_BACKGROUND:
        // FIXME: unimplemented
        break;
    case CSS_PROP_BORDER:
        // FIXME: unimplemented
        break;
    case CSS_PROP_BORDER_COLOR:
        // FIXME: unimplemented
        break;
    case CSS_PROP_BORDER_STYLE:
        // FIXME: unimplemented
        break;
    case CSS_PROP_BORDER_TOP:
        // FIXME: unimplemented
        break;
    case CSS_PROP_BORDER_RIGHT:
        // FIXME: unimplemented
        break;
    case CSS_PROP_BORDER_BOTTOM:
        // FIXME: unimplemented
        break;
    case CSS_PROP_BORDER_LEFT:
        // FIXME: unimplemented
        break;
    case CSS_PROP_BORDER_WIDTH:
        // FIXME: unimplemented
        break;
    case CSS_PROP_FONT:
        // FIXME: unimplemented
        break;
    case CSS_PROP_LIST_STYLE:
        // FIXME: unimplemented
        break;
    case CSS_PROP_MARGIN:
        // FIXME: unimplemented
        break;
    case CSS_PROP_OUTLINE:
        // FIXME: unimplemented
        break;
    case CSS_PROP_PADDING:
        // FIXME: unimplemented
        break;
#if __APPLE__
        case CSS_PROP__KHTML_DASHBOARD_REGION: {
            QValueList<StyleDashboardRegion> regions = style->dashboardRegions();
            unsigned count = regions.count();
            if (count == 1 && regions[0].type == StyleDashboardRegion::None)
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
            
            RefPtr<DashboardRegionImpl> firstRegion;
            DashboardRegionImpl* previousRegion = 0;
            for (unsigned i = 0; i < count; i++) {
                RefPtr<DashboardRegionImpl> region = new DashboardRegionImpl;
                StyleDashboardRegion styleRegion = regions[i];

                region->m_label = styleRegion.label;
                LengthBox offset = styleRegion.offset;
                region->setTop(new CSSPrimitiveValueImpl(offset.top.value(), CSSPrimitiveValue::CSS_PX));
                region->setRight(new CSSPrimitiveValueImpl(offset.right.value(), CSSPrimitiveValue::CSS_PX));
                region->setBottom(new CSSPrimitiveValueImpl(offset.bottom.value(), CSSPrimitiveValue::CSS_PX));
                region->setLeft(new CSSPrimitiveValueImpl(offset.left.value(), CSSPrimitiveValue::CSS_PX));
                region->m_isRectangle = (styleRegion.type == StyleDashboardRegion::Rectangle); 
                region->m_isCircle = (styleRegion.type == StyleDashboardRegion::Circle);

                if (previousRegion)
                    previousRegion->m_next = region;
                else
                    firstRegion = region;
                previousRegion = region.get();
            }
            return new CSSPrimitiveValueImpl(firstRegion.release());
        }
#endif
    }

    LOG_ERROR("unimplemented propertyID: %d", propertyID);
    return 0;
}

String CSSComputedStyleDeclarationImpl::getPropertyValue(int propertyID) const
{
    RefPtr<CSSValueImpl> value = getPropertyCSSValue(propertyID);
    if (value)
        return value->cssText();
    return "";
}

bool CSSComputedStyleDeclarationImpl::getPropertyPriority(int) const
{
    // All computed styles have a priority of false (not "important").
    return false;
}

String CSSComputedStyleDeclarationImpl::removeProperty(int, int &exceptionCode)
{
    exceptionCode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
    return String();
}

void CSSComputedStyleDeclarationImpl::setProperty(int, const String&, bool, int &exceptionCode)
{
    exceptionCode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
}

unsigned CSSComputedStyleDeclarationImpl::length() const
{
    return numComputedProperties;
}

String CSSComputedStyleDeclarationImpl::item(unsigned i) const
{
    if (i >= numComputedProperties)
        return String();
    
    return getPropertyName(computedProperties[i]);
}

PassRefPtr<CSSMutableStyleDeclarationImpl> CSSComputedStyleDeclarationImpl::copyInheritableProperties() const
{
    return copyPropertiesInSet(inheritableProperties, numInheritableProperties);
}

PassRefPtr<CSSMutableStyleDeclarationImpl> CSSComputedStyleDeclarationImpl::copy() const
{
    return copyPropertiesInSet(computedProperties, numComputedProperties);
}

PassRefPtr<CSSMutableStyleDeclarationImpl> CSSComputedStyleDeclarationImpl::makeMutable()
{
    return copy();
}

} // namespace DOM
