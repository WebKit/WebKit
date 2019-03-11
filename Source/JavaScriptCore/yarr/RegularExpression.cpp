/*
 * Copyright (C) 2004, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2011 Peter Varga (pvarga@webkit.org), University of Szeged
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "Yarr.h"
#include "YarrFlags.h"
#include "YarrInterpreter.h"
#include <wtf/Assertions.h>
#include <wtf/BumpPointerAllocator.h>

namespace JSC { namespace Yarr {

class RegularExpression::Private : public RefCounted<RegularExpression::Private> {
public:
    static Ref<Private> create(const String& pattern, TextCaseSensitivity caseSensitivity, MultilineMode multilineMode, UnicodeMode unicodeMode)
    {
        return adoptRef(*new Private(pattern, caseSensitivity, multilineMode, unicodeMode));
    }

    int lastMatchLength { -1 };

    unsigned m_numSubpatterns;
    std::unique_ptr<JSC::Yarr::BytecodePattern> m_regExpByteCode;

private:
    Private(const String& pattern, TextCaseSensitivity caseSensitivity, MultilineMode multilineMode, UnicodeMode unicodeMode)
        : m_regExpByteCode(compile(pattern, caseSensitivity, multilineMode, unicodeMode))
    {
    }

    std::unique_ptr<JSC::Yarr::BytecodePattern> compile(const String& patternString, TextCaseSensitivity caseSensitivity, MultilineMode multilineMode, UnicodeMode unicodeMode)
    {
        OptionSet<JSC::Yarr::Flags> flags;

        if (caseSensitivity == TextCaseInsensitive)
            flags.add(Flags::IgnoreCase);

        if (multilineMode == MultilineEnabled)
            flags.add(Flags::Multiline);

        if (unicodeMode == UnicodeAwareMode)
            flags.add(Flags::Unicode);

        JSC::Yarr::YarrPattern pattern(patternString, flags, m_constructionErrorCode);
        if (JSC::Yarr::hasError(m_constructionErrorCode)) {
            LOG_ERROR("RegularExpression: YARR compile failed with '%s'", JSC::Yarr::errorMessage(m_constructionErrorCode));
            return nullptr;
        }

        m_numSubpatterns = pattern.m_numSubpatterns;

        return JSC::Yarr::byteCompile(pattern, &m_regexAllocator);
    }

    BumpPointerAllocator m_regexAllocator;
    JSC::Yarr::ErrorCode m_constructionErrorCode { Yarr::ErrorCode::NoError };
};

RegularExpression::RegularExpression(const String& pattern, TextCaseSensitivity caseSensitivity, MultilineMode multilineMode, UnicodeMode unicodeMode)
    : d(Private::create(pattern, caseSensitivity, multilineMode, unicodeMode))
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
    if (!d->m_regExpByteCode)
        return -1;

    if (str.isNull())
        return -1;

    int offsetVectorSize = (d->m_numSubpatterns + 1) * 2;
    unsigned* offsetVector;
    Vector<unsigned, 32> nonReturnedOvector;

    nonReturnedOvector.grow(offsetVectorSize);
    offsetVector = nonReturnedOvector.data();

    ASSERT(offsetVector);
    for (unsigned j = 0, i = 0; i < d->m_numSubpatterns + 1; j += 2, i++)
        offsetVector[j] = JSC::Yarr::offsetNoMatch;

    unsigned result;
    if (str.length() <= INT_MAX)
        result = JSC::Yarr::interpret(d->m_regExpByteCode.get(), str, startFrom, offsetVector);
    else {
        // This code can't handle unsigned offsets. Limit our processing to strings with offsets that
        // can be represented as ints.
        result = JSC::Yarr::offsetNoMatch;
    }

    if (result == JSC::Yarr::offsetNoMatch) {
        d->lastMatchLength = -1;
        return -1;
    }

    // 1 means 1 match; 0 means more than one match. First match is recorded in offsetVector.
    d->lastMatchLength = offsetVector[1] - offsetVector[0];
    if (matchLength)
        *matchLength = d->lastMatchLength;
    return offsetVector[0];
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
            break; // Avoid infinite loop on 0-length matches, e.g. [a-z]*
    }
}

bool RegularExpression::isValid() const
{
    return d->m_regExpByteCode.get();
}

} } // namespace JSC::Yarr
