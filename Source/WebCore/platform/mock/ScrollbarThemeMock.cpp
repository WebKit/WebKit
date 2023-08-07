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
#include "ScrollableArea.h"
#include "Scrollbar.h"

namespace WebCore {

IntRect ScrollbarThemeMock::trackRect(Scrollbar& scrollbar, bool)
{
    return scrollbar.frameRect();
}

int ScrollbarThemeMock::scrollbarThickness(ScrollbarWidth scrollbarWidth, ScrollbarExpansionState)
{
    switch (scrollbarWidth) {
    case ScrollbarWidth::Auto:
        return 15;
    case ScrollbarWidth::Thin:
        return 11;
    case ScrollbarWidth::None:
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 15;
}

void ScrollbarThemeMock::paintTrackBackground(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& trackRect)
{
    Color trackColor = scrollbar.enabled() ? Color::lightGray : SRGBA<uint8_t> { 224, 224, 224 };

    auto scrollbarTrackColorStyle = scrollbar.scrollableArea().scrollbarTrackColorStyle();

    if (scrollbarTrackColorStyle.isValid())
        trackColor = scrollbarTrackColorStyle;

    context.fillRect(trackRect, trackColor);
}

void ScrollbarThemeMock::paintThumb(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& thumbRect)
{
    if (scrollbar.enabled()) {
        Color thumbColor = Color::darkGray;

        auto scrollbarThumbColorStyle = scrollbar.scrollableArea().scrollbarThumbColorStyle();

        if (scrollbarThumbColorStyle.isValid())
            thumbColor = scrollbarThumbColorStyle;

        context.fillRect(thumbRect, thumbColor);
    }
}

bool ScrollbarThemeMock::usesOverlayScrollbars() const
{
    // FIXME: This is a layering violation, but ScrollbarThemeMock is also created depending on settings in platform layer,
    // we should fix it in both places.
    return DeprecatedGlobalSettings::usesOverlayScrollbars();
}

}

