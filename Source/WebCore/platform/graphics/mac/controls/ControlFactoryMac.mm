/*
 * Copyright (C) 2022-2023 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ControlFactoryMac.h"

#if PLATFORM(MAC)

#import "ButtonMac.h"
#import "ColorWellMac.h"
#import "ImageControlsButtonMac.h"
#import "InnerSpinButtonMac.h"
#import "MenuListButtonMac.h"
#import "MenuListMac.h"
#import "MeterMac.h"
#import "ProgressBarMac.h"
#import "SearchFieldCancelButtonMac.h"
#import "SearchFieldMac.h"
#import "SearchFieldResultsMac.h"
#import "SearchFieldResultsPart.h"
#import "SliderThumbMac.h"
#import "SliderTrackMac.h"
#import "TextAreaMac.h"
#import "TextFieldMac.h"
#import "ToggleButtonMac.h"
#import "ToggleButtonPart.h"
#import <pal/spi/mac/NSSearchFieldCellSPI.h>
#import <pal/spi/mac/NSServicesRolloverButtonCellSPI.h>
#import <pal/spi/mac/NSViewSPI.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

std::unique_ptr<ControlFactory> ControlFactory::createControlFactory()
{
    return makeUnique<ControlFactoryMac>();
}

ControlFactoryMac& ControlFactoryMac::sharedControlFactory()
{
    return static_cast<ControlFactoryMac&>(ControlFactory::sharedControlFactory());
}

NSView *ControlFactoryMac::drawingView(const FloatRect& rect, const ControlStyle& style) const
{
    if (!m_drawingView)
        m_drawingView = adoptNS([[WebControlView alloc] init]);

    // Use a fake view.
    [m_drawingView setFrameSize:NSSizeFromCGSize(rect.size())];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [m_drawingView setAppearance:[NSAppearance currentAppearance]];
    ALLOW_DEPRECATED_DECLARATIONS_END
#if USE(NSVIEW_SEMANTICCONTEXT)
    if (style.states.contains(ControlStyle::State::FormSemanticContext))
        [m_drawingView _setSemanticContext:NSViewSemanticContextForm];
#else
    UNUSED_PARAM(style);
#endif
    return m_drawingView.get();
}

static RetainPtr<NSButtonCell> createButtonCell()
{
    auto buttonCell = adoptNS([[NSButtonCell alloc] init]);
    [buttonCell setTitle:nil];
    [buttonCell setButtonType:NSButtonTypeMomentaryPushIn];
    return buttonCell;
}

NSButtonCell* ControlFactoryMac::buttonCell() const
{
    if (!m_buttonCell) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_buttonCell = createButtonCell();
        END_BLOCK_OBJC_EXCEPTIONS
    }
    return m_buttonCell.get();
}

NSButtonCell* ControlFactoryMac::defaultButtonCell() const
{
    if (!m_defaultButtonCell) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_defaultButtonCell = createButtonCell();
        [m_defaultButtonCell setKeyEquivalent:@"\r"];
        END_BLOCK_OBJC_EXCEPTIONS
    }
    return m_defaultButtonCell.get();
}

static RetainPtr<NSButtonCell> createToggleButtonCell()
{
    auto buttonCell = adoptNS([[NSButtonCell alloc] init]);
    [buttonCell setTitle:nil];
    [buttonCell setFocusRingType:NSFocusRingTypeExterior];
    return buttonCell;
}

NSButtonCell *ControlFactoryMac::checkboxCell() const
{
    if (!m_checkboxCell) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_checkboxCell = createToggleButtonCell();
        [m_checkboxCell setButtonType:NSButtonTypeSwitch];
        [m_checkboxCell setAllowsMixedState:YES];
        END_BLOCK_OBJC_EXCEPTIONS
    }
    return m_checkboxCell.get();
}

NSButtonCell *ControlFactoryMac::radioCell() const
{
    if (!m_radioCell) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_radioCell = createToggleButtonCell();
        [m_radioCell setButtonType:NSButtonTypeRadio];
        END_BLOCK_OBJC_EXCEPTIONS
    }
    return m_radioCell.get();
}

NSLevelIndicatorCell *ControlFactoryMac::levelIndicatorCell() const
{
    if (!m_levelIndicatorCell) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_levelIndicatorCell = adoptNS([[NSLevelIndicatorCell alloc] initWithLevelIndicatorStyle:NSLevelIndicatorStyleContinuousCapacity]);
        [m_levelIndicatorCell setLevelIndicatorStyle:NSLevelIndicatorStyleContinuousCapacity];
        END_BLOCK_OBJC_EXCEPTIONS
    }
    return m_levelIndicatorCell.get();
}

NSPopUpButtonCell *ControlFactoryMac::popUpButtonCell() const
{
    if (!m_popUpButtonCell) {
        m_popUpButtonCell = adoptNS([[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO]);
        [m_popUpButtonCell setUsesItemFromMenu:NO];
        [m_popUpButtonCell setFocusRingType:NSFocusRingTypeExterior];
        [m_popUpButtonCell setUserInterfaceLayoutDirection:NSUserInterfaceLayoutDirectionLeftToRight];
    }
    return m_popUpButtonCell.get();
}

#if ENABLE(SERVICE_CONTROLS)
NSServicesRolloverButtonCell *ControlFactoryMac::servicesRolloverButtonCell() const
{
    if (!m_servicesRolloverButtonCell) {
        m_servicesRolloverButtonCell = [NSServicesRolloverButtonCell serviceRolloverButtonCellForStyle:NSSharingServicePickerStyleRollover];
        [m_servicesRolloverButtonCell setBezelStyle:NSBezelStyleRoundedDisclosure];
        [m_servicesRolloverButtonCell setButtonType:NSButtonTypePushOnPushOff];
        [m_servicesRolloverButtonCell setImagePosition:NSImageOnly];
        [m_servicesRolloverButtonCell setState:NO];
    }
    return m_servicesRolloverButtonCell.get();
}
#endif

NSSearchFieldCell *ControlFactoryMac::searchFieldCell() const
{
    if (!m_searchFieldCell) {
        m_searchFieldCell = adoptNS([[NSSearchFieldCell alloc] initTextCell:@""]);
        [m_searchFieldCell setBezelStyle:NSTextFieldRoundedBezel];
        [m_searchFieldCell setBezeled:YES];
        [m_searchFieldCell setEditable:YES];
        [m_searchFieldCell setFocusRingType:NSFocusRingTypeExterior];
        [m_searchFieldCell setCenteredLook:NO];
    }
    return m_searchFieldCell.get();
}

NSMenu *ControlFactoryMac::searchMenuTemplate() const
{
    if (!m_searchMenuTemplate)
        m_searchMenuTemplate = adoptNS([[NSMenu alloc] initWithTitle:@""]);
    return m_searchMenuTemplate.get();
}

NSSliderCell *ControlFactoryMac::sliderCell() const
{
    if (!m_sliderCell) {
        m_sliderCell = adoptNS([[NSSliderCell alloc] init]);
        [m_sliderCell setSliderType:NSSliderTypeLinear];
        [m_sliderCell setControlSize:NSControlSizeSmall];
        [m_sliderCell setFocusRingType:NSFocusRingTypeExterior];
    }
    return m_sliderCell.get();
}

NSTextFieldCell *ControlFactoryMac::textFieldCell() const
{
    if (!m_textFieldCell) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_textFieldCell = adoptNS([[WebControlTextFieldCell alloc] initTextCell:@""]);
        [m_textFieldCell setBezeled:YES];
        [m_textFieldCell setEditable:YES];
        [m_textFieldCell setFocusRingType:NSFocusRingTypeExterior];
        // Post-Lion, WebCore can be in charge of paintinng the background thanks to
        // the workaround in place for <rdar://problem/11385461>, which is implemented
        // above as _coreUIDrawOptionsWithFrame.
        [m_textFieldCell setDrawsBackground:NO];
        END_BLOCK_OBJC_EXCEPTIONS
    }
    return m_textFieldCell.get();
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformButton(ButtonPart& part)
{
    return makeUnique<ButtonMac>(part, *this, part.type() == StyleAppearance::DefaultButton ? defaultButtonCell() : buttonCell());
}

#if ENABLE(INPUT_TYPE_COLOR)
std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformColorWell(ColorWellPart& part)
{
    return makeUnique<ColorWellMac>(part, *this, buttonCell());
}
#endif

#if ENABLE(SERVICE_CONTROLS)
std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformImageControlsButton(ImageControlsButtonPart& part)
{
    return makeUnique<ImageControlsButtonMac>(part, *this, servicesRolloverButtonCell());
}
#endif

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformInnerSpinButton(InnerSpinButtonPart& part)
{
    return makeUnique<InnerSpinButtonMac>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformMenuList(MenuListPart& part)
{
    return makeUnique<MenuListMac>(part, *this, popUpButtonCell());
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformMenuListButton(MenuListButtonPart& part)
{
    return makeUnique<MenuListButtonMac>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformMeter(MeterPart& part)
{
    return makeUnique<MeterMac>(part, *this, levelIndicatorCell());
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformProgressBar(ProgressBarPart& part)
{
    return makeUnique<ProgressBarMac>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformSearchField(SearchFieldPart& part)
{
    return makeUnique<SearchFieldMac>(part, *this, searchFieldCell());
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformSearchFieldCancelButton(SearchFieldCancelButtonPart& part)
{
    return makeUnique<SearchFieldCancelButtonMac>(part, *this, searchFieldCell());
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformSearchFieldResults(SearchFieldResultsPart& part)
{
    return makeUnique<SearchFieldResultsMac>(part, *this, searchFieldCell(), part.type() == StyleAppearance::SearchFieldResultsButton ? searchMenuTemplate() : nullptr);
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformSliderThumb(SliderThumbPart& part)
{
    return makeUnique<SliderThumbMac>(part, *this, sliderCell());
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformSliderTrack(SliderTrackPart& part)
{
    return makeUnique<SliderTrackMac>(part, *this);
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformTextArea(TextAreaPart& part)
{
    return makeUnique<TextAreaMac>(part);
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformTextField(TextFieldPart& part)
{
    return makeUnique<TextFieldMac>(part, *this, textFieldCell());
}

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformToggleButton(ToggleButtonPart& part)
{
    return makeUnique<ToggleButtonMac>(part, *this, part.type() == StyleAppearance::Checkbox ? checkboxCell() : radioCell());
}

} // namespace WebCore

#endif // PLATFORM(MAC)
