/*
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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
#include "YarrInterpreter.h"
#include <wtf/Assertions.h>
#include <wtf/BumpPointerAllocator.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC { namespace Yarr {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RegularExpression);

class RegularExpression::Private : public RefCounted<RegularExpression::Private> {
public:
    static Ref<Private> create(StringView pattern, OptionSet<Flags> flags)
    {
        return adoptRef(*new Private(pattern, flags));
    }

private:
    Private(StringView pattern, OptionSet<Flags> flags)
        : m_regExpByteCode(compile(pattern, flags))
    {
    }

    std::unique_ptr<BytecodePattern> compile(StringView patternString, OptionSet<Flags> flags)
    {
        ASSERT(!(flags - OptionSet<Flags> { Flags::IgnoreCase, Flags::Multiline, Flags::UnicodeSets }));

        YarrPattern pattern(patternString, flags, m_constructionErrorCode);
        if (hasError(m_constructionErrorCode)) {
            LOG_ERROR("RegularExpression: YARR compile failed with '%s'", errorMessage(m_constructionErrorCode).characters());
            return nullptr;
        }

        m_numSubpatterns = pattern.m_numSubpatterns;

        return byteCompile(pattern, &m_regexAllocator, m_constructionErrorCode);
    }

    ErrorCode m_constructionErrorCode { Yarr::ErrorCode::NoError };
    BumpPointerAllocator m_regexAllocator;

public:
    int lastMatchLength { -1 };
    unsigned m_numSubpatterns;
    std::unique_ptr<BytecodePattern> m_regExpByteCode;
};

RegularExpression::RegularExpression(StringView pattern, OptionSet<Flags> flags)
    : d(Private::create(pattern, flags))
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

int RegularExpression::match(StringView str, unsigned startFrom, int* matchLength) const
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
        offsetVector[j] = offsetNoMatch;

    unsigned result;
    if (str.length() <= INT_MAX)
        result = interpret(d->m_regExpByteCode.get(), str, startFrom, offsetVector);
    else {
        // This code can't handle unsigned offsets. Limit our processing to strings with offsets that
        // can be represented as ints.
        result = offsetNoMatch;
    }

    if (result == offsetNoMatch) {
        d->lastMatchLength = -1;
        return -1;
    }

    // 1 means 1 match; 0 means more than one match. First match is recorded in offsetVector.
    d->lastMatchLength = offsetVector[1] - offsetVector[0];
    if (matchLength)
        *matchLength = d->lastMatchLength;
    return offsetVector[0];
}

int RegularExpression::searchRev(StringView str) const
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

void replace(String& string, const RegularExpression& target, StringView replacement)
{
    int index = 0;
    while (index < static_cast<int>(string.length())) {
        int matchLength;
        index = target.match(string, index, &matchLength);
        if (index < 0)
            break;
        string = makeStringByReplacing(string, index, matchLength, replacement);
        if (!matchLength)
            break; // Avoid infinite loop on 0-length matches, e.g. [a-z]*
        index += replacement.length();
    }
}

bool RegularExpression::isValid() const
{
    return d->m_regExpByteCode.get();
}

} } // namespace JSC::Yarr
