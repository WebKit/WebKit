/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <qscrollview.h>

#include <kwqdebug.h>

/*
    This class implementation does NOT actually emulate the Qt QScrollView.
    Instead out WebPageView, like any other NSView can be set as the document
    to a standard NSScrollView.

    We do implement the placeWidget() function to essentially addSubview views
    onto this view.
*/

QScrollView::QScrollView(QWidget *parent=0, const char *name=0, WFlags f=0)
{
    _logNeverImplemented();
}


QScrollView::~QScrollView()
{
    _logNeverImplemented();
}


QWidget* QScrollView::viewport() const
{
    return (QWidget *)this;
}


int QScrollView::visibleWidth() const
{
    NSScrollView *scrollView = [[getView() superview] superview];
    int visibleWidth;
    
    if (scrollView != nil && [scrollView isKindOfClass: [NSScrollView class]])
        visibleWidth = (int)([scrollView documentVisibleRect].size.width);
    else
        visibleWidth = (int)([getView() bounds].size.width);
    return visibleWidth;
}


int QScrollView::visibleHeight() const
{
    NSScrollView *scrollView = [[getView() superview] superview];
    int visibleHeight;
    
    if (scrollView != nil && [scrollView isKindOfClass: [NSScrollView class]])
        visibleHeight = (int)([scrollView documentVisibleRect].size.height);
    else
        visibleHeight = (int)([getView() bounds].size.height);
    return visibleHeight;
}


int QScrollView::contentsWidth() const
{
    NSRect bounds = [getView() bounds];
    return (int)bounds.size.width;
}


int QScrollView::contentsHeight() const
{
    NSRect bounds = [getView() bounds];
    return (int)bounds.size.height;
}


int QScrollView::contentsX() const
{
    return 0;
}


int QScrollView::contentsY() const
{
    return 0;
}


void QScrollView::scrollBy(int dx, int dy)
{
    _logNeverImplemented();
}


void QScrollView::setContentsPos(int x, int y)
{
    _logNeverImplemented();
}


QScrollBar *QScrollView::horizontalScrollBar() const
{
    _logNeverImplemented();
    return 0L;
}


QScrollBar *QScrollView::verticalScrollBar() const
{
    _logNeverImplemented();
    return 0L;
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
        child->move (x, y);
        
    if ([getView() isKindOfClass: NSClassFromString(@"NSScrollView")]){
        thisView = [(NSScrollView *)getView() documentView];
    }
    else {
        thisView = getView();
    }

    subView = child->getView();
    NSRect wFrame = [subView frame];

    if ([subView superview] == thisView){
        return;
    }
    
    [subView removeFromSuperview];
    
    KWQDEBUGLEVEL6 (KWQ_LOG_FRAMES, "Adding 0x%08x %s at (%d,%d) w %d h %d\n", subView, [[[subView class] className] cString], x, y, (int)wFrame.size.width, (int)wFrame.size.height);
    [thisView addSubview: subView];
}


void QScrollView::removeChild(QWidget* child)
{
    NSView *subView;

    subView = child->getView();
    [subView removeFromSuperview];
}

@interface IFWebView: NSObject
- (QWidget *)_widget;
- (void)setFrameSize: (NSSize)r;
@end

void QScrollView::resizeContents(int w, int h)
{
    KWQDEBUGLEVEL4 (KWQ_LOG_FRAMES, "0x%08x %s at w %d h %d\n", getView(), [[[getView() class] className] cString], w, h);
    //if ([nsview isKindOfClass: NSClassFromString(@"IFDynamicScrollBarsView")])
    if ([getView() isKindOfClass: NSClassFromString(@"NSScrollView")]){
        IFWebView *wview = [(NSScrollView *)getView() documentView];
        
        KWQDEBUGLEVEL4 (KWQ_LOG_FRAMES, "0x%08x %s at w %d h %d\n", wview, [[[wview class] className] cString], w, h);
        //w -= (int)[NSScroller scrollerWidth];
        //w -= 1;
        if (w < 0)
            w = 0;
        // Why isn't there a scollerHeight?
        //h -= (int)[NSScroller scrollerWidth];
        //h -= 1;
        if (h < 0)
            h = 0;
        [wview setFrameSize: NSMakeSize (w,h)];
    }
    else {
        resize (w, h);
    }
}


void QScrollView::updateContents(int x, int y, int w, int h)
{
    KWQDEBUGLEVEL6 (KWQ_LOG_FRAMES, "0x%08x %s at (%d,%d) w %d h %d\n", getView(), [[[getView() class] className] cString], x, y, w, h);
}

void QScrollView::updateContents(const QRect &rect)
{
    return updateContents(rect.x(), rect.y(), rect.width(), rect.height());
}


void QScrollView::repaintContents(int x, int y, int w, int h, bool erase=TRUE)
{
    KWQDEBUGLEVEL6 (KWQ_LOG_FRAMES, "0x%08x %s at (%d,%d) w %d h %d\n", getView(), [[[getView() class] className] cString], x, y, w, h);
}

QPoint QScrollView::contentsToViewport(const QPoint &)
{
    _logNeverImplemented();
    return QPoint();
}


void QScrollView::viewportToContents(int vx, int vy, int& x, int& y)
{
    NSPoint p = NSMakePoint (vx, vy);
    NSPoint np = [getView() convertPoint: NSMakePoint (vx, vy) fromView: nil];
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


QScrollView::QScrollView(const QScrollView &)
{
    _logNeverImplemented();
}


QScrollView &QScrollView::operator=(const QScrollView &)
{
    _logNeverImplemented();
    return *this;
}

