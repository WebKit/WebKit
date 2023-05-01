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
#import <WebCore/FloatPoint.h>
#import <WebCore/IntRect.h>
#import <WebCore/NSScrollerImpDetails.h>
#import <WebCore/PlatformWheelEvent.h>
#import <WebCore/ScrollTypes.h>
#import <WebCore/ScrollableArea.h>
#import <WebCore/ScrollingTreeScrollingNode.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>

@interface WebScrollerImpPairDelegateMac : NSObject <NSScrollerImpPairDelegate> {
    WebCore::ScrollerPairMac* _scrollerPair;
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
    if (!_scrollerPair)
        return NSZeroRect;

    auto size = _scrollerPair->visibleSize();
    return NSMakeRect(0, 0, size.width(), size.height());
}

- (BOOL)inLiveResizeForScrollerImpPair:(NSScrollerImpPair *)scrollerImpPair
{
    return _scrollerPair->inLiveResize();
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

    if (!_scrollerPair || !scrollerImp)
        return NSZeroPoint;

    WebCore::ScrollerMac* scroller = nullptr;
    if ([scrollerImp isHorizontal])
        scroller = &_scrollerPair->horizontalScroller();
    else
        scroller = &_scrollerPair->verticalScroller();

    ASSERT(scrollerImp == scroller->scrollerImp());

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

    _scrollerPair->setScrollbarStyle(WebCore::scrollbarStyle(newRecommendedScrollerStyle));
}

@end

namespace WebCore {

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

    m_verticalScroller.attach();
    m_horizontalScroller.attach();
}

ScrollerPairMac::~ScrollerPairMac()
{
    [m_scrollerImpPairDelegate invalidate];
    [m_scrollerImpPair setDelegate:nil];
    
    m_verticalScroller.detach();
    m_horizontalScroller.detach();

    ensureOnMainThread([scrollerImpPair = std::exchange(m_scrollerImpPair, nil), verticalScrollerImp = verticalScroller().takeScrollerImp(), horizontalScrollerImp = horizontalScroller().takeScrollerImp()] {
    });
}

void ScrollerPairMac::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    ensureOnMainThreadWithProtectedThis([phase, this] {
        switch (phase) {
        case PlatformWheelEventPhase::Began:
            [m_scrollerImpPair beginScrollGesture];
            break;
        case PlatformWheelEventPhase::Ended:
        case PlatformWheelEventPhase::Cancelled:
            [m_scrollerImpPair endScrollGesture];
            break;
        case PlatformWheelEventPhase::MayBegin:
            [m_scrollerImpPair beginScrollGesture];
            [m_scrollerImpPair contentAreaScrolled];
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

    ensureOnMainThreadWithProtectedThis([this] {
        if ([m_scrollerImpPair overlayScrollerStateIsLocked])
            return;

        [m_scrollerImpPair startLiveResize];
    });
}

void ScrollerPairMac::viewWillEndLiveResize()
{
    if (!m_inLiveResize)
        return;
    
    m_inLiveResize = false;

    ensureOnMainThreadWithProtectedThis([this] {
        if ([m_scrollerImpPair overlayScrollerStateIsLocked])
            return;

        [m_scrollerImpPair endLiveResize];
    });
}

void ScrollerPairMac::contentsSizeChanged()
{
    ensureOnMainThreadWithProtectedThis([this] {
        if ([m_scrollerImpPair overlayScrollerStateIsLocked])
            return;

        [m_scrollerImpPair contentAreaDidResize];
    });
}

void ScrollerPairMac::setUsePresentationValues(bool inMomentumPhase)
{
    m_usingPresentationValues = inMomentumPhase;
    [scrollerImpHorizontal() setUsePresentationValue:m_usingPresentationValues];
    [scrollerImpVertical() setUsePresentationValue:m_usingPresentationValues];
}

void ScrollerPairMac::setHorizontalScrollbarPresentationValue(float scrollbValue)
{
    [scrollerImpHorizontal() setPresentationValue:scrollbValue];
}

void ScrollerPairMac::setVerticalScrollbarPresentationValue(float scrollbValue)
{
    [scrollerImpVertical() setPresentationValue:scrollbValue];
}

void ScrollerPairMac::updateValues()
{
    auto offset = m_scrollingNode.currentScrollOffset();

    if (offset != m_lastScrollOffset) {
        if (m_lastScrollOffset) {
            ensureOnMainThreadWithProtectedThis([delta = offset - *m_lastScrollOffset, this] {
                [m_scrollerImpPair contentAreaScrolledInDirection:NSMakePoint(delta.width(), delta.height())];
            });
        }
        m_lastScrollOffset = offset;
    }

    m_horizontalScroller.updateValues();
    m_verticalScroller.updateValues();
}

FloatSize ScrollerPairMac::visibleSize() const
{
    return m_scrollingNode.scrollableAreaSize();
}

bool ScrollerPairMac::useDarkAppearance() const
{
    return m_scrollingNode.useDarkAppearanceForScrollbars();
}

ScrollerPairMac::Values ScrollerPairMac::valuesForOrientation(ScrollbarOrientation orientation)
{
    float position;
    float totalSize;
    float visibleSize;
    if (orientation == ScrollbarOrientation::Vertical) {
        position = m_scrollingNode.currentScrollOffset().y();
        totalSize = m_scrollingNode.totalContentsSize().height();
        visibleSize = m_scrollingNode.scrollableAreaSize().height();
    } else {
        position = m_scrollingNode.currentScrollOffset().x();
        totalSize = m_scrollingNode.totalContentsSize().width();
        visibleSize = m_scrollingNode.scrollableAreaSize().width();
    }

    float value;
    float overhang;
    ScrollableArea::computeScrollbarValueAndOverhang(position, totalSize, visibleSize, value, overhang);

    float proportion = totalSize ? (visibleSize - overhang) / totalSize : 1;

    return { value, proportion };
}

bool ScrollerPairMac::hasScrollerImp()
{
    return verticalScroller().scrollerImp() || horizontalScroller().scrollerImp();
}

void ScrollerPairMac::releaseReferencesToScrollerImpsOnTheMainThread()
{
    if (hasScrollerImp()) {
        // FIXME: This is a workaround in place for the time being since NSScrollerImps cannot be deallocated
        // on a non-main thread. rdar://problem/24535055
        WTF::callOnMainThread([verticalScrollerImp = verticalScroller().takeScrollerImp(), horizontalScrollerImp = horizontalScroller().takeScrollerImp()] {
        });
    }
}

String ScrollerPairMac::scrollbarStateForOrientation(ScrollbarOrientation orientation) const
{
    return orientation == ScrollbarOrientation::Vertical ? m_verticalScroller.scrollbarState() : m_horizontalScroller.scrollbarState();
}

void ScrollerPairMac::setVerticalScrollerImp(NSScrollerImp *scrollerImp)
{
    ensureOnMainThreadWithProtectedThis([this, scrollerImp = RetainPtr { scrollerImp }] {
        [m_scrollerImpPair setVerticalScrollerImp:scrollerImp.get()];
    });
}

void ScrollerPairMac::setHorizontalScrollerImp(NSScrollerImp *scrollerImp)
{
    ensureOnMainThreadWithProtectedThis([this, scrollerImp = RetainPtr { scrollerImp }] {
        [m_scrollerImpPair setHorizontalScrollerImp:scrollerImp.get()];
    });
}

void ScrollerPairMac::setScrollbarStyle(ScrollbarStyle style)
{
    m_scrollbarStyle = style;

    ensureOnMainThreadWithProtectedThis([this, scrollerStyle = nsScrollerStyle(style)] {
        m_horizontalScroller.updateScrollbarStyle();
        m_verticalScroller.updateScrollbarStyle();
        [m_scrollerImpPair setScrollerStyle:scrollerStyle];
    });
}

void ScrollerPairMac::ensureOnMainThreadWithProtectedThis(Function<void()>&& task)
{
    ensureOnMainThread([protectedThis = Ref { *this }, task = WTFMove(task)]() mutable {
        task();
    });
}

void ScrollerPairMac::mouseEnteredContentArea()
{
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollerPairMac for [" << m_scrollingNode.scrollingNodeID() << "] mouseEnteredContentArea");

    ensureOnMainThreadWithProtectedThis([this] {
        if ([m_scrollerImpPair overlayScrollerStateIsLocked])
            return;

        [m_scrollerImpPair mouseEnteredContentArea];
    });
}

void ScrollerPairMac::mouseExitedContentArea()
{
    m_mouseInContentArea = false;
    LOG_WITH_STREAM(OverlayScrollbars, stream << "ScrollerPairMac for [" << m_scrollingNode.scrollingNodeID() << "] mouseExitedContentArea");
    
    ensureOnMainThreadWithProtectedThis([this] {
        if ([m_scrollerImpPair overlayScrollerStateIsLocked])
            return;

        [m_scrollerImpPair mouseExitedContentArea];
    });
}

void ScrollerPairMac::mouseMovedInContentArea(const MouseLocationState& state)
{
    m_mouseInContentArea = true;
    horizontalScroller().setLastKnownMousePositionInScrollbar(state.locationInHorizontalScrollbar);
    verticalScroller().setLastKnownMousePositionInScrollbar(state.locationInVerticalScrollbar);

    ensureOnMainThreadWithProtectedThis([this] {
        if ([m_scrollerImpPair overlayScrollerStateIsLocked])
            return;
        
        [m_scrollerImpPair mouseMovedInContentArea];
    });
}

void ScrollerPairMac::mouseIsInScrollbar(ScrollbarHoverState hoverState)
{
    if (m_scrollbarHoverState.mouseIsOverVerticalScrollbar != hoverState.mouseIsOverVerticalScrollbar) {
        if (hoverState.mouseIsOverVerticalScrollbar)
            verticalScroller().mouseEnteredScrollbar();
        else
            verticalScroller().mouseExitedScrollbar();
    }

    if (m_scrollbarHoverState.mouseIsOverHorizontalScrollbar != hoverState.mouseIsOverHorizontalScrollbar) {
        if (hoverState.mouseIsOverHorizontalScrollbar)
            horizontalScroller().mouseEnteredScrollbar();
        else
            horizontalScroller().mouseExitedScrollbar();
    }
    m_scrollbarHoverState = hoverState;
}

} // namespace WebCore

#endif // PLATFORM(MAC)
