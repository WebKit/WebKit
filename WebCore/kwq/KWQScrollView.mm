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

QScrollView::QScrollView(QWidget *parent=0, const char *name=0, WFlags f=0)
{
}


QScrollView::~QScrollView()
{
}


QWidget* QScrollView::viewport() const
{
}


int QScrollView::visibleWidth() const
{
}


int QScrollView::visibleHeight() const
{
}


int QScrollView::contentsWidth() const
{
}


int QScrollView::contentsHeight() const
{
}


int QScrollView::contentsX() const
{
}


int QScrollView::contentsY() const
{
}


void QScrollView::scrollBy(int dx, int dy)
{
}


void QScrollView::setContentsPos(int x, int y)
{
}


QScrollBar *QScrollView::horizontalScrollBar() const
{
}


QScrollBar *QScrollView::verticalScrollBar() const
{
}


void QScrollView::setVScrollBarMode(ScrollBarMode)
{
}


void QScrollView::setHScrollBarMode(ScrollBarMode)
{
}


void QScrollView::addChild(QWidget* child, int x=0, int y=0)
{
}


void QScrollView::removeChild(QWidget* child)
{
}


void QScrollView::resizeContents(int w, int h)
{
}


void QScrollView::updateContents(int x, int y, int w, int h)
{
}


void QScrollView::repaintContents(int x, int y, int w, int h, bool erase=TRUE)
{
}

QPoint QScrollView::contentsToViewport(const QPoint &)
{
}


void QScrollView::viewportToContents(int vx, int vy, int& x, int& y)
{
}


void QScrollView::viewportWheelEvent(QWheelEvent *)
{
}


QWidget *QScrollView::clipper() const
{
}


void QScrollView::enableClipper(bool)
{
}


void QScrollView::setStaticBackground(bool)
{
}


void QScrollView::resizeEvent(QResizeEvent *)
{
}


void QScrollView::ensureVisible(int,int)
{
}


void QScrollView::ensureVisible(int,int,int,int)
{
}


QScrollView::QScrollView(const QScrollView &)
{
}


QScrollView &QScrollView::operator=(const QScrollView &)
{
}

