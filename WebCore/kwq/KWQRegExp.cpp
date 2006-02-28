/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "KWQRegExp.h"
#include "Logging.h"

#include <sys/types.h>
#include <pcre/pcre.h>
#include "Shared.h"

#define MAX_SUBSTRINGS  10
#define MAX_OFFSETS     (3 *MAX_SUBSTRINGS)

class QRegExp::KWQRegExpPrivate : public Shared<QRegExp::KWQRegExpPrivate>
{
public:
    KWQRegExpPrivate();
    KWQRegExpPrivate(QString pattern, bool caseSensitive, bool glob);
    ~KWQRegExpPrivate();

    void compile(bool caseSensitive, bool glob);

    QString pattern;
    pcre *regex;

    QString lastMatchString;
    int lastMatchOffsets[MAX_OFFSETS];
    int lastMatchCount;
    int lastMatchPos;
    int lastMatchLength;
};

QRegExp::KWQRegExpPrivate::KWQRegExpPrivate() : pattern("")
{
    compile(true, false);
}

QRegExp::KWQRegExpPrivate::KWQRegExpPrivate(QString p, bool caseSensitive, bool glob) : pattern(p), lastMatchPos(-1), lastMatchLength(-1)
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
    // on the regex syntax (see MacFrame.mm for a couple examples).
    
    const char *errorMessage;
    int errorOffset;
    char null = 0;
    p.append(null);
    regex = pcre_compile(reinterpret_cast<const uint16_t *>(p.unicode()), caseSensitive ? 0 : PCRE_CASELESS, &errorMessage, &errorOffset, NULL);
    if (regex == NULL) {
        LOG_ERROR("KWQRegExp: pcre_compile failed with '%s'", errorMessage);
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
    RefPtr<QRegExp::KWQRegExpPrivate> tmpD = tmp.d;
    
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
    d->lastMatchString = str;
    // First 2 offsets are start and end offsets; 3rd entry is used internally by pcre
    d->lastMatchCount = pcre_exec(d->regex, NULL, reinterpret_cast<const uint16_t *>(d->lastMatchString.unicode()), d->lastMatchString.length(), startFrom, startFrom == 0 ? 0 : PCRE_NOTBOL, d->lastMatchOffsets, MAX_OFFSETS);
    if (d->lastMatchCount < 0) {
        if (d->lastMatchCount != PCRE_ERROR_NOMATCH)
            LOG_ERROR("KWQRegExp: pcre_exec() failed with result %d", d->lastMatchCount);
        d->lastMatchPos = -1;
        d->lastMatchLength = -1;
        d->lastMatchString = QString();
        return -1;
    }
    
    // 1 means 1 match; 0 means more than one match. First match is recorded in offsets.
    //ASSERT(d->lastMatchCount < 2);
    d->lastMatchPos = d->lastMatchOffsets[0];
    d->lastMatchLength = d->lastMatchOffsets[1] - d->lastMatchOffsets[0];
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

QString QRegExp::cap(int n) const
{
    const pcre_char *substring = NULL;
    int substringLength = pcre_get_substring(reinterpret_cast<const uint16_t *>(d->lastMatchString.unicode()), d->lastMatchOffsets, d->lastMatchCount, n, &substring);
    if (substringLength > 0) {
       QString capture(reinterpret_cast<const QChar *>(substring), substringLength);
       pcre_free_substring(substring);
       return capture;
    }
    return QString();
}
