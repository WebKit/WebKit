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

#include <qstring.h>

#ifndef USING_BORROWED_QSTRING

const QString QString::null;

// constructors, copy constructors, and destructors ------------------------

QString::QString()
{
    s = NULL;
}

QString::QString(QChar qc)
{
    s = CFStringCreateMutable(NULL, 0);
    CFStringAppendCharacters(s, &qc.c, 1);
}

QString::QString(const QByteArray &qba)
{
    if (qba.size() && *qba.data()) {
        s = CFStringCreateMutable(NULL, 0);
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
    } else {
        s = NULL;
    }
}

QString::QString(const QChar *qcs, uint len)
{
    if (qcs || len) {
        s = CFStringCreateMutable(NULL, 0);
        CFStringAppendCharacters(s, &qcs->c, len);
    } else {
        s = NULL;
    }
}

QString::QString(const char *chs)
{
    if (chs && *chs) {
        s = CFStringCreateMutable(NULL, 0);
        // FIXME: is ISO Latin-1 the correct encoding?
        CFStringAppendCString(s, chs, kCFStringEncodingISOLatin1);
    } else {
        s = NULL;
    }
}

QString::QString(const QString &other)
{
    s = other.s;
}

QString::~QString()
{
    CFRelease(s);
}

// assignment operators ----------------------------------------------------

QString &QString::operator=(const QString &qs)
{
    // shared copy
    CFRetain(qs.s);
    CFRelease(s);
    s = qs.s;
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

// member functions --------------------------------------------------------

bool QString::isNull() const
{
    return (s == NULL);
}

bool QString::isEmpty() const
{
    return (s == NULL || CFStringGetLength(s) == 0);
}

uint QString::length() const
{
    return CFStringGetLength(s);
}

bool QString::startsWith(const QString &s) const
{
    // FIXME: awaiting real implementation
    return FALSE;
}

int QString::toInt() const
{
    // FIXME: awaiting real implementation
    return 0;
}

int QString::toInt(bool *b, int base=10) const
{
    // FIXME: awaiting real implementation
    return 0;
}

uint QString::toUInt(bool *ok=0, int base=10) const
{
    // FIXME: awaiting real implementation
    return 0;
}

long QString::toLong(bool *ok=0, int base=10) const
{
    // FIXME: awaiting real implementation
    return 0L;
}

float QString::toFloat(bool *b=0) const
{
    // FIXME: awaiting real implementation
    return 0.0f;
}

QString &QString::prepend(const QString &s)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString &QString::append(const char *s)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString &QString::append(const QString &s)
{
    // FIXME: awaiting real implementation
    return *this;
}

int QString::contains(const char *c, bool cs=TRUE) const
{
    // FIXME: awaiting real implementation
    return 0;
}

int QString::contains(char c) const
{
    // FIXME: awaiting real implementation
    return 0;
}

int QString::find(char c, int index=0) const
{
    // FIXME: awaiting real implementation
    return 0;
}

int QString::find(const char *s, int index=0, bool b=0) const
{
    // FIXME: awaiting real implementation
    return 0;
}

int QString::find(const QString &s, int index=0, bool b=0) const
{
    // FIXME: awaiting real implementation
    return 0;
}

int QString::find(const QRegExp &r, int index=0, bool b=0) const
{
    // FIXME: awaiting real implementation
    return 0;
}

int QString::findRev(char c, int index=0) const
{
    // FIXME: awaiting real implementation
    return 0;
}

int QString::findRev(const char *s, int index=0) const
{
    // FIXME: awaiting real implementation
    return 0;
}

QString &QString::remove(uint u1, uint u2)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString &QString::replace(const QRegExp &, const QString &)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString &QString::insert(uint i, char c)
{
    // FIXME: awaiting real implementation
    return *this;
}

void QString::truncate(uint pos)
{
    // FIXME: awaiting real implementation
}

void QString::fill(QChar c, int len=-1)
{
    // FIXME: awaiting real implementation
}

QString QString::arg(int &a)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString QString::arg(int a, int fieldwidth=0, int base=10) const
{
    // FIXME: awaiting real implementation
    return *this;
}

QString QString::arg(const QString &s, int fieldwidth=0) const
{
    // FIXME: awaiting real implementation
    return *this;
}

QString QString::left(uint pos) const
{
    // FIXME: awaiting real implementation
    return *this;
}

QString QString::right(uint pos) const
{
    // FIXME: awaiting real implementation
    return *this;
}

QString QString::mid(int pos, int len=0xffffffff) const
{
    // FIXME: awaiting real implementation
    return *this;
}

QString QString::fromLatin1(const char *s, int len=-1)
{
    // FIXME: awaiting real implementation
    return NULL;
}

const char *QString::latin1() const
{
    // FIXME: awaiting real implementation
    return (const char *)CFStringGetCStringPtr(s, kCFStringEncodingISOLatin1); 
}

const char *QString::ascii() const
{
    // FIXME: awaiting real implementation
    return NULL;
}

const QChar *QString::unicode() const
{
    // FIXME: awaiting real implementation
    return (QChar *)CFStringGetCharactersPtr(s); 
}

QCString QString::utf8() const
{
    // FIXME: awaiting real implementation
    return 0;
}

QCString QString::local8Bit() const
{
    // FIXME: awaiting real implementation
    return 0;
}

QString &QString::setUnicode(const QChar *s, uint i)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString &QString::setNum(int i, int base=10)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString &QString::sprintf(const char *s, ...)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString QString::lower() const
{
    // FIXME: awaiting real implementation
    QString result(*this);

    CFStringLowercase(result.s, NULL);

    return result;
}

QString QString::stripWhiteSpace() const
{
    // FIXME: awaiting real implementation
    return *this;
}

QString QString::simplifyWhiteSpace() const
{
    // FIXME: awaiting real implementation
    return *this;
}

void QString::compose()
{
    // FIXME: awaiting real implementation
}

QString QString::visual(int index=0, int len=-1)
{
    // FIXME: awaiting real implementation
    return *this;
}

bool QString::operator!() const
{ 
    return isNull(); 
}

QString::operator const char *() const
{
    return latin1();
}

QChar QString::operator[](int i) const
{
    // FIXME: awaiting real implementation
    return 0;
}

QString &QString::operator+(char c)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString &QString::operator+(QChar c)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString &QString::operator+(const QString &s)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString &QString::operator+=(char c)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString &QString::operator+=(QChar c)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString &QString::operator+=(const QString &s)
{
    // FIXME: awaiting real implementation
    return *this;
}

QString::operator QChar() const
{
    // FIXME: awaiting real implementation
    return 0;
}

/*
QString &operator+(const char *s1, const QString &s2)
{
    // FIXME: awaiting real implementation
}

QString &operator+(QChar c, const QString &s)
{
    // FIXME: awaiting real implementation
}
*/

bool operator==(const QString &s, QChar c)
{
    // FIXME: awaiting real implementation
    return FALSE;
}

bool operator==(const QString &s1, const QString &s2)
{
    // FIXME: awaiting real implementation
    CFComparisonResult cmp;
    int flags;

    flags = 0;

    cmp = CFStringCompare(s1.s, s2.s, flags);
    
    return (cmp == kCFCompareEqualTo);
}

bool operator==(const QString &s1, const char *s2)
{
    // FIXME: awaiting real implementation
    return FALSE;
}

bool operator==(const char *s1, const QString &s2)
{
    // FIXME: awaiting real implementation
    return FALSE;
}

bool operator!=(const QString &s, QChar c)
{
    // FIXME: awaiting real implementation
    return FALSE;
}

bool operator!=(const QString &s1, const QString &s2)
{
    // FIXME: awaiting real implementation
    CFComparisonResult cmp;
    int flags;

    flags = 0;

    cmp = CFStringCompare(s1.s, s2.s, flags);
    
    return (cmp != kCFCompareEqualTo);
}

bool operator!=(const QString &s1, const char *s2)
{
    // FIXME: awaiting real implementation
    return FALSE;
}

bool operator!=(const char *s1, const QString &s2)
{
    // FIXME: awaiting real implementation
    return FALSE;
}

QString operator+(char c, const QString &s)
{
    // FIXME: awaiting real implementation
    return FALSE;
}

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

const QString &QConstString::string() const
{
    return *this;
}

#else // USING_BORROWED_QSTRING
// This will help to keep the linker from complaining about empty archives
void KWQString_Dummy() {}
#endif // USING_BORROWED_QSTRING
