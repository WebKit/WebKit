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
#import "ButtonControlMac.h"

#if PLATFORM(MAC)

#import <pal/spi/cocoa/NSButtonCellSPI.h>

namespace WebCore {

ButtonControlMac::ButtonControlMac(ControlPart& part, ControlFactoryMac& controlFactory, NSButtonCell *buttonCell)
    : ControlMac(part, controlFactory)
    , m_buttonCell(buttonCell)
{
    ASSERT(m_buttonCell);
}

void ButtonControlMac::updateCellStates(const FloatRect& rect, const ControlStyle& style)
{
    ControlMac::updateCellStates(rect, style);

    const auto& states = style.states;

    updatePressedState(m_buttonCell.get(), style);
    updateEnabledState(m_buttonCell.get(), style);
    updateCheckedState(m_buttonCell.get(), style);
    
    if (states.contains(ControlStyle::State::Presenting))
        [m_buttonCell _setHighlighted:YES animated:NO];

    // Window inactive state does not need to be checked explicitly, since we paint parented to
    // a view in a window whose key state can be detected.

    // Only update if we have to, since AppKit does work even if the size is the same.
    auto controlSize = controlSizeForSize(rect.size(), style);
    if (controlSize != [m_buttonCell controlSize])
        [m_buttonCell setControlSize:controlSize];
}

} // namespace WebCore

#endif // PLATFORM(MAC)
