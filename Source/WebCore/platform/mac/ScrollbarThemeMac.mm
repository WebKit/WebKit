/*
 * Copyright (C) 2008, 2011 Apple Inc. All Rights Reserved.
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

#include "BlockExceptions.h"
#include "ColorMac.h"
#include "ImageBuffer.h"
#include "GraphicsLayer.h"
#include "LocalCurrentGraphicsContext.h"
#include "NSScrollerImpDetails.h"
#include "PlatformMouseEvent.h"
#include "ScrollAnimatorMac.h"
#include "ScrollView.h"
#include "WebCoreSystemInterface.h"
#include <Carbon/Carbon.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TemporaryChange.h>

// FIXME: There are repainting problems due to Aqua scroll bar buttons' visual overflow.

using namespace WebCore;

@interface NSColor (WebNSColorDetails)
+ (NSImage *)_linenPatternImage;
@end

namespace WebCore {

typedef HashMap<ScrollbarThemeClient*, RetainPtr<ScrollbarPainter>> ScrollbarPainterMap;

static ScrollbarPainterMap* scrollbarMap()
{
    static ScrollbarPainterMap* map = new ScrollbarPainterMap;
    return map;
}

}

@interface WebScrollbarPrefsObserver : NSObject
{
}

+ (void)registerAsObserver;
+ (void)appearancePrefsChanged:(NSNotification*)theNotification;
+ (void)behaviorPrefsChanged:(NSNotification*)theNotification;

@end

@implementation WebScrollbarPrefsObserver

+ (void)appearancePrefsChanged:(NSNotification*)unusedNotification
{
    UNUSED_PARAM(unusedNotification);

    ScrollbarTheme* theme = ScrollbarTheme::theme();
    if (theme->isMockTheme())
        return;

    static_cast<ScrollbarThemeMac*>(ScrollbarTheme::theme())->preferencesChanged();
    if (scrollbarMap()->isEmpty())
        return;
    ScrollbarPainterMap::iterator end = scrollbarMap()->end();
    for (ScrollbarPainterMap::iterator it = scrollbarMap()->begin(); it != end; ++it) {
        it->key->styleChanged();
        it->key->invalidate();
    }
}

+ (void)behaviorPrefsChanged:(NSNotification*)unusedNotification
{
    UNUSED_PARAM(unusedNotification);

    ScrollbarTheme* theme = ScrollbarTheme::theme();
    if (theme->isMockTheme())
        return;

    static_cast<ScrollbarThemeMac*>(ScrollbarTheme::theme())->preferencesChanged();
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
    DEPRECATED_DEFINE_STATIC_LOCAL(ScrollbarThemeMac, theme, ());
    return &theme;
}

// FIXME: Get these numbers from CoreUI.
static int cRealButtonLength[] = { 28, 21 };
static int cButtonHitInset[] = { 3, 2 };
// cRealButtonLength - cButtonInset
static int cButtonLength[] = { 14, 10 };

static int cOuterButtonLength[] = { 16, 14 }; // The outer button in a double button pair is a bit bigger.
static int cOuterButtonOverlap = 2;

static float gInitialButtonDelay = 0.5f;
static float gAutoscrollButtonDelay = 0.05f;
static bool gJumpOnTrackClick = false;
static bool gUsesOverlayScrollbars = false;

static ScrollbarButtonsPlacement gButtonPlacement = ScrollbarButtonsDoubleEnd;

static bool supportsExpandedScrollbars()
{
    // FIXME: This is temporary until all platforms that support ScrollbarPainter support this part of the API.
    static bool globalSupportsExpandedScrollbars = [NSClassFromString(@"NSScrollerImp") instancesRespondToSelector:@selector(setExpanded:)];
    return globalSupportsExpandedScrollbars;
}

static void updateArrowPlacement()
{
    NSString *buttonPlacement = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleScrollBarVariant"];
    if ([buttonPlacement isEqualToString:@"Single"])
        gButtonPlacement = ScrollbarButtonsSingle;
    else if ([buttonPlacement isEqualToString:@"DoubleMin"])
        gButtonPlacement = ScrollbarButtonsDoubleStart;
    else if ([buttonPlacement isEqualToString:@"DoubleBoth"])
        gButtonPlacement = ScrollbarButtonsDoubleBoth;
    else {

        gButtonPlacement = ScrollbarButtonsDoubleEnd;
    }
}

static NSControlSize scrollbarControlSizeToNSControlSize(ScrollbarControlSize controlSize)
{
    switch (controlSize) {
    case RegularScrollbar:
        return NSRegularControlSize;
    case SmallScrollbar:
        return NSSmallControlSize;
    }

    ASSERT_NOT_REACHED();
    return NSRegularControlSize;
}

void ScrollbarThemeMac::registerScrollbar(ScrollbarThemeClient* scrollbar)
{
    bool isHorizontal = scrollbar->orientation() == HorizontalScrollbar;
    ScrollbarPainter scrollbarPainter = [NSClassFromString(@"NSScrollerImp") scrollerImpWithStyle:recommendedScrollerStyle() controlSize:scrollbarControlSizeToNSControlSize(scrollbar->controlSize()) horizontal:isHorizontal replacingScrollerImp:nil];
    scrollbarMap()->add(scrollbar, scrollbarPainter);
    updateEnabledState(scrollbar);
    updateScrollbarOverlayStyle(scrollbar);
}

void ScrollbarThemeMac::unregisterScrollbar(ScrollbarThemeClient* scrollbar)
{
    scrollbarMap()->remove(scrollbar);
}

void ScrollbarThemeMac::setNewPainterForScrollbar(ScrollbarThemeClient* scrollbar, ScrollbarPainter newPainter)
{
    scrollbarMap()->set(scrollbar, newPainter);
    updateEnabledState(scrollbar);
    updateScrollbarOverlayStyle(scrollbar);
}

ScrollbarPainter ScrollbarThemeMac::painterForScrollbar(ScrollbarThemeClient* scrollbar)
{
    return scrollbarMap()->get(scrollbar).get();
}

static bool g_isCurrentlyDrawingIntoLayer;
    
bool ScrollbarThemeMac::isCurrentlyDrawingIntoLayer()
{
    return g_isCurrentlyDrawingIntoLayer;
}

void ScrollbarThemeMac::setIsCurrentlyDrawingIntoLayer(bool b)
{
    g_isCurrentlyDrawingIntoLayer = b;
}

ScrollbarThemeMac::ScrollbarThemeMac()
{
    static bool initialized;
    if (!initialized) {
        initialized = true;
        gButtonPlacement = ScrollbarButtonsNone;
        [WebScrollbarPrefsObserver registerAsObserver];
        preferencesChanged();
    }
}

ScrollbarThemeMac::~ScrollbarThemeMac()
{
}

void ScrollbarThemeMac::preferencesChanged()
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults synchronize];
    updateArrowPlacement();
    gInitialButtonDelay = [defaults floatForKey:@"NSScrollerButtonDelay"];
    gAutoscrollButtonDelay = [defaults floatForKey:@"NSScrollerButtonPeriod"];
    gJumpOnTrackClick = [defaults boolForKey:@"AppleScrollerPagingBehavior"];
    usesOverlayScrollbarsChanged();
}

int ScrollbarThemeMac::scrollbarThickness(ScrollbarControlSize controlSize)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    ScrollbarPainter scrollbarPainter = [NSClassFromString(@"NSScrollerImp") scrollerImpWithStyle:recommendedScrollerStyle() controlSize:scrollbarControlSizeToNSControlSize(controlSize) horizontal:NO replacingScrollerImp:nil];
    if (supportsExpandedScrollbars())
        [scrollbarPainter setExpanded:YES];
    return [scrollbarPainter trackBoxWidth];
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool ScrollbarThemeMac::usesOverlayScrollbars() const
{
    return gUsesOverlayScrollbars;
}

void ScrollbarThemeMac::usesOverlayScrollbarsChanged()
{
    gUsesOverlayScrollbars = recommendedScrollerStyle() == NSScrollerStyleOverlay;
}

void ScrollbarThemeMac::updateScrollbarOverlayStyle(ScrollbarThemeClient* scrollbar)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    ScrollbarPainter painter = painterForScrollbar(scrollbar);
    switch (scrollbar->scrollbarOverlayStyle()) {
    case ScrollbarOverlayStyleDefault:
        [painter setKnobStyle:NSScrollerKnobStyleDefault];
        break;
    case ScrollbarOverlayStyleDark:
        [painter setKnobStyle:NSScrollerKnobStyleDark];
        break;
    case ScrollbarOverlayStyleLight:
        [painter setKnobStyle:NSScrollerKnobStyleLight];
        break;
    }
    END_BLOCK_OBJC_EXCEPTIONS;
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

bool ScrollbarThemeMac::hasButtons(ScrollbarThemeClient* scrollbar)
{
    return scrollbar->enabled() && buttonsPlacement() != ScrollbarButtonsNone
             && (scrollbar->orientation() == HorizontalScrollbar
             ? scrollbar->width()
             : scrollbar->height()) >= 2 * (cRealButtonLength[scrollbar->controlSize()] - cButtonHitInset[scrollbar->controlSize()]);
}

bool ScrollbarThemeMac::hasThumb(ScrollbarThemeClient* scrollbar)
{
    int minLengthForThumb;

    ScrollbarPainter painter = scrollbarMap()->get(scrollbar).get();
    minLengthForThumb = [painter knobMinLength] + [painter trackOverlapEndInset] + [painter knobOverlapEndInset]
        + 2 * ([painter trackEndInset] + [painter knobEndInset]);

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

IntRect ScrollbarThemeMac::backButtonRect(ScrollbarThemeClient* scrollbar, ScrollbarPart part, bool painting)
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

IntRect ScrollbarThemeMac::forwardButtonRect(ScrollbarThemeClient* scrollbar, ScrollbarPart part, bool painting)
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

IntRect ScrollbarThemeMac::trackRect(ScrollbarThemeClient* scrollbar, bool painting)
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

int ScrollbarThemeMac::minimumThumbLength(ScrollbarThemeClient* scrollbar)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [scrollbarMap()->get(scrollbar) knobMinLength];
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool ScrollbarThemeMac::shouldCenterOnThumb(ScrollbarThemeClient*, const PlatformMouseEvent& evt)
{
    if (evt.button() != LeftButton)
        return false;
    if (gJumpOnTrackClick)
        return !evt.altKey();
    return evt.altKey();
}

bool ScrollbarThemeMac::shouldDragDocumentInsteadOfThumb(ScrollbarThemeClient*, const PlatformMouseEvent& event)
{
    return event.altKey();
}

int ScrollbarThemeMac::scrollbarPartToHIPressedState(ScrollbarPart part)
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

void ScrollbarThemeMac::updateEnabledState(ScrollbarThemeClient* scrollbar)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [scrollbarMap()->get(scrollbar) setEnabled:scrollbar->enabled()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollbarThemeMac::setPaintCharacteristicsForScrollbar(ScrollbarThemeClient* scrollbar)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    ScrollbarPainter painter = painterForScrollbar(scrollbar);

    float value;
    float overhang;
    ScrollableArea::computeScrollbarValueAndOverhang(scrollbar->currentPos(), scrollbar->totalSize(), scrollbar->visibleSize(), value, overhang);
    float proportion = scrollbar->totalSize() > 0 ? (static_cast<CGFloat>(scrollbar->visibleSize()) - overhang) / scrollbar->totalSize() : 1;

    [painter setEnabled:scrollbar->enabled()];
    [painter setBoundsSize:scrollbar->frameRect().size()];
    [painter setDoubleValue:value];
    [painter setKnobProportion:proportion];
    END_BLOCK_OBJC_EXCEPTIONS;
}

static void scrollbarPainterPaint(ScrollbarPainter scrollbarPainter, bool enabled)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    // Use rectForPart: here; it will take the expansion transition progress into account.
    NSRect trackRect = [scrollbarPainter rectForPart:NSScrollerKnobSlot];
    [scrollbarPainter drawKnobSlotInRect:trackRect highlight:NO];

    // If the scrollbar is not enabled, then there is nothing to scroll to, and we shouldn't
    // call drawKnob.
    if (enabled)
        [scrollbarPainter drawKnob];
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool ScrollbarThemeMac::paint(ScrollbarThemeClient* scrollbar, GraphicsContext* context, const IntRect& damageRect)
{
    setPaintCharacteristicsForScrollbar(scrollbar);

    if (!scrollbar->supportsUpdateOnSecondaryThread()) {
        TemporaryChange<bool> isCurrentlyDrawingIntoLayer(g_isCurrentlyDrawingIntoLayer, context->isCALayerContext());
    
        GraphicsContextStateSaver stateSaver(*context);
        context->clip(damageRect);
        context->translate(scrollbar->frameRect().x(), scrollbar->frameRect().y());
        LocalCurrentGraphicsContext localContext(context);
        scrollbarPainterPaint(scrollbarMap()->get(scrollbar).get(), scrollbar->enabled());
    }

    return true;
}

#if ENABLE(RUBBER_BANDING)
static RetainPtr<CGColorRef> linenBackgroundColor()
{
    NSImage *image = [NSColor _linenPatternImage];
    CGImageRef cgImage = [image CGImageForProposedRect:NULL context:NULL hints:nil];

    RetainPtr<CGPatternRef> pattern = adoptCF(wkCGPatternCreateWithImageAndTransform(cgImage, CGAffineTransformIdentity, wkPatternTilingNoDistortion));
    RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreatePattern(0));

    const CGFloat alpha = 1.0;
    return adoptCF(CGColorCreateWithPattern(colorSpace.get(), pattern.get(), &alpha));
}

void ScrollbarThemeMac::setUpOverhangAreaBackground(CALayer *layer, const Color& customBackgroundColor)
{
    static CGColorRef cachedLinenBackgroundColor = linenBackgroundColor().leakRef();
    // We operate on the CALayer directly here, since GraphicsLayer doesn't have the concept
    // of pattern images, and we know that WebCore won't touch this layer.
    layer.backgroundColor = customBackgroundColor.isValid() ? cachedCGColor(customBackgroundColor, ColorSpaceDeviceRGB) : cachedLinenBackgroundColor;
}

void ScrollbarThemeMac::removeOverhangAreaBackground(CALayer *layer)
{
    layer.backgroundColor = nil;
}

void ScrollbarThemeMac::setUpOverhangAreaShadow(CALayer *layer)
{
    static const CGFloat shadowOpacity = 0.66;
    static const CGFloat shadowRadius = 3;

    // We only need to set these shadow properties once.
    if (!layer.shadowOpacity) {
        layer.shadowColor = CGColorGetConstantColor(kCGColorBlack);
        layer.shadowOffset = CGSizeZero;
        layer.shadowOpacity = shadowOpacity;
        layer.shadowRadius = shadowRadius;
    }

    RetainPtr<CGPathRef> shadowPath = adoptCF(CGPathCreateWithRect(layer.bounds, NULL));
    layer.shadowPath = shadowPath.get();
}

void ScrollbarThemeMac::removeOverhangAreaShadow(CALayer *layer)
{
    layer.shadowPath = nil;
    layer.shadowOpacity = 0;
}

void ScrollbarThemeMac::setUpOverhangAreasLayerContents(GraphicsLayer* graphicsLayer, const Color& customBackgroundColor)
{
    ScrollbarThemeMac::setUpOverhangAreaBackground(graphicsLayer->platformLayer(), customBackgroundColor);
}

void ScrollbarThemeMac::setUpContentShadowLayer(GraphicsLayer* graphicsLayer)
{
    // We operate on the CALayer directly here, since GraphicsLayer doesn't have the concept
    // of shadows, and we know that WebCore won't touch this layer.
    setUpOverhangAreaShadow(graphicsLayer->platformLayer());
}

#endif

} // namespace WebCore
