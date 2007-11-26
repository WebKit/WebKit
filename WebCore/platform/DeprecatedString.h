/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef DeprecatedString_h
#define DeprecatedString_h

#include "DeprecatedCString.h"
#include <wtf/ASCIICType.h>
#include <wtf/unicode/Unicode.h>

/* On some ARM platforms GCC won't pack structures by default so sizeof(DeprecatedChar)
   will end up being != 2 which causes crashes since the code depends on that. */
#if COMPILER(GCC) && PLATFORM(FORCE_PACK)
#define PACK_STRUCT __attribute__((packed))
#else
#define PACK_STRUCT
#endif

#if PLATFORM(CF)
typedef const struct __CFString * CFStringRef;
#endif

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif
#endif

#if PLATFORM(QT)
class QString;
#endif

#if PLATFORM(WX)
class wxString;
#endif

namespace KJS {
    class Identifier;
    class UString;
}

namespace WebCore {

class RegularExpression;

class DeprecatedChar {
public:
    DeprecatedChar();
    DeprecatedChar(char);
    DeprecatedChar(unsigned char);
    DeprecatedChar(short);
    DeprecatedChar(unsigned short);
    DeprecatedChar(int);
    DeprecatedChar(unsigned);

    unsigned short unicode() const;
    char latin1() const;
    bool isSpace() const;
    DeprecatedChar lower() const;
    DeprecatedChar upper() const;

private:
    unsigned short c;
} PACK_STRUCT;

inline DeprecatedChar::DeprecatedChar() : c(0)
{
}

inline DeprecatedChar::DeprecatedChar(char ch) : c((unsigned char) ch)
{
}

inline DeprecatedChar::DeprecatedChar(unsigned char uch) : c(uch)
{
}

inline DeprecatedChar::DeprecatedChar(short n) : c(n)
{
}

inline DeprecatedChar::DeprecatedChar(unsigned short n) : c(n)
{
}

inline DeprecatedChar::DeprecatedChar(unsigned n) : c(n)
{
}

inline DeprecatedChar::DeprecatedChar(int n) : c(n)
{
}

inline unsigned short DeprecatedChar::unicode() const
{
    return c;
}

inline bool DeprecatedChar::isSpace() const
{
#if USE(ICU_UNICODE)
    // Use isspace() for basic Latin-1.
    // This will include newlines, which aren't included in unicode DirWS.
    return c <= 0x7F ? WTF::isASCIISpace(c) : (u_charDirection(c) == U_WHITE_SPACE_NEUTRAL);
#elif USE(QT4_UNICODE)
    return QChar(c).isSpace();
#endif
}

inline DeprecatedChar DeprecatedChar::lower() const
{
#if USE(ICU_UNICODE)
    // FIXME: If fast enough, we should just call u_tolower directly.
    return c <= 0x7F ? WTF::toASCIILower(c) : u_tolower(c);
#elif USE(QT4_UNICODE)
    return QChar(c).toLower().unicode();
#endif
}

inline DeprecatedChar DeprecatedChar::upper() const
{
#if USE(ICU_UNICODE)
    // FIXME: If fast enough, we should just call u_toupper directly.
    return c <= 0x7F ? WTF::toASCIIUpper(c) : u_toupper(c);
#elif USE(QT4_UNICODE)
    return QChar(c).toUpper().unicode();
#endif
}

inline char DeprecatedChar::latin1() const
{
    return c > 0xff ? 0 : c;
}

inline bool operator==(DeprecatedChar qc1, DeprecatedChar qc2)
{
    return qc1.unicode() == qc2.unicode();
}

inline bool operator==(DeprecatedChar qc, char ch)
{
    return qc.unicode() == (unsigned char) ch;
}

inline bool operator==(char ch, DeprecatedChar qc)
{
    return (unsigned char) ch == qc.unicode();
}

inline bool operator!=(DeprecatedChar qc1, DeprecatedChar qc2)
{
    return qc1.unicode() != qc2.unicode();
}

inline bool operator!=(DeprecatedChar qc, char ch)
{
    return qc.unicode() != (unsigned char) ch;
}

inline bool operator!=(char ch, DeprecatedChar qc)
{
    return (unsigned char) ch != qc.unicode();
}

// Keep this struct to <= 46 bytes, that's what the system will allocate.
// Will be rounded up to a multiple of 4, so we're stuck at 44.

#define WEBCORE_DS_INTERNAL_BUFFER_SIZE 20
#define WEBCORE_DS_INTERNAL_BUFFER_CHARS WEBCORE_DS_INTERNAL_BUFFER_SIZE-1
#define WEBCORE_DS_INTERNAL_BUFFER_UCHARS WEBCORE_DS_INTERNAL_BUFFER_SIZE/2

struct DeprecatedStringData  
{
    // Uses shared null data.
    DeprecatedStringData();
    void initialize();
    
    // No copy.
    DeprecatedStringData(DeprecatedChar *u, unsigned l, unsigned m);
    void initialize(DeprecatedChar *u, unsigned l, unsigned m);
    
    // Copy bytes.
    DeprecatedStringData(const DeprecatedChar *u, unsigned l);
    void initialize(const DeprecatedChar *u, unsigned l);

    // Copy bytes.
    DeprecatedStringData(const char *u, unsigned l);
    void initialize(const char *u, unsigned l);

    // Move from destination to source.
    static DeprecatedStringData* createAndAdopt(DeprecatedStringData &);

    ~DeprecatedStringData();

#ifdef WEBCORE_DS_DEBUG_ALLOCATIONS
    void* operator new(size_t s);
    void operator delete(void*p);
#endif

    inline void ref() { refCount++; }
    inline void deref() { if (--refCount == 0 && _isHeapAllocated) delete this; }
        
    char *ascii();
    char *makeAscii();
    void increaseAsciiSize(unsigned size);

    DeprecatedChar *unicode();
    DeprecatedChar *makeUnicode();    
    void increaseUnicodeSize(unsigned size);
    
    bool isUnicodeInternal() const { return (char *)_unicode == _internalBuffer; }
    bool isAsciiInternal() const { return _ascii == _internalBuffer; }

    unsigned refCount;
    unsigned _length;
    mutable DeprecatedChar *_unicode;
    mutable char *_ascii;

    unsigned _maxUnicode : 30;
    bool _isUnicodeValid : 1;
    bool _isHeapAllocated : 1; // Fragile, but the only way we can be sure the instance was created with 'new'.
    unsigned _maxAscii : 31;
    bool _isAsciiValid : 1;

    // _internalBuffer must be at the end - otherwise it breaks on archs that
    // don't pack structs on byte boundary, like some versions of gcc on ARM
    char _internalBuffer[WEBCORE_DS_INTERNAL_BUFFER_SIZE]; // Pad out to a (((size + 1) & ~15) + 14) size
    
private:
    void adopt(DeprecatedStringData&);

    DeprecatedStringData(const DeprecatedStringData &);
    DeprecatedStringData &operator=(const DeprecatedStringData &);
};

class DeprecatedString;

bool operator==(const DeprecatedString&, const DeprecatedString&);
bool operator==(const DeprecatedString&, const char*);

class DeprecatedString {
public:
    static const char * const null;

    DeprecatedString();
    DeprecatedString(DeprecatedChar);
    DeprecatedString(const DeprecatedChar *, unsigned);
    DeprecatedString(const char *);
    DeprecatedString(const char *, int len);
    DeprecatedString(const KJS::Identifier&);
    DeprecatedString(const KJS::UString&);
    
    DeprecatedString(const DeprecatedString &);
    DeprecatedString &operator=(const DeprecatedString &);

    ~DeprecatedString();

    operator KJS::Identifier() const;
    operator KJS::UString() const;

#if PLATFORM(QT)
    DeprecatedString(const QString&);
    operator QString() const;
#endif

    static DeprecatedString fromLatin1(const char *);
    static DeprecatedString fromLatin1(const char *, int len);
    static DeprecatedString fromUtf8(const char *);
    static DeprecatedString fromUtf8(const char *, int len);
#if PLATFORM(CF)
    static DeprecatedString fromCFString(CFStringRef);
#endif
#if PLATFORM(MAC)
    static DeprecatedString fromNSString(NSString*);
#endif
#if PLATFORM(SYMBIAN)
    static DeprecatedString fromDes(const TDesC&);
    static DeprecatedString fromDes(const TDesC8&);
#endif
    DeprecatedString &operator=(char);
    DeprecatedString &operator=(DeprecatedChar);
    DeprecatedString &operator=(const char *);
    DeprecatedString &operator=(const DeprecatedCString &);

    unsigned length() const;

    const DeprecatedChar *unicode() const;
    const DeprecatedChar *stableUnicode();
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

    DeprecatedChar at(unsigned) const;

    int compare(const DeprecatedString &) const;
    int compare(const char *) const;

    bool startsWith(const DeprecatedString &) const;
    bool startsWith(const char *) const;
    bool startsWith(const char *, bool caseSensitive) const;

    int find(char, int index = 0) const;
    int find(DeprecatedChar, int index = 0) const;
    int find(const char *, int index = 0, bool cs = true) const;
    int find(const DeprecatedString &, int index = 0, bool cs = true) const;
    int find(const RegularExpression &, int index = 0) const;

    int findRev(char, int index = -1) const;
    int findRev(const DeprecatedString& str, int index, bool cs = true) const;
    int findRev(const char *, int index = -1) const;

    int contains(char) const;
    int contains(const char *, bool cs = true) const;
    int contains(const DeprecatedString &, bool cs = true) const;
    int contains(DeprecatedChar c, bool cs = true) const;

    bool endsWith(const DeprecatedString &) const;

    short toShort(bool *ok = 0, int base = 10) const;
    unsigned short toUShort(bool *ok = 0, int base = 10) const;
    int toInt(bool *ok = 0, int base = 10) const;
    int64_t toInt64(bool *ok = 0, int base = 10) const;
    unsigned toUInt(bool *ok = 0, int base = 10) const;
    uint64_t toUInt64(bool *ok = 0, int base = 10) const;

    double toDouble(bool *ok = 0) const;
    float toFloat(bool* ok = 0) const;

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

    DeprecatedString &setUnicode(const DeprecatedChar *, unsigned);
    DeprecatedString &setLatin1(const char *, int len=-1);

    DeprecatedString &setNum(short);
    DeprecatedString &setNum(unsigned short);
    DeprecatedString &setNum(int);
    DeprecatedString &setNum(unsigned);
    DeprecatedString &setNum(long);
    DeprecatedString &setNum(unsigned long);
    DeprecatedString &setNum(double);

    DeprecatedString& format(const char *, ...) WTF_ATTRIBUTE_PRINTF(2, 3);

    DeprecatedString &append(const DeprecatedString &);
    DeprecatedString &append(DeprecatedChar);
    DeprecatedString &append(char);
    DeprecatedString &insert(unsigned, const DeprecatedString &);
    DeprecatedString &insert(unsigned, DeprecatedChar);
    DeprecatedString &insert(unsigned, char);
    DeprecatedString &insert(unsigned index, const char *insertChars, unsigned insertLength);
    DeprecatedString &prepend(const DeprecatedString &);
    DeprecatedString &remove(unsigned, unsigned);
    DeprecatedString &remove(const DeprecatedChar &c) { return replace(DeprecatedString(c), ""); }
    DeprecatedString &remove(const DeprecatedString &s) { return replace(s, ""); }
    DeprecatedString &replace(unsigned index, unsigned len, const DeprecatedString &s);
    DeprecatedString &replace(char, const DeprecatedString &);
    DeprecatedString &replace(DeprecatedChar, const DeprecatedString &);
    DeprecatedString &replace(const DeprecatedString &, const DeprecatedString &);
    DeprecatedString &replace(const RegularExpression &, const DeprecatedString &);
    DeprecatedString &replace(DeprecatedChar, DeprecatedChar);

    DeprecatedString &append(const DeprecatedChar *, unsigned length);
    DeprecatedString &append(const char *, unsigned length);
    DeprecatedString &insert(unsigned position, const DeprecatedChar *, unsigned length);
    DeprecatedString &prepend(const DeprecatedChar *, unsigned length);
    
    void fill(DeprecatedChar, int len=-1);
    void truncate(unsigned);

    void reserve(unsigned);

    bool operator!() const;

    const DeprecatedChar operator[](int) const;

    DeprecatedString &operator+=(const DeprecatedString &s) { return append(s); }
    DeprecatedString &operator+=(DeprecatedChar c) { return append(c); }
    DeprecatedString &operator+=(char c) { return append(c); }

#if PLATFORM(CF)
    CFStringRef getCFString() const;
    void setBufferFromCFString(CFStringRef);
#endif
    
#if PLATFORM(MAC)
    NSString *getNSString() const;

#ifdef __OBJC__
    operator NSString*() const { return getNSString(); }
#endif

#endif

#if PLATFORM(WX)
    operator wxString() const;
#endif

#if PLATFORM(SYMBIAN)
    TPtrC des() const;
    TPtrC8 des8() const;
    void setBufferFromDes(const TDesC&);
    void setBufferFromDes(const TDesC8&);
#endif

private:
    // Used by DeprecatedConstString.
    DeprecatedString(DeprecatedStringData *constData, bool /*dummy*/);
    void detach();
    void detachAndDiscardCharacters();
    void detachIfInternal();
    void detachInternal();
    void deref();
    DeprecatedChar *forceUnicode();
    void setLength(unsigned);

    DeprecatedStringData **dataHandle;
    DeprecatedStringData internalData;
    
    static DeprecatedStringData *shared_null;
    static DeprecatedStringData *makeSharedNull();
    static DeprecatedStringData **shared_null_handle;
    static DeprecatedStringData **makeSharedNullHandle();

    friend bool operator==(const DeprecatedString &, const DeprecatedString &);
    friend bool operator==(const DeprecatedString &, const char *);
    friend bool equalIgnoringCase(const DeprecatedString&, const DeprecatedString&);

    friend class DeprecatedConstString;
    friend class QGDict;
    friend struct DeprecatedStringData;
};

DeprecatedString operator+(const DeprecatedString &, const DeprecatedString &);
DeprecatedString operator+(const DeprecatedString &, const char *);
DeprecatedString operator+(const DeprecatedString &, DeprecatedChar);
DeprecatedString operator+(const DeprecatedString &, char);
DeprecatedString operator+(const char *, const DeprecatedString &);
DeprecatedString operator+(DeprecatedChar, const DeprecatedString &);
DeprecatedString operator+(char, const DeprecatedString &);

bool equalIgnoringCase(const DeprecatedString&, const DeprecatedString&);
inline bool equalIgnoringCase(const DeprecatedString& a, const char* b) { return equalIgnoringCase(a, DeprecatedString(b)); }
inline bool equalIgnoringCase(const char* a, const DeprecatedString& b) { return equalIgnoringCase(DeprecatedString(a), b); }

inline char *DeprecatedStringData::ascii()
{
    return _isAsciiValid ? _ascii : makeAscii();
}

inline DeprecatedChar *DeprecatedStringData::unicode()
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

inline const DeprecatedChar *DeprecatedString::unicode() const
{
    return dataHandle[0]->unicode();
}

#if PLATFORM(MAC)
#if PLATFORM(CF)
inline CFStringRef DeprecatedString::getCFString() const
{
    return (CFStringRef)getNSString();
}
#endif
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

inline const DeprecatedChar DeprecatedString::operator[](int index) const
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

class DeprecatedConstString : private DeprecatedString {
public:
    DeprecatedConstString(const DeprecatedChar *, unsigned);
    ~DeprecatedConstString();
    const DeprecatedString &string() const { return *this; }
};

}

#endif
