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
#include "RegularExpression.h"

#include "Logging.h"
#include <wtf/RefCounted.h>
#include <pcre/pcre.h>
#include <sys/types.h>

namespace WebCore {

const size_t maxSubstrings = 10;
const size_t maxOffsets = 3 * maxSubstrings;

class RegularExpression::Private : public RefCounted<RegularExpression::Private>
{
public:
    Private();
    Private(DeprecatedString pattern, bool caseSensitive, bool glob);
    ~Private();

    void compile(bool caseSensitive, bool glob);

    DeprecatedString pattern;
    JSRegExp* regex;

    DeprecatedString lastMatchString;
    int lastMatchOffsets[maxOffsets];
    int lastMatchCount;
    int lastMatchPos;
    int lastMatchLength;
};

RegularExpression::Private::Private() : pattern("")
{
    compile(true, false);
}

RegularExpression::Private::Private(DeprecatedString p, bool caseSensitive, bool glob) : pattern(p), lastMatchPos(-1), lastMatchLength(-1)
{
    compile(caseSensitive, glob);
}

static DeprecatedString RegExpFromGlob(DeprecatedString glob)
{
    DeprecatedString result = glob;

    // escape regexp metacharacters which are NOT glob metacharacters

    result.replace(RegularExpression("\\\\"), "\\\\");
    result.replace(RegularExpression("\\."), "\\.");
    result.replace(RegularExpression("\\+"), "\\+");
    result.replace(RegularExpression("\\$"), "\\$");
    // FIXME: incorrect for ^ inside bracket group
    result.replace(RegularExpression("\\^"), "\\^");

    // translate glob metacharacters into regexp metacharacters
    result.replace(RegularExpression("\\*"), ".*");
    result.replace(RegularExpression("\\?"), ".");
   
    // Require the glob to match the whole string
    result = "^" + result + "$";

    return result;
}

void RegularExpression::Private::compile(bool caseSensitive, bool glob)
{
    DeprecatedString p;

    if (glob) {
        p = RegExpFromGlob(pattern);
    } else {
        p = pattern;
    }
    // Note we don't honor the Qt syntax for various character classes.  If we convert
    // to a different underlying engine, we may need to change client code that relies
    // on the regex syntax (see FrameMac.mm for a couple examples).
    
    const char* errorMessage;
    regex = jsRegExpCompile(reinterpret_cast<const UChar*>(p.unicode()), p.length(),
        caseSensitive ? JSRegExpDoNotIgnoreCase : JSRegExpIgnoreCase, JSRegExpSingleLine,
        0, &errorMessage);
    if (!regex)
        LOG_ERROR("RegularExpression: pcre_compile failed with '%s'", errorMessage);
}

RegularExpression::Private::~Private()
{
    jsRegExpFree(regex);
}


RegularExpression::RegularExpression() : d(new RegularExpression::Private())
{
}

RegularExpression::RegularExpression(const DeprecatedString &pattern, bool caseSensitive, bool glob) : d(new RegularExpression::Private(pattern, caseSensitive, glob))
{
}

RegularExpression::RegularExpression(const char *cpattern) : d(new RegularExpression::Private(cpattern, true, false))
{
}


RegularExpression::RegularExpression(const RegularExpression &re) : d (re.d)
{
}

RegularExpression::~RegularExpression()
{
}

RegularExpression &RegularExpression::operator=(const RegularExpression &re)
{
    RegularExpression tmp(re);
    RefPtr<RegularExpression::Private> tmpD = tmp.d;
    
    tmp.d = d;
    d = tmpD;

    return *this;
}

DeprecatedString RegularExpression::pattern() const
{
    return d->pattern;
}

int RegularExpression::match(const DeprecatedString &str, int startFrom, int *matchLength) const
{
    d->lastMatchString = str;
    // First 2 offsets are start and end offsets; 3rd entry is used internally by pcre
    d->lastMatchCount = jsRegExpExecute(d->regex, reinterpret_cast<const UChar*>(d->lastMatchString.unicode()), d->lastMatchString.length(), startFrom, d->lastMatchOffsets, maxOffsets);
    if (d->lastMatchCount < 0) {
        if (d->lastMatchCount != JSRegExpErrorNoMatch)
            LOG_ERROR("RegularExpression: pcre_exec() failed with result %d", d->lastMatchCount);
        d->lastMatchPos = -1;
        d->lastMatchLength = -1;
        d->lastMatchString = DeprecatedString();
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

int RegularExpression::search(const DeprecatedString &str, int startFrom) const
{
    if (startFrom < 0) {
        startFrom = str.length() - startFrom;
    }
    return match(str, startFrom, NULL);
}

int RegularExpression::searchRev(const DeprecatedString &str) const
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

int RegularExpression::pos(int n)
{
    ASSERT(n == 0);
    return d->lastMatchPos;
}

int RegularExpression::matchedLength() const
{
    return d->lastMatchLength;
}

}
