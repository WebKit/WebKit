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

#import "KWQLogging.h"

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
    int visibleWidth;
    if ([view _KWQ_isScrollView]) {
        visibleWidth = (int)[view documentVisibleRect].size.width;
    } else {
        visibleWidth = (int)[view bounds].size.width;
    }

    return visibleWidth;
}

int QScrollView::visibleHeight() const
{
    NSScrollView *view = (NSScrollView *)getView();
    int visibleHeight;
    
    if ([view _KWQ_isScrollView]) {
        visibleHeight = (int)[view documentVisibleRect].size.height;
    } else {
        visibleHeight = (int)[view bounds].size.height;
    }
    
    return visibleHeight;
}

int QScrollView::contentsWidth() const
{
    NSView *docView, *view = getView();
    docView = getDocumentView();
    if (docView)
        return (int)[docView bounds].size.width;
    return (int)[view bounds].size.width;
}

int QScrollView::contentsHeight() const
{
    NSView *docView, *view = getView();
    docView = getDocumentView();
    if (docView)
        return (int)[docView bounds].size.height;
    return (int)[view bounds].size.height;
}

int QScrollView::contentsX() const
{
    NSView *view = getView();
    float vx;
    if ([view _KWQ_isScrollView]) {
        NSScrollView *sview = view;
        vx = (int)[sview documentVisibleRect].origin.x;
    } else {
        vx = (int)[view visibleRect].origin.x;
    }
    return (int)vx;
}

int QScrollView::contentsY() const
{
    NSView *view = getView();
    float vy;
    if ([view _KWQ_isScrollView]) {
        NSScrollView *sview = view;
        vy = (int)[sview documentVisibleRect].origin.y;
    } else {
        vy = (int)[view visibleRect].origin.y;
    }
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
    NSView *docView, *view = getView();    
    docView = getDocumentView();
    if (docView)
        view = docView;
        
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    [view scrollPoint: NSMakePoint(x,y)];
}

void QScrollView::setVScrollBarMode(ScrollBarMode)
{
    LOG(NotYetImplemented, "not yet implemented");
}

void QScrollView::setHScrollBarMode(ScrollBarMode)
{
    LOG(NotYetImplemented, "not yet implemented");
}

void QScrollView::addChild(QWidget* child, int x, int y)
{
    NSView *thisView, *thisDocView, *subview;

    ASSERT(child != this);
    
    child->move(x, y);
    
    thisView = getView();
    thisDocView = getDocumentView();
    if (thisDocView)
        thisView = thisDocView;

    subview = child->getOuterView();
    ASSERT(subview != thisView);
    if ([subview superview] == thisView) {
        return;
    }
    
    [subview removeFromSuperview];
    
    LOG(Frames, "Adding %p %@ at (%d,%d) w %d h %d\n", subview,
        [[subview class] className], x, y, (int)[subview frame].size.width, (int)[subview frame].size.height);

    [thisView addSubview:subview];
}

void QScrollView::removeChild(QWidget* child)
{
    [child->getOuterView() removeFromSuperview];
}

void QScrollView::resizeContents(int w, int h)
{
    LOG(Frames, "%p %@ at w %d h %d\n", getView(), [[getView() class] className], w, h);
    NSView *view = getView();
    if ([view _KWQ_isScrollView]){
        view = getDocumentView();
        
        LOG(Frames, "%p %@ at w %d h %d\n", view, [[view class] className], w, h);
        if (w < 0)
            w = 0;
        if (h < 0)
            h = 0;
        [view setFrameSize: NSMakeSize (w,h)];
    } else {
        resize (w, h);
    }
}

void QScrollView::updateContents(int x, int y, int w, int h, bool now)
{
    updateContents(QRect(x, y, w, h), now);
}

void QScrollView::updateContents(const QRect &rect, bool now)
{
    NSView *view = getView();

    if ([view _KWQ_isScrollView])
        view = getDocumentView();

    if (now)
        [view displayRect: rect];
    else
        [view setNeedsDisplayInRect:rect];
}

void QScrollView::repaintContents(int x, int y, int w, int h, bool erase)
{
    LOG(Frames, "%p %@ at (%d,%d) w %d h %d\n", getView(), [[getView() class] className], x, y, w, h);
}

QPoint QScrollView::contentsToViewport(const QPoint &p)
{
    int vx, vy;
    contentsToViewport(p.x(), p.y(), vx, vy);
    return QPoint(vx, vy);
}

void QScrollView::contentsToViewport(int x, int y, int& vx, int& vy)
{
    NSView *docView, *view = getView();    
     
    docView = getDocumentView();
    if (docView)
        view = docView;
        
    NSPoint np = [view convertPoint: NSMakePoint (x, y) toView: nil];
    
    vx = (int)np.x;
    vy = (int)np.y;
}

void QScrollView::viewportToContents(int vx, int vy, int& x, int& y)
{
    NSView *docView, *view = getView();    

    docView = getDocumentView();
    if (docView)
        view = docView;
        
    NSPoint np = [view convertPoint: NSMakePoint (vx, vy) fromView: nil];
    
    x = (int)np.x;
    y = (int)np.y;
}

void QScrollView::setStaticBackground(bool b)
{
    NSScrollView *view = (NSScrollView *)getView();
    if ([view _KWQ_isScrollView])
        [[view contentView] setCopiesOnScroll: !b];
}

void QScrollView::resizeEvent(QResizeEvent *)
{
}

void QScrollView::ensureVisible(int x, int y)
{
    [getDocumentView() scrollRectToVisible:NSMakeRect(x, y, 0, 0)];
}

void QScrollView::ensureVisible(int x, int y, int w, int h)
{
    [getDocumentView() scrollRectToVisible:NSMakeRect(x, y, w, h)];
}

NSView *QScrollView::getDocumentView() const
{
    id view = getView();
    return [view respondsToSelector:@selector(documentView)] ? [view documentView] : nil;
}
