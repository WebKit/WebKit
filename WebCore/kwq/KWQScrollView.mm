/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQScrollView.h"

#import "KWQExceptions.h"
#import "KWQLogging.h"
#import "WebCoreFrameView.h"

/*
    This class implementation does NOT actually emulate the Qt QScrollView.
    It does provide an implementation that khtml will use to interact with
    WebKit's WebFrameView documentView and our NSScrollView subclass.

    QScrollView's view is a NSScrollView (or subclass of NSScrollView)
    in most cases. That scrollview is a subview of an
    WebCoreFrameView. The WebCoreFrameView's documentView will also be
    the scroll view's documentView.
    
    The WebCoreFrameView's size is the frame size.  The WebCoreFrameView's documentView
    corresponds to the frame content size.  The scrollview itself is autosized to the
    WebCoreFrameView's size (see QWidget::resize).
*/

@interface NSView (KWQExtensions)
- (BOOL)_KWQ_isScrollView;
@end

@implementation NSView (KWQExtensions)

- (BOOL)_KWQ_isScrollView
{
    return [self isKindOfClass:[NSScrollView class]];
}

@end

QWidget* QScrollView::viewport() const
{
    return const_cast<QScrollView *>(this);
}

int QScrollView::visibleWidth() const
{
    NSScrollView *view = (NSScrollView *)getView();
    volatile int visibleWidth = 0;

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view _KWQ_isScrollView]) {
        visibleWidth = (int)[view documentVisibleRect].size.width;
    } else {
        visibleWidth = (int)[view bounds].size.width;
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return visibleWidth;
}

int QScrollView::visibleHeight() const
{
    NSScrollView *view = (NSScrollView *)getView();
    volatile int visibleHeight = 0;
    
    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view _KWQ_isScrollView]) {
        visibleHeight = (int)[view documentVisibleRect].size.height;
    } else {
        visibleHeight = (int)[view bounds].size.height;
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;
    
    return visibleHeight;
}

int QScrollView::contentsWidth() const
{
    NSView *docView, *view = getView();
    docView = getDocumentView();

    volatile int result = 0;
    KWQ_BLOCK_NS_EXCEPTIONS;
    if (docView) {
        result = (int)[docView bounds].size.width;
    } else {
	result = (int)[view bounds].size.width;
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return result;
}

int QScrollView::contentsHeight() const
{
    NSView *docView, *view = getView();
    docView = getDocumentView();

    volatile int result = 0;
    KWQ_BLOCK_NS_EXCEPTIONS;
    if (docView) {
        result = (int)[docView bounds].size.height;
    } else {
	result = (int)[view bounds].size.height;
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return result;
}

int QScrollView::contentsX() const
{
    NSView *view = getView();
    volatile float vx = 0;

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view _KWQ_isScrollView]) {
        NSScrollView *sview = view;
        vx = (int)[sview documentVisibleRect].origin.x;
    } else {
        vx = (int)[view visibleRect].origin.x;
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return (int)vx;
}

int QScrollView::contentsY() const
{
    NSView *view = getView();
    volatile float vy = 0;

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view _KWQ_isScrollView]) {
        NSScrollView *sview = view;
        vy = (int)[sview documentVisibleRect].origin.y;
    } else {
        vy = (int)[view visibleRect].origin.y;
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return (int)vy;
}

int QScrollView::childX(QWidget *)
{
    return 0;
}

int QScrollView::childY(QWidget *)
{
    return 0;
}

void QScrollView::scrollBy(int dx, int dy)
{
    setContentsPos(contentsX() + dx, contentsY() + dy);
}

void QScrollView::setContentsPos(int x, int y)
{
    NSView *docView;
    volatile NSView * volatile view = getView();    
    docView = getDocumentView();
    if (docView)
        view = docView;
        
    volatile int _x = (x < 0) ? 0 : x;
    volatile int _y = (y < 0) ? 0 : y;

    KWQ_BLOCK_NS_EXCEPTIONS;
    [view scrollPoint: NSMakePoint(_x,_y)];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QScrollView::setVScrollBarMode(ScrollBarMode vMode)
{
    NSView* view = getView();

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        [frameView setVerticalScrollingMode: (WebCoreScrollBarMode)vMode];
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QScrollView::setHScrollBarMode(ScrollBarMode hMode)
{
    NSView* view = getView();

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        [frameView setHorizontalScrollingMode: (WebCoreScrollBarMode)hMode];
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QScrollView::setScrollBarsMode(ScrollBarMode mode)
{
    NSView* view = getView();

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        [frameView setScrollingMode: (WebCoreScrollBarMode)mode];
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

QScrollView::ScrollBarMode
QScrollView::vScrollBarMode() const
{
    NSView* view = getView();

    volatile QScrollView::ScrollBarMode mode = Auto;
    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        mode = (ScrollBarMode)[frameView verticalScrollingMode];
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return mode;
}

QScrollView::ScrollBarMode
QScrollView::hScrollBarMode() const
{
    NSView* view = getView();

    volatile QScrollView::ScrollBarMode mode = Auto;

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        mode = (ScrollBarMode)[frameView horizontalScrollingMode];
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return mode;
}

bool QScrollView::hasVerticalScrollBar() const
{
    NSScrollView *view = (NSScrollView *)getView();
    volatile bool result = false;

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        result = [view hasVerticalScroller];
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return result;
}

bool QScrollView::hasHorizontalScrollBar() const
{
    NSScrollView *view = (NSScrollView *)getView();
    volatile bool result = false;

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        result = [view hasHorizontalScroller];
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return result;
}

void QScrollView::suppressScrollBars(bool suppressed,  bool repaintOnUnsuppress)
{
    NSView* view = getView();

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        [frameView setScrollBarsSuppressed: suppressed
                       repaintOnUnsuppress: repaintOnUnsuppress];
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QScrollView::addChild(QWidget* child, int x, int y)
{
    volatile NSView * volatile thisView;
    NSView *thisDocView, *subview;

    ASSERT(child != this);
    
    child->move(x, y);
    
    thisView = getView();
    thisDocView = getDocumentView();
    if (thisDocView)
        thisView = thisDocView;

    subview = child->getOuterView();
    ASSERT(subview != thisView);

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([subview superview] != thisView) {
	[subview removeFromSuperview];
	
	LOG(Frames, "Adding %p %@ at (%d,%d) w %d h %d\n", subview,
	    [(id)[subview class] className], x, y, (int)[subview frame].size.width, (int)[subview frame].size.height);
	
	[thisView addSubview:subview];
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QScrollView::removeChild(QWidget* child)
{
    KWQ_BLOCK_NS_EXCEPTIONS;
    [child->getOuterView() removeFromSuperview];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QScrollView::resizeContents(int w, int h)
{
    volatile int _w = w;
    volatile int _h = h;

    LOG(Frames, "%p %@ at w %d h %d\n", getView(), [(id)[getView() class] className], w, h);
    NSView *view = getView();
    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view _KWQ_isScrollView]){
        view = getDocumentView();
        
        LOG(Frames, "%p %@ at w %d h %d\n", view, [(id)[view class] className], w, h);
        if (_w < 0)
            _w = 0;
        if (_h < 0)
            _h = 0;

        [view setFrameSize: NSMakeSize (_w,_h)];
    } else {
        resize (_w, _h);
    }
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QScrollView::updateContents(int x, int y, int w, int h, bool now)
{
    updateContents(QRect(x, y, w, h), now);
}

void QScrollView::updateContents(const QRect &rect, bool now)
{
    volatile NSView * volatile view = getView();

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        view = getDocumentView();

    if (now)
        [view displayRect: rect];
    else
        [view setNeedsDisplayInRect:rect];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QScrollView::repaintContents(int x, int y, int w, int h, bool erase)
{
    LOG(Frames, "%p %@ at (%d,%d) w %d h %d\n", getView(), [(id)[getView() class] className], x, y, w, h);
}

QPoint QScrollView::contentsToViewport(const QPoint &p)
{
    int vx, vy;
    contentsToViewport(p.x(), p.y(), vx, vy);
    return QPoint(vx, vy);
}

void QScrollView::contentsToViewport(int x, int y, int& vx, int& vy)
{
    NSView *docView;
    volatile NSView * volatile view = getView();    
     
    docView = getDocumentView();
    if (docView)
        view = docView;
    
    NSPoint np = {0,0};
    KWQ_BLOCK_NS_EXCEPTIONS;
    np = [view convertPoint: NSMakePoint (x, y) toView: nil];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
    
    vx = (int)np.x;
    vy = (int)np.y;
}

void QScrollView::viewportToContents(int vx, int vy, int& x, int& y)
{
    NSView *docView;
    volatile NSView * volatile view = getView();    

    docView = getDocumentView();
    if (docView)
        view = docView;
        
    NSPoint np;
    KWQ_BLOCK_NS_EXCEPTIONS;
    np = [view convertPoint: NSMakePoint (vx, vy) fromView: nil];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
    
    x = (int)np.x;
    y = (int)np.y;
}

void QScrollView::setStaticBackground(bool b)
{
    NSScrollView *view = (NSScrollView *)getView();
    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        [[view contentView] setCopiesOnScroll: !b];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QScrollView::resizeEvent(QResizeEvent *)
{
}

void QScrollView::ensureVisible(int x, int y)
{
    KWQ_BLOCK_NS_EXCEPTIONS;
    [getDocumentView() scrollRectToVisible:NSMakeRect(x, y, 0, 0)];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QScrollView::ensureVisible(int x, int y, int w, int h)
{
    KWQ_BLOCK_NS_EXCEPTIONS;
    [getDocumentView() scrollRectToVisible:NSMakeRect(x, y, w, h)];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

NSView *QScrollView::getDocumentView() const
{
    id view = getView();
    volatile NSView * volatile result = nil;

    KWQ_BLOCK_NS_EXCEPTIONS;
    if ([view respondsToSelector:@selector(documentView)]) 
	result = [view documentView];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
    
    return (NSView *)result;
}
