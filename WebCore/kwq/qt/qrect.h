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

#ifndef QRECT_H_
#define QRECT_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <KWQDef.h>

#include "qsize.h"
#include "qpoint.h"

// class QRect =================================================================

class QRect {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QRect();
    QRect(int, int, int, int);
    QRect(const QRect &);

#ifdef _KWQ_COMPLETE_
    QRect(const QPoint &, const QPoint &);
    QRect(const QPoint &, const QSize &);
#endif
    
// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QRect() {}
#endif

    // member functions --------------------------------------------------------

    bool isNull() const;
    bool isValid() const;

    int x() const;
    int y() const;
    int left() const;
    int top() const;
    int right() const;
    int bottom() const;
    int width() const;
    int height() const;

    QSize size() const;
    void setWidth(int);
    void setHeight(int);
    QRect intersect(const QRect &) const;
    bool intersects(const QRect &) const;

#ifdef _KWQ_COMPLETE_
    bool isEmpty() const;
    QRect normalize() const;

    void setLeft(int);
    void setTop(int);
    void setRight(int);
    void setBottom(int);
    void setX(int);
    void setY(int);

    QPoint topLeft() const;
    QPoint bottomRight() const;
    QPoint topRight() const;
    QPoint bottomLeft() const;
    QPoint center() const;

    void rect(int *, int *, int *, int *) const;
    void coords(int *, int *, int *, int *) const;

    void setSize(const QSize &);
    void setRect(int, int, int, int);
    void setCoords(int, int, int, int);

    void moveTopLeft(const QPoint &);
    void moveBottomRight(const QPoint &);
    void moveTopRight(const QPoint &);
    void moveBottomLeft(const QPoint &);
    void moveCenter(const QPoint &);
    void moveBy(int, int);

    bool contains(const QPoint &, bool proper=FALSE) const;
    bool contains(int, int, bool proper=FALSE) const;
    bool contains(const QRect &, bool proper=FALSE) const;
    QRect unite(const QRect &) const;
#endif // _KWQ_COMPLETE_

    // operators ---------------------------------------------------------------

    /* Note: Trolltech seems to want operator= to be a bitwise copy
     * QRect &operator=(const QRect &);
     */

#ifdef _KWQ_COMPLETE_
    friend bool operator==(const QRect &, const QRect &);
    friend bool operator!=(const QRect &, const QRect &);

    QRect operator|(const QRect &) const;
    QRect operator&(const QRect &) const;
    QRect& operator|=(const QRect &);
    QRect& operator&=(const QRect &);
#endif

#ifdef _KWQ_IOSTREAM_
    friend ostream &operator<<(ostream &, const QRect &);
#endif

// protected -------------------------------------------------------------------

// private ---------------------------------------------------------------------

    QCOORD x1;
    QCOORD x2;
    QCOORD y1;
    QCOORD y2;

}; // class QRect ==============================================================

// operators associated with QRect =============================================

bool operator==(const QRect &, const QRect &);
bool operator!=(const QRect &, const QRect &);

#endif
