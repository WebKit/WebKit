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

#ifndef QREGION_H_
#define QREGION_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// USING_BORROWED_QREGION ======================================================

#ifdef USING_BORROWED_QREGION
#include <_qregion.h>
#include "qpoint.h"
#include "qimage.h"
#include "qrect.h"
#else

#include "qpoint.h"
#include "qimage.h"
#include "qrect.h"

// class QRegion ===============================================================

class QRegion {
public:

    // typedefs ----------------------------------------------------------------

    // NOTE: alphabetical order
    enum RegionType { Ellipse, Rectangle };

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QRegion();
    QRegion(const QRect &);
    QRegion(int, int, int, int, RegionType = Rectangle);
    QRegion(const QPointArray &);
    QRegion(const QRegion &);
    ~QRegion();

    // member functions --------------------------------------------------------

    QRegion intersect(const QRegion &) const;
    bool contains(const QPoint &) const;
    bool isNull() const;

    // operators ---------------------------------------------------------------

    QRegion &operator=(const QRegion &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QRegion ============================================================

#endif // USING_BORROWED_QREGION

#endif
