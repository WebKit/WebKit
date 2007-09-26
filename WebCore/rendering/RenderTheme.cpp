/**
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RenderTheme.h"

#include "Document.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "RenderStyle.h"

// The methods in this file are shared by all themes on every platform.

namespace WebCore {

using namespace HTMLNames;

void RenderTheme::adjustStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e,
                              bool UAHasAppearance, const BorderData& border, const BackgroundLayer& background, const Color& backgroundColor)
{
    // Force inline and table display styles to be inline-block (except for table- which is block)
    if (style->display() == INLINE || style->display() == INLINE_TABLE || style->display() == TABLE_ROW_GROUP ||
        style->display() == TABLE_HEADER_GROUP || style->display() == TABLE_FOOTER_GROUP ||
        style->display() == TABLE_ROW || style->display() == TABLE_COLUMN_GROUP || style->display() == TABLE_COLUMN ||
        style->display() == TABLE_CELL || style->display() == TABLE_CAPTION)
        style->setDisplay(INLINE_BLOCK);
    else if (style->display() == COMPACT || style->display() == RUN_IN || style->display() == LIST_ITEM || style->display() == TABLE)
        style->setDisplay(BLOCK);

    if (UAHasAppearance && theme()->isControlStyled(style, border, background, backgroundColor)) {
        if (style->appearance() == MenulistAppearance)
            style->setAppearance(MenulistButtonAppearance);
        else
            style->setAppearance(NoAppearance);
    }

    // Call the appropriate style adjustment method based off the appearance value.
    switch (style->appearance()) {
        case CheckboxAppearance:
            return adjustCheckboxStyle(selector, style, e);
        case RadioAppearance:
            return adjustRadioStyle(selector, style, e);
        case PushButtonAppearance:
        case SquareButtonAppearance:
        case ButtonAppearance:
            return adjustButtonStyle(selector, style, e);
        case TextFieldAppearance:
            return adjustTextFieldStyle(selector, style, e);
        case TextAreaAppearance:
            return adjustTextAreaStyle(selector, style, e);
        case MenulistAppearance:
            return adjustMenuListStyle(selector, style, e);
        case MenulistButtonAppearance:
            return adjustMenuListButtonStyle(selector, style, e);
        case SliderHorizontalAppearance:
        case SliderVerticalAppearance:
            return adjustSliderTrackStyle(selector, style, e);
        case SliderThumbHorizontalAppearance:
        case SliderThumbVerticalAppearance:
            return adjustSliderThumbStyle(selector, style, e);
        case SearchFieldAppearance:
            return adjustSearchFieldStyle(selector, style, e);
        case SearchFieldCancelButtonAppearance:
            return adjustSearchFieldCancelButtonStyle(selector, style, e);
        case SearchFieldDecorationAppearance:
            return adjustSearchFieldDecorationStyle(selector, style, e);
        case SearchFieldResultsDecorationAppearance:
            return adjustSearchFieldResultsDecorationStyle(selector, style, e);
        case SearchFieldResultsButtonAppearance:
            return adjustSearchFieldResultsButtonStyle(selector, style, e);
        default:
            break;
    }
}

bool RenderTheme::paint(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    // If painting is disabled, but we aren't updating control tints, then just bail.
    // If we are updating control tints, just schedule a repaint if the theme supports tinting
    // for that control.
    if (paintInfo.context->updatingControlTints()) {
        if (controlSupportsTints(o))
            o->repaint();
        return false;
    }
    if (paintInfo.context->paintingDisabled())
        return false;

    // Call the appropriate paint method based off the appearance value.
    switch (o->style()->appearance()) {
        case CheckboxAppearance:
            return paintCheckbox(o, paintInfo, r);
        case RadioAppearance:
            return paintRadio(o, paintInfo, r);
        case PushButtonAppearance:
        case SquareButtonAppearance:
        case ButtonAppearance:
            return paintButton(o, paintInfo, r);
        case MenulistAppearance:
            return paintMenuList(o, paintInfo, r);
        case SliderHorizontalAppearance:
        case SliderVerticalAppearance:
            return paintSliderTrack(o, paintInfo, r);
        case SliderThumbHorizontalAppearance:
        case SliderThumbVerticalAppearance:
            if (o->parent()->isSlider())
                return paintSliderThumb(o, paintInfo, r);
            // We don't support drawing a slider thumb without a parent slider
            break;
        case MenulistButtonAppearance:
        case TextFieldAppearance:
        case TextAreaAppearance:
        case ListboxAppearance:
            return true;
        case SearchFieldAppearance:
            return paintSearchField(o, paintInfo, r);
        case SearchFieldCancelButtonAppearance:
            return paintSearchFieldCancelButton(o, paintInfo, r);
        case SearchFieldDecorationAppearance:
            return paintSearchFieldDecoration(o, paintInfo, r);
        case SearchFieldResultsDecorationAppearance:
            return paintSearchFieldResultsDecoration(o, paintInfo, r);
        case SearchFieldResultsButtonAppearance:
            return paintSearchFieldResultsButton(o, paintInfo, r);
        default:
            break;
    }

    return true; // We don't support the appearance, so let the normal background/border paint.
}

bool RenderTheme::paintBorderOnly(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    if (paintInfo.context->paintingDisabled())
        return false;

    // Call the appropriate paint method based off the appearance value.
    switch (o->style()->appearance()) {
        case TextFieldAppearance:
            return paintTextField(o, paintInfo, r);
        case ListboxAppearance:
        case TextAreaAppearance:
            return paintTextArea(o, paintInfo, r);
        case MenulistButtonAppearance:
            return true;
        case CheckboxAppearance:
        case RadioAppearance:
        case PushButtonAppearance:
        case SquareButtonAppearance:
        case ButtonAppearance:
        case MenulistAppearance:
        case SliderHorizontalAppearance:
        case SliderVerticalAppearance:
        case SliderThumbHorizontalAppearance:
        case SliderThumbVerticalAppearance:
        case SearchFieldAppearance:
        case SearchFieldCancelButtonAppearance:
        case SearchFieldDecorationAppearance:
        case SearchFieldResultsDecorationAppearance:
        case SearchFieldResultsButtonAppearance:
        default:
            break;
    }

    return false;
}

bool RenderTheme::paintDecorations(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    if (paintInfo.context->paintingDisabled())
        return false;

    // Call the appropriate paint method based off the appearance value.
    switch (o->style()->appearance()) {
        case MenulistButtonAppearance:
            return paintMenuListButton(o, paintInfo, r);
        case TextFieldAppearance:
        case TextAreaAppearance:
        case ListboxAppearance:
        case CheckboxAppearance:
        case RadioAppearance:
        case PushButtonAppearance:
        case SquareButtonAppearance:
        case ButtonAppearance:
        case MenulistAppearance:
        case SliderHorizontalAppearance:
        case SliderVerticalAppearance:
        case SliderThumbHorizontalAppearance:
        case SliderThumbVerticalAppearance:
        case SearchFieldAppearance:
        case SearchFieldCancelButtonAppearance:
        case SearchFieldDecorationAppearance:
        case SearchFieldResultsDecorationAppearance:
        case SearchFieldResultsButtonAppearance:
        default:
            break;
    }

    return false;
}

Color RenderTheme::activeSelectionBackgroundColor() const
{
    if (!m_activeSelectionColor.isValid())
        m_activeSelectionColor = platformActiveSelectionBackgroundColor().blendWithWhite();
    return m_activeSelectionColor;
}

Color RenderTheme::inactiveSelectionBackgroundColor() const
{
    if (!m_inactiveSelectionColor.isValid())
        m_inactiveSelectionColor = platformInactiveSelectionBackgroundColor().blendWithWhite();
    return m_inactiveSelectionColor;
}

Color RenderTheme::platformActiveSelectionBackgroundColor() const
{
    // Use a blue color by default if the platform theme doesn't define anything.
    return Color(0, 0, 255);
}

Color RenderTheme::platformInactiveSelectionBackgroundColor() const
{
    // Use a grey color by default if the platform theme doesn't define anything.
    return Color(128, 128, 128);
}

Color RenderTheme::platformActiveSelectionForegroundColor() const
{
    return Color();
}

Color RenderTheme::platformInactiveSelectionForegroundColor() const
{
    return Color();
}

Color RenderTheme::activeListBoxSelectionBackgroundColor() const
{
    return activeSelectionBackgroundColor();
}

Color RenderTheme::activeListBoxSelectionForegroundColor() const
{
    // Use a white color by default if the platform theme doesn't define anything.
    return Color(255, 255, 255);
}

Color RenderTheme::inactiveListBoxSelectionBackgroundColor() const
{
    return inactiveSelectionBackgroundColor();
}

Color RenderTheme::inactiveListBoxSelectionForegroundColor() const
{
    // Use a black color by default if the platform theme doesn't define anything.
    return Color(0, 0, 0);
}

short RenderTheme::baselinePosition(const RenderObject* o) const
{
    return o->height() + o->marginTop();
}

bool RenderTheme::isControlContainer(EAppearance appearance) const
{
    // There are more leaves than this, but we'll patch this function as we add support for
    // more controls.
    return appearance != CheckboxAppearance && appearance != RadioAppearance;
}

bool RenderTheme::isControlStyled(const RenderStyle* style, const BorderData& border, const BackgroundLayer& background,
                                  const Color& backgroundColor) const
{
    switch (style->appearance()) {
        case PushButtonAppearance:
        case SquareButtonAppearance:
        case ButtonAppearance:
        case ListboxAppearance:
        case MenulistAppearance:
        // FIXME: Uncomment this when making search fields style-able.
        // case SearchFieldAppearance:
        case TextFieldAppearance:
        case TextAreaAppearance:
            // Test the style to see if the UA border and background match.
            return (style->border() != border ||
                    *style->backgroundLayers() != background ||
                    style->backgroundColor() != backgroundColor);
        default:
            return false;
    }
}

bool RenderTheme::supportsFocusRing(const RenderStyle* style) const
{
    return (style->hasAppearance() && style->appearance() != TextFieldAppearance && style->appearance() != TextAreaAppearance && style->appearance() != MenulistButtonAppearance && style->appearance() != ListboxAppearance);
}

bool RenderTheme::stateChanged(RenderObject* o, ControlState state) const
{
    // Default implementation assumes the controls dont respond to changes in :hover state
    if (state == HoverState && !supportsHover(o->style()))
        return false;

    // Assume pressed state is only responded to if the control is enabled.
    if (state == PressedState && !isEnabled(o))
        return false;

    // Repaint the control.
    o->repaint();
    return true;
}

bool RenderTheme::isChecked(const RenderObject* o) const
{
    if (!o->element())
        return false;
    return o->element()->isChecked();
}

bool RenderTheme::isIndeterminate(const RenderObject* o) const
{
    if (!o->element())
        return false;
    return o->element()->isIndeterminate();
}

bool RenderTheme::isEnabled(const RenderObject* o) const
{
    if (!o->element())
        return true;
    return o->element()->isEnabled();
}

bool RenderTheme::isFocused(const RenderObject* o) const
{
    Node* node = o->element();
    if (!node)
        return false;
    Document* document = node->document();
    Frame* frame = document->frame();
    return node == document->focusedNode() && frame && frame->isActive();
}

bool RenderTheme::isPressed(const RenderObject* o) const
{
    if (!o->element())
        return false;
    return o->element()->active();
}

bool RenderTheme::isReadOnlyControl(const RenderObject* o) const
{
    if (!o->element())
        return false;
    return o->element()->isReadOnlyControl();
}

bool RenderTheme::isHovered(const RenderObject* o) const
{
    if (!o->element())
        return false;
    return o->element()->hovered();
}

void RenderTheme::adjustCheckboxStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // A summary of the rules for checkbox designed to match WinIE:
    // width/height - honored (WinIE actually scales its control for small widths, but lets it overflow for small heights.)
    // font-size - not honored (control has no text), but we use it to decide which control size to use.
    setCheckboxSize(style);

    // padding - not honored by WinIE, needs to be removed.
    style->resetPadding();

    // border - honored by WinIE, but looks terrible (just paints in the control box and turns off the Windows XP theme)
    // for now, we will not honor it.
    style->resetBorder();

    style->setBoxShadow(0);
}

void RenderTheme::adjustRadioStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // A summary of the rules for checkbox designed to match WinIE:
    // width/height - honored (WinIE actually scales its control for small widths, but lets it overflow for small heights.)
    // font-size - not honored (control has no text), but we use it to decide which control size to use.
    setRadioSize(style);

    // padding - not honored by WinIE, needs to be removed.
    style->resetPadding();

    // border - honored by WinIE, but looks terrible (just paints in the control box and turns off the Windows XP theme)
    // for now, we will not honor it.
    style->resetBorder();

    style->setBoxShadow(0);
}

void RenderTheme::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // Most platforms will completely honor all CSS, and so we have no need to adjust the style
    // at all by default.  We will still allow the theme a crack at setting up a desired vertical size.
    setButtonSize(style);
}

void RenderTheme::adjustTextFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

void RenderTheme::adjustTextAreaStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

void RenderTheme::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

void RenderTheme::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

void RenderTheme::adjustSliderTrackStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

void RenderTheme::adjustSliderThumbStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

void RenderTheme::adjustSliderThumbSize(RenderObject*) const
{
}

void RenderTheme::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

void RenderTheme::adjustSearchFieldCancelButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

void RenderTheme::adjustSearchFieldDecorationStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

void RenderTheme::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

void RenderTheme::adjustSearchFieldResultsButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

void RenderTheme::platformColorsDidChange()
{
    m_activeSelectionColor = Color();
    m_inactiveSelectionColor = Color();
}

} // namespace WebCore
