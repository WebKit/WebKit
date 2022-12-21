/*
 * Copyright (C) 2022 Apple Inc. All Rights Reserved.
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

#import "MeterMac.h"
#import "TextAreaMac.h"
#import "TextFieldMac.h"
#import "ToggleButtonMac.h"
#import "ToggleButtonPart.h"
#import <pal/spi/mac/NSViewSPI.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

std::unique_ptr<ControlFactory> ControlFactory::createControlFactory()
{
    return makeUnique<ControlFactoryMac>();
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

static RetainPtr<NSButtonCell> createToggleButtonCell()
{
    auto buttonCell = adoptNS([[NSButtonCell alloc] init]);
    [buttonCell setTitle:nil];
    [buttonCell setFocusRingType:NSFocusRingTypeExterior];
    return buttonCell;
}

NSButtonCell* ControlFactoryMac::checkboxCell() const
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

NSButtonCell* ControlFactoryMac::radioCell() const
{
    if (!m_radioCell) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_radioCell = createToggleButtonCell();
        [m_radioCell setButtonType:NSButtonTypeRadio];
        END_BLOCK_OBJC_EXCEPTIONS
    }
    return m_radioCell.get();
}

NSLevelIndicatorCell* ControlFactoryMac::levelIndicatorCell() const
{
    if (!m_levelIndicatorCell) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_levelIndicatorCell = adoptNS([[NSLevelIndicatorCell alloc] initWithLevelIndicatorStyle:NSLevelIndicatorStyleContinuousCapacity]);
        [m_levelIndicatorCell setLevelIndicatorStyle:NSLevelIndicatorStyleContinuousCapacity];
        END_BLOCK_OBJC_EXCEPTIONS
    }
    return m_levelIndicatorCell.get();
}

NSTextFieldCell* ControlFactoryMac::textFieldCell() const
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

std::unique_ptr<PlatformControl> ControlFactoryMac::createPlatformMeter(MeterPart& part)
{
    return makeUnique<MeterMac>(part, *this, levelIndicatorCell());
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
    return makeUnique<ToggleButtonMac>(part, *this, part.type() == ControlPartType::Checkbox ? checkboxCell() : radioCell());
}

} // namespace WebCore

#endif // PLATFORM(MAC)
