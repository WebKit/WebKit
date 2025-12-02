/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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
#import "LocalDefaultSystemAppearance.h"
#import "NSScrollerImpDetails.h"
#import "PlatformMouseEvent.h"
#import "ScrollTypesMac.h"
#import "ScrollView.h"
#import "ScrollbarInlines.h"
#import "ScrollbarMac.h"
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

using ScrollbarSet = HashSet<SingleThreadWeakRef<Scrollbar>>;

static ScrollbarSet& scrollbarMap()
{
    static NeverDestroyed<ScrollbarSet> instances;
    return instances;
}

} // namespace WebCore

using WebCore::ScrollbarTheme;
using WebCore::ScrollbarThemeMac;
using WebCore::scrollbarMap;
using WebCore::ScrollbarSet;

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

    downcast<ScrollbarThemeMac>(theme).preferencesChanged();

    for (auto& scrollbar : scrollbarMap()) {
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

    downcast<ScrollbarThemeMac>(theme).preferencesChanged();
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
static constexpr std::array cRealButtonLength { 28, 21 };
static constexpr std::array cButtonHitInset { 3, 2 };
// cRealButtonLength - cButtonInset
static constexpr std::array cButtonLength { 14, 10 };

static constexpr std::array cOuterButtonLength { 16, 14 }; // The outer button in a double button pair is a bit bigger.
static constexpr int cOuterButtonOverlap = 2;

static bool gJumpOnTrackClick = false;
static bool gUsesOverlayScrollbars = false;

static ScrollbarButtonsPlacement gButtonPlacement = ScrollbarButtonsDoubleEnd;

void ScrollbarThemeMac::didCreateScrollerImp(Scrollbar& scrollbar)
{
#if PLATFORM(MAC)
    RetainPtr scrollerImp = scrollerImpForScrollbar(scrollbar);
    ASSERT(scrollerImp);
    scrollerImp.get().userInterfaceLayoutDirection = scrollbar.checkedScrollableArea()->shouldPlaceVerticalScrollbarOnLeft() ? NSUserInterfaceLayoutDirectionRightToLeft : NSUserInterfaceLayoutDirectionLeftToRight;
#else
    UNUSED_PARAM(scrollbar);
#endif
}

void ScrollbarThemeMac::registerScrollbar(Scrollbar& scrollbar)
{
    if (scrollbar.isCustomScrollbar() || !scrollbar.shouldRegisterScrollbar())
        return;

    scrollbarMap().add(scrollbar);
}

void ScrollbarThemeMac::unregisterScrollbar(Scrollbar& scrollbar)
{
    scrollbarMap().take(scrollbar);
}

NSScrollerImp *ScrollbarThemeMac::scrollerImpForScrollbar(Scrollbar& scrollbar)
{
    if (auto* macScrollbar = dynamicDowncast<ScrollbarMac>(scrollbar))
        return macScrollbar->scrollerImp();
    return nil;
}

RetainPtr<NSScrollerImp> ScrollbarThemeMac::protectedScrollerImpForScrollbar(Scrollbar& scrollbar)
{
    return scrollerImpForScrollbar(scrollbar);
}

bool ScrollbarThemeMac::isLayoutDirectionRTL(Scrollbar& scrollbar)
{
#if PLATFORM(MAC)
    RetainPtr scrollerImp = scrollerImpForScrollbar(scrollbar);
    if (!scrollerImp) {
        if (!scrollbar.shouldRegisterScrollbar())
            return scrollbar.checkedScrollableArea()->shouldPlaceVerticalScrollbarOnLeft() ? NSUserInterfaceLayoutDirectionRightToLeft : NSUserInterfaceLayoutDirectionLeftToRight;
        return false;
    }
    return scrollerImp.get().userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionRightToLeft;
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

int ScrollbarThemeMac::scrollbarThickness(ScrollbarWidth scrollbarWidth, OverlayScrollbarSizeRelevancy overlayRelevancy)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (scrollbarWidth == ScrollbarWidth::None || (usesOverlayScrollbars() && overlayRelevancy == OverlayScrollbarSizeRelevancy::IgnoreOverlayScrollbarSize))
        return 0;

    return [[NSScrollerImp self] scrollerWidthForControlSize:nsControlSizeFromScrollbarWidth(scrollbarWidth) scrollerStyle:ScrollerStyle::recommendedScrollerStyle()];
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
    RetainPtr painter = scrollerImpForScrollbar(scrollbar);
    switch (scrollbar.scrollableArea().scrollbarOverlayStyle()) {
    case ScrollbarOverlayStyle::Default:
        [painter setKnobStyle:NSScrollerKnobStyleDefault];
        break;
    case ScrollbarOverlayStyle::Dark:
        [painter setKnobStyle:NSScrollerKnobStyleDark];
        break;
    case ScrollbarOverlayStyle::Light:
        [painter setKnobStyle:NSScrollerKnobStyleLight];
        break;
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

ScrollbarButtonsPlacement ScrollbarThemeMac::buttonsPlacement() const
{
    return gButtonPlacement;
}

inline constexpr unsigned scrollbarWidthToIndex(ScrollbarWidth scrollbarWidth)
{
    switch (scrollbarWidth) {
    case ScrollbarWidth::Auto:
    case ScrollbarWidth::None:
        return 0;
    case ScrollbarWidth::Thin: return 1;
    }
    return 0;
}

bool ScrollbarThemeMac::hasButtons(Scrollbar& scrollbar)
{
    if (scrollbar.enabled() && buttonsPlacement() != ScrollbarButtonsNone && (scrollbar.orientation() == ScrollbarOrientation::Horizontal))
        return scrollbar.width();
    return scrollbar.height() >= 2 * (cRealButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())] - cButtonHitInset[scrollbarWidthToIndex(scrollbar.widthStyle())]);
}

bool ScrollbarThemeMac::hasThumb(Scrollbar& scrollbar)
{
    int minLengthForThumb;

    RetainPtr painter = scrollerImpForScrollbar(scrollbar);
    minLengthForThumb = [painter knobMinLength] + [painter trackOverlapEndInset] + [painter knobOverlapEndInset]
        + 2 * ([painter trackEndInset] + [painter knobEndInset]);

    return scrollbar.enabled() && (scrollbar.orientation() == ScrollbarOrientation::Horizontal ?
             scrollbar.width() :
             scrollbar.height()) >= minLengthForThumb;
}

static IntRect buttonRepaintRect(const IntRect& buttonRect, ScrollbarOrientation orientation, ScrollbarWidth widthStyle, bool start)
{
    ASSERT(gButtonPlacement != ScrollbarButtonsNone);

    IntRect paintRect(buttonRect);
    if (orientation == ScrollbarOrientation::Horizontal) {
        paintRect.setWidth(cRealButtonLength[scrollbarWidthToIndex(widthStyle)]);
        if (!start)
            paintRect.setX(buttonRect.x() - (cRealButtonLength[scrollbarWidthToIndex(widthStyle)] - buttonRect.width()));
    } else {
        paintRect.setHeight(cRealButtonLength[scrollbarWidthToIndex(widthStyle)]);
        if (!start)
            paintRect.setY(buttonRect.y() - (cRealButtonLength[scrollbarWidthToIndex(widthStyle)] - buttonRect.height()));
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
        
    int thickness = scrollbarThickness(scrollbar.widthStyle());
    bool outerButton = part == BackButtonStartPart && (buttonsPlacement() == ScrollbarButtonsDoubleStart || buttonsPlacement() == ScrollbarButtonsDoubleBoth);
    if (outerButton) {
        if (scrollbar.orientation() == ScrollbarOrientation::Horizontal)
            result = IntRect(scrollbar.x(), scrollbar.y(), cOuterButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())] + (painting ? cOuterButtonOverlap : 0), thickness);
        else
            result = IntRect(scrollbar.x(), scrollbar.y(), thickness, cOuterButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())] + (painting ? cOuterButtonOverlap : 0));
        return result;
    }
    
    // Our repaint rect is slightly larger, since we are a button that is adjacent to the track.
    if (scrollbar.orientation() == ScrollbarOrientation::Horizontal) {
        int start = part == BackButtonStartPart ? scrollbar.x() : scrollbar.x() + scrollbar.width() - cOuterButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())] - cButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())];
        result = IntRect(start, scrollbar.y(), cButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())], thickness);
    } else {
        int start = part == BackButtonStartPart ? scrollbar.y() : scrollbar.y() + scrollbar.height() - cOuterButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())] - cButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())];
        result = IntRect(scrollbar.x(), start, thickness, cButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())]);
    }
    
    if (painting)
        return buttonRepaintRect(result, scrollbar.orientation(), scrollbar.widthStyle(), part == BackButtonStartPart);
    return result;
}

IntRect ScrollbarThemeMac::forwardButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool painting)
{
    IntRect result;
    
    if (part == ForwardButtonEndPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleStart))
        return result;
    
    if (part == ForwardButtonStartPart && (buttonsPlacement() == ScrollbarButtonsNone || buttonsPlacement() == ScrollbarButtonsDoubleEnd || buttonsPlacement() == ScrollbarButtonsSingle))
        return result;
        
    int thickness = scrollbarThickness(scrollbar.widthStyle());
    int outerButtonLength = cOuterButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())];
    int buttonLength = cButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())];
    
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
        return buttonRepaintRect(result, scrollbar.orientation(), scrollbar.widthStyle(), part == ForwardButtonStartPart);
    return result;
}

IntRect ScrollbarThemeMac::trackRect(Scrollbar& scrollbar, bool painting)
{
    if (painting || !hasButtons(scrollbar))
        return scrollbar.frameRect();
    
    int thickness = scrollbarThickness(scrollbar.widthStyle());
    int startWidth = 0;
    int endWidth = 0;
    int outerButtonLength = cOuterButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())];
    int buttonLength = cButtonLength[scrollbarWidthToIndex(scrollbar.widthStyle())];
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
    if (scrollbar.shouldRegisterScrollbar()) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        return [protectedScrollerImpForScrollbar(scrollbar) knobMinLength];
        END_BLOCK_OBJC_EXCEPTIONS
    } else
        return scrollbar.minimumThumbLength();
}

static bool shouldCenterOnThumb(const PlatformMouseEvent& evt)
{
    if (evt.button() != MouseButton::Left)
        return false;
    if (gJumpOnTrackClick)
        return !evt.altKey();
    return evt.altKey();
}

ScrollbarButtonPressAction ScrollbarThemeMac::handleMousePressEvent(Scrollbar&, const PlatformMouseEvent& event, ScrollbarPart pressedPart)
{
    if (event.button() == MouseButton::Right)
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
    [protectedScrollerImpForScrollbar(scrollbar) setEnabled:scrollbar.enabled()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollbarThemeMac::setPaintCharacteristicsForScrollbar(Scrollbar& scrollbar)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    RetainPtr painter = scrollerImpForScrollbar(scrollbar);

    float value;
    float overhang;
    ScrollableArea::computeScrollbarValueAndOverhang(scrollbar.currentPos(), scrollbar.totalSize(), scrollbar.visibleSize(), value, overhang);
    float proportion = scrollbar.totalSize() > 0 ? (static_cast<CGFloat>(scrollbar.visibleSize()) - overhang) / scrollbar.totalSize() : 1;

    [painter setEnabled:scrollbar.enabled()];
    [painter setBoundsSize:scrollbar.frameRect().size()];
    [painter setDoubleValue:value];
#if PLATFORM(MAC)
    [painter setPresentationValue:value];
#endif
    [painter setKnobProportion:proportion];
    END_BLOCK_OBJC_EXCEPTIONS
}

static void paintScrollbar(Scrollbar& scrollbar, GraphicsContext& context)
{
    LocalCurrentGraphicsContext localContext { context };

    RetainPtr scrollerImp = ScrollbarThemeMac::scrollerImpForScrollbar(scrollbar);
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    // Use rectForPart: here; it will take the expansion transition progress into account.
    NSRect trackRect = [scrollerImp rectForPart:NSScrollerKnobSlot];
    [scrollerImp drawKnobSlotInRect:trackRect highlight:NO];

    // If the scrollbar is not enabled, then there is nothing to scroll to, and we shouldn't
    // call drawKnob.
    if (scrollbar.enabled())
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

    auto scrollbarRect = scrollbar.frameRect();
    if (context.hasPlatformContext()) {
        context.translate(scrollbarRect.location());
        paintScrollbar(scrollbar, context);
    } else {
        if (auto imageBuffer = [&] -> RefPtr<ImageBuffer> {
            if (auto buffer = context.createImageBuffer(scrollbarRect.size(), scrollbar.deviceScaleFactor(), DestinationColorSpace::SRGB(), context.renderingMode(), RenderingMethod::Local)) {
                paintScrollbar(scrollbar, buffer->context());
                return buffer;
            }
            return nullptr;
        }())
            context.drawImageBuffer(*imageBuffer, scrollbarRect);
    }

    return true;
}

void ScrollbarThemeMac::paintScrollCorner(ScrollableArea& area, GraphicsContext& context, const IntRect& cornerRect)
{
    if (context.paintingDisabled())
        return;

#if ENABLE(FORM_CONTROL_REFRESH)
    // The scrollbar tracks will be pill shaped, so they cannot be visually
    // contiguous with the corner. Paint nothing.
    if (area.formControlRefreshEnabled())
        return;
#endif

    // Keep this in sync with ScrollAnimatorMac's effectiveAppearanceForScrollerImp:.
    LocalDefaultSystemAppearance localAppearance(area.useDarkAppearanceForScrollbars());

    context.drawSystemImage(ScrollbarTrackCornerSystemImageMac::create(), cornerRect);
}

#if HAVE(RUBBER_BANDING)
void ScrollbarThemeMac::setUpOverhangAreaShadow(CALayer *layer)
{
    static const CGFloat shadowOpacity = 0.66;
    static const CGFloat shadowRadius = 3;

    // We only need to set these shadow properties once.
    if (!layer.shadowOpacity) {
        layer.shadowColor = RetainPtr { CGColorGetConstantColor(kCGColorBlack) }.get();
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
#endif

} // namespace WebCore

#endif // PLATFORM(MAC)
