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

#ifndef QSCROLLVIEW_H_
#define QSCROLLVIEW_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <KWQScrollBar.h>
#include <KWQFrame.h>
#include "qwidget.h"

// class QScrollView ===========================================================

class QScrollView : public QFrame {
public:

    // typedefs ----------------------------------------------------------------

    // enums -------------------------------------------------------------------

    // NOTE: alphabetical order
    enum ScrollBarMode { AlwaysOff, AlwaysOn, Auto };

    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QScrollView(QWidget *parent=0, const char *name=0, WFlags f=0);
    ~QScrollView();

    // member functions --------------------------------------------------------

    QWidget* viewport() const;
    int visibleWidth() const;
    int visibleHeight() const;
    int contentsWidth() const;
    int contentsHeight() const;
    int contentsX() const;
    int contentsY() const;
    void scrollBy(int dx, int dy);

    virtual void setContentsPos(int x, int y);

    virtual void setVScrollBarMode(ScrollBarMode);
    virtual void setHScrollBarMode(ScrollBarMode);

    virtual void addChild(QWidget* child, int x=0, int y=0);
    void removeChild(QWidget* child);

    virtual void resizeContents(int w, int h);
    void updateContents(int x, int y, int w, int h);
    void updateContents(const QRect &r);
    void repaintContents(int x, int y, int w, int h, bool erase=TRUE);
    QPoint contentsToViewport(const QPoint &);
    void viewportToContents(int vx, int vy, int& x, int& y);

    virtual void viewportWheelEvent(QWheelEvent *);

    QWidget *clipper() const;
    void enableClipper(bool);

    void setStaticBackground(bool);

    void resizeEvent(QResizeEvent *);

    void ensureVisible(int,int);
    void ensureVisible(int,int,int,int);

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    // note that these are "standard" (no pendantic stuff needed)
    QScrollView(const QScrollView &);
    QScrollView &operator=(const QScrollView &);

}; // class QScrollView ========================================================

#endif
