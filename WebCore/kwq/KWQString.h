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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// USING_BORROWED_QSTRING ======================================================

#ifdef USING_BORROWED_QSTRING
#include <_qstring.h>
#else

// FIXME: this clever hack may need to be moved into KWQDef.h or elsewhere
#define Fixed MacFixed
#define Rect MacRect
#define Boolean MacBoolean
#include <CoreFoundation/CoreFoundation.h>
#undef Fixed
#undef Rect
#undef Boolean

#include "qcstring.h"

class QString;
class QRegExp;

// QChar class =================================================================

class QChar {
public:

    // typedefs ----------------------------------------------------------------

    // enums -------------------------------------------------------------------

    enum Direction {
        // NOTE: alphabetical order
        DirAL, DirAN, DirB, DirBN, DirCS, DirEN, DirES, DirET, DirL, DirLRE,
        DirLRO, DirNSM, DirON, DirPDF, DirR, DirRLE, DirRLO, DirS, DirWS
    };

    // constants ---------------------------------------------------------------

    static const QChar null;

    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QChar();
    QChar(char);
    QChar(uchar);
    QChar(short);
    QChar(ushort);
    QChar(int);
    QChar(uint);

    QChar(const QChar &);

    ~QChar();

    // member functions --------------------------------------------------------

    ushort unicode() const;
    uchar cell() const;
    uchar row() const;
    char latin1() const;
    bool isNull() const;
    bool isSpace() const;
    bool isDigit() const;
    bool isLetter() const;
    bool isLetterOrNumber() const;
    bool isPunct() const;
    QChar lower() const;
    QChar upper() const;
    Direction direction() const;
    bool mirrored() const;
    QChar mirroredChar() const;

    // operators ---------------------------------------------------------------

    operator char() const;
    friend int operator==(QChar, QChar);
    friend int operator==(QChar, char);
    friend int operator==(char, QChar);
    friend int operator!=(QChar, QChar);
    friend int operator!=(QChar, char);
    friend int operator!=(char, QChar);

    // data members ------------------------------------------------------------

// NOTE: this is NOT private:
    UniChar c;

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QChar ==============================================================


// QString class ===============================================================

class QString {
public:
    static QString fromLatin1(const char *, int len=-1);

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------

    static const QString null;

    // static member functions -------------------------------------------------

    static QString number(long, int base=10);

    // constructors, copy constructors, and destructors ------------------------

    QString();
    QString(QChar);
    QString(const QByteArray &);
    QString(const QChar *, uint);
    QString(const char *);

    QString(const QString &);

    ~QString();

    // assignment operators ----------------------------------------------------

    QString &operator=(const QString &);
    QString &operator=(const QCString &);
    QString &operator=(const char *);
    QString &operator=(QChar);
    QString &operator=(char);

    // member functions --------------------------------------------------------

    bool isNull() const;
    bool isEmpty() const;
    uint length() const;
    bool startsWith(const QString &) const;

    int toInt() const;
    int toInt(bool *, int base=10) const;
    uint toUInt(bool *ok=0, int base=10) const;
    long toLong(bool *ok=0, int base=10) const;
    float toFloat(bool *b=0) const;

    QString &prepend(const QString &);
    QString &append(const char *);
    QString &append(const QString &);

    int contains(const char *, bool cs=TRUE) const;
    int contains(char) const;

    int find(char, int index=0) const;
    int find(const char *, int index=0, bool b=0) const;
    int find(const QString &, int index=0, bool b=0) const;
    int find(const QRegExp &, int index=0, bool b=0) const;
    int findRev(char, int index=0) const;
    int findRev(const char *, int index=0) const;

    QString &remove(uint, uint);
    QString &replace(const QRegExp &, const QString &);
    QString &insert(uint, char);
    void truncate(uint pos);
    void fill(QChar, int len=-1);

    QString arg (int &);
    QString arg(int a, int fieldwidth=0, int base=10) const;
    QString arg(const QString &, int fieldwidth = 0) const;

    QString left(uint) const;
    QString right(uint) const;
    QString mid(int, int len=0xffffffff) const;

    const char* latin1() const;
    const char *ascii() const;
    const QChar *unicode() const;
    QCString utf8() const;
    QCString local8Bit() const;
    QString &setUnicode(const QChar *, uint);

    QString &setNum(int, int base=10);
    QString &sprintf(const char *, ...);
    QString lower() const;
    QString stripWhiteSpace() const;
    QString simplifyWhiteSpace() const;
    void compose();
    QString visual(int index=0, int len=-1);

    // operators ---------------------------------------------------------------

    bool operator!() const;
    operator const char *() const;
    QChar operator[](int) const;
    QString &operator+(char);
    QString &operator+(QChar);
    QString &operator+(const QString &);
    QString &operator+=(char);
    QString &operator+=(QChar);
    QString &operator+=(const QString &);
    operator QChar () const;

    // data members ------------------------------------------------------------

// NOTE: this is NOT private:
    CFMutableStringRef s;

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------
    
}; // class QString ============================================================


// operators associated with QChar and QString =================================

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


// class QConstString ==========================================================

class QConstString : /* NOTE: this is NOT private */ QString {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------

    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QConstString(QChar *, uint);
#ifdef _KWQ_PEDANTIC_
    // NOTE: copy constructor not needed
    // QConstString(const QConstString &);
#endif

    // NOTE: destructor not needed
    //~QConstString();

    // member functions --------------------------------------------------------

    const QString &string() const;

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

    // assignment operators ----------------------------------------------------

    // private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    // NOTE: assignment operator not needed
    // QConstString &operator=(const QConstString &);
#endif

}; // class QConstString =======================================================

#endif // USING_BORROWED_QSTRING

#endif
