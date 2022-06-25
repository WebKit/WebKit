/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollbarThemePlayStation.h"

#include "HostWindow.h"
#include "NotImplemented.h"
#include "ScrollView.h"
#include "Scrollbar.h"

namespace WebCore {

ScrollbarTheme& ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemePlayStation theme;
    return theme;
}

int ScrollbarThemePlayStation::scrollbarThickness(ScrollbarControlSize, ScrollbarExpansionState)
{
    return 7;
}

bool ScrollbarThemePlayStation::hasButtons(Scrollbar&)
{
    notImplemented();
    return true;
}

bool ScrollbarThemePlayStation::hasThumb(Scrollbar&)
{
    notImplemented();
    return true;
}

IntRect ScrollbarThemePlayStation::backButtonRect(Scrollbar&, ScrollbarPart, bool)
{
    notImplemented();
    return { };
}

IntRect ScrollbarThemePlayStation::forwardButtonRect(Scrollbar&, ScrollbarPart, bool)
{
    notImplemented();
    return { };
}

IntRect ScrollbarThemePlayStation::trackRect(Scrollbar& scrollbar, bool)
{
    return scrollbar.frameRect();
}

void ScrollbarThemePlayStation::paintTrackBackground(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& trackRect)
{
    context.fillRect(trackRect, scrollbar.enabled() ? Color::lightGray : SRGBA<uint8_t> { 224, 224, 224 });
}

void ScrollbarThemePlayStation::paintThumb(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& thumbRect)
{
    if (scrollbar.enabled())
        context.fillRect(thumbRect, Color::darkGray);
}

} // namespace WebCore
