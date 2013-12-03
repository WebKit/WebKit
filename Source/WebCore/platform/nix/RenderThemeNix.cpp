/*
 * Copyright (C) 2012, 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderThemeNix.h"

#include "HTMLMediaElement.h"
#include "InputTypeNames.h"
#include "PaintInfo.h"
#include "PlatformContextCairo.h"
#include "UserAgentStyleSheets.h"
#include "public/Canvas.h"
#include "public/Platform.h"
#include "public/Rect.h"
#include "public/ThemeEngine.h"
#if ENABLE(PROGRESS_ELEMENT)
#include "RenderProgress.h"
#endif
#if ENABLE(METER_ELEMENT)
#include "HTMLMeterElement.h"
#include "RenderMeter.h"
#endif

namespace WebCore {

static void setSizeIfAuto(RenderStyle* style, const IntSize& size)
{
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(size.width(), Fixed));
    if (style->height().isAuto())
        style->setHeight(Length(size.height(), Fixed));
}

static Nix::ThemeEngine* themeEngine()
{
    return Nix::Platform::current()->themeEngine();
}

static Nix::Canvas* webCanvas(const PaintInfo& info)
{
    return info.context->platformContext()->cr();
}

static IntSize toIntSize(const Nix::Size& size)
{
    return IntSize(size.width, size.height);
}

static Nix::Rect toNixRect(const IntRect& rect)
{
    return Nix::Rect(rect.x(), rect.y(), rect.width(), rect.height());
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page*)
{
    return RenderThemeNix::create();
}

PassRefPtr<RenderTheme> RenderThemeNix::create()
{
    return adoptRef(new RenderThemeNix);
}

RenderThemeNix::RenderThemeNix()
    : RenderTheme()
{
}

RenderThemeNix::~RenderThemeNix()
{

}

String RenderThemeNix::extraDefaultStyleSheet()
{
    return themeEngine()->extraDefaultStyleSheet();
}

String RenderThemeNix::extraQuirksStyleSheet()
{
    return themeEngine()->extraQuirksStyleSheet();
}

String RenderThemeNix::extraPlugInsStyleSheet()
{
    return themeEngine()->extraPlugInsStyleSheet();
}

Color RenderThemeNix::platformActiveSelectionBackgroundColor() const
{
    return themeEngine()->activeSelectionBackgroundColor().argb32();
}

Color RenderThemeNix::platformInactiveSelectionBackgroundColor() const
{
    return themeEngine()->inactiveSelectionBackgroundColor().argb32();
}

Color RenderThemeNix::platformActiveSelectionForegroundColor() const
{
    return themeEngine()->activeSelectionForegroundColor().argb32();
}

Color RenderThemeNix::platformInactiveSelectionForegroundColor() const
{
    return themeEngine()->inactiveSelectionForegroundColor().argb32();
}

Color RenderThemeNix::platformActiveListBoxSelectionBackgroundColor() const
{
    return themeEngine()->activeListBoxSelectionBackgroundColor().argb32();
}

Color RenderThemeNix::platformInactiveListBoxSelectionBackgroundColor() const
{
    return themeEngine()->inactiveListBoxSelectionBackgroundColor().argb32();
}

Color RenderThemeNix::platformActiveListBoxSelectionForegroundColor() const
{
    return themeEngine()->activeListBoxSelectionForegroundColor().argb32();
}

Color RenderThemeNix::platformInactiveListBoxSelectionForegroundColor() const
{
    return themeEngine()->inactiveListBoxSelectionForegroundColor().argb32();
}

Color RenderThemeNix::platformActiveTextSearchHighlightColor() const
{
    return themeEngine()->activeTextSearchHighlightColor().argb32();
}

Color RenderThemeNix::platformInactiveTextSearchHighlightColor() const
{
    return themeEngine()->inactiveTextSearchHighlightColor().argb32();
}

Color RenderThemeNix::platformFocusRingColor() const
{
    return themeEngine()->focusRingColor().argb32();
}

#if ENABLE(TOUCH_EVENTS)
Color RenderThemeNix::platformTapHighlightColor() const
{
    return themeEngine()->tapHighlightColor().argb32();
}
#endif

void RenderThemeNix::systemFont(WebCore::CSSValueID, FontDescription&) const
{
}

static Nix::ThemeEngine::State getWebThemeState(const RenderTheme* theme, const RenderObject* o)
{
    if (!theme->isEnabled(o))
        return Nix::ThemeEngine::StateDisabled;
    if (theme->isPressed(o))
        return Nix::ThemeEngine::StatePressed;
    if (theme->isHovered(o))
        return Nix::ThemeEngine::StateHover;

    return Nix::ThemeEngine::StateNormal;
}

bool RenderThemeNix::paintButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    Nix::ThemeEngine::ButtonExtraParams extraParams;
    extraParams.isDefault = isDefault(o);
    extraParams.hasBorder = true;
    themeEngine()->paintButton(webCanvas(i), getWebThemeState(this, o), toNixRect(rect), extraParams);
    return false;
}

bool RenderThemeNix::paintTextField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    // WebThemeEngine does not handle border rounded corner and background image
    // so return true to draw CSS border and background.
    if (o->style().hasBorderRadius() || o->style().hasBackgroundImage())
        return true;

    themeEngine()->paintTextField(webCanvas(i), getWebThemeState(this, o), toNixRect(rect));
    return false;
}

bool RenderThemeNix::paintTextArea(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintTextField(o, i, rect);
}

bool RenderThemeNix::paintCheckbox(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    Nix::ThemeEngine::ButtonExtraParams extraParams;
    extraParams.checked = isChecked(o);
    extraParams.indeterminate = isIndeterminate(o);

    themeEngine()->paintCheckbox(webCanvas(i), getWebThemeState(this, o), toNixRect(rect), extraParams);
    return false;
}

void RenderThemeNix::setCheckboxSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    IntSize size = toIntSize(themeEngine()->getCheckboxSize());
    setSizeIfAuto(style, size);
}

bool RenderThemeNix::paintRadio(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    Nix::ThemeEngine::ButtonExtraParams extraParams;
    extraParams.checked = isChecked(o);
    extraParams.indeterminate = isIndeterminate(o);

    themeEngine()->paintRadio(webCanvas(i), getWebThemeState(this, o), toNixRect(rect), extraParams);
    return false;
}

void RenderThemeNix::setRadioSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    IntSize size = toIntSize(themeEngine()->getRadioSize());
    setSizeIfAuto(style, size);
}

bool RenderThemeNix::paintMenuList(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    themeEngine()->paintMenuList(webCanvas(i), getWebThemeState(this, o), toNixRect(rect));
    return false;
}

void RenderThemeNix::adjustMenuListStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    style->resetBorder();
    style->setWhiteSpace(PRE);

    int paddingTop = 0;
    int paddingLeft = 0;
    int paddingBottom = 0;
    int paddingRight = 0;
    themeEngine()->getMenuListPadding(paddingTop, paddingLeft, paddingBottom, paddingRight);
    style->setPaddingTop(Length(paddingTop, Fixed));
    style->setPaddingRight(Length(paddingRight, Fixed));
    style->setPaddingBottom(Length(paddingBottom, Fixed));
    style->setPaddingLeft(Length(paddingLeft, Fixed));
}

#if ENABLE(PROGRESS_ELEMENT)
void RenderThemeNix::adjustProgressBarStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(nullptr);
}

bool RenderThemeNix::paintProgressBar(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    RenderProgress* renderProgress = toRenderProgress(o);
    Nix::ThemeEngine::ProgressBarExtraParams extraParams;
    extraParams.isDeterminate = renderProgress->isDeterminate();
    extraParams.position = renderProgress->position();
    extraParams.animationProgress = renderProgress->animationProgress();
    extraParams.animationStartTime = renderProgress->animationStartTime();
    themeEngine()->paintProgressBar(webCanvas(i), getWebThemeState(this, o), toNixRect(rect), extraParams);

    return false;
}

double RenderThemeNix::animationRepeatIntervalForProgressBar(RenderProgress*) const
{
    return themeEngine()->getAnimationRepeatIntervalForProgressBar();
}

double RenderThemeNix::animationDurationForProgressBar(RenderProgress*) const
{
    return themeEngine()->getAnimationDurationForProgressBar();
}
#endif

bool RenderThemeNix::paintSliderTrack(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    themeEngine()->paintSliderTrack(webCanvas(info), getWebThemeState(this, object), toNixRect(rect));
#if ENABLE(DATALIST_ELEMENT)
    paintSliderTicks(object, info, rect);
#endif
    return false;
}

void RenderThemeNix::adjustSliderTrackStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(nullptr);
}

bool RenderThemeNix::paintSliderThumb(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    themeEngine()->paintSliderThumb(webCanvas(info), getWebThemeState(this, object), toNixRect(rect));

    return false;
}

void RenderThemeNix::adjustSliderThumbStyle(StyleResolver* styleResolver, RenderStyle* style, Element* element) const
{
    RenderTheme::adjustSliderThumbStyle(styleResolver, style, element);
    style->setBoxShadow(nullptr);
}

const int SliderThumbWidth = 10;
const int SliderThumbHeight = 20;

void RenderThemeNix::adjustSliderThumbSize(RenderStyle* style, Element*) const
{
    ControlPart part = style->appearance();
    if (part == SliderThumbVerticalPart) {
        style->setWidth(Length(SliderThumbWidth, Fixed));
        style->setHeight(Length(SliderThumbHeight, Fixed));
    } else if (part == SliderThumbHorizontalPart) {
        style->setWidth(Length(SliderThumbWidth, Fixed));
        style->setHeight(Length(SliderThumbHeight, Fixed));
    }
}

#if ENABLE(DATALIST_ELEMENT)
IntSize RenderThemeNix::sliderTickSize() const
{
    return IntSize(1, 6);
}

int RenderThemeNix::sliderTickOffsetFromTrackCenter() const
{
    return -12;
}

LayoutUnit RenderThemeNix::sliderTickSnappingThreshold() const
{
    return 5;
}

bool RenderThemeNix::supportsDataListUI(const AtomicString& type) const
{
    return type == InputTypeNames::range();
}
#endif

void RenderThemeNix::adjustInnerSpinButtonStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    style->resetBorder();
    style->setWhiteSpace(PRE);

    int paddingTop = 0;
    int paddingLeft = 0;
    int paddingBottom = 0;
    int paddingRight = 0;
    themeEngine()->getInnerSpinButtonPadding(paddingTop, paddingLeft, paddingBottom, paddingRight);
    style->setPaddingTop(Length(paddingTop, Fixed));
    style->setPaddingRight(Length(paddingRight, Fixed));
    style->setPaddingBottom(Length(paddingBottom, Fixed));
    style->setPaddingLeft(Length(paddingLeft, Fixed));
}

bool RenderThemeNix::paintInnerSpinButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    Nix::ThemeEngine::InnerSpinButtonExtraParams extraParams;
    extraParams.spinUp = isSpinUpButtonPartPressed(o);
    extraParams.readOnly = isReadOnlyControl(o);

    themeEngine()->paintInnerSpinButton(webCanvas(i), getWebThemeState(this, o), toNixRect(rect), extraParams);
    return false;
}

#if ENABLE(METER_ELEMENT)
void RenderThemeNix::adjustMeterStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(nullptr);
}

IntSize RenderThemeNix::meterSizeForBounds(const RenderMeter*, const IntRect& bounds) const
{
    return bounds.size();
}

bool RenderThemeNix::supportsMeter(ControlPart part) const
{
    switch (part) {
    case RelevancyLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
    case MeterPart:
    case ContinuousCapacityLevelIndicatorPart:
        return true;
    default:
        return false;
    }
}

bool RenderThemeNix::paintMeter(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (!o->isMeter())
        return true;

    RenderMeter* renderMeter = toRenderMeter(o);
    HTMLMeterElement* e = renderMeter->meterElement();
    Nix::ThemeEngine::MeterExtraParams extraParams;
    extraParams.min = e->min();
    extraParams.max = e->max();
    extraParams.value = e->value();
    extraParams.low = e->low();
    extraParams.high = e->high();
    extraParams.optimum = e->optimum();

    themeEngine()->paintMeter(webCanvas(i), getWebThemeState(this, o), toNixRect(rect), extraParams);

    return false;
}
#endif

#if ENABLE(VIDEO)
String RenderThemeNix::extraMediaControlsStyleSheet()
{
    return String(mediaControlsNixUserAgentStyleSheet, sizeof(mediaControlsNixUserAgentStyleSheet));
}

bool RenderThemeNix::paintMediaPlayButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    auto state = toHTMLMediaElement(o->node()->shadowHost())->canPlay() ? Nix::ThemeEngine::StatePaused : Nix::ThemeEngine::StatePlaying;
    themeEngine()->paintMediaPlayButton(webCanvas(i), state, toNixRect(rect));
    return false;
}

bool RenderThemeNix::paintMediaMuteButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    auto state = toHTMLMediaElement(o->node()->shadowHost())->muted() ? Nix::ThemeEngine::StateMuted : Nix::ThemeEngine::StateNotMuted;
    themeEngine()->paintMediaMuteButton(webCanvas(i), state, toNixRect(rect));
    return false;
}

bool RenderThemeNix::paintMediaSeekBackButton(RenderObject*, const PaintInfo& i, const IntRect& rect)
{
    themeEngine()->paintMediaSeekBackButton(webCanvas(i), toNixRect(rect));
    return false;
}

bool RenderThemeNix::paintMediaSeekForwardButton(RenderObject*, const PaintInfo& i, const IntRect& rect)
{
    themeEngine()->paintMediaSeekForwardButton(webCanvas(i), toNixRect(rect));
    return false;
}

bool RenderThemeNix::paintMediaSliderTrack(RenderObject*, const PaintInfo&, const IntRect&)
{
    return false;
}

bool RenderThemeNix::paintMediaVolumeSliderContainer(RenderObject*, const PaintInfo&, const IntRect&)
{
    return false;
}

bool RenderThemeNix::paintMediaVolumeSliderTrack(RenderObject*, const PaintInfo&, const IntRect&)
{
    return false;
}

bool RenderThemeNix::paintMediaRewindButton(RenderObject*, const PaintInfo& i, const IntRect& rect)
{
    themeEngine()->paintMediaRewindButton(webCanvas(i), toNixRect(rect));
    return false;
}
#endif // ENABLE(VIDEO)

} // namespace WebCore

