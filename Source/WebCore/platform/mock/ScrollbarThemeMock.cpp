/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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
#include "ScrollbarThemeMock.h"

// FIXME: This is a layering violation.
#include "DeprecatedGlobalSettings.h"
#include "Scrollbar.h"

namespace WebCore {

static const int cScrollbarThickness[] = { 15, 11 };

IntRect ScrollbarThemeMock::trackRect(Scrollbar& scrollbar, bool)
{
    return scrollbar.frameRect();
}

int ScrollbarThemeMock::scrollbarThickness(ScrollbarControlSize controlSize, ScrollbarExpansionState)
{
    return cScrollbarThickness[static_cast<uint8_t>(controlSize)];
}

void ScrollbarThemeMock::paintTrackBackground(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& trackRect)
{
    context.fillRect(trackRect, scrollbar.enabled() ? Color::lightGray.color() : SRGBA<uint8_t> { 224, 224, 224 });
}

void ScrollbarThemeMock::paintThumb(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& thumbRect)
{
    if (scrollbar.enabled())
        context.fillRect(thumbRect, Color::darkGray);
}

bool ScrollbarThemeMock::usesOverlayScrollbars() const
{
    // FIXME: This is a layering violation, but ScrollbarThemeMock is also created depending on settings in platform layer,
    // we should fix it in both places.
    return DeprecatedGlobalSettings::usesOverlayScrollbars();
}

}

