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

#ifndef QSIZE_H_
#define QSIZE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <KWQDef.h>

// class QSize =================================================================

class QSize {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QSize();
    QSize(int,int);
    QSize(const QSize &);

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QSize() {}
#endif

    // member functions --------------------------------------------------------

    bool isValid() const;
    int width() const;
    int height() const;
    void setWidth(int);
    void setHeight(int);
    QSize expandedTo(const QSize &) const;

#ifdef USING_BORROWED_QSIZE
    bool isNull() const;
    bool isEmpty() const;
    void transpose();
    QSize boundedTo(const QSize &) const;
#endif

    // operators ---------------------------------------------------------------

    /* Note: Trolltech seems to want operator= to be a bitwise copy
     * QSize &operator=(const QSize &);
     */

    friend QSize operator+(const QSize &, const QSize &);
    friend bool operator==(const QSize &, const QSize &);
    friend bool operator!=(const QSize &, const QSize &);

#ifdef USING_BORROWED_QSIZE
    QSize &operator+=(const QSize &);
    QSize &operator-=(const QSize &);
    QSize &operator*=(int);
    QSize &operator*=(double);
    QSize &operator/=(int);
    QSize &operator/=(double);

    friend QSize operator-(const QSize &, const QSize &);
    friend QSize operator*(const QSize &, int);
    friend QSize operator*(int, const QSize &);
    friend QSize operator*(const QSize &, double);
    friend QSize operator*(double, const QSize &);
    friend QSize operator/(const QSize &, int);
    friend QSize operator/(const QSize &, double);
#endif

#ifdef _KWQ_IOSTREAM_
    friend ostream &operator<<(ostream &, const QSize &);
#endif

// protected -------------------------------------------------------------------

// private ---------------------------------------------------------------------

    QCOORD w;
    QCOORD h;

}; // class QSize ==============================================================

#endif
