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

#ifndef QBRUSH_H_
#define QBRUSH_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "qcolor.h"
#include "qnamespace.h"

class QBrushPrivate;

// class QBrush ================================================================

class QBrush : public Qt {
friend class QPainter;
public: 

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------

    QBrush::QBrush(const QColor &c = Qt::black, BrushStyle style = SolidPattern);

    // Defaults are fine
    // QBrush(const QBrush &);
    // QBrush &operator=(const QBrush &);
    // ~QBrush();
 
    // member functions --------------------------------------------------------
    
    const QColor &color() const;
    void setColor(const QColor &);
    BrushStyle style() const;
    void setStyle(BrushStyle);
    
    // operators ---------------------------------------------------------------
    
    bool operator==(const QBrush &) const;
    bool operator!=(const QBrush &) const;

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------
private:
    QColor brushColor;
    BrushStyle brushStyle;
}; // class QBrush =============================================================

#endif
