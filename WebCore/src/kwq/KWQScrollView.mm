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
    _logNeverImplemented();
}


int QScrollView::visibleHeight() const
{
    _logNeverImplemented();
}


int QScrollView::contentsWidth() const
{
    _logNeverImplemented();
}


int QScrollView::contentsHeight() const
{
    _logNeverImplemented();
}


int QScrollView::contentsX() const
{
    _logNeverImplemented();
}


int QScrollView::contentsY() const
{
    _logNeverImplemented();
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
}


QScrollBar *QScrollView::verticalScrollBar() const
{
    _logNeverImplemented();
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
    
    child->move (x, y);
    
    thisView = getView();
    subView = child->getView();
    [thisView addSubview: subView];
}


void QScrollView::removeChild(QWidget* child)
{
    NSView *subView;

    subView = child->getView();
    [subView removeFromSuperview];
}


void QScrollView::resizeContents(int w, int h)
{
    _logNeverImplemented();
}


void QScrollView::updateContents(int x, int y, int w, int h)
{
    _logNeverImplemented();
}


void QScrollView::repaintContents(int x, int y, int w, int h, bool erase=TRUE)
{
    _logNeverImplemented();
}

QPoint QScrollView::contentsToViewport(const QPoint &)
{
    _logNeverImplemented();
}


void QScrollView::viewportToContents(int vx, int vy, int& x, int& y)
{
    _logNeverImplemented();
}


void QScrollView::viewportWheelEvent(QWheelEvent *)
{
    _logNeverImplemented();
}


QWidget *QScrollView::clipper() const
{
    _logNeverImplemented();
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
}

