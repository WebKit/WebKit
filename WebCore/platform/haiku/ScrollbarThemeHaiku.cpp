/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright 2009 Maxime Simon <simon.maxime@gmail.com> All Rights Reserved.
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
#include "ScrollbarThemeHaiku.h"

#include "GraphicsContext.h"
#include "Scrollbar.h"
#include <ControlLook.h>
#include <InterfaceDefs.h>


int buttonWidth(int scrollbarWidth, int thickness)
{
    return scrollbarWidth < 2 * thickness ? scrollbarWidth / 2 : thickness;
}

namespace WebCore {

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeHaiku theme;
    return &theme;
}

ScrollbarThemeHaiku::ScrollbarThemeHaiku()
{
}

ScrollbarThemeHaiku::~ScrollbarThemeHaiku()
{
}

int ScrollbarThemeHaiku::scrollbarThickness(ScrollbarControlSize controlSize)
{
    // FIXME: Should we make a distinction between a Small and a Regular Scrollbar?
    return 16;
}

bool ScrollbarThemeHaiku::hasButtons(Scrollbar* scrollbar)
{
    return scrollbar->enabled();
}

bool ScrollbarThemeHaiku::hasThumb(Scrollbar* scrollbar)
{
    return scrollbar->enabled() && thumbLength(scrollbar) > 0;
}

IntRect ScrollbarThemeHaiku::backButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool)
{
    if (part == BackButtonEndPart)
        return IntRect();

    int thickness = scrollbarThickness();
    IntPoint buttonOrigin(scrollbar->x(), scrollbar->y());
    IntSize buttonSize = scrollbar->orientation() == HorizontalScrollbar
        ? IntSize(buttonWidth(scrollbar->width(), thickness), thickness)
        : IntSize(thickness, buttonWidth(scrollbar->height(), thickness));
    IntRect buttonRect(buttonOrigin, buttonSize);

    return buttonRect;
}

IntRect ScrollbarThemeHaiku::forwardButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool)
{
    if (part == BackButtonStartPart)
        return IntRect();

    int thickness = scrollbarThickness();
    if (scrollbar->orientation() == HorizontalScrollbar) {
        int width = buttonWidth(scrollbar->width(), thickness);
        return IntRect(scrollbar->x() + scrollbar->width() - width, scrollbar->y(), width, thickness);
    }

    int height = buttonWidth(scrollbar->height(), thickness);
    return IntRect(scrollbar->x(), scrollbar->y() + scrollbar->height() - height, thickness, height);
}

IntRect ScrollbarThemeHaiku::trackRect(Scrollbar* scrollbar, bool)
{
    int thickness = scrollbarThickness();
    if (scrollbar->orientation() == HorizontalScrollbar) {
        if (scrollbar->width() < 2 * thickness)
            return IntRect();
        return IntRect(scrollbar->x() + thickness, scrollbar->y(), scrollbar->width() - 2 * thickness, thickness);
    }
    if (scrollbar->height() < 2 * thickness)
        return IntRect();
    return IntRect(scrollbar->x(), scrollbar->y() + thickness, thickness, scrollbar->height() - 2 * thickness);
}

void ScrollbarThemeHaiku::paintScrollbarBackground(GraphicsContext* context, Scrollbar* scrollbar)
{
    if (!be_control_look)
        return;

    BRect rect = trackRect(scrollbar, false);
    orientation scrollbarOrientation = scrollbar->orientation() == HorizontalScrollbar ? B_HORIZONTAL : B_VERTICAL;

    be_control_look->DrawScrollBarBackground(context->platformContext(), rect, rect, ui_color(B_PANEL_BACKGROUND_COLOR), 0, scrollbarOrientation);
}

void ScrollbarThemeHaiku::paintButton(GraphicsContext* context, Scrollbar* scrollbar, const IntRect& rect, ScrollbarPart part)
{
    if (!be_control_look)
        return;

    BRect drawRect = BRect(rect);
    BView* view = context->platformContext();
    rgb_color panelBgColor = ui_color(B_PANEL_BACKGROUND_COLOR);
    rgb_color buttonBgColor = tint_color(panelBgColor, B_LIGHTEN_1_TINT);

    be_control_look->DrawButtonFrame(view, drawRect, drawRect, buttonBgColor, panelBgColor);
    be_control_look->DrawButtonBackground(view, drawRect, drawRect, buttonBgColor);

    int arrowDirection;
    if (scrollbar->orientation() == VerticalScrollbar)
        arrowDirection = part == BackButtonStartPart ? BControlLook::B_UP_ARROW : BControlLook::B_DOWN_ARROW;
    else
        arrowDirection = part == BackButtonStartPart ? BControlLook::B_LEFT_ARROW : BControlLook::B_RIGHT_ARROW;

    be_control_look->DrawArrowShape(view, drawRect, drawRect, ui_color(B_CONTROL_TEXT_COLOR), arrowDirection);
}

void ScrollbarThemeHaiku::paintThumb(GraphicsContext* context, Scrollbar*, const IntRect& rect)
{
    if (!be_control_look)
        return;

    BRect drawRect = BRect(rect);
    BView* view = context->platformContext();
    rgb_color panelBgColor = ui_color(B_PANEL_BACKGROUND_COLOR);
    rgb_color buttonBgColor = tint_color(panelBgColor, B_LIGHTEN_1_TINT);

    be_control_look->DrawButtonFrame(view, drawRect, drawRect, buttonBgColor, panelBgColor);
    be_control_look->DrawButtonBackground(view, drawRect, drawRect, buttonBgColor);
}

}

