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

/*
    This implementation uses CFMutableStringRefs as a rep for the actual
    string data.  Reps may be shared between QString instances, and follow
    a copy-on-write sematic.  If you change the implementation be sure to
    copy the rep before calling any of the CF functions that might mutate
    the string.
*/

#include <Foundation/Foundation.h>
#include <kwqdebug.h>
#include <qstring.h>
#include <qregexp.h>
#include <stdio.h>


#ifndef USING_BORROWED_QSTRING

// QString class ===============================================================

// constants -------------------------------------------------------------------

const QString QString::null;

static CFMutableStringRef
getNullCFString()
{
    static CFStringRef ref = CFSTR("");
    CFRetain(ref);
    return (CFMutableStringRef)ref;
}

static const QChar *
getNullQCharString()
{
    static QChar nullCharacter;
    return &nullCharacter;
}

// static member functions -----------------------------------------------------

QString QString::number(int n)
{
    QString qs;
    qs.setNum(n);
    return qs;
}

QString QString::number(uint n)
{
    QString qs;
    qs.setNum(n);
    return qs;
}

QString QString::number(long n)
{
    QString qs;
    qs.setNum(n);
    return qs;
}

QString QString::number(ulong n)
{
    QString qs;
    qs.setNum(n);
    return qs;
}

QString QString::number(double n)
{
    QString qs;
    qs.setNum(n);
    return qs;
}

#ifdef USING_BORROWED_KURL
QString QString::fromLocal8Bit(const char *chs, int len)
{
    // FIXME: is MacRoman the correct encoding?
    return fromStringWithEncoding(chs, len, kCFStringEncodingMacRoman);
}
#endif // USING_BORROWED_KURL

QString QString::fromStringWithEncoding(const char *chs, int len,
        CFStringEncoding encoding)
{
    QString qs;
    if (chs && *chs) {
        qs.s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        if (qs.s) {
            if (len < 0) {
                // append null-terminated string
                CFStringAppendCString(qs.s, chs, encoding);
            } else {
                // append length-specified string
                // FIXME: can we find some way of not using this temporary?
#if 1
                char *buf = CFAllocatorAllocate(kCFAllocatorDefault, len + 1, 0);
                strncpy(buf, chs, len);
                *(buf + len) = '\0';
                CFStringAppendCString(qs.s, buf, encoding);
                CFAllocatorDeallocate(kCFAllocatorDefault, buf);
#else
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
#endif
            }
        }
    }
    return qs;
}

QString QString::fromCFMutableString(CFMutableStringRef cfs)
{
    QString qs;
    // shared copy
    if (cfs) {
        CFRetain(cfs);
        qs.s = cfs;
    }
    return qs;
}

QString QString::fromCFString(CFStringRef cfs)
{
    CFMutableStringRef ref;
    QString qs;

    ref = CFStringCreateMutableCopy(NULL, 0, cfs);
    qs = QString::fromCFMutableString(ref);
#ifdef KWQ_STRING_DEBUG
#else
    CFRelease(ref);
#endif
    
    return qs;
}

QString QString::gstring_toQString(CFMutableStringRef *ref, UniChar *uchars, int len){
    if (*ref == 0)
        *ref = CFStringCreateMutableWithExternalCharactersNoCopy (kCFAllocatorDefault, uchars, len, len, kCFAllocatorDefault);
    else
        CFStringSetExternalCharactersNoCopy (*ref, uchars, len, len);
    return QString::fromCFMutableString(*ref);
}

CFMutableStringRef QString::gstring_toCFString(CFMutableStringRef *ref, UniChar *uchars, int len)
{
    if (*ref == 0)
        *ref = CFStringCreateMutableWithExternalCharactersNoCopy (kCFAllocatorDefault, uchars, len, len, kCFAllocatorDefault);
    else
        CFStringSetExternalCharactersNoCopy (*ref, uchars, len, len);
    return *ref;
}



// constructors, copy constructors, and destructors ----------------------------

QString::QString()
{
    s = getNullCFString();
    cacheType = CacheInvalid;
}

QString::~QString()
{
    CFRelease(s);
    if (cacheType == CacheAllocatedUnicode || cacheType == CacheAllocatedLatin1)
        CFAllocatorDeallocate(kCFAllocatorDefault, cache);
}

QString::QString(QChar qc)
{
    s = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringAppendCharacters(s, &qc.c, 1);
    cacheType = CacheInvalid;
}

QString::QString(const QByteArray &qba)
{
    if (qba.size() && *qba.data()) {
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
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
        if (fill)
            CFStringAppendCharacters(s, buf, fill);
    } else
        s = getNullCFString();
    cacheType = CacheInvalid;
}

QString::QString(const QChar *qcs, uint len)
{
    if (qcs || len) {
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppendCharacters(s, &qcs->c, len);
    } else
        s = getNullCFString();
    cacheType = CacheInvalid;
}

QString::QString(const char *chs)
{
    if (chs && *chs) {
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppendCString(s, chs, kCFStringEncodingISOLatin1);
    } else
        s = getNullCFString();
    cacheType = CacheInvalid;
}

QString::QString(const char *chs, int len)
{
    if (len > 0) {
        CFStringRef tmp = CFStringCreateWithBytes (kCFAllocatorDefault, (const UInt8 *)chs, len, kCFStringEncodingISOLatin1, false);
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppend(s, tmp);
        CFRelease(tmp);
    } else
        s = getNullCFString();
    cacheType = CacheInvalid;
}

QString::QString(const QString &qs)
{
    // shared copy
    CFRetain(qs.s);
    s = qs.s;
    cacheType = CacheInvalid;
}

// assignment operators --------------------------------------------------------

QString &QString::operator=(const QString &qs)
{
    // shared copy
    CFRetain(qs.s);
    CFRelease(s);
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

QChar QString::at(uint index) const
{
    CFIndex signedIndex = index;
    CFIndex len = CFStringGetLength(s);
    if (signedIndex < len)
        return CFStringGetCharacterAtIndex(s, signedIndex);
    return 0;
}

const QChar *QString::unicode() const
{
    if (cacheType != CacheUnicode && cacheType != CacheAllocatedUnicode) {
        flushCache();
        const UniChar *ucs = CFStringGetCharactersPtr(s);
        if (ucs) {
            cacheType = CacheUnicode;
            cache = const_cast<UniChar *>(ucs);
        } else {
#if _KWQ_DEBUG_
            KWQDEBUG3("WARNING %s:%s:%d (CFStringGetCharactersPtr failed)\n", __FUNCTION__, __FILE__, __LINE__);
#endif
            uint len = length();
            if (len == 0) {
                cacheType = CacheUnicode;
                cache = isNull() ? 0 : const_cast<QChar *>(getNullQCharString());
            } else {
                cacheType = CacheAllocatedUnicode;
                cache = CFAllocatorAllocate(kCFAllocatorDefault, len * sizeof (UniChar), 0);
                CFStringGetCharacters(s, CFRangeMake(0, len), cache);
            }
        }
    }

    // NOTE: this works because our QChar implementation contains a single UniChar data member
    return static_cast<QChar *>(cache); 
}

const char *QString::latin1() const
{
    if (cacheType != CacheLatin1 && cacheType != CacheAllocatedLatin1) {
        flushCache();
        const char *chs = CFStringGetCStringPtr(s, kCFStringEncodingISOLatin1);
        if (chs) {
            cacheType = CacheLatin1;
            cache = const_cast<char *>(chs);
        } else {
#if _KWQ_DEBUG_
            KWQDEBUG3("WARNING %s:%s:%d (CFStringGetCharactersPtr failed)\n", __FUNCTION__, __FILE__, __LINE__);
#endif
            uint len = length();
            if (len == 0) {
                cacheType = CacheLatin1;
                cache = const_cast<char *>("");
            } else {
                cacheType = CacheAllocatedLatin1;
                cache = CFAllocatorAllocate(kCFAllocatorDefault, len + 1, 0);
                if (!CFStringGetCString(s, cache, len + 1, kCFStringEncodingISOLatin1)) {
#if _KWQ_DEBUG_
                    KWQDEBUG3("WARNING %s:%s:%d (CFStringGetCString failed)\n", __FUNCTION__, __FILE__, __LINE__);
#endif
                    *static_cast<char *>(cache) = '\0';
                }
            }
        }
    }

    return static_cast<char *>(cache);
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
    return s == getNullCFString();
}

int QString::find(QChar qc, int index) const
{
    CFIndex len = CFStringGetLength(s);
    if (index < 0)
        index += len;
    if (len && (index >= 0) && (index < len)) {
        CFStringInlineBuffer buf;
        CFStringInitInlineBuffer(s, &buf, CFRangeMake(0, len));
        for (CFIndex i = index; i < len; i++)
            if (qc.c == CFStringGetCharacterFromInlineBuffer(&buf, i))
                return i;
    }
    return -1;
}

int QString::find(char ch, int index) const
{
    return find(QChar(ch), index);
}

int QString::find(const QString &qs, int index) const
{
    CFIndex len = CFStringGetLength(s);
    if (index < 0)
        index += len;
    if (len && (index >= 0) && (index < len)) {
        CFRange r;
        CFRange start = CFRangeMake(index, len - index); 
        if (CFStringFindWithOptions(s, qs.s, start, 0, &r))
            return r.location;
    }
    return -1;
}

#ifdef DEBUG_FIND_COUNTER
static int findCount = 0;
static int findExpensiveCount = 0;
static int findCheapCount = 0;
#endif

const int caseDelta = ('a' - 'A');

int QString::find(const char *chs, int index, bool caseSensitive) const
{
    int pos = -1;
    if (chs) {
#ifdef DEBUG_FIND_COUNTER
        findCount++;
#endif
        const UniChar *internalBuffer = CFStringGetCharactersPtr(s);
        CFIndex len = CFStringGetLength(s);
        if (index < 0)
            index += len;
        if (internalBuffer == 0){
#ifdef DEBUG_FIND_COUNTER
            findExpensiveCount++;
            if (findCount % 500 == 0)
                fprintf (stdout, "findCount = %d, expensive = %d, cheap = %d\n", findCount, findExpensiveCount, findCheapCount);
#endif
            if (len && (index >= 0) && (index < len)) {
                CFStringRef tmp = CFStringCreateWithCStringNoCopy(
                        kCFAllocatorDefault, chs, kCFStringEncodingISOLatin1,
                        kCFAllocatorNull);
                CFRange r;
                if (CFStringFindWithOptions(s, tmp,
                        CFRangeMake(index, len - index),
                        caseSensitive ? 0 : kCFCompareCaseInsensitive, &r)) {
                    pos = r.location;
                }
                CFRelease(tmp);
            }
        }
        else {
#ifdef DEBUG_FIND_COUNTER
            findCheapCount++;
            if (findCount % 500 == 0)
                fprintf (stdout, "findCount = %d, expensive = %d, cheap = %d\n", findCount, findExpensiveCount, findCheapCount);
#endif
            if (len && (index >= 0) && (index < len)) {
                UniChar firstC, c1, c2;
                const char *_chs;
                int remaining = len - index, found = -1;
                int compareToLength = strlen(chs);
                
                internalBuffer = &internalBuffer[index];
                
                _chs = chs + 1;
                firstC = (UniChar)(*chs);
                while (remaining >= compareToLength){
                    if (*internalBuffer++ == firstC){
                        const UniChar *compareTo = internalBuffer;
                        
                        found = len - remaining;
                        while ( (c2 = (UniChar)(*_chs++)) ){
                            c1 = (UniChar)(*compareTo++);
                            if (!caseSensitive){
                                if (c2 >= 'a' && c2 <= 'z'){
                                    if (c1 == c2 || c1 == c2 - caseDelta)
                                        continue;
                                }
                                else if (c2 >= 'A' && c2 <= 'Z'){
                                    if (c1 == c2 || c1 == c2 + caseDelta)
                                        continue;
                                }
                                else if (c1 == c2)
                                    continue;
                            }
                            else if (c1 == c2)
                                continue;
                            break;
                        }
                        if (c2 == 0)
                            return found;
                        _chs = chs + 1;
                    }
                    remaining--;
                }
            }
        }
    }
    return pos;
}

int QString::find(const QRegExp &qre, int index) const
{
    CFIndex len = CFStringGetLength(s);
    if (index < 0)
        index += len;
    if (len && (index >= 0) && (index < len))
        return qre.match(*this, index);
    return -1;
}

int QString::findRev(char ch, int index) const
{
    CFIndex len = CFStringGetLength(s);
    if (index < 0)
        index += len;
    if (len && (index <= len)) {
        CFStringInlineBuffer buf;
        CFStringInitInlineBuffer(s, &buf, CFRangeMake(0, index));
        for (CFIndex i = index; i >= 0; i--) {
            if (ch == CFStringGetCharacterFromInlineBuffer(&buf, i))
                return i;
        }
    }
    return -1;
}

int QString::findRev(const char *chs, int index) const
{
    int pos = -1;
    if (chs) {
        CFIndex len = CFStringGetLength(s);
        if (index < 0)
            index += len;
        if (len && (index <= len)) {
            CFStringRef tmp = CFStringCreateWithCStringNoCopy(
                    kCFAllocatorDefault, chs, kCFStringEncodingISOLatin1,
                    kCFAllocatorNull);
            CFRange r;
            if (CFStringFindWithOptions(s, tmp, CFRangeMake(0, index),
                                        kCFCompareBackwards, &r)) {
                pos = r.location;
            }
            CFRelease(tmp);
        }
    }
    return pos;
}

#ifdef DEBUG_CONTAINS_COUNTER
static int containsCount = 0;
#endif

int QString::contains(char ch) const
{
    int c = 0;
    CFIndex len = CFStringGetLength(s);
    CFStringInlineBuffer buf;
    CFStringInitInlineBuffer(s, &buf, CFRangeMake(0, len));
    for (CFIndex i = 0; i < len; i++)
        if (ch == CFStringGetCharacterFromInlineBuffer(&buf, i))
            c++;
    return c;
}

int QString::contains(const char *chs, bool cs) const
{
    int c = 0;
    if (chs) {
#ifdef DEBUG_CONTAINS_COUNTER
        containsCount++;
        if (containsCount % 500 == 0)
            fprintf (stdout, "containsCount = %d\n", containsCount);
#endif
        CFStringRef tmp = CFStringCreateWithCStringNoCopy(
                kCFAllocatorDefault, chs, kCFStringEncodingISOLatin1,
                kCFAllocatorNull);
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
    return c;
}

short QString::toShort(bool *ok, int base) const
{
    bool neg;
    short n = convertToNumber(ok, base, &neg);
    return neg ? -n : n;
}

ushort QString::toUShort(bool *ok, int base) const
{
    return convertToNumber(ok, base, NULL);
}

int QString::toInt(bool *ok, int base) const
{
    bool neg;
    int n = convertToNumber(ok, base, &neg);
    return neg ? -n : n;
}

uint QString::toUInt(bool *ok, int base) const
{
    return convertToNumber(ok, base, NULL);
}

long QString::toLong(bool *ok, int base) const
{
    bool neg;
    long n = convertToNumber(ok, base, &neg);
    return neg ? -n : n;
}

ulong QString::toULong(bool *ok, int base) const
{
    return convertToNumber(ok, base, NULL);
}

double QString::toDouble(bool *ok) const
{
    double n;
    if (s) {
        n = CFStringGetDoubleValue(s);
    } else {
        n = 0.0;
    }
    if (ok) {
        // NOTE: since CFStringGetDoubleValue returns 0.0 on error there is no
        // way to know if "n" is valid in that case
        //
        // EXTRA NOTE: We can't assume 0.0 is bad, since it totally breaks
        // html like border="0". So, only trigger breakage if the char 
        // at index 0 is neither a '0' nor a '.' nor a '-'.
        UniChar uc = CFStringGetCharacterAtIndex(s,0);
        *ok = true;
        if (n == 0.0 && uc != '0' && uc != '.' && uc != '-') {
            *ok = false;
        }
    }
    return n;
}

QString QString::arg(const QString &replacement, int width) const
{
    QString qs;
    if (CFStringGetLength(s)) {
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
        CFIndex mlen;
        if (found) {
            mlen = 2;
        } else {
            // append space and then replacement text at end of string
            CFStringAppend(qs.s, CFSTR(" "));
            pos = len + 1;
            mlen = 0;
        }
        if (replacement.s) {
            CFStringReplace(qs.s, CFRangeMake(pos, mlen), replacement.s);
            if (width) {
                CFIndex rlen = CFStringGetLength(replacement.s);
                CFIndex padding;
                if (width < 0) {
                    padding = -width;
                    pos += rlen;
                } else {
                    padding = width;
                }
                padding -= rlen;
                if (padding > 0) {
                    CFMutableStringRef tmp =
                        CFStringCreateMutable(kCFAllocatorDefault, 0);
                    if (tmp) {
                        CFStringPad(tmp, CFSTR(" "), padding, 0);
                        CFStringInsert(qs.s, pos, tmp);
                        CFRelease(tmp);
                    }
                }
            }
        }
    }
    return qs;
}

QString QString::arg(short replacement, int width) const
{
    return arg(number((int)replacement), width);
}

QString QString::arg(ushort replacement, int width) const
{
    return arg(number((uint)replacement), width);
}

QString QString::arg(int replacement, int width) const
{
    return arg(number(replacement), width);
}

QString QString::arg(uint replacement, int width) const
{
    return arg(number(replacement), width);
}

QString QString::arg(long replacement, int width) const
{
    return arg(number(replacement), width);
}

QString QString::arg(ulong replacement, int width) const
{
    return arg(number(replacement), width);
}

QString QString::arg(double replacement, int width) const
{
    return arg(number(replacement), width);
}

QString QString::left(uint width) const
{
    return leftRight(width, true);
}

QString QString::right(uint width) const
{
    return leftRight(width, false);
}

QString QString::mid(uint index, uint width) const
{
    QString qs;
    CFIndex len = CFStringGetLength(s);
    if (len && (index < (uint)len) && width) {
        if (!((index == 0) && (width >= (uint)len))) {
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
    return qs;
}

QString QString::copy() const
{
    // FIXME: we really need a deep copy here
    //return QString(*this);
    return QString(unicode(), length());
}

QString QString::lower() const
{
    QString qs;
    if (CFStringGetLength(s)) {
        qs.s = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, s);
        CFStringLowercase(qs.s, NULL);
    }
    return qs;
}

QString QString::stripWhiteSpace() const
{
    QString qs;
    CFIndex len = CFStringGetLength(s);
    if (len) {
        static CFCharacterSetRef wscs = CFCharacterSetGetPredefined(
                kCFCharacterSetWhitespaceAndNewline);
        if (CFCharacterSetIsCharacterMember(wscs,
                CFStringGetCharacterAtIndex(s, 0))
                || CFCharacterSetIsCharacterMember(wscs,
                CFStringGetCharacterAtIndex(s, len - 1))) {
            qs.s = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, s);
            CFStringTrimWhitespace(qs.s);
        } else {
            CFRetain(s);
            qs.s = s;
        }
    }
    return qs;
}

QString QString::simplifyWhiteSpace() const
{
    QString qs;
    CFIndex len = CFStringGetLength(s);
    if (len) {
        qs.s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        static CFCharacterSetRef wscs = CFCharacterSetGetPredefined(
                kCFCharacterSetWhitespaceAndNewline);
        const UniChar *ucs = CFStringGetCharactersPtr(s);
        const int capacity = 64;
        UniChar buf[capacity];
        int fill = 0;
        bool chars = false;
        bool space = false;
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
                space = true;
            } else {
                if (space) {
                    buf[fill] = ' ';
                    fill++;
                    if (fill == capacity) {
                        CFStringAppendCharacters(qs.s, buf, fill);
                        fill = 0;
                    }
                    space = false;
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
    return qs;
}

QString &QString::setUnicode(const QChar *qcs, uint len)
{
    flushCache();
    if (qcs && len) {
        CFRelease(s);
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppendCharacters(s, &qcs->c, len);
    } else {
        CFRelease(s);
        s = getNullCFString();
    }
    return *this;
}

QString &QString::setLatin1(const char *chs)
{
    flushCache();
    if (chs && *chs) {
        CFRelease(s);
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppendCString(s, chs, kCFStringEncodingISOLatin1);
    } else if (s) {
        CFRelease(s);
        s = getNullCFString();
    }
    return *this;
}

QString &QString::setNum(short n)
{
    return setNum((long)n);
}

QString &QString::setNum(ushort n)
{
    return setNum((ulong)n);
}

QString &QString::setNum(int n)
{
    const int capacity = 64;
    char buf[capacity];
    buf[snprintf(buf, capacity - 1, "%d", n)] = '\0';
    return setLatin1(buf);
}

QString &QString::setNum(uint n)
{
    const int capacity = 64;
    char buf[capacity];
    buf[snprintf(buf, capacity - 1, "%u", n)] = '\0';
    return setLatin1(buf);
}

QString &QString::setNum(long n)
{
    const int capacity = 64;
    char buf[capacity];
    buf[snprintf(buf, capacity - 1, "%D", n)] = '\0';
    return setLatin1(buf);
}

QString &QString::setNum(ulong n)
{
    const int capacity = 64;
    char buf[capacity];
    buf[snprintf(buf, capacity - 1, "%U", n)] = '\0';
    return setLatin1(buf);
}

QString &QString::setNum(double n)
{
    const int capacity = 64;
    char buf[capacity];
    buf[snprintf(buf, capacity - 1, "%.6lg", n)] = '\0';
    return setLatin1(buf);
}

QString &QString::sprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    flushCache();
    if (format && *format) {
        CFRelease(s);
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringRef f = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault,
                format, kCFStringEncodingISOLatin1, kCFAllocatorNull);
        if (f) {
            CFStringRef tmp = CFStringCreateWithFormatAndArguments(
                    kCFAllocatorDefault, NULL, f, args);
            if (tmp) {
                CFStringReplaceAll(s, tmp);
                CFRelease(tmp);
            }
            CFRelease(f);
        }
    } else {
        CFRelease(s);
        s = getNullCFString();
    }
    va_end(args);
    return *this;
}

QString &QString::prepend(const QString &qs)
{
    return insert(0, qs);
}

QString &QString::append(const QString &qs)
{
    return insert(length(), qs);
}

void QString::_copyIfNeededInternalString()
{
    if (CFGetRetainCount(s) != 1) {
        CFMutableStringRef tmp = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, s);
        CFRelease(s);
        s = tmp;
    }
}

QString &QString::insert(uint index, const QString &qs)
{
    flushCache();
    _copyIfNeededInternalString();
    if (index < (uint) CFStringGetLength(s))
        CFStringInsert(s, index, qs.s);
    else
        CFStringAppend(s, qs.s);
    return *this;
}

QString &QString::insert(uint index, QChar qc)
{
    return insert(index, QString(qc));
}

QString &QString::insert(uint index, char ch)
{
    return insert(index, QString(QChar(ch)));
}

QString &QString::remove(uint index, uint width)
{
    flushCache();
    CFIndex len = CFStringGetLength(s);
    if (len && (index < (uint)len) && width) {
        if (width > (len - index)) {
            width = len - index;
        }
        _copyIfNeededInternalString();
        CFStringDelete(s, CFRangeMake(index, width));
    }
    return *this;
}

QString &QString::replace(const QRegExp &qre, const QString &qs)
{
    int len = qs.length();
    for (int i = 0; i < CFStringGetLength(s); i += len) {
        int width = 0;
        i = qre.match(*this, i, &width, false);
        flushCache();
        if ((i < 0) || !width) {
            break;
        }
        CFRange r = CFRangeMake(i, width);
        _copyIfNeededInternalString();
        if (len) {
            CFStringReplace(s, r, qs.s);
        } else {
            CFStringDelete(s, r);
        }
    }
    return *this;
}

void QString::truncate(uint newLen)
{
    flushCache();
    _copyIfNeededInternalString();
    CFIndex len = CFStringGetLength(s);
    if (len && (newLen < (uint)len)) {
        CFStringDelete(s, CFRangeMake(newLen, len - newLen));
    }
}

void QString::fill(QChar qc, int len)
{
    flushCache();
    if (len < 0)
        len = CFStringGetLength(s);
    CFRelease(s);
    if (len <= 0)
        s = getNullCFString();
    else {
        UniChar *ucs = CFAllocatorAllocate(kCFAllocatorDefault, len * sizeof (UniChar), 0);
        for (int i = 0; i < len; i++)
            ucs[i] = qc.c;
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppendCharacters(s, ucs, len);
        CFAllocatorDeallocate(kCFAllocatorDefault, ucs);
    }
}

void QString::compose()
{
    // FIXME: unimplemented because we don't do ligatures yet
    _logNotYetImplemented();
}

QString QString::visual()
{
    // FIXME: unimplemented because we don't do BIDI yet
    _logNotYetImplemented();
    return QString(*this);
}

// operators -------------------------------------------------------------------

QString &QString::operator+=(const QString &qs)
{
    return insert(length(), qs);
}

QString &QString::operator+=(QChar qc)
{
    return insert(length(), qc);
}

QString &QString::operator+=(char ch)
{
    return insert(length(), QChar(ch));
}

// private member functions ----------------------------------------------------

void QString::flushCache() const
{
    if (cacheType == CacheAllocatedUnicode || cacheType == CacheAllocatedLatin1) {
        CFAllocatorDeallocate(kCFAllocatorDefault, cache);
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
#ifdef KWQ_STRING_DEBUG
#else
            CFAllocatorDeallocate(kCFAllocatorDefault, chs);
#endif
            return qcs;
        }
    }
    return QCString();
}

ulong QString::convertToNumber(bool *ok, int base, bool *neg) const
{
    ulong n = 0;
    bool valid = false;
    bool negative = false;
    if (s) {
        CFIndex len = CFStringGetLength(s);
        if (len) {
            CFStringInlineBuffer buf;
            UniChar uc;
            static CFCharacterSetRef wscs =
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
            if (neg) {
                // is there a sign?
                if (i < len) {
                    uc = CFStringGetCharacterFromInlineBuffer(&buf, i);
                    if (uc == '-') {
                        i++;
                        negative = true;
                    } else if (uc == '+') {
                        i++;
                    }
                }
            }
            ulong max = negative ? LONG_MAX : ULONG_MAX;
            // is there a number?
            while (i < len) {
                uc = CFStringGetCharacterFromInlineBuffer(&buf, i);
                // NOTE: ignore anything other than base 10 and base 16
                if ((uc >= '0') && (uc <= '9')) {
                    if (n > (max / base)) {
                        valid = false;
                        break;
                    }
                    n = (n * base) + (uc - '0');
                } else if (base == 16) {
                    if ((uc >= 'A') && (uc <= 'F')) {
                        if (n > (max / base)) {
                            valid = false;
                            break;
                        }
                        n = (n * base) + (10 + (uc - 'A'));
                    } else if ((uc >= 'a') && (uc <= 'f')) {
                        if (n > (max / base)) {
                            valid = false;
                            break;
                        }
                        n = (n * base) + (10 + (uc - 'a'));
                    } else {
                        break;
                    }
                } else {
                    break;
                }
                valid = true;
                i++;
            }
            // ignore any trailing whitespace
            while (i < len) {
                uc = CFStringGetCharacterFromInlineBuffer(&buf, i);
                if (!CFCharacterSetIsCharacterMember(wscs, uc)) {
                    valid = false;
                    break;
                }
                i++;
            }
        }
    }
    if (ok) {
        *ok = valid;
    }
    if (neg) {
        *neg = negative;
    }
    return valid ? n : 0;
}

QString QString::leftRight(uint width, bool left) const
{
    QString qs;
    if (s) {
        CFIndex len = CFStringGetLength(s);
        if (len && width) {
            if ((uint)len > width) {
                CFStringRef tmp = CFStringCreateWithSubstring(
                        kCFAllocatorDefault, s, left ? CFRangeMake(0, width)
                        : CFRangeMake(len - width, width));
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

#ifdef DEBUG_COMPARE_COUNTER
static int compareCount = 0;
static int compareCountExpensive = 0;
static int compareCountCheap = 0;
#endif

int QString::compareToLatin1(const char *chs) const
{
    if (!chs) {
        if (length() == 0)
            return kCFCompareEqualTo;
        return kCFCompareGreaterThan;
    }

#ifdef DEBUG_COMPARE_COUNTER
    compareCount++;
    if (compareCount % 500 == 0)
        fprintf (stdout, "compareCount = %d\n", compareCount);
#endif

    const UniChar *internalBuffer = CFStringGetCharactersPtr(s);
    if (internalBuffer == 0){
#ifdef DEBUG_COMPARE_COUNTER
        compareCountExpensive++;

        if (compareCount % 500 == 0)
            fprintf (stdout, "compareCount = %d, expensive = %d, cheap = %d\n", compareCount, compareCountExpensive, compareCountCheap);
#endif
        CFStringRef tmp = CFStringCreateWithCStringNoCopy(
                kCFAllocatorDefault, chs, kCFStringEncodingISOLatin1,
                kCFAllocatorNull);
        if (tmp) {
            int result = CFStringCompare(s, tmp, 0);
            CFRelease(tmp);
            return result;
        }
        return kCFCompareGreaterThan;
    }
    else {
        CFIndex len = CFStringGetLength(s);
        
#ifdef DEBUG_COMPARE_COUNTER
        compareCountCheap++;
        if (compareCount % 500 == 0)
            fprintf (stdout, "compareCount = %d, expensive = %d, cheap = %d\n", compareCount, compareCountExpensive, compareCountCheap);
#endif
        while (len && *chs){
            UniChar c1 = *internalBuffer++;
            UniChar c2 = (UniChar)(*chs++);
            if (c1 < c2)
                return kCFCompareLessThan;
            else if (c1 > c2)
                return kCFCompareGreaterThan;
            len--;
        }
        if (len == 0 && *chs == 0)
            return kCFCompareEqualTo;
        return kCFCompareGreaterThan;
    }
}

// operators associated with QString ===========================================

bool operator==(const QString &qs1, const QString &qs2)
{
    return CFEqual(qs1.s, qs2.s);
}

bool operator==(const QString &qs, const char *chs)
{
    if (!chs)
        return qs.isEmpty();
    const char *latin1 = qs.latin1();
    if (!latin1)
        return false;
    return strcmp(latin1, chs) == 0;
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

QString operator+(const QString &qs, QChar qc)
{
    QString tmp(qs);
    tmp += qc;
    return tmp;
}

QString operator+(const QString &qs, char ch)
{
    QString tmp(qs);
    tmp += ch;
    return tmp;
}

QString operator+(const char *chs, const QString &qs)
{
    QString tmp(chs);
    tmp += qs;
    return tmp;
}

QString operator+(QChar qc, const QString &qs)
{
    QString tmp = qc;
    tmp += qs;
    return tmp;
}

QString operator+(char ch, const QString &qs)
{
    QString tmp = QChar(ch);
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
        //s = CFStringCreateMutableWithExternalCharactersNoCopy(
        //        kCFAllocatorDefault, &qcs->c, len, len, kCFAllocatorNull);
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppendCharacters(s, &qcs->c, len);
    } else {
        s = getNullCFString();
    }
    cacheType = CacheInvalid;
}

// member functions ------------------------------------------------------------

#else // USING_BORROWED_QSTRING
// This will help to keep the linker from complaining about empty archives
void KWQString_Dummy() {}
#endif // USING_BORROWED_QSTRING
