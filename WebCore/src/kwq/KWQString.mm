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

#include <Foundation/Foundation.h>
#include <qstring.h>
#include <qregexp.h>
#include <stdio.h>

#ifndef USING_BORROWED_QSTRING

// QString class ===============================================================

// constants -------------------------------------------------------------------

const QString QString::null;

// static member functions -----------------------------------------------------

QString QString::number(int n)
{
    QString qs;
    qs.setNum(n);
    return qs;
}

QString QString::fromLatin1(const char *chs)
{
    return QString(chs);
}

#ifdef USING_BORROWED_KURL
QString QString::fromLocal8Bit(const char *chs, int len)
{
    QString qs;
    if (chs && *chs) {
        qs.s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        if (qs.s) {
            if (len < 0) {
                // append null-terminated string
                // FIXME: is MacRoman the correct encoding?
                CFStringAppendCString(qs.s, chs, kCFStringEncodingMacRoman);
            } else {
                // append length-specified string
                // FIXME: this uses code similar to that in the
                // "QString(const QByteArray &)" constructor and could possibly
                // be refactored
                const int capacity = 64;
                UniChar buf[capacity];
                int fill = 0;
                for (uint i = 0; (i < len) && chs[i]; i++) {
                    buf[fill] = chs[i];
                    fill++;
                    if (fill == capacity) {
                        CFStringAppendCharacters(qs.s, buf, fill);
                        fill = 0;
                    }
                }
                // append any remainder in buffer
                if (fill) {
                    CFStringAppendCharacters(qs.s, buf, fill);
                }
            }
        }
    }
    return qs;
}
#endif // USING_BORROWED_KURL

// constructors, copy constructors, and destructors ----------------------------

QString::QString()
{
    s = NULL;
    cache = NULL;
    cacheType = CacheInvalid;
}

QString::QString(QChar qc)
{
    s = CFStringCreateMutable(kCFAllocatorDefault, 0);
    if (s) {
        // NOTE: this only works since our QChar implementation contains a
        // single UniChar data member
        CFStringAppendCharacters(s, &qc.c, 1);
    }
    cache = NULL;
    cacheType = CacheInvalid;
}

QString::QString(const QByteArray &qba)
{
    if (qba.size() && *qba.data()) {
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        if (s) {
            // FIXME: this uses code similar to that in the "fromLocal8Bit"
            // function and could possibly be refactored
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
            // append any remainder in buffer
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
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
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
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
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

QString::QString(const QString &qs)
{
    // shared copy
    if (qs.s) {
        CFRetain(qs.s);
    }
    s = qs.s;
    cache = NULL;
    cacheType = CacheInvalid;
}

QString::~QString()
{
    if (s) {
        CFRelease(s);
    }
    if (cache) {
        CFAllocatorDeallocate(kCFAllocatorDefault, cache);
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
#ifdef _KWQ_DEBUG_
            NSLog(@"WARNING %s:%s:%d (CFStringGetCharactersPtr failed)\n",
                    __FILE__, __FUNCTION__, __LINE__);
#endif
            if (cacheType != CacheUnicode) {
                flushCache();
                cache = CFAllocatorAllocate(kCFAllocatorDefault,
                        len * sizeof (UniChar), 0);
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

const char *QString::latin1() const
{
    char *chs = NULL;
    uint len = length();
    if (len) {
        // FIXME: is ISO Latin-1 the correct encoding?
        chs = const_cast<char *>(CFStringGetCStringPtr(s,
                    kCFStringEncodingISOLatin1));
        if (!chs) {
#ifdef _KWQ_DEBUG_
            NSLog(@"WARNING %s:%s:%d (CFStringGetCharactersPtr failed)\n",
                    __FILE__, __FUNCTION__, __LINE__);
#endif
            if (cacheType != CacheLatin1) {
                flushCache();
                cache = CFAllocatorAllocate(kCFAllocatorDefault, len + 1, 0);
                if (cache) {
                    // FIXME: is ISO Latin-1 the correct encoding?
                    if (!CFStringGetCString(s, cache, len + 1,
                                kCFStringEncodingISOLatin1)) {
#ifdef _KWQ_DEBUG_
                        NSLog(@"WARNING %s:%s:%d (CFStringGetCString failed)\n",
                                __FILE__, __FUNCTION__, __LINE__);
#endif
                        *reinterpret_cast<char *>(cache) = '\0';
                    }
                    cacheType = CacheLatin1;
                }
            }
            chs = cache;
        }
    }
    // always return a valid pointer
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

QCString QString::utf8() const
{
    return convertToQCString(kCFStringEncodingUTF8);
}

QCString QString::local8Bit() const
{
    return convertToQCString(kCFStringEncodingMacRoman);
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

#ifdef USING_BORROWED_KURL
QChar QString::at(uint i) const
{
    return operator[](i);
}
#endif // USING_BORROWED_KURL

int QString::compare(const QString &qs) const
{
    if (s == qs.s) {
        return kCFCompareEqualTo;
    }
    if (!s) {
        return kCFCompareLessThan;
    }
    if (!qs.s) {
        return kCFCompareGreaterThan;
    }
    return CFStringCompare(s, qs.s, 0);
}

bool QString::startsWith(const QString &qs) const
{
    if (s && qs.s) {
        return CFStringHasPrefix(s, qs.s);
    }
    return FALSE;
}

int QString::find(QChar qc, int index) const
{
    if (s && qc.c) {
        CFIndex len = CFStringGetLength(s);
        if (index < 0) {
            index += len;
        }
        if (len && (index >= 0) && (index < len)) {
            CFStringInlineBuffer buf;
            CFStringInitInlineBuffer(s, &buf, CFRangeMake(0, len));
            for (CFIndex i = index; i < len; i++) {
                if (qc.c == CFStringGetCharacterFromInlineBuffer(&buf, i)) {
                    return i;
                }
            }
        }
    }
    return -1;
}

int QString::find(char ch, int index) const
{
    return find(QChar(ch), index);
}

int QString::find(const QString &qs, int index) const
{
    if (s && qs) {
        CFIndex len = CFStringGetLength(s);
        if (index < 0) {
            index += len;
        }
        if (len && (index >= 0) && (index < len)) {
            CFRange r = CFStringFind(s, qs.s, 0);
            if (r.location != kCFNotFound) {
                return r.location;
            }
        }
    }
    return -1;
}

int QString::find(const char *chs, int index, bool cs) const
{
    int pos = -1;
    if (s && chs) {
        CFIndex len = CFStringGetLength(s);
        if (index < 0) {
            index += len;
        }
        if (len && (index >= 0) && (index < len)) {
            // FIXME: is ISO Latin-1 the correct encoding?
            CFStringRef tmp = CFStringCreateWithCStringNoCopy(
                    kCFAllocatorDefault, chs, kCFStringEncodingISOLatin1,
                    kCFAllocatorDefault);
            if (tmp) {
                CFRange r;
                if (CFStringFindWithOptions(s, tmp,
                        CFRangeMake(index, len - index),
                        cs ? 0 : kCFCompareCaseInsensitive, &r)) {
                    pos = r.location;
                }
                CFRelease(tmp);
            }
        }
    }
    return pos;
}

int QString::find(const QRegExp &qre, int index) const
{
    if (s) {
        CFIndex len = CFStringGetLength(s);
        if (index < 0) {
            index += len;
        }
        if (len && (index >= 0) && (index < len)) {
            return qre.match(*this, index);
        }
    }
    return -1;
}

int QString::findRev(char ch, int index) const
{
    if (s && ch) {
        CFIndex len = CFStringGetLength(s);
        if (index < 0) {
            index += len;
        }
        if (len && (index >= 0) && (index < len)) {
            CFStringInlineBuffer buf;
            CFStringInitInlineBuffer(s, &buf, CFRangeMake(0, len));
            for (CFIndex i = index; i >= 0; i--) {
                if (ch == CFStringGetCharacterFromInlineBuffer(&buf, i)) {
                    return i;
                }
            }
        }
    }
    return -1;
}

int QString::findRev(const char *chs, int index) const
{
    int pos = -1;
    if (s && chs) {
        CFIndex len = CFStringGetLength(s);
        if (index < 0) {
            index += len;
        }
        if (len && (index >= 0) && (index < len)) {
            // FIXME: is ISO Latin-1 the correct encoding?
            CFStringRef tmp = CFStringCreateWithCStringNoCopy(
                    kCFAllocatorDefault, chs, kCFStringEncodingISOLatin1,
                    kCFAllocatorDefault);
            if (tmp) {
                CFRange r;
                // FIXME: is this the right way to specifiy a range for a
                // reversed search?
                if (CFStringFindWithOptions(s, tmp, CFRangeMake(0, index + 1),
                        kCFCompareBackwards, &r)) {
                    pos = r.location;
                }
                CFRelease(tmp);
            }
        }
    }
    return pos;
}

int QString::contains(char ch) const
{
    int c = 0;
    if (s && ch) {
        CFIndex len = CFStringGetLength(s);
        if (len) {
            CFStringInlineBuffer buf;
            CFStringInitInlineBuffer(s, &buf, CFRangeMake(0, len));
            for (CFIndex i = 0; i < len; i++) {
                if (ch == CFStringGetCharacterFromInlineBuffer(&buf, i)) {
                    c++;
                }
            }
        }
    }
    return c;
}

int QString::contains(const char *chs, bool cs) const
{
    int c = 0;
    if (s && chs) {
        // FIXME: is ISO Latin-1 the correct encoding?
        CFStringRef tmp = CFStringCreateWithCStringNoCopy(
                kCFAllocatorDefault, chs, kCFStringEncodingISOLatin1,
                kCFAllocatorDefault);
        if (tmp) {
            CFIndex pos = 0;
            CFIndex len = CFStringGetLength(s);
            while (pos < len) {
                CFRange r;
                if (!CFStringFindWithOptions(s, tmp,
                        CFRangeMake(pos, len - pos),
                        cs ? 0 : kCFCompareCaseInsensitive, &r)) {
                    break;
                }
                c++;
                // move to next possible overlapping match
                pos += r.location + 1;
            }
            CFRelease(tmp);
        }
    }
    return c;
}

#ifdef USING_BORROWED_KURL
ushort QString::toUShort() const
{
    return toUInt();
}
#endif // USING_BORROWED_KURL

int QString::toInt(bool *ok, int base) const
{
    return toLong(ok, base);
}

uint QString::toUInt(bool *ok) const
{
    // FIXME: this uses code similar to that in the "toLong" function and could
    // possibly be refactored
    uint n = 0;
    bool valid = FALSE;
    if (s) {
        CFIndex len = CFStringGetLength(s);
        if (len) {
            CFStringInlineBuffer buf;
            UniChar uc;
            CFCharacterSetRef wscs =
                CFCharacterSetGetPredefined(
                        kCFCharacterSetWhitespaceAndNewline);
            CFStringInitInlineBuffer(s, &buf, CFRangeMake(0, len));
            CFIndex i;
            // ignore any leading whitespace
            for (i = 0; i < len; i++) {
                uc = CFStringGetCharacterFromInlineBuffer(&buf, i);
                if (!CFCharacterSetIsCharacterMember(wscs, uc)) {
                    break;
                }
            }
            // is there a number?
            while (i < len) {
                uc = CFStringGetCharacterFromInlineBuffer(&buf, i);
                if ((uc >= '0') && (uc <= '9')) {
                    n += uc - '0';
                } else {
                    break;
                }
                valid = TRUE;
                i++;
            }
            // ignore any trailing whitespace
            while (i < len) {
                uc = CFStringGetCharacterFromInlineBuffer(&buf, i);
                if (!CFCharacterSetIsCharacterMember(wscs, uc)) {
                    valid = FALSE;
                    break;
                }
                i++;
            }
        }
    }
    if (ok) {
        *ok = valid;
    }
    return valid ? n : 0;
}

long QString::toLong(bool *ok, int base) const
{
    // FIXME: this uses code similar to that in the "toUInt" function and could
    // possibly be refactored
    long n = 0;
    bool valid = FALSE;
    if (s) {
        CFIndex len = CFStringGetLength(s);
        if (len) {
            CFStringInlineBuffer buf;
            UniChar uc;
            CFCharacterSetRef wscs =
                CFCharacterSetGetPredefined(
                        kCFCharacterSetWhitespaceAndNewline);
            CFStringInitInlineBuffer(s, &buf, CFRangeMake(0, len));
            CFIndex i;
            // ignore any leading whitespace
            for (i = 0; i < len; i++) {
                uc = CFStringGetCharacterFromInlineBuffer(&buf, i);
                if (!CFCharacterSetIsCharacterMember(wscs, uc)) {
                    break;
                }
            }
            // is there a sign?
            bool neg = FALSE;
            if (i < len) {
                uc = CFStringGetCharacterFromInlineBuffer(&buf, i);
                if (uc == '-') {
                    i++;
                    neg = TRUE;
                } else if (uc == '+') {
                    i++;
                }
            }
            // is there a number?
            while (i < len) {
                uc = CFStringGetCharacterFromInlineBuffer(&buf, i);
                // NOTE: ignore anything other than base 10 and base 16
                if ((uc >= '0') && (uc <= '9')) {
                    n += uc - '0';
                } else if (base == 16) {
                    if ((uc >= 'A') && (uc <= 'F')) {
                        n += 10 + (uc - 'A');
                    } else if ((uc >= 'a') && (uc <= 'f')) {
                        n += 10 + (uc - 'a');
                    } else {
                        break;
                    }
                } else {
                    break;
                }
                valid = TRUE;
                i++;
            }
            if (neg) {
                n = -n;
            }
            // ignore any trailing whitespace
            while (i < len) {
                uc = CFStringGetCharacterFromInlineBuffer(&buf, i);
                if (!CFCharacterSetIsCharacterMember(wscs, uc)) {
                    valid = FALSE;
                    break;
                }
                i++;
            }
        }
    }
    if (ok) {
        *ok = valid;
    }
    return valid ? n : 0;
}

float QString::toFloat(bool *ok) const
{
    float n;
    if (s) {
        n = CFStringGetDoubleValue(s);
    } else {
        n = 0.0;
    }
    if (ok) {
        // NOTE: since CFStringGetDoubleValue returns 0.0 on error there is no
        // way to know if "n" is valid in that case
        if (n == 0.0) {
            *ok = FALSE;
        } else {
            *ok = TRUE;
        }
    }
    return n;
}

QString QString::arg(const QString &replacement, int padding) const
{
    QString qs;
    if (s && CFStringGetLength(s)) {
        qs.s = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, s);
    }
    if (qs.s) {
        CFIndex pos = 0;
        UniChar found = 0;
        CFIndex len = CFStringGetLength(qs.s);
        if (len) {
            CFStringInlineBuffer buf;
            CFStringInitInlineBuffer(qs.s, &buf, CFRangeMake(0, len));
            // find position of lowest numerical position marker
            for (CFIndex i = 0; i < len; i++) {
                UniChar uc = CFStringGetCharacterFromInlineBuffer(&buf, i);
                if ((uc == '%') && ((i + 1) < len)) {
                    UniChar uc2 = CFStringGetCharacterFromInlineBuffer(&buf,
                            i + 1);
                    if ((uc2 >= '0') && (uc2 <= '9')) {
                        if (!found || (uc2 < found)) {
                            found = uc2;
                            pos = i;
                        }
                    }
                }
            }
        }
        CFIndex rlen;
        if (found) {
            rlen = 2;
        } else {
            // append space and then replacement text at end of string
            CFStringAppend(qs.s, CFSTR(" "));
            pos = len + 1;
            rlen = 0;
        }
        if (replacement.s) {
            CFStringReplace(qs.s, CFRangeMake(pos, rlen), replacement.s);
            if (padding) {
                CFMutableStringRef tmp =
                    CFStringCreateMutable(kCFAllocatorDefault, 0);
                if (tmp) {
                    CFIndex plen;
                    if (padding < 0) {
                        plen = -padding;
                        pos += CFStringGetLength(replacement.s);
                    } else {
                        plen = padding;
                    }
                    CFStringPad(tmp, CFSTR(" "), plen, 0);
                    CFStringInsert(qs.s, pos, tmp);
                    CFRelease(tmp);
                }
            }
        }
    }
    return qs;
}

QString QString::arg(int replacement, int padding) const
{
    return arg(number(replacement), padding);
}

QString QString::left(uint width) const
{
    return leftRight(width, TRUE);
}

QString QString::right(uint width) const
{
    return leftRight(width, FALSE);
}

QString QString::mid(int index, int width) const
{
    QString qs;
    if (s) {
        CFIndex len = CFStringGetLength(s);
        if (len && (index >= 0) && (index < len) && width) {
            if (!((index == 0) && (width >= len))) {
                if (width > (len - index)) {
                    width = len - index;
                }
                CFStringRef tmp = CFStringCreateWithSubstring(
                        kCFAllocatorDefault, s, CFRangeMake(index, width));
                if (tmp) {
                    qs.s = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                            tmp);
                    CFRelease(tmp);
                }
            } else {
                CFRetain(s);
                qs.s = s;
            }
        }
    }
    return qs;
}

#ifdef USING_BORROWED_KURL
QString QString::copy() const
{
    return QString(*this);
}
#endif // USING_BORROWED_KURL

QString QString::lower() const
{
    QString qs;
    if (s && CFStringGetLength(s)) {
        qs.s = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, s);
    }
    if (qs.s) {
	CFStringLowercase(qs.s, NULL);
    }
    return qs;
}

QString QString::stripWhiteSpace() const
{
    QString qs;
    if (s) {
        CFIndex len = CFStringGetLength(s);
        if (len) {
            CFCharacterSetRef wscs = CFCharacterSetGetPredefined(
                    kCFCharacterSetWhitespaceAndNewline);
            if (CFCharacterSetIsCharacterMember(wscs,
                    CFStringGetCharacterAtIndex(s, 0))
                    || CFCharacterSetIsCharacterMember(wscs,
                    CFStringGetCharacterAtIndex(s, len - 1))) {
                qs.s = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, s);
                if (qs.s) {
                    CFStringTrimWhitespace(qs.s);
                }
            }
            if (!qs.s) {
                CFRetain(s);
                qs.s = s;
            }
        }
    }
    return qs;
}

QString QString::simplifyWhiteSpace() const
{
    QString qs;
    if (s) {
        CFIndex len = CFStringGetLength(s);
        if (len) {
            qs.s = CFStringCreateMutable(kCFAllocatorDefault, 0);
            if (qs.s) {
                CFCharacterSetRef wscs = CFCharacterSetGetPredefined(
                        kCFCharacterSetWhitespaceAndNewline);
                const UniChar *ucs = CFStringGetCharactersPtr(s);
                const int capacity = 64;
                UniChar buf[capacity];
                int fill = 0;
                bool chars = FALSE;
                bool space = FALSE;
                for (CFIndex i = 0; i < len; i++) {
                    UniChar uc;
                    if (ucs) {
                        uc = ucs[i];
                    } else {
                        uc = CFStringGetCharacterAtIndex(s, i);
                    }
                    if (CFCharacterSetIsCharacterMember(wscs, uc)) {
                        if (!chars) {
                            continue;
                        }
                        space = TRUE;
                    } else {
                        if (space) {
                            buf[fill] = ' ';
                            fill++;
                            if (fill == capacity) {
                                CFStringAppendCharacters(qs.s, buf, fill);
                                fill = 0;
                            }
                            space = FALSE;
                        }
                        buf[fill] = uc;
                        fill++;
                        if (fill == capacity) {
                            CFStringAppendCharacters(qs.s, buf, fill);
                            fill = 0;
                        }
                        chars = true;
                    }
                }
                if (fill) {
                    CFStringAppendCharacters(qs.s, buf, fill);
                }
            } else {
                CFRetain(s);
                qs.s = s;
            }
        }
    }
    return qs;
}

QString &QString::setUnicode(const QChar *qcs, uint len)
{
    flushCache();
    if (!s && len) {
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
    }
    if (s) {
        if (len) {
            if (qcs) {
                CFStringRef tmp = CFStringCreateWithCharactersNoCopy(
                        kCFAllocatorDefault, &qcs->c, len, kCFAllocatorNull);
                if (tmp) {
                    CFStringReplaceAll(s, tmp);
                    CFRelease(tmp);
                }
            }
        } else {
            CFRelease(s);
            s = NULL;
        }
    }
    return *this;
}

QString &QString::setNum(int n)
{
    flushCache();
    if (!s) {
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
    }
    if (s) {
        const int capacity = 64;
        char buf[capacity];
        buf[snprintf(buf, capacity - 1, "%d", n)] = '\0';
        // FIXME: is ISO Latin-1 the correct encoding?
        CFStringRef tmp = CFStringCreateWithCStringNoCopy(
                kCFAllocatorDefault, buf, kCFStringEncodingISOLatin1,
                kCFAllocatorDefault);
        if (tmp) {
            CFStringReplaceAll(s, tmp);
            CFRelease(tmp);
        }
    }
    return *this;
}

QString &QString::sprintf(const char *, ...)
{
    flushCache();
    // FIXME: not yet implemented
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
    return *this;
}

QString &QString::prepend(const QString &qs)
{
    return insert(0, qs);
}

QString &QString::append(const QString &qs)
{
    return operator+=(qs);
}

QString &QString::insert(uint, const QString &)
{
    flushCache();
    // FIXME: not yet implemented
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
    return *this;
}

QString &QString::insert(uint, QChar)
{
    flushCache();
    // FIXME: not yet implemented
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
    return *this;
}

QString &QString::insert(uint, char)
{
    flushCache();
    // FIXME: not yet implemented
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
    return *this;
}

QString &QString::remove(uint, uint)
{
    flushCache();
    // FIXME: not yet implemented
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
    return *this;
}

QString &QString::replace(const QRegExp &, const QString &)
{
    flushCache();
    // FIXME: not yet implemented
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
    return *this;
}

void QString::truncate(uint)
{
    flushCache();
    // FIXME: not yet implemented
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
}

void QString::fill(QChar, int)
{
    flushCache();
    // FIXME: not yet implemented
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
}

void QString::compose()
{
    // FIXME: unimplemented because we don't do ligatures yet
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
}

QString QString::visual()
{
    // FIXME: unimplemented because we don't do BIDI yet
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
    return QString(*this);
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

QChar QString::operator[](int index) const
{
    if (s && (index >= 0)) {
        CFIndex len = CFStringGetLength(s);
        if (index < len) {
            return QChar(CFStringGetCharacterAtIndex(s, index));
        }
    }
    return QChar(0);
}

QString &QString::operator+=(const QString &)
{
    flushCache();
    // FIXME: not yet implemented
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
    return *this;
}

QString &QString::operator+=(QChar)
{
    flushCache();
    // FIXME: not yet implemented
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
    return *this;
}

QString &QString::operator+=(char)
{
    flushCache();
    // FIXME: not yet implemented
    NSLog(@"WARNING %s:%s:%d (NOT YET IMPLEMENTED)\n", __FILE__, __FUNCTION__,
            __LINE__);
    return *this;
}

// private member functions ----------------------------------------------------

void QString::flushCache() const
{
    if (cache) {
        CFAllocatorDeallocate(kCFAllocatorDefault, cache);
        cache = NULL;
        cacheType = CacheInvalid;
    }
}

QCString QString::convertToQCString(CFStringEncoding enc) const
{
    uint len = length();
    if (len) {
        char *chs = CFAllocatorAllocate(kCFAllocatorDefault, len + 1, 0);
        if (chs) {
            if (!CFStringGetCString(s, chs, len + 1, enc)) {
#ifdef _KWQ_DEBUG_
                NSLog(@"WARNING %s:%s:%d (CFStringGetCString failed)\n",
                        __FILE__, __FUNCTION__, __LINE__);
#endif
                *reinterpret_cast<char *>(chs) = '\0';
            }
            QCString qcs = QCString(chs);
            CFAllocatorDeallocate(kCFAllocatorDefault, chs);
            return qcs;
        }
    }
    return QCString();
}

QString QString::leftRight(uint width, bool left) const
{
    QString qs;
    if (s) {
        CFIndex len = CFStringGetLength(s);
        if (len && width) {
            if (len > width) {
                CFStringRef tmp = CFStringCreateWithSubstring(
                        kCFAllocatorDefault, s, left ? CFRangeMake(0, width)
                        : CFRangeMake(len - width, len));
                if (tmp) {
                    qs.s = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                            tmp);
                    CFRelease(tmp);
                }
            } else {
                CFRetain(s);
                qs.s = s;
            }
        }
    }
    return qs;
}


// operators associated with QString ===========================================

bool operator==(const QString &qs1, const QString &qs2)
{
    if (qs1.s == qs2.s) {
        return TRUE;
    }
    if (qs1.s && qs2.s) {
        return CFStringCompare(qs1.s, qs2.s, 0) == kCFCompareEqualTo;
    }
    return FALSE;
}

bool operator==(const QString &qs, const char *chs)
{
    bool result = FALSE;
    if (qs.s && chs) {
        // FIXME: is ISO Latin-1 the correct encoding?
        CFStringRef tmp = CFStringCreateWithCStringNoCopy(
                kCFAllocatorDefault, chs, kCFStringEncodingISOLatin1,
                kCFAllocatorDefault);
        if (tmp) {
            result = CFStringCompare(qs.s, tmp, 0) == kCFCompareEqualTo;
            CFRelease(tmp);
        }
    }
    return result;
}

bool operator==(const char *chs, const QString &qs)
{
    return qs == chs;
}

bool operator!=(const QString &qs1, const QString &qs2)
{
    return !(qs1 == qs2);
}

bool operator!=(const QString &qs, const char *chs)
{
    return !(qs == chs);
}

bool operator!=(const char *chs, const QString &qs)
{
    return !(qs == chs);
}

QString operator+(const QString &qs1, const QString &qs2)
{
    QString tmp(qs1);
    tmp += qs2;
    return tmp;
}

QString operator+(const QString &qs, const char *chs)
{
    QString tmp(qs);
    tmp += chs;
    return tmp;
}

QString operator+(const char *chs, const QString &qs)
{
    QString tmp(chs);
    tmp += qs;
    return tmp;
}


// class QConstString ==========================================================

// constructors, copy constructors, and destructors ----------------------------

QConstString::QConstString(QChar *qcs, uint len)
{
    if (qcs || len) {
        // NOTE: use instead of CFStringCreateWithCharactersNoCopy function to
        // guarantee backing store is not copied even though string is mutable
        s = CFStringCreateMutableWithExternalCharactersNoCopy(
                kCFAllocatorDefault, &qcs->c, len, len, kCFAllocatorNull);
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
