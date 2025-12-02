/*
 * Copyright (C) 2005-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "BorderShape.h"
#include "ButtonPart.h"
#include "CSSContrastColorResolver.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "ColorBlending.h"
#include "ColorLuminance.h"
#include "ColorSerialization.h"
#include "ColorWellPart.h"
#include "ContainerNodeInlines.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "FileList.h"
#include "FloatConversion.h"
#include "FloatRoundedRect.h"
#include "FocusController.h"
#include "FontSelector.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "GraphicsTypes.h"
#include "HTMLAttachmentElement.h"
#include "HTMLButtonElement.h"
#include "HTMLDataListElement.h"
#include "HTMLInputElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLProgressElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "ImageAdapter.h"
#include "ImageControlsButtonPart.h"
#include "InnerSpinButtonPart.h"
#include "LocalFrame.h"
#include "LocalizedStrings.h"
#include "MenuListButtonPart.h"
#include "MenuListPart.h"
#include "MeterPart.h"
#include "Page.h"
#include "PaintInfo.h"
#include "ProgressBarPart.h"
#include "RenderMeter.h"
#include "RenderElementInlines.h"
#include "RenderProgress.h"
#include "RenderStyleSetters.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "SearchFieldCancelButtonPart.h"
#include "SearchFieldPart.h"
#include "SearchFieldResultsPart.h"
#include "SliderThumbElement.h"
#include "SliderThumbPart.h"
#include "SliderTrackPart.h"
#include "SpinButtonElement.h"
#include "StringTruncator.h"
#include "StylePadding.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "SwitchThumbPart.h"
#include "SwitchTrackPart.h"
#include "TextAreaPart.h"
#include "TextControlInnerElements.h"
#include "TextFieldPart.h"
#include "Theme.h"
#include "ToggleButtonPart.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "UserAgentParts.h"
#include <wtf/FileSystem.h>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsMac.h"
#endif

namespace WebCore {

using namespace CSS::Literals;
using namespace HTMLNames;

RenderTheme::RenderTheme() = default;
RenderTheme::~RenderTheme() = default;

StyleAppearance RenderTheme::adjustAppearanceForElement(RenderStyle& style, const RenderStyle& parentStyle, const Element* element, StyleAppearance autoAppearance) const
{
    if (!element) {
        style.setUsedAppearance(StyleAppearance::None);
        return StyleAppearance::None;
    }

    // Each user agent part for input type='color' controls
    // should use StyleAppearance::None if their parent is
    // using primitive appearance.

    // FIXME: (rdar://148625484) If the parent devolves
    // after the initial styles are applied, the children
    // are not immediately updated to match.

    if ((autoAppearance == StyleAppearance::ColorWellSwatch
        || autoAppearance == StyleAppearance::ColorWellSwatchOverlay
        || autoAppearance == StyleAppearance::ColorWellSwatchWrapper)
        && (parentStyle.usedAppearance() == StyleAppearance::None)) {
            style.setUsedAppearance(StyleAppearance::None);
            return StyleAppearance::None;
    }

    auto appearance = style.usedAppearance();
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
        style.setUsedAppearance(autoAppearance);
        return autoAppearance;
    }

    // The following keywords should work well for some element types
    // even if their default appearances are different from the keywords.
    if (appearance == StyleAppearance::Button) {
        if (autoAppearance == StyleAppearance::PushButton || autoAppearance == StyleAppearance::SquareButton)
            return appearance;
        style.setUsedAppearance(autoAppearance);
        return autoAppearance;
    }

    if (appearance == StyleAppearance::MenulistButton) {
        if (autoAppearance == StyleAppearance::Menulist)
            return appearance;
        style.setUsedAppearance(autoAppearance);
        return autoAppearance;
    }

    auto* inputElement = dynamicDowncast<HTMLInputElement>(element);

    if (appearance == StyleAppearance::TextField) {
        if (inputElement && inputElement->isSearchField())
            return appearance;
        style.setUsedAppearance(autoAppearance);
        return autoAppearance;
    }

    if (appearance == StyleAppearance::SliderVertical) {
        if (inputElement && inputElement->isRangeControl())
            return appearance;
        style.setUsedAppearance(autoAppearance);
        return autoAppearance;
    }

#if ENABLE(APPLE_PAY)
    // Only apply `appearance: -apple-pay-button` on buttons and non-form controls.
    if (appearance == StyleAppearance::ApplePayButton) {
        if (autoAppearance == StyleAppearance::Button)
            return appearance;

        if (!inputElement && autoAppearance == StyleAppearance::None)
            return appearance;

        style.setUsedAppearance(autoAppearance);
        return autoAppearance;
    }
#endif

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

static bool devolvableWidgetsEnabledAndSupported(const Element* element)
{
    bool devolvableWidgetsEnabled = element->document().settings().devolvableWidgetsEnabled();
#if PLATFORM(COCOA)
    return devolvableWidgetsEnabled && WTF::linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DevolvableWidgets);
#else
    return devolvableWidgetsEnabled;
#endif
}

static bool shouldCheckLegacyStylesForNativeAppearance(const Element* element)
{
#if PLATFORM(MAC)
#if ENABLE(FORM_CONTROL_REFRESH)
    return element && !element->document().settings().formControlRefreshEnabled();
#else
    UNUSED_PARAM(element);
    return true;
#endif
#else
    UNUSED_PARAM(element);
    return false;
#endif
}

bool RenderTheme::hasAppearanceForElementTypeFromUAStyle(const Element& element)
{
    // NOTE: This is just a legacy hard-coded list of elements that have some appearance value in html.css
    // FIXME: Remove when devolvable widgets are universally enabled.
    const auto& localName = element.localName();
    return localName == HTMLNames::inputTag
        || localName == HTMLNames::textareaTag
        || localName == HTMLNames::buttonTag
        || localName == HTMLNames::progressTag
        || localName == HTMLNames::selectTag
        || localName == HTMLNames::meterTag
        || (element.isInUserAgentShadowTree() && element.userAgentPart() == UserAgentParts::webkitListButton());
}

void RenderTheme::adjustStyle(RenderStyle& style, const RenderStyle& parentStyle, const Element* element)
{
    auto autoAppearance = autoAppearanceForElement(style, element);
    auto appearance = adjustAppearanceForElement(style, parentStyle, element, autoAppearance);
    if (appearance == StyleAppearance::None || appearance == StyleAppearance::Base)
        return;

    // Force inline and table display styles to be inline-block (except for table- which is block)
    if (style.display() == DisplayType::Inline || style.display() == DisplayType::InlineTable || style.display() == DisplayType::TableRowGroup
        || style.display() == DisplayType::TableHeaderGroup || style.display() == DisplayType::TableFooterGroup
        || style.display() == DisplayType::TableRow || style.display() == DisplayType::TableColumnGroup || style.display() == DisplayType::TableColumn
        || style.display() == DisplayType::TableCell || style.display() == DisplayType::TableCaption)
        style.setEffectiveDisplay(DisplayType::InlineBlock);
    else if (style.display() == DisplayType::ListItem || style.display() == DisplayType::Table)
        style.setEffectiveDisplay(DisplayType::Block);

    bool widgetMayDevolve = devolvableWidgetsEnabledAndSupported(element);
    bool widgetHasNativeAppearanceDisabled = widgetMayDevolve && element->isDevolvableWidget() && style.nativeAppearanceDisabled() && !isAppearanceAllowedForAllElements(appearance);
    bool hasAppearanceFromUAStyle = element && hasAppearanceForElementTypeFromUAStyle(*element);

    if (!widgetMayDevolve || shouldCheckLegacyStylesForNativeAppearance(element))
        widgetHasNativeAppearanceDisabled |= hasAppearanceFromUAStyle && isControlStyled(style);

    if (widgetHasNativeAppearanceDisabled) {
        switch (appearance) {
        case StyleAppearance::Menulist:
            appearance = StyleAppearance::MenulistButton;
            break;
        case StyleAppearance::MenulistButton:
            appearance = widgetMayDevolve ? StyleAppearance::MenulistButton : StyleAppearance::None;
            break;
#if PLATFORM(IOS_FAMILY)
        case StyleAppearance::ListButton:
#endif
        default:
            appearance = StyleAppearance::None;
            break;
        }

        style.setUsedAppearance(appearance);
    }

    if (appearance == StyleAppearance::SearchField && searchFieldShouldAppearAsTextField(style, element->document().settings())) {
        appearance = StyleAppearance::TextField;
        style.setUsedAppearance(appearance);
    }

    if (!isAppearanceAllowedForAllElements(appearance)
        && !hasAppearanceFromUAStyle
        && autoAppearance == StyleAppearance::None
        && !style.borderAndBackgroundEqual(RenderStyle::defaultStyleSingleton()))
        style.setUsedAppearance(StyleAppearance::None);

    if (!style.hasUsedAppearance())
        return;

    if (!supportsBoxShadow(style))
        style.setBoxShadow(CSS::Keyword::None { });

    switch (appearance) {
    case StyleAppearance::Checkbox:
        return adjustCheckboxStyle(style, element);
    case StyleAppearance::Radio:
        return adjustRadioStyle(style, element);
    case StyleAppearance::ColorWell:
        return adjustColorWellStyle(style, element);
    case StyleAppearance::ColorWellSwatch:
        return adjustColorWellSwatchStyle(style, element);
    case StyleAppearance::ColorWellSwatchOverlay:
        return adjustColorWellSwatchOverlayStyle(style, element);
    case StyleAppearance::ColorWellSwatchWrapper:
        return adjustColorWellSwatchWrapperStyle(style, element);
    case StyleAppearance::PushButton:
    case StyleAppearance::SquareButton:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
        return adjustButtonStyle(style, element);
    case StyleAppearance::InnerSpinButton:
        return adjustInnerSpinButtonStyle(style, element);
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
    case StyleAppearance::Switch:
        return adjustSwitchStyle(style, element);
    case StyleAppearance::SwitchThumb:
    case StyleAppearance::SwitchTrack:
        return adjustSwitchThumbOrSwitchTrackStyle(style);
    case StyleAppearance::ProgressBar:
        return adjustProgressBarStyle(style, element);
    case StyleAppearance::Meter:
        return adjustMeterStyle(style, element);
#if ENABLE(SERVICE_CONTROLS)
    case StyleAppearance::ImageControlsButton:
        return adjustImageControlsButtonStyle(style, element);
#endif
#if ENABLE(APPLE_PAY)
    case StyleAppearance::ApplePayButton:
        return adjustApplePayButtonStyle(style, element);
#endif
    case StyleAppearance::ListButton:
        return adjustListButtonStyle(style, element);
    default:
        break;
    }
}

StyleAppearance RenderTheme::autoAppearanceForElement(RenderStyle& style, const Element* elementPtr) const
{
    if (!elementPtr)
        return StyleAppearance::None;

    Ref element = *elementPtr;

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(element)) {
        if (input->isTextButton() || input->isUploadButton())
            return StyleAppearance::Button;

        if (input->isSwitch())
            return StyleAppearance::Switch;

        if (input->isCheckbox())
            return StyleAppearance::Checkbox;

        if (input->isRadioButton())
            return StyleAppearance::Radio;

        if (input->isSearchField())
            return StyleAppearance::SearchField;

        if (input->isDateField() || input->isDateTimeLocalField() || input->isMonthField() || input->isTimeField() || input->isWeekField()) {
#if PLATFORM(IOS_FAMILY)
            return StyleAppearance::MenulistButton;
#else
            return StyleAppearance::TextField;
#endif
        }

        if (input->isColorControl())
            return StyleAppearance::ColorWell;

        if (input->isRangeControl())
            return style.writingMode().isHorizontal() ? StyleAppearance::SliderHorizontal : StyleAppearance::SliderVertical;

        if (input->isTextField())
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

    if (RefPtr select = dynamicDowncast<HTMLSelectElement>(element)) {
#if PLATFORM(IOS_FAMILY)
        return StyleAppearance::MenulistButton;
#else
        return select->usesMenuList() ? StyleAppearance::Menulist : StyleAppearance::Listbox;
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
        auto& part = element->userAgentPart();

        if (part == UserAgentParts::webkitListButton())
            return StyleAppearance::ListButton;

        if (part == UserAgentParts::webkitSearchCancelButton())
            return StyleAppearance::SearchFieldCancelButton;

        if (RefPtr button = dynamicDowncast<SearchFieldResultsButtonElement>(element)) {
            if (!button->canAdjustStyleForAppearance())
                return StyleAppearance::None;

            if (part == UserAgentParts::webkitSearchDecoration())
                return StyleAppearance::SearchFieldDecoration;

            if (part == UserAgentParts::webkitSearchResultsDecoration())
                return StyleAppearance::SearchFieldResultsDecoration;

            if (part == UserAgentParts::webkitSearchResultsButton())
                return StyleAppearance::SearchFieldResultsButton;
        }

        if (part == UserAgentParts::webkitSliderThumb())
            return StyleAppearance::SliderThumbHorizontal;

        if (part == UserAgentParts::webkitInnerSpinButton())
            return StyleAppearance::InnerSpinButton;

        if (part == UserAgentParts::internalColorSwatchOverlay())
            return StyleAppearance::ColorWellSwatchOverlay;

        if (part == UserAgentParts::webkitColorSwatch())
            return StyleAppearance::ColorWellSwatch;

        if (part == UserAgentParts::webkitColorSwatchWrapper())
            return StyleAppearance::ColorWellSwatchWrapper;

        if (part == UserAgentParts::thumb())
            return StyleAppearance::SwitchThumb;

        if (part == UserAgentParts::track())
            return StyleAppearance::SwitchTrack;
    }

    return StyleAppearance::None;
}

#if ENABLE(APPLE_PAY)
static void updateApplePayButtonPartForRenderer(ApplePayButtonPart& applePayButtonPart, const RenderElement& renderer)
{
    CheckedRef style = renderer.style();

    auto platformLocale = [&] -> String {
        auto locale = style->computedLocale();
        if (locale.isAuto())
            return defaultLanguage(ShouldMinimizeLanguages::No);
        return Style::toPlatform(locale);
    }();

    applePayButtonPart.setButtonType(style->applePayButtonType());
    applePayButtonPart.setButtonStyle(style->applePayButtonStyle());
    applePayButtonPart.setLocale(WTFMove(platformLocale));
}
#endif

static void updateMeterPartForRenderer(MeterPart& meterPart, const RenderMeter& renderMeter)
{
    Ref element = *renderMeter.meterElement();
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

    meterPart.setGaugeRegion(gaugeRegion);
    meterPart.setValue(element->value());
    meterPart.setMinimum(element->min());
    meterPart.setMaximum(element->max());
}

static void updateProgressBarPartForRenderer(ProgressBarPart& progressBarPart, const RenderProgress& renderProgress)
{
    progressBarPart.setPosition(renderProgress.position());
    progressBarPart.setAnimationStartTime(renderProgress.animationStartTime().secondsSinceEpoch());
}

static void updateSliderTrackPartForRenderer(SliderTrackPart& sliderTrackPart, const RenderElement& renderer)
{
    Ref input = downcast<HTMLInputElement>(*renderer.element());
    ASSERT(input->isRangeControl());

    IntSize thumbSize;
    if (CheckedPtr thumbRenderer = input->sliderThumbElement()->renderer()) {
        const auto& thumbStyle = thumbRenderer->style();

        auto fixedWidth = thumbStyle.width().tryFixed();
        auto fixedHeight = thumbStyle.height().tryFixed();
        auto thumbWidth = fixedWidth ? static_cast<int>(fixedWidth->resolveZoom(thumbStyle.usedZoomForLength())) : 0;
        auto thumbHeight = fixedHeight ? static_cast<int>(fixedHeight->resolveZoom(thumbStyle.usedZoomForLength())) : 0;

        thumbSize = { thumbWidth, thumbHeight };
    }

    IntRect trackBounds;
    if (CheckedPtr trackRenderer = input->sliderTrackElement()->renderer()) {
        trackBounds = trackRenderer->absoluteBoundingBoxRectIgnoringTransforms();

        // We can ignoring transforms because transform is handled by the graphics context.
        auto sliderBounds = renderer.absoluteBoundingBoxRectIgnoringTransforms();

        // Make position relative to the transformed ancestor element.
        trackBounds.moveBy(-sliderBounds.location());
    }

    double minimum = input->minimum();
    double maximum = input->maximum();
    double thumbPosition = 0;
    if (maximum > minimum)
        thumbPosition = (input->valueAsNumber() - minimum) / (maximum - minimum);

    Vector<double> tickRatios;
    if (auto dataList = input->dataList()) {

        for (Ref optionElement : dataList->suggestions()) {
            auto optionValue = input->listOptionValueAsDouble(optionElement.get());
            if (!optionValue)
                continue;
            double tickRatio = (*optionValue - minimum) / (maximum - minimum);
            tickRatios.append(tickRatio);
        }
    }

    sliderTrackPart.setThumbSize(thumbSize);
    sliderTrackPart.setTrackBounds(trackBounds);
    sliderTrackPart.setThumbPosition(thumbPosition);
    sliderTrackPart.setTickRatios(WTFMove(tickRatios));
}

static void updateSwitchThumbPartForRenderer(SwitchThumbPart& switchThumbPart, const RenderElement& renderer)
{
    Ref input = downcast<HTMLInputElement>(*renderer.protectedNode()->shadowHost());
    ASSERT(input->isSwitch());

    switchThumbPart.setIsOn(input->isSwitchVisuallyOn());
    switchThumbPart.setProgress(input->switchAnimationVisuallyOnProgress());
}

static void updateSwitchTrackPartForRenderer(SwitchTrackPart& switchTrackPart, const RenderElement& renderer)
{
    Ref input = downcast<HTMLInputElement>(*renderer.protectedNode()->shadowHost());
    ASSERT(input->isSwitch());

    switchTrackPart.setIsOn(input->isSwitchVisuallyOn());
    switchTrackPart.setProgress(input->switchAnimationVisuallyOnProgress());
}

RefPtr<ControlPart> RenderTheme::createControlPart(const RenderElement& renderer) const
{
    auto appearance = renderer.style().usedAppearance();

    switch (appearance) {
    case StyleAppearance::None:
    case StyleAppearance::Auto:
    case StyleAppearance::Base:
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
        return MeterPart::create();

    case StyleAppearance::ProgressBar:
        return ProgressBarPart::create();

    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
        return SliderTrackPart::create(appearance);

    case StyleAppearance::SearchField:
        return SearchFieldPart::create();

#if ENABLE(APPLE_PAY)
    case StyleAppearance::ApplePayButton:
        return ApplePayButtonPart::create();
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

    case StyleAppearance::ColorWellSwatch:
    case StyleAppearance::ColorWellSwatchOverlay:
    case StyleAppearance::ColorWellSwatchWrapper:
        break;

    case StyleAppearance::ColorWell:
        return ColorWellPart::create();

#if ENABLE(SERVICE_CONTROLS)
    case StyleAppearance::ImageControlsButton:
        return ImageControlsButtonPart::create();
#endif

    case StyleAppearance::InnerSpinButton:
        return InnerSpinButtonPart::create();

    case StyleAppearance::ListButton:
        break;

    case StyleAppearance::SearchFieldDecoration:
        break;

    case StyleAppearance::SearchFieldResultsDecoration:
    case StyleAppearance::SearchFieldResultsButton:
        return SearchFieldResultsPart::create(appearance);

    case StyleAppearance::SearchFieldCancelButton:
        return SearchFieldCancelButtonPart::create();

    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
        return SliderThumbPart::create(appearance);

    case StyleAppearance::Switch:
        break;

    case StyleAppearance::SwitchThumb:
        return SwitchThumbPart::create();

    case StyleAppearance::SwitchTrack:
        return SwitchTrackPart::create();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void RenderTheme::updateControlPartForRenderer(ControlPart& part, const RenderElement& renderer) const
{
    if (auto* meterPart = dynamicDowncast<MeterPart>(part)) {
        updateMeterPartForRenderer(*meterPart, downcast<RenderMeter>(renderer));
        return;
    }

    if (auto* progressBarPart = dynamicDowncast<ProgressBarPart>(part)) {
        updateProgressBarPartForRenderer(*progressBarPart, downcast<RenderProgress>(renderer));
        return;
    }

    if (auto* sliderTrackPart = dynamicDowncast<SliderTrackPart>(part)) {
        updateSliderTrackPartForRenderer(*sliderTrackPart, renderer);
        return;
    }

    if (auto* switchThumbPart = dynamicDowncast<SwitchThumbPart>(part)) {
        updateSwitchThumbPartForRenderer(*switchThumbPart, renderer);
        return;
    }

    if (auto* switchTrackPart = dynamicDowncast<SwitchTrackPart>(part)) {
        updateSwitchTrackPartForRenderer(*switchTrackPart, renderer);
        return;
    }

#if ENABLE(APPLE_PAY)
    if (auto* applePayButtonPart = dynamicDowncast<ApplePayButtonPart>(part)) {
        updateApplePayButtonPartForRenderer(*applePayButtonPart, renderer);
        return;
    }
#endif
}

OptionSet<ControlStyle::State> RenderTheme::extractControlStyleStatesForRendererInternal(const RenderElement& renderer) const
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
    if (isFocused(renderer) && renderer.style().outlineStyle() == OutlineStyle::Auto)
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
    if (renderer.writingMode().isInlineFlipped())
        states.add(ControlStyle::State::InlineFlippedWritingMode);
    if (supportsLargeFormControls())
        states.add(ControlStyle::State::LargeControls);
    if (isReadOnlyControl(renderer))
        states.add(ControlStyle::State::ReadOnly);
    if (hasListButton(renderer)) {
        states.add(ControlStyle::State::ListButton);
        if (hasListButtonPressed(renderer))
            states.add(ControlStyle::State::ListButtonPressed);
    }
    if (!renderer.writingMode().isHorizontal())
        states.add(ControlStyle::State::VerticalWritingMode);
    return states;
}

static const RenderElement* effectiveRendererForAppearance(const RenderObject& renderObject)
{
    auto* renderer = dynamicDowncast<RenderElement>(renderObject);
    if (!renderer) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto type = renderer->style().usedAppearance();
    if (type == StyleAppearance::SearchFieldCancelButton
        || type == StyleAppearance::SwitchTrack
        || type == StyleAppearance::SwitchThumb) {
        RefPtr element = renderer->element();
        RefPtr<Node> input = element->shadowHost();
        if (!input)
            input = element;

        return dynamicDowncast<RenderBox>(input->renderer());
    }
    return renderer;
}

OptionSet<ControlStyle::State> RenderTheme::extractControlStyleStatesForRenderer(const RenderObject& renderObject) const
{
    if (CheckedPtr renderer = effectiveRendererForAppearance(renderObject))
        return extractControlStyleStatesForRendererInternal(*renderer);
    return { };
}

ControlStyle RenderTheme::extractControlStyleForRenderer(const RenderElement& renderObject) const
{
    CheckedPtr renderer = effectiveRendererForAppearance(renderObject);
    if (!renderer)
        return { };

    CheckedRef style = renderer->style();
    return {
        extractControlStyleStatesForRendererInternal(*renderer),
        style->computedFontSize(),
        style->usedZoom(),
        style->usedAccentColor(renderObject.styleColorOptions()),
        style->visitedDependentColorWithColorFilter(CSSPropertyColor),
        Style::evaluate<FloatBoxExtent>(style->borderWidth(), style->usedZoomForLength())
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

    updateControlPartForRenderer(part, box);

    float deviceScaleFactor = box.protectedDocument()->deviceScaleFactor();
    auto zoomedRect = snapRectToDevicePixels(rect, deviceScaleFactor);
    auto borderShape = BorderShape::shapeForBorderRect(box.checkedStyle().get(), LayoutRect(zoomedRect));
    auto controlStyle = extractControlStyleForRenderer(box);
    auto& context = paintInfo.context();

    context.drawControlPart(part, borderShape.deprecatedPixelSnappedRoundedRect(deviceScaleFactor), deviceScaleFactor, controlStyle);
    return false;
}

bool RenderTheme::paint(const RenderBox& box, const PaintInfo& paintInfo, const LayoutRect& rect)
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
    
    auto appearance = box.style().usedAppearance();

    if (!canPaint(paintInfo, box.settings(), appearance)) [[unlikely]]
        return false;

    float deviceScaleFactor = box.protectedDocument()->deviceScaleFactor();
    FloatRect devicePixelSnappedRect = snapRectToDevicePixels(rect, deviceScaleFactor);

    switch (appearance) {
    case StyleAppearance::Checkbox:
        return paintCheckbox(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::Radio:
        return paintRadio(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::ColorWell:
        return paintColorWell(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::ColorWellSwatch:
        return paintColorWellSwatch(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::PushButton:
    case StyleAppearance::SquareButton:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
        return paintButton(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::Menulist:
        return paintMenuList(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::MenulistButton:
        return paintMenuListButton(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::Meter:
        return paintMeter(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::ProgressBar:
        return paintProgressBar(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::SliderHorizontal:
    case StyleAppearance::SliderVertical:
        return paintSliderTrack(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderThumbVertical:
        return paintSliderThumb(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::TextField:
        return paintTextField(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::TextArea:
        return paintTextArea(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::Listbox:
        return true;
    case StyleAppearance::InnerSpinButton:
        return paintInnerSpinButton(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::SearchField:
        return paintSearchField(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::SearchFieldCancelButton:
        return paintSearchFieldCancelButton(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::SearchFieldDecoration:
        return paintSearchFieldDecorationPart(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::SearchFieldResultsDecoration:
        return paintSearchFieldResultsDecorationPart(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::SearchFieldResultsButton:
        return paintSearchFieldResultsButton(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::Switch:
        return true;
    case StyleAppearance::SwitchThumb:
        return paintSwitchThumb(box, paintInfo, devicePixelSnappedRect);
    case StyleAppearance::SwitchTrack:
        return paintSwitchTrack(box, paintInfo, devicePixelSnappedRect);
#if ENABLE(SERVICE_CONTROLS)
    case StyleAppearance::ImageControlsButton:
        return paintImageControlsButton(box, paintInfo, snappedIntRect(rect));
#endif
    case StyleAppearance::ListButton:
        return paintListButton(box, paintInfo, devicePixelSnappedRect);
#if ENABLE(ATTACHMENT_ELEMENT)
    case StyleAppearance::Attachment:
    case StyleAppearance::BorderlessAttachment:
        return paintAttachment(box, paintInfo, snappedIntRect(rect));
#endif
    default:
        break;
    }

    return true; // We don't support the appearance, so let the normal background/border paint.
}

bool RenderTheme::paintBorderOnly(const RenderBox& box, const PaintInfo& paintInfo)
{
    if (paintInfo.context().paintingDisabled())
        return false;

#if PLATFORM(IOS_FAMILY)
    return box.style().usedAppearance() != StyleAppearance::None && box.style().usedAppearance() != StyleAppearance::Base;
#else
    // Call the appropriate paint method based off the appearance value.
    switch (box.style().usedAppearance()) {
    case StyleAppearance::TextField:
    case StyleAppearance::Listbox:
    case StyleAppearance::TextArea:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::SearchField:
        return true;
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
    case StyleAppearance::PushButton:
    case StyleAppearance::SquareButton:
    case StyleAppearance::ColorWell:
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

    FloatRect devicePixelSnappedRect = snapRectToDevicePixels(rect, box.protectedDocument()->deviceScaleFactor());

    // Call the appropriate paint method based off the appearance value.
    switch (box.style().usedAppearance()) {
    case StyleAppearance::MenulistButton:
        paintMenuListButtonDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case StyleAppearance::TextField:
        paintTextFieldDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case StyleAppearance::TextArea:
        paintTextAreaDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case StyleAppearance::ColorWell:
        paintColorWellDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case StyleAppearance::Menulist:
        paintMenuListDecorations(box, paintInfo, devicePixelSnappedRect);
        break;
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SearchField:
        paintSearchFieldDecorations(box, paintInfo, devicePixelSnappedRect);
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
    return box.isHorizontalWritingMode() ? box.height() : LayoutUnit(box.width() / 2.0f);
}

bool RenderTheme::isControlContainer(StyleAppearance appearance) const
{
    // There are more leaves than this, but we'll patch this function as we add support for
    // more controls.
    return appearance != StyleAppearance::Checkbox && appearance != StyleAppearance::Radio;
}

bool RenderTheme::isControlStyled(const RenderStyle& style) const
{
    switch (style.usedAppearance()) {
    case StyleAppearance::PushButton:
    case StyleAppearance::SquareButton:
    case StyleAppearance::ColorWell:
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
        return style.nativeAppearanceDisabled();
    default:
        return false;
    }
}

bool RenderTheme::supportsFocusRing(const RenderElement&, const RenderStyle& style) const
{
    return style.hasUsedAppearance()
        && style.usedAppearance() != StyleAppearance::TextField
        && style.usedAppearance() != StyleAppearance::TextArea
        && style.usedAppearance() != StyleAppearance::MenulistButton
        && style.usedAppearance() != StyleAppearance::Listbox;
}

bool RenderTheme::isWindowActive(const RenderElement& renderer) const
{
    return renderer.page().focusController().isActive();
}

bool RenderTheme::isChecked(const RenderElement& renderer) const
{
    RefPtr element = dynamicDowncast<HTMLInputElement>(renderer.element());
    return element && element->matchesCheckedPseudoClass();
}

bool RenderTheme::isIndeterminate(const RenderElement& renderer) const
{
    // This does not currently support multiple elements and therefore radio buttons are excluded.
    // FIXME: However, what about <progress>?
    RefPtr input = dynamicDowncast<HTMLInputElement>(renderer.element());
    return input && input->isCheckbox() && input->matchesIndeterminatePseudoClass();
}

bool RenderTheme::isEnabled(const RenderElement& renderer) const
{
    RefPtr element = renderer.element();
    return element && !element->isDisabledFormControl();
}

bool RenderTheme::isFocused(const RenderElement& renderer) const
{
    RefPtr element = renderer.element();
    if (!element)
        return false;

    // FIXME: This should be part of RenderTheme::extractControlStyleForRenderer().
    RefPtr delegate = element;
    if (RefPtr sliderThumb = dynamicDowncast<SliderThumbElement>(element))
        delegate = sliderThumb->hostInput();

    Ref document = delegate->document();
    auto* frame = document->frame();
    return delegate == document->focusedElement() && frame && frame->checkedSelection()->isFocusedAndActive();
}

bool RenderTheme::isPressed(const RenderElement& renderer) const
{
    RefPtr element = renderer.element();
    return element && element->active();
}

bool RenderTheme::isSpinUpButtonPartPressed(const RenderElement& renderer) const
{
    if (RefPtr spinButton = dynamicDowncast<SpinButtonElement>(renderer.element()))
        return spinButton->active() && spinButton->upDownState() == SpinButtonElement::Up;
    return false;
}

bool RenderTheme::isReadOnlyControl(const RenderElement& renderer) const
{
    if (RefPtr element = renderer.element())
        return is<HTMLFormControlElement>(*element) && !element->matchesReadWritePseudoClass();
    return false;
}

bool RenderTheme::isHovered(const RenderElement& renderer) const
{
    if (RefPtr spinButton = dynamicDowncast<SpinButtonElement>(renderer.element()))
        return spinButton->hovered() && spinButton->upDownState() != SpinButtonElement::Indeterminate;
    if (RefPtr element = renderer.element())
        return element->hovered();
    return false;
}

bool RenderTheme::isSpinUpButtonPartHovered(const RenderElement& renderer) const
{
    if (RefPtr spinButton = dynamicDowncast<SpinButtonElement>(renderer.element()))
        return spinButton->upDownState() == SpinButtonElement::Up;
    return false;
}

bool RenderTheme::isPresenting(const RenderElement& renderer) const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(renderer.element());
    return input && input->isPresentingAttachedView();
}

bool RenderTheme::isDefault(const RenderElement& renderer) const
{
    // A button should only have the default appearance if the page is active
    if (!isWindowActive(renderer))
        return false;

    return renderer.style().usedAppearance() == StyleAppearance::DefaultButton;
}

bool RenderTheme::hasListButton(const RenderElement& renderer) const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(renderer.generatingElement());
    return input && input->hasDataList();
}

bool RenderTheme::hasListButtonPressed(const RenderElement& renderer) const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(renderer.generatingElement());
    if (!input)
        return false;

    RefPtr dataListButton = input->dataListButtonElement();
    return dataListButton && dataListButton->active();
}

std::optional<FontCascadeDescription> RenderTheme::controlFont(StyleAppearance, const FontCascade&, float) const
{
    return std::nullopt;
}

Style::PaddingBox RenderTheme::controlPadding(StyleAppearance appearance, const Style::PaddingBox& padding, float) const
{
    switch (appearance) {
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
        return Style::PaddingBox { 0_css_px };
    default:
        return padding;
    }
}

Style::PreferredSizePair RenderTheme::controlSize(StyleAppearance, const FontCascade&, const Style::PreferredSizePair& zoomedSize, float) const
{
    return zoomedSize;
}

Style::MinimumSizePair RenderTheme::minimumControlSize(StyleAppearance, const FontCascade&, const Style::MinimumSizePair&, float) const
{
    return { 0_css_px, 0_css_px };
}

Style::MinimumSizePair RenderTheme::minimumControlSize(StyleAppearance appearance, const FontCascade& fontCascade, const Style::MinimumSizePair& minSize, const Style::PreferredSizePair& preferredSize, float zoom) const
{
    auto minimumControlSize = this->minimumControlSize(appearance, fontCascade, minSize, zoom);

    auto resultWidth = minimumControlSize.width();
    auto resultHeight = minimumControlSize.height();

    // Other StyleAppearance types are composed controls with shadow subtree.
    if (appearance == StyleAppearance::Radio || appearance == StyleAppearance::Checkbox) {
        if (minSize.width().isIntrinsicOrLegacyIntrinsicOrAuto())
            resultWidth = preferredSize.width().asMinimumSize();
        if (minSize.height().isIntrinsicOrLegacyIntrinsicOrAuto())
            resultHeight = preferredSize.height().asMinimumSize();
    }

    return { WTFMove(resultWidth), WTFMove(resultHeight) };
}

Style::LineWidthBox RenderTheme::controlBorder(StyleAppearance appearance, const FontCascade&, const Style::LineWidthBox& zoomedBox, float, const Element*) const
{
    switch (appearance) {
    case StyleAppearance::PushButton:
    case StyleAppearance::Menulist:
    case StyleAppearance::SearchField:
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
        return Style::LineWidthBox { 0_css_px };
    default:
        return zoomedBox;
    }
}

// FIXME: iOS does not use this so arguably this should be better abstracted. Or maybe we should
// investigate if we can bring the various ports closer together.
void RenderTheme::adjustButtonOrCheckboxOrColorWellOrInnerSpinButtonOrRadioStyle(RenderStyle& style, const Element* element) const
{
    auto appearance = style.usedAppearance();
    CheckedRef fontCascade = style.fontCascade();

    auto borderBox = controlBorder(appearance, fontCascade.get(), style.borderWidth(), style.usedZoom(), element);

    auto supportsVerticalWritingMode = [](StyleAppearance appearance) {
        return appearance == StyleAppearance::Button
            || appearance == StyleAppearance::ColorWell
            || appearance == StyleAppearance::DefaultButton
            || appearance == StyleAppearance::SquareButton
            || appearance == StyleAppearance::PushButton;
    };
    // Transpose for vertical writing mode:
    if (!style.writingMode().isHorizontal() && supportsVerticalWritingMode(appearance))
        borderBox = Style::LineWidthBox { borderBox.left(), borderBox.top(), borderBox.right(), borderBox.bottom() };

    if (Style::evaluate<float>(borderBox.top(), style.usedZoomForLength()) != Style::evaluate<int>(style.borderTopWidth(), style.usedZoomForLength())) {
        if (!borderBox.top().isZero())
            style.setBorderTopWidth(borderBox.top());
        else
            style.resetBorderTop();
    }
    if (Style::evaluate<float>(borderBox.right(), style.usedZoomForLength()) != Style::evaluate<int>(style.borderRightWidth(), style.usedZoomForLength())) {
        if (!borderBox.right().isZero())
            style.setBorderRightWidth(borderBox.right());
        else
            style.resetBorderRight();
    }
    if (Style::evaluate<float>(borderBox.bottom(), style.usedZoomForLength()) != Style::evaluate<int>(style.borderBottomWidth(), style.usedZoomForLength())) {
        style.setBorderBottomWidth(borderBox.bottom());
        if (!borderBox.bottom().isZero())
            style.setBorderBottomWidth(borderBox.bottom());
        else
            style.resetBorderBottom();
    }
    if (Style::evaluate<float>(borderBox.left(), style.usedZoomForLength()) != Style::evaluate<int>(style.borderLeftWidth(), style.usedZoomForLength())) {
        style.setBorderLeftWidth(borderBox.left());
        if (!borderBox.left().isZero())
            style.setBorderLeftWidth(borderBox.left());
        else
            style.resetBorderLeft();
    }

    // Padding
    auto paddingBox = controlPadding(appearance, style.paddingBox(), style.usedZoom());
    if (paddingBox != style.paddingBox())
        style.setPaddingBox(WTFMove(paddingBox));

    // Whitespace
    if (controlRequiresPreWhiteSpace(appearance)) {
        style.setWhiteSpaceCollapse(WhiteSpaceCollapse::Preserve);
        style.setTextWrapMode(TextWrapMode::NoWrap);
    }

    // Width / Height
    // The width and height here are affected by the zoom.
    // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
    auto zoomForDeterminingControlSize = Style::ZoomFactor { usedZoomForComputedStyle(style), style.deviceScaleFactor() };
    auto controlSize = this->controlSize(appearance, fontCascade.get(), { style.width(), style.height() }, zoomForDeterminingControlSize.value);
    if (controlSize.width() != style.width())
        style.setWidth(Style::PreferredSize { controlSize.width() });
    if (controlSize.height() != style.height())
        style.setHeight(Style::PreferredSize { controlSize.height() });

    // Min-Width / Min-Height
    auto minimumControlSize = this->minimumControlSize(appearance, fontCascade.get(), { style.minWidth(), style.minHeight() }, { style.width(), style.height() }, zoomForDeterminingControlSize.value);

    // FIXME: The min-width/min-heigh value should use `calc-size()` when supported to make non-specified overrides work.

    auto usedZoomForLength = style.usedZoomForLength();
    if (auto fixedOverrideMinWidth = minimumControlSize.width().tryFixed()) {
        if (auto fixedOriginalMinWidth = style.minWidth().tryFixed()) {
            if (fixedOverrideMinWidth->resolveZoom(usedZoomForLength) > fixedOriginalMinWidth->resolveZoom(usedZoomForLength))
                style.setMinWidth(Style::MinimumSize(minimumControlSize.width()));
        } else if (auto percentageOriginalMinWidth = style.minWidth().tryPercentage()) {
            // FIXME: This really makes no sense but matches existing behavior. Should use a `calc(max(override, original))` here instead.
            if (fixedOverrideMinWidth->resolveZoom(usedZoomForLength) > percentageOriginalMinWidth->value)
                style.setMinWidth(Style::MinimumSize(minimumControlSize.width()));
        } else if (fixedOverrideMinWidth->isPositive()) {
            style.setMinWidth(Style::MinimumSize(minimumControlSize.width()));
        }
    } else if (auto percentageOverrideMinWidth = minimumControlSize.width().tryPercentage()) {
        if (auto fixedOriginalMinWidth = style.minWidth().tryFixed()) {
            // FIXME: This really makes no sense but matches existing behavior. Should use a `calc(max(override, original))` here instead.
            if (percentageOverrideMinWidth->value > fixedOriginalMinWidth->resolveZoom(usedZoomForLength))
                style.setMinWidth(Style::MinimumSize(minimumControlSize.width()));
        } else if (auto percentageOriginalMinWidth = style.minWidth().tryPercentage()) {
            if (percentageOverrideMinWidth->value > percentageOriginalMinWidth->value)
                style.setMinWidth(Style::MinimumSize(minimumControlSize.width()));
        } else if (percentageOverrideMinWidth->isPositive()) {
            style.setMinWidth(Style::MinimumSize(minimumControlSize.width()));
        }
    }
    if (auto fixedOverrideMinHeight = minimumControlSize.height().tryFixed()) {
        if (auto fixedOriginalMinHeight = style.minHeight().tryFixed()) {
            if (fixedOverrideMinHeight->resolveZoom(usedZoomForLength) > fixedOriginalMinHeight->resolveZoom(usedZoomForLength))
                style.setMinHeight(Style::MinimumSize(minimumControlSize.height()));
        } else if (auto percentageOriginalMinHeight = style.minHeight().tryPercentage()) {
            // FIXME: This really makes no sense but matches existing behavior. Should use a `calc(max(override, original))` here instead.
            if (fixedOverrideMinHeight->resolveZoom(usedZoomForLength) > percentageOriginalMinHeight->value)
                style.setMinHeight(Style::MinimumSize(minimumControlSize.height()));
        } else if (fixedOverrideMinHeight->isPositive()) {
            style.setMinHeight(Style::MinimumSize(minimumControlSize.height()));
        }
    } else if (auto percentageOverrideMinHeight = minimumControlSize.height().tryPercentage()) {
        if (auto fixedOriginalMinHeight = style.minHeight().tryFixed()) {
            // FIXME: This really makes no sense but matches existing behavior. Should use a `calc(max(override, original))` here instead.
            if (percentageOverrideMinHeight->value > fixedOriginalMinHeight->resolveZoom(usedZoomForLength))
                style.setMinHeight(Style::MinimumSize(minimumControlSize.height()));
        } else if (auto percentageOriginalMinHeight = style.minHeight().tryPercentage()) {
            if (percentageOverrideMinHeight->value > percentageOriginalMinHeight->value)
                style.setMinHeight(Style::MinimumSize(minimumControlSize.height()));
        } else if (percentageOverrideMinHeight->isPositive()) {
            style.setMinHeight(Style::MinimumSize(minimumControlSize.height()));
        }
    }

    // Font
    if (auto controlFont = this->controlFont(appearance, fontCascade.get(), style.usedZoom())) {
        // If overriding the specified font with the theme font, also override the line height with the standard line height.
        style.setLineHeight(RenderStyle::initialLineHeight());
        style.setFontDescription(WTFMove(controlFont.value()));
    }

    // Special style that tells enabled default buttons in active windows to use the ActiveButtonText color.
    // The active window part of the test has to be done at paint time since it's not triggered by a style change.
    style.setInsideDefaultButton(appearance == StyleAppearance::DefaultButton && element && !element->isDisabledFormControl());
}

void RenderTheme::adjustCheckboxStyle(RenderStyle& style, const Element* element) const
{
    adjustButtonOrCheckboxOrColorWellOrInnerSpinButtonOrRadioStyle(style, element);
}

void RenderTheme::adjustRadioStyle(RenderStyle& style, const Element* element) const
{
    adjustButtonOrCheckboxOrColorWellOrInnerSpinButtonOrRadioStyle(style, element);
}

void RenderTheme::adjustColorWellStyle(RenderStyle& style, const Element* element) const
{
    adjustButtonOrCheckboxOrColorWellOrInnerSpinButtonOrRadioStyle(style, element);
}

void RenderTheme::adjustButtonStyle(RenderStyle& style, const Element* element) const
{
    adjustButtonOrCheckboxOrColorWellOrInnerSpinButtonOrRadioStyle(style, element);
}

void RenderTheme::adjustInnerSpinButtonStyle(RenderStyle& style, const Element* element) const
{
    adjustButtonOrCheckboxOrColorWellOrInnerSpinButtonOrRadioStyle(style, element);
}

void RenderTheme::adjustMenuListStyle(RenderStyle& style, const Element*) const
{
    style.setOverflowX(Overflow::Visible);
    style.setOverflowY(Overflow::Visible);
}

void RenderTheme::adjustMeterStyle(RenderStyle& style, const Element*) const
{
    style.setBoxShadow(CSS::Keyword::None { });
}

FloatSize RenderTheme::meterSizeForBounds(const RenderMeter&, const FloatRect& bounds) const
{
    return bounds.size();
}

#if ENABLE(ATTACHMENT_ELEMENT)

String RenderTheme::attachmentStyleSheet() const
{
    ASSERT(DeprecatedGlobalSettings::attachmentElementEnabled());
    return "attachment { appearance: auto; }"_s;
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

void RenderTheme::paintSliderTicks(const RenderElement& renderer, const PaintInfo& paintInfo, const FloatRect& rect)
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(renderer.element());
    if (!input || !input->isRangeControl())
        return;

    auto dataList = input->dataList();
    if (!dataList)
        return;

    double min = input->minimum();
    double max = input->maximum();
    CheckedRef style = renderer.style();
    auto appearance = style->usedAppearance();
    // We don't support ticks on alternate sliders like MediaVolumeSliders.
    if (appearance != StyleAppearance::SliderHorizontal && appearance != StyleAppearance::SliderVertical)
        return;
    bool isHorizontal = appearance == StyleAppearance::SliderHorizontal;

    IntSize thumbSize;
    if (CheckedPtr thumbRenderer = input->sliderThumbElement()->renderer()) {
        auto& thumbStyle = thumbRenderer->style();

        int thumbWidth = 0;
        auto usedZoom = thumbStyle.usedZoomForLength();
        if (auto fixedWidth = thumbStyle.width().tryFixed())
            thumbWidth = static_cast<int>(fixedWidth->resolveZoom(usedZoom));

        int thumbHeight = 0;
        if (auto fixedHeight = thumbStyle.height().tryFixed())
            thumbHeight = static_cast<int>(fixedHeight->resolveZoom(usedZoom));

        thumbSize.setWidth(isHorizontal ? thumbWidth : thumbHeight);
        thumbSize.setHeight(isHorizontal ? thumbHeight : thumbWidth);
    }

    IntSize tickSize = sliderTickSize();
    float zoomFactor = style->usedZoom();
    FloatRect tickRect;
    int tickRegionSideMargin = 0;
    int tickRegionWidth = 0;
    IntRect trackBounds;
    // We can ignoring transforms because transform is handled by the graphics context.
    if (CheckedPtr trackRenderer = input->sliderTrackElement()->renderer())
        trackBounds = trackRenderer->absoluteBoundingBoxRectIgnoringTransforms();
    IntRect sliderBounds = renderer.absoluteBoundingBoxRectIgnoringTransforms();

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
    paintInfo.context().setFillColor(style->visitedDependentColorWithColorFilter(CSSPropertyColor));
    bool isInlineFlipped = (!isHorizontal && renderer.writingMode().isHorizontal()) || renderer.writingMode().isInlineFlipped();
    for (Ref optionElement : dataList->suggestions()) {
        if (auto optionValue = input->listOptionValueAsDouble(optionElement.get())) {
            double tickFraction = (*optionValue - min) / (max - min);
            double tickRatio = isInlineFlipped ? 1.0 - tickFraction : tickFraction;
            double tickPosition = round(tickRegionSideMargin + tickRegionWidth * tickRatio);
            if (isHorizontal)
                tickRect.setX(tickPosition);
            else
                tickRect.setY(tickPosition);
            paintInfo.context().fillRect(tickRect);
        }
    }
}

void RenderTheme::paintPlatformResizer(const RenderLayerModelObject& renderer, GraphicsContext& context, const LayoutRect& resizerCornerRect)
{
    RefPtr<Image> resizeCornerImage;
    FloatSize cornerResizerSize;
    Ref document = renderer.document();
    if (document->deviceScaleFactor() >= 2) {
        static NeverDestroyed<Image*> resizeCornerImageHiRes(&ImageAdapter::loadPlatformResource("textAreaResizeCorner@2x").leakRef());
        resizeCornerImage = resizeCornerImageHiRes;
        cornerResizerSize = resizeCornerImage->size();
        cornerResizerSize.scale(0.5f);
    } else {
        static NeverDestroyed<Image*> resizeCornerImageLoRes(&ImageAdapter::loadPlatformResource("textAreaResizeCorner").leakRef());
        resizeCornerImage = resizeCornerImageLoRes;
        cornerResizerSize = resizeCornerImage->size();
    }

    if (renderer.shouldPlaceVerticalScrollbarOnLeft()) {
        GraphicsContextStateSaver stateSaver(context);
        context.translate(resizerCornerRect.x() + cornerResizerSize.width(), resizerCornerRect.y() + resizerCornerRect.height() - cornerResizerSize.height());
        context.scale(FloatSize(-1.0, 1.0));
        if (resizeCornerImage)
            context.drawImage(*resizeCornerImage, FloatRect(FloatPoint(), cornerResizerSize));
        return;
    }

    if (!resizeCornerImage)
        return;
    FloatRect imageRect = snapRectToDevicePixels(LayoutRect(resizerCornerRect.maxXMaxYCorner() - cornerResizerSize, cornerResizerSize), document->deviceScaleFactor());
    context.drawImage(*resizeCornerImage, imageRect);
}

void RenderTheme::paintPlatformResizerFrame(const RenderLayerModelObject&, GraphicsContext& context, const LayoutRect& resizerAbsRect)
{
    // Clipping will exclude the right and bottom edges of this frame.
    GraphicsContextStateSaver stateSaver(context);
    context.clip(resizerAbsRect);
    LayoutRect largerCorner = resizerAbsRect;
    largerCorner.setSize(LayoutSize(largerCorner.width() + 1_lu, largerCorner.height() + 1_lu));
    context.setStrokeColor(SRGBA<uint8_t> { 217, 217, 217 });
    context.setStrokeThickness(1.0f);
    context.setFillColor(Color::transparentBlack);
    context.drawRect(snappedIntRect(largerCorner));
}

bool RenderTheme::shouldHaveSpinButton(const HTMLInputElement& inputElement) const
{
    return inputElement.isSteppable() && !inputElement.isRangeControl();
}

void RenderTheme::setColorWellSwatchBackground(HTMLElement& swatch, Color color)
{
    if (!color.isOpaque())
        color = blendSourceOver(Color::white, color);
    swatch.setInlineStyleProperty(CSSPropertyBackgroundColor, serializationForHTML(color));
}

void RenderTheme::adjustSliderThumbStyle(RenderStyle& style, const Element* element) const
{
    adjustSliderThumbSize(style, element);
}

void RenderTheme::adjustSwitchStyleDisplay(RenderStyle& style) const
{
    // RenderTheme::adjustStyle() normalizes a bunch of display types to InlineBlock and Block.
    switch (style.display()) {
    case DisplayType::InlineBlock:
        style.setEffectiveDisplay(DisplayType::InlineGrid);
        break;
    case DisplayType::Block:
        style.setEffectiveDisplay(DisplayType::Grid);
        break;
    default:
        break;
    }
}

void RenderTheme::adjustSwitchStyle(RenderStyle& style, const Element*) const
{
    // FIXME: This probably has the same flaw as
    // RenderTheme::adjustButtonOrCheckboxOrColorWellOrInnerSpinButtonOrRadioStyle() by not taking
    // min-width/min-height into account.
    auto controlSize = this->controlSize(StyleAppearance::Switch, style.checkedFontCascade().get(), { style.logicalWidth(), style.logicalHeight() }, usedZoomForComputedStyle(style));
    style.setLogicalWidth(Style::PreferredSize { controlSize.width() });
    style.setLogicalHeight(Style::PreferredSize { controlSize.height() });

    adjustSwitchStyleDisplay(style);
}

void RenderTheme::adjustSwitchThumbOrSwitchTrackStyle(RenderStyle& style) const
{
    style.setGridItemRowStart(Style::GridPosition::Explicit { { 1 } });
    style.setGridItemColumnStart(Style::GridPosition::Explicit { { 1 } });
}

Style::PaddingBox RenderTheme::popupInternalPaddingBox(const RenderStyle&) const
{
    return Style::PaddingBox { 0_css_px };
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

static Color defaultLinkColor(bool useDarkAppearance)
{
    return useDarkAppearance ? SRGBA<uint8_t> { 158, 158, 255 } : SRGBA<uint8_t> { 0, 0, 238 };
}

static Color defaultVisitedLinkColor(bool useDarkAppearance)
{
    return useDarkAppearance ? SRGBA<uint8_t> { 208, 173, 240 } : SRGBA<uint8_t> { 85, 26, 139 };
}

Color RenderTheme::systemColor(CSSValueID cssValueId, OptionSet<StyleColorOptions> options) const
{
    auto useDarkAppearance = options.contains(StyleColorOptions::UseDarkAppearance);
    auto forVisitedLink = options.contains(StyleColorOptions::ForVisitedLink);

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
        return defaultLinkColor(useDarkAppearance);

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-visitedtext
    // Text in visited links. For light backgrounds, traditionally purple.
    case CSSValueVisitedtext:
        return defaultVisitedLinkColor(useDarkAppearance);

    // https://drafts.csswg.org/css-color-4/#valdef-system-color-activetext
    // Text in active links. For light backgrounds, traditionally red.
    case CSSValueActivetext:
    case CSSValueWebkitActivelink: // Non-standard addition.
        return useDarkAppearance ? SRGBA<uint8_t> { 255, 158, 158 } : Color::red;

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
    case CSSValueWebkitLink: {
        if (forVisitedLink)
            return defaultVisitedLinkColor(useDarkAppearance);
        return defaultLinkColor(useDarkAppearance);
    }

    // Deprecated system-colors:
    // https://drafts.csswg.org/css-color-4/#deprecated-system-colors

    // https://drafts.csswg.org/css-color-4/#activeborder
    // DEPRECATED: Active window border.
    case CSSValueActiveborder:
        return systemColor(CSSValueButtonborder, options);

    // https://drafts.csswg.org/css-color-4/#activecaption
    // DEPRECATED: Active window caption.
    case CSSValueActivecaption:
        return systemColor(CSSValueCanvastext, options);

    // https://drafts.csswg.org/css-color-4/#appworkspace
    // DEPRECATED: Background color of multiple document interface.
    case CSSValueAppworkspace:
        return systemColor(CSSValueCanvas, options);

    // https://drafts.csswg.org/css-color-4/#background
    // DEPRECATED: Desktop background.
    case CSSValueBackground:
        return systemColor(CSSValueCanvas, options);

    // https://drafts.csswg.org/css-color-4/#buttonhighlight
    // DEPRECATED: The color of the border facing the light source for 3-D elements that
    // appear 3-D due to one layer of surrounding border.
    case CSSValueButtonhighlight:
        return systemColor(CSSValueButtonface, options);

    // https://drafts.csswg.org/css-color-4/#buttonshadow
    // DEPRECATED: The color of the border away from the light source for 3-D elements that
    // appear 3-D due to one layer of surrounding border.
    case CSSValueButtonshadow:
        return systemColor(CSSValueButtonface, options);

    // https://drafts.csswg.org/css-color-4/#captiontext
    // DEPRECATED: Text in caption, size box, and scrollbar arrow box.
    case CSSValueCaptiontext:
        return systemColor(CSSValueCanvastext, options);

    // https://drafts.csswg.org/css-color-4/#inactiveborder
    // DEPRECATED: Inactive window border.
    case CSSValueInactiveborder:
        return systemColor(CSSValueButtonborder, options);

    // https://drafts.csswg.org/css-color-4/#inactivecaption
    // DEPRECATED: Inactive window caption.
    case CSSValueInactivecaption:
        return systemColor(CSSValueCanvas, options);

    // https://drafts.csswg.org/css-color-4/#inactivecaptiontext
    // DEPRECATED: Color of text in an inactive caption.
    case CSSValueInactivecaptiontext:
        return systemColor(CSSValueGraytext, options);

    // https://drafts.csswg.org/css-color-4/#infobackground
    // DEPRECATED: Background color for tooltip controls.
    case CSSValueInfobackground:
        return systemColor(CSSValueCanvas, options);

    // https://drafts.csswg.org/css-color-4/#infotext
    // DEPRECATED: Text color for tooltip controls.
    case CSSValueInfotext:
        return systemColor(CSSValueCanvastext, options);

    // https://drafts.csswg.org/css-color-4/#menu
    // DEPRECATED: Menu background.
    case CSSValueMenu:
        return systemColor(CSSValueCanvas, options);

    // https://drafts.csswg.org/css-color-4/#menutext
    // DEPRECATED: Text in menus.
    case CSSValueMenutext:
        return systemColor(CSSValueCanvastext, options);

    // https://drafts.csswg.org/css-color-4/#scrollbar
    // DEPRECATED: Scroll bar gray area.
    case CSSValueScrollbar:
        return systemColor(CSSValueCanvas, options);

    // https://drafts.csswg.org/css-color-4/#threeddarkshadow
    // DEPRECATED: The color of the darker (generally outer) of the two borders away from
    // thelight source for 3-D elements that appear 3-D due to two concentric layers of
    // surrounding border.
    case CSSValueThreeddarkshadow:
        return systemColor(CSSValueButtonborder, options);

    // https://drafts.csswg.org/css-color-4/#threedface
    // DEPRECATED: The face background color for 3-D elements that appear 3-D due to two
    // concentric layers of surrounding border
    case CSSValueThreedface:
        return systemColor(CSSValueButtonface, options);

    // https://drafts.csswg.org/css-color-4/#threedhighlight
    // DEPRECATED: The color of the lighter (generally outer) of the two borders facing
    // the light source for 3-D elements that appear 3-D due to two concentric layers of
    // surrounding border.
    case CSSValueThreedhighlight:
        return systemColor(CSSValueButtonborder, options);

    // https://drafts.csswg.org/css-color-4/#threedlightshadow
    // DEPRECATED: The color of the darker (generally inner) of the two borders facing
    // the light source for 3-D elements that appear 3-D due to two concentric layers of
    // surrounding border
    case CSSValueThreedlightshadow:
        return systemColor(CSSValueButtonborder, options);

    // https://drafts.csswg.org/css-color-4/#threedshadow
    // DEPRECATED: The color of the lighter (generally inner) of the two borders away
    // from the light source for 3-D elements that appear 3-D due to two concentric layers
    // of surrounding border.
    case CSSValueThreedshadow:
        return systemColor(CSSValueButtonborder, options);

    // https://drafts.csswg.org/css-color-4/#window
    // DEPRECATED: Window background.
    case CSSValueWindow:
        return systemColor(CSSValueCanvas, options);

    // https://drafts.csswg.org/css-color-4/#windowframe
    // DEPRECATED: Window frame.
    case CSSValueWindowframe:
        return systemColor(CSSValueButtonborder, options);

    // https://drafts.csswg.org/css-color-4/#windowtext
    // DEPRECATED: Text in windows.
    case CSSValueWindowtext:
        return systemColor(CSSValueCanvastext, options);

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

Color RenderTheme::annotationHighlightBackgroundColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.annotationHighlightBackgroundColor.isValid())
        cache.annotationHighlightBackgroundColor = transformSelectionBackgroundColor(platformAnnotationHighlightBackgroundColor(options), options);
    return cache.annotationHighlightBackgroundColor;
}

Color RenderTheme::platformAnnotationHighlightBackgroundColor(OptionSet<StyleColorOptions>) const
{
    return Color::yellow;
}

Color RenderTheme::annotationHighlightForegroundColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.annotationHighlightForegroundColor.isValid())
        cache.annotationHighlightForegroundColor = resolve(CSS::ContrastColorResolver { annotationHighlightBackgroundColor(options) });
    return cache.annotationHighlightForegroundColor;
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


Color RenderTheme::spellingMarkerColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.spellingMarkerColor.isValid())
        cache.spellingMarkerColor = platformSpellingMarkerColor(options);
    return cache.spellingMarkerColor;
}

Color RenderTheme::platformSpellingMarkerColor(OptionSet<StyleColorOptions>) const
{
    return Color::red;
}

Color RenderTheme::dictationAlternativesMarkerColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.dictationAlternativesMarkerColor.isValid())
        cache.dictationAlternativesMarkerColor = platformDictationAlternativesMarkerColor(options);
    return cache.dictationAlternativesMarkerColor;
}

Color RenderTheme::platformDictationAlternativesMarkerColor(OptionSet<StyleColorOptions>) const
{
    return Color::green;
}

Color RenderTheme::autocorrectionReplacementMarkerColor(const RenderText& renderer) const
{
    auto options = renderer.styleColorOptions();
    auto& cache = colorCache(options);
    if (!cache.autocorrectionReplacementMarkerColor.isValid())
        cache.autocorrectionReplacementMarkerColor = platformAutocorrectionReplacementMarkerColor(options);
    return cache.autocorrectionReplacementMarkerColor;
}

Color RenderTheme::platformAutocorrectionReplacementMarkerColor(OptionSet<StyleColorOptions>) const
{
    return Color::green;
}

Color RenderTheme::grammarMarkerColor(OptionSet<StyleColorOptions> options) const
{
    auto& cache = colorCache(options);
    if (!cache.grammarMarkerColor.isValid())
        cache.grammarMarkerColor = platformGrammarMarkerColor(options);
    return cache.grammarMarkerColor;
}

Color RenderTheme::platformGrammarMarkerColor(OptionSet<StyleColorOptions>) const
{
    return Color::green;
}

Color RenderTheme::documentMarkerLineColor(const RenderText& renderer, DocumentMarkerLineStyleMode mode) const
{
    auto options = renderer.styleColorOptions();

    switch (mode) {
    case DocumentMarkerLineStyleMode::Spelling:
        return spellingMarkerColor(options);
    case DocumentMarkerLineStyleMode::DictationAlternatives:
    case DocumentMarkerLineStyleMode::TextCheckingDictationPhraseWithAlternatives:
        return dictationAlternativesMarkerColor(options);
    case DocumentMarkerLineStyleMode::AutocorrectionReplacement:
        return autocorrectionReplacementMarkerColor(renderer);
    case DocumentMarkerLineStyleMode::Grammar:
        return grammarMarkerColor(options);
    }

    ASSERT_NOT_REACHED();
    return Color::transparentBlack;
}

Color RenderTheme::focusRingColor(OptionSet<StyleColorOptions> options) const
{
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
