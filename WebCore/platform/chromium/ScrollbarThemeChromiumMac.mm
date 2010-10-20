/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
#include "ScrollbarThemeChromiumMac.h"

#include "ImageBuffer.h"
#include "PlatformMouseEvent.h"
#include "ScrollView.h"
#include <Carbon/Carbon.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

// FIXME: There are repainting problems due to Aqua scroll bar buttons' visual overflow.

using namespace std;
using namespace WebCore;

// This file (and its associated .h file) is a clone of ScrollbarThemeMac.mm.
// Because we want to draw tickmarks in the scrollbar, we must maintain a fork.
// Please maintain this file by performing parallel changes to it.
//
// The only changes from ScrollbarThemeMac should be:
// - The classname change from ScrollbarThemeMac to ScrollbarThemeChromiumMac.
// - In paint() the code to paint the track, tickmarks, and thumb separately.
//
// For all other differences, if it was introduced in this file, then the
// maintainer forgot to include it in the list; otherwise it is an update that
// should have been applied to this file but was not.

static HashSet<Scrollbar*>* gScrollbars;

@interface ScrollbarPrefsObserver : NSObject
{

}

+ (void)registerAsObserver;
+ (void)appearancePrefsChanged:(NSNotification*)theNotification;
+ (void)behaviorPrefsChanged:(NSNotification*)theNotification;

@end

@implementation ScrollbarPrefsObserver

+ (void)appearancePrefsChanged:(NSNotification*)unusedNotification
{
    UNUSED_PARAM(unusedNotification);

    static_cast<ScrollbarThemeChromiumMac*>(ScrollbarTheme::nativeTheme())->preferencesChanged();
    if (!gScrollbars)
        return;
    HashSet<Scrollbar*>::iterator end = gScrollbars->end();
    for (HashSet<Scrollbar*>::iterator it = gScrollbars->begin(); it != end; ++it) {
        (*it)->styleChanged();
        (*it)->invalidate();
    }
}

+ (void)behaviorPrefsChanged:(NSNotification*)unusedNotification
{
    UNUSED_PARAM(unusedNotification);

    static_cast<ScrollbarThemeChromiumMac*>(ScrollbarTheme::nativeTheme())->preferencesChanged();
}

+ (void)registerAsObserver
{
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(appearancePrefsChanged:) name:@"AppleAquaScrollBarVariantChanged" object:nil suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(behaviorPrefsChanged:) name:@"AppleNoRedisplayAppearancePreferenceChanged" object:nil suspensionBehavior:NSNotificationSuspensionBehaviorCoalesce];
}

@end

namespace WebCore {

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    DEFINE_STATIC_LOCAL(ScrollbarThemeChromiumMac, theme, ());
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

static int cOuterButtonLength[] = { 16, 14 }; // The outer button in a double button pair is a bit bigger.
static int cOuterButtonOverlap = 2;

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

void ScrollbarThemeChromiumMac::registerScrollbar(Scrollbar* scrollbar)
{
    if (!gScrollbars)
        gScrollbars = new HashSet<Scrollbar*>;
    gScrollbars->add(scrollbar);
}

void ScrollbarThemeChromiumMac::unregisterScrollbar(Scrollbar* scrollbar)
{
    gScrollbars->remove(scrollbar);
    if (gScrollbars->isEmpty()) {
        delete gScrollbars;
        gScrollbars = 0;
    }
}

ScrollbarThemeChromiumMac::ScrollbarThemeChromiumMac()
{
    static bool initialized;
    if (!initialized) {
        initialized = true;
        [ScrollbarPrefsObserver registerAsObserver];
        preferencesChanged();
    }
}

ScrollbarThemeChromiumMac::~ScrollbarThemeChromiumMac()
{
}

void ScrollbarThemeChromiumMac::preferencesChanged()
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults synchronize];
    updateArrowPlacement();
    gInitialButtonDelay = [defaults floatForKey:@"NSScrollerButtonDelay"];
    gAutoscrollButtonDelay = [defaults floatForKey:@"NSScrollerButtonPeriod"];
    gJumpOnTrackClick = [defaults boolForKey:@"AppleScrollerPagingBehavior"];
}

int ScrollbarThemeChromiumMac::scrollbarThickness(ScrollbarControlSize controlSize)
{
    return cScrollbarThickness[controlSize];
}

double ScrollbarThemeChromiumMac::initialAutoscrollTimerDelay()
{
    return gInitialButtonDelay;
}

double ScrollbarThemeChromiumMac::autoscrollTimerDelay()
{
    return gAutoscrollButtonDelay;
}

ScrollbarButtonsPlacement ScrollbarThemeChromiumMac::buttonsPlacement() const
{
    return gButtonPlacement;
}

bool ScrollbarThemeChromiumMac::hasButtons(Scrollbar* scrollbar)
{
    return scrollbar->enabled() && (scrollbar->orientation() == HorizontalScrollbar ?
             scrollbar->width() :
             scrollbar->height()) >= 2 * (cRealButtonLength[scrollbar->controlSize()] - cButtonHitInset[scrollbar->controlSize()]);
}

bool ScrollbarThemeChromiumMac::hasThumb(Scrollbar* scrollbar)
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

IntRect ScrollbarThemeChromiumMac::backButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool painting)
{
    IntRect result;

    if (part == BackButtonStartPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleEnd))
        return result;

    if (part == BackButtonEndPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleStart || buttonsPlacement() == ScrollbarButtonsSingle))
        return result;

    int thickness = scrollbarThickness(scrollbar->controlSize());
    bool outerButton = part == BackButtonStartPart && (buttonsPlacement() == ScrollbarButtonsDoubleStart || buttonsPlacement() == ScrollbarButtonsDoubleBoth);
    if (outerButton) {
        if (scrollbar->orientation() == HorizontalScrollbar)
            result = IntRect(scrollbar->x(), scrollbar->y(), cOuterButtonLength[scrollbar->controlSize()] + painting ? cOuterButtonOverlap : 0, thickness);
        else
            result = IntRect(scrollbar->x(), scrollbar->y(), thickness, cOuterButtonLength[scrollbar->controlSize()] + painting ? cOuterButtonOverlap : 0);
        return result;
    }

    // Our repaint rect is slightly larger, since we are a button that is adjacent to the track.
    if (scrollbar->orientation() == HorizontalScrollbar) {
        int start = part == BackButtonStartPart ? scrollbar->x() : scrollbar->x() + scrollbar->width() - cOuterButtonLength[scrollbar->controlSize()] - cButtonLength[scrollbar->controlSize()];
        result = IntRect(start, scrollbar->y(), cButtonLength[scrollbar->controlSize()], thickness);
    } else {
        int start = part == BackButtonStartPart ? scrollbar->y() : scrollbar->y() + scrollbar->height() - cOuterButtonLength[scrollbar->controlSize()] - cButtonLength[scrollbar->controlSize()];
        result = IntRect(scrollbar->x(), start, thickness, cButtonLength[scrollbar->controlSize()]);
    }

    if (painting)
        return buttonRepaintRect(result, scrollbar->orientation(), scrollbar->controlSize(), part == BackButtonStartPart);
    return result;
}

IntRect ScrollbarThemeChromiumMac::forwardButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool painting)
{
    IntRect result;

    if (part == ForwardButtonEndPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleStart))
        return result;

    if (part == ForwardButtonStartPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleEnd || buttonsPlacement() == ScrollbarButtonsSingle))
        return result;

    int thickness = scrollbarThickness(scrollbar->controlSize());
    int outerButtonLength = cOuterButtonLength[scrollbar->controlSize()];
    int buttonLength = cButtonLength[scrollbar->controlSize()];

    bool outerButton = part == ForwardButtonEndPart && (buttonsPlacement() == ScrollbarButtonsDoubleEnd || buttonsPlacement() == ScrollbarButtonsDoubleBoth);
    if (outerButton) {
        if (scrollbar->orientation() == HorizontalScrollbar) {
            result = IntRect(scrollbar->x() + scrollbar->width() - outerButtonLength, scrollbar->y(), outerButtonLength, thickness);
            if (painting)
                result.inflateX(cOuterButtonOverlap);
        } else {
            result = IntRect(scrollbar->x(), scrollbar->y() + scrollbar->height() - outerButtonLength, thickness, outerButtonLength);
            if (painting)
                result.inflateY(cOuterButtonOverlap);
        }
        return result;
    }

    if (scrollbar->orientation() == HorizontalScrollbar) {
        int start = part == ForwardButtonEndPart ? scrollbar->x() + scrollbar->width() - buttonLength : scrollbar->x() + outerButtonLength;
        result = IntRect(start, scrollbar->y(), buttonLength, thickness);
    } else {
        int start = part == ForwardButtonEndPart ? scrollbar->y() + scrollbar->height() - buttonLength : scrollbar->y() + outerButtonLength;
        result = IntRect(scrollbar->x(), start, thickness, buttonLength);
    }
    if (painting)
        return buttonRepaintRect(result, scrollbar->orientation(), scrollbar->controlSize(), part == ForwardButtonStartPart);
    return result;
}

IntRect ScrollbarThemeChromiumMac::trackRect(Scrollbar* scrollbar, bool painting)
{
    if (painting || !hasButtons(scrollbar))
        return scrollbar->frameRect();

    IntRect result;
    int thickness = scrollbarThickness(scrollbar->controlSize());
    int startWidth = 0;
    int endWidth = 0;
    int outerButtonLength = cOuterButtonLength[scrollbar->controlSize()];
    int buttonLength = cButtonLength[scrollbar->controlSize()];
    int doubleButtonLength = outerButtonLength + buttonLength;
    switch (buttonsPlacement()) {
        case ScrollbarButtonsSingle:
            startWidth = buttonLength;
            endWidth = buttonLength;
            break;
        case ScrollbarButtonsDoubleStart:
            startWidth = doubleButtonLength;
            break;
        case ScrollbarButtonsDoubleEnd:
            endWidth = doubleButtonLength;
            break;
        case ScrollbarButtonsDoubleBoth:
            startWidth = doubleButtonLength;
            endWidth = doubleButtonLength;
            break;
        default:
            break;
    }

    int totalWidth = startWidth + endWidth;
    if (scrollbar->orientation() == HorizontalScrollbar)
        return IntRect(scrollbar->x() + startWidth, scrollbar->y(), scrollbar->width() - totalWidth, thickness);
    return IntRect(scrollbar->x(), scrollbar->y() + startWidth, thickness, scrollbar->height() - totalWidth);
}

int ScrollbarThemeChromiumMac::minimumThumbLength(Scrollbar* scrollbar)
{
    return cThumbMinLength[scrollbar->controlSize()];
}

bool ScrollbarThemeChromiumMac::shouldCenterOnThumb(Scrollbar*, const PlatformMouseEvent& evt)
{
    if (evt.button() != LeftButton)
        return false;
    if (gJumpOnTrackClick)
        return !evt.altKey();
    return evt.altKey();
}

static int scrollbarPartToHIPressedState(ScrollbarPart part)
{
    switch (part) {
        case BackButtonStartPart:
            return kThemeTopOutsideArrowPressed;
        case BackButtonEndPart:
            return kThemeTopOutsideArrowPressed; // This does not make much sense.  For some reason the outside constant is required.
        case ForwardButtonStartPart:
            return kThemeTopInsideArrowPressed;
        case ForwardButtonEndPart:
            return kThemeBottomOutsideArrowPressed;
        case ThumbPart:
            return kThemeThumbPressed;
        default:
            return 0;
    }
}

bool ScrollbarThemeChromiumMac::paint(Scrollbar* scrollbar, GraphicsContext* context, const IntRect& damageRect)
{
    HIThemeTrackDrawInfo trackInfo;
    trackInfo.version = 0;
    trackInfo.kind = scrollbar->controlSize() == RegularScrollbar ? kThemeMediumScrollBar : kThemeSmallScrollBar;
    trackInfo.bounds = scrollbar->frameRect();
    trackInfo.min = 0;
    trackInfo.max = scrollbar->maximum();
    trackInfo.value = scrollbar->currentPos();
    trackInfo.trackInfo.scrollbar.viewsize = scrollbar->visibleSize();
    trackInfo.attributes = 0;
    if (scrollbar->orientation() == HorizontalScrollbar)
        trackInfo.attributes |= kThemeTrackHorizontal;

    if (!scrollbar->enabled())
        trackInfo.enableState = kThemeTrackDisabled;
    else
        trackInfo.enableState = scrollbar->client()->isActive() ? kThemeTrackActive : kThemeTrackInactive;

    if (!hasButtons(scrollbar))
        trackInfo.enableState = kThemeTrackNothingToScroll;
    trackInfo.trackInfo.scrollbar.pressState = scrollbarPartToHIPressedState(scrollbar->pressedPart());

    CGAffineTransform currentCTM = CGContextGetCTM(context->platformContext());

    // The Aqua scrollbar is buggy when rotated and scaled.  We will just draw into a bitmap if we detect a scale or rotation.
    bool canDrawDirectly = currentCTM.a == 1.0f && currentCTM.b == 0.0f && currentCTM.c == 0.0f && (currentCTM.d == 1.0f || currentCTM.d == -1.0f);
    GraphicsContext* drawingContext = context;
    OwnPtr<ImageBuffer> imageBuffer;
    if (!canDrawDirectly) {
        trackInfo.bounds = IntRect(IntPoint(), scrollbar->frameRect().size());

        IntRect bufferRect(scrollbar->frameRect());
        bufferRect.intersect(damageRect);
        bufferRect.move(-scrollbar->frameRect().x(), -scrollbar->frameRect().y());

        imageBuffer = ImageBuffer::create(bufferRect.size());
        if (!imageBuffer)
            return true;

        drawingContext = imageBuffer->context();
    }

    // Draw thumbless.
    HIThemeDrawTrack(&trackInfo, 0, drawingContext->platformContext(), kHIThemeOrientationNormal);

    Vector<IntRect> tickmarks;
    scrollbar->client()->getTickmarks(tickmarks);
    if (scrollbar->orientation() == VerticalScrollbar && tickmarks.size()) {
        drawingContext->save();
        drawingContext->setShouldAntialias(false);
        drawingContext->setStrokeColor(Color(0xCC, 0xAA, 0x00, 0xFF), ColorSpaceDeviceRGB);
        drawingContext->setFillColor(Color(0xFF, 0xDD, 0x00, 0xFF), ColorSpaceDeviceRGB);

        IntRect thumbArea = trackRect(scrollbar, false);
        if (!canDrawDirectly) {
            thumbArea.setX(0);
            thumbArea.setY(0);
        }
        // The ends are rounded and the thumb doesn't go there.
        thumbArea.inflateY(-thumbArea.width());

        for (Vector<IntRect>::const_iterator i = tickmarks.begin(); i != tickmarks.end(); ++i) {
            // Calculate how far down (in %) the tick-mark should appear.
            const float percent = static_cast<float>(i->y()) / scrollbar->totalSize();
            if (percent < 0.0 || percent > 1.0)
              continue;

            // Calculate how far down (in pixels) the tick-mark should appear.
            const int yPos = static_cast<int>((thumbArea.topLeft().y() + (thumbArea.height() * percent))) & ~1;

            // Paint.
            const int indent = 2;
            FloatRect tickRect(thumbArea.topLeft().x() + indent, yPos, thumbArea.width() - 2 * indent - 1, 2);
            drawingContext->fillRect(tickRect);
            drawingContext->strokeRect(tickRect, 1);
        }

        drawingContext->restore();
    }

    if (hasThumb(scrollbar)) {
        trackInfo.attributes |= (kThemeTrackShowThumb | kThemeTrackHideTrack);
        HIThemeDrawTrack(&trackInfo, 0, drawingContext->platformContext(), kHIThemeOrientationNormal);
    }

    if (!canDrawDirectly)
        context->drawImageBuffer(imageBuffer.get(), ColorSpaceDeviceRGB, scrollbar->frameRect().location());

    return true;
}

}

