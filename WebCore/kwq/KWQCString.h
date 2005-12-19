/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include "KWQMemArray.h"
#include <string.h>

typedef QMemArray<char> QByteArray;

class QCString : public QByteArray {
public:
    QCString();
    QCString(int);
    QCString(const char *);
    QCString(const char *, uint);

    QCString &operator=(const char *);

    bool isEmpty() const;
    bool isNull() const { return data() == 0; }
    int find(const char *, int index=0, bool cs=true) const;
    int contains(char, bool cs=true) const;
    uint length() const;
    bool truncate(uint);
    QCString lower() const;
    QCString upper() const;
    QCString left(uint) const;
    QCString right(uint) const;
    QCString mid(uint, uint len=0xffffffff) const;

    QCString &append(char);
    QCString &append(const char *);
    QCString &replace(char, char);

    operator const char *() const { return data(); }
    QCString &operator+=(const char *s) { return append(s); }
    QCString &operator+=(char c) { return append(c); }

private:
    bool resize(uint);
};

bool operator==(const QCString &s1, const char *s2);
inline bool operator==(const char *s1, const QCString &s2) { return s2 == s1; }
inline bool operator!=(const QCString &s1, const char *s2) { return !(s1 == s2); }
inline bool operator!=(const char *s1, const QCString &s2) { return !(s1 == s2); }

typedef QCString Q3CString;

#endif
