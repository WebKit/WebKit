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

#include "BitmapImage.h"
#include "FrameView.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "LocalCurrentGraphicsContext.h"
#include "PlatformMouseEvent.h"
#include "PlatformSupport.h"
#include "ScrollAnimatorChromiumMac.h"
#include "ScrollView.h"
#include <Carbon/Carbon.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

#if USE(SKIA)
#include "PlatformContextSkia.h"
#include "skia/ext/skia_utils_mac.h"
#endif



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
// - In paint() the thumb is drawn via ChromeBridge/WebThemeEngine.
// - Various functions that were split using #if USE(WK_SCROLLBAR_PAINTER)
//   have been combined using runtime checks instead.
//
// For all other differences, if it was introduced in this file, then the
// maintainer forgot to include it in the list; otherwise it is an update that
// should have been applied to this file but was not.

namespace WebCore {

typedef HashMap<Scrollbar*, RetainPtr<WKScrollbarPainterRef> > ScrollbarPainterMap;

static ScrollbarPainterMap* scrollbarMap()
{
    static ScrollbarPainterMap* map = new ScrollbarPainterMap;
    return map;
}

}

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

    ScrollbarTheme* theme = ScrollbarTheme::theme();
    if (theme->isMockTheme())
        return;

    static_cast<ScrollbarThemeChromiumMac*>(theme)->preferencesChanged();
    if (scrollbarMap()->isEmpty())
        return;
    ScrollbarPainterMap::iterator end = scrollbarMap()->end();
    for (ScrollbarPainterMap::iterator it = scrollbarMap()->begin(); it != end; ++it) {
        it->first->styleChanged();
        it->first->invalidate();
    }
}

+ (void)behaviorPrefsChanged:(NSNotification*)unusedNotification
{
    UNUSED_PARAM(unusedNotification);

    ScrollbarTheme* theme = ScrollbarTheme::theme();
    if (theme->isMockTheme())
        return;

    static_cast<ScrollbarThemeChromiumMac*>(theme)->preferencesChanged();
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
static ScrollbarButtonsPlacement gButtonPlacement = isScrollbarOverlayAPIAvailable() ? ScrollbarButtonsNone : ScrollbarButtonsDoubleEnd;

static void updateArrowPlacement()
{
    if (isScrollbarOverlayAPIAvailable())
        return;

    NSString *buttonPlacement = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleScrollBarVariant"];
    if ([buttonPlacement isEqualToString:@"Single"])
        gButtonPlacement = ScrollbarButtonsSingle;
    else if ([buttonPlacement isEqualToString:@"DoubleMin"])
        gButtonPlacement = ScrollbarButtonsDoubleStart;
    else if ([buttonPlacement isEqualToString:@"DoubleBoth"])
        gButtonPlacement = ScrollbarButtonsDoubleBoth;
    else
        gButtonPlacement = ScrollbarButtonsDoubleEnd;
}

void ScrollbarThemeChromiumMac::registerScrollbar(Scrollbar* scrollbar)
{
    bool isHorizontal = scrollbar->orientation() == HorizontalScrollbar;
    WKScrollbarPainterRef scrollbarPainter = wkMakeScrollbarPainter(scrollbar->controlSize(), isHorizontal);
    scrollbarMap()->add(scrollbar, scrollbarPainter);
    updateEnabledState(scrollbar);
    updateScrollbarOverlayStyle(scrollbar);
}

void ScrollbarThemeChromiumMac::unregisterScrollbar(Scrollbar* scrollbar)
{
    scrollbarMap()->remove(scrollbar);
}

void ScrollbarThemeChromiumMac::setNewPainterForScrollbar(Scrollbar* scrollbar, WKScrollbarPainterRef newPainter)
{
    scrollbarMap()->set(scrollbar, newPainter);
    updateEnabledState(scrollbar);
    updateScrollbarOverlayStyle(scrollbar);
}

WKScrollbarPainterRef ScrollbarThemeChromiumMac::painterForScrollbar(Scrollbar* scrollbar)
{
    return scrollbarMap()->get(scrollbar).get();
}

ScrollbarThemeChromiumMac::ScrollbarThemeChromiumMac()
{
    static bool initialized;
    if (!initialized) {
        initialized = true;

        // Load the linen pattern image used for overhang drawing.
        RefPtr<Image> patternImage = Image::loadPlatformResource("overhangPattern");
        m_overhangPattern = Pattern::create(patternImage, true, true);

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
    if (isScrollbarOverlayAPIAvailable())
        return wkScrollbarThickness(controlSize);
    else
        return cScrollbarThickness[controlSize];
}

bool ScrollbarThemeChromiumMac::usesOverlayScrollbars() const
{
    if (isScrollbarOverlayAPIAvailable())
        return wkScrollbarPainterUsesOverlayScrollers();
    else
        return false;
}

static inline wkScrollerKnobStyle toScrollbarPainterKnobStyle(ScrollbarOverlayStyle style)
{
    switch (style) {
    case ScrollbarOverlayStyleDark:
        return wkScrollerKnobStyleDark;
    case ScrollbarOverlayStyleLight:
        return wkScrollerKnobStyleLight;
    default:
        return wkScrollerKnobStyleDefault;
    }
}

void ScrollbarThemeChromiumMac::updateScrollbarOverlayStyle(Scrollbar* scrollbar)
{
    if (isScrollbarOverlayAPIAvailable()) {
        wkSetScrollbarPainterKnobStyle(painterForScrollbar(scrollbar), toScrollbarPainterKnobStyle(scrollbar->scrollableArea()->scrollbarOverlayStyle()));
    }
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
    return scrollbar->enabled() && gButtonPlacement != ScrollbarButtonsNone
             && (scrollbar->orientation() == HorizontalScrollbar
             ? scrollbar->width()
             : scrollbar->height()) >= 2 * (cRealButtonLength[scrollbar->controlSize()] - cButtonHitInset[scrollbar->controlSize()]);
}

bool ScrollbarThemeChromiumMac::hasThumb(Scrollbar* scrollbar)
{
    int minLengthForThumb;
    if (isScrollbarOverlayAPIAvailable())
        minLengthForThumb = wkScrollbarMinimumTotalLengthNeededForThumb(scrollbarMap()->get(scrollbar).get());
    else
        minLengthForThumb = 2 * cButtonInset[scrollbar->controlSize()] + cThumbMinLength[scrollbar->controlSize()] + 1;
    return scrollbar->enabled() && (scrollbar->orientation() == HorizontalScrollbar ?
             scrollbar->width() :
             scrollbar->height()) >= minLengthForThumb;
}

static IntRect buttonRepaintRect(const IntRect& buttonRect, ScrollbarOrientation orientation, ScrollbarControlSize controlSize, bool start)
{
    ASSERT(gButtonPlacement != ScrollbarButtonsNone);

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
            result = IntRect(scrollbar->x(), scrollbar->y(), cOuterButtonLength[scrollbar->controlSize()] + (painting ? cOuterButtonOverlap : 0), thickness);
        else
            result = IntRect(scrollbar->x(), scrollbar->y(), thickness, cOuterButtonLength[scrollbar->controlSize()] + (painting ? cOuterButtonOverlap : 0));
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
    if (isScrollbarOverlayAPIAvailable())
        return wkScrollbarMinimumThumbLength(scrollbarMap()->get(scrollbar).get());
    else
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

bool ScrollbarThemeChromiumMac::shouldDragDocumentInsteadOfThumb(Scrollbar*, const PlatformMouseEvent& event)
{
    return event.altKey();
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

static PlatformSupport::ThemePaintState scrollbarStateToThemeState(Scrollbar* scrollbar)
{
    if (!scrollbar->enabled())
        return PlatformSupport::StateDisabled;
    if (!scrollbar->scrollableArea()->isActive())
        return PlatformSupport::StateInactive;
    if (scrollbar->pressedPart() == ThumbPart)
        return PlatformSupport::StatePressed;

    return PlatformSupport::StateActive;
}

void ScrollbarThemeChromiumMac::updateEnabledState(Scrollbar* scrollbar)
{
    if (isScrollbarOverlayAPIAvailable()) {
        wkScrollbarPainterSetEnabled(scrollbarMap()->get(scrollbar).get(), scrollbar->enabled());
    }
}

bool ScrollbarThemeChromiumMac::paint(Scrollbar* scrollbar, GraphicsContext* context, const IntRect& damageRect)
{
    // Get the tickmarks for the frameview.
    Vector<IntRect> tickmarks;
    scrollbar->scrollableArea()->getTickmarks(tickmarks);

    if (isScrollbarOverlayAPIAvailable()) {
        float value = 0;
        float overhang = 0;

        if (scrollbar->currentPos() < 0) {
            // Scrolled past the top.
            value = 0;
            overhang = -scrollbar->currentPos();
        } else if (scrollbar->visibleSize() + scrollbar->currentPos() > scrollbar->totalSize()) {
            // Scrolled past the bottom.
            value = 1;
            overhang = scrollbar->currentPos() + scrollbar->visibleSize() - scrollbar->totalSize();
        } else {
            // Within the bounds of the scrollable area.
            int maximum = scrollbar->maximum();
            if (maximum > 0)
                value = scrollbar->currentPos() / maximum;
            else
                value = 0;
        }

        ScrollAnimatorChromiumMac* scrollAnimator = static_cast<ScrollAnimatorChromiumMac*>(scrollbar->scrollableArea()->scrollAnimator());
#if !USE(SKIA)
        scrollAnimator->setIsDrawingIntoLayer(context->isCALayerContext());
#else
        scrollAnimator->setIsDrawingIntoLayer(false);
#endif

        CGFloat oldKnobAlpha = 0;
        CGFloat oldTrackAlpha = 0;
        bool hasTickmarks = tickmarks.size() > 0 && scrollbar->orientation() == VerticalScrollbar;
        if (hasTickmarks) {
          oldKnobAlpha = wkScrollbarPainterKnobAlpha(painterForScrollbar(scrollbar));
          wkSetScrollbarPainterKnobAlpha(painterForScrollbar(scrollbar), 1.0);
          oldTrackAlpha = wkScrollbarPainterTrackAlpha(painterForScrollbar(scrollbar));
          wkSetScrollbarPainterTrackAlpha(painterForScrollbar(scrollbar), 1.0);
        }

        GraphicsContextStateSaver stateSaver(*context);
        context->clip(damageRect);
        context->translate(scrollbar->frameRect().x(), scrollbar->frameRect().y());
        LocalCurrentGraphicsContext localContext(context);
        WKScrollbarPainterRef scrollbarPainter = scrollbarMap()->get(scrollbar).get();
        wkScrollbarPainterPaintTrack(scrollbarPainter,
                                     scrollbar->enabled(),
                                     value,
                                     (static_cast<CGFloat>(scrollbar->visibleSize()) - overhang) / scrollbar->totalSize(),
                                     scrollbar->frameRect());

        IntRect tickmarkTrackRect(IntPoint(), trackRect(scrollbar, false).size());
        if (tickmarkTrackRect.width() <= 10) {
            // For narrow scrollbars inset by 1 on the left and 3 on the right.
            tickmarkTrackRect.setX(tickmarkTrackRect.x() + 1);
            tickmarkTrackRect.setWidth(tickmarkTrackRect.width() - 4);
        } else {
            // For wide scrollbars inset by 2 on the left and 3 on the right.
            tickmarkTrackRect.setX(tickmarkTrackRect.x() + 2);
            tickmarkTrackRect.setWidth(tickmarkTrackRect.width() - 5);
        }
        paintGivenTickmarks(context, scrollbar, tickmarkTrackRect, tickmarks);

        wkScrollbarPainterPaintKnob(scrollbarPainter);

        scrollAnimator->setIsDrawingIntoLayer(false);

        if (hasTickmarks) {
          wkSetScrollbarPainterKnobAlpha(scrollbarPainter, oldKnobAlpha);
          wkSetScrollbarPainterTrackAlpha(scrollbarPainter, oldTrackAlpha);
        }

        return true;
    }

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
        trackInfo.enableState = scrollbar->scrollableArea()->isActive() ? kThemeTrackActive : kThemeTrackInactive;

    if (!hasButtons(scrollbar))
        trackInfo.enableState = kThemeTrackNothingToScroll;
    trackInfo.trackInfo.scrollbar.pressState = scrollbarPartToHIPressedState(scrollbar->pressedPart());

#if USE(SKIA)
    SkCanvas* canvas = context->platformContext()->canvas();
    CGAffineTransform currentCTM = gfx::SkMatrixToCGAffineTransform(canvas->getTotalMatrix());
#else
    CGAffineTransform currentCTM = CGContextGetCTM(context->platformContext());
#endif

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
#if USE(SKIA)
    gfx::SkiaBitLocker bitLocker(drawingContext->platformContext()->canvas());
    CGContextRef cgContext = bitLocker.cgContext();
#else
    CGContextRef cgContext = drawingContext->platformContext();
#endif
    HIThemeDrawTrack(&trackInfo, 0, cgContext, kHIThemeOrientationNormal);

    IntRect tickmarkTrackRect = trackRect(scrollbar, false);
    if (!canDrawDirectly) {
        tickmarkTrackRect.setX(0);
        tickmarkTrackRect.setY(0);
    }
    // The ends are rounded and the thumb doesn't go there.
    tickmarkTrackRect.inflateY(-tickmarkTrackRect.width());
    // Inset by 2 on the left and 3 on the right.
    tickmarkTrackRect.setX(tickmarkTrackRect.x() + 2);
    tickmarkTrackRect.setWidth(tickmarkTrackRect.width() - 5);
    paintGivenTickmarks(drawingContext, scrollbar, tickmarkTrackRect, tickmarks);

    if (hasThumb(scrollbar)) {
        PlatformSupport::ThemePaintScrollbarInfo scrollbarInfo;
        scrollbarInfo.orientation = scrollbar->orientation() == HorizontalScrollbar ? PlatformSupport::ScrollbarOrientationHorizontal : PlatformSupport::ScrollbarOrientationVertical;
        scrollbarInfo.parent = scrollbar->parent() && scrollbar->parent()->isFrameView() && static_cast<FrameView*>(scrollbar->parent())->isScrollViewScrollbar(scrollbar) ? PlatformSupport::ScrollbarParentScrollView : PlatformSupport::ScrollbarParentRenderLayer;
        scrollbarInfo.maxValue = scrollbar->maximum();
        scrollbarInfo.currentValue = scrollbar->currentPos();
        scrollbarInfo.visibleSize = scrollbar->visibleSize();
        scrollbarInfo.totalSize = scrollbar->totalSize();

        PlatformSupport::paintScrollbarThumb(
            drawingContext,
            scrollbarStateToThemeState(scrollbar),
            scrollbar->controlSize() == RegularScrollbar ? PlatformSupport::SizeRegular : PlatformSupport::SizeSmall,
            scrollbar->frameRect(),
            scrollbarInfo);
    }

    if (!canDrawDirectly)
        context->drawImageBuffer(imageBuffer.get(), ColorSpaceDeviceRGB, scrollbar->frameRect().location());

    return true;
}

void ScrollbarThemeChromiumMac::paintGivenTickmarks(GraphicsContext* context, Scrollbar* scrollbar, const IntRect& rect, const Vector<IntRect>& tickmarks)
{
    if (scrollbar->orientation() != VerticalScrollbar)
        return;

    if (rect.height() <= 0 || rect.width() <= 0)
        return;  // nothing to draw on.

    if (!tickmarks.size())
        return;

    GraphicsContextStateSaver stateSaver(*context);
    context->setShouldAntialias(false);
    context->setStrokeColor(Color(0xCC, 0xAA, 0x00, 0xFF), ColorSpaceDeviceRGB);
    context->setFillColor(Color(0xFF, 0xDD, 0x00, 0xFF), ColorSpaceDeviceRGB);

    for (Vector<IntRect>::const_iterator i = tickmarks.begin(); i != tickmarks.end(); ++i) {
        // Calculate how far down (in %) the tick-mark should appear.
        const float percent = static_cast<float>(i->y()) / scrollbar->totalSize();
        if (percent < 0.0 || percent > 1.0)
            continue;

        // Calculate how far down (in pixels) the tick-mark should appear.
        const int yPos = rect.y() + (rect.height() * percent);

        // Paint.
        FloatRect tickRect(rect.x(), yPos, rect.width(), 2);
        context->fillRect(tickRect);
        context->strokeRect(tickRect, 1);
    }
}

void ScrollbarThemeChromiumMac::paintOverhangAreas(ScrollView* view, GraphicsContext* context, const IntRect& horizontalOverhangRect, const IntRect& verticalOverhangRect, const IntRect& dirtyRect)
{
    // The extent of each shadow in pixels.
    const int kShadowSize = 4;
    // Offset of negative one pixel to make the gradient blend with the toolbar's bottom border.
    const int kToolbarShadowOffset = -1;
    const struct {
        float stop;
        Color color;
    } kShadowColors[] = {
        { 0.000, Color(0, 0, 0, 255) },
        { 0.125, Color(0, 0, 0, 57) },
        { 0.375, Color(0, 0, 0, 41) },
        { 0.625, Color(0, 0, 0, 18) },
        { 0.875, Color(0, 0, 0, 6) },
        { 1.000, Color(0, 0, 0, 0) }
    };
    const unsigned kNumShadowColors = sizeof(kShadowColors)/sizeof(kShadowColors[0]);

    const bool hasHorizontalOverhang = !horizontalOverhangRect.isEmpty();
    const bool hasVerticalOverhang = !verticalOverhangRect.isEmpty();
    // Prefer non-additive shadows, but degrade to additive shadows if there is vertical overhang.
    const bool useAdditiveShadows = hasVerticalOverhang;

    GraphicsContextStateSaver stateSaver(*context);

    context->setFillPattern(m_overhangPattern);
    if (hasHorizontalOverhang)
        context->fillRect(intersection(horizontalOverhangRect, dirtyRect));
    if (hasVerticalOverhang)
        context->fillRect(intersection(verticalOverhangRect, dirtyRect));

    IntSize scrollOffset = view->scrollOffset();
    FloatPoint shadowCornerOrigin;
    FloatPoint shadowCornerOffset;

    // Draw the shadow for the horizontal overhang.
    if (hasHorizontalOverhang) {
        int toolbarShadowHeight = kShadowSize;
        RefPtr<Gradient> gradient;
        IntRect shadowRect = horizontalOverhangRect;
        shadowRect.setHeight(kShadowSize);
        if (scrollOffset.height() < 0) {
            if (useAdditiveShadows) {
                toolbarShadowHeight = std::min(horizontalOverhangRect.height(), kShadowSize);
            } else if (horizontalOverhangRect.height() < 2 * kShadowSize + kToolbarShadowOffset) {
                // Split the overhang area between the web content shadow and toolbar shadow if it's too small.
                shadowRect.setHeight((horizontalOverhangRect.height() + 1) / 2);
                toolbarShadowHeight = horizontalOverhangRect.height() - shadowRect.height() - kToolbarShadowOffset;
            }
            shadowRect.setY(horizontalOverhangRect.maxY() - shadowRect.height());
            gradient = Gradient::create(FloatPoint(0, shadowRect.maxY()), FloatPoint(0, shadowRect.maxY() - kShadowSize));
            shadowCornerOrigin.setY(shadowRect.maxY());
            shadowCornerOffset.setY(-kShadowSize);
        } else {
            gradient = Gradient::create(FloatPoint(0, shadowRect.y()), FloatPoint(0, shadowRect.maxY()));
            shadowCornerOrigin.setY(shadowRect.y());
        }
        if (hasVerticalOverhang) {
            shadowRect.setWidth(shadowRect.width() - verticalOverhangRect.width());
            if (scrollOffset.width() < 0) {
                shadowRect.setX(shadowRect.x() + verticalOverhangRect.width());
                shadowCornerOrigin.setX(shadowRect.x());
                shadowCornerOffset.setX(-kShadowSize);
            } else {
                shadowCornerOrigin.setX(shadowRect.maxX());
            }
        }
        for (unsigned i = 0; i < kNumShadowColors; i++)
            gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
        context->setFillGradient(gradient);
        context->fillRect(intersection(shadowRect, dirtyRect));

        // Draw a drop-shadow from the toolbar.
        if (scrollOffset.height() < 0) {
            shadowRect.setY(kToolbarShadowOffset);
            shadowRect.setHeight(toolbarShadowHeight);
            gradient = Gradient::create(FloatPoint(0, shadowRect.y()), FloatPoint(0, shadowRect.y() + kShadowSize));
            for (unsigned i = 0; i < kNumShadowColors; i++)
                gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
            context->setFillGradient(gradient);
            context->fillRect(intersection(shadowRect, dirtyRect));
        }
    }

    // Draw the shadow for the vertical overhang.
    if (hasVerticalOverhang) {
        RefPtr<Gradient> gradient;
        IntRect shadowRect = verticalOverhangRect;
        shadowRect.setWidth(kShadowSize);
        if (scrollOffset.width() < 0) {
            shadowRect.setX(verticalOverhangRect.maxX() - shadowRect.width());
            gradient = Gradient::create(FloatPoint(shadowRect.maxX(), 0), FloatPoint(shadowRect.x(), 0));
        } else {
            gradient = Gradient::create(FloatPoint(shadowRect.x(), 0), FloatPoint(shadowRect.maxX(), 0));
        }
        for (unsigned i = 0; i < kNumShadowColors; i++)
            gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
        context->setFillGradient(gradient);
        context->fillRect(intersection(shadowRect, dirtyRect));

        // Draw a drop-shadow from the toolbar.
        shadowRect = verticalOverhangRect;
        shadowRect.setY(kToolbarShadowOffset);
        shadowRect.setHeight(kShadowSize);
        gradient = Gradient::create(FloatPoint(0, shadowRect.y()), FloatPoint(0, shadowRect.maxY()));
        for (unsigned i = 0; i < kNumShadowColors; i++)
            gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
        context->setFillGradient(gradient);
        context->fillRect(intersection(shadowRect, dirtyRect));
    }

    // If both rectangles present, draw a radial gradient for the corner.
    if (hasHorizontalOverhang && hasVerticalOverhang) {
        RefPtr<Gradient> gradient = Gradient::create(shadowCornerOrigin, 0, shadowCornerOrigin, kShadowSize);
        for (unsigned i = 0; i < kNumShadowColors; i++)
            gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
        context->setFillGradient(gradient);
        context->fillRect(FloatRect(shadowCornerOrigin.x() + shadowCornerOffset.x(), shadowCornerOrigin.y() + shadowCornerOffset.y(), kShadowSize, kShadowSize));
    }
}


}
