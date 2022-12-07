/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
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

#import "config.h"
#import "ScrollbarThemeMac.h"

#if PLATFORM(MAC)

#import "ColorMac.h"
#import "GraphicsLayer.h"
#import "ImageBuffer.h"
#import "LocalCurrentGraphicsContext.h"
#import "NSScrollerImpDetails.h"
#import "PlatformMouseEvent.h"
#import "ScrollView.h"
#import "ScrollbarTrackCornerSystemImageMac.h"
#import <Carbon/Carbon.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/HashMap.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/SetForScope.h>
#import <wtf/StdLibExtras.h>

// FIXME: There are repainting problems due to Aqua scroll bar buttons' visual overflow.

namespace WebCore {

using ScrollbarToScrollerImpMap = HashMap<Scrollbar*, RetainPtr<NSScrollerImp>>;

static ScrollbarToScrollerImpMap& scrollbarMap()
{
    static NeverDestroyed<ScrollbarToScrollerImpMap> instances;
    return instances;
}

} // namespace WebCore

using WebCore::ScrollbarTheme;
using WebCore::ScrollbarThemeMac;
using WebCore::scrollbarMap;
using WebCore::ScrollbarToScrollerImpMap;

@interface NSColor (WebNSColorDetails)
+ (NSImage *)_linenPatternImage;
@end

@interface WebScrollbarPrefsObserver : NSObject
{
}

+ (void)registerAsObserver;
+ (void)appearancePrefsChanged:(NSNotification *)theNotification;
+ (void)behaviorPrefsChanged:(NSNotification *)theNotification;

@end

@implementation WebScrollbarPrefsObserver

+ (void)appearancePrefsChanged:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);

    ScrollbarTheme& theme = ScrollbarTheme::theme();
    if (theme.isMockTheme())
        return;

    static_cast<ScrollbarThemeMac&>(theme).preferencesChanged();

    for (auto keyValuePair : scrollbarMap()) {
        auto* scrollbar = keyValuePair.key;
        scrollbar->styleChanged();
        scrollbar->invalidate();
    }
}

+ (void)behaviorPrefsChanged:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);

    ScrollbarTheme& theme = ScrollbarTheme::theme();
    if (theme.isMockTheme())
        return;

    static_cast<ScrollbarThemeMac&>(theme).preferencesChanged();
}

+ (void)registerAsObserver
{
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(appearancePrefsChanged:) name:@"AppleAquaScrollBarVariantChanged" object:nil suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(behaviorPrefsChanged:) name:@"AppleNoRedisplayAppearancePreferenceChanged" object:nil suspensionBehavior:NSNotificationSuspensionBehaviorCoalesce];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(behaviorPrefsChanged:) name:NSPreferredScrollerStyleDidChangeNotification object:nil];
}

@end

namespace WebCore {

ScrollbarTheme& ScrollbarTheme::nativeTheme()
{
    static NeverDestroyed<ScrollbarThemeMac> theme;
    return theme;
}

// FIXME: Get these numbers from CoreUI.
static const int cRealButtonLength[] = { 28, 21 };
static const int cButtonHitInset[] = { 3, 2 };
// cRealButtonLength - cButtonInset
static const int cButtonLength[] = { 14, 10 };

static const int cOuterButtonLength[] = { 16, 14 }; // The outer button in a double button pair is a bit bigger.
static const int cOuterButtonOverlap = 2;

static bool gJumpOnTrackClick = false;
static bool gUsesOverlayScrollbars = false;

static ScrollbarButtonsPlacement gButtonPlacement = ScrollbarButtonsDoubleEnd;

static NSControlSize scrollbarControlSizeToNSControlSize(ScrollbarControlSize controlSize)
{
    switch (controlSize) {
    case ScrollbarControlSize::Regular:
        return NSControlSizeRegular;
    case ScrollbarControlSize::Small:
        return NSControlSizeSmall;
    }

    ASSERT_NOT_REACHED();
    return NSControlSizeRegular;
}

void ScrollbarThemeMac::didCreateScrollerImp(Scrollbar& scrollbar)
{
#if PLATFORM(MAC)
    NSScrollerImp *scrollerImp = painterForScrollbar(scrollbar);
    ASSERT(scrollerImp);
    scrollerImp.userInterfaceLayoutDirection = scrollbar.scrollableArea().shouldPlaceVerticalScrollbarOnLeft() ? NSUserInterfaceLayoutDirectionRightToLeft : NSUserInterfaceLayoutDirectionLeftToRight;
#else
    UNUSED_PARAM(scrollbar);
#endif
}

void ScrollbarThemeMac::registerScrollbar(Scrollbar& scrollbar)
{
    if (scrollbar.isCustomScrollbar())
        return;

    bool isHorizontal = scrollbar.orientation() == ScrollbarOrientation::Horizontal;
    auto scrollerImp = retainPtr([NSScrollerImp scrollerImpWithStyle:ScrollerStyle::recommendedScrollerStyle() controlSize:scrollbarControlSizeToNSControlSize(scrollbar.controlSize()) horizontal:isHorizontal replacingScrollerImp:nil]);
    scrollbarMap().add(&scrollbar, WTFMove(scrollerImp));
    didCreateScrollerImp(scrollbar);
    updateEnabledState(scrollbar);
    updateScrollbarOverlayStyle(scrollbar);
}

void ScrollbarThemeMac::unregisterScrollbar(Scrollbar& scrollbar)
{
    [scrollbarMap().take(&scrollbar) setDelegate:nil];
}

void ScrollbarThemeMac::setNewPainterForScrollbar(Scrollbar& scrollbar, RetainPtr<NSScrollerImp>&& newPainter)
{
    scrollbarMap().set(&scrollbar, WTFMove(newPainter));
    updateEnabledState(scrollbar);
    updateScrollbarOverlayStyle(scrollbar);
}

NSScrollerImp *ScrollbarThemeMac::painterForScrollbar(Scrollbar& scrollbar)
{
    return scrollbarMap().get(&scrollbar).get();
}

bool ScrollbarThemeMac::isLayoutDirectionRTL(Scrollbar& scrollbar)
{
#if PLATFORM(MAC)
    NSScrollerImp *scrollerImp = painterForScrollbar(scrollbar);
    if (!scrollerImp)
        return false;
    return scrollerImp.userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionRightToLeft;
#else
    UNUSED_PARAM(scrollbar);
    return false;
#endif
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
    gJumpOnTrackClick = [defaults boolForKey:@"AppleScrollerPagingBehavior"];
    usesOverlayScrollbarsChanged();
}

int ScrollbarThemeMac::scrollbarThickness(ScrollbarControlSize controlSize, ScrollbarExpansionState expansionState)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    NSScrollerImp *scrollerImp = [NSScrollerImp scrollerImpWithStyle:ScrollerStyle::recommendedScrollerStyle() controlSize:scrollbarControlSizeToNSControlSize(controlSize) horizontal:NO replacingScrollerImp:nil];
    [scrollerImp setExpanded:(expansionState == ScrollbarExpansionState::Expanded)];
    return [scrollerImp trackBoxWidth];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool ScrollbarThemeMac::usesOverlayScrollbars() const
{
    return gUsesOverlayScrollbars;
}

void ScrollbarThemeMac::usesOverlayScrollbarsChanged()
{
    gUsesOverlayScrollbars = ScrollerStyle::recommendedScrollerStyle() == NSScrollerStyleOverlay;
}

void ScrollbarThemeMac::updateScrollbarOverlayStyle(Scrollbar& scrollbar)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    NSScrollerImp *painter = painterForScrollbar(scrollbar);
    switch (scrollbar.scrollableArea().scrollbarOverlayStyle()) {
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
    END_BLOCK_OBJC_EXCEPTIONS
}

ScrollbarButtonsPlacement ScrollbarThemeMac::buttonsPlacement() const
{
    return gButtonPlacement;
}

inline constexpr unsigned scrollbarSizeToIndex(ScrollbarControlSize scrollbarSize)
{
    switch (scrollbarSize) {
    case ScrollbarControlSize::Regular: return 0;
    case ScrollbarControlSize::Small: return 1;
    }
    return 0;
}

bool ScrollbarThemeMac::hasButtons(Scrollbar& scrollbar)
{
    if (scrollbar.enabled() && buttonsPlacement() != ScrollbarButtonsNone && (scrollbar.orientation() == ScrollbarOrientation::Horizontal))
        return scrollbar.width();
    return scrollbar.height() >= 2 * (cRealButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())] - cButtonHitInset[scrollbarSizeToIndex(scrollbar.controlSize())]);
}

bool ScrollbarThemeMac::hasThumb(Scrollbar& scrollbar)
{
    int minLengthForThumb;

    NSScrollerImp *painter = scrollbarMap().get(&scrollbar).get();
    minLengthForThumb = [painter knobMinLength] + [painter trackOverlapEndInset] + [painter knobOverlapEndInset]
        + 2 * ([painter trackEndInset] + [painter knobEndInset]);

    return scrollbar.enabled() && (scrollbar.orientation() == ScrollbarOrientation::Horizontal ?
             scrollbar.width() :
             scrollbar.height()) >= minLengthForThumb;
}

static IntRect buttonRepaintRect(const IntRect& buttonRect, ScrollbarOrientation orientation, ScrollbarControlSize controlSize, bool start)
{
    ASSERT(gButtonPlacement != ScrollbarButtonsNone);

    IntRect paintRect(buttonRect);
    if (orientation == ScrollbarOrientation::Horizontal) {
        paintRect.setWidth(cRealButtonLength[scrollbarSizeToIndex(controlSize)]);
        if (!start)
            paintRect.setX(buttonRect.x() - (cRealButtonLength[scrollbarSizeToIndex(controlSize)] - buttonRect.width()));
    } else {
        paintRect.setHeight(cRealButtonLength[scrollbarSizeToIndex(controlSize)]);
        if (!start)
            paintRect.setY(buttonRect.y() - (cRealButtonLength[scrollbarSizeToIndex(controlSize)] - buttonRect.height()));
    }

    return paintRect;
}

IntRect ScrollbarThemeMac::backButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool painting)
{
    IntRect result;
    
    if (part == BackButtonStartPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleEnd))
        return result;
    
    if (part == BackButtonEndPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleStart || buttonsPlacement() == ScrollbarButtonsSingle))
        return result;
        
    int thickness = scrollbarThickness(scrollbar.controlSize());
    bool outerButton = part == BackButtonStartPart && (buttonsPlacement() == ScrollbarButtonsDoubleStart || buttonsPlacement() == ScrollbarButtonsDoubleBoth);
    if (outerButton) {
        if (scrollbar.orientation() == ScrollbarOrientation::Horizontal)
            result = IntRect(scrollbar.x(), scrollbar.y(), cOuterButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())] + (painting ? cOuterButtonOverlap : 0), thickness);
        else
            result = IntRect(scrollbar.x(), scrollbar.y(), thickness, cOuterButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())] + (painting ? cOuterButtonOverlap : 0));
        return result;
    }
    
    // Our repaint rect is slightly larger, since we are a button that is adjacent to the track.
    if (scrollbar.orientation() == ScrollbarOrientation::Horizontal) {
        int start = part == BackButtonStartPart ? scrollbar.x() : scrollbar.x() + scrollbar.width() - cOuterButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())] - cButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())];
        result = IntRect(start, scrollbar.y(), cButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())], thickness);
    } else {
        int start = part == BackButtonStartPart ? scrollbar.y() : scrollbar.y() + scrollbar.height() - cOuterButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())] - cButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())];
        result = IntRect(scrollbar.x(), start, thickness, cButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())]);
    }
    
    if (painting)
        return buttonRepaintRect(result, scrollbar.orientation(), scrollbar.controlSize(), part == BackButtonStartPart);
    return result;
}

IntRect ScrollbarThemeMac::forwardButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool painting)
{
    IntRect result;
    
    if (part == ForwardButtonEndPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleStart))
        return result;
    
    if (part == ForwardButtonStartPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleEnd || buttonsPlacement() == ScrollbarButtonsSingle))
        return result;
        
    int thickness = scrollbarThickness(scrollbar.controlSize());
    int outerButtonLength = cOuterButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())];
    int buttonLength = cButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())];
    
    bool outerButton = part == ForwardButtonEndPart && (buttonsPlacement() == ScrollbarButtonsDoubleEnd || buttonsPlacement() == ScrollbarButtonsDoubleBoth);
    if (outerButton) {
        if (scrollbar.orientation() == ScrollbarOrientation::Horizontal) {
            result = IntRect(scrollbar.x() + scrollbar.width() - outerButtonLength, scrollbar.y(), outerButtonLength, thickness);
            if (painting)
                result.inflateX(cOuterButtonOverlap);
        } else {
            result = IntRect(scrollbar.x(), scrollbar.y() + scrollbar.height() - outerButtonLength, thickness, outerButtonLength);
            if (painting)
                result.inflateY(cOuterButtonOverlap);
        }
        return result;
    }
    
    if (scrollbar.orientation() == ScrollbarOrientation::Horizontal) {
        int start = part == ForwardButtonEndPart ? scrollbar.x() + scrollbar.width() - buttonLength : scrollbar.x() + outerButtonLength;
        result = IntRect(start, scrollbar.y(), buttonLength, thickness);
    } else {
        int start = part == ForwardButtonEndPart ? scrollbar.y() + scrollbar.height() - buttonLength : scrollbar.y() + outerButtonLength;
        result = IntRect(scrollbar.x(), start, thickness, buttonLength);
    }
    if (painting)
        return buttonRepaintRect(result, scrollbar.orientation(), scrollbar.controlSize(), part == ForwardButtonStartPart);
    return result;
}

IntRect ScrollbarThemeMac::trackRect(Scrollbar& scrollbar, bool painting)
{
    if (painting || !hasButtons(scrollbar))
        return scrollbar.frameRect();
    
    IntRect result;
    int thickness = scrollbarThickness(scrollbar.controlSize());
    int startWidth = 0;
    int endWidth = 0;
    int outerButtonLength = cOuterButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())];
    int buttonLength = cButtonLength[scrollbarSizeToIndex(scrollbar.controlSize())];
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
    if (scrollbar.orientation() == ScrollbarOrientation::Horizontal)
        return IntRect(scrollbar.x() + startWidth, scrollbar.y(), scrollbar.width() - totalWidth, thickness);
    return IntRect(scrollbar.x(), scrollbar.y() + startWidth, thickness, scrollbar.height() - totalWidth);
}

int ScrollbarThemeMac::minimumThumbLength(Scrollbar& scrollbar)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return [scrollbarMap().get(&scrollbar) knobMinLength];
    END_BLOCK_OBJC_EXCEPTIONS
}

static bool shouldCenterOnThumb(const PlatformMouseEvent& evt)
{
    if (evt.button() != LeftButton)
        return false;
    if (gJumpOnTrackClick)
        return !evt.altKey();
    return evt.altKey();
}

ScrollbarButtonPressAction ScrollbarThemeMac::handleMousePressEvent(Scrollbar&, const PlatformMouseEvent& event, ScrollbarPart pressedPart)
{
    if (event.button() == RightButton)
        return ScrollbarButtonPressAction::None;

    switch (pressedPart) {
    case BackTrackPart:
    case ForwardTrackPart:
        if (shouldCenterOnThumb(event))
            return ScrollbarButtonPressAction::CenterOnThumb;
        break;
    case ThumbPart:
        return ScrollbarButtonPressAction::StartDrag;
    default:
        break;
    }

    return ScrollbarButtonPressAction::Scroll;
}

bool ScrollbarThemeMac::shouldDragDocumentInsteadOfThumb(Scrollbar&, const PlatformMouseEvent& event)
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

void ScrollbarThemeMac::updateEnabledState(Scrollbar& scrollbar)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [scrollbarMap().get(&scrollbar) setEnabled:scrollbar.enabled()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollbarThemeMac::setPaintCharacteristicsForScrollbar(Scrollbar& scrollbar)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    NSScrollerImp *painter = painterForScrollbar(scrollbar);

    float value;
    float overhang;
    ScrollableArea::computeScrollbarValueAndOverhang(scrollbar.currentPos(), scrollbar.totalSize(), scrollbar.visibleSize(), value, overhang);
    float proportion = scrollbar.totalSize() > 0 ? (static_cast<CGFloat>(scrollbar.visibleSize()) - overhang) / scrollbar.totalSize() : 1;

    [painter setEnabled:scrollbar.enabled()];
    [painter setBoundsSize:scrollbar.frameRect().size()];
    [painter setDoubleValue:value];
#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
    [painter setPresentationValue:value];
#endif
    [painter setKnobProportion:proportion];
    END_BLOCK_OBJC_EXCEPTIONS
}

static void scrollerImpPaint(NSScrollerImp *scrollerImp, bool enabled)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    // Use rectForPart: here; it will take the expansion transition progress into account.
    NSRect trackRect = [scrollerImp rectForPart:NSScrollerKnobSlot];
    [scrollerImp drawKnobSlotInRect:trackRect highlight:NO];

    // If the scrollbar is not enabled, then there is nothing to scroll to, and we shouldn't
    // call drawKnob.
    if (enabled)
        [scrollerImp drawKnob];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool ScrollbarThemeMac::paint(Scrollbar& scrollbar, GraphicsContext& context, const IntRect& damageRect)
{
    if (context.paintingDisabled())
        return false;

    setPaintCharacteristicsForScrollbar(scrollbar);

    if (scrollbar.supportsUpdateOnSecondaryThread())
        return true;

    SetForScope isCurrentlyDrawingIntoLayer(g_isCurrentlyDrawingIntoLayer, context.isCALayerContext());

    GraphicsContextStateSaver stateSaver(context);
    context.clip(damageRect);
    context.translate(scrollbar.frameRect().location());
    LocalCurrentGraphicsContext localContext(context);
    scrollerImpPaint(scrollbarMap().get(&scrollbar).get(), scrollbar.enabled());

    return true;
}

void ScrollbarThemeMac::paintScrollCorner(ScrollableArea&, GraphicsContext& context, const IntRect& cornerRect)
{
    if (context.paintingDisabled())
        return;

    context.drawSystemImage(ScrollbarTrackCornerSystemImageMac::create(), cornerRect);
}

#if HAVE(RUBBER_BANDING)
static RetainPtr<CGColorRef> linenBackgroundColor()
{
    NSImage *image = nil;
    CGImageRef cgImage = nullptr;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    image = [NSColor _linenPatternImage];
    cgImage = [image CGImageForProposedRect:NULL context:NULL hints:nil];
    END_BLOCK_OBJC_EXCEPTIONS
    
    if (!cgImage)
        return nullptr;

    RetainPtr<CGPatternRef> pattern = adoptCF(CGPatternCreateWithImage2(cgImage, CGAffineTransformIdentity, kCGPatternTilingNoDistortion));
    RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreatePattern(0));

    const CGFloat alpha = 1.0;
    return adoptCF(CGColorCreateWithPattern(colorSpace.get(), pattern.get(), &alpha));
}

void ScrollbarThemeMac::setUpOverhangAreaBackground(CALayer *layer, const Color& customBackgroundColor)
{
    static CGColorRef cachedLinenBackgroundColor = linenBackgroundColor().leakRef();
    // We operate on the CALayer directly here, since GraphicsLayer doesn't have the concept
    // of pattern images, and we know that WebCore won't touch this layer.
    layer.backgroundColor = customBackgroundColor.isValid() ? cachedCGColor(customBackgroundColor).get() : cachedLinenBackgroundColor;
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

#endif // PLATFORM(MAC)
