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

#ifndef QTEXTSTREAM_H_
#define QTEXTSTREAM_H_

#include "qstring.h"

// class QTextStream ===========================================================

class QTextStream {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QTextStream();
    QTextStream(QByteArray, int);

    virtual ~QTextStream();       

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

     QTextStream &operator<<(char);
     QTextStream &operator<<(const char *);
     QTextStream &operator<<(const QCString &);
     QTextStream &operator<<(const QString &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QTextStream(const QTextStream &);
    QTextStream &operator=(const QTextStream &);

}; // class QTextStream ========================================================


// class QTextIStream ==========================================================

class QTextIStream : public QTextStream {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QTextIStream(QString *);

    virtual ~QTextIStream();       

    // member functions --------------------------------------------------------

    QString readLine();

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QTextIStream(const QTextIStream &);
    QTextIStream &operator=(const QTextIStream &);

}; // class QTextIStream =======================================================


// class QTextOStream ==========================================================

class QTextOStream : public QTextStream {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QTextOStream(QString *);
    QTextOStream(QByteArray);

    virtual ~QTextOStream();       

    // member functions --------------------------------------------------------

    QString readLine();

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QTextOStream(const QTextOStream &);
    QTextOStream &operator=(const QTextOStream &);

}; // class QTextOStream =======================================================

#endif
