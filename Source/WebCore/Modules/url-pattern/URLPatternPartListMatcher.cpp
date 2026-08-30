/*
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "URLPatternPartListMatcher.h"

#include "URLPatternParser.h"
#include <optional>

namespace WebCore {
namespace URLPatternUtilities {

static bool literalMatchesAt(StringView input, unsigned position, StringView literal, unsigned& endPosition)
{
    if (position > input.length() || literal.length() > input.length() - position)
        return false;

    // Both the input and the literal have already been case-folded when the matcher was constructed
    // with ignoreCase, so this comparison is always case-sensitive.
    if (StringView(input).substring(position, literal.length()) != literal)
        return false;

    endPosition = position + literal.length();
    return true;
}

static inline bool isComponentDelimiter(char16_t character, StringView delimiterCodepoint)
{
    return !delimiterCodepoint.isEmpty() && character == delimiterCodepoint[0];
}

// Appends every end position reachable by matching exactly one occurrence of `part` (prefix, then
// its core, then suffix) starting at `start`. This mirrors how generateRegexAndNameList() would
// encode the part, but evaluated directly against the input rather than via a regular expression.
static void appendSingleOccurrenceEnds(const Part& part, StringView input, unsigned start, StringView delimiterCodepoint, Vector<unsigned>& ends)
{
    unsigned length = input.length();

    unsigned afterPrefix;
    if (!literalMatchesAt(input, start, part.prefix, afterPrefix))
        return;

    auto appendWithSuffix = [&](unsigned afterCore) {
        unsigned afterSuffix;
        if (literalMatchesAt(input, afterCore, part.suffix, afterSuffix))
            ends.append(afterSuffix);
    };

    switch (part.type) {
    case PartType::FixedText: {
        unsigned afterCore;
        if (literalMatchesAt(input, afterPrefix, part.value, afterCore))
            appendWithSuffix(afterCore);
        break;
    }
    case PartType::SegmentWildcard: {
        for (unsigned position = afterPrefix; position < length && !isComponentDelimiter(input[position], delimiterCodepoint); ) {
            ++position;
            appendWithSuffix(position);
        }
        break;
    }
    case PartType::FullWildcard: {
        for (unsigned position = afterPrefix; position <= length; ++position)
            appendWithSuffix(position);
        break;
    }
    case PartType::Regexp:
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("URLPatternPartListMatcher cannot match regexp parts; use the regex-based URLPatternComponent::compile() instead");
        break;
    }
}

static void markReachableEnds(const Part& part, StringView input, unsigned start, StringView delimiterCodepoint, Vector<bool>& reachable)
{
    switch (part.modifier) {
    case Modifier::None: {
        Vector<unsigned> ends;
        appendSingleOccurrenceEnds(part, input, start, delimiterCodepoint, ends);
        for (unsigned end : ends)
            reachable[end] = true;
        return;
    }
    case Modifier::Optional: {
        reachable[start] = true;
        Vector<unsigned> ends;
        appendSingleOccurrenceEnds(part, input, start, delimiterCodepoint, ends);
        for (unsigned end : ends)
            reachable[end] = true;
        return;
    }
    case Modifier::ZeroOrMore:
    case Modifier::OneOrMore: {
        if (part.modifier == Modifier::ZeroOrMore)
            reachable[start] = true;

        Vector<bool> expanded(FillWith { }, input.length() + 1, false);
        Vector<unsigned> worklist;
        worklist.append(start);
        while (!worklist.isEmpty()) {
            unsigned position = worklist.takeLast();
            if (expanded[position])
                continue;
            expanded[position] = true;

            Vector<unsigned> ends;
            appendSingleOccurrenceEnds(part, input, position, delimiterCodepoint, ends);
            for (unsigned end : ends) {
                reachable[end] = true;
                if (!expanded[end])
                    worklist.append(end);
            }
        }
        return;
    }
    }
}

static bool matchPartListFrom(const Vector<Part>& parts, unsigned partIndex, unsigned position, StringView input, StringView delimiterCodepoint, Vector<std::optional<bool>>& memo)
{
    unsigned length = input.length();
    if (partIndex == parts.size())
        return position == length;

    unsigned key = partIndex * (length + 1) + position;
    if (memo[key])
        return *memo[key];

    Vector<bool> reachable(FillWith { }, length + 1, false);
    markReachableEnds(parts[partIndex], input, position, delimiterCodepoint, reachable);

    bool result = false;
    for (unsigned end = 0; end <= length && !result; ++end) {
        if (reachable[end] && matchPartListFrom(parts, partIndex + 1, end, input, delimiterCodepoint, memo))
            result = true;
    }

    memo[key] = result;
    return result;
}

URLPatternPartListMatcher::URLPatternPartListMatcher(Vector<Part>&& partList, const URLPatternStringOptions& options)
    : m_partList(WTF::move(partList))
    , m_delimiterCodepoint(options.delimiterCodepoint)
    , m_ignoreCase(options.ignoreCase)
{
    ASSERT(m_delimiterCodepoint.isEmpty() || m_delimiterCodepoint == "."_s || m_delimiterCodepoint == "/"_s);

    if (m_ignoreCase) {
        for (auto& part : m_partList) {
            part.prefix = part.prefix.foldCase();
            part.value = part.value.foldCase();
            part.suffix = part.suffix.foldCase();
        }
    }
}

URLPatternPartListMatcher::URLPatternPartListMatcher(URLPatternPartListMatcher&&) = default;
URLPatternPartListMatcher& URLPatternPartListMatcher::operator=(URLPatternPartListMatcher&&) = default;
URLPatternPartListMatcher::~URLPatternPartListMatcher() = default;

bool URLPatternPartListMatcher::matches(StringView input) const
{
    String foldedInput;
    if (m_ignoreCase) {
        foldedInput = input.toString().foldCase();
        input = foldedInput;
    }

    unsigned length = input.length();
    Vector<std::optional<bool>> memo((m_partList.size() + 1) * (length + 1));
    return matchPartListFrom(m_partList, 0, 0, input, m_delimiterCodepoint, memo);
}

} // namespace URLPatternUtilities
} // namespace WebCore
