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

#ifndef QSTRING_H_
#define QSTRING_H_

#include <kwq.h>
#include <string.h>

#include "qcstring.h"
#include "qregexp.h"

class QString;

class QChar {
public:
    QChar();
    QChar(char);
    QChar(const QChar &);
    QChar lower() const;
    QChar upper() const;
    char latin1() const;
    bool isNull() const;
    bool isDigit() const;
    bool isSpace() const;
    bool isLetterOrNumber() const;
    friend inline int operator==(QChar, char);
    friend inline int operator==(QChar, QChar);
    friend inline int operator!=(QChar, QChar);
    friend inline int operator!=(char, QChar);
    friend inline int operator!=(QChar, char);
    operator char() const;
    ushort unicode() const;

    static const QChar null;

    enum Direction {
        // NOTE: alphabetical order
        DirAL, DirAN, DirB, DirBN, DirCS, DirEN, DirES, DirET, DirL, DirLRE,
        DirLRO, DirNSM, DirON, DirPDF, DirR, DirRLE, DirRLO, DirS, DirWS
    };
};

class QString {
public:

    static QString fromLatin1(const char*, int len=-1);

    QString();
    QString(const QChar *, uint);
    QString(const char *s);
    int toInt() const;
    int toInt(bool *) const;
    bool isNull() const;
    const QChar *unicode() const;
    bool contains(const char *s, bool b) const;
    uint length() const;
    QString &sprintf(const char *format, ...);
    QString lower() const;
    QString stripWhiteSpace() const;
    QString simplifyWhiteSpace() const;
    bool isEmpty() const;
    int contains(const char *) const;
    int find(char c, int index=0) const;
    int find(const char *s, int index=0, bool b=0) const;
    int find(const QRegExp &regexp, int index=0, bool b=0) const;
    int findRev(char c, int index=0) const;
    int findRev(const char *s, int index=0) const;
    QString left(uint len) const;
    QString &remove(uint index, uint len);
    QString &replace(const QRegExp &, const QString &);
    void truncate(uint pos);

    QString arg(const QString& a, int fieldwidth=0) const;

    QString mid(int index, int len=0xffffffff) const;
    void fill(QChar c, int len = -1);

    float toFloat(bool *b=0) const;

    const char* latin1() const;
    const char *ascii() const;
    // FIXME: is there a standard parameter type for overloaded operators?
    QChar operator[](int) const;
    QString &operator+(QChar);
    QString &operator+(const QString &);
    QString &operator+=(char);
    QString &operator+=(QChar);
    QString &operator+=(const QString &);

    QString &append(const char *);
    QString &append(const QString &);

    QCString utf8() const;

    static const QString null;

    static QString number(long n, int base=10);

    // FIXME: bogus constructor hack for "conversion from int to non-scalar
    // type" error in "Node::toHTML()" function in "dom/dom_node.cpp"
    QString(int);
};

QString &operator+(const char *s1, const QString &s2);
QString &operator+(QChar c1, const QString &s2);
bool operator!=(const QString &s1, QChar c2);
bool operator!=(const QString &s1, const QString &s2);
bool operator!=(const QString &s1, const char *s2);
bool operator!=(const char *s1, const QString &s2);

class QConstString {
public:
    QConstString(QChar *, uint);
    const QString string() const;
};

#endif
