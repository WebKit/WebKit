/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "ScrollbarThemeIOS.h"

#include "GraphicsContext.h"
#include "IntRect.h"
#include "PlatformMouseEvent.h"
#include "Scrollbar.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    static NeverDestroyed<ScrollbarThemeIOS> theme;
    return &theme.get();
}

void ScrollbarThemeIOS::registerScrollbar(ScrollbarThemeClient*)
{
}

void ScrollbarThemeIOS::unregisterScrollbar(ScrollbarThemeClient*)
{
}

ScrollbarThemeIOS::ScrollbarThemeIOS()
{
}

ScrollbarThemeIOS::~ScrollbarThemeIOS()
{
}

void ScrollbarThemeIOS::preferencesChanged()
{
}

int ScrollbarThemeIOS::scrollbarThickness(ScrollbarControlSize)
{
    return 0;
}

double ScrollbarThemeIOS::initialAutoscrollTimerDelay()
{
    return 0;
}

double ScrollbarThemeIOS::autoscrollTimerDelay()
{
    return 0;
}
    
ScrollbarButtonsPlacement ScrollbarThemeIOS::buttonsPlacement() const
{
    return ScrollbarButtonsNone;
}

bool ScrollbarThemeIOS::hasButtons(ScrollbarThemeClient*)
{
    return false;
}

bool ScrollbarThemeIOS::hasThumb(ScrollbarThemeClient*)
{
    return false;
}

IntRect ScrollbarThemeIOS::backButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool /*painting*/)
{
    return IntRect();
}

IntRect ScrollbarThemeIOS::forwardButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool /*painting*/)
{
    return IntRect();
}

IntRect ScrollbarThemeIOS::trackRect(ScrollbarThemeClient*, bool /*painting*/)
{
    return IntRect();
}

int ScrollbarThemeIOS::minimumThumbLength(ScrollbarThemeClient*)
{
    return 0;
}

bool ScrollbarThemeIOS::shouldCenterOnThumb(ScrollbarThemeClient*, const PlatformMouseEvent&)
{
    return false;
}

bool ScrollbarThemeIOS::paint(ScrollbarThemeClient*, GraphicsContext*, const IntRect& /*damageRect*/)
{
    return true;
}

} // namespace WebCore
