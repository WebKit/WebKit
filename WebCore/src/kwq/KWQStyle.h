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

#ifndef KWQSTYLE_H_
#define KWQSTYLE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qobject.h>
#include <qsize.h>

// class QStyle ================================================================

class QStyle : public QObject {
public:
    
    typedef enum { 
	PM_IndicatorWidth,
	PM_IndicatorHeight,
	PM_ExclusiveIndicatorWidth,
	PM_ExclusiveIndicatorHeight,
        PM_DefaultFrameWidth
    } PixelMetric;

    typedef enum {
	SH_GUIStyle
    } StyleHint;

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------

    QStyle();
    virtual ~QStyle();

    // member functions --------------------------------------------------------

    GUIStyle guiStyle() const;
    virtual QSize indicatorSize() { return QSize(22,22); }	// FIXME!  Shouldn't be hardcoded.
    virtual QSize exclusiveIndicatorSize() { return QSize(22,22); } // FIXME!  Shouldn't be hardcoded.
    virtual int pixelMetric(int metric) const { return 22; } // FIXME!  Shouldn't be hardcoded.
    virtual int styleHint(StyleHint hint) const { return 0; }

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // note that these are "standard" (no pendantic stuff needed)
    QStyle(const QStyle &);
    QStyle &operator=(const QStyle &);
    
}; // class QStyle =============================================================

#endif
