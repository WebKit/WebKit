/*
 * Copyright (C) 2004, 2008, 2009 Apple Inc. All rights reserved.
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

#include "Logging.h"
#include <pcre/pcre.h>

namespace WebCore {

class RegularExpression::Private : public RefCounted<Private> {
public:
    static PassRefPtr<Private> create(const String& pattern, TextCaseSensitivity);
    ~Private();

    JSRegExp* regexp() const { return m_regexp; }
    int lastMatchLength;    

private:
    Private(const String& pattern, TextCaseSensitivity);
    static JSRegExp* compile(const String& pattern, TextCaseSensitivity);

    JSRegExp* m_regexp;
};

inline JSRegExp* RegularExpression::Private::compile(const String& pattern, TextCaseSensitivity caseSensitivity)
{
    const char* errorMessage;
    JSRegExp* regexp = jsRegExpCompile(pattern.characters(), pattern.length(),
        caseSensitivity == TextCaseSensitive ? JSRegExpDoNotIgnoreCase : JSRegExpIgnoreCase, JSRegExpSingleLine,
        0, &errorMessage);
    if (!regexp)
        LOG_ERROR("RegularExpression: pcre_compile failed with '%s'", errorMessage);
    return regexp;
}

inline RegularExpression::Private::Private(const String& pattern, TextCaseSensitivity caseSensitivity)
    : lastMatchLength(-1)
    , m_regexp(compile(pattern, caseSensitivity))
{
}

inline PassRefPtr<RegularExpression::Private> RegularExpression::Private::create(const String& pattern, TextCaseSensitivity caseSensitivity)
{
    return adoptRef(new Private(pattern, caseSensitivity));
}

RegularExpression::Private::~Private()
{
    jsRegExpFree(m_regexp);
}

RegularExpression::RegularExpression(const String& pattern, TextCaseSensitivity caseSensitivity)
    : d(Private::create(pattern, caseSensitivity))
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
    d = re.d;
    return *this;
}

int RegularExpression::match(const String& str, int startFrom, int* matchLength) const
{
    if (!d->regexp())
        return -1;

    if (str.isNull())
        return -1;

    // First 2 offsets are start and end offsets; 3rd entry is used internally by pcre
    static const size_t maxOffsets = 3;
    int offsets[maxOffsets];
    int result = jsRegExpExecute(d->regexp(), str.characters(), str.length(), startFrom, offsets, maxOffsets);
    if (result < 0) {
        if (result != JSRegExpErrorNoMatch)
            LOG_ERROR("RegularExpression: pcre_exec() failed with result %d", result);
        d->lastMatchLength = -1;
        return -1;
    }

    // 1 means 1 match; 0 means more than one match. First match is recorded in offsets.
    d->lastMatchLength = offsets[1] - offsets[0];
    if (matchLength)
        *matchLength = d->lastMatchLength;
    return offsets[0];
}

int RegularExpression::searchRev(const String& str) const
{
    // FIXME: This could be faster if it actually searched backwards.
    // Instead, it just searches forwards, multiple times until it finds the last match.

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
    d->lastMatchLength = lastMatchLength;
    return lastPos;
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
