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
    It does provide an implementation that khtml will use to interact with
    WebKit's IFWebView documentView and our NSScrollView subclass.

    QScrollView's view is a NSScrollView (or subclass of NSScrollView) in most
    cases (except for provisional widgets).  That scrollview is a subview of an
    IFWebView.  The IFWebView's documentView will also be the scroll view's 
    documentView.
    
    The IFWebView's size is the frame size.  The IFWebView's documentView
    corresponds to the frame content size.  The scrollview itself is autosized to the
    IFWebView's size (see QWidget::resize).
*/

@interface NSView (IFExtensions)
- (BOOL)_IF_isScrollView;
- (NSView *)_IF_getDocumentView;
@end


@implementation NSView (IFExtensions)
- (NSView *)_IF_getDocumentView
{
    if ([self respondsToSelector: @selector(documentView)]){
        NSScrollView *sv = (NSScrollView *)self; // Compile complains about in-line cast.
        return [sv documentView];
    }
    return nil;
}


- (BOOL)_IF_isScrollView
{
    if([self isKindOfClass: [NSScrollView class]])
        return YES;
    return NO;
}
@end


QScrollView::QScrollView(QWidget *parent, const char *name, int f)
    : QFrame(parent)
{
}


QWidget* QScrollView::viewport() const
{
    return const_cast<QScrollView *>(this);
}


int QScrollView::visibleWidth() const
{
    NSScrollView *view = (NSScrollView *)getView();
    int visibleWidth;
    if ([view _IF_isScrollView]){
        visibleWidth = (int)[view documentVisibleRect].size.width;
    }
    else
        visibleWidth = (int)[view bounds].size.width;

    return visibleWidth;
}


int QScrollView::visibleHeight() const
{
    NSScrollView *view = (NSScrollView *)getView();
    int visibleHeight;
    
    if ([view _IF_isScrollView]){
        visibleHeight = (int)[view documentVisibleRect].size.height;
    }
    else
        visibleHeight = (int)[view bounds].size.height;
        
    return visibleHeight;
}


int QScrollView::contentsWidth() const
{
    NSView *docView, *view = getView();
    docView = [view _IF_getDocumentView];
    if (docView)
        return (int)[docView bounds].size.width;
    return (int)[view bounds].size.width;
}


int QScrollView::contentsHeight() const
{
    NSView *docView, *view = getView();
    docView = [view _IF_getDocumentView];
    if (docView)
        return (int)[docView bounds].size.height;
    return (int)[view bounds].size.height;
}

int QScrollView::contentsX() const
{
    NSView *docView, *view = getView();
    docView = [view _IF_getDocumentView];
    if (docView)
        return (int)[docView bounds].origin.x;
    return 0;
}

int QScrollView::contentsY() const
{
    NSView *docView, *view = getView();
    docView = [view _IF_getDocumentView];
    if (docView)
        return (int)[docView bounds].origin.y;
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
    NSView *docView, *view = getView();    
    docView = [view _IF_getDocumentView];
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
    _logNeverImplemented();
}

void QScrollView::setHScrollBarMode(ScrollBarMode)
{
    _logNeverImplemented();
}

void QScrollView::addChild(QWidget* child, int x, int y)
{
    NSView *thisView, *thisDocView, *subView;

    KWQ_ASSERT(child != this);

    if (child->x() != x || child->y() != y)
        child->move(x, y);
    
    thisView = getView();
    thisDocView = [thisView _IF_getDocumentView];
    if (thisDocView)
        thisView = thisDocView;

    subView = child->getView();
    if ([subView _IF_isScrollView]) {
        subView = [subView superview];
    }
    
    KWQ_ASSERT(subView != thisView);

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
    NSView *view = getView();
    if ([view _IF_isScrollView]){
        view = [view _IF_getDocumentView];
        
        KWQDEBUGLEVEL (KWQ_LOG_FRAMES, "%p %s at w %d h %d\n", view, [[[view class] className] cString], w, h);
        if (w < 0)
            w = 0;
        if (h < 0)
            h = 0;
        [view setFrameSize: NSMakeSize (w,h)];
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
    NSView *docView, *view = getView();    
     
    docView = [view _IF_getDocumentView];
    if (docView)
        view = docView;
        
    NSPoint np = [view convertPoint: NSMakePoint (x, y) toView: nil];
    
    vx = (int)np.x;
    vy = (int)np.y;
}

void QScrollView::viewportToContents(int vx, int vy, int& x, int& y)
{
    NSView *docView, *view = getView();    

    docView = [view _IF_getDocumentView];
    if (docView)
        view = docView;
        
    NSPoint np = [view convertPoint: NSMakePoint (vx, vy) fromView: nil];
    
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
