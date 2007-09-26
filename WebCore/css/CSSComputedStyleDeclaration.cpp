/**
 *
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
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
};

const unsigned numComputedProperties = sizeof(computedProperties) / sizeof(computedProperties[0]);

static PassRefPtr<CSSValue> valueForLength(const Length& length)
{
    switch (length.type()) {
        case Auto:
            return new CSSPrimitiveValue(CSS_VAL_AUTO);
        case WebCore::Fixed:
            return new CSSPrimitiveValue(length.value(), CSSPrimitiveValue::CSS_PX);
        case Intrinsic:
            return new CSSPrimitiveValue(CSS_VAL_INTRINSIC);
        case MinIntrinsic:
            return new CSSPrimitiveValue(CSS_VAL_MIN_INTRINSIC);
        case Percent:
            return new CSSPrimitiveValue(length.percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
        case Relative:
        case Static:
            break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

// Handles special value for "none".
static PassRefPtr<CSSValue> valueForMaxLength(const Length& length)
{
    if (length.isFixed() && length.value() == undefinedLength)
        return new CSSPrimitiveValue(CSS_VAL_NONE);
    return valueForLength(length);
}

static PassRefPtr<CSSValue> valueForBorderStyle(EBorderStyle style)
{
    switch (style) {
        case BNONE:
            return new CSSPrimitiveValue(CSS_VAL_NONE);
        case BHIDDEN:
            return new CSSPrimitiveValue(CSS_VAL_HIDDEN);
        case INSET:
            return new CSSPrimitiveValue(CSS_VAL_INSET);
        case GROOVE:
            return new CSSPrimitiveValue(CSS_VAL_GROOVE);
        case RIDGE:
            return new CSSPrimitiveValue(CSS_VAL_RIDGE);
        case OUTSET:
            return new CSSPrimitiveValue(CSS_VAL_OUTSET);
        case DOTTED:
            return new CSSPrimitiveValue(CSS_VAL_DOTTED);
        case DASHED:
            return new CSSPrimitiveValue(CSS_VAL_DASHED);
        case SOLID:
            return new CSSPrimitiveValue(CSS_VAL_SOLID);
        case DOUBLE:
            return new CSSPrimitiveValue(CSS_VAL_DOUBLE);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static PassRefPtr<CSSValue> valueForTextAlign(ETextAlign align)
{
    switch (align) {
        case TAAUTO:
            return new CSSPrimitiveValue(CSS_VAL_AUTO);
        case LEFT:
            return new CSSPrimitiveValue(CSS_VAL_LEFT);
        case RIGHT:
            return new CSSPrimitiveValue(CSS_VAL_RIGHT);
        case CENTER:
            return new CSSPrimitiveValue(CSS_VAL_CENTER);
        case JUSTIFY:
            return new CSSPrimitiveValue(CSS_VAL_JUSTIFY);
        case WEBKIT_LEFT:
            return new CSSPrimitiveValue(CSS_VAL__WEBKIT_LEFT);
        case WEBKIT_RIGHT:
            return new CSSPrimitiveValue(CSS_VAL__WEBKIT_RIGHT);
        case WEBKIT_CENTER:
            return new CSSPrimitiveValue(CSS_VAL__WEBKIT_CENTER);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static PassRefPtr<CSSValue> valueForAppearance(EAppearance appearance)
{
    switch (appearance) {
        case NoAppearance:
            return new CSSPrimitiveValue(CSS_VAL_NONE);
        case CheckboxAppearance:
            return new CSSPrimitiveValue(CSS_VAL_CHECKBOX);
        case RadioAppearance:
            return new CSSPrimitiveValue(CSS_VAL_RADIO);
        case PushButtonAppearance:
            return new CSSPrimitiveValue(CSS_VAL_PUSH_BUTTON);
        case SquareButtonAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SQUARE_BUTTON);
        case ButtonAppearance:
            return new CSSPrimitiveValue(CSS_VAL_BUTTON);
        case ButtonBevelAppearance:
            return new CSSPrimitiveValue(CSS_VAL_BUTTON_BEVEL);
        case ListboxAppearance:
            return new CSSPrimitiveValue(CSS_VAL_LISTBOX);
        case ListItemAppearance:
            return new CSSPrimitiveValue(CSS_VAL_LISTITEM);
        case MenulistAppearance:
            return new CSSPrimitiveValue(CSS_VAL_MENULIST);
        case MenulistButtonAppearance:
            return new CSSPrimitiveValue(CSS_VAL_MENULIST_BUTTON);
        case MenulistTextAppearance:
            return new CSSPrimitiveValue(CSS_VAL_MENULIST_TEXT);
        case MenulistTextFieldAppearance:
            return new CSSPrimitiveValue(CSS_VAL_MENULIST_TEXTFIELD);
        case ScrollbarButtonUpAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SCROLLBARBUTTON_UP);
        case ScrollbarButtonDownAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SCROLLBARBUTTON_DOWN);
        case ScrollbarButtonLeftAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SCROLLBARBUTTON_LEFT);
        case ScrollbarButtonRightAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SCROLLBARBUTTON_RIGHT);
        case ScrollbarTrackHorizontalAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SCROLLBARTRACK_HORIZONTAL);
        case ScrollbarTrackVerticalAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SCROLLBARTRACK_VERTICAL);
        case ScrollbarThumbHorizontalAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SCROLLBARTHUMB_HORIZONTAL);
        case ScrollbarThumbVerticalAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SCROLLBARTHUMB_VERTICAL);
        case ScrollbarGripperHorizontalAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SCROLLBARGRIPPER_HORIZONTAL);
        case ScrollbarGripperVerticalAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SCROLLBARGRIPPER_VERTICAL);
        case SliderHorizontalAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SLIDER_HORIZONTAL);
        case SliderVerticalAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SLIDER_VERTICAL);
        case SliderThumbHorizontalAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SLIDERTHUMB_HORIZONTAL);
        case SliderThumbVerticalAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SLIDERTHUMB_VERTICAL);
        case CaretAppearance:
            return new CSSPrimitiveValue(CSS_VAL_CARET);
        case SearchFieldAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SEARCHFIELD);
        case SearchFieldDecorationAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SEARCHFIELD_DECORATION);
        case SearchFieldResultsDecorationAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SEARCHFIELD_RESULTS_DECORATION);
        case SearchFieldResultsButtonAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SEARCHFIELD_RESULTS_BUTTON);
        case SearchFieldCancelButtonAppearance:
            return new CSSPrimitiveValue(CSS_VAL_SEARCHFIELD_CANCEL_BUTTON);
        case TextFieldAppearance:
            return new CSSPrimitiveValue(CSS_VAL_TEXTFIELD);
        case TextAreaAppearance:
            return new CSSPrimitiveValue(CSS_VAL_TEXTAREA);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static PassRefPtr<CSSValue> valueForMarginCollapse(EMarginCollapse collapse)
{
    switch (collapse) {
        case MCOLLAPSE:
            return new CSSPrimitiveValue(CSS_VAL_COLLAPSE);
        case MSEPARATE:
            return new CSSPrimitiveValue(CSS_VAL_SEPARATE);
        case MDISCARD:
            return new CSSPrimitiveValue(CSS_VAL_DISCARD);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

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
        return valueForLength(l);

    if (style->position() == RelativePosition)
        // FIXME: It's not enough to simply return "auto" values for one offset if the other side is defined.
        // In other words if left is auto and right is not auto, then left's computed value is negative right.
        // So we should get the opposite length unit and see if it is auto.
        return valueForLength(l);

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

// Display integers in integer format instead of "1.0".
static String numberAsString(double n)
{
    long i = static_cast<long>(n);
    return i == n ? String::number(i) : String::number(n);
}

PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(int propertyID) const
{
    return getPropertyCSSValue(propertyID, UpdateLayout);
}

PassRefPtr<CSSPrimitiveValue> primitiveValueFromLength(Length length)
{
    String string;
    switch (length.type()) {
        case Percent:
            string = numberAsString(length.percent()) + "%";
            break;
        case Fixed:
            string = numberAsString(length.calcMinValue(0));
            break;
        case Auto:
            string = "auto";
            break;
        default:
            break;
    }
    return new CSSPrimitiveValue(string, CSSPrimitiveValue::CSS_STRING);
}

PassRefPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(int propertyID, EUpdateLayout updateLayout) const
{
    Node* node = m_node.get();
    if (!node)
        return 0;

    // Make sure our layout is up to date before we allow a query on these attributes.
    if (updateLayout)
        node->document()->updateLayout();

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
            list->append(valueForLength(style->backgroundSize().width));
            list->append(valueForLength(style->backgroundSize().height));
            return list.release();
        }  
        case CSS_PROP_BACKGROUND_REPEAT:
            switch (style->backgroundRepeat()) {
                case REPEAT:
                    return new CSSPrimitiveValue(CSS_VAL_REPEAT);
                case REPEAT_X:
                    return new CSSPrimitiveValue(CSS_VAL_REPEAT_X);
                case REPEAT_Y:
                    return new CSSPrimitiveValue(CSS_VAL_REPEAT_Y);
                case NO_REPEAT:
                    return new CSSPrimitiveValue(CSS_VAL_NO_REPEAT);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_BACKGROUND_COMPOSITE:
            switch (style->backgroundComposite()) {
                case CompositeClear:
                    return new CSSPrimitiveValue(CSS_VAL_CLEAR);
                case CompositeCopy:
                    return new CSSPrimitiveValue(CSS_VAL_COPY);
                case CompositeSourceOver:
                    return new CSSPrimitiveValue(CSS_VAL_SOURCE_OVER);
                case CompositeSourceIn:
                    return new CSSPrimitiveValue(CSS_VAL_SOURCE_IN);
                case CompositeSourceOut:
                    return new CSSPrimitiveValue(CSS_VAL_SOURCE_OUT);
                case CompositeSourceAtop:
                    return new CSSPrimitiveValue(CSS_VAL_SOURCE_ATOP);
                case CompositeDestinationOver:
                    return new CSSPrimitiveValue(CSS_VAL_DESTINATION_OVER);
                case CompositeDestinationIn:
                    return new CSSPrimitiveValue(CSS_VAL_DESTINATION_IN);
                case CompositeDestinationOut:
                    return new CSSPrimitiveValue(CSS_VAL_DESTINATION_OUT);
                case CompositeDestinationAtop:
                    return new CSSPrimitiveValue(CSS_VAL_DESTINATION_ATOP);
                case CompositeXOR:
                    return new CSSPrimitiveValue(CSS_VAL_XOR);
                case CompositePlusDarker:
                    return new CSSPrimitiveValue(CSS_VAL_PLUS_DARKER);
                case CompositeHighlight:
                    return new CSSPrimitiveValue(CSS_VAL_HIGHLIGHT);
                case CompositePlusLighter:
                    return new CSSPrimitiveValue(CSS_VAL_PLUS_LIGHTER);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_BACKGROUND_ATTACHMENT:
            if (style->backgroundAttachment())
                return new CSSPrimitiveValue(CSS_VAL_SCROLL);
            return new CSSPrimitiveValue(CSS_VAL_FIXED);
        case CSS_PROP__WEBKIT_BACKGROUND_CLIP:
        case CSS_PROP__WEBKIT_BACKGROUND_ORIGIN: {
            EBackgroundBox box = (propertyID == CSS_PROP__WEBKIT_BACKGROUND_CLIP ? style->backgroundClip() : style->backgroundOrigin());
            switch (box) {
                case BGBORDER:
                    return new CSSPrimitiveValue(CSS_VAL_BORDER);
                case BGPADDING:
                    return new CSSPrimitiveValue(CSS_VAL_PADDING);
                case BGCONTENT:
                    return new CSSPrimitiveValue(CSS_VAL_CONTENT);
            }
            ASSERT_NOT_REACHED();
            return 0;
        }
        case CSS_PROP_BACKGROUND_POSITION: {
            RefPtr<CSSValueList> list = new CSSValueList(true);

            Length length(style->backgroundXPosition());
            if (length.isPercent())
                list->append(new CSSPrimitiveValue(length.percent(), CSSPrimitiveValue::CSS_PERCENTAGE));
            else
                list->append(new CSSPrimitiveValue(length.value(), CSSPrimitiveValue::CSS_PX));

            length = style->backgroundYPosition();
            if (length.isPercent())
                list->append(new CSSPrimitiveValue(length.percent(), CSSPrimitiveValue::CSS_PERCENTAGE));
            else
                list->append(new CSSPrimitiveValue(length.value(), CSSPrimitiveValue::CSS_PX));

            return list.release();
        }
        case CSS_PROP_BACKGROUND_POSITION_X: {
            Length length(style->backgroundXPosition());
            if (length.isPercent())
                return new CSSPrimitiveValue(length.percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
            else
                return new CSSPrimitiveValue(length.value(), CSSPrimitiveValue::CSS_PX);
        }
        case CSS_PROP_BACKGROUND_POSITION_Y: {
            Length length(style->backgroundYPosition());
            if (length.isPercent())
                return new CSSPrimitiveValue(length.percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
            else
                return new CSSPrimitiveValue(length.value(), CSSPrimitiveValue::CSS_PX);
        }
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
            return valueForBorderStyle(style->borderTopStyle());
        case CSS_PROP_BORDER_RIGHT_STYLE:
            return valueForBorderStyle(style->borderRightStyle());
        case CSS_PROP_BORDER_BOTTOM_STYLE:
            return valueForBorderStyle(style->borderBottomStyle());
        case CSS_PROP_BORDER_LEFT_STYLE:
            return valueForBorderStyle(style->borderLeftStyle());
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
            switch (style->boxAlign()) {
                case BSTRETCH:
                    return new CSSPrimitiveValue(CSS_VAL_STRETCH);
                case BSTART:
                    return new CSSPrimitiveValue(CSS_VAL_START);
                case BCENTER:
                    return new CSSPrimitiveValue(CSS_VAL_CENTER);
                case BEND:
                    return new CSSPrimitiveValue(CSS_VAL_END);
                case BBASELINE:
                    return new CSSPrimitiveValue(CSS_VAL_BASELINE);
                case BJUSTIFY:
                    break; // not allowed
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_BOX_DIRECTION:
            switch (style->boxDirection()) {
                case BNORMAL:
                    return new CSSPrimitiveValue(CSS_VAL_NORMAL);
                case BREVERSE:
                    return new CSSPrimitiveValue(CSS_VAL_REVERSE);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_BOX_FLEX:
            return new CSSPrimitiveValue(style->boxFlex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP__WEBKIT_BOX_FLEX_GROUP:
            return new CSSPrimitiveValue(style->boxFlexGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP__WEBKIT_BOX_LINES:
            switch (style->boxLines()) {
                case SINGLE:
                    return new CSSPrimitiveValue(CSS_VAL_SINGLE);
                case MULTIPLE:
                    return new CSSPrimitiveValue(CSS_VAL_MULTIPLE);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_BOX_ORDINAL_GROUP:
            return new CSSPrimitiveValue(style->boxOrdinalGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP__WEBKIT_BOX_ORIENT:
            switch (style->boxOrient()) {
                case HORIZONTAL:
                    return new CSSPrimitiveValue(CSS_VAL_HORIZONTAL);
                case VERTICAL:
                    return new CSSPrimitiveValue(CSS_VAL_VERTICAL);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_BOX_PACK:
            switch (style->boxPack()) {
                case BSTART:
                    return new CSSPrimitiveValue(CSS_VAL_START);
                case BEND:
                    return new CSSPrimitiveValue(CSS_VAL_END);
                case BCENTER:
                    return new CSSPrimitiveValue(CSS_VAL_CENTER);
                case BJUSTIFY:
                    return new CSSPrimitiveValue(CSS_VAL_JUSTIFY);
                case BSTRETCH:
                case BBASELINE:
                    break; // not allowed
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_BOX_SHADOW:
            return valueForShadow(style->boxShadow());
        case CSS_PROP_CAPTION_SIDE:
            switch (style->captionSide()) {
                case CAPLEFT:
                    return new CSSPrimitiveValue(CSS_VAL_LEFT);
                case CAPRIGHT:
                    return new CSSPrimitiveValue(CSS_VAL_RIGHT);
                case CAPTOP:
                    return new CSSPrimitiveValue(CSS_VAL_TOP);
                case CAPBOTTOM:
                    return new CSSPrimitiveValue(CSS_VAL_BOTTOM);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_CLEAR:
            switch (style->clear()) {
                case CNONE:
                    return new CSSPrimitiveValue(CSS_VAL_NONE);
                case CLEFT:
                    return new CSSPrimitiveValue(CSS_VAL_LEFT);
                case CRIGHT:
                    return new CSSPrimitiveValue(CSS_VAL_RIGHT);
                case CBOTH:
                    return new CSSPrimitiveValue(CSS_VAL_BOTH);
            }
            ASSERT_NOT_REACHED();
            return 0;
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
            return valueForBorderStyle(style->columnRuleStyle());
        case CSS_PROP__WEBKIT_COLUMN_RULE_WIDTH:
            return new CSSPrimitiveValue(style->columnRuleWidth(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP__WEBKIT_COLUMN_BREAK_AFTER:
            switch (style->columnBreakAfter()) {
                case PBAUTO:
                    return new CSSPrimitiveValue(CSS_VAL_AUTO);
                case PBALWAYS:
                    return new CSSPrimitiveValue(CSS_VAL_ALWAYS);
                case PBAVOID:
                    return new CSSPrimitiveValue(CSS_VAL_AVOID);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_COLUMN_BREAK_BEFORE:
            switch (style->columnBreakBefore()) {
                case PBAUTO:
                    return new CSSPrimitiveValue(CSS_VAL_AUTO);
                case PBALWAYS:
                    return new CSSPrimitiveValue(CSS_VAL_ALWAYS);
                case PBAVOID:
                    return new CSSPrimitiveValue(CSS_VAL_AVOID);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_COLUMN_BREAK_INSIDE:
            switch (style->columnBreakInside()) {
                case PBAUTO:
                    return new CSSPrimitiveValue(CSS_VAL_AUTO);
                case PBAVOID:
                    return new CSSPrimitiveValue(CSS_VAL_AVOID);
                case PBALWAYS:
                    break; // not allowed
            }
            ASSERT_NOT_REACHED();
            return 0;
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
            RefPtr<CSSValue> value;
            switch (style->cursor()) {
                case CURSOR_AUTO:
                    value = new CSSPrimitiveValue(CSS_VAL_AUTO);
                    break;
                case CURSOR_CROSS:
                    value = new CSSPrimitiveValue(CSS_VAL_CROSSHAIR);
                    break;
                case CURSOR_DEFAULT:
                    value = new CSSPrimitiveValue(CSS_VAL_DEFAULT);
                    break;
                case CURSOR_POINTER:
                    value = new CSSPrimitiveValue(CSS_VAL_POINTER);
                    break;
                case CURSOR_MOVE:
                    value = new CSSPrimitiveValue(CSS_VAL_MOVE);
                    break;
                case CURSOR_CELL:
                    value = new CSSPrimitiveValue(CSS_VAL_CELL);
                    break;
                case CURSOR_VERTICAL_TEXT:
                    value = new CSSPrimitiveValue(CSS_VAL_VERTICAL_TEXT);
                    break;
                case CURSOR_CONTEXT_MENU:
                    value = new CSSPrimitiveValue(CSS_VAL_CONTEXT_MENU);
                    break;
                case CURSOR_ALIAS:
                    value = new CSSPrimitiveValue(CSS_VAL_ALIAS);
                    break;
                case CURSOR_COPY:
                    value = new CSSPrimitiveValue(CSS_VAL_COPY);
                    break;
                case CURSOR_NONE:
                    value = new CSSPrimitiveValue(CSS_VAL_NONE);
                    break;
                case CURSOR_PROGRESS:
                    value = new CSSPrimitiveValue(CSS_VAL_PROGRESS);
                    break;
                case CURSOR_NO_DROP:
                    value = new CSSPrimitiveValue(CSS_VAL_NO_DROP);
                    break;
                case CURSOR_NOT_ALLOWED:
                    value = new CSSPrimitiveValue(CSS_VAL_NOT_ALLOWED);
                    break;
                case CURSOR_WEBKIT_ZOOM_IN:
                    value = new CSSPrimitiveValue(CSS_VAL__WEBKIT_ZOOM_IN);
                    break;
                case CURSOR_WEBKIT_ZOOM_OUT:
                    value = new CSSPrimitiveValue(CSS_VAL__WEBKIT_ZOOM_OUT);
                    break;
                case CURSOR_E_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_E_RESIZE);
                    break;
                case CURSOR_NE_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_NE_RESIZE);
                    break;
                case CURSOR_NW_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_NW_RESIZE);
                    break;
                case CURSOR_N_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_N_RESIZE);
                    break;
                case CURSOR_SE_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_SE_RESIZE);
                    break;
                case CURSOR_SW_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_SW_RESIZE);
                    break;
                case CURSOR_S_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_S_RESIZE);
                    break;
                case CURSOR_W_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_W_RESIZE);
                    break;
                case CURSOR_EW_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_EW_RESIZE);
                    break;
                case CURSOR_NS_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_NS_RESIZE);
                    break;
                case CURSOR_NESW_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_NESW_RESIZE);
                    break;
                case CURSOR_NWSE_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_NWSE_RESIZE);
                    break;
                case CURSOR_COL_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_COL_RESIZE);
                    break;
                case CURSOR_ROW_RESIZE:
                    value = new CSSPrimitiveValue(CSS_VAL_ROW_RESIZE);
                    break;
                case CURSOR_TEXT:
                    value = new CSSPrimitiveValue(CSS_VAL_TEXT);
                    break;
                case CURSOR_WAIT:
                    value = new CSSPrimitiveValue(CSS_VAL_WAIT);
                    break;
                case CURSOR_HELP:
                    value = new CSSPrimitiveValue(CSS_VAL_HELP);
                    break;
                case CURSOR_ALL_SCROLL:
                    value = new CSSPrimitiveValue(CSS_VAL_ALL_SCROLL);
                    break;
            }
            ASSERT(value);
            if (list) {
                list->append(value);
                return list.release();
            }
            return value.release();
        }
        case CSS_PROP_DIRECTION:
            switch (style->direction()) {
                case LTR:
                    return new CSSPrimitiveValue(CSS_VAL_LTR);
                case RTL:
                    return new CSSPrimitiveValue(CSS_VAL_RTL);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_DISPLAY:
            switch (style->display()) {
                case INLINE:
                    return new CSSPrimitiveValue(CSS_VAL_INLINE);
                case BLOCK:
                    return new CSSPrimitiveValue(CSS_VAL_BLOCK);
                case LIST_ITEM:
                    return new CSSPrimitiveValue(CSS_VAL_LIST_ITEM);
                case RUN_IN:
                    return new CSSPrimitiveValue(CSS_VAL_RUN_IN);
                case COMPACT:
                    return new CSSPrimitiveValue(CSS_VAL_COMPACT);
                case INLINE_BLOCK:
                    return new CSSPrimitiveValue(CSS_VAL_INLINE_BLOCK);
                case TABLE:
                    return new CSSPrimitiveValue(CSS_VAL_TABLE);
                case INLINE_TABLE:
                    return new CSSPrimitiveValue(CSS_VAL_INLINE_TABLE);
                case TABLE_ROW_GROUP:
                    return new CSSPrimitiveValue(CSS_VAL_TABLE_ROW_GROUP);
                case TABLE_HEADER_GROUP:
                    return new CSSPrimitiveValue(CSS_VAL_TABLE_HEADER_GROUP);
                case TABLE_FOOTER_GROUP:
                    return new CSSPrimitiveValue(CSS_VAL_TABLE_FOOTER_GROUP);
                case TABLE_ROW:
                    return new CSSPrimitiveValue(CSS_VAL_TABLE_ROW);
                case TABLE_COLUMN_GROUP:
                    return new CSSPrimitiveValue(CSS_VAL_TABLE_COLUMN_GROUP);
                case TABLE_COLUMN:
                    return new CSSPrimitiveValue(CSS_VAL_TABLE_COLUMN);
                case TABLE_CELL:
                    return new CSSPrimitiveValue(CSS_VAL_TABLE_CELL);
                case TABLE_CAPTION:
                    return new CSSPrimitiveValue(CSS_VAL_TABLE_CAPTION);
                case BOX:
                    return new CSSPrimitiveValue(CSS_VAL__WEBKIT_BOX);
                case INLINE_BOX:
                    return new CSSPrimitiveValue(CSS_VAL__WEBKIT_INLINE_BOX);
                case NONE:
                    return new CSSPrimitiveValue(CSS_VAL_NONE);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_EMPTY_CELLS:
            switch (style->emptyCells()) {
                case SHOW:
                    return new CSSPrimitiveValue(CSS_VAL_SHOW);
                case HIDE:
                    return new CSSPrimitiveValue(CSS_VAL_HIDE);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_FLOAT:
            switch (style->floating()) {
                case FNONE:
                    return new CSSPrimitiveValue(CSS_VAL_NONE);
                case FLEFT:
                    return new CSSPrimitiveValue(CSS_VAL_LEFT);
                case FRIGHT:
                    return new CSSPrimitiveValue(CSS_VAL_RIGHT);
            }
            ASSERT_NOT_REACHED();
            return 0;
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
            return valueForLength(style->height());
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
            switch (style->listStylePosition()) {
                case OUTSIDE:
                    return new CSSPrimitiveValue(CSS_VAL_OUTSIDE);
                case INSIDE:
                    return new CSSPrimitiveValue(CSS_VAL_INSIDE);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_LIST_STYLE_TYPE:
            switch (style->listStyleType()) {
                case LNONE:
                    return new CSSPrimitiveValue(CSS_VAL_NONE);
                case DISC:
                    return new CSSPrimitiveValue(CSS_VAL_DISC);
                case CIRCLE:
                    return new CSSPrimitiveValue(CSS_VAL_CIRCLE);
                case SQUARE:
                    return new CSSPrimitiveValue(CSS_VAL_SQUARE);
                case LDECIMAL:
                    return new CSSPrimitiveValue(CSS_VAL_DECIMAL);
                case DECIMAL_LEADING_ZERO:
                    return new CSSPrimitiveValue(CSS_VAL_DECIMAL_LEADING_ZERO);
                case LOWER_ROMAN:
                    return new CSSPrimitiveValue(CSS_VAL_LOWER_ROMAN);
                case UPPER_ROMAN:
                    return new CSSPrimitiveValue(CSS_VAL_UPPER_ROMAN);
                case LOWER_GREEK:
                    return new CSSPrimitiveValue(CSS_VAL_LOWER_GREEK);
                case LOWER_ALPHA:
                    return new CSSPrimitiveValue(CSS_VAL_LOWER_ALPHA);
                case LOWER_LATIN:
                    return new CSSPrimitiveValue(CSS_VAL_LOWER_LATIN);
                case UPPER_ALPHA:
                    return new CSSPrimitiveValue(CSS_VAL_UPPER_ALPHA);
                case UPPER_LATIN:
                    return new CSSPrimitiveValue(CSS_VAL_UPPER_LATIN);
                case HEBREW:
                    return new CSSPrimitiveValue(CSS_VAL_HEBREW);
                case ARMENIAN:
                    return new CSSPrimitiveValue(CSS_VAL_ARMENIAN);
                case GEORGIAN:
                    return new CSSPrimitiveValue(CSS_VAL_GEORGIAN);
                case CJK_IDEOGRAPHIC:
                    return new CSSPrimitiveValue(CSS_VAL_CJK_IDEOGRAPHIC);
                case HIRAGANA:
                    return new CSSPrimitiveValue(CSS_VAL_HIRAGANA);
                case KATAKANA:
                    return new CSSPrimitiveValue(CSS_VAL_KATAKANA);
                case HIRAGANA_IROHA:
                    return new CSSPrimitiveValue(CSS_VAL_HIRAGANA_IROHA);
                case KATAKANA_IROHA:
                    return new CSSPrimitiveValue(CSS_VAL_KATAKANA_IROHA);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_MARGIN_TOP:
            if (renderer)
                // FIXME: Supposed to return the percentage if percentage was specified.
                return new CSSPrimitiveValue(renderer->marginTop(), CSSPrimitiveValue::CSS_PX);
            return valueForLength(style->marginTop());
        case CSS_PROP_MARGIN_RIGHT:
            if (renderer)
                // FIXME: Supposed to return the percentage if percentage was specified.
                return new CSSPrimitiveValue(renderer->marginRight(), CSSPrimitiveValue::CSS_PX);
            return valueForLength(style->marginRight());
        case CSS_PROP_MARGIN_BOTTOM:
            if (renderer)
                // FIXME: Supposed to return the percentage if percentage was specified.
                return new CSSPrimitiveValue(renderer->marginBottom(), CSSPrimitiveValue::CSS_PX);
            return valueForLength(style->marginBottom());
        case CSS_PROP_MARGIN_LEFT:
            if (renderer)
                // FIXME: Supposed to return the percentage if percentage was specified.
                return new CSSPrimitiveValue(renderer->marginLeft(), CSSPrimitiveValue::CSS_PX);
            return valueForLength(style->marginLeft());
        case CSS_PROP__WEBKIT_MARQUEE_DIRECTION:
            switch (style->marqueeDirection()) {
                case MFORWARD:
                    return new CSSPrimitiveValue(CSS_VAL_FORWARDS);
                case MBACKWARD:
                    return new CSSPrimitiveValue(CSS_VAL_BACKWARDS);
                case MAUTO:
                    return new CSSPrimitiveValue(CSS_VAL_AUTO);
                case MUP:
                    return new CSSPrimitiveValue(CSS_VAL_UP);
                case MDOWN:
                    return new CSSPrimitiveValue(CSS_VAL_DOWN);
                case MLEFT:
                    return new CSSPrimitiveValue(CSS_VAL_LEFT);
                case MRIGHT:
                    return new CSSPrimitiveValue(CSS_VAL_RIGHT);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_MARQUEE_INCREMENT:
            return valueForLength(style->marqueeIncrement());
        case CSS_PROP__WEBKIT_MARQUEE_REPETITION:
            if (style->marqueeLoopCount() < 0)
                return new CSSPrimitiveValue(CSS_VAL_INFINITE);
            return new CSSPrimitiveValue(style->marqueeLoopCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP__WEBKIT_MARQUEE_STYLE:
            switch (style->marqueeBehavior()) {
                case MNONE:
                    return new CSSPrimitiveValue(CSS_VAL_NONE);
                case MSCROLL:
                    return new CSSPrimitiveValue(CSS_VAL_SCROLL);
                case MSLIDE:
                    return new CSSPrimitiveValue(CSS_VAL_SLIDE);
                case MALTERNATE:
                    return new CSSPrimitiveValue(CSS_VAL_ALTERNATE);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_USER_MODIFY:
            switch (style->userModify()) {
                case READ_ONLY:
                    return new CSSPrimitiveValue(CSS_VAL_READ_ONLY);
                case READ_WRITE:
                    return new CSSPrimitiveValue(CSS_VAL_READ_WRITE);
                case READ_WRITE_PLAINTEXT_ONLY:
                    return new CSSPrimitiveValue(CSS_VAL_READ_WRITE_PLAINTEXT_ONLY);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_MAX_HEIGHT:
            return valueForMaxLength(style->maxHeight());
        case CSS_PROP_MAX_WIDTH:
            return valueForMaxLength(style->maxWidth());
        case CSS_PROP_MIN_HEIGHT:
            return valueForLength(style->minHeight());
        case CSS_PROP_MIN_WIDTH:
            return valueForLength(style->minWidth());
        case CSS_PROP_OPACITY:
            return new CSSPrimitiveValue(style->opacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_ORPHANS:
            return new CSSPrimitiveValue(style->orphans(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_OUTLINE_COLOR:
            return currentColorOrValidColor(style, style->outlineColor());
        case CSS_PROP_OUTLINE_STYLE:
            if (style->outlineStyleIsAuto())
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            return valueForBorderStyle(style->outlineStyle());
        case CSS_PROP_OUTLINE_WIDTH:
            return new CSSPrimitiveValue(style->outlineWidth(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP_OVERFLOW:
        case CSS_PROP_OVERFLOW_X:
        case CSS_PROP_OVERFLOW_Y: {
            EOverflow overflow;
            switch (propertyID) {
                case CSS_PROP_OVERFLOW_X:
                    overflow = style->overflowX();
                    break;
                case CSS_PROP_OVERFLOW_Y:
                    overflow = style->overflowY();
                    break;
                default:
                    overflow = max(style->overflowX(), style->overflowY());
            }
            switch (overflow) {
                case OVISIBLE:
                    return new CSSPrimitiveValue(CSS_VAL_VISIBLE);
                case OHIDDEN:
                    return new CSSPrimitiveValue(CSS_VAL_HIDDEN);
                case OSCROLL:
                    return new CSSPrimitiveValue(CSS_VAL_SCROLL);
                case OAUTO:
                    return new CSSPrimitiveValue(CSS_VAL_AUTO);
                case OMARQUEE:
                    return new CSSPrimitiveValue(CSS_VAL__WEBKIT_MARQUEE);
                case OOVERLAY:
                    return new CSSPrimitiveValue(CSS_VAL_OVERLAY);
            }
            ASSERT_NOT_REACHED();
            return 0;
        }
        case CSS_PROP_PADDING_TOP:
            if (renderer)
                return new CSSPrimitiveValue(renderer->paddingTop(), CSSPrimitiveValue::CSS_PX);
            return valueForLength(style->paddingTop());
        case CSS_PROP_PADDING_RIGHT:
            if (renderer)
                return new CSSPrimitiveValue(renderer->paddingRight(), CSSPrimitiveValue::CSS_PX);
            return valueForLength(style->paddingRight());
        case CSS_PROP_PADDING_BOTTOM:
            if (renderer)
                return new CSSPrimitiveValue(renderer->paddingBottom(), CSSPrimitiveValue::CSS_PX);
            return valueForLength(style->paddingBottom());
        case CSS_PROP_PADDING_LEFT:
            if (renderer)
                return new CSSPrimitiveValue(renderer->paddingLeft(), CSSPrimitiveValue::CSS_PX);
            return valueForLength(style->paddingLeft());
        case CSS_PROP_PAGE_BREAK_AFTER:
            switch (style->pageBreakAfter()) {
                case PBAUTO:
                    return new CSSPrimitiveValue(CSS_VAL_AUTO);
                case PBALWAYS:
                    return new CSSPrimitiveValue(CSS_VAL_ALWAYS);
                case PBAVOID:
                    return new CSSPrimitiveValue(CSS_VAL_AVOID);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_PAGE_BREAK_BEFORE:
            switch (style->pageBreakBefore()) {
                case PBAUTO:
                    return new CSSPrimitiveValue(CSS_VAL_AUTO);
                case PBALWAYS:
                    return new CSSPrimitiveValue(CSS_VAL_ALWAYS);
                case PBAVOID:
                    return new CSSPrimitiveValue(CSS_VAL_AVOID);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_PAGE_BREAK_INSIDE:
            switch (style->pageBreakInside()) {
                case PBAUTO:
                    return new CSSPrimitiveValue(CSS_VAL_AUTO);
                case PBAVOID:
                    return new CSSPrimitiveValue(CSS_VAL_AVOID);
                case PBALWAYS:
                    break; // not allowed
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_POSITION:
            switch (style->position()) {
                case StaticPosition:
                    return new CSSPrimitiveValue(CSS_VAL_STATIC);
                case RelativePosition:
                    return new CSSPrimitiveValue(CSS_VAL_RELATIVE);
                case AbsolutePosition:
                    return new CSSPrimitiveValue(CSS_VAL_ABSOLUTE);
                case FixedPosition:
                    return new CSSPrimitiveValue(CSS_VAL_FIXED);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_RIGHT:
            return getPositionOffsetValue(style, CSS_PROP_RIGHT);
        case CSS_PROP_TABLE_LAYOUT:
            switch (style->tableLayout()) {
                case TAUTO:
                    return new CSSPrimitiveValue(CSS_VAL_AUTO);
                case TFIXED:
                    return new CSSPrimitiveValue(CSS_VAL_FIXED);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_TEXT_ALIGN:
            return valueForTextAlign(style->textAlign());
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
            return valueForLength(style->textIndent());
        case CSS_PROP_TEXT_SHADOW:
            return valueForShadow(style->textShadow());
        case CSS_PROP__WEBKIT_TEXT_SECURITY:
            switch (style->textSecurity()) {
                case TSNONE:
                    return new CSSPrimitiveValue(CSS_VAL_NONE);
                case TSDISC:
                    return new CSSPrimitiveValue(CSS_VAL_DISC);
                case TSCIRCLE:
                    return new CSSPrimitiveValue(CSS_VAL_CIRCLE);
                case TSSQUARE:
                    return new CSSPrimitiveValue(CSS_VAL_SQUARE);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_TEXT_SIZE_ADJUST:
            if (style->textSizeAdjust())
                return new CSSPrimitiveValue(CSS_VAL_AUTO);
            return new CSSPrimitiveValue(CSS_VAL_NONE);
        case CSS_PROP__WEBKIT_TEXT_STROKE_COLOR:
            return currentColorOrValidColor(style, style->textStrokeColor());
        case CSS_PROP__WEBKIT_TEXT_STROKE_WIDTH:
            return new CSSPrimitiveValue(style->textStrokeWidth(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_TEXT_TRANSFORM:
            switch (style->textTransform()) {
                case CAPITALIZE:
                    return new CSSPrimitiveValue(CSS_VAL_CAPITALIZE);
                case UPPERCASE:
                    return new CSSPrimitiveValue(CSS_VAL_UPPERCASE);
                case LOWERCASE:
                    return new CSSPrimitiveValue(CSS_VAL_LOWERCASE);
                case TTNONE:
                    return new CSSPrimitiveValue(CSS_VAL_NONE);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_TOP:
            return getPositionOffsetValue(style, CSS_PROP_TOP);
        case CSS_PROP_UNICODE_BIDI:
            switch (style->unicodeBidi()) {
                case UBNormal:
                    return new CSSPrimitiveValue(CSS_VAL_NORMAL);
                case Embed:
                    return new CSSPrimitiveValue(CSS_VAL_EMBED);
                case Override:
                    return new CSSPrimitiveValue(CSS_VAL_BIDI_OVERRIDE);
            }
            ASSERT_NOT_REACHED();
            return 0;
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
                    return valueForLength(style->verticalAlignLength());
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_VISIBILITY:
            switch (style->visibility()) {
                case VISIBLE:
                    return new CSSPrimitiveValue(CSS_VAL_VISIBLE);
                case HIDDEN:
                    return new CSSPrimitiveValue(CSS_VAL_HIDDEN);
                case COLLAPSE:
                    return new CSSPrimitiveValue(CSS_VAL_COLLAPSE);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_WHITE_SPACE:
            switch (style->whiteSpace()) {
                case NORMAL:
                    return new CSSPrimitiveValue(CSS_VAL_NORMAL);
                case PRE:
                    return new CSSPrimitiveValue(CSS_VAL_PRE);
                case PRE_WRAP:
                    return new CSSPrimitiveValue(CSS_VAL_PRE_WRAP);
                case PRE_LINE:
                    return new CSSPrimitiveValue(CSS_VAL_PRE_LINE);
                case NOWRAP:
                    return new CSSPrimitiveValue(CSS_VAL_NOWRAP);
                case KHTML_NOWRAP:
                    return new CSSPrimitiveValue(CSS_VAL__WEBKIT_NOWRAP);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_WIDOWS:
            return new CSSPrimitiveValue(style->widows(), CSSPrimitiveValue::CSS_NUMBER);
        case CSS_PROP_WIDTH:
            if (renderer)
                return new CSSPrimitiveValue(sizingBox(renderer).width(), CSSPrimitiveValue::CSS_PX);
            return valueForLength(style->width());
        case CSS_PROP_WORD_BREAK:
            switch (style->wordBreak()) {
                case NormalWordBreak:
                    return new CSSPrimitiveValue(CSS_VAL_NORMAL);
                case BreakAllWordBreak:
                    return new CSSPrimitiveValue(CSS_VAL_BREAK_ALL);
                case BreakWordBreak:
                    return new CSSPrimitiveValue(CSS_VAL_BREAK_WORD);
            }
        case CSS_PROP_WORD_SPACING:
            return new CSSPrimitiveValue(style->wordSpacing(), CSSPrimitiveValue::CSS_PX);
        case CSS_PROP_WORD_WRAP:
            switch (style->wordWrap()) {
                case NormalWordWrap:
                    return new CSSPrimitiveValue(CSS_VAL_NORMAL);
                case BreakWordWrap:
                    return new CSSPrimitiveValue(CSS_VAL_BREAK_WORD);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_LINE_BREAK:
            switch (style->khtmlLineBreak()) {
                case LBNORMAL:
                    return new CSSPrimitiveValue(CSS_VAL_NORMAL);
                case AFTER_WHITE_SPACE:
                    return new CSSPrimitiveValue(CSS_VAL_AFTER_WHITE_SPACE);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_NBSP_MODE:
            switch (style->nbspMode()) {
                case NBNORMAL:
                    return new CSSPrimitiveValue(CSS_VAL_NORMAL);
                case SPACE:
                    return new CSSPrimitiveValue(CSS_VAL_SPACE);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP__WEBKIT_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR:
            switch (style->matchNearestMailBlockquoteColor()) {
                case BCNORMAL:
                    return new CSSPrimitiveValue(CSS_VAL_NORMAL);
                case MATCH:
                    return new CSSPrimitiveValue(CSS_VAL_MATCH);
            }
            ASSERT_NOT_REACHED();
            return 0;
        case CSS_PROP_RESIZE:
            switch (style->resize()) {
                case RESIZE_BOTH:
                    return new CSSPrimitiveValue(CSS_VAL_BOTH);
                case RESIZE_HORIZONTAL:
                    return new CSSPrimitiveValue(CSS_VAL_HORIZONTAL);
                case RESIZE_VERTICAL:
                    return new CSSPrimitiveValue(CSS_VAL_VERTICAL);
                case RESIZE_NONE:
                    return new CSSPrimitiveValue(CSS_VAL_NONE);
            }
            ASSERT_NOT_REACHED();
            return 0;
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
            return valueForAppearance(style->appearance());
        case CSS_PROP__WEBKIT_FONT_SIZE_DELTA:
            // Not a real style property -- used by the editing engine -- so has no computed value.
            break;
        case CSS_PROP__WEBKIT_MARGIN_BOTTOM_COLLAPSE:
            return valueForMarginCollapse(style->marginBottomCollapse());
        case CSS_PROP__WEBKIT_MARGIN_TOP_COLLAPSE:
            return valueForMarginCollapse(style->marginTopCollapse());
        case CSS_PROP__WEBKIT_RTL_ORDERING:
            if (style->visuallyOrdered())
                return new CSSPrimitiveValue(CSS_VAL_VISUAL);
            return new CSSPrimitiveValue(CSS_VAL_LOGICAL);
        case CSS_PROP__WEBKIT_USER_DRAG:
            switch (style->userDrag()) {
                case DRAG_AUTO:
                    return new CSSPrimitiveValue(CSS_VAL_AUTO);
                case DRAG_NONE:
                    return new CSSPrimitiveValue(CSS_VAL_NONE);
                case DRAG_ELEMENT:
                    return new CSSPrimitiveValue(CSS_VAL_ELEMENT);
            }
            break;
        case CSS_PROP__WEBKIT_USER_SELECT:
            switch (style->userSelect()) {
                case SELECT_NONE:
                    return new CSSPrimitiveValue(CSS_VAL_NONE);
                case SELECT_TEXT:
                    return new CSSPrimitiveValue(CSS_VAL_TEXT);
            }
            break;
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
            // FIXME: The above are unimplemented.
            break;
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
