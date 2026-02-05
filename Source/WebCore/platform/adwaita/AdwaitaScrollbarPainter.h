/*
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(THEME_ADWAITA)

#include "IntRect.h"
#include "ScrollTypes.h"

namespace WebCore {

class GraphicsContext;

namespace AdwaitaScrollbarPainter {

static const unsigned scrollbarSize = 21;
static const unsigned scrollbarBorderSize = 1;
static const unsigned thumbBorderSize = 1;
static const unsigned overlayThumbSize = 3;
static const unsigned minimumThumbSize = 40;
static const unsigned horizThumbMargin = 6;
static const unsigned horizOverlayThumbMargin = 3;
static const unsigned vertThumbMargin = 7;

static constexpr auto scrollbarBackgroundColorLight = Color::white;
static constexpr auto scrollbarBorderColorLight = Color::black.colorWithAlphaByte(38);
static constexpr auto overlayThumbBorderColorLight = Color::white.colorWithAlphaByte(102);
static constexpr auto overlayTroughColorLight = Color::black.colorWithAlphaByte(25);
static constexpr auto thumbHoveredColorLight = Color::black.colorWithAlphaByte(102);
static constexpr auto thumbPressedColorLight = Color::black.colorWithAlphaByte(153);
static constexpr auto thumbColorLight = Color::black.colorWithAlphaByte(51);

static constexpr auto scrollbarBackgroundColorDark = SRGBA<uint8_t> { 30, 30, 30 };
static constexpr auto scrollbarBorderColorDark = Color::white.colorWithAlphaByte(38);
static constexpr auto overlayThumbBorderColorDark = Color::black.colorWithAlphaByte(51);
static constexpr auto overlayTroughColorDark = Color::white.colorWithAlphaByte(25);
static constexpr auto thumbHoveredColorDark = Color::white.colorWithAlphaByte(102);
static constexpr auto thumbPressedColorDark = Color::white.colorWithAlphaByte(153);
static constexpr auto thumbColorDark = Color::white.colorWithAlphaByte(51);

struct State {
    bool enabled { false };
    bool useDarkAppearanceForScrollbars { false };
    bool shouldPlaceVerticalScrollbarOnLeft { false };
    bool usesOverlayScrollbars { false };
    ScrollbarOrientation orientation { ScrollbarOrientation::Horizontal };
    ScrollbarPart hoveredPart { NoPart };
    ScrollbarPart pressedPart { NoPart };
    int thumbPosition { 0 };
    int thumbLength { 0 };
    IntRect frameRect;
    double opacity { 1 };
};

void paint(GraphicsContext&, const IntRect&, const State&);

} // namespace AdwaitaScrollbarPainter
} // namespace WebCore

#endif // USE(THEME_ADWAITA)
