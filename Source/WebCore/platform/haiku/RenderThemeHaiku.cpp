/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 *               2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderThemeHaiku.h"

#include "GraphicsContext.h"
#include "InputTypeNames.h"
#include "NotImplemented.h"
#include "PaintInfo.h"
#include "RenderElement.h"
#include "UserAgentScripts.h"
#include "UserAgentStyleSheets.h"
#include <ControlLook.h>
#include <View.h>

#include <wtf/text/StringBuilder.h>


namespace WebCore {

static const int sliderThumbWidth = 15;
static const int sliderThumbHeight = 17;

RenderTheme& RenderTheme::singleton()
{
	static NeverDestroyed<RenderThemeHaiku> theme;
	return theme;
}

RenderThemeHaiku::RenderThemeHaiku()
{
}

RenderThemeHaiku::~RenderThemeHaiku()
{
}

bool RenderThemeHaiku::supportsFocusRing(const RenderStyle& style) const
{
    switch (style.appearance()) {
    case PushButtonPart:
    case ButtonPart:
    case TextFieldPart:
    case TextAreaPart:
    case SearchFieldPart:
    case MenulistPart:
    case RadioPart:
    case CheckboxPart:
        return true;
    default:
        return false;
    }
}

bool RenderThemeHaiku::paintSliderTrack(const RenderObject& object, const PaintInfo& info, const IntRect& intRect)
{
    rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    rgb_color background = base;
    	// TODO: From PaintInfo?
    BRect rect = intRect;
    BView* view = info.context().platformContext();
    unsigned flags = flagsForObject(object);
    if (isPressed(object))
    	flags |= BControlLook::B_ACTIVATED;
    if (isDefault(object))
    	flags |= BControlLook::B_DEFAULT_BUTTON;
    be_control_look->DrawSliderBar(view, rect, rect, base, background, flags,
        object.style().appearance() == SliderHorizontalPart ?
            B_HORIZONTAL : B_VERTICAL);

#if ENABLE(DATALIST_ELEMENT)
    paintSliderTicks(object, info, intRect);
#endif

    return false;
}

void RenderThemeHaiku::adjustSliderTrackStyle(RenderStyle& style, const Element*) const
{
    style.setBoxShadow(nullptr);
}

void RenderThemeHaiku::adjustSliderThumbStyle(RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustSliderThumbStyle(style, element);
    style.setBoxShadow(nullptr);
}

void RenderThemeHaiku::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    ControlPart part = style.appearance();
    if (part == SliderThumbVerticalPart) {
        style.setWidth(Length(sliderThumbHeight, Fixed));
        style.setHeight(Length(sliderThumbWidth, Fixed));
    } else if (part == SliderThumbHorizontalPart) {
        style.setWidth(Length(sliderThumbWidth, Fixed));
        style.setHeight(Length(sliderThumbHeight, Fixed));
    }
}

#if ENABLE(DATALIST_ELEMENT)
IntSize RenderThemeHaiku::sliderTickSize() const
{
    return IntSize(1, 6);
}

int RenderThemeHaiku::sliderTickOffsetFromTrackCenter() const
{
    static const int sliderTickOffset = -(sliderThumbHeight / 2 + 1);

    return sliderTickOffset;
}

#endif

bool RenderThemeHaiku::supportsDataListUI(const AtomString& type) const
{
#if ENABLE(DATALIST_ELEMENT)
    // FIXME: We need to support other types.
    return type == InputTypeNames::email()
        || type == InputTypeNames::range()
        || type == InputTypeNames::color()
        || type == InputTypeNames::search()
        || type == InputTypeNames::url();
#else
    UNUSED_PARAM(type);
    return false;
#endif
}

bool RenderThemeHaiku::paintSliderThumb(const RenderObject& object, const PaintInfo& info, const IntRect& intRect)
{
    rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    BRect rect = intRect;
    BView* view = info.context().platformContext();
    unsigned flags = flagsForObject(object);
    if (isPressed(object))
    	flags |= BControlLook::B_ACTIVATED;
    if (isDefault(object))
    	flags |= BControlLook::B_DEFAULT_BUTTON;
    be_control_look->DrawSliderThumb(view, rect, rect, base, flags,
        object.style().appearance() == SliderHorizontalPart ?
            B_HORIZONTAL : B_VERTICAL);

    return false;
}


void RenderThemeHaiku::updateCachedSystemFontDescription(CSSValueID, FontCascadeDescription& fontDescription) const
{
    fontDescription.setOneFamily("Sans");
    fontDescription.setSpecifiedSize(14);
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setWeight(normalWeightValue());
    fontDescription.setIsItalic(false);
}

#if ENABLE(VIDEO)
String RenderThemeHaiku::mediaControlsStyleSheet()
{
    return ASCIILiteral::fromLiteralUnsafe(mediaControlsBaseUserAgentStyleSheet);
}

String RenderThemeHaiku::mediaControlsScript()
{
    StringBuilder scriptBuilder;
    scriptBuilder.appendCharacters(mediaControlsLocalizedStringsJavaScript, sizeof(mediaControlsLocalizedStringsJavaScript));
    scriptBuilder.appendCharacters(mediaControlsBaseJavaScript, sizeof(mediaControlsBaseJavaScript));
    return scriptBuilder.toString();
}
#endif

#if !USE(NEW_THEME)
bool RenderThemeHaiku::paintCheckbox(const RenderObject& object, const PaintInfo& info, const IntRect& intRect)
{
    if (info.context().paintingDisabled())
        return true;

    if (!be_control_look)
        return true;

    rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    BRect rect = intRect;
    BView* view = info.context().platformContext();
    unsigned flags = flagsForObject(object);

	view->PushState();
    be_control_look->DrawCheckBox(view, rect, rect, base, flags);
	view->PopState();
    return false;
}

void RenderThemeHaiku::setCheckboxSize(RenderStyle& style) const
{
    int size = 14;

    // If the width and height are both specified, then we have nothing to do.
    if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
        return;

    // FIXME: A hard-coded size of 'size' is used. This is wrong but necessary for now.
    if (style.width().isIntrinsicOrAuto())
        style.setWidth(Length(size, Fixed));

    if (style.height().isAuto())
        style.setHeight(Length(size, Fixed));
}

bool RenderThemeHaiku::paintRadio(const RenderObject& object, const PaintInfo& info,
	const IntRect& intRect)
{
    if (info.context().paintingDisabled())
        return true;

    if (!be_control_look)
        return true;

    rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    BRect rect = intRect;
    BView* view = info.context().platformContext();
    unsigned flags = flagsForObject(object);

    view->PushState();
    be_control_look->DrawRadioButton(view, rect, rect, base, flags);
    view->PopState();
    return false;
}

void RenderThemeHaiku::setRadioSize(RenderStyle& style) const
{
    // This is the same as checkboxes.
    setCheckboxSize(style);
}

bool RenderThemeHaiku::paintButton(const RenderObject& object, const PaintInfo& info, const IntRect& intRect)
{
    if (info.context().paintingDisabled())
        return true;

    if (!be_control_look)
        return true;

    rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    rgb_color background = base;
        // TODO: From PaintInfo?
    BRect rect = intRect;
    BView* view = info.context().platformContext();
    unsigned flags = flagsForObject(object);
    if (isPressed(object))
        flags |= BControlLook::B_ACTIVATED;
    if (isDefault(object))
        flags |= BControlLook::B_DEFAULT_BUTTON;

    view->PushState();
    be_control_look->DrawButtonFrame(view, rect, rect, base, background, flags);
    be_control_look->DrawButtonBackground(view, rect, rect, base, flags);
    view->PopState();
    return false;
}
#endif

void RenderThemeHaiku::adjustTextFieldStyle(RenderStyle&, const Element*) const
{
}

bool RenderThemeHaiku::paintTextField(const RenderObject& object, const PaintInfo& info, const FloatRect& intRect)
{
    if (info.context().paintingDisabled())
        return true;

    if (!be_control_look)
        return true;

    rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    //rgb_color background = base;
        // TODO: From PaintInfo?
    BRect rect(intRect);
    BView* view(info.context().platformContext());
    unsigned flags = flagsForObject(object) & ~BControlLook::B_CLICKED;

    view->PushState();
    be_control_look->DrawTextControlBorder(view, rect, rect, base, flags);
    view->PopState();
    return false;
}

void RenderThemeHaiku::adjustTextAreaStyle(RenderStyle& style, const Element* element) const
{
	adjustTextFieldStyle(style, element);
}

bool RenderThemeHaiku::paintTextArea(const RenderObject& object, const PaintInfo& info, const FloatRect& intRect)
{
    return paintTextField(object, info, intRect);
}

void RenderThemeHaiku::adjustMenuListStyle(RenderStyle& style, const Element* element) const
{
    adjustMenuListButtonStyle(style, element);
}

void RenderThemeHaiku::adjustMenuListButtonStyle(RenderStyle& style, const Element* element) const
{
    style.resetBorder();
    style.resetBorderRadius();

    int labelSpacing = be_control_look ? static_cast<int>(be_control_look->DefaultLabelSpacing()) : 3;
    // Position the text correctly within the select box and make the box wide enough to fit the dropdown button
    style.setPaddingTop(Length(3, Fixed));
    style.setPaddingLeft(Length(3 + labelSpacing, Fixed));
    style.setPaddingRight(Length(22, Fixed));
    style.setPaddingBottom(Length(3, Fixed));

    // Height is locked to auto
    style.setHeight(Length(Auto));

    // Calculate our min-height
    const int menuListButtonMinHeight = 20;
    int minHeight = style.computedFontSize();
    minHeight = std::max(minHeight, menuListButtonMinHeight);

    style.setMinHeight(Length(minHeight, Fixed));
}

void RenderThemeHaiku::paintMenuListButtonDecorations(const RenderBox& object, const PaintInfo& info, const FloatRect& floatRect)
{
    if (info.context().paintingDisabled())
        return;

    if (!be_control_look)
        return;

    rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
        // TODO get the color from PaintInfo?
    BRect rect = floatRect;
    BView* view = info.context().platformContext();
    unsigned flags = BControlLook::B_BLEND_FRAME;
        // TODO unfortunately we don't get access to the RenderObject here so
        // we can't use flagsForObject(object) & ~BControlLook::B_CLICKED;

    view->PushState();
    be_control_look->DrawMenuFieldFrame(view, rect, rect, base, base, flags);
    be_control_look->DrawMenuFieldBackground(view, rect, rect, base, true, flags);
    view->PopState();
}

bool RenderThemeHaiku::paintMenuList(const RenderObject& object, const PaintInfo& info, const FloatRect& intRect)
{
    // This is never called: the list is handled natively as a BMenu.
    return true;
}

unsigned RenderThemeHaiku::flagsForObject(const RenderObject& object) const
{
    unsigned flags = BControlLook::B_BLEND_FRAME;
    if (!isEnabled(object))
    	flags |= BControlLook::B_DISABLED;
    if (isFocused(object))
    	flags |= BControlLook::B_FOCUSED;
    if (isPressed(object))
    	flags |= BControlLook::B_CLICKED;
    if (isChecked(object))
    	flags |= BControlLook::B_ACTIVATED;
	return flags;
}

} // namespace WebCore
