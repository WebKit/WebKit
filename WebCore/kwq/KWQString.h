/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>

#include "KWQCString.h"

// Make htmltokenizer.cpp happy
#define QT_VERSION 300

class QRegExp;

#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif

class QChar {
public:
    enum Direction {
        // NOTE: alphabetical order
        //DirAL, DirAN, DirB, DirBN, DirCS, DirEN, DirES, DirET, DirL, DirLRE,
        //DirLRO, DirNSM, DirON, DirPDF, DirR, DirRLE, DirRLO, DirS, DirWS
        //
        // Until we understand the implications better, I say we go with the qt
        // ordering here
        DirL, DirR, DirEN, DirES, DirET, DirAN, DirCS, DirB, DirS, DirWS, DirON,
        DirLRE, DirLRO, DirAL, DirRLE, DirRLO, DirPDF, DirNSM, DirBN
    };

    static const QChar null;

    QChar();
    QChar(char);
    QChar(uchar);
    QChar(short);
    QChar(ushort);
    QChar(int);
    QChar(uint);

    ushort unicode() const;
    uchar cell() const;
    uchar row() const;
    char latin1() const;
    bool isNull() const;
    bool isSpace() const;
    bool isDigit() const;
    bool isLetter() const;
    bool isNumber() const;
    bool isLetterOrNumber() const;
    bool isPunct() const;
    int digitValue() const;
    QChar lower() const;
    QChar upper() const;
    Direction direction() const;
    bool mirrored() const;
    QChar mirroredChar() const;

    operator char() const;

    friend bool operator==(QChar, QChar);
    friend bool operator==(QChar, char);
    friend bool operator==(char, QChar);

    friend bool operator!=(QChar, QChar);
    friend bool operator!=(QChar, char);
    friend bool operator!=(char, QChar);

    friend bool operator>(QChar, QChar);
    friend bool operator>(QChar, char);
    friend bool operator>(char, QChar);

    friend bool operator>=(QChar, QChar);
    friend bool operator>=(QChar, char);
    friend bool operator>=(char, QChar);

    friend bool operator<(QChar, QChar);
    friend bool operator<(QChar, char);
    friend bool operator<(char, QChar);

    friend bool operator<=(QChar, QChar);
    friend bool operator<=(QChar, char);
    friend bool operator<=(char, QChar);

private:
    UniChar c;

    friend class QString;
    friend class QConstString;

};

inline QChar::QChar() : c(0)
{
}

inline QChar::QChar(char ch) : c((uchar) ch)
{
}

inline QChar::QChar(uchar uch) : c(uch)
{
}

inline QChar::QChar(short n) : c(n)
{
}

inline QChar::QChar(ushort n) : c(n)
{
}

inline QChar::QChar(uint n) : c(n)
{
}

inline QChar::QChar(int n) : c(n)
{
}

inline ushort QChar::unicode() const
{
    return c;
}

inline uchar QChar::cell() const
{
    return c;
}

inline bool QChar::isNull() const
{
    return c == 0;
}

inline uchar QChar::row() const
{
    return c >> 8;
}

inline char QChar::latin1() const
{
    return c > 0xff ? 0 : c;
}

inline QChar::operator char() const
{
    return c > 0xff ? 0 : c;
}

inline bool operator==(QChar qc1, QChar qc2)
{
    return qc1.c == qc2.c;
}

inline bool operator==(QChar qc, char ch)
{
    return qc.c == (uchar) ch;
}

inline bool operator==(char ch, QChar qc)
{
    return (uchar) ch == qc.c;
}

inline bool operator!=(QChar qc1, QChar qc2)
{
    return qc1.c != qc2.c;
}

inline bool operator!=(QChar qc, char ch)
{
    return qc.c != (uchar) ch;
}

inline bool operator!=(char ch, QChar qc)
{
    return (uchar) ch != qc.c;
}

inline bool operator>=(QChar qc1, QChar qc2)
{
    return qc1.c >= qc2.c;
}

inline bool operator>=(QChar qc, char ch)
{
    return qc.c >= (uchar) ch;
}

inline bool operator>=(char ch, QChar qc)
{
    return (uchar) ch >= qc.c;
}

inline bool operator>(QChar qc1, QChar qc2)
{
    return qc1.c > qc2.c;
}

inline bool operator>(QChar qc, char ch)
{
    return qc.c > (uchar) ch;
}

inline bool operator>(char ch, QChar qc)
{
    return (uchar) ch > qc.c;
}

inline bool operator<=(QChar qc1, QChar qc2)
{
    return qc1.c <= qc2.c;
}

inline bool operator<=(QChar qc, char ch)
{
    return qc.c <= (uchar) ch;
}

inline bool operator<=(char ch, QChar qc)
{
    return (uchar) ch <= qc.c;
}

inline bool operator<(QChar qc1, QChar qc2)
{
    return qc1.c < qc2.c;
}

inline bool operator<(QChar qc, char ch)
{
    return qc.c < (uchar) ch;
}

inline bool operator<(char ch, QChar qc)
{
    return (uchar) ch < qc.c;
}

// Keep this struct to <= 46 bytes, that's what the system will allocate.
// Will be rounded up to a multiple of 4, so we're stuck at 44.

#define QS_INTERNAL_BUFFER_SIZE 20
#define QS_INTERNAL_BUFFER_CHARS QS_INTERNAL_BUFFER_SIZE-1
#define QS_INTERNAL_BUFFER_UCHARS QS_INTERNAL_BUFFER_SIZE/2

struct QStringData {
    // Uses shared null data.
    QStringData();
    void initialize();
    
    // No copy.
    QStringData(QChar *u, uint l, uint m);
    void initialize(QChar *u, uint l, uint m);
    
    // Copy bytes;
    QStringData(const QChar *u, uint l);
    void initialize(const QChar *u, uint l);

    // Copy bytes;
    QStringData(const char *u, uint l);
    void initialize(const char *u, uint l);

    ~QStringData();

#ifdef QSTRING_DEBUG_ALLOCATIONS
    void* operator new(size_t s);
    void operator delete(void*p);
#endif

    inline void ref() { refCount++; }
    inline void deref() { if (--refCount == 0 && _isHeapAllocated) delete this; }
        
    char *ascii();
    char *makeAscii();
    void increaseAsciiSize(uint size);

    QChar *unicode();
    QChar *makeUnicode();    
    void increaseUnicodeSize(uint size);
        
    uint refCount;
    uint _length;
    mutable QChar *_unicode;
    mutable char *_ascii;
    uint _maxUnicode:29;
    uint _isUnicodeInternal:1;
    uint _isUnicodeValid:1;
    uint _isHeapAllocated:1;	// Fragile, but the only way we can be sure the instance was
                                // created with 'new'.
    uint _maxAscii:30;
    uint _isAsciiInternal:1;
    uint _isAsciiValid:1;
    char _internalBuffer[QS_INTERNAL_BUFFER_SIZE]; // Pad out to a (((size + 1) & ~15) + 14) size
};

class QString {
public:
    static const QString null;

    QString();
    QString(QChar);
    QString(const QByteArray &);
    QString(const QChar *, uint);
    QString(const char *);
    QString(const char *, int len);
    
    QString(const QString &);
    QString &operator=(const QString &);

    ~QString();

    static QString fromLatin1(const char *);
    static QString fromLatin1(const char *, int len);
    static QString fromStringWithEncoding(const char *, int, CFStringEncoding);
    static QString fromCFString(CFStringRef);
    static QString fromNSString(NSString *);
    
    QString &operator=(char);
    QString &operator=(QChar);
    QString &operator=(const char *);
    QString &operator=(const QCString &);

    uint length() const;

    const QChar *unicode() const;
    const char *latin1() const;
    const char *ascii() const;
    QCString utf8() const;
    QCString local8Bit() const;

    bool isNull() const;
    bool isEmpty() const;

    QChar at(uint) const;

    int compare(const QString &) const;
    int compare(const char *) const;

    bool startsWith(const QString &) const;

    int find(char, int index = 0) const;
    int find(QChar, int index = 0) const;
    int find(const char *, int index = 0, bool cs = true) const;
    int find(const QString &, int index = 0, bool cs = true) const;
    int find(const QRegExp &, int index = 0) const;

    int findRev(char, int index = -1) const;
    int findRev(const QString& str, int index, bool cs = true) const;
    int findRev(const char *, int index = -1) const;

    int contains(char) const;
    int contains(const char *, bool cs = true) const;
    int contains(const QString &, bool cs = true) const;
    int contains(QChar c, bool cs = true) const;

    bool endsWith(const QString &) const;

    // NOTE: toXXXXX integer functions only support base 10 and base 16
    // NOTE: toShort, toUShort, toULong, and toDouble are NOT used but are kept
    // for completeness
    short toShort(bool *ok=NULL, int base=10) const;
    // NOTE: ok and base NOT used for toUShort
    ushort toUShort(bool *ok=NULL, int base=10) const;
    int toInt(bool *ok=NULL, int base=10) const;
    // NOTE: base NOT used for toUInt
    uint toUInt(bool *ok=NULL, int base=10) const;
    long toLong(bool *ok=NULL, int base=10) const;
    ulong toULong(bool *ok=NULL, int base=10) const;
    float toFloat(bool *ok=NULL) const;
    double toDouble(bool *ok=NULL) const;

    static QString number(int);
    static QString number(uint);
    static QString number(long);
    static QString number(ulong);
    static QString number(double);

    bool findArg(int& pos, int& len) const;
    
    QString arg(const QString &, int width=0) const;
    QString arg(short, int width=0) const;
    QString arg(ushort, int width=0) const;
    QString arg(int, int width=0) const;
    QString arg(uint, int width=0) const;
    QString arg(long, int width=0) const;
    QString arg(ulong, int width=0) const;
    QString arg(double, int width=0) const;

    QString left(uint) const;
    QString right(uint) const;
    QString mid(uint, uint len=0xffffffff) const;

    QString copy() const;

    QString lower() const;
    QString stripWhiteSpace() const;
    QString simplifyWhiteSpace() const;

    QString &setUnicode(const QChar *, uint);
    QString &setLatin1(const char *, int len=-1);

    QString &setNum(short);
    QString &setNum(ushort);
    QString &setNum(int);
    QString &setNum(uint);
    QString &setNum(long);
    QString &setNum(ulong);
    QString &setNum(double);

    QString &sprintf(const char *, ...) __attribute__ ((format (printf, 2, 3)));

    QString &prepend(const QString &);
    QString &append(const QString &);
    QString &insert(uint, const QString &);
    QString &insert(uint, QChar);
    QString &insert(uint, char);
    QString &insert(uint index, const char *insertChars, uint insertLength);
    QString &remove(uint, uint);
    QString &replace( uint index, uint len, const QString &s );
    //QString &replace( uint index, uint len, const QChar* s, uint slen );
    QString &replace(const QRegExp &, const QString &);

    void truncate(uint);
    void fill(QChar, int len=-1);

    void compose();
    QString visual();

    CFStringRef getCFString() const;
    NSString *getNSString() const;

    bool operator!() const;

    const QChar operator[](int) const;

    QString &operator+=(const QString &);
    QString &operator+=(QChar);
    QString &operator+=(char);

    void setBufferFromCFString(CFStringRef);
    
private:
    // Used by QConstString.
    QString(QStringData *constData, bool /*dummy*/);
    void detach();
    void detachInternal();
    void deref();
    QChar *forceUnicode();
    void setLength(uint);

    QStringData *data() const;
    
    QCString convertToQCString(CFStringEncoding) const;

    QStringData **dataHandle;
    QStringData internalData;
    
    static QStringData* shared_null;
    static QStringData* makeSharedNull();
    static QStringData**shared_null_handle;
    static QStringData**makeSharedNullHandle();

    friend bool operator==(const QString &, const QString &);
    friend bool operator==(const QString &, const char *);

    friend class QConstString;
    friend class QGDict;
    friend struct QStringData;
};

QString operator+(const QString &, const QString &);
QString operator+(const QString &, const char *);
QString operator+(const QString &, QChar);
QString operator+(const QString &, char);
QString operator+(const char *, const QString &);
QString operator+(QChar, const QString &);
QString operator+(char, const QString &);

inline QStringData *QString::data() const { return *dataHandle; }

inline uint QString::length() const
{
    return data()->_length;
}

inline bool QString::isEmpty() const
{
    return length() == 0;
}

inline QString QString::fromLatin1(const char *chs)
{
    return chs;
}

inline QString QString::fromLatin1(const char *chs, int length)
{
    return QString(chs, length);
}

inline const char *QString::ascii() const
{
    return latin1();
}

inline float QString::toFloat(bool *ok) const
{
    return toDouble(ok);
}

inline bool QString::operator!() const
{
    return isNull();
}

inline const QChar QString::operator[](int index) const
{
    return at(index);
}

inline bool operator==(const char *chs, const QString &qs)
{
    return qs == chs;
}

inline bool operator!=(const QString &qs1, const QString &qs2)
{
    return !(qs1 == qs2);
}

inline bool operator!=(const QString &qs, const char *chs)
{
    return !(qs == chs);
}

inline bool operator!=(const char *chs, const QString &qs)
{
    return !(qs == chs);
}

inline bool operator<(const QString &qs1, const QString &qs2)
{
    return qs1.compare(qs2) < 0;
}

inline bool operator<(const QString &qs, const char *chs)
{
    return qs.compare(chs) < 0;
}

inline bool operator<(const char *chs, const QString &qs)
{
    return qs.compare(chs) > 0;
}

inline bool operator<=(const QString &qs1, const QString &qs2)
{
    return qs1.compare(qs2) <= 0;
}

inline bool operator<=(const QString &qs, const char *chs)
{
    return qs.compare(chs) <= 0;
}

inline bool operator<=(const char *chs, const QString &qs)
{
    return qs.compare(chs) >= 0;
}

inline bool operator>(const QString &qs1, const QString &qs2)
{
    return qs1.compare(qs2) > 0;
}

inline bool operator>(const QString &qs, const char *chs)
{
    return qs.compare(chs) > 0;
}

inline bool operator>(const char *chs, const QString &qs)
{
    return qs.compare(chs) < 0;
}

inline bool operator>=(const QString &qs1, const QString &qs2)
{
    return qs1.compare(qs2) >= 0;
}

inline bool operator>=(const QString &qs, const char *chs)
{
    return qs.compare(chs) >= 0;
}

inline bool operator>=(const char *chs, const QString &qs)
{
    return qs.compare(chs) <= 0;
}

class QConstString : private QString {
public:
    QConstString(const QChar *, uint);
    ~QConstString();
    const QString &string() const { return *this; }
};

#endif
