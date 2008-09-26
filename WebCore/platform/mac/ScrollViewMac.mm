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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#import "BlockExceptions.h"
#import "FloatRect.h"
#import "Frame.h"
#import "FrameView.h"
#import "IntRect.h"
#import "Logging.h"
#import "Page.h"
#import "WebCoreFrameView.h"

using namespace std;

@interface NSWindow (WebWindowDetails)
- (BOOL)_needsToResetDragMargins;
- (void)_setNeedsToResetDragMargins:(BOOL)needs;
@end

namespace WebCore {

class ScrollView::ScrollViewPrivate {
public:
    ScrollViewPrivate()
        : m_scrollbarsAvoidingResizer(0)
    {
    }

    int m_scrollbarsAvoidingResizer;
};

ScrollView::ScrollView()
    : m_data(new ScrollViewPrivate)
{
    init();
}

ScrollView::~ScrollView()
{
    delete m_data;
}

inline NSScrollView<WebCoreFrameScrollView> *ScrollView::scrollView() const
{
    ASSERT(!platformWidget() || [platformWidget() isKindOfClass:[NSScrollView class]]);
    ASSERT(!platformWidget() || [platformWidget() conformsToProtocol:@protocol(WebCoreFrameScrollView)]);
    return static_cast<NSScrollView<WebCoreFrameScrollView> *>(platformWidget());
}

void ScrollView::platformAddChild(Widget* child)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSView *parentView = documentView();
    NSView *childView = child->getOuterView();
    ASSERT(![parentView isDescendantOf:childView]);
    
    // Suppress the resetting of drag margins since we know we can't affect them.
    NSWindow *window = [parentView window];
    BOOL resetDragMargins = [window _needsToResetDragMargins];
    [window _setNeedsToResetDragMargins:NO];
    if ([childView superview] != parentView)
        [parentView addSubview:childView];
    [window _setNeedsToResetDragMargins:resetDragMargins];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollView::platformRemoveChild(Widget* child)
{
    child->removeFromSuperview();
}

void ScrollView::platformSetScrollbarModes()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [scrollView() setScrollingModes:m_horizontalScrollbarMode vertical:m_verticalScrollbarMode andLock:NO];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollView::platformScrollbarModes(ScrollbarMode& horizontal, ScrollbarMode& vertical) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [scrollView() scrollingModes:&horizontal vertical:&vertical];
    END_BLOCK_OBJC_EXCEPTIONS;
}
    
void ScrollView::platformSetCanBlitOnScroll()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [[scrollView() contentView] setCopiesOnScroll:canBlitOnScroll()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

IntRect ScrollView::platformVisibleContentRect(bool includeScrollbars) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS; 
    if (includeScrollbars) {
        if (NSView* documentView = this->documentView())
            return enclosingIntRect([documentView visibleRect]);
    }
    return enclosingIntRect([scrollView() documentVisibleRect]); 
    END_BLOCK_OBJC_EXCEPTIONS; 
    return IntRect();
}

IntSize ScrollView::platformContentsSize() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (NSView* documentView = this->documentView())
        return enclosingIntRect([documentView bounds]).size();
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntSize();
}

void ScrollView::platformSetContentsSize()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    int w = m_contentsSize.width();
    int h = m_contentsSize.height();
    LOG(Frames, "%p %@ at w %d h %d\n", documentView(), [(id)[documentView() class] className], w, h);            
    NSSize tempSize = { max(0, w), max(0, h) }; // workaround for 4213314
    [documentView() setFrameSize:tempSize];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollView::setScrollPosition(const IntPoint& scrollPoint)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSPoint tempPoint = { max(0, scrollPoint.x()), max(0, scrollPoint.y()) }; // Don't use NSMakePoint to work around 4213314.
    [documentView() scrollPoint:tempPoint];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollView::suppressScrollbars(bool suppressed, bool repaintOnUnsuppress)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [scrollView() setScrollBarsSuppressed:suppressed
                      repaintOnUnsuppress:repaintOnUnsuppress];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollView::updateContents(const IntRect& rect, bool now)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSView *view = documentView();
    if (!now && ![[view window] isVisible] && !shouldUpdateWhileOffscreen())
        return;

    NSRect visibleRect = visibleContentRect();

    // Checking for rect visibility is an important optimization for the case of
    // Select All of a large document. AppKit does not do this check, and so ends
    // up building a large complicated NSRegion if we don't perform the check.
    NSRect dirtyRect = NSIntersectionRect(rect, visibleRect);
    if (!NSIsEmptyRect(dirtyRect)) {
        [view setNeedsDisplayInRect:dirtyRect];
        if (now) {
            [[view window] displayIfNeeded];
            [[view window] flushWindowIfNeeded];
        }
    }

    END_BLOCK_OBJC_EXCEPTIONS;
}

void ScrollView::update()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSView *view = platformWidget();
    [[view window] displayIfNeeded];
    [[view window] flushWindowIfNeeded];

    END_BLOCK_OBJC_EXCEPTIONS;
}

// "Containing Window" means the NSWindow's coord system, which is origin lower left

IntPoint ScrollView::contentsToWindow(const IntPoint& contentsPoint) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (NSView* documentView = this->documentView()) {
        NSPoint tempPoint = { contentsPoint.x(), contentsPoint.y() }; // Don't use NSMakePoint to work around 4213314.
        return IntPoint([documentView convertPoint:tempPoint toView:nil]);
    }
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntPoint();
}

IntPoint ScrollView::windowToContents(const IntPoint& point) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (NSView* documentView = this->documentView()) {
        NSPoint tempPoint = { point.x(), point.y() }; // Don't use NSMakePoint to work around 4213314.
        return IntPoint([documentView convertPoint:tempPoint fromView:nil]);
    }
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntPoint();
}

IntRect ScrollView::contentsToWindow(const IntRect& contentsRect) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (NSView* documentView = this->documentView())
        return IntRect([documentView convertRect:contentsRect toView:nil]);
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntRect();
}

IntRect ScrollView::windowToContents(const IntRect& rect) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (NSView* documentView = this->documentView())
        return IntRect([documentView convertRect:rect fromView:nil]);
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntRect();
}

IntRect ScrollView::contentsToScreen(const IntRect& rect) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (NSView* documentView = this->documentView()) {
        NSRect tempRect = rect;
        tempRect = [documentView convertRect:tempRect toView:nil];
        tempRect.origin = [[documentView window] convertBaseToScreen:tempRect.origin];
        return enclosingIntRect(tempRect);
    }
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntRect();
}

IntPoint ScrollView::screenToContents(const IntPoint& point) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (NSView* documentView = this->documentView()) {
        NSPoint windowCoord = [[documentView window] convertScreenToBase: point];
        return IntPoint([documentView convertPoint:windowCoord fromView:nil]);
    }
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntPoint();
}

NSView *ScrollView::documentView() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [scrollView() documentView];
    END_BLOCK_OBJC_EXCEPTIONS;
    return nil;
}

Scrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent&)
{
    return 0;
}

bool ScrollView::inWindow() const
{
    return [platformWidget() window];
}

void ScrollView::wheelEvent(PlatformWheelEvent&)
{
    // Do nothing. NSScrollView handles doing the scroll for us.
}

IntRect ScrollView::windowResizerRect()
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return IntRect();
    return page->chrome()->windowResizerRect();
}

bool ScrollView::resizerOverlapsContent() const
{
    return !m_data->m_scrollbarsAvoidingResizer;
}

void ScrollView::adjustOverlappingScrollbarCount(int overlapDelta)
{
    m_data->m_scrollbarsAvoidingResizer += overlapDelta;
    if (parent() && parent()->isFrameView())
        static_cast<FrameView*>(parent())->adjustOverlappingScrollbarCount(overlapDelta);
}

void ScrollView::setParent(ScrollView* parentView)
{
    if (!parentView && m_data->m_scrollbarsAvoidingResizer && parent() && parent()->isFrameView())
        static_cast<FrameView*>(parent())->adjustOverlappingScrollbarCount(false);
    Widget::setParent(parentView);
}

}
