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

#include "CSSValueKeywords.h"
#include "ColorBlending.h"
#include "ColorLuminance.h"
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
#include "LocalizedStrings.h"
#include "MeterPart.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderMeter.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "ShadowPseudoIds.h"
#include "SliderThumbElement.h"
#include "SpinButtonElement.h"
#include "StringTruncator.h"
#include "TextAreaPart.h"
#include "TextControlInnerElements.h"
#include "TextFieldPart.h"
#include "ToggleButtonPart.h"
#include <wtf/FileSystem.h>
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

ControlPartType RenderTheme::adjustAppearanceForElement(RenderStyle& style, const Element* element, ControlPartType autoAppearance) const
{
    if (!element)
        return ControlPartType::NoControl;

    auto type = style.effectiveAppearance();
    if (type == autoAppearance)
        return type;

    // Aliases of 'auto'.
    // https://drafts.csswg.org/css-ui-4/#typedef-appearance-compat-auto
    if (type == ControlPartType::Auto
        || type == ControlPartType::SearchField
        || type == ControlPartType::TextArea
        || type == ControlPartType::Checkbox
        || type == ControlPartType::Radio
        || type == ControlPartType::Listbox
        || type == ControlPartType::Meter
        || type == ControlPartType::ProgressBar
        || type == ControlPartType::SquareButton
        || type == ControlPartType::PushButton
        || type == ControlPartType::SliderHorizontal
        || type == ControlPartType::Menulist) {
        style.setEffectiveAppearance(autoAppearance);
        return autoAppearance;
    }

    // The following keywords should work well for some element types
    // even if their default appearances are different from the keywords.
    if (type == ControlPartType::Button) {
        if (autoAppearance == ControlPartType::PushButton || autoAppearance == ControlPartType::SquareButton)
            return type;
        style.setEffectiveAppearance(autoAppearance);
        return autoAppearance;
    }

    if (type == ControlPartType::MenulistButton) {
        if (autoAppearance == ControlPartType::Menulist)
            return type;
        style.setEffectiveAppearance(autoAppearance);
        return autoAppearance;
    }

    if (type == ControlPartType::TextField) {
        if (is<HTMLInputElement>(*element) && downcast<HTMLInputElement>(*element).isSearchField())
            return type;
        style.setEffectiveAppearance(autoAppearance);
        return autoAppearance;
    }

    return type;
}

static bool isAppearanceAllowedForAllElements(ControlPartType type)
{
#if ENABLE(APPLE_PAY)
    if (type == ControlPartType::ApplePayButton)
        return true;
#endif

    UNUSED_PARAM(type);
    return false;
}

void RenderTheme::adjustStyle(RenderStyle& style, const Element* element, const RenderStyle* userAgentAppearanceStyle)
{
    auto autoAppearance = autoAppearanceForElement(style, element);
    auto type = adjustAppearanceForElement(style, element, autoAppearance);

    if (type == ControlPartType::NoControl)
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
        switch (type) {
        case ControlPartType::Menulist:
            type = ControlPartType::MenulistButton;
            break;
        default:
            type = ControlPartType::NoControl;
            break;
        }

        style.setEffectiveAppearance(type);
    }

    if (!isAppearanceAllowedForAllElements(type)
        && !userAgentAppearanceStyle
        && autoAppearance == ControlPartType::NoControl
        && !style.borderAndBackgroundEqual(RenderStyle::defaultStyle()))
        style.setEffectiveAppearance(ControlPartType::NoControl);

    if (!style.hasEffectiveAppearance())
        return;

    if (!supportsBoxShadow(style))
        style.setBoxShadow(nullptr);

#if USE(NEW_THEME)
    switch (type) {
    case ControlPartType::Checkbox:
    case ControlPartType::InnerSpinButton:
    case ControlPartType::Radio:
    case ControlPartType::PushButton:
    case ControlPartType::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
#endif
    case ControlPartType::DefaultButton:
    case ControlPartType::Button: {
        // Border
        LengthBox borderBox(style.borderTopWidth(), style.borderRightWidth(), style.borderBottomWidth(), style.borderLeftWidth());
        borderBox = Theme::singleton().controlBorder(type, style.fontCascade(), borderBox, style.effectiveZoom());
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
        LengthBox paddingBox = Theme::singleton().controlPadding(type, style.fontCascade(), style.paddingBox(), style.effectiveZoom());
        if (paddingBox != style.paddingBox())
            style.setPaddingBox(WTFMove(paddingBox));

        // Whitespace
        if (Theme::singleton().controlRequiresPreWhiteSpace(type))
            style.setWhiteSpace(WhiteSpace::Pre);

        // Width / Height
        // The width and height here are affected by the zoom.
        // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
        LengthSize controlSize = Theme::singleton().controlSize(type, style.fontCascade(), { style.width(), style.height() }, style.effectiveZoom());
        if (controlSize.width != style.width())
            style.setWidth(WTFMove(controlSize.width));
        if (controlSize.height != style.height())
            style.setHeight(WTFMove(controlSize.height));

        // Min-Width / Min-Height
        LengthSize minControlSize = Theme::singleton().minimumControlSize(type, style.fontCascade(), { style.minWidth(), style.minHeight() }, { style.width(), style.height() }, style.effectiveZoom());
        if (minControlSize.width.value() > style.minWidth().value())
            style.setMinWidth(WTFMove(minControlSize.width));
        if (minControlSize.height.value() > style.minHeight().value())
            style.setMinHeight(WTFMove(minControlSize.height));

        // Font
        if (auto themeFont = Theme::singleton().controlFont(type, style.fontCascade(), style.effectiveZoom())) {
            // If overriding the specified font with the theme font, also override the line height with the standard line height.
            style.setLineHeight(RenderStyle::initialLineHeight());
            if (style.setFontDescription(WTFMove(themeFont.value())))
                style.fontCascade().update(nullptr);
        }

        // Special style that tells enabled default buttons in active windows to use the ActiveButtonText color.
        // The active window part of the test has to be done at paint time since it's not triggered by a style change.
        style.setInsideDefaultButton(type == ControlPartType::DefaultButton && element && !element->isDisabledFormControl());
        break;
    }
    default:
        break;
    }
#endif

    // Call the appropriate style adjustment method based off the appearance value.
    switch (type) {
#if !USE(NEW_THEME)
    case ControlPartType::Checkbox:
        return adjustCheckboxStyle(style, element);
    case ControlPartType::Radio:
        return adjustRadioStyle(style, element);
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
        return adjustColorWellStyle(style, element);
#endif
    case ControlPartType::PushButton:
    case ControlPartType::SquareButton:
    case ControlPartType::DefaultButton:
    case ControlPartType::Button:
        return adjustButtonStyle(style, element);
    case ControlPartType::InnerSpinButton:
        return adjustInnerSpinButtonStyle(style, element);
#endif
    case ControlPartType::TextField:
        return adjustTextFieldStyle(style, element);
    case ControlPartType::TextArea:
        return adjustTextAreaStyle(style, element);
    case ControlPartType::Menulist:
        return adjustMenuListStyle(style, element);
    case ControlPartType::MenulistButton:
        return adjustMenuListButtonStyle(style, element);
    case ControlPartType::SliderHorizontal:
    case ControlPartType::SliderVertical:
        return adjustSliderTrackStyle(style, element);
    case ControlPartType::SliderThumbHorizontal:
    case ControlPartType::SliderThumbVertical:
        return adjustSliderThumbStyle(style, element);
    case ControlPartType::SearchField:
        return adjustSearchFieldStyle(style, element);
    case ControlPartType::SearchFieldCancelButton:
        return adjustSearchFieldCancelButtonStyle(style, element);
    case ControlPartType::SearchFieldDecoration:
        return adjustSearchFieldDecorationPartStyle(style, element);
    case ControlPartType::SearchFieldResultsDecoration:
        return adjustSearchFieldResultsDecorationPartStyle(style, element);
    case ControlPartType::SearchFieldResultsButton:
        return adjustSearchFieldResultsButtonStyle(style, element);
    case ControlPartType::ProgressBar:
        return adjustProgressBarStyle(style, element);
    case ControlPartType::Meter:
        return adjustMeterStyle(style, element);
#if ENABLE(SERVICE_CONTROLS)
    case ControlPartType::ImageControlsButton:
        return adjustImageControlsButtonStyle(style, element);
#endif
    case ControlPartType::CapsLockIndicator:
        return adjustCapsLockIndicatorStyle(style, element);
#if ENABLE(APPLE_PAY)
    case ControlPartType::ApplePayButton:
        return adjustApplePayButtonStyle(style, element);
#endif
#if ENABLE(DATALIST_ELEMENT)
    case ControlPartType::ListButton:
        return adjustListButtonStyle(style, element);
#endif
    default:
        break;
    }
}

ControlPartType RenderTheme::autoAppearanceForElement(RenderStyle& style, const Element* elementPtr) const
{
    if (!elementPtr)
        return ControlPartType::NoControl;

#if ENABLE(SERVICE_CONTROLS)
    if (isImageControl(*elementPtr))
        return ControlPartType::ImageControlsButton;
#endif

    Ref element = *elementPtr;

    if (is<HTMLInputElement>(element)) {
        auto& input = downcast<HTMLInputElement>(element.get());

        if (input.isTextButton() || input.isUploadButton())
            return ControlPartType::PushButton;

        if (input.isCheckbox())
            return ControlPartType::Checkbox;

        if (input.isRadioButton())
            return ControlPartType::Radio;

        if (input.isSearchField())
            return ControlPartType::SearchField;

        if (input.isDateField() || input.isDateTimeLocalField() || input.isMonthField() || input.isTimeField() || input.isWeekField()) {
#if PLATFORM(IOS_FAMILY)
            return ControlPartType::MenulistButton;
#else
            return ControlPartType::TextField;
#endif
        }

#if ENABLE(INPUT_TYPE_COLOR)
        if (input.isColorControl())
            return ControlPartType::ColorWell;
#endif

        if (input.isRangeControl())
            return style.isHorizontalWritingMode() ? ControlPartType::SliderHorizontal : ControlPartType::SliderVertical;

        if (input.isTextField())
            return ControlPartType::TextField;

        // <input type=hidden|image|file>
        return ControlPartType::NoControl;
    }

    if (is<HTMLButtonElement>(element))
        return ControlPartType::Button;

    if (is<HTMLSelectElement>(element)) {
#if PLATFORM(IOS_FAMILY)
        return ControlPartType::MenulistButton;
#else
        auto& select = downcast<HTMLSelectElement>(element.get());
        return select.usesMenuList() ? ControlPartType::Menulist : ControlPartType::Listbox;
#endif
    }

    if (is<HTMLTextAreaElement>(element))
        return ControlPartType::TextArea;

    if (is<HTMLMeterElement>(element))
        return ControlPartType::Meter;

    if (is<HTMLProgressElement>(element))
        return ControlPartType::ProgressBar;

#if ENABLE(ATTACHMENT_ELEMENT)
    if (is<HTMLAttachmentElement>(element))
        return ControlPartType::Attachment;
#endif

    if (element->isInUserAgentShadowTree()) {
        auto& pseudo = element->shadowPseudoId();

#if ENABLE(DATALIST_ELEMENT)
        if (pseudo == ShadowPseudoIds::webkitListButton())
            return ControlPartType::ListButton;
#endif

        if (pseudo == ShadowPseudoIds::webkitCapsLockIndicator())
            return ControlPartType::CapsLockIndicator;

        if (pseudo == ShadowPseudoIds::webkitSearchCancelButton())
            return ControlPartType::SearchFieldCancelButton;

        if (is<SearchFieldResultsButtonElement>(element)) {
            if (!downcast<SearchFieldResultsButtonElement>(element.get()).canAdjustStyleForAppearance())
                return ControlPartType::NoControl;

            if (pseudo == ShadowPseudoIds::webkitSearchDecoration())
                return ControlPartType::SearchFieldDecoration;

            if (pseudo == ShadowPseudoIds::webkitSearchResultsDecoration())
                return ControlPartType::SearchFieldResultsDecoration;

            if (pseudo == ShadowPseudoIds::webkitSearchResultsButton())
                return ControlPartType::SearchFieldResultsButton;
        }

        if (pseudo == ShadowPseudoIds::webkitSliderThumb())
            return ControlPartType::SliderThumbHorizontal;

        if (pseudo == ShadowPseudoIds::webkitInnerSpinButton())
            return ControlPartType::InnerSpinButton;
    }

    return ControlPartType::NoControl;
}

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

RefPtr<ControlPart> RenderTheme::createControlPart(const RenderObject& renderer) const
{
    ControlPartType type = renderer.style().effectiveAppearance();

    switch (type) {
    case ControlPartType::NoControl:
    case ControlPartType::Auto:
        break;

    case ControlPartType::Checkbox:
    case ControlPartType::Radio:
        return ToggleButtonPart::create(type);

    case ControlPartType::PushButton:
    case ControlPartType::SquareButton:
    case ControlPartType::Button:
    case ControlPartType::DefaultButton:
    case ControlPartType::Menulist:
    case ControlPartType::MenulistButton:
        break;

    case ControlPartType::Meter:
        return createMeterPartForRenderer(renderer);

    case ControlPartType::ProgressBar:
    case ControlPartType::SliderHorizontal:
    case ControlPartType::SliderVertical:
    case ControlPartType::SearchField:
#if ENABLE(APPLE_PAY)
    case ControlPartType::ApplePayButton:
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
    case ControlPartType::Attachment:
    case ControlPartType::BorderlessAttachment:
#endif
        break;

    case ControlPartType::Listbox:
    case ControlPartType::TextArea:
        return TextAreaPart::create(type);

    case ControlPartType::TextField:
        return TextFieldPart::create();

    case ControlPartType::CapsLockIndicator:
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
#endif
#if ENABLE(SERVICE_CONTROLS)
    case ControlPartType::ImageControlsButton:
#endif
    case ControlPartType::InnerSpinButton:
#if ENABLE(DATALIST_ELEMENT)
    case ControlPartType::ListButton:
#endif
    case ControlPartType::SearchFieldDecoration:
    case ControlPartType::SearchFieldResultsDecoration:
    case ControlPartType::SearchFieldResultsButton:
    case ControlPartType::SearchFieldCancelButton:
    case ControlPartType::SliderThumbHorizontal:
    case ControlPartType::SliderThumbVertical:
        break;
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
    if (!isActive(renderer))
        states.add(ControlStyle::State::WindowInactive);
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
    return states;
}

ControlStyle RenderTheme::extractControlStyleForRenderer(const RenderObject& renderer) const
{
    return {
        extractControlStyleStatesForRenderer(renderer),
        renderer.style().computedFontPixelSize(),
        renderer.style().effectiveZoom(),
        renderer.style().effectiveAccentColor()
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
    auto controlStyle = extractControlStyleForRenderer(box);
    auto& context = paintInfo.context();

    context.drawControlPart(part, zoomedRect, deviceScaleFactor, controlStyle);
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
    
    auto type = box.style().effectiveAppearance();

    if (UNLIKELY(!canPaint(paintInfo, box.settings(), type)))
        return false;

    IntRect integralSnappedRect = snappedIntRect(rect);
    float deviceScaleFactor = box.document().deviceScaleFactor();
    FloatRect devicePixelSnappedRect = snapRectToDevicePixels(rect, deviceScaleFactor);

#if USE(NEW_THEME)
    float pageScaleFactor = box.page().pageScaleFactor();

    switch (type) {
    case ControlPartType::Checkbox:
    case ControlPartType::Radio:
    case ControlPartType::PushButton:
    case ControlPartType::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
#endif
    case ControlPartType::DefaultButton:
    case ControlPartType::Button:
    case ControlPartType::InnerSpinButton:
        updateControlStatesForRenderer(box, controlStates);
        Theme::singleton().paint(type, controlStates, paintInfo.context(), devicePixelSnappedRect, box.style().effectiveZoom(), &box.view().frameView(), deviceScaleFactor, pageScaleFactor, box.document().useSystemAppearance(), box.useDarkAppearance(), box.style().effectiveAccentColor());
        return false;
    default:
        break;
    }
#else
    UNUSED_PARAM(controlStates);
#endif

    // Call the appropriate paint method based off the appearance value.
    switch (type) {
#if !USE(NEW_THEME)
    case ControlPartType::Checkbox:
        return paintCheckbox(box, paintInfo, devicePixelSnappedRect);
    case ControlPartType::Radio:
        return paintRadio(box, paintInfo, devicePixelSnappedRect);
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
        return paintColorWell(box, paintInfo, integralSnappedRect);
#endif
    case ControlPartType::PushButton:
    case ControlPartType::SquareButton:
    case ControlPartType::DefaultButton:
    case ControlPartType::Button:
        return paintButton(box, paintInfo, integralSnappedRect);
    case ControlPartType::InnerSpinButton:
        return paintInnerSpinButton(box, paintInfo, integralSnappedRect);
#endif
    case ControlPartType::Menulist:
        return paintMenuList(box, paintInfo, devicePixelSnappedRect);
    case ControlPartType::Meter:
        return paintMeter(box, paintInfo, integralSnappedRect);
    case ControlPartType::ProgressBar:
        return paintProgressBar(box, paintInfo, integralSnappedRect);
    case ControlPartType::SliderHorizontal:
    case ControlPartType::SliderVertical:
        return paintSliderTrack(box, paintInfo, integralSnappedRect);
    case ControlPartType::SliderThumbHorizontal:
    case ControlPartType::SliderThumbVertical:
        return paintSliderThumb(box, paintInfo, integralSnappedRect);
    case ControlPartType::MenulistButton:
    case ControlPartType::TextField:
    case ControlPartType::TextArea:
    case ControlPartType::Listbox:
        return true;
    case ControlPartType::SearchField:
        return paintSearchField(box, paintInfo, integralSnappedRect);
    case ControlPartType::SearchFieldCancelButton:
        return paintSearchFieldCancelButton(box, paintInfo, integralSnappedRect);
    case ControlPartType::SearchFieldDecoration:
        return paintSearchFieldDecorationPart(box, paintInfo, integralSnappedRect);
    case ControlPartType::SearchFieldResultsDecoration:
        return paintSearchFieldResultsDecorationPart(box, paintInfo, integralSnappedRect);
    case ControlPartType::SearchFieldResultsButton:
        return paintSearchFieldResultsButton(box, paintInfo, integralSnappedRect);
#if ENABLE(SERVICE_CONTROLS)
    case ControlPartType::ImageControlsButton:
        return paintImageControlsButton(box, paintInfo, integralSnappedRect);
#endif
    case ControlPartType::CapsLockIndicator:
        return paintCapsLockIndicator(box, paintInfo, integralSnappedRect);
#if ENABLE(APPLE_PAY)
    case ControlPartType::ApplePayButton:
        return paintApplePayButton(box, paintInfo, integralSnappedRect);
#endif
#if ENABLE(DATALIST_ELEMENT)
    case ControlPartType::ListButton:
        return paintListButton(box, paintInfo, devicePixelSnappedRect);
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
    case ControlPartType::Attachment:
    case ControlPartType::BorderlessAttachment:
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
    return box.style().effectiveAppearance() != ControlPartType::NoControl;
#else
    FloatRect devicePixelSnappedRect = snapRectToDevicePixels(rect, box.document().deviceScaleFactor());
    // Call the appropriate paint method based off the appearance value.
    switch (box.style().effectiveAppearance()) {
    case ControlPartType::TextField:
        return paintTextField(box, paintInfo, devicePixelSnappedRect);
    case ControlPartType::Listbox:
    case ControlPartType::TextArea:
        return paintTextArea(box, paintInfo, devicePixelSnappedRect);
    case ControlPartType::MenulistButton:
    case ControlPartType::SearchField:
        return true;
    case ControlPartType::Checkbox:
    case ControlPartType::Radio:
    case ControlPartType::PushButton:
    case ControlPartType::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
#endif
    case ControlPartType::DefaultButton:
    case ControlPartType::Button:
    case ControlPartType::Menulist:
    case ControlPartType::Meter:
    case ControlPartType::ProgressBar:
    case ControlPartType::SliderHorizontal:
    case ControlPartType::SliderVertical:
    case ControlPartType::SliderThumbHorizontal:
    case ControlPartType::SliderThumbVertical:
    case ControlPartType::SearchFieldCancelButton:
    case ControlPartType::SearchFieldDecoration:
    case ControlPartType::SearchFieldResultsDecoration:
    case ControlPartType::SearchFieldResultsButton:
#if ENABLE(SERVICE_CONTROLS)
    case ControlPartType::ImageControlsButton:
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
    case ControlPartType::MenulistButton:
        paintMenuListButtonDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case ControlPartType::TextField:
        paintTextFieldDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case ControlPartType::TextArea:
        paintTextAreaDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case ControlPartType::Checkbox:
        paintCheckboxDecorations(box, paintInfo, integralSnappedRect);
        break;
    case ControlPartType::Radio:
        paintRadioDecorations(box, paintInfo, integralSnappedRect);
        break;
    case ControlPartType::PushButton:
        paintPushButtonDecorations(box, paintInfo, integralSnappedRect);
        break;
    case ControlPartType::SquareButton:
        paintSquareButtonDecorations(box, paintInfo, integralSnappedRect);
        break;
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
        paintColorWellDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
#endif
    case ControlPartType::Button:
        paintButtonDecorations(box, paintInfo, integralSnappedRect);
        break;
    case ControlPartType::Menulist:
        paintMenuListDecorations(box, paintInfo, integralSnappedRect);
        break;
    case ControlPartType::SliderThumbHorizontal:
    case ControlPartType::SliderThumbVertical:
        paintSliderThumbDecorations(box, paintInfo, integralSnappedRect);
        break;
    case ControlPartType::SearchField:
        paintSearchFieldDecorations(box, paintInfo, integralSnappedRect);
        break;
    case ControlPartType::Meter:
    case ControlPartType::ProgressBar:
    case ControlPartType::SliderHorizontal:
    case ControlPartType::SliderVertical:
    case ControlPartType::Listbox:
    case ControlPartType::DefaultButton:
    case ControlPartType::SearchFieldCancelButton:
    case ControlPartType::SearchFieldDecoration:
    case ControlPartType::SearchFieldResultsDecoration:
    case ControlPartType::SearchFieldResultsButton:
#if ENABLE(SERVICE_CONTROLS)
    case ControlPartType::ImageControlsButton:
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

bool RenderTheme::isControlContainer(ControlPartType appearance) const
{
    // There are more leaves than this, but we'll patch this function as we add support for
    // more controls.
    return appearance != ControlPartType::Checkbox && appearance != ControlPartType::Radio;
}

bool RenderTheme::isControlStyled(const RenderStyle& style, const RenderStyle& userAgentStyle) const
{
    switch (style.effectiveAppearance()) {
    case ControlPartType::PushButton:
    case ControlPartType::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
#endif
    case ControlPartType::DefaultButton:
    case ControlPartType::Button:
    case ControlPartType::Listbox:
    case ControlPartType::Menulist:
    case ControlPartType::ProgressBar:
    case ControlPartType::Meter:
    // FIXME: SearchFieldPart should be included here when making search fields style-able.
    case ControlPartType::TextField:
    case ControlPartType::TextArea:
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
        && style.effectiveAppearance() != ControlPartType::TextField
        && style.effectiveAppearance() != ControlPartType::TextArea
        && style.effectiveAppearance() != ControlPartType::MenulistButton
        && style.effectiveAppearance() != ControlPartType::Listbox;
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
    if (!isActive(o))
        return false;

    return o.style().effectiveAppearance() == ControlPartType::DefaultButton;
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

bool RenderTheme::supportsMeter(ControlPartType, const HTMLMeterElement&) const
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
    ASSERT(DeprecatedGlobalSettings::dataListElementEnabled());
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
    auto type = o.style().effectiveAppearance();
    // We don't support ticks on alternate sliders like MediaVolumeSliders.
    if (type != ControlPartType::SliderHorizontal && type != ControlPartType::SliderVertical)
        return;
    bool isHorizontal = type == ControlPartType::SliderHorizontal;

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
