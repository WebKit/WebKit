/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#import "MenuListMac.h"

#if PLATFORM(MAC)

#import "ControlFactoryMac.h"
#import "GraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "MenuListPart.h"
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MenuListMac);

MenuListMac::MenuListMac(MenuListPart& owningPart, ControlFactoryMac& controlFactory, NSPopUpButtonCell *popUpButtonCell)
    : ControlMac(owningPart, controlFactory)
    , m_popUpButtonCell(popUpButtonCell)
{
    ASSERT(m_popUpButtonCell);
}

MenuListMac::~MenuListMac() = default;

IntSize MenuListMac::cellSize(NSControlSize controlSize, const ControlStyle&) const
{
    static constexpr std::array sizes {
        IntSize { 0, 21 },
        IntSize { 0, 18 },
        IntSize { 0, 15 },
        IntSize { 0, 24 }
    };
    return sizes[controlSize];
}

IntOutsets MenuListMac::cellOutsets(NSControlSize controlSize, const ControlStyle&) const
{
    static const std::array outsets {
        // top right bottom left
        IntOutsets { 0, 3, 1, 3 },
        IntOutsets { 0, 3, 2, 3 },
        IntOutsets { 0, 1, 0, 1 },
        IntOutsets { 0, 6, 2, 6 },
    };
    return outsets[controlSize];
}

void MenuListMac::updateCellStates(const FloatRect& rect, const ControlStyle& style)
{
    ControlMac::updateCellStates(rect, style);

    auto direction = style.states.contains(ControlStyle::State::InlineFlippedWritingMode) ? NSUserInterfaceLayoutDirectionRightToLeft : NSUserInterfaceLayoutDirectionLeftToRight;
    [m_popUpButtonCell setUserInterfaceLayoutDirection:direction];

    // Update the various states we respond to.
    updateCheckedState(m_popUpButtonCell.get(), style);
    updateEnabledState(m_popUpButtonCell.get(), style);
    updatePressedState(m_popUpButtonCell.get(), style);

    // Only update if we have to, since AppKit does work even if the size is the same.
    auto controlSize = controlSizeForSize(rect.size(), style);
    if (controlSize != [m_popUpButtonCell controlSize])
        [m_popUpButtonCell setControlSize:controlSize];
}

FloatRect MenuListMac::rectForBounds(const FloatRect& bounds, const ControlStyle& style) const
{
    int minimumMenuListSize = sizeForSystemFont(style).width();
    if (bounds.width() < minimumMenuListSize)
        return bounds;

    auto controlSize = [m_popUpButtonCell controlSize];
    auto size = cellSize(controlSize, style);
    auto outsets = cellOutsets(controlSize, style);

    size.scale(style.zoomFactor);
    size.setWidth(bounds.width());

    // Make enough room for the shadow.
    return inflatedRect(bounds, size, outsets, style);
}

void MenuListMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    GraphicsContextStateSaver stateSaver(context);

    auto inflatedRect = rectForBounds(borderRect.rect(), style);

    if (style.zoomFactor != 1) {
        inflatedRect.scale(1 / style.zoomFactor);
        context.scale(style.zoomFactor);
    }

    drawCell(context, inflatedRect, deviceScaleFactor, style, m_popUpButtonCell.get(), true);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
