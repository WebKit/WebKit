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

#ifndef QCSTRING_H_
#define QCSTRING_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// USING_BORROWED_QSTRING ======================================================

#ifdef USING_BORROWED_QSTRING
#include <_qcstring.h>
#else

// FIXME: does our implementation of QCString really need to inherit from
// QByteArray and QArray?

#include "qarray.h"

#include <iostream>
#include <string.h>

typedef QArray<char> QByteArray;

// class QCString ==============================================================

class QCString : public QByteArray {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QCString();
    QCString(int);
    QCString(const char *);
    QCString(const char *, uint);
    QCString(const QCString &);

    ~QCString() {}

    // member functions --------------------------------------------------------

    bool isEmpty() const;
    bool isNull() const;
    int find(const char *, int index=0, bool cs=TRUE) const;
    int contains(char, bool cs=TRUE) const;
    uint length() const;
    bool truncate(uint);
    QCString lower() const;
    QCString upper() const;
    QCString left(uint) const;
    QCString right(uint) const;
    QCString mid(uint, uint len=0xffffffff) const;

    // operators ---------------------------------------------------------------

    operator const char *() const;
    QCString &operator=(const QCString &);
    QCString &operator=(const char *);
    QCString &operator+=(const char *);
    QCString &operator+=(char);

#ifdef _KWQ_IOSTREAM_
    friend ostream &operator<<(ostream &, const QCString &);
#endif

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    bool resize(uint);

}; // class QCString ===========================================================


// operators associated with QCString ==========================================

bool operator==(const char *s1, const QCString &s2);
bool operator==(const QCString &s1, const char *s2);
bool operator!=(const char *s1, const QCString &s2);
bool operator!=(const QCString &s1, const char *s2);

#endif // USING_BORROWED_QSTRING

#endif
