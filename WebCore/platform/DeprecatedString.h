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

#include <ctype.h>
#include <unicode/uchar.h>
#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif
#include "KWQCString.h"

class RegularExpression;

#if __APPLE__
#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif
#endif

namespace KJS {
    class Identifier;
    class UString;
}

class QChar {
public:
    QChar();
    QChar(char);
    QChar(unsigned char);
    QChar(short);
    QChar(unsigned short);
    QChar(int);
    QChar(unsigned);

    unsigned short unicode() const;
    char latin1() const;
    bool isSpace() const;
    QChar lower() const;
    QChar upper() const;

private:
    unsigned short c;
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

inline QChar::QChar(unsigned n) : c(n)
{
}

inline QChar::QChar(int n) : c(n)
{
}

inline unsigned short QChar::unicode() const
{
    return c;
}

inline bool QChar::isSpace() const
{
    // Use isspace() for basic Latin-1.
    // This will include newlines, which aren't included in unicode DirWS.
    return c <= 0x7F ? isspace(c) : (u_charDirection(c) == U_WHITE_SPACE_NEUTRAL);
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

inline char QChar::latin1() const
{
    return c > 0xff ? 0 : c;
}

inline bool operator==(QChar qc1, QChar qc2)
{
    return qc1.unicode() == qc2.unicode();
}

inline bool operator==(QChar qc, char ch)
{
    return qc.unicode() == (unsigned char) ch;
}

inline bool operator==(char ch, QChar qc)
{
    return (unsigned char) ch == qc.unicode();
}

inline bool operator!=(QChar qc1, QChar qc2)
{
    return qc1.unicode() != qc2.unicode();
}

inline bool operator!=(QChar qc, char ch)
{
    return qc.unicode() != (unsigned char) ch;
}

inline bool operator!=(char ch, QChar qc)
{
    return (unsigned char) ch != qc.unicode();
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
    KWQStringData(QChar *u, unsigned l, unsigned m);
    void initialize(QChar *u, unsigned l, unsigned m);
    
    // Copy bytes.
    KWQStringData(const QChar *u, unsigned l);
    void initialize(const QChar *u, unsigned l);

    // Copy bytes.
    KWQStringData(const char *u, unsigned l);
    void initialize(const char *u, unsigned l);

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
    void increaseAsciiSize(unsigned size);

    QChar *unicode();
    QChar *makeUnicode();    
    void increaseUnicodeSize(unsigned size);
    
    bool isUnicodeInternal() const { return (char *)_unicode == _internalBuffer; }
    bool isAsciiInternal() const { return _ascii == _internalBuffer; }

    unsigned refCount;
    unsigned _length;
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

class DeprecatedString {
public:
    static const char * const null;

    DeprecatedString();
    DeprecatedString(QChar);
    DeprecatedString(const QChar *, unsigned);
    DeprecatedString(const char *);
    DeprecatedString(const char *, int len);
    DeprecatedString(const KJS::Identifier&);
    DeprecatedString(const KJS::UString&);
    
    DeprecatedString(const DeprecatedString &);
    DeprecatedString &operator=(const DeprecatedString &);

    ~DeprecatedString();

    operator KJS::Identifier() const;
    operator KJS::UString() const;

    static DeprecatedString fromLatin1(const char *);
    static DeprecatedString fromLatin1(const char *, int len);
    static DeprecatedString fromUtf8(const char *);
    static DeprecatedString fromUtf8(const char *, int len);
#if __APPLE__
    static DeprecatedString fromCFString(CFStringRef);
    static DeprecatedString fromNSString(NSString*);
#endif
    DeprecatedString &operator=(char);
    DeprecatedString &operator=(QChar);
    DeprecatedString &operator=(const char *);
    DeprecatedString &operator=(const DeprecatedCString &);

    unsigned length() const;

    const QChar *unicode() const;
    const QChar *stableUnicode();
    const char *latin1() const;
    const char *ascii() const;
    bool isAllASCII() const;
    bool isAllLatin1() const;
    bool hasFastLatin1() const;
    void copyLatin1(char *buffer, unsigned position = 0, unsigned length = 0xffffffff) const;
    DeprecatedCString utf8() const { int length; return utf8(length); }
    DeprecatedCString utf8(int &length) const;

    bool isNull() const;
    bool isEmpty() const;

    QChar at(unsigned) const;

    int compare(const DeprecatedString &) const;
    int compare(const char *) const;

    bool startsWith(const DeprecatedString &) const;
    bool startsWith(const char *) const;
    bool startsWith(const char *, bool caseSensitive) const;

    int find(char, int index = 0) const;
    int find(QChar, int index = 0) const;
    int find(const char *, int index = 0, bool cs = true) const;
    int find(const DeprecatedString &, int index = 0, bool cs = true) const;
    int find(const RegularExpression &, int index = 0) const;

    int findRev(char, int index = -1) const;
    int findRev(const DeprecatedString& str, int index, bool cs = true) const;
    int findRev(const char *, int index = -1) const;

    int contains(char) const;
    int contains(const char *, bool cs = true) const;
    int contains(const DeprecatedString &, bool cs = true) const;
    int contains(QChar c, bool cs = true) const;

    bool endsWith(const DeprecatedString &) const;

    short toShort(bool *ok = 0, int base = 10) const;
    unsigned short toUShort(bool *ok = 0, int base = 10) const;
    int toInt(bool *ok = 0, int base = 10) const;
    unsigned toUInt(bool *ok = 0, int base = 10) const;
    double toDouble(bool *ok = 0) const;

    static DeprecatedString number(int);
    static DeprecatedString number(unsigned);
    static DeprecatedString number(long);
    static DeprecatedString number(unsigned long);
    static DeprecatedString number(double);

    DeprecatedString left(unsigned) const;
    DeprecatedString right(unsigned) const;
    DeprecatedString mid(unsigned, unsigned len=0xffffffff) const;

    DeprecatedString copy() const;

    DeprecatedString lower() const;
    DeprecatedString stripWhiteSpace() const;
    DeprecatedString simplifyWhiteSpace() const;

    DeprecatedString &setUnicode(const QChar *, unsigned);
    DeprecatedString &setLatin1(const char *, int len=-1);

    DeprecatedString &setNum(short);
    DeprecatedString &setNum(unsigned short);
    DeprecatedString &setNum(int);
    DeprecatedString &setNum(unsigned);
    DeprecatedString &setNum(long);
    DeprecatedString &setNum(unsigned long);
    DeprecatedString &setNum(double);

    DeprecatedString &sprintf(const char *, ...) 
#if __GNUC__
    __attribute__ ((format (printf, 2, 3)))
#endif
    ;

    DeprecatedString &append(const DeprecatedString &);
    DeprecatedString &append(QChar);
    DeprecatedString &append(char);
    DeprecatedString &insert(unsigned, const DeprecatedString &);
    DeprecatedString &insert(unsigned, QChar);
    DeprecatedString &insert(unsigned, char);
    DeprecatedString &insert(unsigned index, const char *insertChars, unsigned insertLength);
    DeprecatedString &prepend(const DeprecatedString &);
    DeprecatedString &remove(unsigned, unsigned);
    DeprecatedString &remove(const QChar &c) { return replace(DeprecatedString(c), ""); }
    DeprecatedString &remove(const DeprecatedString &s) { return replace(s, ""); }
    DeprecatedString &replace(unsigned index, unsigned len, const DeprecatedString &s);
    DeprecatedString &replace(char, const DeprecatedString &);
    DeprecatedString &replace(QChar, const DeprecatedString &);
    DeprecatedString &replace(const DeprecatedString &, const DeprecatedString &);
    DeprecatedString &replace(const RegularExpression &, const DeprecatedString &);
    DeprecatedString &replace(QChar, QChar);

    DeprecatedString &append(const QChar *, unsigned length);
    DeprecatedString &append(const char *, unsigned length);
    DeprecatedString &insert(unsigned position, const QChar *, unsigned length);
    DeprecatedString &prepend(const QChar *, unsigned length);
    
    void fill(QChar, int len=-1);
    void truncate(unsigned);

    void reserve(unsigned);

    bool operator!() const;

    const QChar operator[](int) const;

    DeprecatedString &operator+=(const DeprecatedString &s) { return append(s); }
    DeprecatedString &operator+=(QChar c) { return append(c); }
    DeprecatedString &operator+=(char c) { return append(c); }

#if __APPLE__
    CFStringRef getCFString() const;
    NSString *getNSString() const;
    void setBufferFromCFString(CFStringRef);
#endif

private:
    // Used by QConstString.
    DeprecatedString(KWQStringData *constData, bool /*dummy*/);
    void detach();
    void detachAndDiscardCharacters();
    void detachIfInternal();
    void detachInternal();
    void deref();
    QChar *forceUnicode();
    void setLength(unsigned);

    KWQStringData **dataHandle;
    KWQStringData internalData;
    
    static KWQStringData *shared_null;
    static KWQStringData *makeSharedNull();
    static KWQStringData **shared_null_handle;
    static KWQStringData **makeSharedNullHandle();

    friend bool operator==(const DeprecatedString &, const DeprecatedString &);
    friend bool operator==(const DeprecatedString &, const char *);

    friend class QConstString;
    friend class QGDict;
    friend struct KWQStringData;
};

DeprecatedString operator+(const DeprecatedString &, const DeprecatedString &);
DeprecatedString operator+(const DeprecatedString &, const char *);
DeprecatedString operator+(const DeprecatedString &, QChar);
DeprecatedString operator+(const DeprecatedString &, char);
DeprecatedString operator+(const char *, const DeprecatedString &);
DeprecatedString operator+(QChar, const DeprecatedString &);
DeprecatedString operator+(char, const DeprecatedString &);

inline char *KWQStringData::ascii()
{
    return _isAsciiValid ? _ascii : makeAscii();
}

inline QChar *KWQStringData::unicode()
{
    return _isUnicodeValid ? _unicode : makeUnicode();
}

inline unsigned DeprecatedString::length() const
{
    return dataHandle[0]->_length;
}

inline bool DeprecatedString::isEmpty() const
{
    return dataHandle[0]->_length == 0;
}

inline const char *DeprecatedString::latin1() const
{
    return dataHandle[0]->ascii();
}

inline const QChar *DeprecatedString::unicode() const
{
    return dataHandle[0]->unicode();
}

#if __APPLE__
inline CFStringRef DeprecatedString::getCFString() const
{
    return (CFStringRef)getNSString();
}
#endif

inline DeprecatedString DeprecatedString::fromLatin1(const char *chs)
{
    return chs;
}

inline DeprecatedString DeprecatedString::fromLatin1(const char *chs, int length)
{
    return DeprecatedString(chs, length);
}

inline const char *DeprecatedString::ascii() const
{
    return latin1();
}

inline bool DeprecatedString::operator!() const
{
    return isNull();
}

inline const QChar DeprecatedString::operator[](int index) const
{
    return at(index);
}

inline bool operator==(const char *chs, const DeprecatedString &qs)
{
    return qs == chs;
}

inline bool operator!=(const DeprecatedString &qs1, const DeprecatedString &qs2)
{
    return !(qs1 == qs2);
}

inline bool operator!=(const DeprecatedString &qs, const char *chs)
{
    return !(qs == chs);
}

inline bool operator!=(const char *chs, const DeprecatedString &qs)
{
    return !(qs == chs);
}

inline bool operator<(const DeprecatedString &qs1, const DeprecatedString &qs2)
{
    return qs1.compare(qs2) < 0;
}

inline bool operator<(const DeprecatedString &qs, const char *chs)
{
    return qs.compare(chs) < 0;
}

inline bool operator<(const char *chs, const DeprecatedString &qs)
{
    return qs.compare(chs) > 0;
}

inline bool operator<=(const DeprecatedString &qs1, const DeprecatedString &qs2)
{
    return qs1.compare(qs2) <= 0;
}

inline bool operator<=(const DeprecatedString &qs, const char *chs)
{
    return qs.compare(chs) <= 0;
}

inline bool operator<=(const char *chs, const DeprecatedString &qs)
{
    return qs.compare(chs) >= 0;
}

inline bool operator>(const DeprecatedString &qs1, const DeprecatedString &qs2)
{
    return qs1.compare(qs2) > 0;
}

inline bool operator>(const DeprecatedString &qs, const char *chs)
{
    return qs.compare(chs) > 0;
}

inline bool operator>(const char *chs, const DeprecatedString &qs)
{
    return qs.compare(chs) < 0;
}

inline bool operator>=(const DeprecatedString &qs1, const DeprecatedString &qs2)
{
    return qs1.compare(qs2) >= 0;
}

inline bool operator>=(const DeprecatedString &qs, const char *chs)
{
    return qs.compare(chs) >= 0;
}

inline bool operator>=(const char *chs, const DeprecatedString &qs)
{
    return qs.compare(chs) <= 0;
}

class QConstString : private DeprecatedString {
public:
    QConstString(const QChar *, unsigned);
    ~QConstString();
    const DeprecatedString &string() const { return *this; }
};

#endif
