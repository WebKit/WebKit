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

// class QTime =================================================================

class QTime {
public:

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QTime();
    QTime(int, int);
    QTime(const QTime &);

    // member functions --------------------------------------------------------

    bool isNull() const;
    void start();
    int msec() const;
    int elapsed() const;

    // operators ---------------------------------------------------------------

    QTime &operator=(const QTime &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

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

    QDate(const QDate &);

    // member functions --------------------------------------------------------

    // operators ---------------------------------------------------------------

    QDate &operator=(const QDate &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

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

    // member functions --------------------------------------------------------

    int secsTo(const QDateTime &) const;
    QTime time() const;

    // operators ---------------------------------------------------------------

    QDateTime &operator=(const QDateTime &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QDateTime ==========================================================

#endif
