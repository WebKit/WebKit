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

#ifndef QPALETTE_H_
#define QPALETTE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qcolor.h>
#include <qbrush.h>

// class QColorGroup ===========================================================

class QColorGroup {
public:

    // typedefs ----------------------------------------------------------------

    enum ColorRole { 
        Foreground, 
        Shadow, 
        Light, 
        Mid, 
        Midlight, 
        Dark, 
        Base, 
        ButtonText, 
        Button, 
        Background, 
        Text 
    };

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------

    QColorGroup();
    QColorGroup(const QColorGroup &);
    ~QColorGroup();

    // member functions --------------------------------------------------------

    const QBrush &brush(ColorRole) const;

    const QColor &color(ColorRole) const;
    void setColor(ColorRole, const QColor &);

    const QColor &foreground() const;
    const QColor &shadow() const;
    const QColor &light() const;
    const QColor &midlight() const;
    const QColor &dark() const;
    const QColor &base() const;
    const QColor &buttonText() const;
    const QColor &button() const;
    const QColor &text() const;
    const QColor &background() const;

    // operators ---------------------------------------------------------------

    QColorGroup &operator=(const QColorGroup &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QColorGroup ========================================================


// class QPalette ==============================================================

class QPalette {
public:

    // typedefs ----------------------------------------------------------------
 
    // enums -------------------------------------------------------------------

    enum ColorGroup { Active, Inactive, Disabled };

    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------

    QPalette();
    QPalette(const QPalette &);
    ~QPalette();

    // member functions --------------------------------------------------------

    void setColor(ColorGroup, QColorGroup::ColorRole role, const QColor &color);

    const QColorGroup &active() const;
    const QColorGroup &inactive() const;
    const QColorGroup &disabled() const;
    const QColorGroup &normal() const;

    // operators ---------------------------------------------------------------

    QPalette &operator=(const QPalette &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QPalette ===========================================================

#endif
