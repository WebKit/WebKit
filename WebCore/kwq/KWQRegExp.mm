/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQRegExp.h"
#import "KWQLogging.h"

#import <sys/types.h>
#import <JavaScriptCore/pcre.h>


// Functions to convert between byte offets and character offsets were
// lifted from JavaScriptCore/regexp.cpp. It would be nice to share this code.
struct StringOffset {
    int offset;
    int locationInOffsetsArray;
};

static int compareStringOffsets(const void *a, const void *b)
{
    const StringOffset *oa = static_cast<const StringOffset *>(a);
    const StringOffset *ob = static_cast<const StringOffset *>(b);
    
    if (oa->offset < ob->offset) {
        return -1;
    }
    if (oa->offset > ob->offset) {
        return +1;
    }
    return 0;
}

const int sortedOffsetsFixedBufferSize = 128;

static StringOffset *createSortedOffsetsArray(const int offsets[], int numOffsets,
                                              StringOffset sortedOffsetsFixedBuffer[sortedOffsetsFixedBufferSize])
{
    // Allocate the sorted offsets.
    StringOffset *sortedOffsets;
    if (numOffsets <= sortedOffsetsFixedBufferSize) {
        sortedOffsets = sortedOffsetsFixedBuffer;
    } else {
        sortedOffsets = new StringOffset [numOffsets];
    }
    
    // Copy offsets.
    for (int i = 0; i != numOffsets; ++i) {
        sortedOffsets[i].offset = offsets[i];
        sortedOffsets[i].locationInOffsetsArray = i;
    }
    
    // Sort them.
    qsort(sortedOffsets, numOffsets, sizeof(StringOffset), compareStringOffsets);
    
    return sortedOffsets;
}

static void convertCharacterOffsetsToUTF8ByteOffsets(const char *s, int *offsets, int numOffsets)
{
    // Allocate buffer.
    StringOffset fixedBuffer[sortedOffsetsFixedBufferSize];
    StringOffset *sortedOffsets = createSortedOffsetsArray(offsets, numOffsets, fixedBuffer);
    
    // Walk through sorted offsets and string, adjusting all the offests.
    // Offsets that are off the ends of the string map to the edges of the string.
    int characterOffset = 0;
    const char *p = s;
    for (int oi = 0; oi != numOffsets; ++oi) {
        const int nextOffset = sortedOffsets[oi].offset;
        while (*p && characterOffset < nextOffset) {
            // Skip to the next character.
            ++characterOffset;
            do ++p; while ((*p & 0xC0) == 0x80); // if 1 of the 2 high bits is set, it's not the start of a character
        }
        offsets[sortedOffsets[oi].locationInOffsetsArray] = p - s;
    }
    
    // Free buffer.
    if (sortedOffsets != fixedBuffer) {
        delete [] sortedOffsets;
    }
}

static void convertUTF8ByteOffsetsToCharacterOffsets(const char *s, int *offsets, int numOffsets)
{
    // Allocate buffer.
    StringOffset fixedBuffer[sortedOffsetsFixedBufferSize];
    StringOffset *sortedOffsets = createSortedOffsetsArray(offsets, numOffsets, fixedBuffer);
    
    // Walk through sorted offsets and string, adjusting all the offests.
    // Offsets that are off the end of the string map to the edges of the string.
    int characterOffset = 0;
    const char *p = s;
    for (int oi = 0; oi != numOffsets; ++oi) {
        const int nextOffset = sortedOffsets[oi].offset;
        while (*p && (p - s) < nextOffset) {
            // Skip to the next character.
            ++characterOffset;
            do ++p; while ((*p & 0xC0) == 0x80); // if 1 of the 2 high bits is set, it's not the start of a character
        }
        offsets[sortedOffsets[oi].locationInOffsetsArray] = characterOffset;
    }
    
    // Free buffer.
    if (sortedOffsets != fixedBuffer) {
        delete [] sortedOffsets;
    }
}


class QRegExp::KWQRegExpPrivate
{
public:
    KWQRegExpPrivate();
    KWQRegExpPrivate(QString pattern, bool caseSensitive, bool glob);
    ~KWQRegExpPrivate();

    void compile(bool caseSensitive, bool glob);

    QString pattern;
    pcre *regex;
    
    uint refCount;

    int lastMatchPos;
    int lastMatchLength;
};

QRegExp::KWQRegExpPrivate::KWQRegExpPrivate() : pattern(""), refCount(0)
{
    compile(true, false);
}

QRegExp::KWQRegExpPrivate::KWQRegExpPrivate(QString p, bool caseSensitive, bool glob) : pattern(p), refCount(0), lastMatchPos(-1), lastMatchLength(-1)
{
    compile(caseSensitive, glob);
}

static QString RegExpFromGlob(QString glob)
{
    QString result = glob;

    // escape regexp metacharacters which are NOT glob metacharacters

    result.replace(QRegExp("\\\\"), "\\\\");
    result.replace(QRegExp("\\."), "\\.");
    result.replace(QRegExp("\\+"), "\\+");
    result.replace(QRegExp("\\$"), "\\$");
    // FIXME: incorrect for ^ inside bracket group
    result.replace(QRegExp("\\^"), "\\^");

    // translate glob metacharacters into regexp metacharacters
    result.replace(QRegExp("\\*"), ".*");
    result.replace(QRegExp("\\?"), ".");
   
    // Require the glob to match the whole string
    result = "^" + result + "$";

    return result;
}

void QRegExp::KWQRegExpPrivate::compile(bool caseSensitive, bool glob)
{
    QString p;

    if (glob) {
	p = RegExpFromGlob(pattern);
    } else {
	p = pattern;
    }
    // Note we don't honor the Qt syntax for various character classes.  If we convert
    // to a different underlying engine, we may need to change client code that relies
    // on the regex syntax (see KWQKHTMLPart.mm for a couple examples).
    
    QCString asUTF8;
    const char *cpattern;
    
    if (p.isAllASCII()) {
        cpattern = p.ascii();
    } else {
        asUTF8 = p.utf8();
        cpattern = asUTF8;
    }
        
    const char *errorMessage;
    int errorOffset;
    regex = pcre_compile(cpattern, PCRE_UTF8 | (caseSensitive ? 0 : PCRE_CASELESS), &errorMessage, &errorOffset, NULL);
    if (regex == NULL) {
        ERROR("KWQRegExp: pcre_compile failed with '%s'", errorMessage);
    }
}

QRegExp::KWQRegExpPrivate::~KWQRegExpPrivate()
{
    pcre_free(regex);
}


QRegExp::QRegExp() : d(new QRegExp::KWQRegExpPrivate())
{
}

QRegExp::QRegExp(const QString &pattern, bool caseSensitive, bool glob) : d(new QRegExp::KWQRegExpPrivate(pattern, caseSensitive, glob))
{
}

QRegExp::QRegExp(const char *cpattern) : d(new QRegExp::KWQRegExpPrivate(cpattern, true, false))
{
}


QRegExp::QRegExp(const QRegExp &re) : d (re.d)
{
}

QRegExp::~QRegExp()
{
}

QRegExp &QRegExp::operator=(const QRegExp &re)
{
    QRegExp tmp(re);
    KWQRefPtr<QRegExp::KWQRegExpPrivate> tmpD = tmp.d;
    
    tmp.d = d;
    d = tmpD;

    return *this;
}

QString QRegExp::pattern() const
{
    return d->pattern;
}

int QRegExp::match(const QString &str, int startFrom, int *matchLength) const
{    
    QCString asUTF8;
    const char *cstring;
    
    if (str.isAllASCII()) {
        cstring = str.ascii();
    } else {
        asUTF8 = str.utf8();
        cstring = asUTF8;
    }
        
    // first 2 offsets are start and end offsets; 3rd entry is used internally by pcre
    int offsets[3];
    convertCharacterOffsetsToUTF8ByteOffsets(cstring, &startFrom, 1);
    int result = pcre_exec(d->regex, NULL, cstring, strlen(cstring), startFrom, 
                           startFrom == 0 ? 0 : PCRE_NOTBOL, offsets, 3);
    
    if (result < 0) {
        if (result != PCRE_ERROR_NOMATCH) {
            ERROR("KWQRegExp: pcre_exec() failed with result %d", result);
        }
        d->lastMatchPos = -1;
        d->lastMatchLength = -1;
        return -1;
    }
    
    ASSERT(result < 2);
    // 1 means 1 match; 0 means more than one match, first one is recorded in offsets
    convertUTF8ByteOffsetsToCharacterOffsets(cstring, offsets, 2);
    d->lastMatchPos = offsets[0];
    d->lastMatchLength = offsets[1] - offsets[0];
    if (matchLength != NULL) {
        *matchLength = d->lastMatchLength;
    }
    return d->lastMatchPos;
}

int QRegExp::search(const QString &str, int startFrom) const
{
    if (startFrom < 0) {
        startFrom = str.length() - startFrom;
    }
    return match(str, startFrom, NULL);
}

int QRegExp::searchRev(const QString &str) const
{
    // FIXME: Total hack for now.  Search forward, return the last, greedy match
    int start = 0;
    int pos;
    int lastPos = -1;
    int lastMatchLength = -1;
    do {
        int matchLength;
        pos = match(str, start, &matchLength);
        if (pos >= 0) {
            if ((pos+matchLength) > (lastPos+lastMatchLength)) {
                // replace last match if this one is later and not a subset of the last match
                lastPos = pos;
                lastMatchLength = matchLength;
            }
            start = pos + 1;
        }
    } while (pos != -1);
    d->lastMatchPos = lastPos;
    d->lastMatchLength = lastMatchLength;
    return lastPos;
}

int QRegExp::pos(int n)
{
    ASSERT(n == 0);
    return d->lastMatchPos;
}

int QRegExp::matchedLength() const
{
    return d->lastMatchLength;
}
