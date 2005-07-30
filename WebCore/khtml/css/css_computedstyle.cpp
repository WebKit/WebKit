/**
 * css_computedstyle.cpp
 *
 * Copyright (C)  2004  Zack Rusin <zack@kde.org>
 * Copyright (C) 2004 Apple Computer, Inc.
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

#include "css_computedstyle.h"

#include "cssproperties.h"
#include "cssvalues.h"
#include "dom_atomicstring.h"
#include "dom_exception.h"
#include "dom_string.h"
#include "font.h"
#include "khtmllayout.h"
#include "loader.h"
#include "rendering/render_style.h"
#include "rendering/render_object.h"

#if APPLE_CHANGES
#import "KWQAssertions.h"
#import "KWQFontFamily.h"
#import "KWQLogging.h"
#endif

using khtml::EBorderStyle;
using khtml::ETextAlign;
using khtml::Font;
using khtml::FontDef;
using khtml::Length;
using khtml::LengthBox;
using khtml::RenderObject;
using khtml::RenderStyle;
using khtml::ShadowData;
using khtml::StyleDashboardRegion;

extern DOM::DOMString getPropertyName(unsigned short id);

namespace DOM {

// List of all properties we know how to compute, omitting shorthands.
static const int computedProperties[] = {
    CSS_PROP_BACKGROUND_COLOR,
    CSS_PROP_BACKGROUND_IMAGE,
    CSS_PROP_BACKGROUND_REPEAT,
    CSS_PROP_BACKGROUND_ATTACHMENT,
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
    CSS_PROP__KHTML_DASHBOARD_REGION,
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
    switch (length.type) {
        case khtml::Percent:
            return new CSSPrimitiveValueImpl(length.length(), CSSPrimitiveValue::CSS_PERCENTAGE);
        case khtml::Fixed:
            return new CSSPrimitiveValueImpl(length.length(), CSSPrimitiveValue::CSS_PX);
        default: // FIXME: Intrinsic and MinIntrinsic should probably return keywords.
            return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
    }
}

static CSSValueImpl *valueForBorderStyle(EBorderStyle style)
{
    switch (style) {
        case khtml::BNONE:
            return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
        case khtml::BHIDDEN:
            return new CSSPrimitiveValueImpl(CSS_VAL_HIDDEN);
        case khtml::INSET:
            return new CSSPrimitiveValueImpl(CSS_VAL_INSET);
        case khtml::GROOVE:
            return new CSSPrimitiveValueImpl(CSS_VAL_GROOVE);
        case khtml::RIDGE:
            return new CSSPrimitiveValueImpl(CSS_VAL_RIDGE);
        case khtml::OUTSET:
            return new CSSPrimitiveValueImpl(CSS_VAL_OUTSET);
        case khtml::DOTTED:
            return new CSSPrimitiveValueImpl(CSS_VAL_DOTTED);
        case khtml::DASHED:
            return new CSSPrimitiveValueImpl(CSS_VAL_DASHED);
        case khtml::SOLID:
            return new CSSPrimitiveValueImpl(CSS_VAL_SOLID);
        case khtml::DOUBLE:
            return new CSSPrimitiveValueImpl(CSS_VAL_DOUBLE);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static CSSValueImpl *valueForTextAlign(ETextAlign align)
{
    switch (align) {
        case khtml::TAAUTO:
            return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
        case khtml::LEFT:
            return new CSSPrimitiveValueImpl(CSS_VAL_LEFT);
        case khtml::RIGHT:
            return new CSSPrimitiveValueImpl(CSS_VAL_RIGHT);
        case khtml::CENTER:
            return new CSSPrimitiveValueImpl(CSS_VAL_CENTER);
        case khtml::JUSTIFY:
            return new CSSPrimitiveValueImpl(CSS_VAL_JUSTIFY);
        case khtml::KHTML_LEFT:
            return new CSSPrimitiveValueImpl(CSS_VAL__KHTML_LEFT);
        case khtml::KHTML_RIGHT:
            return new CSSPrimitiveValueImpl(CSS_VAL__KHTML_RIGHT);
        case khtml::KHTML_CENTER:
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

CSSComputedStyleDeclarationImpl::CSSComputedStyleDeclarationImpl(NodeImpl *n)
    : m_node(n)
{
}

CSSComputedStyleDeclarationImpl::~CSSComputedStyleDeclarationImpl()
{
}

DOMString CSSComputedStyleDeclarationImpl::cssText() const
{
    DOMString result;
    
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

void CSSComputedStyleDeclarationImpl::setCssText(const DOMString &, int &exceptionCode)
{
    exceptionCode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
}

// Display integers in integer format instead of "1.0".
static QString numberAsString(double n)
{
    long i = static_cast<long>(n);
    return i == n ? QString::number(i) : QString::number(n);
}

CSSValueImpl *CSSComputedStyleDeclarationImpl::getPropertyCSSValue(int propertyID) const
{
    return getPropertyCSSValue(propertyID, UpdateLayout);
}

CSSValueImpl *CSSComputedStyleDeclarationImpl::getPropertyCSSValue(int propertyID, EUpdateLayout updateLayout) const
{
    NodeImpl *node = m_node.get();
    if (!node)
        return 0;

    // Make sure our layout is up to date before we allow a query on these attributes.
    DocumentImpl* docimpl = node->getDocument();
    if (docimpl && updateLayout)
        docimpl->updateLayout();

    RenderObject *renderer = node->renderer();
    if (!renderer)
        return 0;
    RenderStyle *style = renderer->style();
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
            case khtml::REPEAT:
                return new CSSPrimitiveValueImpl(CSS_VAL_REPEAT);
            case khtml::REPEAT_X:
                return new CSSPrimitiveValueImpl(CSS_VAL_REPEAT_X);
            case khtml::REPEAT_Y:
                return new CSSPrimitiveValueImpl(CSS_VAL_REPEAT_Y);
            case khtml::NO_REPEAT:
                return new CSSPrimitiveValueImpl(CSS_VAL_NO_REPEAT);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_BACKGROUND_ATTACHMENT:
        if (style->backgroundAttachment())
            return new CSSPrimitiveValueImpl(CSS_VAL_SCROLL);
        else
            return new CSSPrimitiveValueImpl(CSS_VAL_FIXED);
    case CSS_PROP_BACKGROUND_POSITION:
    {
        DOMString string;
        Length length(style->backgroundXPosition());
        if (length.isPercent())
            string = numberAsString(length.length()) + "%";
        else
            string = numberAsString(length.minWidth(renderer->contentWidth()));
        string += " ";
        length = style->backgroundYPosition();
        if (length.isPercent())
            string += numberAsString(length.length()) + "%";
        else
            string += numberAsString(length.minWidth(renderer->contentWidth()));
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
            case khtml::BSTRETCH:
                return new CSSPrimitiveValueImpl(CSS_VAL_STRETCH);
            case khtml::BSTART:
                return new CSSPrimitiveValueImpl(CSS_VAL_START);
            case khtml::BCENTER:
                return new CSSPrimitiveValueImpl(CSS_VAL_CENTER);
            case khtml::BEND:
                return new CSSPrimitiveValueImpl(CSS_VAL_END);
            case khtml::BBASELINE:
                return new CSSPrimitiveValueImpl(CSS_VAL_BASELINE);
            case khtml::BJUSTIFY:
                break; // not allowed
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_BOX_DIRECTION:
        switch (style->boxDirection()) {
            case khtml::BNORMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case khtml::BREVERSE:
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
            case khtml::SINGLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_SINGLE);
            case khtml::MULTIPLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_MULTIPLE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_BOX_ORDINAL_GROUP:
        return new CSSPrimitiveValueImpl(style->boxOrdinalGroup(), CSSPrimitiveValue::CSS_NUMBER);
    case CSS_PROP__KHTML_BOX_ORIENT:
        switch (style->boxOrient()) {
            case khtml::HORIZONTAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_HORIZONTAL);
            case khtml::VERTICAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_VERTICAL);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_BOX_PACK:
        switch (style->boxPack()) {
            case khtml::BSTART:
                return new CSSPrimitiveValueImpl(CSS_VAL_START);
            case khtml::BEND:
                return new CSSPrimitiveValueImpl(CSS_VAL_END);
            case khtml::BCENTER:
                return new CSSPrimitiveValueImpl(CSS_VAL_CENTER);
            case khtml::BJUSTIFY:
                return new CSSPrimitiveValueImpl(CSS_VAL_JUSTIFY);
            case khtml::BSTRETCH:
            case khtml::BBASELINE:
                break; // not allowed
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_CAPTION_SIDE:
        switch (style->captionSide()) {
            case khtml::CAPLEFT:
                return new CSSPrimitiveValueImpl(CSS_VAL_LEFT);
            case khtml::CAPRIGHT:
                return new CSSPrimitiveValueImpl(CSS_VAL_RIGHT);
            case khtml::CAPTOP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TOP);
            case khtml::CAPBOTTOM:
                return new CSSPrimitiveValueImpl(CSS_VAL_BOTTOM);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_CLEAR:
        switch (style->clear()) {
            case khtml::CNONE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
            case khtml::CLEFT:
                return new CSSPrimitiveValueImpl(CSS_VAL_LEFT);
            case khtml::CRIGHT:
                return new CSSPrimitiveValueImpl(CSS_VAL_RIGHT);
            case khtml::CBOTH:
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
        switch (style->cursor()) {
            case khtml::CURSOR_AUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case khtml::CURSOR_CROSS:
                return new CSSPrimitiveValueImpl(CSS_VAL_CROSSHAIR);
            case khtml::CURSOR_DEFAULT:
                return new CSSPrimitiveValueImpl(CSS_VAL_DEFAULT);
            case khtml::CURSOR_POINTER:
                return new CSSPrimitiveValueImpl(CSS_VAL_POINTER);
            case khtml::CURSOR_MOVE:
                return new CSSPrimitiveValueImpl(CSS_VAL_MOVE);
            case khtml::CURSOR_E_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_E_RESIZE);
            case khtml::CURSOR_NE_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NE_RESIZE);
            case khtml::CURSOR_NW_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NW_RESIZE);
            case khtml::CURSOR_N_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_N_RESIZE);
            case khtml::CURSOR_SE_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_SE_RESIZE);
            case khtml::CURSOR_SW_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_SW_RESIZE);
            case khtml::CURSOR_S_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_S_RESIZE);
            case khtml::CURSOR_W_RESIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_W_RESIZE);
            case khtml::CURSOR_TEXT:
                return new CSSPrimitiveValueImpl(CSS_VAL_TEXT);
            case khtml::CURSOR_WAIT:
                return new CSSPrimitiveValueImpl(CSS_VAL_WAIT);
            case khtml::CURSOR_HELP:
                return new CSSPrimitiveValueImpl(CSS_VAL_HELP);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_DIRECTION:
        switch (style->direction()) {
            case khtml::LTR:
                return new CSSPrimitiveValueImpl(CSS_VAL_LTR);
            case khtml::RTL:
                return new CSSPrimitiveValueImpl(CSS_VAL_RTL);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_DISPLAY:
        switch (style->display()) {
            case khtml::INLINE:
                return new CSSPrimitiveValueImpl(CSS_VAL_INLINE);
            case khtml::BLOCK:
                return new CSSPrimitiveValueImpl(CSS_VAL_BLOCK);
            case khtml::LIST_ITEM:
                return new CSSPrimitiveValueImpl(CSS_VAL_LIST_ITEM);
            case khtml::RUN_IN:
                return new CSSPrimitiveValueImpl(CSS_VAL_RUN_IN);
            case khtml::COMPACT:
                return new CSSPrimitiveValueImpl(CSS_VAL_COMPACT);
            case khtml::INLINE_BLOCK:
                return new CSSPrimitiveValueImpl(CSS_VAL_INLINE_BLOCK);
            case khtml::TABLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE);
            case khtml::INLINE_TABLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_INLINE_TABLE);
            case khtml::TABLE_ROW_GROUP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_ROW_GROUP);
            case khtml::TABLE_HEADER_GROUP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_HEADER_GROUP);
            case khtml::TABLE_FOOTER_GROUP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_FOOTER_GROUP);
            case khtml::TABLE_ROW:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_ROW);
            case khtml::TABLE_COLUMN_GROUP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_COLUMN_GROUP);
            case khtml::TABLE_COLUMN:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_COLUMN);
            case khtml::TABLE_CELL:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_CELL);
            case khtml::TABLE_CAPTION:
                return new CSSPrimitiveValueImpl(CSS_VAL_TABLE_CAPTION);
            case khtml::BOX:
                return new CSSPrimitiveValueImpl(CSS_VAL__KHTML_BOX);
            case khtml::INLINE_BOX:
                return new CSSPrimitiveValueImpl(CSS_VAL__KHTML_INLINE_BOX);
            case khtml::NONE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_EMPTY_CELLS:
        switch (style->emptyCells()) {
            case khtml::SHOW:
                return new CSSPrimitiveValueImpl(CSS_VAL_SHOW);
            case khtml::HIDE:
                return new CSSPrimitiveValueImpl(CSS_VAL_HIDE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_FLOAT:
        switch (style->floating()) {
            case khtml::FNONE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
            case khtml::FLEFT:
                return new CSSPrimitiveValueImpl(CSS_VAL_LEFT);
            case khtml::FRIGHT:
                return new CSSPrimitiveValueImpl(CSS_VAL_RIGHT);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_FONT_FAMILY:
    {
        FontDef def = style->htmlFont().getFontDef();
        return new CSSPrimitiveValueImpl(def.firstFamily().family().domString(), CSSPrimitiveValue::CSS_STRING);
    }
    case CSS_PROP_FONT_SIZE:
    {
        FontDef def = style->htmlFont().getFontDef();
        return new CSSPrimitiveValueImpl(def.specifiedSize, CSSPrimitiveValue::CSS_PX);
    }
    case CSS_PROP_FONT_STRETCH:
        // FIXME: unimplemented
        break;
    case CSS_PROP_FONT_STYLE:
    {
        // FIXME: handle oblique?
        FontDef def = style->htmlFont().getFontDef();
        if (def.italic)
            return new CSSPrimitiveValueImpl(CSS_VAL_ITALIC);
        else
            return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
    }
    case CSS_PROP_FONT_VARIANT:
    {
        FontDef def = style->htmlFont().getFontDef();
        if (def.smallCaps)
            return new CSSPrimitiveValueImpl(CSS_VAL_SMALL_CAPS);
        else
            return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
    }
    case CSS_PROP_FONT_WEIGHT:
    {
        // FIXME: this does not reflect the full range of weights
        // that can be expressed with CSS
        FontDef def = style->htmlFont().getFontDef();
        if (def.weight == QFont::Bold)
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
	if (length.value < 0)
            return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
        if (length.isPercent()) {
            // This is imperfect, because it doesn't include the zoom factor and the real computation
            // for how high to be in pixels does include things like minimum font size and the zoom factor.
            // On the other hand, since font-size doesn't include the zoom factor, we really can't do
            // that here either.
            float fontSize = style->htmlFont().getFontDef().specifiedSize;
            return new CSSPrimitiveValueImpl((int)(length.length() * fontSize) / 100, CSSPrimitiveValue::CSS_PX);
        }
        else {
            return new CSSPrimitiveValueImpl(length.length(), CSSPrimitiveValue::CSS_PX);
        }
    }
    case CSS_PROP_LIST_STYLE_IMAGE:
        if (style->listStyleImage())
            return new CSSPrimitiveValueImpl(style->listStyleImage()->url(), CSSPrimitiveValue::CSS_URI);
        return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
    case CSS_PROP_LIST_STYLE_POSITION:
        switch (style->listStylePosition()) {
            case khtml::OUTSIDE:
                return new CSSPrimitiveValueImpl(CSS_VAL_OUTSIDE);
            case khtml::INSIDE:
                return new CSSPrimitiveValueImpl(CSS_VAL_INSIDE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_LIST_STYLE_TYPE:
        switch (style->listStyleType()) {
            case khtml::LNONE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
            case khtml::DISC:
                return new CSSPrimitiveValueImpl(CSS_VAL_DISC);
            case khtml::CIRCLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_CIRCLE);
            case khtml::SQUARE:
                return new CSSPrimitiveValueImpl(CSS_VAL_SQUARE);
            case khtml::LDECIMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_DECIMAL);
            case khtml::DECIMAL_LEADING_ZERO:
                return new CSSPrimitiveValueImpl(CSS_VAL_DECIMAL_LEADING_ZERO);
            case khtml::LOWER_ROMAN:
                return new CSSPrimitiveValueImpl(CSS_VAL_LOWER_ROMAN);
            case khtml::UPPER_ROMAN:
                return new CSSPrimitiveValueImpl(CSS_VAL_UPPER_ROMAN);
            case khtml::LOWER_GREEK:
                return new CSSPrimitiveValueImpl(CSS_VAL_LOWER_GREEK);
            case khtml::LOWER_ALPHA:
                return new CSSPrimitiveValueImpl(CSS_VAL_LOWER_ALPHA);
            case khtml::LOWER_LATIN:
                return new CSSPrimitiveValueImpl(CSS_VAL_LOWER_LATIN);
            case khtml::UPPER_ALPHA:
                return new CSSPrimitiveValueImpl(CSS_VAL_UPPER_ALPHA);
            case khtml::UPPER_LATIN:
                return new CSSPrimitiveValueImpl(CSS_VAL_UPPER_LATIN);
            case khtml::HEBREW:
                return new CSSPrimitiveValueImpl(CSS_VAL_HEBREW);
            case khtml::ARMENIAN:
                return new CSSPrimitiveValueImpl(CSS_VAL_ARMENIAN);
            case khtml::GEORGIAN:
                return new CSSPrimitiveValueImpl(CSS_VAL_GEORGIAN);
            case khtml::CJK_IDEOGRAPHIC:
                return new CSSPrimitiveValueImpl(CSS_VAL_CJK_IDEOGRAPHIC);
            case khtml::HIRAGANA:
                return new CSSPrimitiveValueImpl(CSS_VAL_HIRAGANA);
            case khtml::KATAKANA:
                return new CSSPrimitiveValueImpl(CSS_VAL_KATAKANA);
            case khtml::HIRAGANA_IROHA:
                return new CSSPrimitiveValueImpl(CSS_VAL_HIRAGANA_IROHA);
            case khtml::KATAKANA_IROHA:
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
            case khtml::MFORWARD:
                return new CSSPrimitiveValueImpl(CSS_VAL_FORWARDS);
            case khtml::MBACKWARD:
                return new CSSPrimitiveValueImpl(CSS_VAL_BACKWARDS);
            case khtml::MAUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case khtml::MUP:
                return new CSSPrimitiveValueImpl(CSS_VAL_UP);
            case khtml::MDOWN:
                return new CSSPrimitiveValueImpl(CSS_VAL_DOWN);
            case khtml::MLEFT:
                return new CSSPrimitiveValueImpl(CSS_VAL_LEFT);
            case khtml::MRIGHT:
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
            case khtml::MNONE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
            case khtml::MSCROLL:
                return new CSSPrimitiveValueImpl(CSS_VAL_SCROLL);
            case khtml::MSLIDE:
                return new CSSPrimitiveValueImpl(CSS_VAL_SLIDE);
            case khtml::MALTERNATE:
                return new CSSPrimitiveValueImpl(CSS_VAL_ALTERNATE);
            case khtml::MUNFURL:
                return new CSSPrimitiveValueImpl(CSS_VAL_UNFURL);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_USER_MODIFY:
        switch (style->userModify()) {
            case khtml::READ_ONLY:
                return new CSSPrimitiveValueImpl(CSS_VAL_READ_ONLY);
            case khtml::READ_WRITE:
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
            case khtml::OVISIBLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_VISIBLE);
            case khtml::OHIDDEN:
                return new CSSPrimitiveValueImpl(CSS_VAL_HIDDEN);
            case khtml::OSCROLL:
                return new CSSPrimitiveValueImpl(CSS_VAL_SCROLL);
            case khtml::OAUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case khtml::OMARQUEE:
                return new CSSPrimitiveValueImpl(CSS_VAL_MARQUEE);
            case khtml::OOVERLAY:
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
            case khtml::PBAUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case khtml::PBALWAYS:
                return new CSSPrimitiveValueImpl(CSS_VAL_ALWAYS);
            case khtml::PBAVOID:
                return new CSSPrimitiveValueImpl(CSS_VAL_AVOID);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_PAGE_BREAK_BEFORE:
        switch (style->pageBreakBefore()) {
            case khtml::PBAUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case khtml::PBALWAYS:
                return new CSSPrimitiveValueImpl(CSS_VAL_ALWAYS);
            case khtml::PBAVOID:
                return new CSSPrimitiveValueImpl(CSS_VAL_AVOID);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_PAGE_BREAK_INSIDE:
        switch (style->pageBreakInside()) {
            case khtml::PBAUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case khtml::PBAVOID:
                return new CSSPrimitiveValueImpl(CSS_VAL_AVOID);
            case khtml::PBALWAYS:
                break; // not allowed
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_POSITION:
        switch (style->position()) {
            case khtml::STATIC:
                return new CSSPrimitiveValueImpl(CSS_VAL_STATIC);
            case khtml::RELATIVE:
                return new CSSPrimitiveValueImpl(CSS_VAL_RELATIVE);
            case khtml::ABSOLUTE:
                return new CSSPrimitiveValueImpl(CSS_VAL_ABSOLUTE);
            case khtml::FIXED:
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
            case khtml::TAUTO:
                return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
            case khtml::TFIXED:
                return new CSSPrimitiveValueImpl(CSS_VAL_FIXED);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_TEXT_ALIGN:
        return valueForTextAlign(style->textAlign());
    case CSS_PROP_TEXT_DECORATION:
    {
        QString string;
        if (style->textDecoration() & khtml::UNDERLINE)
            string += "underline";
        if (style->textDecoration() & khtml::OVERLINE) {
            if (string.length() > 0)
                string += " ";
            string += "overline";
        }
        if (style->textDecoration() & khtml::LINE_THROUGH) {
            if (string.length() > 0)
                string += " ";
            string += "line-through";
        }
        if (style->textDecoration() & khtml::BLINK) {
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
        if (style->textDecorationsInEffect() & khtml::UNDERLINE)
            string += "underline";
        if (style->textDecorationsInEffect() & khtml::OVERLINE) {
            if (string.length() > 0)
                string += " ";
            string += "overline";
        }
        if (style->textDecorationsInEffect() & khtml::LINE_THROUGH) {
            if (string.length() > 0)
                string += " ";
            string += "line-through";
        }
        if (style->textDecorationsInEffect() & khtml::BLINK) {
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
            case khtml::CAPITALIZE:
                return new CSSPrimitiveValueImpl(CSS_VAL_CAPITALIZE);
            case khtml::UPPERCASE:
                return new CSSPrimitiveValueImpl(CSS_VAL_UPPERCASE);
            case khtml::LOWERCASE:
                return new CSSPrimitiveValueImpl(CSS_VAL_LOWERCASE);
            case khtml::TTNONE:
                return new CSSPrimitiveValueImpl(CSS_VAL_NONE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_TOP:
        return getPositionOffsetValue(renderer, CSS_PROP_TOP);
    case CSS_PROP_UNICODE_BIDI:
        switch (style->unicodeBidi()) {
            case khtml::UBNormal:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case khtml::Embed:
                return new CSSPrimitiveValueImpl(CSS_VAL_EMBED);
            case khtml::Override:
                return new CSSPrimitiveValueImpl(CSS_VAL_BIDI_OVERRIDE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_VERTICAL_ALIGN:
        switch (style->verticalAlign()) {
            case khtml::BASELINE:
                return new CSSPrimitiveValueImpl(CSS_VAL_BASELINE);
            case khtml::MIDDLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_MIDDLE);
            case khtml::SUB:
                return new CSSPrimitiveValueImpl(CSS_VAL_SUB);
            case khtml::SUPER:
                return new CSSPrimitiveValueImpl(CSS_VAL_SUPER);
            case khtml::TEXT_TOP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TEXT_TOP);
            case khtml::TEXT_BOTTOM:
                return new CSSPrimitiveValueImpl(CSS_VAL_TEXT_BOTTOM);
            case khtml::TOP:
                return new CSSPrimitiveValueImpl(CSS_VAL_TOP);
            case khtml::BOTTOM:
                return new CSSPrimitiveValueImpl(CSS_VAL_BOTTOM);
            case khtml::BASELINE_MIDDLE:
                return new CSSPrimitiveValueImpl(CSS_VAL__KHTML_BASELINE_MIDDLE);
            case khtml::LENGTH:
                return valueForLength(style->verticalAlignLength());
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_VISIBILITY:
        switch (style->visibility()) {
            case khtml::VISIBLE:
                return new CSSPrimitiveValueImpl(CSS_VAL_VISIBLE);
            case khtml::HIDDEN:
                return new CSSPrimitiveValueImpl(CSS_VAL_HIDDEN);
            case khtml::COLLAPSE:
                return new CSSPrimitiveValueImpl(CSS_VAL_COLLAPSE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP_WHITE_SPACE:
        switch (style->whiteSpace()) {
            case khtml::NORMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case khtml::PRE:
                return new CSSPrimitiveValueImpl(CSS_VAL_PRE);
            case khtml::NOWRAP:
                return new CSSPrimitiveValueImpl(CSS_VAL_NOWRAP);
            case khtml::KHTML_NOWRAP:
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
            case khtml::WBNORMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case khtml::BREAK_WORD:
                return new CSSPrimitiveValueImpl(CSS_VAL_BREAK_WORD);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_LINE_BREAK:
        switch (style->khtmlLineBreak()) {
            case khtml::LBNORMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case khtml::AFTER_WHITE_SPACE:
                return new CSSPrimitiveValueImpl(CSS_VAL_AFTER_WHITE_SPACE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_NBSP_MODE:
        switch (style->nbspMode()) {
            case khtml::NBNORMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case khtml::SPACE:
                return new CSSPrimitiveValueImpl(CSS_VAL_SPACE);
        }
        ASSERT_NOT_REACHED();
        return 0;
    case CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR:
        switch (style->matchNearestMailBlockquoteColor()) {
            case khtml::BCNORMAL:
                return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
            case khtml::MATCH:
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
#if !APPLE_CHANGES
    case CSS_PROP_SCROLLBAR_FACE_COLOR:
        // FIXME: unimplemented
        break;
    case CSS_PROP_SCROLLBAR_SHADOW_COLOR:
        // FIXME: unimplemented
        break;
    case CSS_PROP_SCROLLBAR_HIGHLIGHT_COLOR:
        // FIXME: unimplemented
        break;
    case CSS_PROP_SCROLLBAR_3DLIGHT_COLOR:
        // FIXME: unimplemented
        break;
    case CSS_PROP_SCROLLBAR_DARKSHADOW_COLOR:
        // FIXME: unimplemented
        break;
    case CSS_PROP_SCROLLBAR_TRACK_COLOR:
        // FIXME: unimplemented
        break;
    case CSS_PROP_SCROLLBAR_ARROW_COLOR:
        // FIXME: unimplemented
        break;
#endif
#if APPLE_CHANGES
        case CSS_PROP__KHTML_DASHBOARD_REGION: {
            QValueList<StyleDashboardRegion> regions = style->dashboardRegions();
            uint i, count = regions.count();
            if (count == 1 && regions[0].type == StyleDashboardRegion::None)
                return new CSSPrimitiveValueImpl (CSS_VAL_NONE);
                
            DashboardRegionImpl *firstRegion = new DashboardRegionImpl(), *region;
            region = firstRegion;
            for (i = 0; i < count; i++) {
                StyleDashboardRegion styleRegion = regions[i];
                region->m_label = styleRegion.label;
                LengthBox offset = styleRegion.offset;
                region->setTop (new CSSPrimitiveValueImpl(offset.top.value, CSSPrimitiveValue::CSS_PX));
                region->setRight (new CSSPrimitiveValueImpl(offset.right.value, CSSPrimitiveValue::CSS_PX));
                region->setBottom (new CSSPrimitiveValueImpl(offset.bottom.value, CSSPrimitiveValue::CSS_PX));
                region->setLeft (new CSSPrimitiveValueImpl(offset.left.value, CSSPrimitiveValue::CSS_PX));
                region->m_isRectangle = (styleRegion.type == StyleDashboardRegion::Rectangle); 
                region->m_isCircle = (styleRegion.type == StyleDashboardRegion::Circle);
                if (i != count-1) {
                    DashboardRegionImpl *newRegion = new DashboardRegionImpl();
                    region->setNext (newRegion);
                    region = newRegion;
                }
            }
            return new CSSPrimitiveValueImpl(firstRegion);
        }
#endif
    }

    ERROR("unimplemented propertyID: %d", propertyID);
    return 0;
}

DOMString CSSComputedStyleDeclarationImpl::getPropertyValue(int propertyID) const
{
    CSSValueImpl* value = getPropertyCSSValue(propertyID);
    if (value) {
        value->ref();
        DOMString result = value->cssText();
        value->deref();
        return result;
    }
    return "";
}

bool CSSComputedStyleDeclarationImpl::getPropertyPriority(int) const
{
    // All computed styles have a priority of false (not "important").
    return false;
}

DOMString CSSComputedStyleDeclarationImpl::removeProperty(int, int &exceptionCode)
{
    exceptionCode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
    return DOMString();
}

void CSSComputedStyleDeclarationImpl::setProperty(int, const DOMString &, bool, int &exceptionCode)
{
    exceptionCode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
}

unsigned long CSSComputedStyleDeclarationImpl::length() const
{
    return numComputedProperties;
}

DOMString CSSComputedStyleDeclarationImpl::item(unsigned long i) const
{
    if (i >= numComputedProperties)
        return DOMString();
    
    return getPropertyName(computedProperties[i]);
}

CSSMutableStyleDeclarationImpl *CSSComputedStyleDeclarationImpl::copyInheritableProperties() const
{
    return copyPropertiesInSet(inheritableProperties, numInheritableProperties);
}

CSSMutableStyleDeclarationImpl *CSSComputedStyleDeclarationImpl::copy() const
{
    return copyPropertiesInSet(computedProperties, numComputedProperties);
}

CSSMutableStyleDeclarationImpl *CSSComputedStyleDeclarationImpl::makeMutable()
{
    return copy();
}

} // namespace DOM
