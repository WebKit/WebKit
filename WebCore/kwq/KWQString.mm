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

// FIXME: obviously many functions here can be made inline

// #include <Foundation/Foundation.h>
#include <qstring.h>

#ifndef USING_BORROWED_QSTRING

// QString class ===============================================================

// constants -------------------------------------------------------------------

const QString QString::null;

// constructors, copy constructors, and destructors ----------------------------

QString::QString()
{
    s = NULL;
    cache = NULL;
    cacheType = CacheInvalid;
}

QString::QString(QChar qc)
{
    s = CFStringCreateMutable(NULL, 0);
    if (s) {
        CFStringAppendCharacters(s, &qc.c, 1);
    }
    cache = NULL;
    cacheType = CacheInvalid;
}

QString::QString(const QByteArray &qba)
{
    if (qba.size() && *qba.data()) {
        s = CFStringCreateMutable(NULL, 0);
        if (s) {
            const int capacity = 64;
            UniChar buf[capacity];
            int fill = 0;
            for (uint len = 0; (len < qba.size()) && qba[len]; len++) {
                buf[fill] = qba[len];
                fill++;
                if (fill == capacity) {
                    CFStringAppendCharacters(s, buf, fill);
                    fill = 0;
                }
            }
            if (fill) {
                CFStringAppendCharacters(s, buf, fill);
            }
        }
    } else {
        s = NULL;
    }
    cache = NULL;
    cacheType = CacheInvalid;
}

QString::QString(const QChar *qcs, uint len)
{
    if (qcs || len) {
        s = CFStringCreateMutable(NULL, 0);
        if (s) {
            CFStringAppendCharacters(s, &qcs->c, len);
        }
    } else {
        s = NULL;
    }
    cache = NULL;
    cacheType = CacheInvalid;
}

QString::QString(const char *chs)
{
    if (chs && *chs) {
        s = CFStringCreateMutable(NULL, 0);
        if (s) {
            // FIXME: is ISO Latin-1 the correct encoding?
            CFStringAppendCString(s, chs, kCFStringEncodingISOLatin1);
        }
    } else {
        s = NULL;
    }
    cache = NULL;
    cacheType = CacheInvalid;
}

QString::QString(const QString &other)
{
    // shared copy
    if (other.s) {
        CFRetain(other.s);
    }
    s = other.s;
    cache = NULL;
    cacheType = CacheInvalid;
}

QString::~QString()
{
    if (s) {
        CFRelease(s);
    }
    if (cache) {
        CFAllocatorDeallocate(CFAllocatorGetDefault(), cache);
    }
}

// assignment operators --------------------------------------------------------

QString &QString::operator=(const QString &qs)
{
    // shared copy
    if (qs.s) {
        CFRetain(qs.s);
    }
    if (s) {
        CFRelease(s);
    }
    s = qs.s;
    cacheType = CacheInvalid;
    return *this;
}

QString &QString::operator=(const QCString &qcs)
{
    return *this = QString(qcs);
}

QString &QString::operator=(const char *chs)
{
    return *this = QString(chs);
}

QString &QString::operator=(QChar qc)
{
    return *this = QString(qc);
}

QString &QString::operator=(char ch)
{
    return *this = QString(QChar(ch));
}

// member functions ------------------------------------------------------------

QString QString::copy() const
{
    // FIXME: not yet implemented
    return *this;
}

uint QString::length() const
{
    return s ? CFStringGetLength(s) : 0;
}

const QChar *QString::unicode() const
{
    UniChar *ucs = NULL;
    uint len = length();
    if (len) {
        ucs = const_cast<UniChar *>(CFStringGetCharactersPtr(s));
        if (!ucs) {
            // NSLog(@"CFStringGetCharactersPtr returned NULL!!!");
            if (cacheType != CacheUnicode) {
                if (cache) {
                    CFAllocatorDeallocate(CFAllocatorGetDefault(), cache);
                    cache = NULL;
                    cacheType = CacheInvalid;
                }
                if (!cache) {
                    cache = CFAllocatorAllocate(CFAllocatorGetDefault(),
                            len * sizeof (UniChar), 0);
                }
                if (cache) {
                    CFStringGetCharacters(s, CFRangeMake(0, len), cache);
                    cacheType = CacheUnicode;
                }
            }
            ucs = cache;
        }
    }
    // NOTE: this only works since our QChar implementation contains a single
    // UniChar data member
    return reinterpret_cast<const QChar *>(ucs); 
}

QChar QString::at(uint) const
{
    // FIXME: not yet implemented
    return QChar(0);
}

const char *QString::latin1() const
{
    char *chs = NULL;
    uint len = length();
    if (len) {
        // FIXME: is ISO Latin-1 the correct encoding?
        chs = const_cast<char *>(CFStringGetCStringPtr(s,
                    kCFStringEncodingISOLatin1));
        if (!chs) {
            // NSLog(@"CFStringGetCStringPtr returned NULL!!!");
            if (cacheType != CacheLatin1) {
                if (cache) {
                    CFAllocatorDeallocate(CFAllocatorGetDefault(), cache);
                    cache = NULL;
                    cacheType = CacheInvalid;
                }
                if (!cache) {
                    cache = CFAllocatorAllocate(CFAllocatorGetDefault(),
                            len + 1, 0);
                }
                if (cache) {
                    // FIXME: is ISO Latin-1 the correct encoding?
                    if (!CFStringGetCString(s, cache, len + 1,
                                kCFStringEncodingISOLatin1)) {
                        // NSLog(@"CFStringGetCString returned FALSE!!!");
                        *reinterpret_cast<char *>(cache) = '\0';
                    }
                    cacheType = CacheLatin1;
                }
            }
            chs = cache;
        }
    }
    if (!chs) {
        static char emptyString[] = "";
        chs = emptyString;
    }
    return chs; 
}

const char *QString::ascii() const
{
    return latin1(); 
}

bool QString::isNull() const
{
    // NOTE: do NOT use "unicode() == NULL"
    return s == NULL;
}

bool QString::isEmpty() const
{
    return length() == 0;
}

bool QString::startsWith(const QString &) const
{
    // FIXME: not yet implemented
    return FALSE;
}

ushort QString::toUShort() const
{
    // FIXME: not yet implemented
    return 0;
}

int QString::toInt() const
{
    // FIXME: not yet implemented
    return 0;
}

int QString::toInt(bool *, int) const
{
    // FIXME: not yet implemented
    return 0;
}

uint QString::toUInt(bool *, int) const
{
    // FIXME: not yet implemented
    return 0;
}

long QString::toLong(bool *, int) const
{
    // FIXME: not yet implemented
    return 0;
}

float QString::toFloat(bool *) const
{
    // FIXME: not yet implemented
    return 0.0f;
}

QString &QString::prepend(const QString &)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::append(const char *)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::append(const QString &)
{
    // FIXME: not yet implemented
    return *this;
}

int QString::contains(const char *, bool) const
{
    // FIXME: not yet implemented
    return 0;
}

int QString::contains(char) const
{
    // FIXME: not yet implemented
    return 0;
}

int QString::find(char, int, bool) const
{
    // FIXME: not yet implemented
    return 0;
}

int QString::find(QChar, int, bool) const
{
    // FIXME: not yet implemented
    return 0;
}

int QString::find(const QString &, int, bool) const
{
    // FIXME: not yet implemented
    return 0;
}

int QString::find(const QRegExp &, int, bool) const
{
    // FIXME: not yet implemented
    return 0;
}

int QString::findRev(char, int, bool) const
{
    // FIXME: not yet implemented
    return 0;
}

int QString::findRev(const char *, int, bool) const
{
    // FIXME: not yet implemented
    return 0;
}

QString &QString::remove(uint, uint)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::replace(const QRegExp &, const QString &)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::insert(uint, char)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::insert(uint, QChar)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::insert(uint, const QString &)
{
    // FIXME: not yet implemented
    return *this;
}

void QString::truncate(uint)
{
    // FIXME: not yet implemented
}

void QString::fill(QChar, int)
{
    // FIXME: not yet implemented
}

QString QString::arg(int, int, int) const
{
    // FIXME: not yet implemented
    return QString(*this);
}

QString QString::arg(const QString &, int) const
{
    // FIXME: not yet implemented
    return QString(*this);
}

QString QString::left(uint) const
{
    // FIXME: not yet implemented
    return QString(*this);
}

QString QString::right(uint) const
{
    // FIXME: not yet implemented
    return QString(*this);
}

QString QString::mid(int, int) const
{
    // FIXME: not yet implemented
    return QString(*this);
}

int QString::compare(const QString &) const
{
    // FIXME: not yet implemented
    return 0;
}

QString QString::fromLatin1(const char *, int)
{
    // FIXME: not yet implemented
    return QString();
}

QString QString::fromLocal8Bit(const char *, int)
{
    // FIXME: not yet implemented
    return QString();
}

QCString QString::utf8() const
{
    // FIXME: not yet implemented
    return QCString();
}

QCString QString::local8Bit() const
{
    // FIXME: not yet implemented
    return QCString();
}

QString &QString::setUnicode(const QChar *, uint)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::setNum(int, int)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::sprintf(const char *, ...)
{
    // FIXME: not yet implemented
    return *this;
}

QString QString::lower() const
{
    // FIXME: not yet implemented
    return QString(*this);
}

QString QString::stripWhiteSpace() const
{
    // FIXME: not yet implemented
    return QString(*this);
}

QString QString::simplifyWhiteSpace() const
{
    // FIXME: not yet implemented
    return QString(*this);
}

void QString::compose()
{
    // FIXME: not yet implemented
}

QString QString::visual(int index=0, int len=-1)
{
    // FIXME: not yet implemented
    return *this;
}

// operators -------------------------------------------------------------------

bool QString::operator!() const
{ 
    return isNull(); 
}

QString::operator const char *() const
{
    return latin1();
}

QChar QString::operator[](int) const
{
    // FIXME: not yet implemented
    return 0;
}

QString &QString::operator+(char)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::operator+(QChar)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::operator+(const QString &)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::operator+=(char)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::operator+=(QChar)
{
    // FIXME: not yet implemented
    return *this;
}

QString &QString::operator+=(const QString &)
{
    // FIXME: not yet implemented
    return *this;
}

QString::operator QChar() const
{
    // FIXME: not yet implemented
    return QChar();
}


// operators associated with QChar and QString =================================

/*
QString &operator+(const char *, const QString &)
{
    // FIXME: not yet implemented
}

QString &operator+(QChar, const QString &)
{
    // FIXME: not yet implemented
}
*/

bool operator==(const QString &, QChar)
{
    // FIXME: not yet implemented
    return FALSE;
}

bool operator==(const QString &, const QString &)
{
#if 0
    CFComparisonResult cmp;
    int flags;

    flags = 0;

    cmp = CFStringCompare(s1.s, s2.s, flags);
    
    return (cmp == kCFCompareEqualTo);
#endif
    // FIXME: not yet implemented
    return FALSE;
}

bool operator==(const QString &, const char *)
{
    // FIXME: not yet implemented
    return FALSE;
}

bool operator==(const char *, const QString &)
{
    // FIXME: not yet implemented
    return FALSE;
}

bool operator!=(const QString &s, QChar c)
{
    // FIXME: not yet implemented
    return FALSE;
}

bool operator!=(const QString &, const QString &)
{
#if 0
    // FIXME: awaiting real implementation
    CFComparisonResult cmp;
    int flags;

    flags = 0;

    cmp = CFStringCompare(s1.s, s2.s, flags);
    
    return (cmp != kCFCompareEqualTo);
#endif
    // FIXME: not yet implemented
    return FALSE;
}

bool operator!=(const QString &, const char *)
{
    // FIXME: not yet implemented
    return FALSE;
}

bool operator!=(const char *, const QString &)
{
    // FIXME: not yet implemented
    return FALSE;
}

QString operator+(char, const QString &)
{
    // FIXME: not yet implemented
    return QString();
}


// class QConstString ==========================================================

// constructors, copy constructors, and destructors ----------------------------

QConstString::QConstString(QChar *qcs, uint len)
{
    if (qcs || len) {
        // NOTE: use instead of CFStringCreateWithCharactersNoCopy function to
        // guarantee backing store is not copied even though string is mutable
        s = CFStringCreateMutableWithExternalCharactersNoCopy(NULL, &qcs->c,
                len, len, kCFAllocatorNull);
    } else {
        s = NULL;
    }
}

// member functions ------------------------------------------------------------

const QString &QConstString::string() const
{
    return *this;
}

#else // USING_BORROWED_QSTRING
// This will help to keep the linker from complaining about empty archives
void KWQString_Dummy() {}
#endif // USING_BORROWED_QSTRING
