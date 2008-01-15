/*
 * Copyright (C) 2007 Alexey Proskuryakov <ap@nypop.com>.
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSPrimitiveValueMappings_h
#define CSSPrimitiveValueMappings_h

#include "CSSPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include "RenderStyle.h"

namespace WebCore {

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBorderStyle e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case BNONE:
            m_value.ident = CSS_VAL_NONE;
            break;
        case BHIDDEN:
            m_value.ident = CSS_VAL_HIDDEN;
            break;
        case INSET:
            m_value.ident = CSS_VAL_INSET;
            break;
        case GROOVE:
            m_value.ident = CSS_VAL_GROOVE;
            break;
        case RIDGE:
            m_value.ident = CSS_VAL_RIDGE;
            break;
        case OUTSET:
            m_value.ident = CSS_VAL_OUTSET;
            break;
        case DOTTED:
            m_value.ident = CSS_VAL_DOTTED;
            break;
        case DASHED:
            m_value.ident = CSS_VAL_DASHED;
            break;
        case SOLID:
            m_value.ident = CSS_VAL_SOLID;
            break;
        case DOUBLE:
            m_value.ident = CSS_VAL_DOUBLE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EBorderStyle() const
{
    return (EBorderStyle)(m_value.ident - CSS_VAL_NONE);
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(CompositeOperator e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case CompositeClear:
            m_value.ident = CSS_VAL_CLEAR;
            break;
        case CompositeCopy:
            m_value.ident = CSS_VAL_COPY;
            break;
        case CompositeSourceOver:
            m_value.ident = CSS_VAL_SOURCE_OVER;
            break;
        case CompositeSourceIn:
            m_value.ident = CSS_VAL_SOURCE_IN;
            break;
        case CompositeSourceOut:
            m_value.ident = CSS_VAL_SOURCE_OUT;
            break;
        case CompositeSourceAtop:
            m_value.ident = CSS_VAL_SOURCE_ATOP;
            break;
        case CompositeDestinationOver:
            m_value.ident = CSS_VAL_DESTINATION_OVER;
            break;
        case CompositeDestinationIn:
            m_value.ident = CSS_VAL_DESTINATION_IN;
            break;
        case CompositeDestinationOut:
            m_value.ident = CSS_VAL_DESTINATION_OUT;
            break;
        case CompositeDestinationAtop:
            m_value.ident = CSS_VAL_DESTINATION_ATOP;
            break;
        case CompositeXOR:
            m_value.ident = CSS_VAL_XOR;
            break;
        case CompositePlusDarker:
            m_value.ident = CSS_VAL_PLUS_DARKER;
            break;
        case CompositeHighlight:
            m_value.ident = CSS_VAL_HIGHLIGHT;
            break;
        case CompositePlusLighter:
            m_value.ident = CSS_VAL_PLUS_LIGHTER;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator CompositeOperator() const
{
    switch (m_value.ident) {
        case CSS_VAL_CLEAR:
            return CompositeClear;
        case CSS_VAL_COPY:
            return CompositeCopy;
        case CSS_VAL_SOURCE_OVER:
            return CompositeSourceOver;
        case CSS_VAL_SOURCE_IN:
            return CompositeSourceIn;
        case CSS_VAL_SOURCE_OUT:
            return CompositeSourceOut;
        case CSS_VAL_SOURCE_ATOP:
            return CompositeSourceAtop;
        case CSS_VAL_DESTINATION_OVER:
            return CompositeDestinationOver;
        case CSS_VAL_DESTINATION_IN:
            return CompositeDestinationIn;
        case CSS_VAL_DESTINATION_OUT:
            return CompositeDestinationOut;
        case CSS_VAL_DESTINATION_ATOP:
            return CompositeDestinationAtop;
        case CSS_VAL_XOR:
            return CompositeXOR;
        case CSS_VAL_PLUS_DARKER:
            return CompositePlusDarker;
        case CSS_VAL_HIGHLIGHT:
            return CompositeHighlight;
        case CSS_VAL_PLUS_LIGHTER:
            return CompositePlusLighter;
    }
    ASSERT_NOT_REACHED();
    return CompositeClear;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EAppearance e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case NoAppearance:
            m_value.ident = CSS_VAL_NONE;
            break;
        case CheckboxAppearance:
            m_value.ident = CSS_VAL_CHECKBOX;
            break;
        case RadioAppearance:
            m_value.ident = CSS_VAL_RADIO;
            break;
        case PushButtonAppearance:
            m_value.ident = CSS_VAL_PUSH_BUTTON;
            break;
        case SquareButtonAppearance:
            m_value.ident = CSS_VAL_SQUARE_BUTTON;
            break;
        case ButtonAppearance:
            m_value.ident = CSS_VAL_BUTTON;
            break;
        case ButtonBevelAppearance:
            m_value.ident = CSS_VAL_BUTTON_BEVEL;
            break;
        case ListboxAppearance:
            m_value.ident = CSS_VAL_LISTBOX;
            break;
        case ListItemAppearance:
            m_value.ident = CSS_VAL_LISTITEM;
            break;
        case MediaFullscreenButtonAppearance:
            m_value.ident = CSS_VAL_MEDIA_FULLSCREEN_BUTTON;
            break;
        case MediaPlayButtonAppearance:
            m_value.ident = CSS_VAL_MEDIA_PLAY_BUTTON;
            break;
        case MediaMuteButtonAppearance:
            m_value.ident = CSS_VAL_MEDIA_MUTE_BUTTON;
            break;
        case MediaSeekBackButtonAppearance:
            m_value.ident = CSS_VAL_MEDIA_SEEK_BACK_BUTTON;
            break;
        case MediaSeekForwardButtonAppearance:
            m_value.ident = CSS_VAL_MEDIA_SEEK_FORWARD_BUTTON;
            break;
        case MediaSliderAppearance:
            m_value.ident = CSS_VAL_MEDIA_SLIDER;
            break;
        case MediaSliderThumbAppearance:
            m_value.ident = CSS_VAL_MEDIA_SLIDERTHUMB;
            break;
        case MenulistAppearance:
            m_value.ident = CSS_VAL_MENULIST;
            break;
        case MenulistButtonAppearance:
            m_value.ident = CSS_VAL_MENULIST_BUTTON;
            break;
        case MenulistTextAppearance:
            m_value.ident = CSS_VAL_MENULIST_TEXT;
            break;
        case MenulistTextFieldAppearance:
            m_value.ident = CSS_VAL_MENULIST_TEXTFIELD;
            break;
        case ScrollbarButtonUpAppearance:
            m_value.ident = CSS_VAL_SCROLLBARBUTTON_UP;
            break;
        case ScrollbarButtonDownAppearance:
            m_value.ident = CSS_VAL_SCROLLBARBUTTON_DOWN;
            break;
        case ScrollbarButtonLeftAppearance:
            m_value.ident = CSS_VAL_SCROLLBARBUTTON_LEFT;
            break;
        case ScrollbarButtonRightAppearance:
            m_value.ident = CSS_VAL_SCROLLBARBUTTON_RIGHT;
            break;
        case ScrollbarTrackHorizontalAppearance:
            m_value.ident = CSS_VAL_SCROLLBARTRACK_HORIZONTAL;
            break;
        case ScrollbarTrackVerticalAppearance:
            m_value.ident = CSS_VAL_SCROLLBARTRACK_VERTICAL;
            break;
        case ScrollbarThumbHorizontalAppearance:
            m_value.ident = CSS_VAL_SCROLLBARTHUMB_HORIZONTAL;
            break;
        case ScrollbarThumbVerticalAppearance:
            m_value.ident = CSS_VAL_SCROLLBARTHUMB_VERTICAL;
            break;
        case ScrollbarGripperHorizontalAppearance:
            m_value.ident = CSS_VAL_SCROLLBARGRIPPER_HORIZONTAL;
            break;
        case ScrollbarGripperVerticalAppearance:
            m_value.ident = CSS_VAL_SCROLLBARGRIPPER_VERTICAL;
            break;
        case SliderHorizontalAppearance:
            m_value.ident = CSS_VAL_SLIDER_HORIZONTAL;
            break;
        case SliderVerticalAppearance:
            m_value.ident = CSS_VAL_SLIDER_VERTICAL;
            break;
        case SliderThumbHorizontalAppearance:
            m_value.ident = CSS_VAL_SLIDERTHUMB_HORIZONTAL;
            break;
        case SliderThumbVerticalAppearance:
            m_value.ident = CSS_VAL_SLIDERTHUMB_VERTICAL;
            break;
        case CaretAppearance:
            m_value.ident = CSS_VAL_CARET;
            break;
        case SearchFieldAppearance:
            m_value.ident = CSS_VAL_SEARCHFIELD;
            break;
        case SearchFieldDecorationAppearance:
            m_value.ident = CSS_VAL_SEARCHFIELD_DECORATION;
            break;
        case SearchFieldResultsDecorationAppearance:
            m_value.ident = CSS_VAL_SEARCHFIELD_RESULTS_DECORATION;
            break;
        case SearchFieldResultsButtonAppearance:
            m_value.ident = CSS_VAL_SEARCHFIELD_RESULTS_BUTTON;
            break;
        case SearchFieldCancelButtonAppearance:
            m_value.ident = CSS_VAL_SEARCHFIELD_CANCEL_BUTTON;
            break;
        case TextFieldAppearance:
            m_value.ident = CSS_VAL_TEXTFIELD;
            break;
        case TextAreaAppearance:
            m_value.ident = CSS_VAL_TEXTAREA;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EAppearance() const
{
    if (m_value.ident == CSS_VAL_NONE)
        return NoAppearance;
    else
        return EAppearance(m_value.ident - CSS_VAL_CHECKBOX + 1);
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBackgroundBox e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case BGBORDER:
            m_value.ident = CSS_VAL_BORDER;
            break;
        case BGPADDING:
            m_value.ident = CSS_VAL_PADDING;
            break;
        case BGCONTENT:
            m_value.ident = CSS_VAL_CONTENT;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EBackgroundBox() const
{
    switch (m_value.ident) {
        case CSS_VAL_BORDER:
            return BGBORDER;
        case CSS_VAL_PADDING:
            return BGPADDING;
        case CSS_VAL_CONTENT:
            return BGCONTENT;
    }
    ASSERT_NOT_REACHED();
    return BGBORDER;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBackgroundRepeat e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case REPEAT:
            m_value.ident = CSS_VAL_REPEAT;
            break;
        case REPEAT_X:
            m_value.ident = CSS_VAL_REPEAT_X;
            break;
        case REPEAT_Y:
            m_value.ident = CSS_VAL_REPEAT_Y;
            break;
        case NO_REPEAT:
            m_value.ident = CSS_VAL_NO_REPEAT;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EBackgroundRepeat() const
{
    switch (m_value.ident) {
        case CSS_VAL_REPEAT:
            return REPEAT;
        case CSS_VAL_REPEAT_X:
            return REPEAT_X;
        case CSS_VAL_REPEAT_Y:
            return REPEAT_Y;
        case CSS_VAL_NO_REPEAT:
            return NO_REPEAT;
    }
    ASSERT_NOT_REACHED();
    return REPEAT;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBoxAlignment e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case BSTRETCH:
            m_value.ident = CSS_VAL_STRETCH;
            break;
        case BSTART:
            m_value.ident = CSS_VAL_START;
            break;
        case BCENTER:
            m_value.ident = CSS_VAL_CENTER;
            break;
        case BEND:
            m_value.ident = CSS_VAL_END;
            break;
        case BBASELINE:
            m_value.ident = CSS_VAL_BASELINE;
            break;
        case BJUSTIFY:
            m_value.ident = CSS_VAL_JUSTIFY;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EBoxAlignment() const
{
    switch (m_value.ident) {
        case CSS_VAL_STRETCH:
            return BSTRETCH;
        case CSS_VAL_START:
            return BSTART;
        case CSS_VAL_END:
            return BEND;
        case CSS_VAL_CENTER:
            return BCENTER;
        case CSS_VAL_BASELINE:
            return BBASELINE;
        case CSS_VAL_JUSTIFY:
            return BJUSTIFY;
    }
    ASSERT_NOT_REACHED();
    return BSTRETCH;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBoxDirection e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case BNORMAL:
            m_value.ident = CSS_VAL_NORMAL;
            break;
        case BREVERSE:
            m_value.ident = CSS_VAL_REVERSE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EBoxDirection() const
{
    switch (m_value.ident) {
        case CSS_VAL_NORMAL:
            return BNORMAL;
        case CSS_VAL_REVERSE:
            return BREVERSE;
    }
    ASSERT_NOT_REACHED();
    return BNORMAL;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBoxLines e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case SINGLE:
            m_value.ident = CSS_VAL_SINGLE;
            break;
        case MULTIPLE:
            m_value.ident = CSS_VAL_MULTIPLE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EBoxLines() const
{
    switch (m_value.ident) {
        case CSS_VAL_SINGLE:
            return SINGLE;
        case CSS_VAL_MULTIPLE:
            return MULTIPLE;
    }
    ASSERT_NOT_REACHED();
    return SINGLE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EBoxOrient e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case HORIZONTAL:
            m_value.ident = CSS_VAL_HORIZONTAL;
            break;
        case VERTICAL:
            m_value.ident = CSS_VAL_VERTICAL;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EBoxOrient() const
{
    switch (m_value.ident) {
        case CSS_VAL_HORIZONTAL:
        case CSS_VAL_INLINE_AXIS:
            return HORIZONTAL;
        case CSS_VAL_VERTICAL:
            return VERTICAL;
    }
    ASSERT_NOT_REACHED();
    return HORIZONTAL;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ECaptionSide e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case CAPLEFT:
            m_value.ident = CSS_VAL_LEFT;
            break;
        case CAPRIGHT:
            m_value.ident = CSS_VAL_RIGHT;
            break;
        case CAPTOP:
            m_value.ident = CSS_VAL_TOP;
            break;
        case CAPBOTTOM:
            m_value.ident = CSS_VAL_BOTTOM;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator ECaptionSide() const
{
    switch (m_value.ident) {
        case CSS_VAL_LEFT:
            return CAPLEFT;
        case CSS_VAL_RIGHT:
            return CAPRIGHT;
        case CSS_VAL_TOP:
            return CAPTOP;
        case CSS_VAL_BOTTOM:
            return CAPBOTTOM;
    }
    ASSERT_NOT_REACHED();
    return CAPTOP;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EClear e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case CNONE:
            m_value.ident = CSS_VAL_NONE;
            break;
        case CLEFT:
            m_value.ident = CSS_VAL_LEFT;
            break;
        case CRIGHT:
            m_value.ident = CSS_VAL_RIGHT;
            break;
        case CBOTH:
            m_value.ident = CSS_VAL_BOTH;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EClear() const
{
    switch (m_value.ident) {
        case CSS_VAL_NONE:
            return CNONE;
        case CSS_VAL_LEFT:
            return CLEFT;
        case CSS_VAL_RIGHT:
            return CRIGHT;
        case CSS_VAL_BOTH:
            return CBOTH;
    }
    ASSERT_NOT_REACHED();
    return CNONE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ECursor e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case CURSOR_AUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case CURSOR_CROSS:
            m_value.ident = CSS_VAL_CROSSHAIR;
            break;
        case CURSOR_DEFAULT:
            m_value.ident = CSS_VAL_DEFAULT;
            break;
        case CURSOR_POINTER:
            m_value.ident = CSS_VAL_POINTER;
            break;
        case CURSOR_MOVE:
            m_value.ident = CSS_VAL_MOVE;
            break;
        case CURSOR_CELL:
            m_value.ident = CSS_VAL_CELL;
            break;
        case CURSOR_VERTICAL_TEXT:
            m_value.ident = CSS_VAL_VERTICAL_TEXT;
            break;
        case CURSOR_CONTEXT_MENU:
            m_value.ident = CSS_VAL_CONTEXT_MENU;
            break;
        case CURSOR_ALIAS:
            m_value.ident = CSS_VAL_ALIAS;
            break;
        case CURSOR_COPY:
            m_value.ident = CSS_VAL_COPY;
            break;
        case CURSOR_NONE:
            m_value.ident = CSS_VAL_NONE;
            break;
        case CURSOR_PROGRESS:
            m_value.ident = CSS_VAL_PROGRESS;
            break;
        case CURSOR_NO_DROP:
            m_value.ident = CSS_VAL_NO_DROP;
            break;
        case CURSOR_NOT_ALLOWED:
            m_value.ident = CSS_VAL_NOT_ALLOWED;
            break;
        case CURSOR_WEBKIT_ZOOM_IN:
            m_value.ident = CSS_VAL__WEBKIT_ZOOM_IN;
            break;
        case CURSOR_WEBKIT_ZOOM_OUT:
            m_value.ident = CSS_VAL__WEBKIT_ZOOM_OUT;
            break;
        case CURSOR_E_RESIZE:
            m_value.ident = CSS_VAL_E_RESIZE;
            break;
        case CURSOR_NE_RESIZE:
            m_value.ident = CSS_VAL_NE_RESIZE;
            break;
        case CURSOR_NW_RESIZE:
            m_value.ident = CSS_VAL_NW_RESIZE;
            break;
        case CURSOR_N_RESIZE:
            m_value.ident = CSS_VAL_N_RESIZE;
            break;
        case CURSOR_SE_RESIZE:
            m_value.ident = CSS_VAL_SE_RESIZE;
            break;
        case CURSOR_SW_RESIZE:
            m_value.ident = CSS_VAL_SW_RESIZE;
            break;
        case CURSOR_S_RESIZE:
            m_value.ident = CSS_VAL_S_RESIZE;
            break;
        case CURSOR_W_RESIZE:
            m_value.ident = CSS_VAL_W_RESIZE;
            break;
        case CURSOR_EW_RESIZE:
            m_value.ident = CSS_VAL_EW_RESIZE;
            break;
        case CURSOR_NS_RESIZE:
            m_value.ident = CSS_VAL_NS_RESIZE;
            break;
        case CURSOR_NESW_RESIZE:
            m_value.ident = CSS_VAL_NESW_RESIZE;
            break;
        case CURSOR_NWSE_RESIZE:
            m_value.ident = CSS_VAL_NWSE_RESIZE;
            break;
        case CURSOR_COL_RESIZE:
            m_value.ident = CSS_VAL_COL_RESIZE;
            break;
        case CURSOR_ROW_RESIZE:
            m_value.ident = CSS_VAL_ROW_RESIZE;
            break;
        case CURSOR_TEXT:
            m_value.ident = CSS_VAL_TEXT;
            break;
        case CURSOR_WAIT:
            m_value.ident = CSS_VAL_WAIT;
            break;
        case CURSOR_HELP:
            m_value.ident = CSS_VAL_HELP;
            break;
        case CURSOR_ALL_SCROLL:
            m_value.ident = CSS_VAL_ALL_SCROLL;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator ECursor() const
{
    if (m_value.ident == CSS_VAL_COPY)
        return CURSOR_COPY;
    else if (m_value.ident == CSS_VAL_NONE)
        return CURSOR_NONE;
    else
        return (ECursor)(m_value.ident - CSS_VAL_AUTO);
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EDisplay e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case INLINE:
            m_value.ident = CSS_VAL_INLINE;
            break;
        case BLOCK:
            m_value.ident = CSS_VAL_BLOCK;
            break;
        case LIST_ITEM:
            m_value.ident = CSS_VAL_LIST_ITEM;
            break;
        case RUN_IN:
            m_value.ident = CSS_VAL_RUN_IN;
            break;
        case COMPACT:
            m_value.ident = CSS_VAL_COMPACT;
            break;
        case INLINE_BLOCK:
            m_value.ident = CSS_VAL_INLINE_BLOCK;
            break;
        case TABLE:
            m_value.ident = CSS_VAL_TABLE;
            break;
        case INLINE_TABLE:
            m_value.ident = CSS_VAL_INLINE_TABLE;
            break;
        case TABLE_ROW_GROUP:
            m_value.ident = CSS_VAL_TABLE_ROW_GROUP;
            break;
        case TABLE_HEADER_GROUP:
            m_value.ident = CSS_VAL_TABLE_HEADER_GROUP;
            break;
        case TABLE_FOOTER_GROUP:
            m_value.ident = CSS_VAL_TABLE_FOOTER_GROUP;
            break;
        case TABLE_ROW:
            m_value.ident = CSS_VAL_TABLE_ROW;
            break;
        case TABLE_COLUMN_GROUP:
            m_value.ident = CSS_VAL_TABLE_COLUMN_GROUP;
            break;
        case TABLE_COLUMN:
            m_value.ident = CSS_VAL_TABLE_COLUMN;
            break;
        case TABLE_CELL:
            m_value.ident = CSS_VAL_TABLE_CELL;
            break;
        case TABLE_CAPTION:
            m_value.ident = CSS_VAL_TABLE_CAPTION;
            break;
        case BOX:
            m_value.ident = CSS_VAL__WEBKIT_BOX;
            break;
        case INLINE_BOX:
            m_value.ident = CSS_VAL__WEBKIT_INLINE_BOX;
            break;
        case NONE:
            m_value.ident = CSS_VAL_NONE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EDisplay() const
{
    if (m_value.ident == CSS_VAL_NONE)
        return NONE;
    else
        return EDisplay(m_value.ident - CSS_VAL_INLINE);
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EEmptyCell e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case SHOW:
            m_value.ident = CSS_VAL_SHOW;
            break;
        case HIDE:
            m_value.ident = CSS_VAL_HIDE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EEmptyCell() const
{
    switch (m_value.ident) {
        case CSS_VAL_SHOW:
            return SHOW;
        case CSS_VAL_HIDE:
            return HIDE;
    }
    ASSERT_NOT_REACHED();
    return SHOW;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EFloat e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case FNONE:
            m_value.ident = CSS_VAL_NONE;
            break;
        case FLEFT:
            m_value.ident = CSS_VAL_LEFT;
            break;
        case FRIGHT:
            m_value.ident = CSS_VAL_RIGHT;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EFloat() const
{
    switch (m_value.ident) {
        case CSS_VAL_LEFT:
            return FLEFT;
        case CSS_VAL_RIGHT:
            return FRIGHT;
        case CSS_VAL_NONE:
        case CSS_VAL_CENTER:  // Non-standard CSS value
            return FNONE;
    }
    ASSERT_NOT_REACHED();
    return FNONE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EKHTMLLineBreak e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case LBNORMAL:
            m_value.ident = CSS_VAL_NORMAL;
            break;
        case AFTER_WHITE_SPACE:
            m_value.ident = CSS_VAL_AFTER_WHITE_SPACE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EKHTMLLineBreak() const
{
    switch (m_value.ident) {
        case CSS_VAL_AFTER_WHITE_SPACE:
            return AFTER_WHITE_SPACE;
        case CSS_VAL_NORMAL:
            return LBNORMAL;
    }
    ASSERT_NOT_REACHED();
    return LBNORMAL;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EListStylePosition e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case OUTSIDE:
            m_value.ident = CSS_VAL_OUTSIDE;
            break;
        case INSIDE:
            m_value.ident = CSS_VAL_INSIDE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EListStylePosition() const
{
    return (EListStylePosition)(m_value.ident - CSS_VAL_OUTSIDE);
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EListStyleType e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case LNONE:
            m_value.ident = CSS_VAL_NONE;
            break;
        case DISC:
            m_value.ident = CSS_VAL_DISC;
            break;
        case CIRCLE:
            m_value.ident = CSS_VAL_CIRCLE;
            break;
        case SQUARE:
            m_value.ident = CSS_VAL_SQUARE;
            break;
        case LDECIMAL:
            m_value.ident = CSS_VAL_DECIMAL;
            break;
        case DECIMAL_LEADING_ZERO:
            m_value.ident = CSS_VAL_DECIMAL_LEADING_ZERO;
            break;
        case LOWER_ROMAN:
            m_value.ident = CSS_VAL_LOWER_ROMAN;
            break;
        case UPPER_ROMAN:
            m_value.ident = CSS_VAL_UPPER_ROMAN;
            break;
        case LOWER_GREEK:
            m_value.ident = CSS_VAL_LOWER_GREEK;
            break;
        case LOWER_ALPHA:
            m_value.ident = CSS_VAL_LOWER_ALPHA;
            break;
        case LOWER_LATIN:
            m_value.ident = CSS_VAL_LOWER_LATIN;
            break;
        case UPPER_ALPHA:
            m_value.ident = CSS_VAL_UPPER_ALPHA;
            break;
        case UPPER_LATIN:
            m_value.ident = CSS_VAL_UPPER_LATIN;
            break;
        case HEBREW:
            m_value.ident = CSS_VAL_HEBREW;
            break;
        case ARMENIAN:
            m_value.ident = CSS_VAL_ARMENIAN;
            break;
        case GEORGIAN:
            m_value.ident = CSS_VAL_GEORGIAN;
            break;
        case CJK_IDEOGRAPHIC:
            m_value.ident = CSS_VAL_CJK_IDEOGRAPHIC;
            break;
        case HIRAGANA:
            m_value.ident = CSS_VAL_HIRAGANA;
            break;
        case KATAKANA:
            m_value.ident = CSS_VAL_KATAKANA;
            break;
        case HIRAGANA_IROHA:
            m_value.ident = CSS_VAL_HIRAGANA_IROHA;
            break;
        case KATAKANA_IROHA:
            m_value.ident = CSS_VAL_KATAKANA_IROHA;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EListStyleType() const
{
    switch (m_value.ident) {
        case CSS_VAL_NONE:
            return LNONE;
        default:
            return EListStyleType(m_value.ident - CSS_VAL_DISC);
    }
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EMarginCollapse e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case MCOLLAPSE:
            m_value.ident = CSS_VAL_COLLAPSE;
            break;
        case MSEPARATE:
            m_value.ident = CSS_VAL_SEPARATE;
            break;
        case MDISCARD:
            m_value.ident = CSS_VAL_DISCARD;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EMarginCollapse() const
{
    switch (m_value.ident) {
        case CSS_VAL_COLLAPSE:
            return MCOLLAPSE;
        case CSS_VAL_SEPARATE:
            return MSEPARATE;
        case CSS_VAL_DISCARD:
            return MDISCARD;
    }
    ASSERT_NOT_REACHED();
    return MCOLLAPSE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EMarqueeBehavior e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case MNONE:
            m_value.ident = CSS_VAL_NONE;
            break;
        case MSCROLL:
            m_value.ident = CSS_VAL_SCROLL;
            break;
        case MSLIDE:
            m_value.ident = CSS_VAL_SLIDE;
            break;
        case MALTERNATE:
            m_value.ident = CSS_VAL_ALTERNATE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EMarqueeBehavior() const
{
    switch (m_value.ident) {
        case CSS_VAL_NONE:
            return MNONE;
        case CSS_VAL_SCROLL:
            return MSCROLL;
        case CSS_VAL_SLIDE:
            return MSLIDE;
        case CSS_VAL_ALTERNATE:
            return MALTERNATE;
    }
    ASSERT_NOT_REACHED();
    return MNONE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EMarqueeDirection e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case MFORWARD:
            m_value.ident = CSS_VAL_FORWARDS;
            break;
        case MBACKWARD:
            m_value.ident = CSS_VAL_BACKWARDS;
            break;
        case MAUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case MUP:
            m_value.ident = CSS_VAL_UP;
            break;
        case MDOWN:
            m_value.ident = CSS_VAL_DOWN;
            break;
        case MLEFT:
            m_value.ident = CSS_VAL_LEFT;
            break;
        case MRIGHT:
            m_value.ident = CSS_VAL_RIGHT;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EMarqueeDirection() const
{
    switch (m_value.ident) {
        case CSS_VAL_FORWARDS:
            return MFORWARD;
        case CSS_VAL_BACKWARDS:
            return MBACKWARD;
        case CSS_VAL_AUTO:
            return MAUTO;
        case CSS_VAL_AHEAD:
        case CSS_VAL_UP: // We don't support vertical languages, so AHEAD just maps to UP.
            return MUP;
        case CSS_VAL_REVERSE:
        case CSS_VAL_DOWN: // REVERSE just maps to DOWN, since we don't do vertical text.
            return MDOWN;
        case CSS_VAL_LEFT:
            return MLEFT;
        case CSS_VAL_RIGHT:
            return MRIGHT;
    }
    ASSERT_NOT_REACHED();
    return MAUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EMatchNearestMailBlockquoteColor e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case BCNORMAL:
            m_value.ident = CSS_VAL_NORMAL;
            break;
        case MATCH:
            m_value.ident = CSS_VAL_MATCH;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EMatchNearestMailBlockquoteColor() const
{
    switch (m_value.ident) {
        case CSS_VAL_NORMAL:
            return BCNORMAL;
        case CSS_VAL_MATCH:
            return MATCH;
    }
    ASSERT_NOT_REACHED();
    return BCNORMAL;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ENBSPMode e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case NBNORMAL:
            m_value.ident = CSS_VAL_NORMAL;
            break;
        case SPACE:
            m_value.ident = CSS_VAL_SPACE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator ENBSPMode() const
{
    switch (m_value.ident) {
        case CSS_VAL_SPACE:
            return SPACE;
        case CSS_VAL_NORMAL:
            return NBNORMAL;
    }
    ASSERT_NOT_REACHED();
    return NBNORMAL;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EOverflow e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case OVISIBLE:
            m_value.ident = CSS_VAL_VISIBLE;
            break;
        case OHIDDEN:
            m_value.ident = CSS_VAL_HIDDEN;
            break;
        case OSCROLL:
            m_value.ident = CSS_VAL_SCROLL;
            break;
        case OAUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case OMARQUEE:
            m_value.ident = CSS_VAL__WEBKIT_MARQUEE;
            break;
        case OOVERLAY:
            m_value.ident = CSS_VAL_OVERLAY;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EOverflow() const
{
    switch (m_value.ident) {
        case CSS_VAL_VISIBLE:
            return OVISIBLE;
        case CSS_VAL_HIDDEN:
            return OHIDDEN;
        case CSS_VAL_SCROLL:
            return OSCROLL;
        case CSS_VAL_AUTO:
            return OAUTO;
        case CSS_VAL__WEBKIT_MARQUEE:
            return OMARQUEE;
        case CSS_VAL_OVERLAY:
            return OOVERLAY;
    }
    ASSERT_NOT_REACHED();
    return OVISIBLE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EPageBreak e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case PBAUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case PBALWAYS:
            m_value.ident = CSS_VAL_ALWAYS;
            break;
        case PBAVOID:
            m_value.ident = CSS_VAL_AVOID;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EPageBreak() const
{
    switch (m_value.ident) {
        case CSS_VAL_AUTO:
            return PBAUTO;
        case CSS_VAL_LEFT:
        case CSS_VAL_RIGHT:
        case CSS_VAL_ALWAYS:
            return PBALWAYS; // CSS2.1: "Conforming user agents may map left/right to always."
        case CSS_VAL_AVOID:
            return PBAVOID;
    }
    ASSERT_NOT_REACHED();
    return PBAUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EPosition e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case StaticPosition:
            m_value.ident = CSS_VAL_STATIC;
            break;
        case RelativePosition:
            m_value.ident = CSS_VAL_RELATIVE;
            break;
        case AbsolutePosition:
            m_value.ident = CSS_VAL_ABSOLUTE;
            break;
        case FixedPosition:
            m_value.ident = CSS_VAL_FIXED;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EPosition() const
{
    switch (m_value.ident) {
        case CSS_VAL_STATIC:
            return StaticPosition;
        case CSS_VAL_RELATIVE:
            return RelativePosition;
        case CSS_VAL_ABSOLUTE:
            return AbsolutePosition;
        case CSS_VAL_FIXED:
            return FixedPosition;
    }
    ASSERT_NOT_REACHED();
    return StaticPosition;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EResize e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case RESIZE_BOTH:
            m_value.ident = CSS_VAL_BOTH;
            break;
        case RESIZE_HORIZONTAL:
            m_value.ident = CSS_VAL_HORIZONTAL;
            break;
        case RESIZE_VERTICAL:
            m_value.ident = CSS_VAL_VERTICAL;
            break;
        case RESIZE_NONE:
            m_value.ident = CSS_VAL_NONE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EResize() const
{
    switch (m_value.ident) {
        case CSS_VAL_BOTH:
            return RESIZE_BOTH;
        case CSS_VAL_HORIZONTAL:
            return RESIZE_HORIZONTAL;
        case CSS_VAL_VERTICAL:
            return RESIZE_VERTICAL;
        case CSS_VAL_AUTO:
            ASSERT_NOT_REACHED(); // Depends on settings, thus should be handled by the caller.
            return RESIZE_NONE;
        case CSS_VAL_NONE:
            return RESIZE_NONE;
    }
    ASSERT_NOT_REACHED();
    return RESIZE_NONE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ETableLayout e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case TAUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case TFIXED:
            m_value.ident = CSS_VAL_FIXED;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator ETableLayout() const
{
    switch (m_value.ident) {
        case CSS_VAL_FIXED:
            return TFIXED;
        case CSS_VAL_AUTO:
            return TAUTO;
    }
    ASSERT_NOT_REACHED();
    return TAUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ETextAlign e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case TAAUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case LEFT:
            m_value.ident = CSS_VAL_LEFT;
            break;
        case RIGHT:
            m_value.ident = CSS_VAL_RIGHT;
            break;
        case CENTER:
            m_value.ident = CSS_VAL_CENTER;
            break;
        case JUSTIFY:
            m_value.ident = CSS_VAL_JUSTIFY;
            break;
        case WEBKIT_LEFT:
            m_value.ident = CSS_VAL__WEBKIT_LEFT;
            break;
        case WEBKIT_RIGHT:
            m_value.ident = CSS_VAL__WEBKIT_RIGHT;
            break;
        case WEBKIT_CENTER:
            m_value.ident = CSS_VAL__WEBKIT_CENTER;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator ETextAlign() const
{
    switch (m_value.ident) {
        case CSS_VAL_START:
        case CSS_VAL_END:
            ASSERT_NOT_REACHED(); // Depends on direction, thus should be handled by the caller.
            return LEFT;
        default:
            return (ETextAlign)(m_value.ident - CSS_VAL__WEBKIT_AUTO);
    }
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ETextSecurity e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case TSNONE:
            m_value.ident = CSS_VAL_NONE;
            break;
        case TSDISC:
            m_value.ident = CSS_VAL_DISC;
            break;
        case TSCIRCLE:
            m_value.ident = CSS_VAL_CIRCLE;
            break;
        case TSSQUARE:
            m_value.ident = CSS_VAL_SQUARE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator ETextSecurity() const
{
    switch (m_value.ident) {
        case CSS_VAL_NONE:
            return TSNONE;
        case CSS_VAL_DISC:
            return TSDISC;
        case CSS_VAL_CIRCLE:
            return TSCIRCLE;
        case CSS_VAL_SQUARE:
            return TSSQUARE;
    }
    ASSERT_NOT_REACHED();
    return TSNONE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ETextTransform e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case CAPITALIZE:
            m_value.ident = CSS_VAL_CAPITALIZE;
            break;
        case UPPERCASE:
            m_value.ident = CSS_VAL_UPPERCASE;
            break;
        case LOWERCASE:
            m_value.ident = CSS_VAL_LOWERCASE;
            break;
        case TTNONE:
            m_value.ident = CSS_VAL_NONE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator ETextTransform() const
{
    switch (m_value.ident) {
        case CSS_VAL_CAPITALIZE:
            return CAPITALIZE;
        case CSS_VAL_UPPERCASE:
            return UPPERCASE;
        case CSS_VAL_LOWERCASE:
            return LOWERCASE;
        case CSS_VAL_NONE:
            return TTNONE;
    }
    ASSERT_NOT_REACHED();
    return TTNONE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EUnicodeBidi e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case UBNormal:
            m_value.ident = CSS_VAL_NORMAL;
            break;
        case Embed:
            m_value.ident = CSS_VAL_EMBED;
            break;
        case Override:
            m_value.ident = CSS_VAL_BIDI_OVERRIDE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EUnicodeBidi() const
{
    switch (m_value.ident) {
        case CSS_VAL_NORMAL:
            return UBNormal; 
        case CSS_VAL_EMBED:
            return Embed; 
        case CSS_VAL_BIDI_OVERRIDE:
            return Override;
    }
    ASSERT_NOT_REACHED();
    return UBNormal;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EUserDrag e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case DRAG_AUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case DRAG_NONE:
            m_value.ident = CSS_VAL_NONE;
            break;
        case DRAG_ELEMENT:
            m_value.ident = CSS_VAL_ELEMENT;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EUserDrag() const
{
    switch (m_value.ident) {
        case CSS_VAL_AUTO:
            return DRAG_AUTO;
        case CSS_VAL_NONE:
            return DRAG_NONE;
        case CSS_VAL_ELEMENT:
            return DRAG_ELEMENT;
    }
    ASSERT_NOT_REACHED();
    return DRAG_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EUserModify e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case READ_ONLY:
            m_value.ident = CSS_VAL_READ_ONLY;
            break;
        case READ_WRITE:
            m_value.ident = CSS_VAL_READ_WRITE;
            break;
        case READ_WRITE_PLAINTEXT_ONLY:
            m_value.ident = CSS_VAL_READ_WRITE_PLAINTEXT_ONLY;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EUserModify() const
{
    return EUserModify(m_value.ident - CSS_VAL_READ_ONLY);
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EUserSelect e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case SELECT_NONE:
            m_value.ident = CSS_VAL_NONE;
            break;
        case SELECT_TEXT:
            m_value.ident = CSS_VAL_TEXT;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EUserSelect() const
{
    switch (m_value.ident) {
        case CSS_VAL_AUTO:
            return SELECT_TEXT;
        case CSS_VAL_NONE:
            return SELECT_NONE;
        case CSS_VAL_TEXT:
            return SELECT_TEXT;
    }
    ASSERT_NOT_REACHED();
    return SELECT_TEXT;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EVisibility e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case VISIBLE:
            m_value.ident = CSS_VAL_VISIBLE;
            break;
        case HIDDEN:
            m_value.ident = CSS_VAL_HIDDEN;
            break;
        case COLLAPSE:
            m_value.ident = CSS_VAL_COLLAPSE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EVisibility() const
{
    switch (m_value.ident) {
        case CSS_VAL_HIDDEN:
            return HIDDEN;
        case CSS_VAL_VISIBLE:
            return VISIBLE;
        case CSS_VAL_COLLAPSE:
            return COLLAPSE;
    }
    ASSERT_NOT_REACHED();
    return VISIBLE;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EWhiteSpace e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case NORMAL:
            m_value.ident = CSS_VAL_NORMAL;
            break;
        case PRE:
            m_value.ident = CSS_VAL_PRE;
            break;
        case PRE_WRAP:
            m_value.ident = CSS_VAL_PRE_WRAP;
            break;
        case PRE_LINE:
            m_value.ident = CSS_VAL_PRE_LINE;
            break;
        case NOWRAP:
            m_value.ident = CSS_VAL_NOWRAP;
            break;
        case KHTML_NOWRAP:
            m_value.ident = CSS_VAL__WEBKIT_NOWRAP;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EWhiteSpace() const
{
    switch (m_value.ident) {
        case CSS_VAL__WEBKIT_NOWRAP:
            return KHTML_NOWRAP;
        case CSS_VAL_NOWRAP:
            return NOWRAP;
        case CSS_VAL_PRE:
            return PRE;
        case CSS_VAL_PRE_WRAP:
            return PRE_WRAP;
        case CSS_VAL_PRE_LINE:
            return PRE_LINE;
        case CSS_VAL_NORMAL:
            return NORMAL;
    }
    ASSERT_NOT_REACHED();
    return NORMAL;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EWordBreak e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case NormalWordBreak:
            m_value.ident = CSS_VAL_NORMAL;
            break;
        case BreakAllWordBreak:
            m_value.ident = CSS_VAL_BREAK_ALL;
            break;
        case BreakWordBreak:
            m_value.ident = CSS_VAL_BREAK_WORD;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EWordBreak() const
{
    switch (m_value.ident) {
        case CSS_VAL_BREAK_ALL:
            return BreakAllWordBreak;
        case CSS_VAL_BREAK_WORD:
            return BreakWordBreak;
        case CSS_VAL_NORMAL:
            return NormalWordBreak;
    }
    ASSERT_NOT_REACHED();
    return NormalWordBreak;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EWordWrap e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case NormalWordWrap:
            m_value.ident = CSS_VAL_NORMAL;
            break;
        case BreakWordWrap:
            m_value.ident = CSS_VAL_BREAK_WORD;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EWordWrap() const
{
    switch (m_value.ident) {
        case CSS_VAL_BREAK_WORD:
            return BreakWordWrap;
        case CSS_VAL_NORMAL:
            return NormalWordWrap;
    }
    ASSERT_NOT_REACHED();
    return NormalWordWrap;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(TextDirection e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case LTR:
            m_value.ident = CSS_VAL_LTR;
            break;
        case RTL:
            m_value.ident = CSS_VAL_RTL;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator TextDirection() const
{
    switch (m_value.ident) {
        case CSS_VAL_LTR:
            return LTR;
        case CSS_VAL_RTL:
            return RTL;
    }
    ASSERT_NOT_REACHED();
    return LTR;
}

#if ENABLE(SVG)

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(LineCap e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case ButtCap:
            m_value.ident = CSS_VAL_BUTT;
            break;
        case RoundCap:
            m_value.ident = CSS_VAL_ROUND;
            break;
        case SquareCap:
            m_value.ident = CSS_VAL_SQUARE;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator LineCap() const
{
    switch (m_value.ident) {
        case CSS_VAL_BUTT:
            return ButtCap;
        case CSS_VAL_ROUND:
            return RoundCap;
        case CSS_VAL_SQUARE:
            return SquareCap;
    }
    ASSERT_NOT_REACHED();
    return ButtCap;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(LineJoin e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case MiterJoin:
            m_value.ident = CSS_VAL_MITER;
            break;
        case RoundJoin:
            m_value.ident = CSS_VAL_ROUND;
            break;
        case BevelJoin:
            m_value.ident = CSS_VAL_BEVEL;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator LineJoin() const
{
    switch (m_value.ident) {
        case CSS_VAL_MITER:
            return MiterJoin;
        case CSS_VAL_ROUND:
            return RoundJoin;
        case CSS_VAL_BEVEL:
            return BevelJoin;
    }
    ASSERT_NOT_REACHED();
    return MiterJoin;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(WindRule e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case RULE_NONZERO:
            m_value.ident = CSS_VAL_NONZERO;
            break;
        case RULE_EVENODD:
            m_value.ident = CSS_VAL_EVENODD;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator WindRule() const
{
    switch (m_value.ident) {
        case CSS_VAL_NONZERO:
            return RULE_NONZERO;
        case CSS_VAL_EVENODD:
            return RULE_EVENODD;
    }
    ASSERT_NOT_REACHED();
    return RULE_NONZERO;
}


template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EAlignmentBaseline e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case AB_AUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case AB_BASELINE:
            m_value.ident = CSS_VAL_BASELINE;
            break;
        case AB_BEFORE_EDGE:
            m_value.ident = CSS_VAL_BEFORE_EDGE;
            break;
        case AB_TEXT_BEFORE_EDGE:
            m_value.ident = CSS_VAL_TEXT_BEFORE_EDGE;
            break;
        case AB_MIDDLE:
            m_value.ident = CSS_VAL_MIDDLE;
            break;
        case AB_CENTRAL:
            m_value.ident = CSS_VAL_CENTRAL;
            break;
        case AB_AFTER_EDGE:
            m_value.ident = CSS_VAL_AFTER_EDGE;
            break;
        case AB_TEXT_AFTER_EDGE:
            m_value.ident = CSS_VAL_TEXT_AFTER_EDGE;
            break;
        case AB_IDEOGRAPHIC:
            m_value.ident = CSS_VAL_IDEOGRAPHIC;
            break;
        case AB_ALPHABETIC:
            m_value.ident = CSS_VAL_ALPHABETIC;
            break;
        case AB_HANGING:
            m_value.ident = CSS_VAL_HANGING;
            break;
        case AB_MATHEMATICAL:
            m_value.ident = CSS_VAL_MATHEMATICAL;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EAlignmentBaseline() const
{
    switch (m_value.ident) {
        case CSS_VAL_AUTO:
            return AB_AUTO;
        case CSS_VAL_BASELINE:
            return AB_BASELINE;
        case CSS_VAL_BEFORE_EDGE:
            return AB_BEFORE_EDGE;
        case CSS_VAL_TEXT_BEFORE_EDGE:
            return AB_TEXT_BEFORE_EDGE;
        case CSS_VAL_MIDDLE:
            return AB_MIDDLE;
        case CSS_VAL_CENTRAL:
            return AB_CENTRAL;
        case CSS_VAL_AFTER_EDGE:
            return AB_AFTER_EDGE;
        case CSS_VAL_TEXT_AFTER_EDGE:
            return AB_TEXT_AFTER_EDGE;
        case CSS_VAL_IDEOGRAPHIC:
            return AB_IDEOGRAPHIC;
        case CSS_VAL_ALPHABETIC:
            return AB_ALPHABETIC;
        case CSS_VAL_HANGING:
            return AB_HANGING;
        case CSS_VAL_MATHEMATICAL:
            return AB_MATHEMATICAL;
    }
    ASSERT_NOT_REACHED();
    return AB_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EColorInterpolation e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case CI_AUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case CI_SRGB:
            m_value.ident = CSS_VAL_SRGB;
            break;
        case CI_LINEARRGB:
            m_value.ident = CSS_VAL_LINEARRGB;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EColorInterpolation() const
{
    switch (m_value.ident) {
        case CSS_VAL_SRGB:
            return CI_SRGB;
        case CSS_VAL_LINEARRGB:
            return CI_LINEARRGB;
        case CSS_VAL_AUTO:
            return CI_AUTO;
    }
    ASSERT_NOT_REACHED();
    return CI_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EColorRendering e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case CR_AUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case CR_OPTIMIZESPEED:
            m_value.ident = CSS_VAL_OPTIMIZESPEED;
            break;
        case CR_OPTIMIZEQUALITY:
            m_value.ident = CSS_VAL_OPTIMIZEQUALITY;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EColorRendering() const
{
    switch (m_value.ident) {
        case CSS_VAL_OPTIMIZESPEED:
            return CR_OPTIMIZESPEED;
        case CSS_VAL_OPTIMIZEQUALITY:
            return CR_OPTIMIZEQUALITY;
        case CSS_VAL_AUTO:
            return CR_AUTO;
    }
    ASSERT_NOT_REACHED();
    return CR_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EDominantBaseline e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case DB_AUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case DB_USE_SCRIPT:
            m_value.ident = CSS_VAL_USE_SCRIPT;
            break;
        case DB_NO_CHANGE:
            m_value.ident = CSS_VAL_NO_CHANGE;
            break;
        case DB_RESET_SIZE:
            m_value.ident = CSS_VAL_RESET_SIZE;
            break;
        case DB_CENTRAL:
            m_value.ident = CSS_VAL_CENTRAL;
            break;
        case DB_MIDDLE:
            m_value.ident = CSS_VAL_MIDDLE;
            break;
        case DB_TEXT_BEFORE_EDGE:
            m_value.ident = CSS_VAL_TEXT_BEFORE_EDGE;
            break;
        case DB_TEXT_AFTER_EDGE:
            m_value.ident = CSS_VAL_TEXT_AFTER_EDGE;
            break;
        case DB_IDEOGRAPHIC:
            m_value.ident = CSS_VAL_IDEOGRAPHIC;
            break;
        case DB_ALPHABETIC:
            m_value.ident = CSS_VAL_ALPHABETIC;
            break;
        case DB_HANGING:
            m_value.ident = CSS_VAL_HANGING;
            break;
        case DB_MATHEMATICAL:
            m_value.ident = CSS_VAL_MATHEMATICAL;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EDominantBaseline() const
{
    switch (m_value.ident) {
        case CSS_VAL_AUTO:
            return DB_AUTO;
        case CSS_VAL_USE_SCRIPT:
            return DB_USE_SCRIPT;
        case CSS_VAL_NO_CHANGE:
            return DB_NO_CHANGE;
        case CSS_VAL_RESET_SIZE:
            return DB_RESET_SIZE;
        case CSS_VAL_IDEOGRAPHIC:
            return DB_IDEOGRAPHIC;
        case CSS_VAL_ALPHABETIC:
            return DB_ALPHABETIC;
        case CSS_VAL_HANGING:
            return DB_HANGING;
        case CSS_VAL_MATHEMATICAL:
            return DB_MATHEMATICAL;
        case CSS_VAL_CENTRAL:
            return DB_CENTRAL;
        case CSS_VAL_MIDDLE:
            return DB_MIDDLE;
        case CSS_VAL_TEXT_AFTER_EDGE:
            return DB_TEXT_AFTER_EDGE;
        case CSS_VAL_TEXT_BEFORE_EDGE:
            return DB_TEXT_BEFORE_EDGE;
    }
    ASSERT_NOT_REACHED();
    return DB_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EImageRendering e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case IR_AUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case IR_OPTIMIZESPEED:
            m_value.ident = CSS_VAL_OPTIMIZESPEED;
            break;
        case IR_OPTIMIZEQUALITY:
            m_value.ident = CSS_VAL_OPTIMIZEQUALITY;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EImageRendering() const
{
    switch (m_value.ident) {
        case CSS_VAL_AUTO:
            return IR_AUTO;
        case CSS_VAL_OPTIMIZESPEED:
            return IR_OPTIMIZESPEED;
        case CSS_VAL_OPTIMIZEQUALITY:
            return IR_OPTIMIZEQUALITY;
    }
    ASSERT_NOT_REACHED();
    return IR_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EPointerEvents e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case PE_NONE:
            m_value.ident = CSS_VAL_NONE;
            break;
        case PE_STROKE:
            m_value.ident = CSS_VAL_STROKE;
            break;
        case PE_FILL:
            m_value.ident = CSS_VAL_FILL;
            break;
        case PE_PAINTED:
            m_value.ident = CSS_VAL_PAINTED;
            break;
        case PE_VISIBLE:
            m_value.ident = CSS_VAL_VISIBLE;
            break;
        case PE_VISIBLE_STROKE:
            m_value.ident = CSS_VAL_VISIBLESTROKE;
            break;
        case PE_VISIBLE_FILL:
            m_value.ident = CSS_VAL_VISIBLEFILL;
            break;
        case PE_VISIBLE_PAINTED:
            m_value.ident = CSS_VAL_VISIBLEPAINTED;
            break;
        case PE_ALL:
            m_value.ident = CSS_VAL_ALL;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EPointerEvents() const
{
    switch (m_value.ident) {
        case CSS_VAL_ALL:
            return PE_ALL;
        case CSS_VAL_NONE:
            return PE_NONE;
        case CSS_VAL_VISIBLEPAINTED:
            return PE_VISIBLE_PAINTED;
        case CSS_VAL_VISIBLEFILL:
            return PE_VISIBLE_FILL;
        case CSS_VAL_VISIBLESTROKE:
            return PE_VISIBLE_STROKE;
        case CSS_VAL_VISIBLE:
            return PE_VISIBLE;
        case CSS_VAL_PAINTED:
            return PE_PAINTED;
        case CSS_VAL_FILL:
            return PE_FILL;
        case CSS_VAL_STROKE:
            return PE_STROKE;
    }
    ASSERT_NOT_REACHED();
    return PE_ALL;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EShapeRendering e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case IR_AUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case IR_OPTIMIZESPEED:
            m_value.ident = CSS_VAL_OPTIMIZESPEED;
            break;
        case SR_CRISPEDGES:
            m_value.ident = CSS_VAL_CRISPEDGES;
            break;
        case SR_GEOMETRICPRECISION:
            m_value.ident = CSS_VAL_GEOMETRICPRECISION;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EShapeRendering() const
{
    switch (m_value.ident) {
        case CSS_VAL_AUTO:
            return SR_AUTO;
        case CSS_VAL_OPTIMIZESPEED:
            return SR_OPTIMIZESPEED;
        case CSS_VAL_CRISPEDGES:
            return SR_CRISPEDGES;
        case CSS_VAL_GEOMETRICPRECISION:
            return SR_GEOMETRICPRECISION;
    }
    ASSERT_NOT_REACHED();
    return SR_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ETextAnchor e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case TA_START:
            m_value.ident = CSS_VAL_START;
            break;
        case TA_MIDDLE:
            m_value.ident = CSS_VAL_MIDDLE;
            break;
        case TA_END:
            m_value.ident = CSS_VAL_END;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator ETextAnchor() const
{
    switch (m_value.ident) {
        case CSS_VAL_START:
            return TA_START;
        case CSS_VAL_MIDDLE:
            return TA_MIDDLE;
        case CSS_VAL_END:
            return TA_END;
    }
    ASSERT_NOT_REACHED();
    return TA_START;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(ETextRendering e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case TR_AUTO:
            m_value.ident = CSS_VAL_AUTO;
            break;
        case TR_OPTIMIZESPEED:
            m_value.ident = CSS_VAL_OPTIMIZESPEED;
            break;
        case TR_OPTIMIZELEGIBILITY:
            m_value.ident = CSS_VAL_OPTIMIZELEGIBILITY;
            break;
        case TR_GEOMETRICPRECISION:
            m_value.ident = CSS_VAL_GEOMETRICPRECISION;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator ETextRendering() const
{
    switch (m_value.ident) {
        case CSS_VAL_AUTO:
            return TR_AUTO;
        case CSS_VAL_OPTIMIZESPEED:
            return TR_OPTIMIZESPEED;
        case CSS_VAL_OPTIMIZELEGIBILITY:
            return TR_OPTIMIZELEGIBILITY;
        case CSS_VAL_GEOMETRICPRECISION:
            return TR_GEOMETRICPRECISION;
    }
    ASSERT_NOT_REACHED();
    return TR_AUTO;
}

template<> inline CSSPrimitiveValue::CSSPrimitiveValue(EWritingMode e)
    : m_type(CSS_IDENT)
{
    switch (e) {
        case WM_LRTB:
            m_value.ident = CSS_VAL_LR_TB;
            break;
        case WM_LR:
            m_value.ident = CSS_VAL_LR;
            break;
        case WM_RLTB:
            m_value.ident = CSS_VAL_RL_TB;
            break;
        case WM_RL:
            m_value.ident = CSS_VAL_RL;
            break;
        case WM_TBRL:
            m_value.ident = CSS_VAL_TB_RL;
            break;
        case WM_TB:
            m_value.ident = CSS_VAL_TB;
            break;
    }
}

template<> inline CSSPrimitiveValue::operator EWritingMode() const
{
    return (EWritingMode)(m_value.ident - CSS_VAL_LR_TB);
}

#endif

}

#endif
