/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollbarThemeChromiumLinux.h"

#include "ChromiumBridge.h"
#include "PlatformMouseEvent.h"
#include "Scrollbar.h"

namespace WebCore {

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeChromiumLinux theme;
    return &theme;
}

int ScrollbarThemeChromiumLinux::scrollbarThickness(ScrollbarControlSize controlSize)
{
    // Horiz and Vert scrollbars are the same thickness.
    IntSize scrollbarSize = ChromiumBridge::getThemePartSize(ChromiumBridge::PartScrollbarVerticalTrack);
    return scrollbarSize.width();
}

void ScrollbarThemeChromiumLinux::paintTrackPiece(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect, ScrollbarPart partType)
{
    ChromiumBridge::ThemePaintState state = scrollbar->hoveredPart() == partType ? ChromiumBridge::StateHover : ChromiumBridge::StateNormal;
    IntRect alignRect = trackRect(scrollbar, false);
    ChromiumBridge::ThemePaintExtraParams extraParams;
    extraParams.scrollbarTrack.trackX = alignRect.x();
    extraParams.scrollbarTrack.trackY = alignRect.y();
    extraParams.scrollbarTrack.trackWidth = alignRect.width();
    extraParams.scrollbarTrack.trackHeight = alignRect.height();
    ChromiumBridge::paintThemePart(
        gc,
        scrollbar->orientation() == HorizontalScrollbar ? ChromiumBridge::PartScrollbarHoriztonalTrack : ChromiumBridge::PartScrollbarVerticalTrack,
        state,
        rect,
        &extraParams);
}

void ScrollbarThemeChromiumLinux::paintButton(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect, ScrollbarPart part)
{
    ChromiumBridge::ThemePart paintPart;
    ChromiumBridge::ThemePaintState state = ChromiumBridge::StateNormal;
    bool checkMin = false;
    bool checkMax = false;
    if (scrollbar->orientation() == HorizontalScrollbar) {
        if (part == BackButtonStartPart) {
            paintPart = ChromiumBridge::PartScrollbarLeftArrow;
            checkMin = true;
        } else {
            paintPart = ChromiumBridge::PartScrollbarRightArrow;
            checkMax = true;
        }
    } else {
        if (part == BackButtonStartPart) {
            paintPart = ChromiumBridge::PartScrollbarUpArrow;
            checkMin = true;
        } else {
            paintPart = ChromiumBridge::PartScrollbarDownArrow;
            checkMax = true;
        }
    }
    if ((checkMin && (scrollbar->currentPos() <= 0))
        || (checkMax && scrollbar->currentPos() == scrollbar->maximum())) {
        state = ChromiumBridge::StateDisabled;
    } else {
        if (part == scrollbar->pressedPart())
            state = ChromiumBridge::StatePressed;
        else if (part == scrollbar->hoveredPart())
            state = ChromiumBridge::StateHover;
    }
    ChromiumBridge::paintThemePart(gc, paintPart, state, rect, 0);
}

void ScrollbarThemeChromiumLinux::paintThumb(GraphicsContext* gc, Scrollbar* scrollbar, const IntRect& rect)
{
    ChromiumBridge::ThemePaintState state;

    if (scrollbar->pressedPart() == ThumbPart)
        state = ChromiumBridge::StatePressed;
    else if (scrollbar->hoveredPart() == ThumbPart)
        state = ChromiumBridge::StateHover;
    else
        state = ChromiumBridge::StateNormal;
    ChromiumBridge::paintThemePart(
        gc,
        scrollbar->orientation() == HorizontalScrollbar ? ChromiumBridge::PartScrollbarHorizontalThumb : ChromiumBridge::PartScrollbarVerticalThumb,
        state,
        rect,
        0);
}

bool ScrollbarThemeChromiumLinux::shouldCenterOnThumb(Scrollbar*, const PlatformMouseEvent& evt)
{
    return (evt.shiftKey() && evt.button() == LeftButton) || (evt.button() == MiddleButton);
}

IntSize ScrollbarThemeChromiumLinux::buttonSize(Scrollbar* scrollbar)
{
    if (scrollbar->orientation() == VerticalScrollbar) {
        IntSize size = ChromiumBridge::getThemePartSize(ChromiumBridge::PartScrollbarUpArrow);
        return IntSize(size.width(), scrollbar->height() < 2 * size.height() ? scrollbar->height() / 2 : size.height());
    }

    // HorizontalScrollbar
    IntSize size = ChromiumBridge::getThemePartSize(ChromiumBridge::PartScrollbarLeftArrow);
    return IntSize(scrollbar->width() < 2 * size.width() ? scrollbar->width() / 2 : size.width(), size.height());
}

int ScrollbarThemeChromiumLinux::minimumThumbLength(Scrollbar* scrollbar)
{
    if (scrollbar->orientation() == VerticalScrollbar) {
        IntSize size = ChromiumBridge::getThemePartSize(ChromiumBridge::PartScrollbarVerticalThumb);
        return size.height();
    }

    IntSize size = ChromiumBridge::getThemePartSize(ChromiumBridge::PartScrollbarHorizontalThumb);
    return size.width();
}

} // namespace WebCore
