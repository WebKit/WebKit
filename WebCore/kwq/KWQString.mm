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

/*
    This implementation uses CFMutableStringRefs as a rep for the actual
    string data.  Reps may be shared between QString instances, and follow
    a copy-on-write sematic.  If you change the implementation be sure to
    copy the rep before calling any of the CF functions that might mutate
    the string.
*/

#import <Foundation/Foundation.h>
#import <kwqdebug.h>
#import <qstring.h>
#import <qregexp.h>
#import <stdio.h>

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
                char *buf = (char *)CFAllocatorAllocate(kCFAllocatorDefault, len + 1, 0);
                strncpy(buf, chs, len);
                *(buf + len) = '\0';
                CFStringAppendCString(qs.s, buf, encoding);
                CFAllocatorDeallocate(kCFAllocatorDefault, buf);
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
    QString qs;

    qs.s = CFStringCreateMutableCopy(NULL, 0, cfs);
    return qs;
}

QString QString::fromNSString(NSString *nss)
{
    QString qs;

    qs.s = CFStringCreateMutableCopy(NULL, 0, (CFStringRef)nss);
    return qs;
}

QString QString::gstring_toQString(CFMutableStringRef *ref, UniChar *uchars, int len)
{
    if (*ref == 0)
        *ref = CFStringCreateMutableWithExternalCharactersNoCopy(kCFAllocatorDefault, uchars, len, len, kCFAllocatorDefault);
    else
        CFStringSetExternalCharactersNoCopy(*ref, uchars, len, len);
    return QString::fromCFMutableString(*ref);
}

CFMutableStringRef QString::gstring_toCFString(CFMutableStringRef *ref, UniChar *uchars, int len)
{
    if (*ref == 0)
        *ref = CFStringCreateMutableWithExternalCharactersNoCopy(kCFAllocatorDefault, uchars, len, len, kCFAllocatorDefault);
    else
        CFStringSetExternalCharactersNoCopy(*ref, uchars, len, len);
    return *ref;
}

QString::QString()
{
    s = getNullCFString();
    cache = NULL;
}

QString::~QString()
{
    CFRelease(s);
    if (cache)
        CFAllocatorDeallocate(kCFAllocatorDefault, cache);
}

QString::QString(QChar qc)
{
    s = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringAppendCharacters(s, &qc.c, 1);
    cache = NULL;
}

QString::QString(const QByteArray &qba)
{
    if (qba.size() && *qba.data()) {
        CFStringRef tmp = CFStringCreateWithBytes
            (kCFAllocatorDefault, (const UInt8 *) qba.data(), qba.size(),
             kCFStringEncodingISOLatin1, false);
        s = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, tmp);
        CFRelease(tmp);
    } else
        s = getNullCFString();
    cache = NULL;
}

QString::QString(const QChar *qcs, uint len)
{
    if (qcs || len) {
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppendCharacters(s, &qcs->c, len);
    } else
        s = getNullCFString();
    cache = NULL;
}

QString::QString(const char *chs)
{
    if (chs && *chs) {
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppendCString(s, chs, kCFStringEncodingISOLatin1);
    } else
        s = getNullCFString();
    cache = NULL;
}

QString::QString(const char *chs, int len)
{
    if (len > 0) {
        CFStringRef tmp = CFStringCreateWithBytes
             (kCFAllocatorDefault, (const UInt8 *)chs, len,
              kCFStringEncodingISOLatin1, false);
        s = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, tmp);
        CFRelease(tmp);
    } else
        s = getNullCFString();
    cache = NULL;
}

QString::QString(const QString &qs)
{
    // shared copy
    CFRetain(qs.s);
    s = qs.s;
    cache = NULL;
}

QString &QString::operator=(const QString &qs)
{
    // shared copy
    CFRetain(qs.s);
    CFRelease(s);
    s = qs.s;
    flushCache();
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
    const UniChar *ucs = CFStringGetCharactersPtr(s);
    if (ucs)
        return (QChar *) ucs;

    uint len = length();
    if (len == 0)
        return getNullQCharString();

    if (cache == NULL || * (int *) cache != CacheUnicode) {
        flushCache();
        cache = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(int) + len * sizeof (UniChar), 0);
        * (int *) cache = CacheUnicode;
        CFStringGetCharacters(s, CFRangeMake(0, len), (UniChar *) ((int *) cache + 1));
    }

    return (QChar *) ((int *) cache + 1); 
}

const char *QString::latin1() const
{
    const char *chs = CFStringGetCStringPtr(s, kCFStringEncodingISOLatin1);
    if (chs)
        return chs;

    uint len = length();
    if (len == 0)
        return "";
        
    if (cache == NULL || * (int *) cache != CacheLatin1) {
        flushCache();
        cache = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(int) + len + 1, 0);
        * (int *) cache = CacheLatin1;
        if (!CFStringGetCString(s, (char *) ((int *) cache + 1), len + 1, kCFStringEncodingISOLatin1)) {
            * (char *) ((int *) cache + 1) = '\0';
        }
    }

    return (char *) ((int *) cache + 1);
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

static bool compareToLatinCharacter (UniChar c1, UniChar c2, bool caseSensitive)
{
    if (!caseSensitive){
        if (c2 >= 'a' && c2 <= 'z'){
            if (c1 == c2 || c1 == c2 - caseDelta)
                return true;
        }
        else if (c2 >= 'A' && c2 <= 'Z'){
            if (c1 == c2 || c1 == c2 + caseDelta)
                return true;
        }
        else if (c1 == c2)
            return true;
    }
    else if (c1 == c2)
        return true;
    return false;
}

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
                    if (compareToLatinCharacter(*internalBuffer++,firstC,caseSensitive)){
                        const UniChar *compareTo = internalBuffer;
                        
                        found = len - remaining;
                        while ( (c2 = (UniChar)(*_chs++)) ){
                            c1 = (UniChar)(*compareTo++);
                            if (compareToLatinCharacter(c1, c2,caseSensitive))
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
    n = CFStringGetDoubleValue(s);
    if (ok) {
        // NOTE: since CFStringGetDoubleValue returns 0.0 on error there is no
        // way to know if "n" is valid in that case
        //
        // EXTRA NOTE: We can't assume 0.0 is bad, since it totally breaks
        // html like border="0". So, only trigger breakage if the char 
        // at index 0 is neither a '0' nor a '.' nor a '-'.
        *ok = true;
        if (n == 0.0) {
            UniChar uc = CFStringGetLength(s) == 0 ? 0 : CFStringGetCharacterAtIndex(s, 0);
            if (uc != '0' && uc != '.' && uc != '-') {
                *ok = false;
            }
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
    // does not need to be a deep copy
    return *this;
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
        CFStringInlineBuffer buf;
        CFStringInitInlineBuffer(s, &buf, CFRangeMake(0, len));
        bool chars = false;
        bool space = false;
        for (CFIndex i = 0; i < len; i++) {
            UniChar uc = CFStringGetCharacterFromInlineBuffer(&buf, i);
            if (CFCharacterSetIsCharacterMember(wscs, uc)) {
                if (chars)
                    space = true;
            } else {
                if (space) {
                    UniChar spc = ' ';
                    CFStringAppendCharacters(qs.s, &spc, 1);
                    space = false;
                }
                CFStringAppendCharacters(qs.s, &uc, 1);
                chars = true;
            }
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
    CFRelease(s);
    if (qcs && len) {
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppendCharacters(s, &qcs->c, len);
    } else {
        s = getNullCFString();
    }
    return *this;
}

QString &QString::setLatin1(const char *chs)
{
    flushCache();
    CFRelease(s);
    if (chs && *chs) {
        s = CFStringCreateMutable(kCFAllocatorDefault, 0);
        CFStringAppendCString(s, chs, kCFStringEncodingISOLatin1);
    } else {
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
    buf[snprintf(buf, capacity - 1, "%ld", n)] = '\0';
    return setLatin1(buf);
}

QString &QString::setNum(ulong n)
{
    const int capacity = 64;
    char buf[capacity];
    buf[snprintf(buf, capacity - 1, "%lu", n)] = '\0';
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
    CFRelease(s);
    if (format && *format) {
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
    flushCache();
    _copyIfNeededInternalString();
    UniChar uch = qc.unicode();
    if (index < (uint) CFStringGetLength(s)) {
        CFStringRef chs = CFStringCreateWithCharactersNoCopy(NULL, &uch, 1, kCFAllocatorNull);
        CFStringInsert(s, index, chs);
        CFRelease(chs);
    } else {
        CFStringAppendCharacters(s, &uch, 1);
    }
    return *this;
}

QString &QString::insert(uint index, char ch)
{
    flushCache();
    _copyIfNeededInternalString();
    UniChar uch = (uchar) ch;
    if (index < (uint) CFStringGetLength(s)) {
        CFStringRef chs = CFStringCreateWithCharactersNoCopy(NULL, &uch, 1, kCFAllocatorNull);
        CFStringInsert(s, index, chs);
        CFRelease(chs);
    } else {
        CFStringAppendCharacters(s, &uch, 1);
    }
    return *this;
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
        UniChar *ucs = (UniChar *)CFAllocatorAllocate(kCFAllocatorDefault, len * sizeof (UniChar), 0);
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
    return insert(length(), ch);
}

void QString::flushCache() const
{
    if (cache) {
        CFAllocatorDeallocate(kCFAllocatorDefault, cache);
        cache = NULL;
    }
}

QCString QString::convertToQCString(CFStringEncoding enc) const
{
    uint len = length();
    if (len) {
        char *chs = (char *)CFAllocatorAllocate(kCFAllocatorDefault, len + 1, 0);
        if (chs) {
            if (!CFStringGetCString(s, chs, len + 1, enc)) {
                *reinterpret_cast<char *>(chs) = '\0';
            }
            QCString qcs = QCString(chs);
            CFAllocatorDeallocate(kCFAllocatorDefault, chs);
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
                qs.s = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                        tmp);
                CFRelease(tmp);
            } else {
                CFRetain(s);
                qs.s = s;
            }
        }
    }
    return qs;
}

int QString::compareToLatin1(const char *chs) const
{
    if (!chs || !*chs) {
        if (length() == 0)
            return kCFCompareEqualTo;
        return kCFCompareGreaterThan;
    }

    CFIndex len = CFStringGetLength(s);
    CFStringInlineBuffer buf;
    CFStringInitInlineBuffer(s, &buf, CFRangeMake(0, len));
    for (CFIndex i = 0; i < len; i++) {
        UniChar c1 = CFStringGetCharacterFromInlineBuffer(&buf, i);
        UniChar c2 = (uchar) chs[i];
        if (c1 < c2)
            return kCFCompareLessThan;
        if (c1 > c2)
            return kCFCompareGreaterThan;
    }
    if (chs[len] == 0)
        return kCFCompareEqualTo;
    return kCFCompareGreaterThan;
}

bool operator==(const QString &qs1, const QString &qs2)
{
    return CFEqual(qs1.s, qs2.s);
}

bool operator==(const QString &qs, const char *chs)
{
    if (!chs || !*chs) {
        if (qs.length() == 0)
            return kCFCompareEqualTo;
        return kCFCompareGreaterThan;
    }

    CFIndex len = CFStringGetLength(qs.s);
    CFIndex chsLen = strlen(chs);
    if (len != chsLen)
        return false;
    
    CFStringInlineBuffer buf;
    CFStringInitInlineBuffer(qs.s, &buf, CFRangeMake(0, len));
    for (CFIndex i = 0; i < len; i++) {
        UniChar c1 = CFStringGetCharacterFromInlineBuffer(&buf, i);
        UniChar c2 = (uchar) chs[i];
        if (c1 != c2)
            return false;
    }
    return true;
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

QConstString::QConstString(const QChar *qcs, uint len)
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
    cache = NULL;
}
