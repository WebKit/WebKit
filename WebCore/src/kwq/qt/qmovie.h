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

#ifndef QMOVIE_H_
#define QMOVIE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "qobject.h"
#include "qasyncio.h"
#include "qpixmap.h"
#include "qimage.h"
#include "qrect.h"

#ifdef _KWQ_
class QMoviePrivate;
#endif

// class QMovie ================================================================

class QMovie {
public:

    // typedefs ----------------------------------------------------------------

    // enums -------------------------------------------------------------------

    enum Status { EndOfFrame, EndOfMovie };

    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QMovie();
    QMovie(QDataSource*, int bufsize=1024);
    QMovie(const QMovie &);
    ~QMovie();
     
    // member functions --------------------------------------------------------

    void unpause();
    void pause();
    void restart();
    bool finished();
    bool running();
    int frameNumber() const;

    const QRect& getValidRect() const;
    const QPixmap& framePixmap() const;
    const QImage& frameImage() const;

    void connectResize(QObject* receiver, const char *member);
    void connectUpdate(QObject* receiver, const char *member);
    void connectStatus(QObject* receiver, const char *member);
    
    void disconnectResize(QObject* receiver, const char *member=0);
    void disconnectUpdate(QObject* receiver, const char *member=0);
    void disconnectStatus(QObject* receiver, const char *member=0);

    // operators ---------------------------------------------------------------

    QMovie &operator=(const QMovie &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------
private:
#ifdef _KWQ_
    QMoviePrivate *d;
#endif

}; // class QMovie =============================================================

#endif
