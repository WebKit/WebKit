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

#ifdef USING_BORROWED_QRECT

#include <_qrect.h>

#else

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
    QRect(QPoint p, QSize s);
    QRect(int, int, int, int);
    // QRect(const QRect &);
    // default copy constructor is fine

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

    QPoint topLeft() const;
    QSize size() const;
    void setWidth(int);
    void setHeight(int);
    QRect intersect(const QRect &) const;
    bool intersects(const QRect &) const;


    // operators ---------------------------------------------------------------

    /* Note: Trolltech seems to want operator= to be a bitwise copy
     * QRect &operator=(const QRect &);
     */

    QRect operator&(const QRect &) const;

#ifdef _KWQ_IOSTREAM_
    friend ostream &operator<<(ostream &, const QRect &);
#endif

// protected -------------------------------------------------------------------

// private ---------------------------------------------------------------------

private:
    QCOORD xp;
    QCOORD yp;
    QCOORD w;
    QCOORD h;

    friend bool operator==(const QRect &, const QRect &);
    friend bool operator!=(const QRect &, const QRect &);
}; // class QRect ==============================================================

// operators associated with QRect =============================================

bool operator==(const QRect &, const QRect &);
bool operator!=(const QRect &, const QRect &);

#endif

#endif
