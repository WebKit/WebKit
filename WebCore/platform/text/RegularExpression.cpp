/*
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora Ltd.
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
#include "RegularExpression.h"

#include "PlatformString.h"
#include "Logging.h"
#include <wtf/RefCounted.h>

namespace WebCore {

const size_t maxSubstrings = 10;
const size_t maxOffsets = 3 * maxSubstrings;

class RegularExpression::Private : public RefCounted<Private> {
public:
    static PassRefPtr<Private> create() { return adoptRef(new Private); }
    static PassRefPtr<Private> create(const String& pattern, bool caseSensitive) { return adoptRef(new Private(pattern, caseSensitive)); }

    ~Private();

    void compile(bool caseSensitive);

    String pattern;
    JSRegExp* regex;

    String lastMatchString;
    int lastMatchOffsets[maxOffsets];
    int lastMatchCount;
    int lastMatchPos;
    int lastMatchLength;
    
private:
    Private();
    Private(const String& pattern, bool caseSensitive);
};

RegularExpression::Private::Private()
    : pattern("")
{
    compile(true);
}

RegularExpression::Private::Private(const String& p, bool caseSensitive)
    : pattern(p)
    , lastMatchPos(-1)
    , lastMatchLength(-1)
{
    compile(caseSensitive);
}

void RegularExpression::Private::compile(bool caseSensitive)
{
    const char* errorMessage;
    regex = jsRegExpCompile(pattern.characters(), pattern.length(),
        caseSensitive ? JSRegExpDoNotIgnoreCase : JSRegExpIgnoreCase, JSRegExpSingleLine,
        0, &errorMessage);
    if (!regex)
        LOG_ERROR("RegularExpression: pcre_compile failed with '%s'", errorMessage);
}

RegularExpression::Private::~Private()
{
    jsRegExpFree(regex);
}


RegularExpression::RegularExpression()
    : d(Private::create())
{
}

RegularExpression::RegularExpression(const String& pattern, bool caseSensitive)
    : d(Private::create(pattern, caseSensitive))
{
}

RegularExpression::RegularExpression(const char* pattern)
    : d(Private::create(pattern, true))
{
}


RegularExpression::RegularExpression(const RegularExpression& re)
    : d(re.d)
{
}

RegularExpression::~RegularExpression()
{
}

RegularExpression& RegularExpression::operator=(const RegularExpression& re)
{
    RegularExpression tmp(re);
    tmp.d.swap(d);
    return *this;
}

String RegularExpression::pattern() const
{
    return d->pattern;
}

int RegularExpression::match(const String& str, int startFrom, int* matchLength) const
{
    if (str.isNull())
        return -1;

    d->lastMatchString = str;
    // First 2 offsets are start and end offsets; 3rd entry is used internally by pcre
    d->lastMatchCount = jsRegExpExecute(d->regex, d->lastMatchString.characters(),
        d->lastMatchString.length(), startFrom, d->lastMatchOffsets, maxOffsets);
    if (d->lastMatchCount < 0) {
        if (d->lastMatchCount != JSRegExpErrorNoMatch)
            LOG_ERROR("RegularExpression: pcre_exec() failed with result %d", d->lastMatchCount);
        d->lastMatchPos = -1;
        d->lastMatchLength = -1;
        d->lastMatchString = String();
        return -1;
    }
    
    // 1 means 1 match; 0 means more than one match. First match is recorded in offsets.
    d->lastMatchPos = d->lastMatchOffsets[0];
    d->lastMatchLength = d->lastMatchOffsets[1] - d->lastMatchOffsets[0];
    if (matchLength)
        *matchLength = d->lastMatchLength;
    return d->lastMatchPos;
}

int RegularExpression::search(const String& str, int startFrom) const
{
    if (startFrom < 0)
        startFrom = str.length() - startFrom;
    return match(str, startFrom, 0);
}

int RegularExpression::searchRev(const String& str) const
{
    // FIXME: Total hack for now. Search forward, return the last, greedy match
    int start = 0;
    int pos;
    int lastPos = -1;
    int lastMatchLength = -1;
    do {
        int matchLength;
        pos = match(str, start, &matchLength);
        if (pos >= 0) {
            if (pos + matchLength > lastPos + lastMatchLength) {
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

int RegularExpression::pos(int n)
{
    ASSERT(n == 0);
    return d->lastMatchPos;
}

int RegularExpression::matchedLength() const
{
    return d->lastMatchLength;
}

void replace(String& string, const RegularExpression& target, const String& replacement)
{
    int index = 0;
    while (index < static_cast<int>(string.length())) {
        int matchLength;
        index = target.match(string, index, &matchLength);
        if (index < 0)
            break;
        string.replace(index, matchLength, replacement);
        index += replacement.length();
        if (!matchLength)
            break;  // Avoid infinite loop on 0-length matches, e.g. [a-z]*
    }
}

} // namespace WebCore
