/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ScrollerPairMac.h"

#if PLATFORM(MAC)

#import "Logging.h"
#import "ScrollTypesMac.h"
#import "ScrollingTreeFrameScrollingNode.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/FloatPoint.h>
#import <WebCore/IntRect.h>
#import <WebCore/NSScrollerImpDetails.h>
#import <WebCore/PlatformWheelEvent.h>
#import <WebCore/ScrollTypes.h>
#import <WebCore/ScrollableArea.h>
#import <WebCore/ScrollingTreeScrollingNode.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <wtf/TZoneMallocInlines.h>

@interface WebScrollerImpPairDelegateMac : NSObject <NSScrollerImpPairDelegate> {
    ThreadSafeWeakPtr<WebCore::ScrollerPairMac> _scrollerPair;
}
- (id)initWithScrollerPair:(WebCore::ScrollerPairMac*)scrollerPair;
@end

@implementation WebScrollerImpPairDelegateMac

- (id)initWithScrollerPair:(WebCore::ScrollerPairMac*)scrollerPair
{
    self = [super init];
    if (!self)
        return nil;

    _scrollerPair = scrollerPair;
    return self;
}

- (void)invalidate
{
    _scrollerPair = nullptr;
}

- (NSRect)contentAreaRectForScrollerImpPair:(NSScrollerImpPair *)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    RefPtr scrollerPair = _scrollerPair.get();
    if (!scrollerPair)
        return NSZeroRect;

    auto size = scrollerPair->visibleSize();
    return NSMakeRect(0, 0, size.width(), size.height());
}

- (BOOL)inLiveResizeForScrollerImpPair:(NSScrollerImpPair *)scrollerImpPair
{
    RefPtr scrollerPair = _scrollerPair.get();
    if (!scrollerPair)
        return NO;

    return scrollerPair->inLiveResize();
}

- (NSPoint)mouseLocationInContentAreaForScrollerImpPair:(NSScrollerImpPair *)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    // This location is only used when calling mouseLocationInScrollerForScrollerImp,
    // where we will use the converted mouse position from the Web Process
    return NSZeroPoint;
}

- (NSPoint)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair convertContentPoint:(NSPoint)pointInContentArea toScrollerImp:(NSScrollerImp *)scrollerImp
{
    UNUSED_PARAM(scrollerImpPair);
    UNUSED_PARAM(pointInContentArea);

    RefPtr scrollerPair = _scrollerPair.get();
    if (!scrollerPair)
        return NSZeroPoint;

    if (!scrollerPair || !scrollerImp)
        return NSZeroPoint;

    CheckedPtr<WebCore::ScrollerMac> scroller;
    if ([scrollerImp isHorizontal])
        scroller = &scrollerPair->horizontalScroller();
    else
        scroller = &scrollerPair->verticalScroller();

    ASSERT(scroller->isScrollerFor(scrollerImp));

    return scroller->lastKnownMousePositionInScrollbar();
}

- (void)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair setContentAreaNeedsDisplayInRect:(NSRect)rect
{
    UNUSED_PARAM(scrollerImpPair);
    UNUSED_PARAM(rect);
}

- (void)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair updateScrollerStyleForNewRecommendedScrollerStyle:(NSScrollerStyle)newRecommendedScrollerStyle
{
    UNUSED_PARAM(scrollerImpPair);

    RefPtr scrollerPair = _scrollerPair.get();
    if (scrollerPair)
        scrollerPair->setScrollbarStyle(WebCore::scrollbarStyle(newRecommendedScrollerStyle));
}

@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollerPairMac);

ScrollerPairMac::ScrollerPairMac(ScrollingTreeScrollingNode& node)
    : m_scrollingNode(node)
    , m_verticalScroller(*this, ScrollbarOrientation::Vertical)
    , m_horizontalScroller(*this, ScrollbarOrientation::Horizontal)
{
}

void ScrollerPairMac::init()
{
    m_scrollerImpPairDelegate = adoptNS([[WebScrollerImpPairDelegateMac alloc] initWithScrollerPair:this]);

    m_scrollerImpPair = adoptNS([[NSScrollerImpPair alloc] init]);
    [m_scrollerImpPair setDelegate:m_scrollerImpPairDelegate.get()];
    auto style = ScrollerStyle::recommendedScrollerStyle();
    m_scrollbarStyle = WebCore::scrollbarStyle(style);
    [m_scrollerImpPair setScrollerStyle:style];

    checkedVerticalScroller()->attach();
    checkedHorizontalScroller()->attach();
}

ScrollerPairMac::~ScrollerPairMac()
{
    [m_scrollerImpPairDelegate invalidate];
    [m_scrollerImpPair setDelegate:nil];

    checkedVerticalScroller()->detach();
    checkedHorizontalScroller()->detach();

    ensureOnMainThread([scrollerImpPair = std::exchange(m_scrollerImpPair, nil), verticalScrollerImp = checkedVerticalScroller()->takeScrollerImp(), horizontalScrollerImp = checkedHorizontalScroller()->takeScrollerImp()] {
    });
}

void ScrollerPairMac::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    ensureOnMainThreadWithProtectedThis([phase](auto& scrollerPair) {
        switch (phase) {
        case PlatformWheelEventPhase::Began:
            [scrollerPair.m_scrollerImpPair beginScrollGesture];
            break;
        case PlatformWheelEventPhase::Ended:
        case PlatformWheelEventPhase::Cancelled:
            [scrollerPair.m_scrollerImpPair endScrollGesture];
            break;
        case PlatformWheelEventPhase::MayBegin:
            [scrollerPair.m_scrollerImpPair beginScrollGesture];
            [scrollerPair.m_scrollerImpPair contentAreaScrolled];
            break;
        default:
            break;
        }
    });
}

void ScrollerPairMac::viewWillStartLiveResize()
{
    if (m_inLiveResize)
        return;
    
    m_inLiveResize = true;

    ensureOnMainThreadWithProtectedThis([](auto& scrollerPair) {
        if ([scrollerPair.m_scrollerImpPair overlayScrollerStateIsLocked])
            return;

        [scrollerPair.m_scrollerImpPair startLiveResize];
    });
}

void ScrollerPairMac::viewWillEndLiveResize()
{
    if (!m_inLiveResize)
        return;
    
    m_inLiveResize = false;

    ensureOnMainThreadWithProtectedThis([](auto& scrollerPair) {
        if ([scrollerPair.m_scrollerImpPair overlayScrollerStateIsLocked])
            return;

        [scrollerPair.m_scrollerImpPair endLiveResize];
    });
}

void ScrollerPairMac::contentsSizeChanged()
{
    ensureOnMainThreadWithProtectedThis([](auto& scrollerPair) {
        if ([scrollerPair.m_scrollerImpPair overlayScrollerStateIsLocked])
            return;

        [scrollerPair.m_scrollerImpPair contentAreaDidResize];
    });
}

void ScrollerPairMac::setUsePresentationValues(bool inMomentumPhase)
{
    m_usingPresentationValues = inMomentumPhase;
    m_horizontalScroller.setUsePresentationValue(m_usingPresentationValues);
    m_verticalScroller.setUsePresentationValue(m_usingPresentationValues);
}

void ScrollerPairMac::setHorizontalScrollbarPresentationValue(float scrollbValue)
{
    m_horizontalScroller.setUsePresentationValue(scrollbValue);
}

void ScrollerPairMac::setVerticalScrollbarPresentationValue(float scrollbValue)
{
    m_verticalScroller.setUsePresentationValue(scrollbValue);
}

void ScrollerPairMac::updateValues()
{
    RefPtr node = m_scrollingNode.get();
    if (!node)
        return;

    auto offset = node->currentScrollOffset();

    if (offset != m_lastScrollOffset) {
        if (m_lastScrollOffset) {
            ensureOnMainThreadWithProtectedThis([delta = offset - *m_lastScrollOffset](auto& scrollerPair) {
                [scrollerPair.m_scrollerImpPair contentAreaScrolledInDirection:NSMakePoint(delta.width(), delta.height())];
            });
        }
        m_lastScrollOffset = offset;
    }

    checkedHorizontalScroller()->updateValues();
    checkedVerticalScroller()->updateValues();
}

FloatSize ScrollerPairMac::visibleSize() const
{
    RefPtr node = m_scrollingNode.get();
    if (!node)
        return { };

    return node->scrollableAreaSize();
}

bool ScrollerPairMac::useDarkAppearance() const
{
    return m_useDarkAppearance;
}

ScrollbarWidth ScrollerPairMac::scrollbarWidthStyle() const
{
    return m_scrollbarWidth;
}

ScrollerPairMac::Values ScrollerPairMac::valuesForOrientation(ScrollbarOrientation orientation)
{
    RefPtr node = m_scrollingNode.get();
    if (!node)
        return { };

    float position;
    float totalSize;
    float visibleSize;
    if (orientation == ScrollbarOrientation::Vertical) {
        position = node->currentScrollOffset().y();
        totalSize = node->totalContentsSize().height();
        visibleSize = node->scrollableAreaSize().height();
    } else {
        position = node->currentScrollOffset().x();
        totalSize = node->totalContentsSize().width();
        visibleSize = node->scrollableAreaSize().width();
    }

    float value;
    float overhang;
    ScrollableArea::computeScrollbarValueAndOverhang(position, totalSize, visibleSize, value, overhang);

    float proportion = totalSize ? (visibleSize - overhang) / totalSize : 1;

    return { value, proportion };
}

bool ScrollerPairMac::hasScrollerImp()
{
    return checkedVerticalScroller()->hasScrollerImp() || checkedHorizontalScroller()->hasScrollerImp();
}

void ScrollerPairMac::releaseReferencesToScrollerImpsOnTheMainThread()
{
    if (hasScrollerImp()) {
        // FIXME: This is a workaround in place for the time being since NSScrollerImps cannot be deallocated
        // on a non-main thread. rdar://problem/24535055
        WTF::callOnMainThread([verticalScrollerImp = checkedVerticalScroller()->takeScrollerImp(), horizontalScrollerImp = checkedHorizontalScroller()->takeScrollerImp()] {
        });
    }
}

String ScrollerPairMac::scrollbarStateForOrientation(ScrollbarOrientation orientation) const
{
    return orientation == ScrollbarOrientation::Vertical ? checkedVerticalScroller()->scrollbarState() : checkedHorizontalScroller()->scrollbarState();
}

void ScrollerPairMac::setVerticalScrollerImp(NSScrollerImp *scrollerImp)
{
    ensureOnMainThreadWithProtectedThis([scrollerImp = RetainPtr { scrollerImp }](auto& srollerPair) {
        [srollerPair.m_scrollerImpPair setVerticalScrollerImp:scrollerImp.get()];
    });
}

void ScrollerPairMac::setHorizontalScrollerImp(NSScrollerImp *scrollerImp)
{
    ensureOnMainThreadWithProtectedThis([scrollerImp = RetainPtr { scrollerImp }](auto& scrollerPair) {
        [scrollerPair.m_scrollerImpPair setHorizontalScrollerImp:scrollerImp.get()];
    });
}

void ScrollerPairMac::setScrollbarStyle(ScrollbarStyle style)
{
    m_scrollbarStyle = style;

    ensureOnMainThreadWithProtectedThis([scrollerStyle = nsScrollerStyle(style)](auto& scrollerPair) {
        scrollerPair.m_horizontalScroller.updateScrollbarStyle();
        scrollerPair.m_verticalScroller.updateScrollbarStyle();
        [scrollerPair.m_scrollerImpPair setScrollerStyle:scrollerStyle];
    });
}

void ScrollerPairMac::ensureOnMainThreadWithProtectedThis(Function<void(ScrollerPairMac&)>&& task)
{
    ensureOnMainThread([protectedThis = Ref { *this }, task = WTFMove(task)]() mutable {
        task(protectedThis.get());
    });
}

void ScrollerPairMac::mouseEnteredContentArea()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollerPairMac for [" << protectedNode()->scrollingNodeID() << "] mouseEnteredContentArea");

    ensureOnMainThreadWithProtectedThis([](auto& scrollerPair) {
        if ([scrollerPair.m_scrollerImpPair overlayScrollerStateIsLocked])
            return;

        [scrollerPair.m_scrollerImpPair mouseEnteredContentArea];
    });
}

void ScrollerPairMac::mouseExitedContentArea()
{
    m_mouseInContentArea = false;
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollerPairMac for [" << protectedNode()->scrollingNodeID() << "] mouseExitedContentArea");

    ensureOnMainThreadWithProtectedThis([](auto& scrollerPair) {
        if ([scrollerPair.m_scrollerImpPair overlayScrollerStateIsLocked])
            return;

        [scrollerPair.m_scrollerImpPair mouseExitedContentArea];
    });
}

void ScrollerPairMac::mouseMovedInContentArea(const MouseLocationState& state)
{
    m_mouseInContentArea = true;
    horizontalScroller().setLastKnownMousePositionInScrollbar(state.locationInHorizontalScrollbar);
    verticalScroller().setLastKnownMousePositionInScrollbar(state.locationInVerticalScrollbar);

    ensureOnMainThreadWithProtectedThis([](auto& scrollerPair) {
        if ([scrollerPair.m_scrollerImpPair overlayScrollerStateIsLocked])
            return;
        
        [scrollerPair.m_scrollerImpPair mouseMovedInContentArea];
    });
}

void ScrollerPairMac::mouseIsInScrollbar(ScrollbarHoverState hoverState)
{
    if (m_scrollbarHoverState.mouseIsOverVerticalScrollbar != hoverState.mouseIsOverVerticalScrollbar) {
        if (hoverState.mouseIsOverVerticalScrollbar)
            checkedVerticalScroller()->mouseEnteredScrollbar();
        else
            checkedVerticalScroller()->mouseExitedScrollbar();
    }

    if (m_scrollbarHoverState.mouseIsOverHorizontalScrollbar != hoverState.mouseIsOverHorizontalScrollbar) {
        if (hoverState.mouseIsOverHorizontalScrollbar)
            checkedHorizontalScroller()->mouseEnteredScrollbar();
        else
            checkedHorizontalScroller()->mouseExitedScrollbar();
    }
    m_scrollbarHoverState = hoverState;
}

void ScrollerPairMac::setUseDarkAppearance(bool useDarkAppearance)
{
    if (m_useDarkAppearance == useDarkAppearance)
        return;
    m_useDarkAppearance = useDarkAppearance;

    checkedHorizontalScroller()->setNeedsDisplay();
    checkedVerticalScroller()->setNeedsDisplay();
}

void ScrollerPairMac::setScrollbarWidth(ScrollbarWidth scrollbarWidth)
{
    if (m_scrollbarWidth == scrollbarWidth)
        return;
    m_scrollbarWidth = scrollbarWidth;

    checkedHorizontalScroller()->updateScrollbarStyle();
    checkedVerticalScroller()->updateScrollbarStyle();
}

void ScrollerPairMac::scrollbarColorChanged(const std::optional<ScrollbarColor>& scrollbarColor)
{
    checkedHorizontalScroller()->scrollbarColorChanged(scrollbarColor);
    checkedVerticalScroller()->scrollbarColorChanged(scrollbarColor);
}

void ScrollerPairMac::updateScrollbarPainters()
{
    Locker lockerHorizontal { horizontalScroller().scrollerImpLock() };
    Locker lockerVertical { verticalScroller().scrollerImpLock() };

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [CATransaction lock];

    auto horizontalValues = valuesForOrientation(ScrollbarOrientation::Horizontal);
    setHorizontalScrollbarPresentationValue(horizontalValues.value);

    auto verticalValues = valuesForOrientation(ScrollbarOrientation::Vertical);
    setVerticalScrollbarPresentationValue(verticalValues.value);

    [CATransaction unlock];
    END_BLOCK_OBJC_EXCEPTIONS
}

} // namespace WebCore

#endif // PLATFORM(MAC)
