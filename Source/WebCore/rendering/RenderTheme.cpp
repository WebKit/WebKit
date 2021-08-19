/*
 * Copyright (C) 2005-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "CSSValueKeywords.h"
#include "ColorBlending.h"
#include "ColorLuminance.h"
#include "ControlStates.h"
#include "Document.h"
#include "FileList.h"
#include "FloatConversion.h"
#include "FloatRoundedRect.h"
#include "FocusController.h"
#include "FontSelector.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "LocalizedStrings.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderMeter.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "SpinButtonElement.h"
#include "StringTruncator.h"
#include "TextControlInnerElements.h"
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringConcatenateNumbers.h>


#if ENABLE(DATALIST_ELEMENT)
#include "HTMLDataListElement.h"
#include "HTMLOptionElement.h"
#endif

#if USE(NEW_THEME)
#include "Theme.h"
#endif

namespace WebCore {

using namespace HTMLNames;

static Color& customFocusRingColor()
{
    static NeverDestroyed<Color> color;
    return color;
}

RenderTheme::RenderTheme()
{
}

void RenderTheme::adjustStyle(RenderStyle& style, const Element* element, const RenderStyle* userAgentAppearanceStyle)
{
    // Force inline and table display styles to be inline-block (except for table- which is block)
    ControlPart part = style.appearance();
    if (style.display() == DisplayType::Inline || style.display() == DisplayType::InlineTable || style.display() == DisplayType::TableRowGroup
        || style.display() == DisplayType::TableHeaderGroup || style.display() == DisplayType::TableFooterGroup
        || style.display() == DisplayType::TableRow || style.display() == DisplayType::TableColumnGroup || style.display() == DisplayType::TableColumn
        || style.display() == DisplayType::TableCell || style.display() == DisplayType::TableCaption)
        style.setEffectiveDisplay(DisplayType::InlineBlock);
    else if (style.display() == DisplayType::ListItem || style.display() == DisplayType::Table)
        style.setEffectiveDisplay(DisplayType::Block);

    if (userAgentAppearanceStyle && isControlStyled(style, *userAgentAppearanceStyle)) {
        switch (part) {
        case MenulistPart:
            style.setAppearance(MenulistButtonPart);
            part = MenulistButtonPart;
            break;
        default:
            style.setAppearance(NoControlPart);
            break;
        }
    }

    if (!style.hasAppearance())
        return;

    if (!supportsBoxShadow(style))
        style.setBoxShadow(nullptr);
    
#if USE(NEW_THEME)
    switch (part) {
    case CheckboxPart:
    case InnerSpinButtonPart:
    case RadioPart:
    case PushButtonPart:
    case SquareButtonPart:
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
#endif
    case DefaultButtonPart:
    case ButtonPart: {
        // Border
        LengthBox borderBox(style.borderTopWidth(), style.borderRightWidth(), style.borderBottomWidth(), style.borderLeftWidth());
        borderBox = Theme::singleton().controlBorder(part, style.fontCascade(), borderBox, style.effectiveZoom());
        if (borderBox.top().value() != static_cast<int>(style.borderTopWidth())) {
            if (borderBox.top().value())
                style.setBorderTopWidth(borderBox.top().value());
            else
                style.resetBorderTop();
        }
        if (borderBox.right().value() != static_cast<int>(style.borderRightWidth())) {
            if (borderBox.right().value())
                style.setBorderRightWidth(borderBox.right().value());
            else
                style.resetBorderRight();
        }
        if (borderBox.bottom().value() != static_cast<int>(style.borderBottomWidth())) {
            style.setBorderBottomWidth(borderBox.bottom().value());
            if (borderBox.bottom().value())
                style.setBorderBottomWidth(borderBox.bottom().value());
            else
                style.resetBorderBottom();
        }
        if (borderBox.left().value() != static_cast<int>(style.borderLeftWidth())) {
            style.setBorderLeftWidth(borderBox.left().value());
            if (borderBox.left().value())
                style.setBorderLeftWidth(borderBox.left().value());
            else
                style.resetBorderLeft();
        }

        // Padding
        LengthBox paddingBox = Theme::singleton().controlPadding(part, style.fontCascade(), style.paddingBox(), style.effectiveZoom());
        if (paddingBox != style.paddingBox())
            style.setPaddingBox(WTFMove(paddingBox));

        // Whitespace
        if (Theme::singleton().controlRequiresPreWhiteSpace(part))
            style.setWhiteSpace(WhiteSpace::Pre);

        // Width / Height
        // The width and height here are affected by the zoom.
        // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
        LengthSize controlSize = Theme::singleton().controlSize(part, style.fontCascade(), { style.width(), style.height() }, style.effectiveZoom());
        if (controlSize.width != style.width())
            style.setWidth(WTFMove(controlSize.width));
        if (controlSize.height != style.height())
            style.setHeight(WTFMove(controlSize.height));

        // Min-Width / Min-Height
        LengthSize minControlSize = Theme::singleton().minimumControlSize(part, style.fontCascade(), { style.minWidth(), style.minHeight() }, { style.width(), style.height() }, style.effectiveZoom());
        if (minControlSize.width != style.minWidth())
            style.setMinWidth(WTFMove(minControlSize.width));
        if (minControlSize.height != style.minHeight())
            style.setMinHeight(WTFMove(minControlSize.height));

        // Font
        if (auto themeFont = Theme::singleton().controlFont(part, style.fontCascade(), style.effectiveZoom())) {
            // If overriding the specified font with the theme font, also override the line height with the standard line height.
            style.setLineHeight(RenderStyle::initialLineHeight());
            if (style.setFontDescription(WTFMove(themeFont.value())))
                style.fontCascade().update(nullptr);
        }

        // Special style that tells enabled default buttons in active windows to use the ActiveButtonText color.
        // The active window part of the test has to be done at paint time since it's not triggered by a style change.
        style.setInsideDefaultButton(part == DefaultButtonPart && element && !element->isDisabledFormControl());
        break;
    }
    default:
        break;
    }
#endif

    // Call the appropriate style adjustment method based off the appearance value.
    switch (style.appearance()) {
#if !USE(NEW_THEME)
    case CheckboxPart:
        return adjustCheckboxStyle(style, element);
    case RadioPart:
        return adjustRadioStyle(style, element);
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
        return adjustColorWellStyle(style, element);
#endif
    case PushButtonPart:
    case SquareButtonPart:
    case DefaultButtonPart:
    case ButtonPart:
        return adjustButtonStyle(style, element);
    case InnerSpinButtonPart:
        return adjustInnerSpinButtonStyle(style, element);
#endif
    case TextFieldPart:
        return adjustTextFieldStyle(style, element);
    case TextAreaPart:
        return adjustTextAreaStyle(style, element);
    case MenulistPart:
        return adjustMenuListStyle(style, element);
    case MenulistButtonPart:
        return adjustMenuListButtonStyle(style, element);
    case MediaPlayButtonPart:
    case MediaCurrentTimePart:
    case MediaTimeRemainingPart:
    case MediaEnterFullscreenButtonPart:
    case MediaExitFullscreenButtonPart:
    case MediaMuteButtonPart:
    case MediaVolumeSliderContainerPart:
        return adjustMediaControlStyle(style, element);
    case MediaSliderPart:
    case MediaVolumeSliderPart:
    case MediaFullScreenVolumeSliderPart:
    case SliderHorizontalPart:
    case SliderVerticalPart:
        return adjustSliderTrackStyle(style, element);
    case SliderThumbHorizontalPart:
    case SliderThumbVerticalPart:
        return adjustSliderThumbStyle(style, element);
    case SearchFieldPart:
        return adjustSearchFieldStyle(style, element);
    case SearchFieldCancelButtonPart:
        return adjustSearchFieldCancelButtonStyle(style, element);
    case SearchFieldDecorationPart:
    case SearchFieldResultsDecorationPart:
    case SearchFieldResultsButtonPart:
        return adjustSearchFieldDecorationStyle(style, element);
    case ProgressBarPart:
        return adjustProgressBarStyle(style, element);
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
        return adjustMeterStyle(style, element);
    case CapsLockIndicatorPart:
        return adjustCapsLockIndicatorStyle(style, element);
#if ENABLE(APPLE_PAY)
    case ApplePayButtonPart:
        return adjustApplePayButtonStyle(style, element);
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
    case AttachmentPart:
    case BorderlessAttachmentPart:
        return adjustAttachmentStyle(style, element);
#endif
#if ENABLE(DATALIST_ELEMENT)
    case ListButtonPart:
        return adjustListButtonStyle(style, element);
#endif
    default:
        break;
    }
}

void RenderTheme::adjustSearchFieldDecorationStyle(RenderStyle& style, const Element* element) const
{
    if (is<SearchFieldResultsButtonElement>(element) && !downcast<SearchFieldResultsButtonElement>(*element).canAdjustStyleForAppearance()) {
        style.setAppearance(NoControlPart);
        return;
    }

    switch (style.appearance()) {
    case SearchFieldDecorationPart:
        return adjustSearchFieldDecorationPartStyle(style, element);
    case SearchFieldResultsDecorationPart:
        return adjustSearchFieldResultsDecorationPartStyle(style, element);
    case SearchFieldResultsButtonPart:
        return adjustSearchFieldResultsButtonStyle(style, element);
    default:
        break;
    }
}

bool RenderTheme::paint(const RenderBox& box, ControlStates& controlStates, const PaintInfo& paintInfo, const LayoutRect& rect)
{
    // If painting is disabled, but we aren't updating control tints, then just bail.
    // If we are updating control tints, just schedule a repaint if the theme supports tinting
    // for that control.
    if (paintInfo.context().invalidatingControlTints()) {
        if (controlSupportsTints(box))
            box.repaint();
        return false;
    }
    if (paintInfo.context().paintingDisabled())
        return false;

    if (UNLIKELY(!canPaint(paintInfo, box.settings())))
        return false;

    ControlPart part = box.style().appearance();
    IntRect integralSnappedRect = snappedIntRect(rect);
    float deviceScaleFactor = box.document().deviceScaleFactor();
    FloatRect devicePixelSnappedRect = snapRectToDevicePixels(rect, deviceScaleFactor);
    
#if USE(NEW_THEME)
    float pageScaleFactor = box.page().pageScaleFactor();

    switch (part) {
    case CheckboxPart:
    case RadioPart:
    case PushButtonPart:
    case SquareButtonPart:
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
#endif
    case DefaultButtonPart:
    case ButtonPart:
    case InnerSpinButtonPart:
        updateControlStatesForRenderer(box, controlStates);
        Theme::singleton().paint(part, controlStates, paintInfo.context(), devicePixelSnappedRect, box.style().effectiveZoom(), &box.view().frameView(), deviceScaleFactor, pageScaleFactor, box.document().useSystemAppearance(), box.useDarkAppearance());
        return false;
    default:
        break;
    }
#else
    UNUSED_PARAM(controlStates);
#endif

    // Call the appropriate paint method based off the appearance value.
    switch (part) {
#if !USE(NEW_THEME)
    case CheckboxPart:
        return paintCheckbox(box, paintInfo, devicePixelSnappedRect);
    case RadioPart:
        return paintRadio(box, paintInfo, devicePixelSnappedRect);
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
        return paintColorWell(box, paintInfo, integralSnappedRect);
#endif
    case PushButtonPart:
    case SquareButtonPart:
    case DefaultButtonPart:
    case ButtonPart:
        return paintButton(box, paintInfo, integralSnappedRect);
    case InnerSpinButtonPart:
        return paintInnerSpinButton(box, paintInfo, integralSnappedRect);
#endif
    case MenulistPart:
        return paintMenuList(box, paintInfo, devicePixelSnappedRect);
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
        return paintMeter(box, paintInfo, integralSnappedRect);
    case ProgressBarPart:
        return paintProgressBar(box, paintInfo, integralSnappedRect);
    case SliderHorizontalPart:
    case SliderVerticalPart:
        return paintSliderTrack(box, paintInfo, integralSnappedRect);
    case SliderThumbHorizontalPart:
    case SliderThumbVerticalPart:
        return paintSliderThumb(box, paintInfo, integralSnappedRect);
    case MediaEnterFullscreenButtonPart:
    case MediaExitFullscreenButtonPart:
        return paintMediaFullscreenButton(box, paintInfo, integralSnappedRect);
    case MediaPlayButtonPart:
        return paintMediaPlayButton(box, paintInfo, integralSnappedRect);
    case MediaOverlayPlayButtonPart:
        return paintMediaOverlayPlayButton(box, paintInfo, integralSnappedRect);
    case MediaMuteButtonPart:
        return paintMediaMuteButton(box, paintInfo, integralSnappedRect);
    case MediaSeekBackButtonPart:
        return paintMediaSeekBackButton(box, paintInfo, integralSnappedRect);
    case MediaSeekForwardButtonPart:
        return paintMediaSeekForwardButton(box, paintInfo, integralSnappedRect);
    case MediaRewindButtonPart:
        return paintMediaRewindButton(box, paintInfo, integralSnappedRect);
    case MediaReturnToRealtimeButtonPart:
        return paintMediaReturnToRealtimeButton(box, paintInfo, integralSnappedRect);
    case MediaToggleClosedCaptionsButtonPart:
        return paintMediaToggleClosedCaptionsButton(box, paintInfo, integralSnappedRect);
    case MediaSliderPart:
        return paintMediaSliderTrack(box, paintInfo, integralSnappedRect);
    case MediaSliderThumbPart:
        return paintMediaSliderThumb(box, paintInfo, integralSnappedRect);
    case MediaVolumeSliderMuteButtonPart:
        return paintMediaMuteButton(box, paintInfo, integralSnappedRect);
    case MediaVolumeSliderContainerPart:
        return paintMediaVolumeSliderContainer(box, paintInfo, integralSnappedRect);
    case MediaVolumeSliderPart:
        return paintMediaVolumeSliderTrack(box, paintInfo, integralSnappedRect);
    case MediaVolumeSliderThumbPart:
        return paintMediaVolumeSliderThumb(box, paintInfo, integralSnappedRect);
    case MediaFullScreenVolumeSliderPart:
        return paintMediaFullScreenVolumeSliderTrack(box, paintInfo, integralSnappedRect);
    case MediaFullScreenVolumeSliderThumbPart:
        return paintMediaFullScreenVolumeSliderThumb(box, paintInfo, integralSnappedRect);
    case MediaTimeRemainingPart:
        return paintMediaTimeRemaining(box, paintInfo, integralSnappedRect);
    case MediaCurrentTimePart:
        return paintMediaCurrentTime(box, paintInfo, integralSnappedRect);
    case MediaControlsBackgroundPart:
        return paintMediaControlsBackground(box, paintInfo, integralSnappedRect);
    case MenulistButtonPart:
    case TextFieldPart:
    case TextAreaPart:
    case ListboxPart:
        return true;
    case SearchFieldPart:
        return paintSearchField(box, paintInfo, integralSnappedRect);
    case SearchFieldCancelButtonPart:
        return paintSearchFieldCancelButton(box, paintInfo, integralSnappedRect);
    case SearchFieldDecorationPart:
        return paintSearchFieldDecorationPart(box, paintInfo, integralSnappedRect);
    case SearchFieldResultsDecorationPart:
        return paintSearchFieldResultsDecorationPart(box, paintInfo, integralSnappedRect);
    case SearchFieldResultsButtonPart:
        return paintSearchFieldResultsButton(box, paintInfo, integralSnappedRect);
    case CapsLockIndicatorPart:
        return paintCapsLockIndicator(box, paintInfo, integralSnappedRect);
#if ENABLE(APPLE_PAY)
    case ApplePayButtonPart:
        return paintApplePayButton(box, paintInfo, integralSnappedRect);
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
    case AttachmentPart:
    case BorderlessAttachmentPart:
        return paintAttachment(box, paintInfo, integralSnappedRect);
#endif
    default:
        break;
    }

    return true; // We don't support the appearance, so let the normal background/border paint.
}

bool RenderTheme::paintBorderOnly(const RenderBox& box, const PaintInfo& paintInfo, const LayoutRect& rect)
{
    if (paintInfo.context().paintingDisabled())
        return false;

#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(rect);
    return box.style().appearance() != NoControlPart;
#else
    FloatRect devicePixelSnappedRect = snapRectToDevicePixels(rect, box.document().deviceScaleFactor());
    // Call the appropriate paint method based off the appearance value.
    switch (box.style().appearance()) {
    case TextFieldPart:
        return paintTextField(box, paintInfo, devicePixelSnappedRect);
    case ListboxPart:
    case TextAreaPart:
        return paintTextArea(box, paintInfo, devicePixelSnappedRect);
    case MenulistButtonPart:
    case SearchFieldPart:
        return true;
    case CheckboxPart:
    case RadioPart:
    case PushButtonPart:
    case SquareButtonPart:
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
#endif
    case DefaultButtonPart:
    case ButtonPart:
    case MenulistPart:
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
    case ProgressBarPart:
    case SliderHorizontalPart:
    case SliderVerticalPart:
    case SliderThumbHorizontalPart:
    case SliderThumbVerticalPart:
    case SearchFieldCancelButtonPart:
    case SearchFieldDecorationPart:
    case SearchFieldResultsDecorationPart:
    case SearchFieldResultsButtonPart:
    default:
        break;
    }

    return false;
#endif
}

void RenderTheme::paintDecorations(const RenderBox& box, const PaintInfo& paintInfo, const LayoutRect& rect)
{
    if (paintInfo.context().paintingDisabled())
        return;

    // FIXME: Investigate whether all controls can use a device-pixel-snapped rect
    // rather than an integral-snapped rect.

    IntRect integralSnappedRect = snappedIntRect(rect);
    FloatRect devicePixelSnappedRect = snapRectToDevicePixels(rect, box.document().deviceScaleFactor());

    // Call the appropriate paint method based off the appearance value.
    switch (box.style().appearance()) {
    case MenulistButtonPart:
        paintMenuListButtonDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case TextFieldPart:
        paintTextFieldDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case TextAreaPart:
        paintTextAreaDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case CheckboxPart:
        paintCheckboxDecorations(box, paintInfo, integralSnappedRect);
        break;
    case RadioPart:
        paintRadioDecorations(box, paintInfo, integralSnappedRect);
        break;
    case PushButtonPart:
        paintPushButtonDecorations(box, paintInfo, integralSnappedRect);
        break;
    case SquareButtonPart:
        paintSquareButtonDecorations(box, paintInfo, integralSnappedRect);
        break;
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
        paintColorWellDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
#endif
    case ButtonPart:
        paintButtonDecorations(box, paintInfo, integralSnappedRect);
        break;
    case MenulistPart:
        paintMenuListDecorations(box, paintInfo, integralSnappedRect);
        break;
    case SliderThumbHorizontalPart:
    case SliderThumbVerticalPart:
        paintSliderThumbDecorations(box, paintInfo, integralSnappedRect);
        break;
    case SearchFieldPart:
        paintSearchFieldDecorations(box, paintInfo, integralSnappedRect);
        break;
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
    case ProgressBarPart:
    case SliderHorizontalPart:
    case SliderVerticalPart:
    case ListboxPart:
    case DefaultButtonPart:
    case SearchFieldCancelButtonPart:
    case SearchFieldDecorationPart:
    case SearchFieldResultsDecorationPart:
    case SearchFieldResultsButtonPart:
    default:
        break;
    }
}

#if ENABLE(VIDEO)

String RenderTheme::formatMediaControlsTime(float time) const
{
    if (!std::isfinite(time))
        time = 0;
    // FIXME: Seems like it would be better to use std::lround here.
    int seconds = static_cast<int>(std::abs(time));
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;
    if (hours)
        return makeString((time < 0 ? "-" : ""), hours, ':', pad('0', 2, minutes), ':', pad('0', 2, seconds));
    return makeString((time < 0 ? "-" : ""), pad('0', 2, minutes), ':', pad('0', 2, seconds));
}

String RenderTheme::formatMediaControlsCurrentTime(float currentTime, float /*duration*/) const
{
    return formatMediaControlsTime(currentTime);
}

String RenderTheme::formatMediaControlsRemainingTime(float currentTime, float duration) const
{
    return formatMediaControlsTime(currentTime - duration);
}

LayoutPoint RenderTheme::volumeSliderOffsetFromMuteButton(const RenderBox& muteButtonBox, const LayoutSize& size) const
{
    LayoutUnit y = -size.height();
    FloatPoint absPoint = muteButtonBox.localToAbsolute(FloatPoint(muteButtonBox.offsetLeft(), y), { IsFixed, UseTransforms });
    if (absPoint.y() < 0)
        y = muteButtonBox.height();
    return LayoutPoint(0_lu, y);
}

#endif

Color RenderTheme::activeSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
    auto& cache = colorCache(options);
    if (!cache.activeSelectionBackgroundColor.isValid())
        cache.activeSelectionBackgroundColor = transformSelectionBackgroundColor(platformActiveSelectionBackgroundColor(options), options);
    return cache.activeSelectionBackgroundColor;
}

Color RenderTheme::inactiveSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
    auto& cache = colorCache(options);
    if (!cache.inactiveSelectionBackgroundColor.isValid())
        cache.inactiveSelectionBackgroundColor = transformSelectionBackgroundColor(platformInactiveSelectionBackgroundColor(options), options);
    return cache.inactiveSelectionBackgroundColor;
}

Color RenderTheme::transformSelectionBackgroundColor(const Color& color, OptionSet<StyleColor::Options>) const
{
    return blendWithWhite(color);
}

Color RenderTheme::activeSelectionForegroundColor(OptionSet<StyleColor::Options> options) const
{
    auto& cache = colorCache(options);
    if (!cache.activeSelectionForegroundColor.isValid() && supportsSelectionForegroundColors(options))
        cache.activeSelectionForegroundColor = platformActiveSelectionForegroundColor(options);
    return cache.activeSelectionForegroundColor;
}

Color RenderTheme::inactiveSelectionForegroundColor(OptionSet<StyleColor::Options> options) const
{
    auto& cache = colorCache(options);
    if (!cache.inactiveSelectionForegroundColor.isValid() && supportsSelectionForegroundColors(options))
        cache.inactiveSelectionForegroundColor = platformInactiveSelectionForegroundColor(options);
    return cache.inactiveSelectionForegroundColor;
}

Color RenderTheme::activeListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
    auto& cache = colorCache(options);
    if (!cache.activeListBoxSelectionBackgroundColor.isValid())
        cache.activeListBoxSelectionBackgroundColor = platformActiveListBoxSelectionBackgroundColor(options);
    return cache.activeListBoxSelectionBackgroundColor;
}

Color RenderTheme::inactiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
    auto& cache = colorCache(options);
    if (!cache.inactiveListBoxSelectionBackgroundColor.isValid())
        cache.inactiveListBoxSelectionBackgroundColor = platformInactiveListBoxSelectionBackgroundColor(options);
    return cache.inactiveListBoxSelectionBackgroundColor;
}

Color RenderTheme::activeListBoxSelectionForegroundColor(OptionSet<StyleColor::Options> options) const
{
    auto& cache = colorCache(options);
    if (!cache.activeListBoxSelectionForegroundColor.isValid() && supportsListBoxSelectionForegroundColors(options))
        cache.activeListBoxSelectionForegroundColor = platformActiveListBoxSelectionForegroundColor(options);
    return cache.activeListBoxSelectionForegroundColor;
}

Color RenderTheme::inactiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options> options) const
{
    auto& cache = colorCache(options);
    if (!cache.inactiveListBoxSelectionForegroundColor.isValid() && supportsListBoxSelectionForegroundColors(options))
        cache.inactiveListBoxSelectionForegroundColor = platformInactiveListBoxSelectionForegroundColor(options);
    return cache.inactiveListBoxSelectionForegroundColor;
}

Color RenderTheme::platformActiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    // Use a blue color by default if the platform theme doesn't define anything.
    return Color::blue;
}

Color RenderTheme::platformActiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const
{
    // Use a white color by default if the platform theme doesn't define anything.
    return Color::white;
}

Color RenderTheme::platformInactiveSelectionBackgroundColor(OptionSet<StyleColor::Options>) const
{
    // Use a grey color by default if the platform theme doesn't define anything.
    // This color matches Firefox's inactive color.
    return SRGBA<uint8_t> { 176, 176, 176 };
}

Color RenderTheme::platformInactiveSelectionForegroundColor(OptionSet<StyleColor::Options>) const
{
    // Use a black color by default.
    return Color::black;
}

Color RenderTheme::platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
    return platformActiveSelectionBackgroundColor(options);
}

Color RenderTheme::platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options> options) const
{
    return platformActiveSelectionForegroundColor(options);
}

Color RenderTheme::platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColor::Options> options) const
{
    return platformInactiveSelectionBackgroundColor(options);
}

Color RenderTheme::platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColor::Options> options) const
{
    return platformInactiveSelectionForegroundColor(options);
}

int RenderTheme::baselinePosition(const RenderBox& box) const
{
#if USE(NEW_THEME)
    return box.height() + box.marginTop() + Theme::singleton().baselinePositionAdjustment(box.style().appearance()) * box.style().effectiveZoom();
#else
    return box.height() + box.marginTop();
#endif
}

bool RenderTheme::isControlContainer(ControlPart appearance) const
{
    // There are more leaves than this, but we'll patch this function as we add support for
    // more controls.
    return appearance != CheckboxPart && appearance != RadioPart;
}

bool RenderTheme::isControlStyled(const RenderStyle& style, const RenderStyle& userAgentStyle) const
{
    switch (style.appearance()) {
    case PushButtonPart:
    case SquareButtonPart:
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
#endif
    case DefaultButtonPart:
    case ButtonPart:
    case ListboxPart:
    case MenulistPart:
    case ProgressBarPart:
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
    // FIXME: SearchFieldPart should be included here when making search fields style-able.
    case TextFieldPart:
    case TextAreaPart:
        // Test the style to see if the UA border and background match.
        return style.border() != userAgentStyle.border()
            || style.backgroundLayers() != userAgentStyle.backgroundLayers()
            || !style.backgroundColorEqualsToColorIgnoringVisited(userAgentStyle.backgroundColor());
    default:
        return false;
    }
}

void RenderTheme::adjustRepaintRect(const RenderObject& renderer, FloatRect& rect)
{
#if USE(NEW_THEME)
    ControlStates states(extractControlStatesForRenderer(renderer));
    Theme::singleton().inflateControlPaintRect(renderer.style().appearance(), states, rect, renderer.style().effectiveZoom());
#else
    UNUSED_PARAM(renderer);
    UNUSED_PARAM(rect);
#endif
}

bool RenderTheme::supportsFocusRing(const RenderStyle& style) const
{
    return (style.hasAppearance() && style.appearance() != TextFieldPart && style.appearance() != TextAreaPart && style.appearance() != MenulistButtonPart && style.appearance() != ListboxPart);
}

bool RenderTheme::stateChanged(const RenderObject& o, ControlStates::States state) const
{
    // Default implementation assumes the controls don't respond to changes in :hover state
    if (state == ControlStates::States::Hovered && !supportsHover(o.style()))
        return false;

    // Assume pressed state is only responded to if the control is enabled.
    if (state == ControlStates::States::Pressed && !isEnabled(o))
        return false;

    // Repaint the control.
    o.repaint();
    return true;
}

void RenderTheme::updateControlStatesForRenderer(const RenderBox& box, ControlStates& controlStates) const
{
    ControlStates newStates = extractControlStatesForRenderer(box);
    controlStates.setStates(newStates.states());
    if (isFocused(box))
        controlStates.setTimeSinceControlWasFocused(box.page().focusController().timeSinceFocusWasSet());
}

OptionSet<ControlStates::States> RenderTheme::extractControlStatesForRenderer(const RenderObject& o) const
{
    OptionSet<ControlStates::States> states;
    if (isHovered(o)) {
        states.add(ControlStates::States::Hovered);
        if (isSpinUpButtonPartHovered(o))
            states.add(ControlStates::States::SpinUp);
    }
    if (isPressed(o)) {
        states.add(ControlStates::States::Pressed);
        if (isSpinUpButtonPartPressed(o))
            states.add(ControlStates::States::SpinUp);
    }
    if (isFocused(o) && o.style().outlineStyleIsAuto() == OutlineIsAuto::On)
        states.add(ControlStates::States::Focused);
    if (isEnabled(o))
        states.add(ControlStates::States::Enabled);
    if (isChecked(o))
        states.add(ControlStates::States::Checked);
    if (isDefault(o))
        states.add(ControlStates::States::Default);
    if (!isActive(o))
        states.add(ControlStates::States::WindowInactive);
    if (isIndeterminate(o))
        states.add(ControlStates::States::Indeterminate);
    if (isPresenting(o))
        states.add(ControlStates::States::Presenting);
    return states;
}

bool RenderTheme::isActive(const RenderObject& renderer) const
{
    return renderer.page().focusController().isActive();
}

bool RenderTheme::isChecked(const RenderObject& o) const
{
    return is<HTMLInputElement>(o.node()) && downcast<HTMLInputElement>(*o.node()).shouldAppearChecked();
}

bool RenderTheme::isIndeterminate(const RenderObject& o) const
{
    return is<HTMLInputElement>(o.node()) && downcast<HTMLInputElement>(*o.node()).shouldAppearIndeterminate();
}

bool RenderTheme::isEnabled(const RenderObject& renderer) const
{
    Node* node = renderer.node();
    if (!is<Element>(node))
        return true;
    return !downcast<Element>(*node).isDisabledFormControl();
}

bool RenderTheme::isFocused(const RenderObject& renderer) const
{
    Node* node = renderer.node();
    if (!is<Element>(node))
        return false;

    auto focusDelegate = downcast<Element>(*node).focusDelegate();
    Document& document = focusDelegate->document();
    Frame* frame = document.frame();
    return focusDelegate == document.focusedElement() && frame && frame->selection().isFocusedAndActive();
}

bool RenderTheme::isPressed(const RenderObject& renderer) const
{
    if (!is<Element>(renderer.node()))
        return false;
    return downcast<Element>(*renderer.node()).active();
}

bool RenderTheme::isSpinUpButtonPartPressed(const RenderObject& renderer) const
{
    Node* node = renderer.node();
    if (!is<Element>(node))
        return false;
    Element& element = downcast<Element>(*node);
    if (!element.active() || !is<SpinButtonElement>(element))
        return false;
    return downcast<SpinButtonElement>(element).upDownState() == SpinButtonElement::Up;
}

bool RenderTheme::isReadOnlyControl(const RenderObject& renderer) const
{
    Node* node = renderer.node();
    if (!is<HTMLFormControlElement>(node))
        return false;
    return !downcast<Element>(*node).matchesReadWritePseudoClass();
}

bool RenderTheme::isHovered(const RenderObject& renderer) const
{
    Node* node = renderer.node();
    if (!is<Element>(node))
        return false;
    Element& element = downcast<Element>(*node);
    if (!is<SpinButtonElement>(element))
        return element.hovered();
    SpinButtonElement& spinButton = downcast<SpinButtonElement>(element);
    return spinButton.hovered() && spinButton.upDownState() != SpinButtonElement::Indeterminate;
}

bool RenderTheme::isSpinUpButtonPartHovered(const RenderObject& renderer) const
{
    Node* node = renderer.node();
    if (!is<SpinButtonElement>(node))
        return false;
    return downcast<SpinButtonElement>(*node).upDownState() == SpinButtonElement::Up;
}

bool RenderTheme::isPresenting(const RenderObject& o) const
{
    return is<HTMLInputElement>(o.node()) && downcast<HTMLInputElement>(*o.node()).isPresentingAttachedView();
}

bool RenderTheme::isDefault(const RenderObject& o) const
{
    // A button should only have the default appearance if the page is active
    if (!isActive(o))
        return false;

    return o.style().appearance() == DefaultButtonPart;
}

#if !USE(NEW_THEME)

void RenderTheme::adjustCheckboxStyle(RenderStyle& style, const Element*) const
{
    // A summary of the rules for checkbox designed to match WinIE:
    // width/height - honored (WinIE actually scales its control for small widths, but lets it overflow for small heights.)
    // font-size - not honored (control has no text), but we use it to decide which control size to use.
    setCheckboxSize(style);

    // padding - not honored by WinIE, needs to be removed.
    style.resetPadding();

    // border - honored by WinIE, but looks terrible (just paints in the control box and turns off the Windows XP theme)
    // for now, we will not honor it.
    style.resetBorder();

    style.setBoxShadow(nullptr);
}

void RenderTheme::adjustRadioStyle(RenderStyle& style, const Element*) const
{
    // A summary of the rules for checkbox designed to match WinIE:
    // width/height - honored (WinIE actually scales its control for small widths, but lets it overflow for small heights.)
    // font-size - not honored (control has no text), but we use it to decide which control size to use.
    setRadioSize(style);

    // padding - not honored by WinIE, needs to be removed.
    style.resetPadding();

    // border - honored by WinIE, but looks terrible (just paints in the control box and turns off the Windows XP theme)
    // for now, we will not honor it.
    style.resetBorder();

    style.setBoxShadow(nullptr);
}

void RenderTheme::adjustButtonStyle(RenderStyle&, const Element*) const
{
}

void RenderTheme::adjustInnerSpinButtonStyle(RenderStyle&, const Element*) const
{
}

#if ENABLE(INPUT_TYPE_COLOR)

void RenderTheme::adjustColorWellStyle(RenderStyle& style, const Element* element) const
{
    adjustButtonStyle(style, element);
}

bool RenderTheme::paintColorWell(const RenderObject& box, const PaintInfo& paintInfo, const IntRect& rect)
{
    return paintButton(box, paintInfo, rect);
}

#endif

#endif

void RenderTheme::adjustTextFieldStyle(RenderStyle&, const Element*) const
{
}

void RenderTheme::adjustTextAreaStyle(RenderStyle&, const Element*) const
{
}

void RenderTheme::adjustMenuListStyle(RenderStyle&, const Element*) const
{
}

void RenderTheme::adjustMeterStyle(RenderStyle& style, const Element*) const
{
    style.setBoxShadow(nullptr);
}

IntSize RenderTheme::meterSizeForBounds(const RenderMeter&, const IntRect& bounds) const
{
    return bounds.size();
}

bool RenderTheme::supportsMeter(ControlPart, const HTMLMeterElement&) const
{
    return false;
}

bool RenderTheme::paintMeter(const RenderObject&, const PaintInfo&, const IntRect&)
{
    return true;
}

#if ENABLE(INPUT_TYPE_COLOR)
void RenderTheme::paintColorWellDecorations(const RenderObject& box, const PaintInfo& paintInfo, const FloatRect& rect)
{
    paintButtonDecorations(box, paintInfo, snappedIntRect(LayoutRect(rect)));
}
#endif

void RenderTheme::adjustCapsLockIndicatorStyle(RenderStyle&, const Element*) const
{
}

bool RenderTheme::paintCapsLockIndicator(const RenderObject&, const PaintInfo&, const IntRect&)
{
    return false;
}

#if ENABLE(ATTACHMENT_ELEMENT)

void RenderTheme::adjustAttachmentStyle(RenderStyle&, const Element*) const
{
}

bool RenderTheme::paintAttachment(const RenderObject&, const PaintInfo&, const IntRect&)
{
    return false;
}

#endif

#if ENABLE(INPUT_TYPE_COLOR)

String RenderTheme::colorInputStyleSheet(const Settings&) const
{
    return "input[type=\"color\"] { -webkit-appearance: color-well; width: 44px; height: 23px; outline: none; } "_s;
}

#endif // ENABLE(INPUT_TYPE_COLOR)

#if ENABLE(DATALIST_ELEMENT)

String RenderTheme::dataListStyleSheet() const
{
    ASSERT(RuntimeEnabledFeatures::sharedFeatures().dataListElementEnabled());
    return "datalist { display: none; }"_s;
}

void RenderTheme::adjustListButtonStyle(RenderStyle&, const Element*) const
{
}

LayoutUnit RenderTheme::sliderTickSnappingThreshold() const
{
    return 0;
}

void RenderTheme::paintSliderTicks(const RenderObject& o, const PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!is<HTMLInputElement>(o.node()))
        return;

    auto& input = downcast<HTMLInputElement>(*o.node());
    if (!input.isRangeControl())
        return;

    auto dataList = input.dataList();
    if (!dataList)
        return;

    double min = input.minimum();
    double max = input.maximum();
    ControlPart part = o.style().appearance();
    // We don't support ticks on alternate sliders like MediaVolumeSliders.
    if (part !=  SliderHorizontalPart && part != SliderVerticalPart)
        return;
    bool isHorizontal = part ==  SliderHorizontalPart;

    IntSize thumbSize;
    const RenderObject* thumbRenderer = input.sliderThumbElement()->renderer();
    if (thumbRenderer) {
        const RenderStyle& thumbStyle = thumbRenderer->style();
        int thumbWidth = thumbStyle.width().intValue();
        int thumbHeight = thumbStyle.height().intValue();
        thumbSize.setWidth(isHorizontal ? thumbWidth : thumbHeight);
        thumbSize.setHeight(isHorizontal ? thumbHeight : thumbWidth);
    }

    IntSize tickSize = sliderTickSize();
    float zoomFactor = o.style().effectiveZoom();
    FloatRect tickRect;
    int tickRegionSideMargin = 0;
    int tickRegionWidth = 0;
    IntRect trackBounds;
    RenderObject* trackRenderer = input.sliderTrackElement()->renderer();
    // We can ignoring transforms because transform is handled by the graphics context.
    if (trackRenderer)
        trackBounds = trackRenderer->absoluteBoundingBoxRectIgnoringTransforms();
    IntRect sliderBounds = o.absoluteBoundingBoxRectIgnoringTransforms();

    // Make position relative to the transformed ancestor element.
    trackBounds.setX(trackBounds.x() - sliderBounds.x() + rect.x());
    trackBounds.setY(trackBounds.y() - sliderBounds.y() + rect.y());

    if (isHorizontal) {
        tickRect.setWidth(floor(tickSize.width() * zoomFactor));
        tickRect.setHeight(floor(tickSize.height() * zoomFactor));
        tickRect.setY(floor(rect.y() + rect.height() / 2.0 + sliderTickOffsetFromTrackCenter() * zoomFactor));
        tickRegionSideMargin = trackBounds.x() + (thumbSize.width() - tickSize.width() * zoomFactor) / 2.0;
        tickRegionWidth = trackBounds.width() - thumbSize.width();
    } else {
        tickRect.setWidth(floor(tickSize.height() * zoomFactor));
        tickRect.setHeight(floor(tickSize.width() * zoomFactor));
        tickRect.setX(floor(rect.x() + rect.width() / 2.0 + sliderTickOffsetFromTrackCenter() * zoomFactor));
        tickRegionSideMargin = trackBounds.y() + (thumbSize.width() - tickSize.width() * zoomFactor) / 2.0;
        tickRegionWidth = trackBounds.height() - thumbSize.width();
    }
    GraphicsContextStateSaver stateSaver(paintInfo.context());
    paintInfo.context().setFillColor(o.style().visitedDependentColorWithColorFilter(CSSPropertyColor));
    for (auto& optionElement : dataList->suggestions()) {
        if (auto optionValue = input.listOptionValueAsDouble(optionElement)) {
            double tickFraction = (*optionValue - min) / (max - min);
            double tickRatio = isHorizontal && o.style().isLeftToRightDirection() ? tickFraction : 1.0 - tickFraction;
            double tickPosition = round(tickRegionSideMargin + tickRegionWidth * tickRatio);
            if (isHorizontal)
                tickRect.setX(tickPosition);
            else
                tickRect.setY(tickPosition);
            paintInfo.context().fillRect(tickRect);
        }
    }
}

#endif

Seconds RenderTheme::animationRepeatIntervalForProgressBar(const RenderProgress&) const
{
    return 0_s;
}

Seconds RenderTheme::animationDurationForProgressBar(const RenderProgress&) const
{
    return 0_s;
}

void RenderTheme::adjustProgressBarStyle(RenderStyle&, const Element*) const
{
}

IntRect RenderTheme::progressBarRectForBounds(const RenderObject&, const IntRect& bounds) const
{
    return bounds;
}

bool RenderTheme::shouldHaveSpinButton(const HTMLInputElement& inputElement) const
{
    return inputElement.isSteppable() && !inputElement.isRangeControl();
}

bool RenderTheme::shouldHaveCapsLockIndicator(const HTMLInputElement&) const
{
    return false;
}

void RenderTheme::adjustMenuListButtonStyle(RenderStyle&, const Element*) const
{
}

void RenderTheme::adjustMediaControlStyle(RenderStyle&, const Element*) const
{
}

void RenderTheme::adjustSliderTrackStyle(RenderStyle&, const Element*) const
{
}

void RenderTheme::adjustSliderThumbStyle(RenderStyle& style, const Element* element) const
{
    adjustSliderThumbSize(style, element);
}

void RenderTheme::adjustSliderThumbSize(RenderStyle&, const Element*) const
{
}

void RenderTheme::adjustSearchFieldStyle(RenderStyle&, const Element*) const
{
}

void RenderTheme::adjustSearchFieldCancelButtonStyle(RenderStyle&, const Element*) const
{
}

void RenderTheme::adjustSearchFieldDecorationPartStyle(RenderStyle&, const Element*) const
{
}

void RenderTheme::adjustSearchFieldResultsDecorationPartStyle(RenderStyle&, const Element*) const
{
}

void RenderTheme::adjustSearchFieldResultsButtonStyle(RenderStyle&, const Element*) const
{
}

void RenderTheme::purgeCaches()
{
    m_colorCacheMap.clear();
}

void RenderTheme::platformColorsDidChange()
{
    m_colorCacheMap.clear();

    Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
}

auto RenderTheme::colorCache(OptionSet<StyleColor::Options> options) const -> ColorCache&
{
    auto optionsIgnoringVisitedLink = options;
    optionsIgnoringVisitedLink.remove(StyleColor::Options::ForVisitedLink);

    return m_colorCacheMap.ensure(optionsIgnoringVisitedLink.toRaw(), [] {
        return ColorCache();
    }).iterator->value;
}

FontCascadeDescription& RenderTheme::cachedSystemFontDescription(CSSValueID systemFontID) const
{
    static auto fontDescriptions = makeNeverDestroyed<std::array<FontCascadeDescription, 10>>({ });

    switch (systemFontID) {
    case CSSValueCaption:
        return fontDescriptions.get()[0];
    case CSSValueIcon:
        return fontDescriptions.get()[1];
    case CSSValueMenu:
        return fontDescriptions.get()[2];
    case CSSValueMessageBox:
        return fontDescriptions.get()[3];
    case CSSValueSmallCaption:
        return fontDescriptions.get()[4];
    case CSSValueStatusBar:
        return fontDescriptions.get()[5];
    case CSSValueWebkitMiniControl:
        return fontDescriptions.get()[6];
    case CSSValueWebkitSmallControl:
        return fontDescriptions.get()[7];
    case CSSValueWebkitControl:
        return fontDescriptions.get()[8];
    case CSSValueNone:
        return fontDescriptions.get()[9];
    default:
        ASSERT_NOT_REACHED();
        return fontDescriptions.get()[9];
    }
}

void RenderTheme::systemFont(CSSValueID systemFontID, FontCascadeDescription& fontDescription) const
{
    fontDescription = cachedSystemFontDescription(systemFontID);
    if (fontDescription.isAbsoluteSize())
        return;

    updateCachedSystemFontDescription(systemFontID, fontDescription);
}

Color RenderTheme::systemColor(CSSValueID cssValueId, OptionSet<StyleColor::Options> options) const
{
    switch (cssValueId) {
    case CSSValueWebkitLink:
        return options.contains(StyleColor::Options::ForVisitedLink) ? SRGBA<uint8_t> { 85, 26, 139 } : SRGBA<uint8_t> { 0, 0, 238 };
    case CSSValueWebkitActivelink:
    case CSSValueActivetext:
        return Color::red;
    case CSSValueLinktext:
        return SRGBA<uint8_t> { 0, 0, 238 };
    case CSSValueVisitedtext:
        return SRGBA<uint8_t> { 85, 26, 139 };
    case CSSValueActiveborder:
        return Color::white;
    case CSSValueActivebuttontext:
        return Color::black;
    case CSSValueActivecaption:
        return SRGBA<uint8_t> { 204, 204, 204 };
    case CSSValueAppworkspace:
        return Color::white;
    case CSSValueBackground:
        return SRGBA<uint8_t> { 99, 99, 206 };
    case CSSValueButtonface:
        return Color::lightGray;
    case CSSValueButtonhighlight:
        return SRGBA<uint8_t> { 221, 221, 221 };
    case CSSValueButtonshadow:
        return SRGBA<uint8_t> { 136, 136, 136 };
    case CSSValueButtontext:
        return Color::black;
    case CSSValueCaptiontext:
        return Color::black;
    case CSSValueCanvas:
        return Color::white;
    case CSSValueCanvastext:
        return Color::black;
    case CSSValueField:
        return Color::white;
    case CSSValueFieldtext:
        return Color::black;
    case CSSValueGraytext:
        return Color::darkGray;
    case CSSValueHighlight:
        return SRGBA<uint8_t> { 181, 213, 255 };
    case CSSValueHighlighttext:
        return Color::black;
    case CSSValueInactiveborder:
        return Color::white;
    case CSSValueInactivecaption:
        return Color::white;
    case CSSValueInactivecaptiontext:
        return SRGBA<uint8_t> { 127, 127, 127 };
    case CSSValueInfobackground:
        return SRGBA<uint8_t> { 251, 252, 197 };
    case CSSValueInfotext:
        return Color::black;
    case CSSValueMenu:
        return Color::lightGray;
    case CSSValueMenutext:
        return Color::black;
    case CSSValueScrollbar:
        return Color::white;
    case CSSValueText:
        return Color::black;
    case CSSValueThreeddarkshadow:
        return SRGBA<uint8_t> { 102, 102, 102 };
    case CSSValueThreedface:
        return Color::lightGray;
    case CSSValueThreedhighlight:
        return SRGBA<uint8_t> { 221, 221, 221 };
    case CSSValueThreedlightshadow:
        return Color::lightGray;
    case CSSValueThreedshadow:
        return SRGBA<uint8_t> { 136, 136, 136 };
    case CSSValueWindow:
        return Color::white;
    case CSSValueWindowframe:
        return SRGBA<uint8_t> { 204, 204, 204 };
    case CSSValueWindowtext:
        return Color::black;
    default:
        return { };
    }
}

Color RenderTheme::textSearchHighlightColor(OptionSet<StyleColor::Options> options) const
{
    auto& cache = colorCache(options);
    if (!cache.textSearchHighlightColor.isValid())
        cache.textSearchHighlightColor = platformTextSearchHighlightColor(options);
    return cache.textSearchHighlightColor;
}

Color RenderTheme::platformTextSearchHighlightColor(OptionSet<StyleColor::Options>) const
{
    return Color::yellow;
}

#if ENABLE(APP_HIGHLIGHTS)
Color RenderTheme::appHighlightColor(OptionSet<StyleColor::Options> options) const
{
    auto& cache = colorCache(options);
    if (!cache.appHighlightColor.isValid())
        cache.appHighlightColor = transformSelectionBackgroundColor(platformAppHighlightColor(options), options);
    return cache.appHighlightColor;
}

Color RenderTheme::platformAppHighlightColor(OptionSet<StyleColor::Options>) const
{
    return Color::yellow;
}
#endif

Color RenderTheme::defaultButtonTextColor(OptionSet<StyleColor::Options> options) const
{
    auto& cache = colorCache(options);
    if (!cache.defaultButtonTextColor.isValid())
        cache.defaultButtonTextColor = platformDefaultButtonTextColor(options);
    return cache.defaultButtonTextColor;
}

Color RenderTheme::platformDefaultButtonTextColor(OptionSet<StyleColor::Options> options) const
{
    return systemColor(CSSValueActivebuttontext, options);
}

#if ENABLE(TOUCH_EVENTS)

Color RenderTheme::tapHighlightColor()
{
    return singleton().platformTapHighlightColor();
}

#endif

// Value chosen by observation. This can be tweaked.
constexpr double minColorContrastValue = 1.195;

// For transparent or translucent background color, use lightening.
constexpr float minDisabledColorAlphaValue = 0.5f;

Color RenderTheme::disabledTextColor(const Color& textColor, const Color& backgroundColor) const
{
    // The explicit check for black is an optimization for the 99% case (black on white).
    // This also means that black on black will turn into grey on black when disabled.
    Color disabledColor;
    if (equalIgnoringSemanticColor(textColor, Color::black) || backgroundColor.alphaAsFloat() < minDisabledColorAlphaValue || textColor.luminance() < backgroundColor.luminance())
        disabledColor = textColor.lightened();
    else
        disabledColor = textColor.darkened();
    
    // If there's not very much contrast between the disabled color and the background color,
    // just leave the text color alone. We don't want to change a good contrast color scheme so that it has really bad contrast.
    // If the contrast was already poor, then it doesn't do any good to change it to a different poor contrast color scheme.
    if (contrastRatio(disabledColor, backgroundColor) < minColorContrastValue)
        return textColor;

    return disabledColor;
}

// Value chosen to return dark gray for both white on black and black on white.
constexpr float datePlaceholderColorLightnessAdjustmentFactor = 0.66f;

Color RenderTheme::datePlaceholderTextColor(const Color& textColor, const Color& backgroundColor) const
{
    // FIXME: Consider using LCHA<float> rather than HSLA<float> for better perceptual results and to avoid clamping to sRGB gamut, which is what HSLA does.
    auto hsla = textColor.toColorTypeLossy<HSLA<float>>();
    if (textColor.luminance() < backgroundColor.luminance())
        hsla.lightness += datePlaceholderColorLightnessAdjustmentFactor * (100.0f - hsla.lightness);
    else
        hsla.lightness *= datePlaceholderColorLightnessAdjustmentFactor;

    // FIXME: Consider keeping color in LCHA (if that change is made) or converting back to the initial underlying color type to avoid unnecessarily clamping colors outside of sRGB.
    return convertColor<SRGBA<float>>(hsla);
}

void RenderTheme::setCustomFocusRingColor(const Color& color)
{
    customFocusRingColor() = color;
}

Color RenderTheme::focusRingColor(OptionSet<StyleColor::Options> options) const
{
    if (customFocusRingColor().isValid())
        return customFocusRingColor();

    auto& cache = colorCache(options);
    if (!cache.systemFocusRingColor.isValid())
        cache.systemFocusRingColor = platformFocusRingColor(options);
    return cache.systemFocusRingColor;
}

String RenderTheme::fileListDefaultLabel(bool multipleFilesAllowed) const
{
    if (multipleFilesAllowed)
        return fileButtonNoFilesSelectedLabel();
    return fileButtonNoFileSelectedLabel();
}

String RenderTheme::fileListNameForWidth(const FileList* fileList, const FontCascade& font, int width, bool multipleFilesAllowed) const
{
    if (width <= 0)
        return String();

    String string;
    if (fileList->isEmpty())
        string = fileListDefaultLabel(multipleFilesAllowed);
    else if (fileList->length() == 1)
        string = fileList->item(0)->name();
    else
        return StringTruncator::rightTruncate(multipleFileUploadText(fileList->length()), width, font);

    return StringTruncator::centerTruncate(string, width, font);
}

#if USE(SYSTEM_PREVIEW)
void RenderTheme::paintSystemPreviewBadge(Image& image, const PaintInfo& paintInfo, const FloatRect& rect)
{
    // The default implementation paints a small marker
    // in the upper right corner, as long as the image is big enough.

    UNUSED_PARAM(image);
    auto& context = paintInfo.context();

    GraphicsContextStateSaver stateSaver { context };

    if (rect.width() < 32 || rect.height() < 32)
        return;

    auto markerRect = FloatRect {rect.x() + rect.width() - 24, rect.y() + 8, 16, 16 };
    auto roundedMarkerRect = FloatRoundedRect { markerRect, FloatRoundedRect::Radii { 8 } };
    context.fillRoundedRect(roundedMarkerRect, Color::red);
}
#endif

#if ENABLE(TOUCH_EVENTS)

Color RenderTheme::platformTapHighlightColor() const
{
    // This color is expected to be drawn on a semi-transparent overlay,
    // making it more transparent than its alpha value indicates.
    return Color::black.colorWithAlphaByte(102);
}

#endif

} // namespace WebCore
