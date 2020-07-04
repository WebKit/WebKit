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
#include "HTMLNames.h"
#include "LocalizedStrings.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "SpinButtonElement.h"
#include "StringTruncator.h"
#include "TextControlInnerElements.h"
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringConcatenateNumbers.h>

#if ENABLE(METER_ELEMENT)
#include "HTMLMeterElement.h"
#include "RenderMeter.h"
#endif

#if ENABLE(DATALIST_ELEMENT)
#include "HTMLDataListElement.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
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
        style.setDisplay(DisplayType::InlineBlock);
    else if (style.display() == DisplayType::ListItem || style.display() == DisplayType::Table)
        style.setDisplay(DisplayType::Block);

    if (userAgentAppearanceStyle && isControlStyled(style, *userAgentAppearanceStyle)) {
        switch (part) {
        case MenulistPart:
            style.setAppearance(MenulistButtonPart);
            part = MenulistButtonPart;
            break;
        case TextFieldPart:
            adjustTextFieldStyle(style, element);
            FALLTHROUGH;
        default:
            style.setAppearance(NoControlPart);
            break;
        }
    }

    if (!style.hasAppearance())
        return;

    // Never support box-shadow on native controls.
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
        LengthSize minControlSize = Theme::singleton().minimumControlSize(part, style.fontCascade(), { style.minWidth(), style.minHeight() }, style.effectiveZoom());
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
    case PushButtonPart:
    case SquareButtonPart:
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
#endif
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
        return adjustSearchFieldDecorationPartStyle(style, element);
    case SearchFieldResultsDecorationPart:
        return adjustSearchFieldResultsDecorationPartStyle(style, element);
    case SearchFieldResultsButtonPart:
        return adjustSearchFieldResultsButtonStyle(style, element);
    case ProgressBarPart:
        return adjustProgressBarStyle(style, element);
#if ENABLE(METER_ELEMENT)
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
        return adjustMeterStyle(style, element);
#endif
#if ENABLE(SERVICE_CONTROLS)
    case ImageControlsButtonPart:
        break;
#endif
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

    if (UNLIKELY(!paintInfo.context().hasPlatformContext()))
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
        return paintCheckbox(box, paintInfo, integralSnappedRect);
    case RadioPart:
        return paintRadio(box, paintInfo, integralSnappedRect);
    case PushButtonPart:
    case SquareButtonPart:
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
#endif
    case DefaultButtonPart:
    case ButtonPart:
        return paintButton(box, paintInfo, integralSnappedRect);
    case InnerSpinButtonPart:
        return paintInnerSpinButton(box, paintInfo, integralSnappedRect);
#endif
    case MenulistPart:
        return paintMenuList(box, paintInfo, devicePixelSnappedRect);
#if ENABLE(METER_ELEMENT)
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
        return paintMeter(box, paintInfo, integralSnappedRect);
#endif
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
    case SnapshottedPluginOverlayPart:
        return paintSnapshottedPluginOverlay(box, paintInfo, integralSnappedRect);
#if ENABLE(SERVICE_CONTROLS)
    case ImageControlsButtonPart:
        return paintImageControlsButton(box, paintInfo, integralSnappedRect);
#endif
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
#if ENABLE(METER_ELEMENT)
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
#endif
    case ProgressBarPart:
    case SliderHorizontalPart:
    case SliderVerticalPart:
    case SliderThumbHorizontalPart:
    case SliderThumbVerticalPart:
    case SearchFieldCancelButtonPart:
    case SearchFieldDecorationPart:
    case SearchFieldResultsDecorationPart:
    case SearchFieldResultsButtonPart:
#if ENABLE(SERVICE_CONTROLS)
    case ImageControlsButtonPart:
#endif
    default:
        break;
    }

    return false;
#endif
}

bool RenderTheme::paintDecorations(const RenderBox& box, const PaintInfo& paintInfo, const LayoutRect& rect)
{
    if (paintInfo.context().paintingDisabled())
        return false;

    IntRect integralSnappedRect = snappedIntRect(rect);
    FloatRect devicePixelSnappedRect = snapRectToDevicePixels(rect, box.document().deviceScaleFactor());

    // Call the appropriate paint method based off the appearance value.
    switch (box.style().appearance()) {
    case MenulistButtonPart:
        return paintMenuListButtonDecorations(box, paintInfo, devicePixelSnappedRect);
    case TextFieldPart:
        return paintTextFieldDecorations(box, paintInfo, devicePixelSnappedRect);
    case TextAreaPart:
        return paintTextAreaDecorations(box, paintInfo, devicePixelSnappedRect);
    case CheckboxPart:
        return paintCheckboxDecorations(box, paintInfo, integralSnappedRect);
    case RadioPart:
        return paintRadioDecorations(box, paintInfo, integralSnappedRect);
    case PushButtonPart:
        return paintPushButtonDecorations(box, paintInfo, integralSnappedRect);
    case SquareButtonPart:
        return paintSquareButtonDecorations(box, paintInfo, integralSnappedRect);
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
#endif
    case ButtonPart:
        return paintButtonDecorations(box, paintInfo, integralSnappedRect);
    case MenulistPart:
        return paintMenuListDecorations(box, paintInfo, integralSnappedRect);
    case SliderThumbHorizontalPart:
    case SliderThumbVerticalPart:
        return paintSliderThumbDecorations(box, paintInfo, integralSnappedRect);
    case SearchFieldPart:
        return paintSearchFieldDecorations(box, paintInfo, integralSnappedRect);
#if ENABLE(METER_ELEMENT)
    case MeterPart:
    case RelevancyLevelIndicatorPart:
    case ContinuousCapacityLevelIndicatorPart:
    case DiscreteCapacityLevelIndicatorPart:
    case RatingLevelIndicatorPart:
#endif
    case ProgressBarPart:
    case SliderHorizontalPart:
    case SliderVerticalPart:
    case ListboxPart:
    case DefaultButtonPart:
    case SearchFieldCancelButtonPart:
    case SearchFieldDecorationPart:
    case SearchFieldResultsDecorationPart:
    case SearchFieldResultsButtonPart:
#if ENABLE(SERVICE_CONTROLS)
    case ImageControlsButtonPart:
#endif
    default:
        break;
    }

    return false;
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
    FloatPoint absPoint = muteButtonBox.localToAbsolute(FloatPoint(muteButtonBox.offsetLeft(), y), IsFixed | UseTransforms);
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
    return makeSimpleColor(0, 0, 255);
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
    return makeSimpleColor(176, 176, 176);
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
    if (state == ControlStates::HoverState && !supportsHover(o.style()))
        return false;

    // Assume pressed state is only responded to if the control is enabled.
    if (state == ControlStates::PressedState && !isEnabled(o))
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

ControlStates::States RenderTheme::extractControlStatesForRenderer(const RenderObject& o) const
{
    ControlStates::States states = 0;
    if (isHovered(o)) {
        states |= ControlStates::HoverState;
        if (isSpinUpButtonPartHovered(o))
            states |= ControlStates::SpinUpState;
    }
    if (isPressed(o)) {
        states |= ControlStates::PressedState;
        if (isSpinUpButtonPartPressed(o))
            states |= ControlStates::SpinUpState;
    }
    if (isFocused(o) && o.style().outlineStyleIsAuto() == OutlineIsAuto::On)
        states |= ControlStates::FocusState;
    if (isEnabled(o))
        states |= ControlStates::EnabledState;
    if (isChecked(o))
        states |= ControlStates::CheckedState;
    if (isDefault(o))
        states |= ControlStates::DefaultState;
    if (!isActive(o))
        states |= ControlStates::WindowInactiveState;
    if (isIndeterminate(o))
        states |= ControlStates::IndeterminateState;
    if (isPresenting(o))
        states |= ControlStates::PresentingState;
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

void RenderTheme::adjustButtonStyle(RenderStyle& style, const Element*) const
{
    // Most platforms will completely honor all CSS, and so we have no need to
    // adjust the style at all by default. We will still allow the theme a crack
    // at setting up a desired vertical size.
    setButtonSize(style);
}

void RenderTheme::adjustInnerSpinButtonStyle(RenderStyle&, const Element*) const
{
}
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

#if ENABLE(METER_ELEMENT)

void RenderTheme::adjustMeterStyle(RenderStyle& style, const Element*) const
{
    style.setBoxShadow(nullptr);
}

IntSize RenderTheme::meterSizeForBounds(const RenderMeter&, const IntRect& bounds) const
{
    return bounds.size();
}

bool RenderTheme::supportsMeter(ControlPart) const
{
    return false;
}

bool RenderTheme::paintMeter(const RenderObject&, const PaintInfo&, const IntRect&)
{
    return true;
}

#endif // METER_ELEMENT

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

String RenderTheme::colorInputStyleSheet() const
{
    ASSERT(RuntimeEnabledFeatures::sharedFeatures().inputTypeColorEnabled());
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

void RenderTheme::paintSliderTicks(const RenderObject& o, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!is<HTMLInputElement>(o.node()))
        return;

    auto& input = downcast<HTMLInputElement>(*o.node());
    if (!input.isRangeControl() || !input.list())
        return;

    auto& dataList = downcast<HTMLDataListElement>(*input.list());

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
    for (auto& optionElement : dataList.suggestions()) {
        String value = optionElement.value();
        if (!input.isValidValue(value))
            continue;
        double parsedValue = parseToDoubleForNumberType(input.sanitizeValue(value));
        double tickFraction = (parsedValue - min) / (max - min);
        double tickRatio = isHorizontal && o.style().isLeftToRightDirection() ? tickFraction : 1.0 - tickFraction;
        double tickPosition = round(tickRegionSideMargin + tickRegionWidth * tickRatio);
        if (isHorizontal)
            tickRect.setX(tickPosition);
        else
            tickRect.setY(tickPosition);
        paintInfo.context().fillRect(tickRect);
    }
}

#endif

Seconds RenderTheme::animationRepeatIntervalForProgressBar(RenderProgress&) const
{
    return 0_s;
}

Seconds RenderTheme::animationDurationForProgressBar(RenderProgress&) const
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
    static NeverDestroyed<FontCascadeDescription> caption;
    static NeverDestroyed<FontCascadeDescription> icon;
    static NeverDestroyed<FontCascadeDescription> menu;
    static NeverDestroyed<FontCascadeDescription> messageBox;
    static NeverDestroyed<FontCascadeDescription> smallCaption;
    static NeverDestroyed<FontCascadeDescription> statusBar;
    static NeverDestroyed<FontCascadeDescription> webkitMiniControl;
    static NeverDestroyed<FontCascadeDescription> webkitSmallControl;
    static NeverDestroyed<FontCascadeDescription> webkitControl;
    static NeverDestroyed<FontCascadeDescription> defaultDescription;

    switch (systemFontID) {
    case CSSValueCaption:
        return caption;
    case CSSValueIcon:
        return icon;
    case CSSValueMenu:
        return menu;
    case CSSValueMessageBox:
        return messageBox;
    case CSSValueSmallCaption:
        return smallCaption;
    case CSSValueStatusBar:
        return statusBar;
    case CSSValueWebkitMiniControl:
        return webkitMiniControl;
    case CSSValueWebkitSmallControl:
        return webkitSmallControl;
    case CSSValueWebkitControl:
        return webkitControl;
    case CSSValueNone:
        return defaultDescription;
    default:
        ASSERT_NOT_REACHED();
        return defaultDescription;
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
        return options.contains(StyleColor::Options::ForVisitedLink) ? SimpleColor { 0xFF551A8B } : SimpleColor { 0xFF0000EE };
    case CSSValueWebkitActivelink:
    case CSSValueActivetext:
        return SimpleColor { 0xFFFF0000 };
    case CSSValueLinktext:
        return SimpleColor { 0xFF0000EE };
    case CSSValueVisitedtext:
        return SimpleColor { 0xFF551A8B };
    case CSSValueActiveborder:
        return Color::white;
    case CSSValueActivebuttontext:
        return Color::black;
    case CSSValueActivecaption:
        return SimpleColor { 0xFFCCCCCC };
    case CSSValueAppworkspace:
        return Color::white;
    case CSSValueBackground:
        return SimpleColor { 0xFF6363CE };
    case CSSValueButtonface:
        return Color::lightGray;
    case CSSValueButtonhighlight:
        return SimpleColor { 0xFFDDDDDD };
    case CSSValueButtonshadow:
        return SimpleColor { 0xFF888888 };
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
        return SimpleColor { 0xFF808080 };
    case CSSValueHighlight:
        return SimpleColor { 0xFFB5D5FF };
    case CSSValueHighlighttext:
        return Color::black;
    case CSSValueInactiveborder:
        return Color::white;
    case CSSValueInactivecaption:
        return Color::white;
    case CSSValueInactivecaptiontext:
        return SimpleColor { 0xFF7F7F7F };
    case CSSValueInfobackground:
        return SimpleColor { 0xFFFBFCC5 };
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
        return SimpleColor { 0xFF666666 };
    case CSSValueThreedface:
        return Color::lightGray;
    case CSSValueThreedhighlight:
        return SimpleColor { 0xFFDDDDDD };
    case CSSValueThreedlightshadow:
        return Color::lightGray;
    case CSSValueThreedshadow:
        return SimpleColor { 0xFF888888 };
    case CSSValueWindow:
        return Color::white;
    case CSSValueWindowframe:
        return SimpleColor { 0xFFCCCCCC };
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

#if ENABLE(TOUCH_EVENTS)

Color RenderTheme::tapHighlightColor()
{
    return singleton().platformTapHighlightColor();
}

#endif

// Value chosen by observation. This can be tweaked.
constexpr float minColorContrastValue = 1.1f;

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
    if (contrastRatio(disabledColor.toSRGBALossy<float>(), backgroundColor.toSRGBALossy<float>()) < minColorContrastValue)
        return textColor;

    return disabledColor;
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
    auto color = makeSimpleColor(255, 0, 0);
    context.fillRoundedRect(roundedMarkerRect, color);
}
#endif

#if ENABLE(TOUCH_EVENTS)

Color RenderTheme::platformTapHighlightColor() const
{
    // This color is expected to be drawn on a semi-transparent overlay,
    // making it more transparent than its alpha value indicates.
    return makeSimpleColor(0, 0, 0, 102);
}

#endif

} // namespace WebCore
