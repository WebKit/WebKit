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

#ifndef QDATETIME_H_
#define QDATETIME_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// class QTime =================================================================

class QTime {
public:

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QTime();
    QTime(int, int);

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QTime() {}
#endif

    // member functions --------------------------------------------------------

    bool isNull() const;
    void start();
    int msec() const;
    int elapsed() const;
    int restart();

    // operators ---------------------------------------------------------------

    QTime &operator=(const QTime &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QTime(const QTime &);
#endif
    
}; // class QTime ==============================================================


// class QDate =================================================================

class QDate {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QDate(int y, int m, int d);

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QDate() {}
#endif

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

    QDate &operator=(const QDate &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QDate(const QDate &);
#endif

}; // class QDate ==============================================================


// class QDateTime =============================================================

class QDateTime {
public:

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    static QDateTime currentDateTime();

    // constructors, copy constructors, and destructors ------------------------

    QDateTime();
    QDateTime(QDate date, QTime time);
    QDateTime(const QDateTime &);

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QDateTime() {}
#endif

    // member functions --------------------------------------------------------

    int secsTo(const QDateTime &) const;
    QTime time() const;

    // operators ---------------------------------------------------------------

    // this is not declared in the code, although assignment of this type
    // is used in the code... i guess a pointer copy is what they want
    //
    //QDateTime &operator=(const QDateTime &);
    //

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QDateTime ==========================================================

#endif
