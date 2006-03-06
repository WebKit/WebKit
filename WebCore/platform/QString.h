/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "KWQDef.h"
#include <ctype.h>
#include <unicode/uchar.h>
#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif
#include "KWQCString.h"

// Make htmltokenizer.cpp happy
#define QT_VERSION 300

class QRegExp;

#if __APPLE__
#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif
#endif

class QChar {
public:

    enum Direction {
        DirL = 0, DirR, DirEN, DirES, DirET, DirAN, DirCS, DirB, DirS, DirWS, DirON,
        DirLRE, DirLRO, DirAL, DirRLE, DirRLO, DirPDF, DirNSM, DirBN
    };
    
    enum SpecialCharacter {
        byteOrderMark = 0xfeff
    };

    static const char null = 0; // not a QChar as in Qt (can't have static constructor), but close enough to be compatible in most cases

    QChar();
    QChar(char);
    QChar(unsigned char);
    QChar(short);
    QChar(unsigned short);
    QChar(int);
    QChar(uint);

    unsigned short unicode() const;
    unsigned char cell() const;
    unsigned char row() const;
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
    unsigned short c;

    friend class QString;
    friend class QConstString;
};

inline QChar::QChar() : c(0)
{
}

inline QChar::QChar(char ch) : c((unsigned char) ch)
{
}

inline QChar::QChar(unsigned char uch) : c(uch)
{
}

inline QChar::QChar(short n) : c(n)
{
}

inline QChar::QChar(unsigned short n) : c(n)
{
}

inline QChar::QChar(uint n) : c(n)
{
}

inline QChar::QChar(int n) : c(n)
{
}

inline unsigned short QChar::unicode() const
{
    return c;
}

inline unsigned char QChar::cell() const
{
    return c;
}

inline bool QChar::isNull() const
{
    return c == 0;
}

inline bool QChar::isSpace() const
{
    // Use isspace() for basic Latin-1.
    // This will include newlines, which aren't included in unicode DirWS.
    return c <= 0x7F ? isspace(c) : direction() == DirWS;
}

inline bool QChar::isDigit() const
{
    // FIXME: If fast enough, we should just call u_isdigit directly.
    return c <= 0x7F ? isdigit(c) : u_isdigit(c);
}

inline bool QChar::isLetter() const
{
    // FIXME: If fast enough, we should just call u_isalpha directly.
    return c <= 0x7F ? isalpha(c) : u_isalpha(c);
}

inline bool QChar::isNumber() const
{
    // FIXME: If fast enough, we should just call u_isdigit directly.
    return c <= 0x7F ? isdigit(c) : u_isdigit(c);
}

inline bool QChar::isLetterOrNumber() const
{
    // FIXME: If fast enough, we should just call u_isalnum directly.
    return c <= 0x7F ? isalnum(c) : u_isalnum(c);
}

inline bool QChar::isPunct() const
{
    return u_ispunct(c);
}

inline int QChar::digitValue() const
{
    // FIXME: If fast enough, we should just call u_charDigitValue directly.
    return c <= '9' ? c - '0' : u_charDigitValue(c);
}

inline QChar QChar::lower() const
{
    // FIXME: If fast enough, we should just call u_tolower directly.
    return c <= 0x7F ? tolower(c) : u_tolower(c);
}

inline QChar QChar::upper() const
{
    // FIXME: If fast enough, we should just call u_toupper directly.
    return c <= 0x7F ? toupper(c) : u_toupper(c);
}

inline QChar::Direction QChar::direction() const
{
    return static_cast<Direction>(u_charDirection(c));
}

inline bool QChar::mirrored() const
{
    return u_isMirrored(c);
}

inline QChar QChar::mirroredChar() const
{
    return QChar(static_cast<uint>(u_charMirror(c)));
}

inline unsigned char QChar::row() const
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
    return qc.c == (unsigned char) ch;
}

inline bool operator==(char ch, QChar qc)
{
    return (unsigned char) ch == qc.c;
}

inline bool operator!=(QChar qc1, QChar qc2)
{
    return qc1.c != qc2.c;
}

inline bool operator!=(QChar qc, char ch)
{
    return qc.c != (unsigned char) ch;
}

inline bool operator!=(char ch, QChar qc)
{
    return (unsigned char) ch != qc.c;
}

inline bool operator>=(QChar qc1, QChar qc2)
{
    return qc1.c >= qc2.c;
}

inline bool operator>=(QChar qc, char ch)
{
    return qc.c >= (unsigned char) ch;
}

inline bool operator>=(char ch, QChar qc)
{
    return (unsigned char) ch >= qc.c;
}

inline bool operator>(QChar qc1, QChar qc2)
{
    return qc1.c > qc2.c;
}

inline bool operator>(QChar qc, char ch)
{
    return qc.c > (unsigned char) ch;
}

inline bool operator>(char ch, QChar qc)
{
    return (unsigned char) ch > qc.c;
}

inline bool operator<=(QChar qc1, QChar qc2)
{
    return qc1.c <= qc2.c;
}

inline bool operator<=(QChar qc, char ch)
{
    return qc.c <= (unsigned char) ch;
}

inline bool operator<=(char ch, QChar qc)
{
    return (unsigned char) ch <= qc.c;
}

inline bool operator<(QChar qc1, QChar qc2)
{
    return qc1.c < qc2.c;
}

inline bool operator<(QChar qc, char ch)
{
    return qc.c < (unsigned char) ch;
}

inline bool operator<(char ch, QChar qc)
{
    return (unsigned char) ch < qc.c;
}

// Keep this struct to <= 46 bytes, that's what the system will allocate.
// Will be rounded up to a multiple of 4, so we're stuck at 44.

#define QS_INTERNAL_BUFFER_SIZE 20
#define QS_INTERNAL_BUFFER_CHARS QS_INTERNAL_BUFFER_SIZE-1
#define QS_INTERNAL_BUFFER_UCHARS QS_INTERNAL_BUFFER_SIZE/2

struct KWQStringData  
{
    // Uses shared null data.
    KWQStringData();
    void initialize();
    
    // No copy.
    KWQStringData(QChar *u, uint l, uint m);
    void initialize(QChar *u, uint l, uint m);
    
    // Copy bytes.
    KWQStringData(const QChar *u, uint l);
    void initialize(const QChar *u, uint l);

    // Copy bytes.
    KWQStringData(const char *u, uint l);
    void initialize(const char *u, uint l);

    // Move from destination to source.
    KWQStringData(KWQStringData &);

    ~KWQStringData();

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
    
    bool isUnicodeInternal() const { return (char *)_unicode == _internalBuffer; }
    bool isAsciiInternal() const { return _ascii == _internalBuffer; }

    uint refCount;
    uint _length;
    mutable QChar *_unicode;
    mutable char *_ascii;
    unsigned _maxUnicode : 30;
    bool _isUnicodeValid : 1;
    bool _isHeapAllocated : 1; // Fragile, but the only way we can be sure the instance was created with 'new'.
    unsigned _maxAscii : 31;
    bool _isAsciiValid : 1;
    
    char _internalBuffer[QS_INTERNAL_BUFFER_SIZE]; // Pad out to a (((size + 1) & ~15) + 14) size

private:
    KWQStringData(const KWQStringData &);
    KWQStringData &operator=(const KWQStringData &);
};

#define QSTRING_NULL QString()

class QString {
public:
    static const char * const null; // not a QString as in Qt (can't have static constructor), but close enough to be compatible in most cases

    QString();
    QString(QChar);
    QString(const ByteArray &);
    QString(const QChar *, uint);
    QString(const char *);
    QString(const char *, int len);
    
    QString(const QString &);
    QString &operator=(const QString &);

    ~QString();

    static QString fromLatin1(const char *);
    static QString fromLatin1(const char *, int len);
    static QString fromUtf8(const char *);
    static QString fromUtf8(const char *, int len);
#if __APPLE__
    static QString fromCFString(CFStringRef);
    static QString fromNSString(NSString*);
#endif
    QString &operator=(char);
    QString &operator=(QChar);
    QString &operator=(const char *);
    QString &operator=(const QCString &);

    uint length() const;

    const QChar *unicode() const;
    const QChar *stableUnicode();
    const char *latin1() const;
    const char *ascii() const;
    bool isAllASCII() const;
    bool isAllLatin1() const;
    bool hasFastLatin1() const;
    void copyLatin1(char *buffer, uint position = 0, uint length = 0xffffffff) const;
    QCString utf8() const { int length; return utf8(length); }
    QCString utf8(int &length) const;

    bool isNull() const;
    bool isEmpty() const;

    QChar at(uint) const;

    int compare(const QString &) const;
    int compare(const char *) const;

    bool startsWith(const QString &) const;
    bool startsWith(const char *) const;
    bool startsWith(const char *, bool caseSensitive) const;

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

    short toShort(bool *ok = 0, int base = 10) const;
    unsigned short toUShort(bool *ok = 0, int base = 10) const;
    int toInt(bool *ok = 0, int base = 10) const;
    uint toUInt(bool *ok = 0, int base = 10) const;
    double toDouble(bool *ok = 0) const;

    static QString number(int);
    static QString number(uint);
    static QString number(long);
    static QString number(unsigned long);
    static QString number(double);

    bool findArg(int& pos, int& len) const;
    
    QString arg(const QString &, int width=0) const;
    QString arg(short, int width=0) const;
    QString arg(unsigned short, int width=0) const;
    QString arg(int, int width=0) const;
    QString arg(uint, int width=0) const;
    QString arg(long, int width=0) const;
    QString arg(unsigned long, int width=0) const;
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
    QString &setNum(unsigned short);
    QString &setNum(int);
    QString &setNum(uint);
    QString &setNum(long);
    QString &setNum(unsigned long);
    QString &setNum(double);

    QString &sprintf(const char *, ...) 
#if __GNUC__
    __attribute__ ((format (printf, 2, 3)))
#endif
    ;

    QString &append(const QString &);
    QString &append(QChar);
    QString &append(char);
    QString &insert(uint, const QString &);
    QString &insert(uint, QChar);
    QString &insert(uint, char);
    QString &insert(uint index, const char *insertChars, uint insertLength);
    QString &prepend(const QString &);
    QString &remove(uint, uint);
    QString &remove(const QChar &c) { return replace(QString(c), ""); }
    QString &remove(const QString &s) { return replace(s, ""); }
    QString &replace(uint index, uint len, const QString &s);
    QString &replace(char, const QString &);
    QString &replace(QChar, const QString &);
    QString &replace(const QString &, const QString &);
    QString &replace(const QRegExp &, const QString &);
    QString &replace(QChar, QChar);

    QString &append(const QChar *, uint length);
    QString &append(const char *, uint length);
    QString &insert(uint position, const QChar *, uint length);
    QString &prepend(const QChar *, uint length);
    
    void fill(QChar, int len=-1);
    void truncate(uint);

    void reserve(uint);

    bool operator!() const;

    const QChar operator[](int) const;

    QString &operator+=(const QString &s) { return append(s); }
    QString &operator+=(QChar c) { return append(c); }
    QString &operator+=(char c) { return append(c); }

#if __APPLE__
    CFStringRef getCFString() const;
    NSString *getNSString() const;
    void setBufferFromCFString(CFStringRef);
#endif

private:
    // Used by QConstString.
    QString(KWQStringData *constData, bool /*dummy*/);
    void detach();
    void detachAndDiscardCharacters();
    void detachIfInternal();
    void detachInternal();
    void deref();
    QChar *forceUnicode();
    void setLength(uint);

    KWQStringData **dataHandle;
    KWQStringData internalData;
    
    static KWQStringData *shared_null;
    static KWQStringData *makeSharedNull();
    static KWQStringData **shared_null_handle;
    static KWQStringData **makeSharedNullHandle();

    friend bool operator==(const QString &, const QString &);
    friend bool operator==(const QString &, const char *);

    friend class QConstString;
    friend class QGDict;
    friend struct KWQStringData;
};

QString operator+(const QString &, const QString &);
QString operator+(const QString &, const char *);
QString operator+(const QString &, QChar);
QString operator+(const QString &, char);
QString operator+(const char *, const QString &);
QString operator+(QChar, const QString &);
QString operator+(char, const QString &);

inline char *KWQStringData::ascii()
{
    return _isAsciiValid ? _ascii : makeAscii();
}

inline QChar *KWQStringData::unicode()
{
    return _isUnicodeValid ? _unicode : makeUnicode();
}

inline uint QString::length() const
{
    return dataHandle[0]->_length;
}

inline bool QString::isEmpty() const
{
    return dataHandle[0]->_length == 0;
}

inline const char *QString::latin1() const
{
    return dataHandle[0]->ascii();
}

inline const QChar *QString::unicode() const
{
    return dataHandle[0]->unicode();
}

#if __APPLE__
inline CFStringRef QString::getCFString() const
{
    return (CFStringRef)getNSString();
}
#endif

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
