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

#include "config.h"
#include "ScrollerPairMac.h"

#if PLATFORM(MAC)

#include "RemoteScrollingTree.h"
#include <WebCore/FloatPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/NSScrollerImpDetails.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ScrollableArea.h>
#include <WebCore/ScrollingTreeScrollingNode.h>
#include <pal/spi/mac/NSScrollerImpSPI.h>

@interface WKScrollerImpPairDelegate : NSObject <NSScrollerImpPairDelegate> {
    WebKit::ScrollerPairMac* _scrollerPair;
}
- (id)initWithScrollerPair:(WebKit::ScrollerPairMac*)scrollerPair;
@end

@implementation WKScrollerImpPairDelegate

- (id)initWithScrollerPair:(WebKit::ScrollerPairMac*)scrollerPair
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
    // FIMXE: Not implemented.
    return NO;
}

- (NSPoint)mouseLocationInContentAreaForScrollerImpPair:(NSScrollerImpPair *)scrollerImpPair
{
    UNUSED_PARAM(scrollerImpPair);
    if (!_scrollerPair)
        return NSZeroPoint;

    return _scrollerPair->lastKnownMousePosition();
}

- (NSPoint)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair convertContentPoint:(NSPoint)pointInContentArea toScrollerImp:(NSScrollerImp *)scrollerImp
{
    UNUSED_PARAM(scrollerImpPair);

    if (!_scrollerPair || !scrollerImp)
        return NSZeroPoint;

    WebKit::ScrollerMac* scroller = nullptr;
    if ([scrollerImp isHorizontal])
        scroller = &_scrollerPair->horizontalScroller();
    else
        scroller = &_scrollerPair->verticalScroller();

    ASSERT(scrollerImp == scroller->scrollerImp());

    return scroller->convertFromContent(WebCore::IntPoint(pointInContentArea));
}

- (void)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair setContentAreaNeedsDisplayInRect:(NSRect)rect
{
    UNUSED_PARAM(scrollerImpPair);
    UNUSED_PARAM(rect);
}

- (void)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair updateScrollerStyleForNewRecommendedScrollerStyle:(NSScrollerStyle)newRecommendedScrollerStyle
{
    [scrollerImpPair setScrollerStyle:newRecommendedScrollerStyle];
}

@end

namespace WebKit {

ScrollerPairMac::ScrollerPairMac(WebCore::ScrollingTreeScrollingNode& node)
    : m_scrollingNode(node)
    , m_verticalScroller(*this, ScrollerMac::Orientation::Vertical)
    , m_horizontalScroller(*this, ScrollerMac::Orientation::Horizontal)
{
    m_scrollerImpPairDelegate = adoptNS([[WKScrollerImpPairDelegate alloc] initWithScrollerPair:this]);

    m_scrollerImpPair = adoptNS([[NSScrollerImpPair alloc] init]);
    [m_scrollerImpPair.get() setDelegate:m_scrollerImpPairDelegate.get()];
    [m_scrollerImpPair setScrollerStyle:WebCore::ScrollerStyle::recommendedScrollerStyle()];

    m_verticalScroller.attach();
    m_horizontalScroller.attach();
}

ScrollerPairMac::~ScrollerPairMac()
{
    [m_scrollerImpPairDelegate invalidate];
    [m_scrollerImpPair setDelegate:nil];
}

bool ScrollerPairMac::handleWheelEvent(const WebCore::PlatformWheelEvent& event)
{
    switch (event.phase()) {
    case WebCore::PlatformWheelEventPhaseBegan:
        [m_scrollerImpPair beginScrollGesture];
        break;
    case WebCore::PlatformWheelEventPhaseEnded:
    case WebCore::PlatformWheelEventPhaseCancelled:
        [m_scrollerImpPair endScrollGesture];
        break;
    case WebCore::PlatformWheelEventPhaseMayBegin:
        [m_scrollerImpPair beginScrollGesture];
        [m_scrollerImpPair contentAreaScrolled];
        break;
    default:
        break;
    }
    // FIXME: this needs to return whether the event was handled.
    return true;
}

bool ScrollerPairMac::handleMouseEvent(const WebCore::PlatformMouseEvent& event)
{
    if (event.type() != WebCore::PlatformEvent::MouseMoved)
        return false;

    m_lastKnownMousePosition = event.position();
    [m_scrollerImpPair mouseMovedInContentArea];
    // FIXME: this needs to return whether the event was handled.
    return true;
}

void ScrollerPairMac::updateValues()
{
    auto position = m_scrollingNode.currentScrollPosition();

    if (position != m_lastScrollPosition) {
        if (m_lastScrollPosition) {
            auto delta = position - *m_lastScrollPosition;
            [m_scrollerImpPair contentAreaScrolledInDirection:NSMakePoint(delta.width(), delta.height())];
        }
        m_lastScrollPosition = position;
    }

    m_verticalScroller.updateValues();
    m_horizontalScroller.updateValues();
}

WebCore::FloatSize ScrollerPairMac::visibleSize() const
{
    return m_scrollingNode.scrollableAreaSize();
}

bool ScrollerPairMac::useDarkAppearance() const
{
    return m_scrollingNode.useDarkAppearanceForScrollbars();
}

ScrollerPairMac::Values ScrollerPairMac::valuesForOrientation(ScrollerMac::Orientation orientation)
{
    float position;
    float totalSize;
    float visibleSize;
    if (orientation == ScrollerMac:: Orientation::Vertical) {
        position = m_scrollingNode.currentScrollPosition().y();
        totalSize = m_scrollingNode.totalContentsSize().height();
        visibleSize = m_scrollingNode.scrollableAreaSize().height();
    } else {
        position = m_scrollingNode.currentScrollPosition().x();
        totalSize = m_scrollingNode.totalContentsSize().width();
        visibleSize = m_scrollingNode.scrollableAreaSize().width();
    }

    float value;
    float overhang;
    WebCore::ScrollableArea::computeScrollbarValueAndOverhang(position, totalSize, visibleSize, value, overhang);

    float proportion = totalSize ? (visibleSize - overhang) / totalSize : 1;

    return { value, proportion };
}

}

#endif
