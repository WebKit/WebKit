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

// FIXME: does our implementation of QCString really need to inherit from
// QByteArray and QArray?

// added to help in compilation of khtml/khtml_part.h:811
#include "qarray.h"

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
    QCString(const QCString&);

    // member functions --------------------------------------------------------

    bool isEmpty() const;
    bool isNull() const;
    int find(const char *str, int index=0, bool cs=TRUE) const;
    int contains(char) const;
    uint length() const;
    bool truncate(uint);
    QCString lower() const;
    QCString mid(uint index, uint len=0xffffffff) const;

    // operators ---------------------------------------------------------------

    operator const char *() const;
    QCString &operator=(const QCString&);
    QCString &operator=(const char *);
    QCString &operator+=(const char *);
    QCString &operator+=(const QCString&);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QCString ===========================================================


// operators associated with QCString ==========================================

bool operator==(const char *, const QCString &);
bool operator==(const QCString &, const char *);
bool operator!=(const char *, const QCString &);
bool operator!=(const QCString &, const char *);

#endif
