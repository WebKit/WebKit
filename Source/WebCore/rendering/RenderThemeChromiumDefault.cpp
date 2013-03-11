/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008, 2009 Google Inc.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
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
 *
 */

#include "config.h"
#include "RenderThemeChromiumDefault.h"

#include "CSSValueKeywords.h"
#include "Color.h"
#include "PaintInfo.h"
#include "PlatformContextSkia.h"
#include "RenderObject.h"
#include "RenderProgress.h"
#include "RenderSlider.h"
#include "ScrollbarTheme.h"
#include "UserAgentStyleSheets.h"
#include <public/Platform.h>
#include <public/WebRect.h>
#include <public/default/WebThemeEngine.h>

namespace WebCore {

unsigned RenderThemeChromiumDefault::m_activeSelectionBackgroundColor =
    0xff1e90ff;
unsigned RenderThemeChromiumDefault::m_activeSelectionForegroundColor =
    Color::black;
unsigned RenderThemeChromiumDefault::m_inactiveSelectionBackgroundColor =
    0xffc8c8c8;
unsigned RenderThemeChromiumDefault::m_inactiveSelectionForegroundColor =
    0xff323232;

double RenderThemeChromiumDefault::m_caretBlinkInterval;

static const unsigned defaultButtonBackgroundColor = 0xffdddddd;

static WebKit::WebThemeEngine::State getWebThemeState(const RenderTheme* theme, const RenderObject* o)
{
    if (!theme->isEnabled(o))
        return WebKit::WebThemeEngine::StateDisabled;
    if (theme->isPressed(o))
        return WebKit::WebThemeEngine::StatePressed;
    if (theme->isHovered(o))
        return WebKit::WebThemeEngine::StateHover;

    return WebKit::WebThemeEngine::StateNormal;
}

PassRefPtr<RenderTheme> RenderThemeChromiumDefault::create()
{
    return adoptRef(new RenderThemeChromiumDefault());
}

// RenderTheme::themeForPage for Android is defined in RenderThemeChromiumAndroid.cpp.
#if !OS(ANDROID)
PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemeChromiumDefault::create().leakRef();
    return rt;
}
#endif

RenderThemeChromiumDefault::RenderThemeChromiumDefault()
{
    m_caretBlinkInterval = RenderTheme::caretBlinkInterval();
}

RenderThemeChromiumDefault::~RenderThemeChromiumDefault()
{
}

Color RenderThemeChromiumDefault::systemColor(int cssValueId) const
{
    static const Color defaultButtonGrayColor(0xffdddddd);
    static const Color defaultMenuColor(0xfff7f7f7);

    if (cssValueId == CSSValueButtonface)
        return defaultButtonGrayColor;
    if (cssValueId == CSSValueMenu)
        return defaultMenuColor;
    return RenderTheme::systemColor(cssValueId);
}

String RenderThemeChromiumDefault::extraDefaultStyleSheet()
{
#if !OS(WINDOWS)
    return RenderThemeChromiumSkia::extraDefaultStyleSheet() +
           String(themeChromiumLinuxUserAgentStyleSheet, sizeof(themeChromiumLinuxUserAgentStyleSheet));
#else
    return RenderThemeChromiumSkia::extraDefaultStyleSheet();
#endif
}

bool RenderThemeChromiumDefault::controlSupportsTints(const RenderObject* o) const
{
    return isEnabled(o);
}

Color RenderThemeChromiumDefault::activeListBoxSelectionBackgroundColor() const
{
    return Color(0x28, 0x28, 0x28);
}

Color RenderThemeChromiumDefault::activeListBoxSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeChromiumDefault::inactiveListBoxSelectionBackgroundColor() const
{
    return Color(0xc8, 0xc8, 0xc8);
}

Color RenderThemeChromiumDefault::inactiveListBoxSelectionForegroundColor() const
{
    return Color(0x32, 0x32, 0x32);
}

Color RenderThemeChromiumDefault::platformActiveSelectionBackgroundColor() const
{
    return m_activeSelectionBackgroundColor;
}

Color RenderThemeChromiumDefault::platformInactiveSelectionBackgroundColor() const
{
    return m_inactiveSelectionBackgroundColor;
}

Color RenderThemeChromiumDefault::platformActiveSelectionForegroundColor() const
{
    return m_activeSelectionForegroundColor;
}

Color RenderThemeChromiumDefault::platformInactiveSelectionForegroundColor() const
{
    return m_inactiveSelectionForegroundColor;
}

#if ENABLE(DATALIST_ELEMENT)
IntSize RenderThemeChromiumDefault::sliderTickSize() const
{
    return IntSize(1, 6);
}

int RenderThemeChromiumDefault::sliderTickOffsetFromTrackCenter() const
{
    return -16;
}
#endif

void RenderThemeChromiumDefault::adjustSliderThumbSize(RenderStyle* style, Element* element) const
{
    IntSize size = WebKit::Platform::current()->themeEngine()->getSize(WebKit::WebThemeEngine::PartSliderThumb);

    if (style->appearance() == SliderThumbHorizontalPart) {
        style->setWidth(Length(size.width(), Fixed));
        style->setHeight(Length(size.height(), Fixed));
    } else if (style->appearance() == SliderThumbVerticalPart) {
        style->setWidth(Length(size.height(), Fixed));
        style->setHeight(Length(size.width(), Fixed));
    } else
        RenderThemeChromiumSkia::adjustSliderThumbSize(style, element);
}

bool RenderThemeChromiumDefault::supportsControlTints() const
{
    return true;
}

void RenderThemeChromiumDefault::setCaretBlinkInterval(double interval)
{
    m_caretBlinkInterval = interval;
}

double RenderThemeChromiumDefault::caretBlinkIntervalInternal() const
{
    return m_caretBlinkInterval;
}

void RenderThemeChromiumDefault::setSelectionColors(
    unsigned activeBackgroundColor,
    unsigned activeForegroundColor,
    unsigned inactiveBackgroundColor,
    unsigned inactiveForegroundColor)
{
    m_activeSelectionBackgroundColor = activeBackgroundColor;
    m_activeSelectionForegroundColor = activeForegroundColor;
    m_inactiveSelectionBackgroundColor = inactiveBackgroundColor;
    m_inactiveSelectionForegroundColor = inactiveForegroundColor;
}

bool RenderThemeChromiumDefault::paintCheckbox(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    WebKit::WebThemeEngine::ExtraParams extraParams;
    WebKit::WebCanvas* canvas = i.context->platformContext()->canvas();
    extraParams.button.checked = isChecked(o);
    extraParams.button.indeterminate = isIndeterminate(o);

    WebKit::Platform::current()->themeEngine()->paint(canvas, WebKit::WebThemeEngine::PartCheckbox, getWebThemeState(this, o), WebKit::WebRect(rect), &extraParams);
    return false;
}

void RenderThemeChromiumDefault::setCheckboxSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    IntSize size = WebKit::Platform::current()->themeEngine()->getSize(WebKit::WebThemeEngine::PartCheckbox);
    setSizeIfAuto(style, size);
}

bool RenderThemeChromiumDefault::paintRadio(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    WebKit::WebThemeEngine::ExtraParams extraParams;
    WebKit::WebCanvas* canvas = i.context->platformContext()->canvas();
    extraParams.button.checked = isChecked(o);

    WebKit::Platform::current()->themeEngine()->paint(canvas, WebKit::WebThemeEngine::PartRadio, getWebThemeState(this, o), WebKit::WebRect(rect), &extraParams);
    return false;
}

void RenderThemeChromiumDefault::setRadioSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    IntSize size = WebKit::Platform::current()->themeEngine()->getSize(WebKit::WebThemeEngine::PartRadio);
    setSizeIfAuto(style, size);
}

bool RenderThemeChromiumDefault::paintButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    WebKit::WebThemeEngine::ExtraParams extraParams;
    WebKit::WebCanvas* canvas = i.context->platformContext()->canvas();
    extraParams.button.isDefault = isDefault(o);
    extraParams.button.hasBorder = true;
    extraParams.button.backgroundColor = defaultButtonBackgroundColor;
    if (o->hasBackground())
        extraParams.button.backgroundColor = o->style()->visitedDependentColor(CSSPropertyBackgroundColor).rgb();

    WebKit::Platform::current()->themeEngine()->paint(canvas, WebKit::WebThemeEngine::PartButton, getWebThemeState(this, o), WebKit::WebRect(rect), &extraParams);
    return false;
}

bool RenderThemeChromiumDefault::paintTextField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    // WebThemeEngine does not handle border rounded corner and background image
    // so return true to draw CSS border and background.
    if (o->style()->hasBorderRadius() || o->style()->hasBackgroundImage())
        return true;

    ControlPart part = o->style()->appearance();

    WebKit::WebThemeEngine::ExtraParams extraParams;
    extraParams.textField.isTextArea = part == TextAreaPart;
    extraParams.textField.isListbox = part == ListboxPart;

    WebKit::WebCanvas* canvas = i.context->platformContext()->canvas();

    // Fallback to white if the specified color object is invalid.
    Color backgroundColor(Color::white);
    if (o->style()->visitedDependentColor(CSSPropertyBackgroundColor).isValid())
        backgroundColor = o->style()->visitedDependentColor(CSSPropertyBackgroundColor);
    extraParams.textField.backgroundColor = backgroundColor.rgb();

    WebKit::Platform::current()->themeEngine()->paint(canvas, WebKit::WebThemeEngine::PartTextField, getWebThemeState(this, o), WebKit::WebRect(rect), &extraParams);
    return false;
}

bool RenderThemeChromiumDefault::paintMenuList(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (!o->isBox())
        return false;

    const int right = rect.x() + rect.width();
    const int middle = rect.y() + rect.height() / 2;

    WebKit::WebThemeEngine::ExtraParams extraParams;
    extraParams.menuList.arrowX = (o->style()->direction() == RTL) ? rect.x() + 7 : right - 13;
    extraParams.menuList.arrowY = middle;
    const RenderBox* box = toRenderBox(o);
    // Match Chromium Win behaviour of showing all borders if any are shown.
    extraParams.menuList.hasBorder = box->borderRight() || box->borderLeft() || box->borderTop() || box->borderBottom();
    extraParams.menuList.hasBorderRadius = o->style()->hasBorderRadius();
    // Fallback to transparent if the specified color object is invalid.
    extraParams.menuList.backgroundColor = Color::transparent;
    if (o->hasBackground())
        extraParams.menuList.backgroundColor = o->style()->visitedDependentColor(CSSPropertyBackgroundColor).rgb();

    WebKit::WebCanvas* canvas = i.context->platformContext()->canvas();

    WebKit::Platform::current()->themeEngine()->paint(canvas, WebKit::WebThemeEngine::PartMenuList, getWebThemeState(this, o), WebKit::WebRect(rect), &extraParams);
    return false;
}

bool RenderThemeChromiumDefault::paintSliderTrack(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    WebKit::WebThemeEngine::ExtraParams extraParams;
    WebKit::WebCanvas* canvas = i.context->platformContext()->canvas();
    extraParams.slider.vertical = o->style()->appearance() == SliderVerticalPart;

    WebKit::Platform::current()->themeEngine()->paint(canvas, WebKit::WebThemeEngine::PartSliderTrack, getWebThemeState(this, o), WebKit::WebRect(rect), &extraParams);

#if ENABLE(DATALIST_ELEMENT)
    paintSliderTicks(o, i, rect);
#endif

    return false;
}

bool RenderThemeChromiumDefault::paintSliderThumb(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    WebKit::WebThemeEngine::ExtraParams extraParams;
    WebKit::WebCanvas* canvas = i.context->platformContext()->canvas();
    extraParams.slider.vertical = o->style()->appearance() == SliderThumbVerticalPart;
    extraParams.slider.inDrag = isPressed(o);

    WebKit::Platform::current()->themeEngine()->paint(canvas, WebKit::WebThemeEngine::PartSliderThumb, getWebThemeState(this, o), WebKit::WebRect(rect), &extraParams);
    return false;
}

void RenderThemeChromiumDefault::adjustInnerSpinButtonStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    IntSize size = WebKit::Platform::current()->themeEngine()->getSize(WebKit::WebThemeEngine::PartInnerSpinButton);

    style->setWidth(Length(size.width(), Fixed));
    style->setMinWidth(Length(size.width(), Fixed));
}

bool RenderThemeChromiumDefault::paintInnerSpinButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    WebKit::WebThemeEngine::ExtraParams extraParams;
    WebKit::WebCanvas* canvas = i.context->platformContext()->canvas();
    extraParams.innerSpin.spinUp = (controlStatesForRenderer(o) & SpinUpState);
    extraParams.innerSpin.readOnly = isReadOnlyControl(o);

    WebKit::Platform::current()->themeEngine()->paint(canvas, WebKit::WebThemeEngine::PartInnerSpinButton, getWebThemeState(this, o), WebKit::WebRect(rect), &extraParams);
    return false;
}

#if ENABLE(PROGRESS_ELEMENT)

bool RenderThemeChromiumDefault::paintProgressBar(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (!o->isProgress())
        return true;

    RenderProgress* renderProgress = toRenderProgress(o);
    IntRect valueRect = progressValueRectFor(renderProgress, rect);

    WebKit::WebThemeEngine::ExtraParams extraParams;
    extraParams.progressBar.determinate = renderProgress->isDeterminate();
    extraParams.progressBar.valueRectX = valueRect.x();
    extraParams.progressBar.valueRectY = valueRect.y();
    extraParams.progressBar.valueRectWidth = valueRect.width();
    extraParams.progressBar.valueRectHeight = valueRect.height();

    DirectionFlippingScope scope(o, i, rect);
    WebKit::WebCanvas* canvas = i.context->platformContext()->canvas();
    WebKit::Platform::current()->themeEngine()->paint(canvas, WebKit::WebThemeEngine::PartProgressBar, getWebThemeState(this, o), WebKit::WebRect(rect), &extraParams);
    return false;
}

#endif

bool RenderThemeChromiumDefault::shouldOpenPickerWithF4Key() const
{
    return true;
}

} // namespace WebCore
