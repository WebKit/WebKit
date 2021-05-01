/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright 2009 Maxime Simon <simon.maxime@gmail.com> All Rights Reserved.
 * Copyright 2010 Stephan Aßmus <superstippi@gmx.de> All Rights Reserved.
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
#include <Shape.h>


static int buttonWidth(int scrollbarWidth, int thickness)
{
    return scrollbarWidth < 2 * thickness ? scrollbarWidth / 2 : thickness;
}

namespace WebCore {

ScrollbarTheme& ScrollbarTheme::nativeTheme()
{
	// FIXME: If the ScrollView is embedded in the main frame, we don't want to
	// draw the outer frame, since that is already drawn be the suroundings.
	// So it would be cool if we would instantiate one theme per ScrollView. On
	// the other hand, it looks better with most web sites anyway, since they also
	// draw an outer frame around a scroll area.
    static ScrollbarThemeHaiku theme(false);
    return theme;
}

ScrollbarThemeHaiku::ScrollbarThemeHaiku(bool drawOuterFrame)
    : m_drawOuterFrame(drawOuterFrame)
{
}

ScrollbarThemeHaiku::~ScrollbarThemeHaiku()
{
}


int ScrollbarThemeHaiku::scrollbarThickness(ScrollbarControlSize controlSize, ScrollbarExpansionState)
{
    // FIXME: Should we make a distinction between a Small and a Regular Scrollbar?

    if (m_drawOuterFrame)
       return (int)be_control_look->GetScrollBarWidth() +1;
    return (int)be_control_look->GetScrollBarWidth();
}


bool ScrollbarThemeHaiku::hasButtons(Scrollbar&)
{
    return true;
}

bool ScrollbarThemeHaiku::hasThumb(Scrollbar& scrollbar)
{
    return thumbLength(scrollbar) > 0;
}

IntRect ScrollbarThemeHaiku::backButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool)
{
    if (part == BackButtonEndPart)
        return IntRect();

    int thickness = scrollbarThickness();
    IntPoint buttonOrigin(scrollbar.x(), scrollbar.y());
    IntSize buttonSize = scrollbar.orientation() == HorizontalScrollbar
        ? IntSize(buttonWidth(scrollbar.width(), thickness), thickness)
        : IntSize(thickness, buttonWidth(scrollbar.height(), thickness));
    IntRect buttonRect(buttonOrigin, buttonSize);

    return buttonRect;
}

IntRect ScrollbarThemeHaiku::forwardButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool)
{
    if (part == ForwardButtonStartPart)
        return IntRect();

    int thickness = scrollbarThickness();
    if (scrollbar.orientation() == HorizontalScrollbar) {
        int width = buttonWidth(scrollbar.width(), thickness);
        return IntRect(scrollbar.x() + scrollbar.width() - width, scrollbar.y(), width, thickness);
    }

    int height = buttonWidth(scrollbar.height(), thickness);
    return IntRect(scrollbar.x(), scrollbar.y() + scrollbar.height() - height - 1, thickness, height);
}

IntRect ScrollbarThemeHaiku::trackRect(Scrollbar& scrollbar, bool)
{
    int thickness = scrollbarThickness();
    if (scrollbar.orientation() == HorizontalScrollbar) {
        if (scrollbar.width() < 2 * thickness)
            return IntRect();
        return IntRect(scrollbar.x() + thickness, scrollbar.y(), scrollbar.width() - 2 * thickness, thickness);
    }
    if (scrollbar.height() < 2 * thickness)
        return IntRect();
    return IntRect(scrollbar.x(), scrollbar.y() + thickness, thickness, scrollbar.height() - 2 * thickness - 1);
}

void ScrollbarThemeHaiku::paintScrollCorner(GraphicsContext& context, const IntRect& rect)
{
	if (rect.width() == 0 || rect.height() == 0)
		return;

    BRect drawRect = BRect(rect);
    BView* view = context.platformContext();
    rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    if (!m_drawOuterFrame) {
    	view->SetHighColor(tint_color(base, B_DARKEN_2_TINT));
    	view->StrokeLine(drawRect.LeftBottom(), drawRect.LeftTop());
    	drawRect.left++;
    	view->StrokeLine(drawRect.LeftTop(), drawRect.RightTop());
    	drawRect.top++;
    }
    view->SetHighColor(base);
    view->FillRect(drawRect);
}


void ScrollbarThemeHaiku::paintScrollbarBackground(GraphicsContext& context, Scrollbar& scrollbar)
{
    if (!be_control_look)
        return;

    rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    BRect rect = trackRect(scrollbar, false);
    BView* view = context.platformContext();
    view->SetHighColor(tint_color(base, B_DARKEN_2_TINT));

    enum orientation orientation;
    if (scrollbar.orientation() == HorizontalScrollbar) {
        orientation = B_HORIZONTAL;
        view->StrokeLine(rect.LeftTop(), rect.RightTop());
        if (m_drawOuterFrame)
            view->StrokeLine(rect.LeftBottom(), rect.RightBottom());
        else
            rect.bottom++;
        rect.InsetBy(-1, 1);
    } else {
        orientation = B_VERTICAL;
        view->StrokeLine(rect.LeftTop(), rect.LeftBottom());
        if (m_drawOuterFrame)
            view->StrokeLine(rect.RightTop(), rect.RightBottom());
        else
            rect.right++;
        rect.InsetBy(1, -1);
    }

    uint32 flags = 0;
    if (!scrollbar.enabled())
        flags |= BControlLook::B_DISABLED;
    be_control_look->DrawScrollBarBackground(view, rect, rect, base, flags, orientation);
}


void ScrollbarThemeHaiku::paintButton(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& intRect, ScrollbarPart part)
{
    if (!be_control_look)
        return;

    BRect rect = BRect(intRect);
    rect.right++;
    rect.bottom++;

    BView* view = context.platformContext();
       bool down = scrollbar.pressedPart() == part;

    rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
    rgb_color dark2 = tint_color(base, B_DARKEN_2_TINT);

    enum orientation orientation;
    int arrowDirection;
    if (scrollbar.orientation() == VerticalScrollbar) {
        orientation = B_VERTICAL;
        arrowDirection = part == BackButtonStartPart ? BControlLook::B_UP_ARROW : BControlLook::B_DOWN_ARROW;
        view->SetHighColor(dark2);
        view->StrokeRect(rect);
    } else {
        orientation = B_HORIZONTAL;
        arrowDirection = part == BackButtonStartPart ? BControlLook::B_LEFT_ARROW : BControlLook::B_RIGHT_ARROW;
        view->SetHighColor(dark2);
        view->StrokeRect(rect);
    }

    BRect temp(rect);
    temp.InsetBy(1, 1);
    unsigned flags = 0;
    if (down)
        flags |= BControlLook::B_ACTIVATED;
    if (!scrollbar.enabled())
        flags |= BControlLook::B_DISABLED;

    be_control_look->DrawButtonBackground(view, temp, rect, base, flags,
        BControlLook::B_ALL_BORDERS, orientation);

    temp.InsetBy(-1, -1);
    be_control_look->DrawArrowShape(view, temp, rect,
        base, arrowDirection, flags, B_DARKEN_MAX_TINT);
}

void ScrollbarThemeHaiku::paintThumb(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect)
{
    if (!be_control_look)
        return;

    BRect drawRect = BRect(rect);
    BView* view = context.platformContext();
    rgb_color base = ui_color(B_SCROLL_BAR_THUMB_COLOR);
    rgb_color dark2 = tint_color(base, B_DARKEN_2_TINT);
    rgb_color dark3 = tint_color(base, B_DARKEN_3_TINT);

    view->PushState();

    enum orientation orientation;
    if (scrollbar.orientation() == VerticalScrollbar) {
        orientation = B_VERTICAL;
        drawRect.InsetBy(1, -1);
        if (!m_drawOuterFrame)
            drawRect.right++;
        view->SetHighColor(dark2);
        view->StrokeLine(drawRect.LeftTop(), drawRect.RightTop());
        view->SetHighColor(dark3);
        view->StrokeLine(drawRect.LeftBottom(), drawRect.RightBottom());
        drawRect.InsetBy(0, 1);
    } else {
        orientation = B_HORIZONTAL;
        drawRect.InsetBy(-1, 1);
        if (!m_drawOuterFrame)
            drawRect.bottom++;
        view->SetHighColor(dark2);
        view->StrokeLine(drawRect.LeftTop(), drawRect.LeftBottom());
        view->SetHighColor(dark3);
        view->StrokeLine(drawRect.RightTop(), drawRect.RightBottom());
        drawRect.InsetBy(1, 0);
    }


    uint32 flags = 0;
    if (!scrollbar.enabled())
        flags |= BControlLook::B_DISABLED;
    be_control_look->DrawButtonBackground(view, drawRect, view->Bounds(), base, flags, BControlLook::B_ALL_BORDERS, orientation);

    view->PopState();
}


} // namespace WebCore

