/**
 * css_computedstyle.cpp
 *
 * Copyright (C)  2004  Zack Rusin <zack@kde.org>
 * Copyright (C)  2004  Apple Computer, Inc.
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
using khtml::RenderStyle;
using khtml::ShadowData;
using khtml::StyleDashboardRegion;

namespace DOM {

// This is the list of properties we want to copy in the copyInheritableProperties() function.
// It is the intersection of the list of inherited CSS properties and the
// properties for which we have a computed implementation in this file.
static const int InheritableProperties[] = {
    CSS_PROP_BORDER_COLLAPSE,
    CSS_PROP_BORDER_SPACING,
    CSS_PROP_COLOR,
    CSS_PROP_FONT_FAMILY,
    CSS_PROP_FONT_SIZE,
    CSS_PROP_FONT_STYLE,
    CSS_PROP_FONT_VARIANT,
    CSS_PROP_FONT_WEIGHT,
    CSS_PROP_LETTER_SPACING,
    CSS_PROP_LINE_HEIGHT,
    CSS_PROP_TEXT_ALIGN,
    CSS_PROP_TEXT_DECORATION, // this is not inheritable, yet we do want to consider it for typing style (name change needed? redesign?)
    CSS_PROP_TEXT_INDENT,
    CSS_PROP_TEXT_TRANSFORM,
    CSS_PROP_WHITE_SPACE,
    CSS_PROP_WORD_SPACING,
};

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

CSSValueImpl* CSSComputedStyleDeclarationImpl::getPositionOffsetValue(int propertyID) const
{
    Length l;
    switch (propertyID) {
    case CSS_PROP_LEFT:
        l = m_renderer->style()->left();
        break;
    case CSS_PROP_RIGHT:
        l = m_renderer->style()->right();
        break;
    case CSS_PROP_TOP:
        l = m_renderer->style()->top();
        break;
    case CSS_PROP_BOTTOM:
        l = m_renderer->style()->bottom();
        break;
    default:
        return 0;
    }

    if (m_renderer->isPositioned())
        return valueForLength(l);
    
    if (m_renderer->isRelPositioned())
        // FIXME: It's not enough to simply return "auto" values for one offset if the other side is defined.
        // In other words if left is auto and right is not auto, then left's computed value is negative right.
        // So we should get the opposite length unit and see if it is auto.
        return valueForLength(l);
    
    return new CSSPrimitiveValueImpl(CSS_VAL_AUTO);
}

CSSComputedStyleDeclarationImpl::CSSComputedStyleDeclarationImpl(NodeImpl *n)
    : CSSStyleDeclarationImpl(0)
{
    setNode(n);
    m_renderer = node()->renderer();
}

CSSComputedStyleDeclarationImpl::~CSSComputedStyleDeclarationImpl()
{
}

DOMString CSSComputedStyleDeclarationImpl::cssText() const
{
    ERROR("unimplemented");
    return DOMString();
}

void CSSComputedStyleDeclarationImpl::setCssText(const DOMString &)
{
    ERROR("CSSComputedStyleDeclarationImpl is a read-only object");
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

CSSValueImpl *CSSComputedStyleDeclarationImpl::getPropertyCSSValue(int propertyID, bool updateLayout) const
{
    // Make sure our layout is up to date before we allow a query on these attributes.
    DocumentImpl* docimpl = node()->getDocument();
    if (docimpl && updateLayout)
        docimpl->updateLayout();

    if (!m_renderer)
        return 0;
    RenderStyle *style = m_renderer->style();
    if (!style)
        return 0;

    switch(propertyID)
    {
    case CSS_PROP_BACKGROUND_COLOR:
        return new CSSPrimitiveValueImpl(style->backgroundColor().rgb());
    case CSS_PROP_BACKGROUND_IMAGE:
        if (style->backgroundImage())
            return new CSSPrimitiveValueImpl(style->backgroundImage()->url(),
                                             CSSPrimitiveValue::CSS_URI);
        return 0;
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
            string = numberAsString(length.minWidth(m_renderer->contentWidth()));
        string += " ";
        length = style->backgroundYPosition();
        if (length.isPercent())
            string += numberAsString(length.length()) + "%";
        else
            string += numberAsString(length.minWidth(m_renderer->contentWidth()));
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
        return getPositionOffsetValue(CSS_PROP_BOTTOM);
    case CSS_PROP__KHTML_BOX_ALIGN:
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_BOX_DIRECTION:
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_BOX_FLEX:
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_BOX_FLEX_GROUP:
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_BOX_LINES:
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_BOX_ORDINAL_GROUP:
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_BOX_ORIENT:
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_BOX_PACK:
        // FIXME: unimplemented
        break;
    case CSS_PROP_CAPTION_SIDE:
        // FIXME: unimplemented
        break;
    case CSS_PROP_CLEAR:
        // FIXME: unimplemented
        break;
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
        // FIXME: unimplemented
        break;
    case CSS_PROP_DIRECTION:
        // FIXME: unimplemented
        break;
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
        // FIXME: unimplemented
        break;
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
    case CSS_PROP_FONT_SIZE_ADJUST:
        // FIXME: unimplemented
        break;
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
        return new CSSPrimitiveValueImpl(m_renderer->contentHeight(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP_LEFT:
        return getPositionOffsetValue(CSS_PROP_LEFT);
    case CSS_PROP_LETTER_SPACING:
        if (style->letterSpacing() == 0)
            return new CSSPrimitiveValueImpl(CSS_VAL_NORMAL);
        return new CSSPrimitiveValueImpl(style->letterSpacing(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP_LINE_HEIGHT: {
        Length length(style->lineHeight());
        if (length.isPercent()) {
            float computedSize = style->htmlFont().getFontDef().computedSize;
            return new CSSPrimitiveValueImpl((int)(length.length() * computedSize) / 100, CSSPrimitiveValue::CSS_PX);
        }
        else {
            return new CSSPrimitiveValueImpl(length.length(), CSSPrimitiveValue::CSS_PX);
        }
    }
    case CSS_PROP_LIST_STYLE_IMAGE:
        // FIXME: unimplemented
        break;
    case CSS_PROP_LIST_STYLE_POSITION:
        // FIXME: unimplemented
        break;
    case CSS_PROP_LIST_STYLE_TYPE:
        // FIXME: unimplemented
        break;
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
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_MARQUEE_INCREMENT:
        return valueForLength(style->marqueeIncrement());
    case CSS_PROP__KHTML_MARQUEE_REPETITION:
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_MARQUEE_SPEED:
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_MARQUEE_STYLE:
        // FIXME: unimplemented
        break;
    case CSS_PROP__KHTML_USER_MODIFY:
        // FIXME: unimplemented
        break;
    case CSS_PROP_MAX_HEIGHT:
        return valueForLength(style->maxHeight());
    case CSS_PROP_MAX_WIDTH:
        return valueForLength(style->maxWidth());
    case CSS_PROP_MIN_HEIGHT:
        return valueForLength(style->minHeight());
    case CSS_PROP_MIN_WIDTH:
        return valueForLength(style->minWidth());
    case CSS_PROP_OPACITY:
        // FIXME: unimplemented
        break;
    case CSS_PROP_ORPHANS:
        // FIXME: unimplemented
        break;
        // FIXME: unimplemented
        break;
    case CSS_PROP_OUTLINE_COLOR:
        // FIXME: unimplemented
        break;
    case CSS_PROP_OUTLINE_OFFSET:
        // FIXME: unimplemented
        break;
    case CSS_PROP_OUTLINE_STYLE:
        // FIXME: unimplemented
        break;
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
        // FIXME: unimplemented
        break;
    case CSS_PROP_PAGE_BREAK_BEFORE:
        // FIXME: unimplemented
        break;
    case CSS_PROP_PAGE_BREAK_INSIDE:
        // FIXME: unimplemented
        break;
    case CSS_PROP_POSITION:
        // FIXME: unimplemented
        break;
    case CSS_PROP_QUOTES:
        // FIXME: unimplemented
        break;
    case CSS_PROP_RIGHT:
        return getPositionOffsetValue(CSS_PROP_RIGHT);
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
    case CSS_PROP_TEXT_INDENT:
        return valueForLength(style->textIndent());
    case CSS_PROP_TEXT_SHADOW:
        return valueForShadow(style->textShadow());
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
        return getPositionOffsetValue(CSS_PROP_TOP);
    case CSS_PROP_UNICODE_BIDI:
        // FIXME: unimplemented
        break;
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
        // FIXME: unimplemented
        break;
    case CSS_PROP_WIDTH:
        return new CSSPrimitiveValueImpl(m_renderer->contentWidth(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP_WORD_SPACING:
        return new CSSPrimitiveValueImpl(style->wordSpacing(), CSSPrimitiveValue::CSS_PX);
    case CSS_PROP_Z_INDEX:
        // FIXME: unimplemented
        break;
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
    case CSS_PROP__KHTML_FLOW_MODE:
        // FIXME: unimplemented
        break;
#if APPLE_CHANGES
        case CSS_PROP__APPLE_DASHBOARD_REGION: {
            QValueList<StyleDashboardRegion> regions = style->dashboardRegions();
            uint i, count = regions.count();
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
    if (value)
        return value->cssText();
    return "";
}

bool CSSComputedStyleDeclarationImpl::getPropertyPriority(int) const
{
    // This class does not support the notion of priority, since the object
    // is a computed value.
    return false;
}

DOMString CSSComputedStyleDeclarationImpl::removeProperty(int)
{
    ASSERT_NOT_REACHED();
    return DOMString();
}

bool CSSComputedStyleDeclarationImpl::setProperty(int, const DOMString &, bool)
{
    ASSERT_NOT_REACHED();
    return false;
}

void CSSComputedStyleDeclarationImpl::setProperty(int, int, bool)
{
    ASSERT_NOT_REACHED();
}

void CSSComputedStyleDeclarationImpl::setLengthProperty(int, const DOMString&, bool, bool)
{
    ASSERT_NOT_REACHED();
}

void CSSComputedStyleDeclarationImpl::setProperty(const DOMString &)
{
    ASSERT_NOT_REACHED();
}

DOMString CSSComputedStyleDeclarationImpl::item(unsigned long) const
{
    ERROR("unimplemented");
    return DOMString();
}


CSSProperty CSSComputedStyleDeclarationImpl::property(int id) const
{
    CSSProperty prop;
    prop.m_id = id;
    prop.m_bImportant = false;
    prop.setValue(getPropertyCSSValue(id));
    return prop;
}

CSSStyleDeclarationImpl *CSSComputedStyleDeclarationImpl::copyInheritableProperties() const
{
    QPtrList<CSSProperty> *list = new QPtrList<CSSProperty>;
    list->setAutoDelete(true);
    for (unsigned i = 0; i < sizeof(InheritableProperties) / sizeof(InheritableProperties[0]); i++) {
        CSSValueImpl *value = getPropertyCSSValue(InheritableProperties[i]);
        if (value) {
            CSSProperty *property = new CSSProperty;
            property->m_id = InheritableProperties[i];
            property->setValue(value);
            list->append(property);
        }
    }
    return new CSSStyleDeclarationImpl(0, list);
}

void CSSComputedStyleDeclarationImpl::diff(CSSStyleDeclarationImpl *style) const
{
    if (!style)
        return;

    QValueList<int> properties;
    for (QPtrListIterator<CSSProperty> it(*style->values()); it.current(); ++it) {
        CSSProperty *property = it.current();
        CSSValueImpl *value = getPropertyCSSValue(property->id());
        if (value && value->cssText() == property->value()->cssText()) {
            properties.append(property->id());
        }
    }
    
    for (QValueListIterator<int> it(properties.begin()); it != properties.end(); ++it)
        style->removeProperty(*it);
}

} // namespace DOM
