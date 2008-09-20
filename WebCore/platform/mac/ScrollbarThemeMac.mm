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
#include "ScrollbarThemeMac.h"

#include "GraphicsContext.h"
#include "IntRect.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "Scrollbar.h"
#include "ScrollbarClient.h"
#include "Settings.h"

// FIXME: There are repainting problems due to Aqua scroll bar buttons' visual overflow.

using namespace std;

namespace WebCore {

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeMac theme;
    return &theme;
}

// FIXME: Get these numbers from CoreUI.
static int cScrollbarThickness[] = { 15, 11 };
static int cRealButtonLength[] = { 28, 21 };
static int cButtonInset[] = { 14, 11 };
static int cButtonHitInset[] = { 3, 2 };
// cRealButtonLength - cButtonInset
static int cButtonLength[] = { 14, 10 };
static int cThumbMinLength[] = { 26, 20 };

static float gInitialButtonDelay = 0.5f;
static float gAutoscrollButtonDelay = 0.05f;
static bool gJumpOnTrackClick = false;
static ScrollbarButtonsPlacement gButtonPlacement = ScrollbarButtonsDoubleEnd;

static void updateArrowPlacement()
{
    NSString *buttonPlacement = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleScrollBarVariant"];
    if ([buttonPlacement isEqualToString:@"Single"])
        gButtonPlacement = ScrollbarButtonsSingle;
    else if ([buttonPlacement isEqualToString:@"DoubleMin"])
        gButtonPlacement = ScrollbarButtonsDoubleStart;
    else if ([buttonPlacement isEqualToString:@"DoubleBoth"])
        gButtonPlacement = ScrollbarButtonsDoubleBoth;
    else
        gButtonPlacement = ScrollbarButtonsDoubleEnd; // The default is ScrollbarButtonsDoubleEnd.
}

ScrollbarThemeMac::ScrollbarThemeMac()
{
    static bool initialized;
    if (!initialized) {
        initialized = true;
        updateArrowPlacement();
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        gInitialButtonDelay = [defaults floatForKey:@"NSScrollerButtonDelay"];
        gAutoscrollButtonDelay = [defaults floatForKey:@"NSScrollerButtonPeriod"];
        gJumpOnTrackClick = [defaults boolForKey:@"AppleScrollerPagingBehavior"];
    }
}

ScrollbarThemeMac::~ScrollbarThemeMac()
{
}

int ScrollbarThemeMac::scrollbarThickness(ScrollbarControlSize controlSize)
{
    return cScrollbarThickness[controlSize];
}

double ScrollbarThemeMac::initialAutoscrollTimerDelay()
{
    return gInitialButtonDelay;
}

double ScrollbarThemeMac::autoscrollTimerDelay()
{
    return gAutoscrollButtonDelay;
}
    
ScrollbarButtonsPlacement ScrollbarThemeMac::buttonsPlacement() const
{
    return gButtonPlacement;
}

bool ScrollbarThemeMac::hasButtons(Scrollbar* scrollbar)
{
    return scrollbar->enabled() && (scrollbar->orientation() == HorizontalScrollbar ? 
             scrollbar->width() : 
             scrollbar->height()) >= 2 * (cRealButtonLength[scrollbar->controlSize()] - cButtonHitInset[scrollbar->controlSize()]);
}

bool ScrollbarThemeMac::hasThumb(Scrollbar* scrollbar)
{
    return scrollbar->enabled() && (scrollbar->orientation() == HorizontalScrollbar ? 
             scrollbar->width() : 
             scrollbar->height()) >= 2 * cButtonInset[scrollbar->controlSize()] + cThumbMinLength[scrollbar->controlSize()] + 1;
}

static IntRect buttonRepaintRect(const IntRect& buttonRect, ScrollbarOrientation orientation, ScrollbarControlSize controlSize, bool start)
{
    IntRect paintRect(buttonRect);
    if (orientation == HorizontalScrollbar) {
        paintRect.setWidth(cRealButtonLength[controlSize]);
        if (!start)
            paintRect.setX(buttonRect.x() - (cRealButtonLength[controlSize] - buttonRect.width()));
    } else {
        paintRect.setHeight(cRealButtonLength[controlSize]);
        if (!start)
            paintRect.setY(buttonRect.y() - (cRealButtonLength[controlSize] - buttonRect.height()));
    }

    return paintRect;
}

IntRect ScrollbarThemeMac::backButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool painting)
{
    IntRect result;
    
    // FIXME: Support more than just the single arrow style.
    if (part == BackButtonEndPart)
        return result;

    int thickness = scrollbarThickness(scrollbar->controlSize());
    if (scrollbar->orientation() == HorizontalScrollbar)
        result = IntRect(scrollbar->x(), scrollbar->y(), cButtonLength[scrollbar->controlSize()], thickness);
    else
        result = IntRect(scrollbar->x(), scrollbar->y(), thickness, cButtonLength[scrollbar->controlSize()]);
    if (painting)
        return buttonRepaintRect(result, scrollbar->orientation(), scrollbar->controlSize(), true);
    return result;
}

IntRect ScrollbarThemeMac::forwardButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool painting)
{
    IntRect result;
    
    // FIXME: Support more than just the single arrow style.
    if (part == ForwardButtonStartPart)
        return result;
    
    int thickness = scrollbarThickness(scrollbar->controlSize());
    if (scrollbar->orientation() == HorizontalScrollbar)
        result = IntRect(scrollbar->x() + scrollbar->width() - cButtonLength[scrollbar->controlSize()], scrollbar->y(), cButtonLength[scrollbar->controlSize()], thickness);
    else
        result = IntRect(scrollbar->x(), scrollbar->y() + scrollbar->height() - cButtonLength[scrollbar->controlSize()], thickness, cButtonLength[scrollbar->controlSize()]);
    if (painting)
        return buttonRepaintRect(result, scrollbar->orientation(), scrollbar->controlSize(), false);
    return result;
}

static IntRect trackRepaintRect(const IntRect& trackRect, ScrollbarOrientation orientation, ScrollbarControlSize controlSize)
{
    IntRect paintRect(trackRect);
    if (orientation == HorizontalScrollbar)
        paintRect.inflateX(cButtonLength[controlSize]);
    else
        paintRect.inflateY(cButtonLength[controlSize]);

    return paintRect;
}

IntRect ScrollbarThemeMac::trackRect(Scrollbar* scrollbar, bool painting)
{
    IntRect result;
    int thickness = scrollbarThickness(scrollbar->controlSize());
    if (scrollbar->orientation() == HorizontalScrollbar) {
        if (!hasButtons(scrollbar))
            result = IntRect(scrollbar->x(), scrollbar->y(), scrollbar->width(), thickness);
        else
            result = IntRect(scrollbar->x() + cButtonLength[scrollbar->controlSize()], scrollbar->y(), scrollbar->width() - 2 * cButtonLength[scrollbar->controlSize()], thickness);
    } else if (!hasButtons(scrollbar))
        result = IntRect(scrollbar->x(), scrollbar->y(), thickness, scrollbar->height());
    else
        result = IntRect(scrollbar->x(), scrollbar->y() + cButtonLength[scrollbar->controlSize()], thickness, scrollbar->height() - 2 * cButtonLength[scrollbar->controlSize()]);
    if (painting)
        return trackRepaintRect(result, scrollbar->orientation(), scrollbar->controlSize());
    return result;
}

int ScrollbarThemeMac::minimumThumbLength(Scrollbar* scrollbar)
{
    return cThumbMinLength[scrollbar->controlSize()];
}

bool ScrollbarThemeMac::shouldCenterOnThumb(Scrollbar*, const PlatformMouseEvent& evt)
{
    if (evt.button() != LeftButton)
        return false;
    if (gJumpOnTrackClick)
        return !evt.altKey();
    return evt.altKey();
}

void ScrollbarThemeMac::paintTrack(GraphicsContext* graphicsContext, Scrollbar* scrollbar, const IntRect& trackRect, ScrollbarControlPartMask)
{
#if !USE(NSSCROLLER)
    graphicsContext->fillRect(trackRect, Color(255, 255, 255));
#endif
}

void ScrollbarThemeMac::paintButton(GraphicsContext* graphicsContext, Scrollbar* scrollbar, const IntRect& buttonRect, ScrollbarPart)
{
#if !USE(NSSCROLLER)
    graphicsContext->fillRect(buttonRect, Color(64, 64, 64));
#endif
}

void ScrollbarThemeMac::paintThumb(GraphicsContext* graphicsContext, Scrollbar* scrollbar, const IntRect& thumbRect)
{
#if !USE(NSSCROLLER)
    graphicsContext->fillRect(thumbRect, Color(192, 192, 192));
#endif
}

}

