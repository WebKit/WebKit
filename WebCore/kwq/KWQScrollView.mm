/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import <qscrollview.h>

#import <kwqdebug.h>

/*
    This class implementation does NOT actually emulate the Qt QScrollView.
    Instead our WebPageView, like any other NSView can be set as the document
    to a standard NSScrollView.

    We do implement the placeWidget() function to essentially addSubview views
    onto this view.
*/

QScrollView::QScrollView(QWidget *parent, const char *name, int f)
    : QFrame(parent)
{
}

static NSView *getDocumentView(NSView *view)
{
    if ([view respondsToSelector:@selector(documentView)]) {
        id untypedView = view;
        view = [untypedView documentView];
    }
    return view;
}

static NSView *getDocumentView(const QWidget *widget)
{
    return getDocumentView(widget->getView());
}

NSView *QScrollView::getDocumentView() const
{
    return ::getDocumentView(this);
}

QWidget* QScrollView::viewport() const
{
    return const_cast<QScrollView *>(this);
}

int QScrollView::visibleWidth() const
{
#if 0
    id view = getView();
    NSScrollView *scrollView = [[view superview] superview];
    int visibleWidth;
    
    if (scrollView != nil && [scrollView isKindOfClass: [NSScrollView class]])
        visibleWidth = (int)[scrollView documentVisibleRect].size.width;
    else
        visibleWidth = (int)[view bounds].size.width;
    return visibleWidth;
#else
    NSScrollView *view = (NSScrollView *)getView();
    int visibleWidth;
    if (view != nil && [view isKindOfClass:[NSScrollView class]]){
        visibleWidth = (int)[view documentVisibleRect].size.width;
    }
    else
        visibleWidth = (int)[view bounds].size.width;
    if (visibleWidth <= 0)
        visibleWidth = 200;
    return visibleWidth;
#endif
}


int QScrollView::visibleHeight() const
{
#if 0
    id view = getView();
    NSScrollView *scrollView = [[view superview] superview];
    int visibleHeight;
    
    if (scrollView != nil && [scrollView isKindOfClass: [NSScrollView class]]){
        visibleHeight = (int)[scrollView documentVisibleRect].size.height;
    }
    else
        visibleHeight = (int)[view bounds].size.height;
    return visibleHeight;
#else
    NSScrollView *view = (NSScrollView *)getView();
    int visibleHeight;
    
    if (view != nil && [view isKindOfClass:[NSScrollView class]]){
        visibleHeight = (int)[view documentVisibleRect].size.height;
    }
    else
        visibleHeight = (int)[view bounds].size.height;
        
    if (visibleHeight <= 0)
        visibleHeight = 200;
    return visibleHeight;
#endif
}


int QScrollView::contentsWidth() const
{
    return (int)[getDocumentView() bounds].size.width;
}


int QScrollView::contentsHeight() const
{
    return (int)[getDocumentView() bounds].size.height;
}

int QScrollView::contentsX() const
{
    NSScrollView *view = (NSScrollView *)getView();
    if ([view respondsToSelector:@selector(documentView)])
        return (int)[[view documentView] bounds].origin.x;
    return 0;
}

int QScrollView::contentsY() const
{
    NSScrollView *view = (NSScrollView *)getView();
    if ([view respondsToSelector:@selector(documentView)])
        return (int)[[view documentView] bounds].origin.y;
    return 0;
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
    _logNeverImplemented();
}

void QScrollView::setContentsPos(int x, int y)
{
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    [getDocumentView() scrollPoint: NSMakePoint(x,y)];
}

void QScrollView::setVScrollBarMode(ScrollBarMode)
{
    _logNeverImplemented();
}

void QScrollView::setHScrollBarMode(ScrollBarMode)
{
    _logNeverImplemented();
}

void QScrollView::addChild(QWidget* child, int x, int y)
{
    NSView *thisView, *subView;

    if (child->x() != x || child->y() != y)
        child->move(x, y);
        
    thisView = getDocumentView();
    subView = ::getDocumentView(child);

    if ([subView superview] == thisView) {
        return;
    }
    
    [subView removeFromSuperview];
    
    KWQDEBUGLEVEL (KWQ_LOG_FRAMES, "Adding %p %s at (%d,%d) w %d h %d\n", subView, [[[subView class] className] cString], x, y, (int)[subView frame].size.width, (int)[subView frame].size.height);
    [thisView addSubview: subView];
}

void QScrollView::removeChild(QWidget* child)
{
    [child->getView() removeFromSuperview];
}

void QScrollView::resizeContents(int w, int h)
{
    KWQDEBUGLEVEL (KWQ_LOG_FRAMES, "%p %s at w %d h %d\n", getView(), [[[getView() class] className] cString], w, h);
    if ([getView() isKindOfClass:[NSScrollView class]]){
        NSView *wview = getDocumentView();
        
        KWQDEBUGLEVEL (KWQ_LOG_FRAMES, "%p %s at w %d h %d\n", wview, [[[wview class] className] cString], w, h);
        //w -= (int)[NSScroller scrollerWidth];
        //w -= 1;
        if (w < 0)
            w = 0;
        // Why isn't there a scrollerHeight?
        //h -= (int)[NSScroller scrollerWidth];
        //h -= 1;
        if (h < 0)
            h = 0;
        [wview setFrameSize:NSMakeSize(w,h)];
    }
    else {
        resize (w, h);
    }
}

void QScrollView::updateContents(int x, int y, int w, int h)
{
    KWQDEBUGLEVEL (KWQ_LOG_FRAMES, "%p %s at (%d,%d) w %d h %d\n", getView(), [[[getView() class] className] cString], x, y, w, h);
}

void QScrollView::updateContents(const QRect &rect)
{
    return updateContents(rect.x(), rect.y(), rect.width(), rect.height());
}


void QScrollView::repaintContents(int x, int y, int w, int h, bool erase)
{
    KWQDEBUGLEVEL (KWQ_LOG_FRAMES, "%p %s at (%d,%d) w %d h %d\n", getView(), [[[getView() class] className] cString], x, y, w, h);
}

QPoint QScrollView::contentsToViewport(const QPoint &p)
{
    int vx, vy;
    contentsToViewport(p.x(), p.y(), vx, vy);
    return QPoint(vx, vy);
}

void QScrollView::contentsToViewport(int x, int y, int& vx, int& vy)
{
    NSPoint np = [getDocumentView() convertPoint: NSMakePoint (x, y) toView: nil];
    
    vx = (int)np.x;
    vy = (int)np.y;
}

void QScrollView::viewportToContents(int vx, int vy, int& x, int& y)
{
    NSPoint np = [getDocumentView() convertPoint: NSMakePoint (vx, vy) fromView: nil];
    
    x = (int)np.x;
    y = (int)np.y;
}

void QScrollView::viewportWheelEvent(QWheelEvent *)
{
    _logNeverImplemented();
}

QWidget *QScrollView::clipper() const
{
    _logNeverImplemented();
    return (QWidget *)this;
}

void QScrollView::enableClipper(bool)
{
    _logNeverImplemented();
}

void QScrollView::setStaticBackground(bool)
{
    _logNeverImplemented();
}

void QScrollView::resizeEvent(QResizeEvent *)
{
    _logNeverImplemented();
}

void QScrollView::ensureVisible(int,int)
{
    _logNeverImplemented();
}

void QScrollView::ensureVisible(int,int,int,int)
{
    _logNeverImplemented();
}
