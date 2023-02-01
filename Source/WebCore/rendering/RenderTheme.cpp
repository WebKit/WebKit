/*
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
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

#include "ApplePayButtonPart.h"
#include "ButtonPart.h"
#include "CSSValueKeywords.h"
#include "ColorBlending.h"
#include "ColorLuminance.h"
#include "ColorWellPart.h"
#include "ControlStates.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "FileList.h"
#include "FloatConversion.h"
#include "FloatRoundedRect.h"
#include "FocusController.h"
#include "FontSelector.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "HTMLAttachmentElement.h"
#include "HTMLButtonElement.h"
#include "HTMLInputElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLProgressElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "ImageControlsButtonPart.h"
#include "InnerSpinButtonPart.h"
#include "LocalizedStrings.h"
#include "MenuListButtonPart.h"
#include "MenuListPart.h"
#include "MeterPart.h"
#include "Page.h"
#include "PaintInfo.h"
#include "ProgressBarPart.h"
#include "RenderMeter.h"
#include "RenderProgress.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "SearchFieldCancelButtonPart.h"
#include "SearchFieldPart.h"
#include "ShadowPseudoIds.h"
#include "SliderThumbElement.h"
#include "SliderThumbPart.h"
#include "SliderTrackPart.h"
#include "SpinButtonElement.h"
#include "StringTruncator.h"
#include "TextAreaPart.h"
#include "TextControlInnerElements.h"
#include "TextFieldPart.h"
#include "ToggleButtonPart.h"
#include <wtf/FileSystem.h>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringConcatenateNumbers.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsMac.h"
#endif

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

StyleAppearance RenderTheme::adjustAppearanceForElement(RenderStyle& style, const Element* element, StyleAppearance autoAppearance) const
{
    if (!element) {
        style.setEffectiveAppearance(StyleAppearance::None);
        return StyleAppearance::None;
    }

    auto appearance = style.effectiveAppearance();
    if (appearance == autoAppearance)
        return appearance;

    // Aliases of 'auto'.
    // https://drafts.csswg.org/css-ui-4/#typedef-appearance-compat-auto
    if (appearance == StyleAppearance::Auto
        || appearance == StyleAppearance::SearchField
        || appearance == StyleAppearance::TextArea
        || appearance == StyleAppearance::Checkbox
        || appearance == StyleAppearance::Radio
        || appearance == StyleAppearance::Listbox
        || appearance == StyleAppearance::Meter
        || appearance == StyleAppearance::ProgressBar
        || appearance == StyleAppearance::SquareButton
        || appearance == StyleAppearance::PushButton
        || appearance == StyleAppearance::SliderHorizontal
        || appearance == StyleAppearance::Menulist) {
        style.setEffectiveAppearance(autoAppearance);
        return autoAppearance;
    }

    // The following keywords should work well for some element types
    // even if their default appearances are different from the keywords.
    if (appearance == StyleAppearance::Button) {
        if (autoAppearance == StyleAppearance::PushButton || autoAppearance == StyleAppearance::SquareButton)
            return appearance;
        style.setEffectiveAppearance(autoAppearance);
        return autoAppearance;
    }

    if (appearance == StyleAppearance::MenulistButton) {
        if (autoAppearance == StyleAppearance::Menulist)
            return appearance;
        style.setEffectiveAppearance(autoAppearance);
        return autoAppearance;
    }

    auto* inputElement = dynamicDowncast<HTMLInputElement>(element);

    if (appearance == StyleAppearance::TextField) {
        if (inputElement && inputElement->isSearchField())
            return appearance;
        style.setEffectiveAppearance(autoAppearance);
        return autoAppearance;
    }

    if (appearance == StyleAppearance::SliderVertical) {
        if (inputElement && inputElement->isRangeControl())
            return appearance;
        style.setEffectiveAppearance(autoAppearance);
        return autoAppearance;
    }

    return appearance;
}

static bool isAppearanceAllowedForAllElements(StyleAppearance appearance)
{
#if ENABLE(APPLE_PAY)
    if (appearance == StyleAppearance::ApplePayButton)
        return true;
#endif

    UNUSED_PARAM(appearance);
    return false;
}

void RenderTheme::adjustStyle(RenderStyle& style, const Element* element, const RenderStyle* userAgentAppearanceStyle)
{
    auto autoAppearance = autoAppearanceForElement(style, element);
    auto appearance = adjustAppearanceForElement(style, element, autoAppearance);

    if (appearance == StyleAppearance::None)
        return;

    // Force inline and table display styles to be inline-block (except for table- which is block)
    if (style.display() == DisplayType::Inline || style.display() == DisplayType::InlineTable || style.display() == DisplayType::TableRowGroup
        || style.display() == DisplayType::TableHeaderGroup || style.display() == DisplayType::TableFooterGroup
        || style.display() == DisplayType::TableRow || style.display() == DisplayType::TableColumnGroup || style.display() == DisplayType::TableColumn
        || style.display() == DisplayType::TableCell || style.display() == DisplayType::TableCaption)
        style.setEffectiveDisplay(DisplayType::InlineBlock);
    else if (style.display() == DisplayType::ListItem || style.display() == DisplayType::Table)
        style.setEffectiveDisplay(DisplayType::Block);

    if (userAgentAppearanceStyle && isControlStyled(style, *userAgentAppearanceStyle)) {
        switch (appearance) {
        case StyleAppearance::Menulist:
            appearance = StyleAppearance::MenulistButton;
            break;
        default:
            appearance = StyleAppearance::None;
            break;
        }

        style.setEffectiveAppearance(appearance);
    }

    if (!isAppearanceAllowedForAllElements(appearance)
        && !userAgentAppearanceStyle
        && autoAppearance == StyleAppearance::None
        && !style.borderAndBackgroundEqual(RenderStyle::defaultStyle()))
        style.setEffectiveAppearance(StyleAppearance::None);

    if (!style.hasEffectiveAppearance())
        return;

    if (!supportsBoxShadow(style))
        style.setBoxShadow(nullptr);

#if USE(NEW_THEME)
    switch (appearance) {
    case StyleAppearance::Checkbox:
    case StyleAppearance::InnerSpinButton:
    case StyleAppearance::Radio:
    case StyleAppearance::PushButton:
    case StyleAppearance::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
#endif
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button: {
        // Border
        LengthBox borderBox(style.borderTopWidth(), style.borderRightWidth(), style.borderBottomWidth(), style.borderLeftWidth());
        borderBox = Theme::singleton().controlBorder(appearance, style.fontCascade(), borderBox, style.effectiveZoom());
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
        LengthBox paddingBox = Theme::singleton().controlPadding(appearance, style.fontCascade(), style.paddingBox(), style.effectiveZoom());
        if (paddingBox != style.paddingBox())
            style.setPaddingBox(WTFMove(paddingBox));

        // Whitespace
        if (Theme::singleton().controlRequiresPreWhiteSpace(appearance))
            style.setWhiteSpace(WhiteSpace::Pre);

        // Width / Height
        // The width and height here are affected by the zoom.
        // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
        LengthSize controlSize = Theme::singleton().controlSize(appearance, style.fontCascade(), { style.width(), style.height() }, style.effectiveZoom());
        if (controlSize.width != style.width())
            style.setWidth(WTFMove(controlSize.width));
        if (controlSize.height != style.height())
            style.setHeight(WTFMove(controlSize.height));

        // Min-Width / Min-Height
        LengthSize minControlSize = Theme::singleton().minimumControlSize(appearance, style.fontCascade(), { style.minWidth(), style.minHeight() }, { style.width(), style.height() }, style.effectiveZoom());
        if (minControlSize.width.value() > style.minWidth().value())
            style.setMinWidth(WTFMove(minControlSize.width));
        if (minControlSize.height.value() > style.minHeight().value())
            style.setMinHeight(WTFMove(minControlSize.height));

        // Font
        if (auto themeFont = Theme::singleton().controlFont(appearance, style.fontCascade(), style.effectiveZoom())) {
            // If overriding the specified font with the theme font, also override the line height with the standard line height.
            style.setLineHeight(RenderStyle::initialLineHeight());
            if (style.setFontDescription(WTFMove(themeFont.value())))
                style.fontCascade().update(nullptr);
        }

        // Special style that tells enabled default buttons in active windows to use the ActiveButtonText color.
        // The active window part of the test has to be done at paint time since it's not triggered by a style change.
        style.setInsideDefaultButton(appearance == StyleAppearance::DefaultButton && element && !element->isDisabledFormControl());
        break;
    }
    default:
        break;
    }
#endif

    // Call the appropriate style adjustment method based off the appearance value.
    switch (appearance) {
#if !USE(NEW_THEME)
    case StyleAppearance::Checkbox:
        return adjustCheckboxStyle(style, element);
    case StyleAppearance::Radio:
        return adjustRadioStyle(style, element);
#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
        return adjustColorWellStyle(style, element);
#endif
    case StyleAppearance::PushButton:
    case StyleAppearance::SquareButton:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
        return adjustButtonStyle(style, element);
    case StyleAppearance::InnerSpinButton:
        return adjustInnerSpinButtonStyle(style, element);
#endif
    case StyleAppearance::TextField:
        return adjustTextFieldStyle(style, element);
    case StyleAppearance::TextArea:
        return adjustTextAreaStyle(style, element);
    case StyleAppearance::Menulist:
        return adjustMenuListStyle(style, element);
    case StyleAppearance::MenulistButton:
        return adjustMenuListButtonStyle(style, element);
    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
        return adjustSliderTrackStyle(style, element);
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
        return adjustSliderThumbStyle(style, element);
    case StyleAppearance::SearchField:
        return adjustSearchFieldStyle(style, element);
    case StyleAppearance::SearchFieldCancelButton:
        return adjustSearchFieldCancelButtonStyle(style, element);
    case StyleAppearance::SearchFieldDecoration:
        return adjustSearchFieldDecorationPartStyle(style, element);
    case StyleAppearance::SearchFieldResultsDecoration:
        return adjustSearchFieldResultsDecorationPartStyle(style, element);
    case StyleAppearance::SearchFieldResultsButton:
        return adjustSearchFieldResultsButtonStyle(style, element);
    case StyleAppearance::ProgressBar:
        return adjustProgressBarStyle(style, element);
    case StyleAppearance::Meter:
        return adjustMeterStyle(style, element);
#if ENABLE(SERVICE_CONTROLS)
    case StyleAppearance::ImageControlsButton:
        return adjustImageControlsButtonStyle(style, element);
#endif
    case StyleAppearance::CapsLockIndicator:
        return adjustCapsLockIndicatorStyle(style, element);
#if ENABLE(APPLE_PAY)
    case StyleAppearance::ApplePayButton:
        return adjustApplePayButtonStyle(style, element);
#endif
#if ENABLE(DATALIST_ELEMENT)
    case StyleAppearance::ListButton:
        return adjustListButtonStyle(style, element);
#endif
    default:
        break;
    }
}

StyleAppearance RenderTheme::autoAppearanceForElement(RenderStyle& style, const Element* elementPtr) const
{
    if (!elementPtr)
        return StyleAppearance::None;

    Ref element = *elementPtr;

    if (is<HTMLInputElement>(element)) {
        auto& input = downcast<HTMLInputElement>(element.get());

        if (input.isTextButton() || input.isUploadButton())
            return StyleAppearance::Button;

        if (input.isCheckbox())
            return StyleAppearance::Checkbox;

        if (input.isRadioButton())
            return StyleAppearance::Radio;

        if (input.isSearchField())
            return StyleAppearance::SearchField;

        if (input.isDateField() || input.isDateTimeLocalField() || input.isMonthField() || input.isTimeField() || input.isWeekField()) {
#if PLATFORM(IOS_FAMILY)
            return StyleAppearance::MenulistButton;
#else
            return StyleAppearance::TextField;
#endif
        }

#if ENABLE(INPUT_TYPE_COLOR)
        if (input.isColorControl())
            return StyleAppearance::ColorWell;
#endif

        if (input.isRangeControl())
            return style.isHorizontalWritingMode() ? StyleAppearance::SliderHorizontal : StyleAppearance::SliderVertical;

        if (input.isTextField())
            return StyleAppearance::TextField;

        // <input type=hidden|image|file>
        return StyleAppearance::None;
    }

    if (is<HTMLButtonElement>(element)) {
#if ENABLE(SERVICE_CONTROLS)
        if (isImageControlsButton(element.get()))
            return StyleAppearance::ImageControlsButton;
#endif

        return StyleAppearance::Button;
    }

    if (is<HTMLSelectElement>(element)) {
#if PLATFORM(IOS_FAMILY)
        return StyleAppearance::MenulistButton;
#else
        auto& select = downcast<HTMLSelectElement>(element.get());
        return select.usesMenuList() ? StyleAppearance::Menulist : StyleAppearance::Listbox;
#endif
    }

    if (is<HTMLTextAreaElement>(element))
        return StyleAppearance::TextArea;

    if (is<HTMLMeterElement>(element))
        return StyleAppearance::Meter;

    if (is<HTMLProgressElement>(element))
        return StyleAppearance::ProgressBar;

#if ENABLE(ATTACHMENT_ELEMENT)
    if (is<HTMLAttachmentElement>(element))
        return StyleAppearance::Attachment;
#endif

    if (element->isInUserAgentShadowTree()) {
        auto& pseudo = element->shadowPseudoId();

#if ENABLE(DATALIST_ELEMENT)
        if (pseudo == ShadowPseudoIds::webkitListButton())
            return StyleAppearance::ListButton;
#endif

        if (pseudo == ShadowPseudoIds::webkitCapsLockIndicator())
            return StyleAppearance::CapsLockIndicator;

        if (pseudo == ShadowPseudoIds::webkitSearchCancelButton())
            return StyleAppearance::SearchFieldCancelButton;

        if (is<SearchFieldResultsButtonElement>(element)) {
            if (!downcast<SearchFieldResultsButtonElement>(element.get()).canAdjustStyleForAppearance())
                return StyleAppearance::None;

            if (pseudo == ShadowPseudoIds::webkitSearchDecoration())
                return StyleAppearance::SearchFieldDecoration;

            if (pseudo == ShadowPseudoIds::webkitSearchResultsDecoration())
                return StyleAppearance::SearchFieldResultsDecoration;

            if (pseudo == ShadowPseudoIds::webkitSearchResultsButton())
                return StyleAppearance::SearchFieldResultsButton;
        }

        if (pseudo == ShadowPseudoIds::webkitSliderThumb())
            return StyleAppearance::SliderThumbHorizontal;

        if (pseudo == ShadowPseudoIds::webkitInnerSpinButton())
            return StyleAppearance::InnerSpinButton;
    }

    return StyleAppearance::None;
}

#if ENABLE(APPLE_PAY)
static RefPtr<ControlPart> createApplePayButtonPartForRenderer(const RenderObject& renderer)
{
    auto& style = renderer.style();

    String locale = style.computedLocale();
    if (locale.isEmpty())
        locale = defaultLanguage(ShouldMinimizeLanguages::No);

    return ApplePayButtonPart::create(style.applePayButtonType(), style.applePayButtonStyle(), locale);
}
#endif

static RefPtr<ControlPart> createMeterPartForRenderer(const RenderObject& renderer)
{
    ASSERT(is<RenderMeter>(renderer));
    const auto& renderMeter = downcast<RenderMeter>(renderer);

    auto element = renderMeter.meterElement();
    MeterPart::GaugeRegion gaugeRegion;

    switch (element->gaugeRegion()) {
    case HTMLMeterElement::GaugeRegionOptimum:
        gaugeRegion = MeterPart::GaugeRegion::Optimum;
        break;
    case HTMLMeterElement::GaugeRegionSuboptimal:
        gaugeRegion = MeterPart::GaugeRegion::Suboptimal;
        break;
    case HTMLMeterElement::GaugeRegionEvenLessGood:
        gaugeRegion = MeterPart::GaugeRegion::EvenLessGood;
        break;
    }

    return MeterPart::create(gaugeRegion, element->value(), element->min(), element->max());
}

static RefPtr<ControlPart> createProgressBarPartForRenderer(const RenderObject& renderer)
{
    ASSERT(is<RenderProgress>(renderer));
    const auto& renderProgress = downcast<RenderProgress>(renderer);
    return ProgressBarPart::create(renderProgress.position(), renderProgress.animationStartTime().secondsSinceEpoch());
}

static RefPtr<ControlPart> createSliderTrackPartForRenderer(const RenderObject& renderer)
{
    auto type = renderer.style().effectiveAppearance();
    ASSERT(type == StyleAppearance::SliderHorizontal || type == StyleAppearance::SliderVertical);

    ASSERT(is<HTMLInputElement>(renderer.node()));
    auto& input = downcast<HTMLInputElement>(*renderer.node());
    ASSERT(input.isRangeControl());

    IntSize thumbSize;
    if (const auto* thumbRenderer = input.sliderThumbElement()->renderer()) {
        const auto& thumbStyle = thumbRenderer->style();
        thumbSize = IntSize { thumbStyle.width().intValue(), thumbStyle.height().intValue() };
    }

    IntRect trackBounds;
    if (const auto* trackRenderer = input.sliderTrackElement()->renderer()) {
        trackBounds = trackRenderer->absoluteBoundingBoxRectIgnoringTransforms();

        // We can ignoring transforms because transform is handled by the graphics context.
        auto sliderBounds = renderer.absoluteBoundingBoxRectIgnoringTransforms();

        // Make position relative to the transformed ancestor element.
        trackBounds.moveBy(-sliderBounds.location());
    }

    Vector<double> tickRatios;
#if ENABLE(DATALIST_ELEMENT)
    if (auto dataList = input.dataList()) {
        double minimum = input.minimum();
        double maximum = input.maximum();

        for (auto& optionElement : dataList->suggestions()) {
            auto optionValue = input.listOptionValueAsDouble(optionElement);
            if (!optionValue)
                continue;
            double tickRatio = (*optionValue - minimum) / (maximum - minimum);
            tickRatios.append(tickRatio);
        }
    }
#endif
    return SliderTrackPart::create(type, thumbSize, trackBounds, WTFMove(tickRatios));
}

RefPtr<ControlPart> RenderTheme::createControlPart(const RenderObject& renderer) const
{
    auto appearance = renderer.style().effectiveAppearance();

    switch (appearance) {
    case StyleAppearance::None:
    case StyleAppearance::Auto:
        break;

    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
        return ToggleButtonPart::create(appearance);

    case StyleAppearance::PushButton:
    case StyleAppearance::SquareButton:
    case StyleAppearance::Button:
    case StyleAppearance::DefaultButton:
        return ButtonPart::create(appearance);

    case StyleAppearance::Menulist:
        return MenuListPart::create();

    case StyleAppearance::MenulistButton:
        return MenuListButtonPart::create();

    case StyleAppearance::Meter:
        return createMeterPartForRenderer(renderer);

    case StyleAppearance::ProgressBar:
        return createProgressBarPartForRenderer(renderer);

    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
        return createSliderTrackPartForRenderer(renderer);

    case StyleAppearance::SearchField:
        return SearchFieldPart::create();

#if ENABLE(APPLE_PAY)
    case StyleAppearance::ApplePayButton:
        return createApplePayButtonPartForRenderer(renderer);
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
    case StyleAppearance::Attachment:
    case StyleAppearance::BorderlessAttachment:
        break;
#endif

    case StyleAppearance::Listbox:
    case StyleAppearance::TextArea:
        return TextAreaPart::create(appearance);

    case StyleAppearance::TextField:
        return TextFieldPart::create();

    case StyleAppearance::CapsLockIndicator:
        break;

#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
        return ColorWellPart::create();
#endif
#if ENABLE(SERVICE_CONTROLS)
    case StyleAppearance::ImageControlsButton:
        return ImageControlsButtonPart::create();
#endif

    case StyleAppearance::InnerSpinButton:
        return InnerSpinButtonPart::create();

#if ENABLE(DATALIST_ELEMENT)
    case StyleAppearance::ListButton:
        break;
#endif

    case StyleAppearance::SearchFieldDecoration:
    case StyleAppearance::SearchFieldResultsDecoration:
    case StyleAppearance::SearchFieldResultsButton:
        break;

    case StyleAppearance::SearchFieldCancelButton:
        return SearchFieldCancelButtonPart::create();

    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
        return SliderThumbPart::create(appearance);
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

OptionSet<ControlStyle::State> RenderTheme::extractControlStyleStatesForRenderer(const RenderObject& renderer) const
{
    OptionSet<ControlStyle::State> states;
    if (isHovered(renderer)) {
        states.add(ControlStyle::State::Hovered);
        if (isSpinUpButtonPartHovered(renderer))
            states.add(ControlStyle::State::SpinUp);
    }
    if (isPressed(renderer)) {
        states.add(ControlStyle::State::Pressed);
        if (isSpinUpButtonPartPressed(renderer))
            states.add(ControlStyle::State::SpinUp);
    }
    if (isFocused(renderer) && renderer.style().outlineStyleIsAuto() == OutlineIsAuto::On)
        states.add(ControlStyle::State::Focused);
    if (isEnabled(renderer))
        states.add(ControlStyle::State::Enabled);
    if (isChecked(renderer))
        states.add(ControlStyle::State::Checked);
    if (isDefault(renderer))
        states.add(ControlStyle::State::Default);
    if (isWindowActive(renderer))
        states.add(ControlStyle::State::WindowActive);
    if (isIndeterminate(renderer))
        states.add(ControlStyle::State::Indeterminate);
    if (isPresenting(renderer))
        states.add(ControlStyle::State::Presenting);
    if (useFormSemanticContext())
        states.add(ControlStyle::State::FormSemanticContext);
    if (renderer.useDarkAppearance())
        states.add(ControlStyle::State::DarkAppearance);
    if (!renderer.style().isLeftToRightDirection())
        states.add(ControlStyle::State::RightToLeft);
    if (supportsLargeFormControls())
        states.add(ControlStyle::State::LargeControls);
    if (isReadOnlyControl(renderer))
        states.add(ControlStyle::State::ReadOnly);
#if ENABLE(DATALIST_ELEMENT)
    if (hasListButton(renderer)) {
        states.add(ControlStyle::State::ListButton);
        if (hasListButtonPressed(renderer))
            states.add(ControlStyle::State::ListButtonPressed);
    }
#endif
    if (!renderer.style().isHorizontalWritingMode())
        states.add(ControlStyle::State::VerticalWritingMode);
    return states;
}

ControlStyle RenderTheme::extractControlStyleForRenderer(const RenderBox& box) const
{
    const RenderObject* renderer = &box;
    auto type = box.style().effectiveAppearance();

    if (type == StyleAppearance::SearchFieldCancelButton) {
        auto* input = box.element()->shadowHost();
        if (!input)
            input = box.element();

        if (!is<RenderBox>(input->renderer()))
            return { };

        renderer = downcast<RenderBox>(input->renderer());
    }

    return {
        extractControlStyleStatesForRenderer(*renderer),
        renderer->style().computedFontPixelSize(),
        renderer->style().effectiveZoom(),
        renderer->style().effectiveAccentColor(),
        renderer->style().visitedDependentColorWithColorFilter(CSSPropertyColor),
        renderer->style().borderWidth()
    };
}

bool RenderTheme::paint(const RenderBox& box, ControlPart& part, const PaintInfo& paintInfo, const LayoutRect& rect)
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

    float deviceScaleFactor = box.document().deviceScaleFactor();
    auto zoomedRect = snapRectToDevicePixels(rect, deviceScaleFactor);
    auto borderRect = FloatRoundedRect(box.style().getRoundedBorderFor(LayoutRect(zoomedRect)));
    auto controlStyle = extractControlStyleForRenderer(box);
    auto& context = paintInfo.context();

    context.drawControlPart(part, borderRect, deviceScaleFactor, controlStyle);
    return false;
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
    
    auto appearance = box.style().effectiveAppearance();

    if (UNLIKELY(!canPaint(paintInfo, box.settings(), appearance)))
        return false;

    IntRect integralSnappedRect = snappedIntRect(rect);
    float deviceScaleFactor = box.document().deviceScaleFactor();
    FloatRect devicePixelSnappedRect = snapRectToDevicePixels(rect, deviceScaleFactor);

#if USE(NEW_THEME)
    float pageScaleFactor = box.page().pageScaleFactor();

    switch (appearance) {
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
    case StyleAppearance::PushButton:
    case StyleAppearance::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
#endif
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
    case StyleAppearance::InnerSpinButton:
        updateControlStatesForRenderer(box, controlStates);
        Theme::singleton().paint(appearance, controlStates, paintInfo.context(), devicePixelSnappedRect, box.style().effectiveZoom(), &box.view().frameView(), deviceScaleFactor, pageScaleFactor, box.document().useSystemAppearance(), box.useDarkAppearance(), box.style().effectiveAccentColor());
        return false;
    default:
        break;
    }
#else
    UNUSED_PARAM(controlStates);
#endif

    // Call the appropriate paint method based off the appearance value.
    switch (appearance) {
#if !USE(NEW_THEME)
    case StyleAppearance::Checkbox:
        return paintCheckbox(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::Radio:
        return paintRadio(box, paintInfo, devicePixelSnappedRect);
#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
        return paintColorWell(box, paintInfo, integralSnappedRect);
#endif
    case StyleAppearance::PushButton:
    case StyleAppearance::SquareButton:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
        return paintButton(box, paintInfo, integralSnappedRect);
    case StyleAppearance::InnerSpinButton:
        return paintInnerSpinButton(box, paintInfo, integralSnappedRect);
#endif
    case StyleAppearance::Menulist:
        return paintMenuList(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::Meter:
        return paintMeter(box, paintInfo, integralSnappedRect);
    case StyleAppearance::ProgressBar:
        return paintProgressBar(box, paintInfo, integralSnappedRect);
    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
        return paintSliderTrack(box, paintInfo, integralSnappedRect);
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
        return paintSliderThumb(box, paintInfo, integralSnappedRect);
    case StyleAppearance::MenulistButton:
    case StyleAppearance::TextField:
    case StyleAppearance::TextArea:
    case StyleAppearance::Listbox:
        return true;
    case StyleAppearance::SearchField:
        return paintSearchField(box, paintInfo, integralSnappedRect);
    case StyleAppearance::SearchFieldCancelButton:
        return paintSearchFieldCancelButton(box, paintInfo, integralSnappedRect);
    case StyleAppearance::SearchFieldDecoration:
        return paintSearchFieldDecorationPart(box, paintInfo, integralSnappedRect);
    case StyleAppearance::SearchFieldResultsDecoration:
        return paintSearchFieldResultsDecorationPart(box, paintInfo, integralSnappedRect);
    case StyleAppearance::SearchFieldResultsButton:
        return paintSearchFieldResultsButton(box, paintInfo, integralSnappedRect);
#if ENABLE(SERVICE_CONTROLS)
    case StyleAppearance::ImageControlsButton:
        return paintImageControlsButton(box, paintInfo, integralSnappedRect);
#endif
    case StyleAppearance::CapsLockIndicator:
        return paintCapsLockIndicator(box, paintInfo, integralSnappedRect);
#if ENABLE(APPLE_PAY)
    case StyleAppearance::ApplePayButton:
        return paintApplePayButton(box, paintInfo, integralSnappedRect);
#endif
#if ENABLE(DATALIST_ELEMENT)
    case StyleAppearance::ListButton:
        return paintListButton(box, paintInfo, devicePixelSnappedRect);
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
    case StyleAppearance::Attachment:
    case StyleAppearance::BorderlessAttachment:
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
    return box.style().effectiveAppearance() != StyleAppearance::None;
#else
    FloatRect devicePixelSnappedRect = snapRectToDevicePixels(rect, box.document().deviceScaleFactor());
    // Call the appropriate paint method based off the appearance value.
    switch (box.style().effectiveAppearance()) {
    case StyleAppearance::TextField:
        return paintTextField(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::Listbox:
    case StyleAppearance::TextArea:
        return paintTextArea(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::MenulistButton:
    case StyleAppearance::SearchField:
        return true;
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
    case StyleAppearance::PushButton:
    case StyleAppearance::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
#endif
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
    case StyleAppearance::Menulist:
    case StyleAppearance::Meter:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
    case StyleAppearance::SearchFieldCancelButton:
    case StyleAppearance::SearchFieldDecoration:
    case StyleAppearance::SearchFieldResultsDecoration:
    case StyleAppearance::SearchFieldResultsButton:
#if ENABLE(SERVICE_CONTROLS)
    case StyleAppearance::ImageControlsButton:
#endif
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
    switch (box.style().effectiveAppearance()) {
    case StyleAppearance::MenulistButton:
        paintMenuListButtonDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case StyleAppearance::TextField:
        paintTextFieldDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case StyleAppearance::TextArea:
        paintTextAreaDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case StyleAppearance::Checkbox:
        paintCheckboxDecorations(box, paintInfo, integralSnappedRect);
        break;
    case StyleAppearance::Radio:
        paintRadioDecorations(box, paintInfo, integralSnappedRect);
        break;
    case StyleAppearance::PushButton:
        paintPushButtonDecorations(box, paintInfo, integralSnappedRect);
        break;
    case StyleAppearance::SquareButton:
        paintSquareButtonDecorations(box, paintInfo, integralSnappedRect);
        break;
#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
        paintColorWellDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
#endif
    case StyleAppearance::Button:
        paintButtonDecorations(box, paintInfo, integralSnappedRect);
        break;
    case StyleAppearance::Menulist:
        paintMenuListDecorations(box, paintInfo, integralSnappedRect);
        break;
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
        paintSliderThumbDecorations(box, paintInfo, integralSnappedRect);
        break;
    case StyleAppearance::SearchField:
        paintSearchFieldDecorations(box, paintInfo, integralSnappedRect);
        break;
    case StyleAppearance::Meter:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
    case StyleAppearance::Listbox:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::SearchFieldCancelButton:
    case StyleAppearance::SearchFieldDecoration:
    case StyleAppearance::SearchFieldResultsDecoration:
    case StyleAppearance::SearchFieldResultsButton:
#if ENABLE(SERVICE_CONTROLS)
    case StyleAppearance::ImageControlsButton:
#endif
    default:
        break;
    }
}

Color RenderTheme::activeSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.activeSelectionBackgroundColor.isValid())
        cache.activeSelectionBackgroundColor = transformSelectionBackgroundColor(platformActiveSelectionBackgroundColor(options), options);
    return cache.activeSelectionBackgroundColor;
}

Color RenderTheme::inactiveSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.inactiveSelectionBackgroundColor.isValid())
        cache.inactiveSelectionBackgroundColor = transformSelectionBackgroundColor(platformInactiveSelectionBackgroundColor(options), options);
    return cache.inactiveSelectionBackgroundColor;
}

Color RenderTheme::transformSelectionBackgroundColor(const Color& color, OptionSet<StyleColorOptions>) const
{
    return blendWithWhite(color);
}

Color RenderTheme::activeSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.activeSelectionForegroundColor.isValid() && supportsSelectionForegroundColors(options))
        cache.activeSelectionForegroundColor = platformActiveSelectionForegroundColor(options);
    return cache.activeSelectionForegroundColor;
}

Color RenderTheme::inactiveSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.inactiveSelectionForegroundColor.isValid() && supportsSelectionForegroundColors(options))
        cache.inactiveSelectionForegroundColor = platformInactiveSelectionForegroundColor(options);
    return cache.inactiveSelectionForegroundColor;
}

Color RenderTheme::activeListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.activeListBoxSelectionBackgroundColor.isValid())
        cache.activeListBoxSelectionBackgroundColor = platformActiveListBoxSelectionBackgroundColor(options);
    return cache.activeListBoxSelectionBackgroundColor;
}

Color RenderTheme::inactiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.inactiveListBoxSelectionBackgroundColor.isValid())
        cache.inactiveListBoxSelectionBackgroundColor = platformInactiveListBoxSelectionBackgroundColor(options);
    return cache.inactiveListBoxSelectionBackgroundColor;
}

Color RenderTheme::activeListBoxSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.activeListBoxSelectionForegroundColor.isValid() && supportsListBoxSelectionForegroundColors(options))
        cache.activeListBoxSelectionForegroundColor = platformActiveListBoxSelectionForegroundColor(options);
    return cache.activeListBoxSelectionForegroundColor;
}

Color RenderTheme::inactiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.inactiveListBoxSelectionForegroundColor.isValid() && supportsListBoxSelectionForegroundColors(options))
        cache.inactiveListBoxSelectionForegroundColor = platformInactiveListBoxSelectionForegroundColor(options);
    return cache.inactiveListBoxSelectionForegroundColor;
}

Color RenderTheme::platformActiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const
{
    // Use a blue color by default if the platform theme doesn't define anything.
    return Color::blue;
}

Color RenderTheme::platformActiveSelectionForegroundColor(OptionSet<StyleColorOptions>) const
{
    // Use a white color by default if the platform theme doesn't define anything.
    return Color::white;
}

Color RenderTheme::platformInactiveSelectionBackgroundColor(OptionSet<StyleColorOptions>) const
{
    // Use a grey color by default if the platform theme doesn't define anything.
    // This color matches Firefox's inactive color.
    return SRGBA<uint8_t> { 176, 176, 176 };
}

Color RenderTheme::platformInactiveSelectionForegroundColor(OptionSet<StyleColorOptions>) const
{
    // Use a black color by default.
    return Color::black;
}

Color RenderTheme::platformActiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    return platformActiveSelectionBackgroundColor(options);
}

Color RenderTheme::platformActiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    return platformActiveSelectionForegroundColor(options);
}

Color RenderTheme::platformInactiveListBoxSelectionBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    return platformInactiveSelectionBackgroundColor(options);
}

Color RenderTheme::platformInactiveListBoxSelectionForegroundColor(OptionSet<StyleColorOptions> options) const
{
    return platformInactiveSelectionForegroundColor(options);
}

int RenderTheme::baselinePosition(const RenderBox& box) const
{
#if USE(NEW_THEME)
    return box.height() + box.marginTop() + Theme::singleton().baselinePositionAdjustment(box.style().effectiveAppearance()) * box.style().effectiveZoom();
#else
    return box.height() + box.marginTop();
#endif
}

bool RenderTheme::isControlContainer(StyleAppearance appearance) const
{
    // There are more leaves than this, but we'll patch this function as we add support for
    // more controls.
    return appearance != StyleAppearance::Checkbox && appearance != StyleAppearance::Radio;
}

bool RenderTheme::isControlStyled(const RenderStyle& style, const RenderStyle& userAgentStyle) const
{
    switch (style.effectiveAppearance()) {
    case StyleAppearance::PushButton:
    case StyleAppearance::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
#endif
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
    case StyleAppearance::Listbox:
    case StyleAppearance::Menulist:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Meter:
    // FIXME: SearchFieldPart should be included here when making search fields style-able.
    case StyleAppearance::TextField:
    case StyleAppearance::TextArea:
        // Test the style to see if the UA border and background match.
        return !style.borderAndBackgroundEqual(userAgentStyle);
    default:
        return false;
    }
}

void RenderTheme::adjustRepaintRect(const RenderObject& renderer, FloatRect& rect)
{
#if USE(NEW_THEME)
    ControlStates states(extractControlStatesForRenderer(renderer));
    Theme::singleton().inflateControlPaintRect(renderer.style().effectiveAppearance(), states, rect, renderer.style().effectiveZoom());
#else
    UNUSED_PARAM(renderer);
    UNUSED_PARAM(rect);
#endif
}

bool RenderTheme::supportsFocusRing(const RenderStyle& style) const
{
    return style.hasEffectiveAppearance()
        && style.effectiveAppearance() != StyleAppearance::TextField
        && style.effectiveAppearance() != StyleAppearance::TextArea
        && style.effectiveAppearance() != StyleAppearance::MenulistButton
        && style.effectiveAppearance() != StyleAppearance::Listbox;
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
    if (isWindowActive(o))
        states.add(ControlStates::States::WindowActive);
    if (isIndeterminate(o))
        states.add(ControlStates::States::Indeterminate);
    if (isPresenting(o))
        states.add(ControlStates::States::Presenting);
    return states;
}

bool RenderTheme::isWindowActive(const RenderObject& renderer) const
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
    if (auto* element = dynamicDowncast<Element>(renderer.node()))
        return !element->isDisabledFormControl();
    return true;
}

bool RenderTheme::isFocused(const RenderObject& renderer) const
{
    auto element = dynamicDowncast<Element>(renderer.node());
    if (!element)
        return false;

    RefPtr delegate = element;
    if (is<SliderThumbElement>(element))
        delegate = downcast<SliderThumbElement>(*element).hostInput();

    Document& document = delegate->document();
    Frame* frame = document.frame();
    return delegate == document.focusedElement() && frame && frame->selection().isFocusedAndActive();
}

bool RenderTheme::isPressed(const RenderObject& renderer) const
{
    if (auto* element = dynamicDowncast<Element>(renderer.node()))
        return element->active();
    return false;
}

bool RenderTheme::isSpinUpButtonPartPressed(const RenderObject& renderer) const
{
    if (auto* spinButton = dynamicDowncast<SpinButtonElement>(renderer.node()))
        return spinButton->active() && spinButton->upDownState() == SpinButtonElement::Up;
    return false;
}

bool RenderTheme::isReadOnlyControl(const RenderObject& renderer) const
{
    if (auto* element = dynamicDowncast<HTMLFormControlElement>(renderer.node()))
        return !static_cast<Element&>(*element).matchesReadWritePseudoClass();
    return false;
}

bool RenderTheme::isHovered(const RenderObject& renderer) const
{
    if (auto* spinButton = dynamicDowncast<SpinButtonElement>(renderer.node()))
        return spinButton->hovered() && spinButton->upDownState() != SpinButtonElement::Indeterminate;
    if (auto* element = dynamicDowncast<Element>(renderer.node()))
        return element->hovered();
    return false;
}

bool RenderTheme::isSpinUpButtonPartHovered(const RenderObject& renderer) const
{
    if (auto* spinButton = dynamicDowncast<SpinButtonElement>(renderer.node()))
        return spinButton->upDownState() == SpinButtonElement::Up;
    return false;
}

bool RenderTheme::isPresenting(const RenderObject& o) const
{
    return is<HTMLInputElement>(o.node()) && downcast<HTMLInputElement>(*o.node()).isPresentingAttachedView();
}

bool RenderTheme::isDefault(const RenderObject& o) const
{
    // A button should only have the default appearance if the page is active
    if (!isWindowActive(o))
        return false;

    return o.style().effectiveAppearance() == StyleAppearance::DefaultButton;
}

#if ENABLE(DATALIST_ELEMENT)
bool RenderTheme::hasListButton(const RenderObject& renderer) const
{
    if (!is<HTMLInputElement>(renderer.generatingNode()))
        return false;

    const auto& input = downcast<HTMLInputElement>(*(renderer.generatingNode()));
    return input.list();
}

bool RenderTheme::hasListButtonPressed(const RenderObject& renderer) const
{
    const auto* input = downcast<HTMLInputElement>(renderer.generatingNode());
    if (!input)
        return false;

    if (auto* buttonElement = input->dataListButtonElement())
        return buttonElement->active();

    return false;
}
#endif

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

void RenderTheme::adjustMenuListStyle(RenderStyle& style, const Element*) const
{
    style.setOverflowX(Overflow::Visible);
    style.setOverflowY(Overflow::Visible);
}

void RenderTheme::adjustMeterStyle(RenderStyle& style, const Element*) const
{
    style.setBoxShadow(nullptr);
}

FloatSize RenderTheme::meterSizeForBounds(const RenderMeter&, const FloatRect& bounds) const
{
    return bounds.size();
}

bool RenderTheme::supportsMeter(StyleAppearance, const HTMLMeterElement&) const
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

String RenderTheme::attachmentStyleSheet() const
{
    ASSERT(DeprecatedGlobalSettings::attachmentElementEnabled());
    return "attachment { appearance: auto; }"_s;
}

bool RenderTheme::paintAttachment(const RenderObject&, const PaintInfo&, const IntRect&)
{
    return false;
}

#endif

#if ENABLE(INPUT_TYPE_COLOR)

String RenderTheme::colorInputStyleSheet(const Settings&) const
{
    return "input[type=\"color\"] { appearance: auto; inline-size: 44px; block-size: 23px; box-sizing: border-box; outline: none; } "_s;
}

#endif // ENABLE(INPUT_TYPE_COLOR)

#if ENABLE(DATALIST_ELEMENT)

String RenderTheme::dataListStyleSheet() const
{
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
    auto appearance = o.style().effectiveAppearance();
    // We don't support ticks on alternate sliders like MediaVolumeSliders.
    if (appearance != StyleAppearance::SliderHorizontal && appearance != StyleAppearance::SliderVertical)
        return;
    bool isHorizontal = appearance == StyleAppearance::SliderHorizontal;

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
    bool isReversedInlineDirection = (!isHorizontal && o.style().isHorizontalWritingMode()) || !o.style().isLeftToRightDirection();
    for (auto& optionElement : dataList->suggestions()) {
        if (auto optionValue = input.listOptionValueAsDouble(optionElement)) {
            double tickFraction = (*optionValue - min) / (max - min);
            double tickRatio = isReversedInlineDirection ? 1.0 - tickFraction : tickFraction;
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

#if ENABLE(SERVICE_CONTROLS)
void RenderTheme::adjustImageControlsButtonStyle(RenderStyle&, const Element*) const
{
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

IntRect RenderTheme::progressBarRectForBounds(const RenderProgress&, const IntRect& bounds) const
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

auto RenderTheme::colorCache(OptionSet<StyleColorOptions> options) const -> ColorCache&
{
    auto optionsIgnoringVisitedLink = options;
    optionsIgnoringVisitedLink.remove(StyleColorOptions::ForVisitedLink);

    return m_colorCacheMap.ensure(optionsIgnoringVisitedLink.toRaw(), [] {
        return ColorCache();
    }).iterator->value;
}

Color RenderTheme::systemColor(CSSValueID cssValueId, OptionSet<StyleColorOptions> options) const
{
    switch (cssValueId) {
    // https://drafts.csswg.org/css-color-4/#valdef-system-color-canvas
    // Background of application content or documents.
    case CSSValueCanvas:
        return Color::white;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-canvastext
    // Text in application content or documents.
    case CSSValueCanvastext:
        return Color::black;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-linktext
    // Text in non-active, non-visited links. For light backgrounds, traditionally blue.
    case CSSValueLinktext:
        return SRGBA<uint8_t> { 0, 0, 238 };

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-visitedtext
    // Text in visited links. For light backgrounds, traditionally purple.
    case CSSValueVisitedtext:
        return SRGBA<uint8_t> { 85, 26, 139 };

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-activetext
    // Text in active links. For light backgrounds, traditionally red.
    case CSSValueActivetext:
    case CSSValueWebkitActivelink: // Non-standard addition.
        return Color::red;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-buttonface
    // The face background color for push buttons.
    case CSSValueButtonface:
        return Color::lightGray;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-buttontext
    // Text on push buttons.
    case CSSValueButtontext:
        return Color::black;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-buttonborder
    // The base border color for push buttons.
    case CSSValueButtonborder:
        return Color::white;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-field
    // Background of input fields.
    case CSSValueField:
        return Color::white;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-fieldtext
    // Text in input fields.
    case CSSValueFieldtext:
        return Color::black;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-highlight
    // Background of selected text, for example from ::selection.
    case CSSValueHighlight:
        return SRGBA<uint8_t> { 181, 213, 255 };

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-highlighttext
    // Text of selected text.
    case CSSValueHighlighttext:
        return Color::black;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-selecteditem
    // Background of selected items, for example a selected checkbox.
    case CSSValueSelecteditem:
        return Color::lightGray;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-selecteditemtext
    // Text of selected items.
    case CSSValueSelecteditemtext:
        return Color::black;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-mark
    // Background of text that has been specially marked (such as by the HTML mark element).
    case CSSValueMark:
        return Color::yellow;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-marktext
    // Text that has been specially marked (such as by the HTML mark element).
    case CSSValueMarktext:
        return Color::black;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-graytext
    // Disabled text. (Often, but not necessarily, gray.)
    case CSSValueGraytext:
        return Color::darkGray;

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-accentcolor
    // Background of accented user interface controls.
    case CSSValueAccentcolor:
        return SRGBA<uint8_t> { 0, 122, 255 };

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-accentcolortext
    // Text of accented user interface controls.
    case CSSValueAccentcolortext:
        return Color::black;

    // Non-standard addition.
    case CSSValueActivebuttontext:
        return Color::black;

    // Non-standard addition.
    case CSSValueText:
        return Color::black;

    // Non-standard addition.
    case CSSValueWebkitLink:
        return options.contains(StyleColorOptions::ForVisitedLink) ? SRGBA<uint8_t> { 85, 26, 139 } : SRGBA<uint8_t> { 0, 0, 238 };

    // Deprecated system-colors:
    // https://drafts.csswg.org/css-color-4/#deprecated-system-colors

    // FIXME: CSS Color 4 imposes same-as requirements on all the deprecated
    // system colors - https://webkit.org/b/245609.

    // https://drafts.csswg.org/css-color-4/#activeborder
    // DEPRECATED: Active window border.
    case CSSValueActiveborder:
        return Color::white;

    // https://drafts.csswg.org/css-color-4/#activecaption
    // DEPRECATED: Active window caption.
    case CSSValueActivecaption:
        return SRGBA<uint8_t> { 204, 204, 204 };

    // https://drafts.csswg.org/css-color-4/#appworkspace
    // DEPRECATED: Background color of multiple document interface.
    case CSSValueAppworkspace:
        return Color::white;

    // https://drafts.csswg.org/css-color-4/#background
    // DEPRECATED: Desktop background.
    case CSSValueBackground:
        return SRGBA<uint8_t> { 99, 99, 206 };

    // https://drafts.csswg.org/css-color-4/#buttonhighlight
    // DEPRECATED: The color of the border facing the light source for 3-D elements that
    // appear 3-D due to one layer of surrounding border.
    case CSSValueButtonhighlight:
        return SRGBA<uint8_t> { 221, 221, 221 };

    // https://drafts.csswg.org/css-color-4/#buttonshadow
    // DEPRECATED: The color of the border away from the light source for 3-D elements that
    // appear 3-D due to one layer of surrounding border.
    case CSSValueButtonshadow:
        return SRGBA<uint8_t> { 136, 136, 136 };

    // https://drafts.csswg.org/css-color-4/#captiontext
    // DEPRECATED: Text in caption, size box, and scrollbar arrow box.
    case CSSValueCaptiontext:
        return Color::black;

    // https://drafts.csswg.org/css-color-4/#inactiveborder
    // DEPRECATED: Inactive window border.
    case CSSValueInactiveborder:
        return Color::white;

    // https://drafts.csswg.org/css-color-4/#inactivecaption
    // DEPRECATED: Inactive window caption.
    case CSSValueInactivecaption:
        return Color::white;

    // https://drafts.csswg.org/css-color-4/#inactivecaptiontext
    // DEPRECATED: Color of text in an inactive caption.
    case CSSValueInactivecaptiontext:
        return SRGBA<uint8_t> { 127, 127, 127 };

    // https://drafts.csswg.org/css-color-4/#infobackground
    // DEPRECATED: Background color for tooltip controls.
    case CSSValueInfobackground:
        return SRGBA<uint8_t> { 251, 252, 197 };

    // https://drafts.csswg.org/css-color-4/#infotext
    // DEPRECATED: Text color for tooltip controls.
    case CSSValueInfotext:
        return Color::black;

    // https://drafts.csswg.org/css-color-4/#menu
    // DEPRECATED: Menu background.
    case CSSValueMenu:
        return Color::lightGray;

    // https://drafts.csswg.org/css-color-4/#menutext
    // DEPRECATED: Text in menus.
    case CSSValueMenutext:
        return Color::black;

    // https://drafts.csswg.org/css-color-4/#scrollbar
    // DEPRECATED: Scroll bar gray area.
    case CSSValueScrollbar:
        return Color::white;

    // https://drafts.csswg.org/css-color-4/#threeddarkshadow
    // DEPRECATED: The color of the darker (generally outer) of the two borders away from
    // thelight source for 3-D elements that appear 3-D due to two concentric layers of
    // surrounding border.
    case CSSValueThreeddarkshadow:
        return SRGBA<uint8_t> { 102, 102, 102 };

    // https://drafts.csswg.org/css-color-4/#threedface
    // DEPRECATED: The face background color for 3-D elements that appear 3-D due to two
    // concentric layers of surrounding border
    case CSSValueThreedface:
        return Color::lightGray;

    // https://drafts.csswg.org/css-color-4/#threedhighlight
    // DEPRECATED: The color of the lighter (generally outer) of the two borders facing
    // the light source for 3-D elements that appear 3-D due to two concentric layers of
    // surrounding border.
    case CSSValueThreedhighlight:
        return SRGBA<uint8_t> { 221, 221, 221 };

    // https://drafts.csswg.org/css-color-4/#threedlightshadow
    // DEPRECATED: The color of the darker (generally inner) of the two borders facing
    // the light source for 3-D elements that appear 3-D due to two concentric layers of
    // surrounding border
    case CSSValueThreedlightshadow:
        return Color::lightGray;

    // https://drafts.csswg.org/css-color-4/#threedshadow
    // DEPRECATED: The color of the lighter (generally inner) of the two borders away
    // from the light source for 3-D elements that appear 3-D due to two concentric layers
    // of surrounding border.
    case CSSValueThreedshadow:
        return SRGBA<uint8_t> { 136, 136, 136 };

    // https://drafts.csswg.org/css-color-4/#window
    // DEPRECATED: Window background.
    case CSSValueWindow:
        return Color::white;

    // https://drafts.csswg.org/css-color-4/#windowframe
    // DEPRECATED: Window frame.
    case CSSValueWindowframe:
        return SRGBA<uint8_t> { 204, 204, 204 };

    // https://drafts.csswg.org/css-color-4/#windowtext
    // DEPRECATED: Text in windows.
    case CSSValueWindowtext:
        return Color::black;

    default:
        return { };
    }
}

Color RenderTheme::textSearchHighlightColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.textSearchHighlightColor.isValid())
        cache.textSearchHighlightColor = platformTextSearchHighlightColor(options);
    return cache.textSearchHighlightColor;
}

Color RenderTheme::platformTextSearchHighlightColor(OptionSet<StyleColorOptions>) const
{
    return Color::yellow;
}

Color RenderTheme::annotationHighlightColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.annotationHighlightColor.isValid())
        cache.annotationHighlightColor = transformSelectionBackgroundColor(platformAnnotationHighlightColor(options), options);
    return cache.annotationHighlightColor;
}

Color RenderTheme::platformAnnotationHighlightColor(OptionSet<StyleColorOptions>) const
{
    return Color::yellow;
}

Color RenderTheme::defaultButtonTextColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.defaultButtonTextColor.isValid())
        cache.defaultButtonTextColor = platformDefaultButtonTextColor(options);
    return cache.defaultButtonTextColor;
}

Color RenderTheme::platformDefaultButtonTextColor(OptionSet<StyleColorOptions> options) const
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
    auto hsla = textColor.toColorTypeLossy<HSLA<float>>().resolved();
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

Color RenderTheme::focusRingColor(OptionSet<StyleColorOptions> options) const
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
