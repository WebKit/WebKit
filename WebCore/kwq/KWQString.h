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

#include "qcstring.h"

class QString;
class QRegExp;

class QChar {
public:
    enum Direction {
        // NOTE: alphabetical order
        DirAL, DirAN, DirB, DirBN, DirCS, DirEN, DirES, DirET, DirL, DirLRE,
        DirLRO, DirNSM, DirON, DirPDF, DirR, DirRLE, DirRLO, DirS, DirWS
    };

    QChar();
    QChar(char);
    QChar(uchar);
    QChar(const QChar &);
    QChar(ushort);
    QChar(short);
    QChar(uint);
    QChar(int);

    QChar lower() const;
    QChar upper() const;
    char latin1() const;
    bool isNull() const;
    bool isDigit() const;
    bool isSpace() const;
    bool isLetter() const;
    bool isLetterOrNumber() const;
    bool isPunct() const;
    uchar cell() const;
    uchar row() const;
    Direction direction() const;
    bool mirrored() const;
    QChar mirroredChar() const;

    friend inline int operator==(char, QChar);
    friend inline int operator==(QChar, char);
    friend inline int operator==(QChar, QChar);
    friend inline int operator!=(QChar, QChar);
    friend inline int operator!=(char, QChar);
    friend inline int operator!=(QChar, char);
    operator char() const;
    ushort unicode() const;

    static const QChar null;

};

class QString {
public:
    static QString fromLatin1(const char*, int len = -1);

    QString();
    QString(QChar);
    QString(const QString&);
    QString(const QByteArray&);
    QString(const QChar *, uint);
    QString(const char *);

    QString &operator=(const QString&);
    QString &operator=(const char *);
    QString &operator=(const QCString&);
    QString &operator=(QChar);
    QString &operator=(char);

    int toInt() const;
    int toInt(bool *, int base=10) const;
    uint toUInt(bool *ok = 0, int base = 10) const;
    long toLong(bool *ok = 0, int base = 10) const;
    QString &setNum(int, int base = 10 );
    bool isNull() const;
    const QChar *unicode() const;
    int contains(const char *, bool) const;
    int contains(const char *) const;
    int contains(char) const;
    uint length() const;
    QString &sprintf(const char *, ...);
    QString lower() const;
    QString stripWhiteSpace() const;
    QString simplifyWhiteSpace() const;
    bool isEmpty() const;
    int find(char, int index=0) const;
    int find(const char *, int index = 0, bool b = 0) const;
    int find(const QString &, int index = 0, bool b = 0) const;
    int find(const QRegExp &, int index = 0, bool b = 0) const;
    int findRev(char, int index = 0) const;
    int findRev(const char *, int index = 0) const;
    QString &remove(uint, uint);
    QString &replace(const QRegExp &, const QString &);
    QString &insert(uint, char);
    void truncate(uint pos);
    bool startsWith(const QString&) const;

    QString arg (int &);
    QString arg(int a, int fieldwidth=0, int base=10) const;
    QString arg(const QString&, int fieldwidth = 0) const;

    QString left(uint) const;
    QString right(uint) const;
    QString mid(int, int len = 0xffffffff) const;
    void fill(QChar, int len = -1);

    float toFloat(bool *b = 0) const;

    const char* latin1() const;
    const char *ascii() const;
    // FIXME: is there a standard parameter type for overloaded operators?
    bool operator!() const;
    operator const char *() const;
    QChar operator[](int) const;
    QString &operator+(char);
    QString &operator+(QChar);
    QString &operator+(const QString &);
    QString &operator+=(char);
    QString &operator+=(QChar);
    QString &operator+=(const QString &);

    QString &prepend(const QString &);
    QString &append(const char *);
    QString &append(const QString &);

    QCString utf8() const;
    QCString local8Bit() const;
    QString &setUnicode(const QChar *, uint);
    void compose();

    QString visual(int index = 0, int len = -1);

    static const QString null;

    static QString number(long, int base = 10);

    operator QChar () const;
};

QString &operator+(const char *, const QString &);
QString &operator+(QChar, const QString &);
bool operator==(const QString &, QChar);
bool operator==(const QString &, const QString &);
bool operator==(const QString &, const char *);
bool operator==(const char *, const QString &);
bool operator!=(const QString &, QChar);
bool operator!=(const QString &, const QString &);
bool operator!=(const QString &, const char *);
bool operator!=(const char *, const QString &);
QString operator+(char, const QString &);

class QConstString {
public:
    QConstString(QChar *, uint);
    const QString string() const;
};

#endif
