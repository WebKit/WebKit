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

#ifndef QPEN_H_
#define QPEN_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qnamespace.h>
#include <qcolor.h>

class QPainter;
class QPenPrivate;

// class QPen ==================================================================

class QPen : public Qt {
friend class QPainter;
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QPen();
    QPen(const QColor &c, uint w=0, PenStyle ps=SolidLine);
    QPen(const QPen &pen);
    ~QPen();

    // member functions --------------------------------------------------------

    const QColor &color() const;
    uint width() const;
    PenStyle style() const;

    void setColor(const QColor &);
    void setWidth(uint);
    void setStyle(PenStyle);

    // operators ---------------------------------------------------------------

    bool operator==(const QPen &) const;
    bool operator!=(const QPen &) const;
    
    QPen &operator=(const QPen &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------
private:
    QPen copy()  const;
    void detach();
    void init(const QColor &, uint, uint);
    
    struct QPenData : public QShared {
        PenStyle  style;
        uint      width;
        QColor    color;
        Q_UINT16  linest;
    } *data;
 
}; // class QPen ===============================================================

#endif
