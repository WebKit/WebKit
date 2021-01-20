/*
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#import "ScrollView.h"

#if PLATFORM(IOS_FAMILY)

#import "FloatRect.h"
#import "IntRect.h"
#import "Logging.h"
#import "NotImplemented.h"
#import "WAKAppKitStubs.h"
#import "WAKClipView.h"
#import "WAKScrollView.h"
#import "WAKViewInternal.h"
#import "WAKWindow.h"
#import "WKViewPrivate.h"
#import "WebCoreFrameView.h"
#import <wtf/BlockObjCExceptions.h>

using namespace std;

namespace WebCore {

inline NSScrollView<WebCoreFrameScrollView> *ScrollView::scrollView() const
{
    ASSERT(!platformWidget() || [platformWidget() isKindOfClass:[NSScrollView class]]);
    ASSERT(!platformWidget() || [platformWidget() conformsToProtocol:@protocol(WebCoreFrameScrollView)]);
    return static_cast<NSScrollView<WebCoreFrameScrollView> *>(platformWidget());
}

NSView *ScrollView::documentView() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return [scrollView() documentView];
    END_BLOCK_OBJC_EXCEPTIONS
    return nil;
}

void ScrollView::platformAddChild(Widget* child)
{
    ASSERT(child != this);

    child->addToSuperview(documentView());
}

void ScrollView::platformRemoveChild(Widget* child)
{
    child->removeFromSuperview();
}

void ScrollView::platformSetScrollbarModes()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [scrollView() setScrollingModes:m_horizontalScrollbarMode vertical:m_verticalScrollbarMode andLock:NO];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollView::platformScrollbarModes(ScrollbarMode& horizontal, ScrollbarMode& vertical) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [scrollView() scrollingModes:&horizontal vertical:&vertical];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollView::platformSetCanBlitOnScroll(bool canBlitOnScroll)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[scrollView() contentView] setCopiesOnScroll:canBlitOnScroll];
    ALLOW_DEPRECATED_DECLARATIONS_END
    END_BLOCK_OBJC_EXCEPTIONS
}

bool ScrollView::platformCanBlitOnScroll() const
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [[scrollView() contentView] copiesOnScroll];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

IntRect ScrollView::platformUnobscuredContentRect(VisibleContentRectIncludesScrollbars) const
{
    ASSERT(platformWidget());
    WAKScrollView *view = static_cast<WAKScrollView *>(platformWidget());
    CGRect r = CGRectZero;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    r = [view unobscuredContentRect];
    END_BLOCK_OBJC_EXCEPTIONS
    return enclosingIntRect(r);
}

FloatRect ScrollView::platformExposedContentRect() const
{
    ASSERT(platformWidget());
    NSScrollView *view = static_cast<NSScrollView *>(platformWidget());
    CGRect r = CGRectZero;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if ([view isKindOfClass:[NSScrollView class]])
        r = [view exposedContentRect];
    else {
        r.origin = [view visibleRect].origin;
        r.size = [view bounds].size;
    }

    END_BLOCK_OBJC_EXCEPTIONS
    return r;
}

void ScrollView::setActualScrollPosition(const IntPoint& position)
{
    NSScrollView *view = static_cast<NSScrollView *>(platformWidget());

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if ([view isKindOfClass:[NSScrollView class]])
        [view setActualScrollPosition:position];
    END_BLOCK_OBJC_EXCEPTIONS
}

float ScrollView::platformTopContentInset() const
{
    return 0;
}

void ScrollView::platformSetTopContentInset(float)
{
}

IntRect ScrollView::platformVisibleContentRect(bool includeScrollbars) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (includeScrollbars) {
        if (NSView* documentView = this->documentView())
            return enclosingIntRect([documentView visibleRect]);
    }
    return enclosingIntRect([scrollView() documentVisibleRect]);
    END_BLOCK_OBJC_EXCEPTIONS
    return IntRect();
}

IntSize ScrollView::platformVisibleContentSize(bool includeScrollbars) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (includeScrollbars) {
        if (NSView* documentView = this->documentView())
            return IntSize([documentView visibleRect].size);
    }

    return expandedIntSize(FloatSize([scrollView() documentVisibleRect].size));
    END_BLOCK_OBJC_EXCEPTIONS
    return IntSize();
}

IntRect ScrollView::platformVisibleContentRectIncludingObscuredArea(bool includeScrollbars) const
{
    return platformVisibleContentRect(includeScrollbars);
}

IntSize ScrollView::platformVisibleContentSizeIncludingObscuredArea(bool includeScrollbars) const
{
    return platformVisibleContentSize(includeScrollbars);
}

LegacyTileCache* ScrollView::legacyTileCache()
{
    // Make tile cache pointer available via the main frame only. Tile cache interaction should be managed by
    // the main frame and this avoids having to add parent checks to all call sites.
    if (parent())
        return 0;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    WAKScrollView *view = static_cast<WAKScrollView *>(platformWidget());
    return [[view window] tileCache];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollView::platformSetContentsSize()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    int w = m_contentsSize.width();
    int h = m_contentsSize.height();
#if !PLATFORM(IOS_FAMILY)
    LOG(Frames, "%p %@ at w %d h %d\n", documentView(), [(id)[documentView() class] className], w, h);            
#else
    LOG(Frames, "%p %@ at w %d h %d\n", documentView(), NSStringFromClass([documentView() class]), w, h);
#endif
    NSSize tempSize = { static_cast<CGFloat>(max(0, w)), static_cast<CGFloat>(max(0, h)) }; // workaround for 4213314
    [documentView() setBoundsSize:tempSize];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollView::platformSetScrollbarsSuppressed(bool repaintOnUnsuppress)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [scrollView() setScrollBarsSuppressed:m_scrollbarsSuppressed
                      repaintOnUnsuppress:repaintOnUnsuppress];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollView::platformSetScrollPosition(const IntPoint& scrollPoint)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    NSPoint floatPoint = scrollPoint;
    NSPoint tempPoint = { max(-[scrollView() scrollOrigin].x, floatPoint.x), max(-[scrollView() scrollOrigin].y, floatPoint.y) };  // Don't use NSMakePoint to work around 4213314.
    [documentView() scrollPoint:tempPoint];
    END_BLOCK_OBJC_EXCEPTIONS
}

bool ScrollView::platformScroll(ScrollDirection, ScrollGranularity)
{
    // FIXME: It would be nice to implement this so that all of the code in WebFrameView could go away.
    notImplemented();
    return false;
}

void ScrollView::platformRepaintContentRectangle(const IntRect& rect)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    NSView *view = documentView();

    [view setNeedsDisplayInRect:rect];    

    END_BLOCK_OBJC_EXCEPTIONS
}

// "Containing Window" means the NSWindow's coord system, which is origin lower left

IntRect ScrollView::platformContentsToScreen(const IntRect& rect) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (NSView* documentView = this->documentView()) {
        NSRect tempRect = rect;
        tempRect = [documentView convertRect:tempRect toView:nil];
        tempRect.origin = [[documentView window] convertBaseToScreen:tempRect.origin];
        return enclosingIntRect(tempRect);
    }
    END_BLOCK_OBJC_EXCEPTIONS
    return IntRect();
}

IntPoint ScrollView::platformScreenToContents(const IntPoint& point) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (NSView* documentView = this->documentView()) {
        NSPoint windowCoord = [[documentView window] convertScreenToBase: point];
        return IntPoint([documentView convertPoint:windowCoord fromView:nil]);
    }
    END_BLOCK_OBJC_EXCEPTIONS
    return IntPoint();
}

bool ScrollView::platformIsOffscreen() const
{
    // FIXME: DDK: ScrollViewMac.mm also checks: ![[platformWidget() window] isVisible]
    // but -[WAKWindow isVisible] doesn't exist.
    return ![platformWidget() window];
}

void ScrollView::platformSetScrollbarOverlayStyle(ScrollbarOverlayStyle)
{
}

void ScrollView::platformSetScrollOrigin(const IntPoint& origin, bool updatePositionAll, bool updatePositionSynchronously)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [scrollView() setScrollOrigin:static_cast<CGPoint>(origin) updatePositionAtAll:updatePositionAll immediately:updatePositionSynchronously];
    END_BLOCK_OBJC_EXCEPTIONS
}

}

#endif // PLATFORM(IOS_FAMILY)
