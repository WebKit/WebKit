/*
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Peter Varga (pvarga@inf.u-szeged.hu), University of Szeged
 * Copyright (C) 2025 Tetsuharu Ohzeki <tetsuharu.ohzeki@gmail.com>.
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
 * OF LIABILITY, WHETHER IN  IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "YarrPattern.h"

#include "Options.h"
#include "Yarr.h"
#include "YarrCanonicalize.h"
#include "YarrParser.h"
#include <limits>
#include <wtf/BitSet.h>
#include <wtf/DataLog.h>
#include <wtf/StackCheck.h>
#include <wtf/TriState.h>
#include <wtf/Vector.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace Yarr {

#include "RegExpJitTables.h"

class CharacterClassConstructor {
public:
    CharacterClassConstructor(bool isCaseInsensitive, CompileMode compileMode)
        : m_isCaseInsensitive(isCaseInsensitive)
        , m_anyCharacter(false)
        , m_mayContainStrings(false)
        , m_invertedStrings(false)
        , m_compileMode(compileMode)
        , m_characterWidths(CharacterClassWidths::Unknown)
        , m_canonicalMode(compileMode == CompileMode::Legacy ? CanonicalMode::UCS2 : CanonicalMode::Unicode)
    {
    }
    
    void reset()
    {
        m_strings.clear();
        m_matches8.clear();
        m_ranges8.clear();
        m_matches32.clear();
        m_ranges32.clear();
        m_setOp = CharacterClassSetOp::Default;
        m_anyCharacter = false;
        m_mayContainStrings = false;
        m_invertedStrings = false;
        m_characterWidths = CharacterClassWidths::Unknown;
    }

    void NODELETE combiningSetOp(CharacterClassSetOp setOp)
    {
        ASSERT(m_setOp == CharacterClassSetOp::Default || m_setOp == setOp);
        m_setOp = setOp;
    }

    void append(const CharacterClass* other)
    {
        if (m_setOp != CharacterClassSetOp::Default) {
            performSetOpWith(other);
            return;
        }

        for (size_t i = 0; i < other->m_strings.size(); ++i)
            m_strings.append(other->m_strings[i]);
        for (size_t i = 0; i < other->m_matches8.size(); ++i)
            addSorted(m_matches8, other->m_matches8[i]);
        for (size_t i = 0; i < other->m_ranges8.size(); ++i)
            addSortedRange(m_ranges8, other->m_ranges8[i].begin, other->m_ranges8[i].end);
        for (size_t i = 0; i < other->m_matches32.size(); ++i)
            addSorted(m_matches32, other->m_matches32[i]);
        for (size_t i = 0; i < other->m_ranges32.size(); ++i)
            addSortedRange(m_ranges32, other->m_ranges32[i].begin, other->m_ranges32[i].end);
        m_mayContainStrings |= other->hasStrings();
    }

    void appendInverted(const CharacterClass* other)
    {
        auto addSortedInverted = [&](char32_t min, char32_t max,
            const Vector<char32_t>& srcMatches, const Vector<CharacterRange>& srcRanges,
            Vector<char32_t>& destMatches, Vector<CharacterRange>& destRanges) {

            auto addSortedMatchOrRange = [&](char32_t lo, char32_t hiPlusOne) {
                if (lo < hiPlusOne) {
                    if (lo + 1 == hiPlusOne)
                        addSorted(destMatches, lo);
                    else
                        addSortedRange(destRanges, lo, hiPlusOne - 1);
                }
            };

            char32_t lo = min;
            size_t matchesIndex = 0;
            size_t rangesIndex = 0;
            bool matchesRemaining = matchesIndex < srcMatches.size();
            bool rangesRemaining = rangesIndex < srcRanges.size();

            if (!matchesRemaining && !rangesRemaining) {
                addSortedMatchOrRange(min, max + 1);
                return;
            }

            while (matchesRemaining || rangesRemaining) {
                char32_t hiPlusOne;
                char32_t nextLo;

                if (matchesRemaining
                    && (!rangesRemaining || srcMatches[matchesIndex] < srcRanges[rangesIndex].begin)) {
                    hiPlusOne = srcMatches[matchesIndex];
                    nextLo = hiPlusOne + 1;
                    ++matchesIndex;
                    matchesRemaining = matchesIndex < srcMatches.size();
                } else {
                    hiPlusOne = srcRanges[rangesIndex].begin;
                    nextLo = srcRanges[rangesIndex].end + 1;
                    ++rangesIndex;
                    rangesRemaining = rangesIndex < srcRanges.size();
                }

                addSortedMatchOrRange(lo, hiPlusOne);

                lo = nextLo;
            }

            addSortedMatchOrRange(lo, max + 1);
        };

        if (other->hasStrings()) {
            m_mayContainStrings = true;
            m_invertedStrings = true;
        }

        addSortedInverted(0, 0xff, other->m_matches8, other->m_ranges8, m_matches8, m_ranges8);
        addSortedInverted(0x100, UCHAR_MAX_VALUE, other->m_matches32, other->m_ranges32, m_matches32, m_ranges32);
    }

    void putChar(char32_t ch)
    {
        if (!isUnionSetOp())
            return putCharNonUnion(ch);

        if (!m_isCaseInsensitive) {
            addSorted(ch);
            return;
        }

        if (m_canonicalMode == CanonicalMode::UCS2 && isASCII(ch)) {
            // Handle ASCII cases.
            if (isASCIIAlpha(ch)) {
                addSorted(m_matches8, toASCIIUpper(ch));
                addSorted(m_matches8, toASCIILower(ch));
            } else
                addSorted(m_matches8, ch);
            return;
        }

        // Add multiple matches, if necessary.
        const CanonicalizationRange* info = canonicalRangeInfoFor(ch, m_canonicalMode);
        if (info->type == CanonicalizeUnique)
            addSorted(ch);
        else
            putUnicodeIgnoreCase(ch, info);
    }

    void putCharNonUnion(char32_t ch)
    {
        Vector<char32_t> matches8;
        Vector<char32_t> matches32;
        Vector<CharacterRange> emptyRanges;

        if (m_setOp == CharacterClassSetOp::Intersection)
            m_strings.clear();

        auto addChar = [&] (char32_t ch) {
            if (isLatin1(ch))
                matches8.append(ch);
            else
                matches32.append(ch);
        };

        auto performOp = [&] () {
            performSetOpWithMatches(matches8, emptyRanges, matches32, emptyRanges);
        };

        if (!m_isCaseInsensitive) {
            addChar(ch);
            performOp();
            return;
        }

        if (m_canonicalMode == CanonicalMode::UCS2 && isASCII(ch)) {
            // Handle ASCII cases.
            if (isASCIIAlpha(ch)) {
                addChar(toASCIIUpper(ch));
                addChar(toASCIILower(ch));
            } else
                addChar(ch);
            performOp();
            return;
        }

        // Add multiple matches, if necessary.
        const CanonicalizationRange* info = canonicalRangeInfoFor(ch, m_canonicalMode);
        if (info->type == CanonicalizeUnique)
            addChar(ch);
        else {
            if (info->type == CanonicalizeSet) {
                for (auto* set = canonicalCharacterSetInfo(info->value, m_canonicalMode); (ch = *set); ++set)
                    addChar(ch);
            } else {
                char32_t canonicalChar = getCanonicalPair(info, ch);
                addChar(std::min(ch, canonicalChar));
                addChar(std::max(ch, canonicalChar));
            }
        }

        performOp();
    }

    void putUnicodeIgnoreCase(char32_t ch, const CanonicalizationRange* info)
    {
        ASSERT(ch >= info->begin && ch <= info->end);
        ASSERT(info->type != CanonicalizeUnique);
        if (info->type == CanonicalizeSet) {
            for (auto* set = canonicalCharacterSetInfo(info->value, m_canonicalMode); (ch = *set); ++set)
                addSorted(ch);
        } else {
            addSorted(ch);
            addSorted(getCanonicalPair(info, ch));
        }
    }

    void putRange(char32_t lo, char32_t hi)
    {
        // The ASCII case-folding fast path is only valid in UCS2 canonical mode. In Unicode
        // canonical mode, U+212A and U+017F canonicalize into ASCII 'k' and 's', so ASCII
        // ranges must go through the canonicalization table below.
        if (!m_isCaseInsensitive || m_canonicalMode == CanonicalMode::UCS2) {
            // This is ASCII-case-folding fast path. So intentionally using isASCII (not isLatin1).
            if (isASCII(lo)) {
                char asciiLo = lo;
                char asciiHi = std::min<char32_t>(hi, 0x7f);
                addSortedRange(lo, asciiHi);

                if (m_isCaseInsensitive) {
                    if ((asciiLo <= 'Z') && (asciiHi >= 'A'))
                        addSortedRange(std::max(asciiLo, 'A')+('a'-'A'), std::min(asciiHi, 'Z')+('a'-'A'));
                    if ((asciiLo <= 'z') && (asciiHi >= 'a'))
                        addSortedRange(std::max(asciiLo, 'a')+('A'-'a'), std::min(asciiHi, 'z')+('A'-'a'));
                }
            }
            if (isASCII(hi))
                return;

            lo = std::max<char32_t>(lo, 0x80);
        }
        addSortedRange(lo, hi);

        if (!m_isCaseInsensitive)
            return;

        const CanonicalizationRange* info = canonicalRangeInfoFor(lo, m_canonicalMode);
        while (true) {
            // Handle the range [lo .. end]
            char32_t end = std::min<char32_t>(info->end, hi);

            switch (info->type) {
            case CanonicalizeUnique:
                // Nothing to do - no canonical equivalents.
                break;
            case CanonicalizeSet: {
                char16_t ch;
                for (auto* set = canonicalCharacterSetInfo(info->value, m_canonicalMode); (ch = *set); ++set)
                    addSorted(ch);
                break;
            }
            case CanonicalizeRangeLo:
                addSortedRange(lo + info->value, end + info->value);
                break;
            case CanonicalizeRangeHi:
                addSortedRange(lo - info->value, end - info->value);
                break;
            case CanonicalizeAlternatingAligned:
                // Use addSortedRange since there is likely an abutting range to combine with.
                if (lo & 1)
                    addSortedRange(lo - 1, lo - 1);
                if (!(end & 1))
                    addSortedRange(end + 1, end + 1);
                break;
            case CanonicalizeAlternatingUnaligned:
                // Use addSortedRange since there is likely an abutting range to combine with.
                if (!(lo & 1))
                    addSortedRange(lo - 1, lo - 1);
                if (end & 1)
                    addSortedRange(end + 1, end + 1);
                break;
            }

            if (hi == end)
                return;

            ++info;
            lo = info->begin;
        }
    }

    void atomClassStringDisjunction(Vector<Vector<char32_t>>& disjunctionStrings)
    {
        Vector<Vector<char32_t>> utf32Strings;
        Vector<char32_t> matches8;
        Vector<char32_t> matches32;
        Vector<CharacterRange> emptyRanges;

        sort(disjunctionStrings);

        auto addCh = [&](char32_t ch) {
            if (isLatin1(ch))
                matches8.append(ch);
            else
                matches32.append(ch);
        };

        for (auto string : disjunctionStrings) {
            if (string.size() == 1) {
                char32_t ch = string[0];
                if (!m_isCaseInsensitive) {
                    addCh(ch);
                    continue;
                }

                // Add multiple matches, if necessary.
                const CanonicalizationRange* info = canonicalRangeInfoFor(ch, m_canonicalMode);
                if (info->type == CanonicalizeUnique)
                    addCh(ch);
                else {
                    if (info->type == CanonicalizeSet) {
                        for (auto* set = canonicalCharacterSetInfo(info->value, m_canonicalMode); (ch = *set); ++set)
                            addCh(ch);
                    } else {
                        addCh(ch);
                        addCh(getCanonicalPair(info, ch));
                    }
                }
                continue;
            }

            utf32Strings.append(string);
        }

        performSetOpWithStrings(utf32Strings);
        performSetOpWithMatches(matches8, emptyRanges, matches32, emptyRanges);
    }

    void invertMatches()
    {
        if (!m_strings.isEmpty())
            m_invertedStrings = true;

        latin1Invert();
        nonLatin1Invert();
    }

    void performSetOpWith(CharacterClassConstructor* rhs)
    {
        performSetOpWithStrings(rhs->m_strings);
        performSetOpWithMatches(rhs->m_matches8, rhs->m_ranges8, rhs->m_matches32, rhs->m_ranges32);
    }

    void performSetOpWith(const CharacterClass* rhs)
    {
        performSetOpWithStrings(rhs->m_strings);
        performSetOpWithMatches(rhs->m_matches8, rhs->m_ranges8, rhs->m_matches32, rhs->m_ranges32);
    }

    void performSetOpWithStrings(const Vector<Vector<char32_t>>& utf32Strings)
    {
        if (m_compileMode != CompileMode::UnicodeSets)
            return;

        switch (m_setOp) {
        case CharacterClassSetOp::Default:
        case CharacterClassSetOp::Union:
            unionStrings(utf32Strings);
            break;

        case CharacterClassSetOp::Intersection:
            intersectionStrings(utf32Strings);
            break;

        case CharacterClassSetOp::Subtraction:
            subtractionStrings(utf32Strings);
            break;
        }
    }

    void performSetOpWithMatches(const Vector<char32_t>& rhsMatches8, const Vector<CharacterRange>& rhsRanges8, const Vector<char32_t>& rhsMatches32, const Vector<CharacterRange>& rhsRanges32)
    {
        if (m_compileMode != CompileMode::UnicodeSets)
            return;

        latin1Op(rhsMatches8, rhsRanges8);
        // Sort the incoming non-Latin-1 matches, since Unicode case folding canonicalization may cause
        // characters to be added to rhsMatches32 out of code point order.
        Vector<char32_t> rhsSortedMatches32(rhsMatches32);
        std::ranges::sort(rhsSortedMatches32);

        nonLatin1OpSorted(rhsSortedMatches32, rhsRanges32);
    }

    bool NODELETE hasInvertedStrings()
    {
        return m_invertedStrings;
    }

    static ALWAYS_INLINE int NODELETE compareUTF32Strings(const Vector<char32_t>& a, const Vector<char32_t>& b)
    {
        // Longer strings before shorter.
        if (a.size() > b.size())
            return -1;

        if (a.size() < b.size())
            return 1;

        // Lexically sort for same length strings.
        for (unsigned i = 0; i < a.size(); ++i) {
            if (a[i] != b[i])
                return (a[i] < b[i]) ? -1 : 1;
        }

        return 0;
    }

    static void sort(Vector<Vector<char32_t>>& utf32Strings)
    {
        std::ranges::sort(utf32Strings, [](const auto& a, const auto& b) {
            return compareUTF32Strings(a, b) < 0;
        });
    }

    std::unique_ptr<CharacterClass> charClass()
    {
        coalesceTables();

        if (!m_strings.isEmpty())
            sort(m_strings);

        auto characterClass = makeUnique<CharacterClass>();

        characterClass->m_strings.swap(m_strings);
        characterClass->m_matches8.swap(m_matches8);
        characterClass->m_ranges8.swap(m_ranges8);
        characterClass->m_matches32.swap(m_matches32);
        characterClass->m_ranges32.swap(m_ranges32);
        characterClass->m_anyCharacter = anyCharacter();
        characterClass->m_characterWidths = characterWidths();

        m_anyCharacter = false;
        m_characterWidths = CharacterClassWidths::Unknown;

        return characterClass;
    }

    void NODELETE setIsCaseInsensitive(bool ignoreCase)
    {
        m_isCaseInsensitive = ignoreCase;
    }

private:
    void addSorted(char32_t ch)
    {
        addSorted(isLatin1(ch) ? m_matches8 : m_matches32, ch);
    }

    void addSorted(Vector<char32_t>& matches, char32_t ch)
    {
        unsigned pos = 0;
        unsigned range = matches.size();

        m_characterWidths |= (U_IS_BMP(ch) ? CharacterClassWidths::HasBMPChars : CharacterClassWidths::HasNonBMPChars);

        // binary chop, find position to insert char.
        while (range) {
            unsigned index = range >> 1;

            int val = matches[pos+index] - ch;
            if (!val)
                return;
            else if (val > 0) {
                if (val == 1) {
                    char32_t lo = ch;
                    char32_t hi = ch + 1;
                    matches.removeAt(pos + index);
                    if (pos + index > 0 && matches[pos + index - 1] == ch - 1) {
                        lo = ch - 1;
                        matches.removeAt(pos + index - 1);
                    }
                    addSortedRange(isLatin1(ch) ? m_ranges8 : m_ranges32, lo, hi);
                    return;
                }
                range = index;
            } else {
                if (val == -1) {
                    char32_t lo = ch - 1;
                    char32_t hi = ch;
                    matches.removeAt(pos + index);
                    if (pos + index + 1 < matches.size() && matches[pos + index + 1] == ch + 1) {
                        hi = ch + 1;
                        matches.removeAt(pos + index + 1);
                    }
                    addSortedRange(isLatin1(ch) ? m_ranges8 : m_ranges32, lo, hi);
                    return;
                }
                pos += (index+1);
                range -= (index+1);
            }
        }
        
        if (pos == matches.size())
            matches.append(ch);
        else
            matches.insert(pos, ch);
    }

    void addSortedRange(Vector<CharacterRange>& ranges, char32_t lo, char32_t hi)
    {
        size_t end = ranges.size();

        if (U_IS_BMP(lo))
            m_characterWidths |= CharacterClassWidths::HasBMPChars;
        if (!U_IS_BMP(hi))
            m_characterWidths |= CharacterClassWidths::HasNonBMPChars;

        // Simple linear scan - I doubt there are that many ranges anyway...
        // feel free to fix this with something faster (eg binary chop).
        for (size_t i = 0; i < end; ++i) {
            // does the new range fall before the current position in the array
            if (hi < ranges[i].begin) {
                // Concatenate appending ranges.
                if (hi == (ranges[i].begin - 1)) {
                    ranges[i].begin = lo;
                    return;
                }
                ranges.insert(i, CharacterRange(lo, hi));
                return;
            }
            // Okay, since we didn't hit the last case, the end of the new range is definitely at or after the begining
            // If the new range start at or before the end of the last range, then the overlap (if it starts one after the
            // end of the last range they concatenate, which is just as good.
            if (lo <= (ranges[i].end + 1)) {
                // found an intersect! we'll replace this entry in the array.
                ranges[i].begin = std::min(ranges[i].begin, lo);
                ranges[i].end = std::max(ranges[i].end, hi);

                mergeRangesFrom(ranges, i);
                return;
            }
        }

        // CharacterRange comes after all existing ranges.
        ranges.append(CharacterRange(lo, hi));
    }


    void addSortedRange(char32_t lo, char32_t hi)
    {
        if (lo == hi) {
            addSorted(lo);
            return;
        }

        if (isLatin1(lo)) {
            auto latin1Hi = std::min<char32_t>(hi, 0xff);
            if (lo == latin1Hi)
                addSorted(m_matches8, lo);
            else
                addSortedRange(m_ranges8, lo, latin1Hi);

            if (isLatin1(hi))
                return;
            lo = 0x100;
            if (lo == hi) {
                addSorted(m_matches32, hi);
                return;
            }
        }
        addSortedRange(m_ranges32, lo, hi);
    }

    void mergeRangesFrom(Vector<CharacterRange>& ranges, size_t index)
    {
        unsigned next = index + 1;

        // each iteration of the loop we will either remove something from the list, or break out of the loop.
        while (next < ranges.size()) {
            if (ranges[next].begin <= (ranges[index].end + 1)) {
                // the next entry now overlaps / concatenates with this one.
                ranges[index].end = std::max(ranges[index].end, ranges[next].end);
                ranges.removeAt(next);
            } else
                break;
        }
    }

    void unionStrings(const Vector<Vector<char32_t>>& rhsStrings)
    {
        // result should include strings in either the LHS or RHS
        Vector<Vector<char32_t>> result;
        size_t lhsIndex = 0;
        size_t rhsIndex = 0;

        while (lhsIndex < m_strings.size() && rhsIndex < rhsStrings.size()) {
            auto lhsString = m_strings[lhsIndex];
            auto rhsString = rhsStrings[rhsIndex];

            auto strCompare = compareUTF32Strings(lhsString, rhsString);
            if (strCompare <= 0) {
                result.append(lhsString);
                lhsIndex++;
                if (!strCompare)
                    rhsIndex++;
            } else {
                result.append(rhsString);
                rhsIndex++;
            }
        }

        // One of LHS or RHS has been exhausted, add the remaining strings.
        while (lhsIndex < m_strings.size())
            result.append(m_strings[lhsIndex++]);

        while (rhsIndex < rhsStrings.size())
            result.append(rhsStrings[rhsIndex++]);

        m_strings.swap(result);
        m_mayContainStrings = !m_strings.isEmpty();
    }

    void intersectionStrings(const Vector<Vector<char32_t>>& rhsStrings)
    {
        // result should include strings that are in both the LHS and RHS.
        Vector<Vector<char32_t>> result;
        size_t lhsIndex = 0;
        size_t rhsIndex = 0;

        while (lhsIndex < m_strings.size() && rhsIndex < rhsStrings.size()) {
            auto lhsString = m_strings[lhsIndex];
            auto rhsString = rhsStrings[rhsIndex];

            auto strCompare = compareUTF32Strings(lhsString, rhsString);
            if (!strCompare) {
                result.append(lhsString);
                lhsIndex++;
                rhsIndex++;
            } else if (strCompare < 0)
                lhsIndex++;
            else
                rhsIndex++;
        }

        m_strings.swap(result);
        m_mayContainStrings = !m_strings.isEmpty();
    }

    void subtractionStrings(const Vector<Vector<char32_t>>& rhsStrings)
    {
        // result should include strings in LHS that are not in RHS.
        Vector<Vector<char32_t>> result;
        size_t lhsIndex = 0;
        size_t rhsIndex = 0;

        while (lhsIndex < m_strings.size() && rhsIndex < rhsStrings.size()) {
            auto lhsString = m_strings[lhsIndex];
            auto rhsString = rhsStrings[rhsIndex];

            auto strCompare = compareUTF32Strings(lhsString, rhsString);
            if (!strCompare) {
                lhsIndex++;
                rhsIndex++;
            } else if (strCompare < 0) {
                result.append(lhsString);
                lhsIndex++;
            } else
                rhsIndex++;
        }

        // Add any remaining LHS strings.
        while (lhsIndex < m_strings.size())
            result.append(m_strings[lhsIndex++]);

        m_strings.swap(result);
        m_mayContainStrings = !m_strings.isEmpty();
    }

    void latin1Op(const Vector<char32_t>& rhsMatches, const Vector<CharacterRange>& rhsRanges)
    {
        Vector<char32_t> resultMatches;
        Vector<CharacterRange> resultRanges;
        WTF::BitSet<0x100> lhsLatin1BitSet;
        WTF::BitSet<0x100> rhsLatin1BitSet;

        for (auto match : m_matches8)
            lhsLatin1BitSet.set(match);

        for (auto range : m_ranges8) {
            for (char32_t ch = range.begin; ch <= range.end; ch++)
                lhsLatin1BitSet.set(ch);
        }

        for (auto match : rhsMatches)
            rhsLatin1BitSet.set(match);

        for (auto range : rhsRanges) {
            for (char32_t ch = range.begin; ch <= range.end; ch++)
                rhsLatin1BitSet.set(ch);
        }

        switch (m_setOp) {
        case CharacterClassSetOp::Default:
        case CharacterClassSetOp::Union:
            lhsLatin1BitSet.merge(rhsLatin1BitSet);
            break;

        case CharacterClassSetOp::Intersection:
            lhsLatin1BitSet.filter(rhsLatin1BitSet);
            break;

        case CharacterClassSetOp::Subtraction:
            lhsLatin1BitSet.exclude(rhsLatin1BitSet);
            break;
        }

        bool firstCharUnset = true;
        char32_t lo = 0;
        char32_t hi = 0;

        auto addCharToResults = [&]() {
            if (lo == hi)
                resultMatches.append(lo);
            else
                resultRanges.append(CharacterRange(lo, hi));
        };

        for (auto setVal : lhsLatin1BitSet) {
            char32_t ch = setVal;
            if (firstCharUnset) {
                lo = hi = ch;
                firstCharUnset = false;
            } else {
                if (ch == hi + 1)
                    hi = ch;
                else {
                    addCharToResults();
                    lo = hi = ch;
                }
            }
        }

        if (!firstCharUnset)
            addCharToResults();

        m_matches8.swap(resultMatches);
        m_ranges8.swap(resultRanges);
    }

    void latin1Invert()
    {
        Vector<char32_t> resultMatches;
        Vector<CharacterRange> resultRanges;
        WTF::BitSet<0x100> latin1BitSet;

        for (auto match : m_matches8)
            latin1BitSet.set(match);

        for (auto range : m_ranges8) {
            for (char32_t ch = range.begin; ch <= range.end; ch++)
                latin1BitSet.set(ch);
        }

        latin1BitSet.invert();

        bool firstCharUnset = true;
        char32_t lo = 0;
        char32_t hi = 0;

        auto addCharToResults = [&]() {
            if (lo == hi)
                resultMatches.append(lo);
            else
                resultRanges.append(CharacterRange(lo, hi));
        };

        for (auto setVal : latin1BitSet) {
            char32_t ch = setVal;
            if (firstCharUnset) {
                lo = hi = ch;
                firstCharUnset = false;
            } else {
                if (ch == hi + 1)
                    hi = ch;
                else {
                    addCharToResults();
                    lo = hi = ch;
                }
            }
        }

        if (!firstCharUnset)
            addCharToResults();

        m_matches8.swap(resultMatches);
        m_ranges8.swap(resultRanges);
    }

    void nonLatin1OpSorted(const Vector<char32_t>& rhsMatches32, const Vector<CharacterRange>& rhsRanges32)
    {
        Vector<char32_t> resultMatches;
        Vector<CharacterRange> resultRanges;

        constexpr size_t chunkSize = 2048;
        WTF::BitSet<chunkSize> lhsChunkBitSet;
        WTF::BitSet<chunkSize> rhsChunkBitSet;

        char32_t chunkLo = INT_MAX, chunkHi;

        size_t lhsMatchIndex = 0;
        size_t lhsRangeIndex = 0;
        size_t rhsMatchIndex = 0;
        size_t rhsRangeIndex = 0;

        if (!m_matches32.isEmpty())
            chunkLo = std::min(chunkLo, m_matches32[0]);

        if (!m_ranges32.isEmpty())
            chunkLo = std::min(chunkLo, m_ranges32[0].begin);

        if (!rhsMatches32.isEmpty())
            chunkLo = std::min(chunkLo, rhsMatches32[0]);

        if (!rhsRanges32.isEmpty())
            chunkLo = std::min(chunkLo, rhsRanges32[0].begin);

        // If both the LHS and RHS are empty, bail out.
        if (chunkLo == INT_MAX)
            return;

        while (lhsMatchIndex < m_matches32.size() || lhsRangeIndex < m_ranges32.size() || rhsMatchIndex < rhsMatches32.size() || rhsRangeIndex < rhsRanges32.size()) {
            if (rhsMatchIndex >= rhsMatches32.size() && rhsRangeIndex > rhsRanges32.size() && m_setOp == CharacterClassSetOp::Intersection) {
                // RHS is exhausted, we can short cut from here. Can't intersect anything more so bail out.
                break;
            }

            chunkHi = chunkLo + chunkSize - 1;

            for (; lhsMatchIndex < m_matches32.size(); ++lhsMatchIndex) {
                char32_t ch = m_matches32[lhsMatchIndex];
                if (ch > chunkHi)
                    break;

                ASSERT(ch >= chunkLo);
                lhsChunkBitSet.set(ch - chunkLo);
            }

            for (; lhsRangeIndex < m_ranges32.size(); ++lhsRangeIndex) {
                auto range = m_ranges32[lhsRangeIndex];
                if (range.begin > chunkHi)
                    break;

                auto begin = std::max(chunkLo, range.begin);
                auto end = std::min(range.end, chunkHi);

                for (char32_t ch = begin; ch <= end; ch++) {
                    ASSERT(ch >= chunkLo);
                    lhsChunkBitSet.set(ch - chunkLo);
                }

                if (range.end > chunkHi)
                    break;
            }

            for (; rhsMatchIndex < rhsMatches32.size(); ++rhsMatchIndex) {
                char32_t ch = rhsMatches32[rhsMatchIndex];
                if (ch > chunkHi)
                    break;

                ASSERT(ch >= chunkLo);
                rhsChunkBitSet.set(ch - chunkLo);
            }

            for (; rhsRangeIndex < rhsRanges32.size(); ++rhsRangeIndex) {
                auto range = rhsRanges32[rhsRangeIndex];
                if (range.begin > chunkHi)
                    break;

                auto begin = std::max(chunkLo, range.begin);
                auto end = std::min(range.end, chunkHi);

                for (char32_t ch = begin; ch <= end; ch++) {
                    ASSERT(ch >= chunkLo);
                    rhsChunkBitSet.set(ch - chunkLo);
                }

                if (range.end > chunkHi)
                    break;
            }

            switch (m_setOp) {
            case CharacterClassSetOp::Default:
            case CharacterClassSetOp::Union:
                lhsChunkBitSet.merge(rhsChunkBitSet);
                break;

            case CharacterClassSetOp::Intersection:
                lhsChunkBitSet.filter(rhsChunkBitSet);
                break;

            case CharacterClassSetOp::Subtraction:
                lhsChunkBitSet.exclude(rhsChunkBitSet);
                break;
            }

            bool firstCharUnset = true;
            char32_t lo = 0;
            char32_t hi = 0;

            auto addCharToResults = [&]() {
                if (lo == hi)
                    resultMatches.append(lo);
                else {
                    // Coalesce the prior range with the new (lo, hi) range if they are adjacent.
                    if (resultRanges.size() > 0) {
                        auto lastIndex = resultRanges.size() - 1;
                        if (resultRanges[lastIndex].end + 1 == lo) {
                            resultRanges[lastIndex].end = hi;
                            return;
                        }
                    }

                    resultRanges.append(CharacterRange(lo, hi));
                }
            };

            for (auto setVal : lhsChunkBitSet) {
                char32_t ch = static_cast<char32_t>(setVal) + chunkLo;
                if (firstCharUnset) {
                    lo = hi = ch;
                    firstCharUnset = false;
                } else {
                    if (ch == hi + 1)
                        hi = ch;
                    else {
                        addCharToResults();
                        lo = hi = ch;
                    }
                }
            }

            if (!firstCharUnset)
                addCharToResults();

            chunkLo = chunkHi + 1;
            lhsChunkBitSet.clearAll();
            rhsChunkBitSet.clearAll();
        }

        m_matches32.swap(resultMatches);
        m_ranges32.swap(resultRanges);
    }

    void nonLatin1Invert()
    {
        auto currentSetOp = m_setOp;
        m_setOp = CharacterClassSetOp::Subtraction;

        Vector<char32_t> matches { };
        Vector<CharacterRange> ranges {
            CharacterRange(0x0100, UCHAR_MAX_VALUE)
        };

        std::swap(m_matches32, matches);
        std::swap(m_ranges32, ranges);

        nonLatin1OpSorted(matches, ranges);

        m_setOp = currentSetOp;
    }

    void coalesceTables()
    {
        auto coalesceMatchesAndRanges = [&](Vector<char32_t>& matches, Vector<CharacterRange>& ranges) {

            size_t matchesIndex = 0;
            size_t rangesIndex = 0;

            while (matchesIndex < matches.size() && rangesIndex < ranges.size()) {
                if (ranges[rangesIndex].begin) {
                    while (matchesIndex < matches.size() && matches[matchesIndex] < ranges[rangesIndex].begin - 1)
                        matchesIndex++;

                    if (matchesIndex < matches.size() && matches[matchesIndex] == ranges[rangesIndex].begin - 1) {
                        ranges[rangesIndex].begin = matches[matchesIndex];
                        matches.removeAt(matchesIndex);
                    }
                }

                while (matchesIndex < matches.size() && matches[matchesIndex] < ranges[rangesIndex].end + 1)
                    matchesIndex++;

                if (matchesIndex < matches.size()) {
                    if (matches[matchesIndex] > ranges[rangesIndex].end + 1) {
                        rangesIndex++;
                        continue;
                    }

                    if (matches[matchesIndex] == ranges[rangesIndex].end + 1) {
                        ranges[rangesIndex].end = matches[matchesIndex];
                        matches.removeAt(matchesIndex);

                        mergeRangesFrom(ranges, rangesIndex);
                    } else
                        matchesIndex++;
                }
            }

            if (ranges.size() > 1) {
                for (auto rangesIndex = ranges.size() - 1; rangesIndex > 0; rangesIndex--) {
                    if (ranges[rangesIndex].begin == ranges[rangesIndex - 1].end + 1) {
                        ranges[rangesIndex - 1].end = ranges[rangesIndex].end;
                        ranges.removeAt(rangesIndex);
                    }
                }
            }
        };

        coalesceMatchesAndRanges(m_matches8, m_ranges8);
        coalesceMatchesAndRanges(m_matches32, m_ranges32);

        if (!m_matches8.size() && !m_matches32.size()
            && m_ranges8.size() == 1 && m_ranges32.size() == 1
            && m_ranges8[0].begin == 0 && m_ranges8[0].end == 0xff
            && m_ranges32[0].begin == 0x100 && m_ranges32[0].end == UCHAR_MAX_VALUE)
            m_anyCharacter = true;
    }

    bool hasNonBMPCharacters()
    {
        return m_characterWidths & CharacterClassWidths::HasNonBMPChars;
    }

    CharacterClassWidths NODELETE characterWidths()
    {
        return m_characterWidths;
    }

    bool NODELETE anyCharacter()
    {
        return m_anyCharacter;
    }

    bool NODELETE isUnionSetOp() { return m_setOp == CharacterClassSetOp::Default || m_setOp == CharacterClassSetOp::Union; }

    bool m_isCaseInsensitive : 1;
    bool m_anyCharacter : 1;
    bool m_mayContainStrings : 1;
    bool m_invertedStrings : 1;

    CharacterClassSetOp m_setOp { CharacterClassSetOp::Default };
    CompileMode m_compileMode;
    CharacterClassWidths m_characterWidths;
    
    CanonicalMode m_canonicalMode;

    Vector<Vector<char32_t>> m_strings;
    Vector<char32_t> m_matches8;
    Vector<CharacterRange> m_ranges8;
    Vector<char32_t> m_matches32;
    Vector<CharacterRange> m_ranges32;
};

class YarrPatternConstructor {
    class UnresolvedForwardReference {
    public:
        UnresolvedForwardReference(PatternAlternative* alternative, unsigned termIndex)
            : m_alternative(alternative)
            , m_termIndex(termIndex)
            , m_namedGroup(String())
        {
        }

        UnresolvedForwardReference(PatternAlternative* alternative, unsigned termIndex, const String namedGroup)
            : m_alternative(alternative)
            , m_termIndex(termIndex)
            , m_namedGroup(namedGroup)
        {
        }

        PatternTerm* term()
        {
            return &m_alternative->m_terms[m_termIndex];
        }

        bool NODELETE hasNamedGroup()
        {
            return !m_namedGroup.isNull();
        }

        const String NODELETE namedGroup()
        {
            return m_namedGroup;
        }

    private:
        PatternAlternative* m_alternative;
        unsigned m_termIndex;
        const String m_namedGroup;
    };

public:
    YarrPatternConstructor(YarrPattern& pattern, OptionSet<Flags> flags)
        : m_pattern(pattern)
        , m_baseCharacterClassConstructor(pattern.ignoreCase(), pattern.compileMode())
        , m_initialFlags(flags)
    {
        m_currentCharacterClassConstructor = &m_baseCharacterClassConstructor;
        auto body = makeUnique<PatternDisjunction>();
        m_pattern.m_body = body.get();
        m_alternative = body->addNewAlternative();
        m_pattern.m_disjunctions.append(WTF::move(body));

        m_flags = m_initialFlags;
        m_parenthesisContext.setFlags(m_initialFlags);
    }

    ~YarrPatternConstructor()
    {
    }

    void resetForReparsing()
    {
        m_pattern.resetForReparsing();
        m_baseCharacterClassConstructor.reset();
        m_currentCharacterClassConstructor = &m_baseCharacterClassConstructor;
        m_error = ErrorCode::NoError;
        m_parenthesisContext.reset();
        m_parenthesisContext.setFlags(m_flags);
        m_forwardReferencesInLookbehind.clear();

        auto body = makeUnique<PatternDisjunction>();
        m_pattern.m_body = body.get();
        m_alternative = body->addNewAlternative();
        m_pattern.m_disjunctions.append(WTF::move(body));

        m_flags = m_initialFlags;
    }

    void addCaptureGroupForName(const String groupName, unsigned subpatternId)
    {
        ASSERT(subpatternId);

        m_pattern.m_hasNamedCaptureGroups = true;

        auto addResult = m_pattern.m_namedGroupToParenIndices.add(groupName, Vector<unsigned>());
        auto& thisGroupNameSubpatternIds = addResult.iterator->value;
        if (addResult.isNewEntry) {
            while (m_pattern.m_captureGroupNames.size() < subpatternId)
                m_pattern.m_captureGroupNames.append(String());
            m_pattern.m_captureGroupNames.append(groupName);

            thisGroupNameSubpatternIds.append(subpatternId);
        } else if (thisGroupNameSubpatternIds.size() == 2) {
            // This named group is now a duplicate.
            thisGroupNameSubpatternIds[0] = ++m_pattern.m_numDuplicateNamedCaptureGroups;
        }

        thisGroupNameSubpatternIds.append(subpatternId);
    }

    void tryConvertingForwardReferencesToBackreferences()
    {
        //  There are forward references that could actually be lookbehind back references.
        for (unsigned i = 0; i < m_forwardReferencesInLookbehind.size(); ++i) {
            UnresolvedForwardReference& unresolvedForwardReference = m_forwardReferencesInLookbehind[i];
            auto term = unresolvedForwardReference.term();
            if (unresolvedForwardReference.hasNamedGroup()) {
                auto namedGroupIndicesIter = m_pattern.m_namedGroupToParenIndices.find(unresolvedForwardReference.namedGroup());
                if (namedGroupIndicesIter == m_pattern.m_namedGroupToParenIndices.end())
                    continue;

                unsigned namedGroupSubpatternId = namedGroupIndicesIter->value.last();
                if (namedGroupSubpatternId == term->backReferenceSubpatternId) {
                    term->backReferenceSubpatternId = 0;
                    continue;
                }
                term->backReferenceSubpatternId = namedGroupSubpatternId;
                term->convertToNamedBackreference();
                m_pattern.m_containsBackreferences = true;
            } else if (term->backReferenceSubpatternId && term->backReferenceSubpatternId <= m_pattern.m_numSubpatterns) {
                term->convertToNumberedBackreference();
                m_pattern.m_containsBackreferences = true;
            }
        }

        m_forwardReferencesInLookbehind.clear();
    }

    void assertionBOL()
    {
        if (!m_alternative->m_terms.size() && !parenthesisInvert() && parenthesisMatchDirection() == Forward) {
            m_alternative->m_startsWithBOL = true;
            m_alternative->m_containsBOL = true;
            m_pattern.m_containsBOL = true;
        }

        auto bolTerm = PatternTerm::BOL(m_flags);
        bolTerm.setMatchDirection(parenthesisMatchDirection());
        m_alternative->m_terms.append(bolTerm);
    }
    void assertionEOL()
    {
        m_alternative->m_terms.append(PatternTerm::EOL(m_flags));
    }
    void assertionWordBoundary(bool invert)
    {
        m_alternative->m_terms.append(PatternTerm::WordBoundary(invert, m_flags));
    }

    void atomPatternCharacter(char32_t ch, bool)
    {
        // We handle case-insensitive checking of unicode characters which do have both
        // cases by handling them as if they were defined using a CharacterClass.
        if (!ignoreCase() || (isASCII(ch) && !m_pattern.eitherUnicode())) {
            m_alternative->m_terms.append(PatternTerm(ch, m_flags, parenthesisMatchDirection()));
            return;
        }

        const CanonicalizationRange* info = canonicalRangeInfoFor(ch, m_pattern.eitherUnicode() ? CanonicalMode::Unicode : CanonicalMode::UCS2);
        if (info->type == CanonicalizeUnique) {
            m_alternative->m_terms.append(PatternTerm(ch, m_flags, parenthesisMatchDirection()));
            return;
        }

        m_currentCharacterClassConstructor->putUnicodeIgnoreCase(ch, info);
        auto newCharacterClass = m_currentCharacterClassConstructor->charClass();
        m_alternative->m_terms.append(PatternTerm(newCharacterClass.get(), false, m_flags, parenthesisMatchDirection()));
        m_pattern.m_userCharacterClasses.append(WTF::move(newCharacterClass));
    }

    void atomBuiltInCharacterClass(BuiltInCharacterClassID classID, bool invert)
    {
        switch (classID) {
        case BuiltInCharacterClassID::DigitClassID:
            m_alternative->m_terms.append(PatternTerm(m_pattern.digitsCharacterClass(), invert, m_flags, parenthesisMatchDirection()));
            break;
        case BuiltInCharacterClassID::SpaceClassID:
            m_alternative->m_terms.append(PatternTerm(m_pattern.spacesCharacterClass(), invert, m_flags, parenthesisMatchDirection()));
            break;
        case BuiltInCharacterClassID::WordClassID:
            if (m_pattern.eitherUnicode() && ignoreCase())
                m_alternative->m_terms.append(PatternTerm(m_pattern.wordUnicodeIgnoreCaseCharCharacterClass(), invert, m_flags, parenthesisMatchDirection()));
            else
                m_alternative->m_terms.append(PatternTerm(m_pattern.wordcharCharacterClass(), invert, m_flags, parenthesisMatchDirection()));
            break;
        case BuiltInCharacterClassID::DotClassID:
            ASSERT(!invert);
            if (dotAll())
                m_alternative->m_terms.append(PatternTerm(m_pattern.anyCharacterClass(), false, m_flags, parenthesisMatchDirection()));
            else
                m_alternative->m_terms.append(PatternTerm(m_pattern.newlineCharacterClass(), true, m_flags, parenthesisMatchDirection()));
            break;
        default: {
            if (characterClassMayContainStrings(classID)) {
                auto characterClass = m_pattern.unicodeCharacterClassFor(classID);
                if (characterClass->hasStrings()) {
                    atomParenthesesSubpatternBegin(false);
                    unsigned alternativeCount = 0;
                    for (unsigned i = 0; i < characterClass->m_strings.size(); ++i) {
                        if (alternativeCount)
                            disjunction(CreateDisjunctionPurpose::ForNextAlternative);

                        auto string = characterClass->m_strings[i];

                        for (auto ch : string)
                            atomPatternCharacter(ch, /* hyphenIsRange */ false);

                        ++alternativeCount;
                    }

                    if (characterClass->hasSingleCharacters()) {
                        if (alternativeCount)
                            disjunction(CreateDisjunctionPurpose::ForNextAlternative);

                        m_alternative->m_terms.append(PatternTerm(characterClass, invert, m_flags, parenthesisMatchDirection()));
                    }

                    atomParenthesesEnd();
                    break;
                }
                // Fall through for the case where the characterClass REALLY doesn't have strings.
            }

            m_alternative->m_terms.append(PatternTerm(m_pattern.unicodeCharacterClassFor(classID), invert, m_flags, parenthesisMatchDirection()));
            break;
        }
        }
    }

    void NODELETE atomCharacterClassBegin(bool invert = false)
    {
        m_invertCharacterClass = invert;

        // We may have modifiers, so set case sensitivity on the fly
        m_currentCharacterClassConstructor->setIsCaseInsensitive(ignoreCase());
    }

    void atomCharacterClassAtom(char32_t ch)
    {
        m_currentCharacterClassConstructor->putChar(ch);
    }

    void atomCharacterClassRange(char32_t begin, char32_t end)
    {
        m_currentCharacterClassConstructor->putRange(begin, end);
    }

    void atomCharacterClassBuiltIn(BuiltInCharacterClassID classID, bool invert)
    {
        ASSERT(classID != BuiltInCharacterClassID::DotClassID);

        switch (classID) {
        case BuiltInCharacterClassID::DigitClassID:
            m_currentCharacterClassConstructor->append(invert ? m_pattern.nondigitsCharacterClass() : m_pattern.digitsCharacterClass());
            break;
        
        case BuiltInCharacterClassID::SpaceClassID:
            m_currentCharacterClassConstructor->append(invert ? m_pattern.nonspacesCharacterClass() : m_pattern.spacesCharacterClass());
            break;
        
        case BuiltInCharacterClassID::WordClassID:
            if (m_pattern.eitherUnicode() && ignoreCase())
                m_currentCharacterClassConstructor->append(invert ? m_pattern.nonwordUnicodeIgnoreCaseCharCharacterClass() : m_pattern.wordUnicodeIgnoreCaseCharCharacterClass());
            else
                m_currentCharacterClassConstructor->append(invert ? m_pattern.nonwordcharCharacterClass() : m_pattern.wordcharCharacterClass());
            break;
        
        default:
            if (!invert)
                m_currentCharacterClassConstructor->append(m_pattern.unicodeCharacterClassFor(classID));
            else
                m_currentCharacterClassConstructor->appendInverted(m_pattern.unicodeCharacterClassFor(classID));
        }
    }

    void atomClassStringDisjunction(Vector<Vector<char32_t>>& utf32Strings)
    {
        m_currentCharacterClassConstructor->atomClassStringDisjunction(utf32Strings);
    }

    void NODELETE atomCharacterClassSetOp(CharacterClassSetOp setOp)
    {
        m_currentCharacterClassConstructor->combiningSetOp(setOp);
    }

    void atomCharacterClassPushNested(bool invert)
    {
        m_characterClassStack.append(CharacterClassConstructor(ignoreCase(), m_pattern.compileMode()));
        m_currentCharacterClassConstructor = &m_characterClassStack.last();
        m_invertCharacterClass = invert;
    }

    void atomCharacterClassPopNested(bool invert)
    {
        if (m_characterClassStack.isEmpty())
            return;

        if (m_invertCharacterClass)
            m_currentCharacterClassConstructor->invertMatches();

        CharacterClassConstructor* priorCharacterClassConstructor = m_characterClassStack.size() == 1 ? &m_baseCharacterClassConstructor : &m_characterClassStack[m_characterClassStack.size() - 2];
        priorCharacterClassConstructor->performSetOpWith(m_currentCharacterClassConstructor);
        m_characterClassStack.removeLast();
        m_currentCharacterClassConstructor = priorCharacterClassConstructor;
        m_invertCharacterClass = invert;
    }

    void atomCharacterClassEnd()
    {
        if (m_currentCharacterClassConstructor->hasInvertedStrings()) {
            m_error = ErrorCode::NegatedClassSetMayContainStrings;
            return;
        }

        auto newCharacterClass = m_currentCharacterClassConstructor->charClass();
        m_currentCharacterClassConstructor->reset();
        auto hasStrings = newCharacterClass->hasStrings();

        auto addCharacterClassTerm = [&] () {
            if (!m_invertCharacterClass && newCharacterClass.get()->m_anyCharacter) {
                m_alternative->m_terms.append(PatternTerm(m_pattern.anyCharacterClass(), false, m_flags));
                return;
            }

            m_alternative->m_terms.append(PatternTerm(newCharacterClass.get(), m_invertCharacterClass, m_flags));
        };

        if (!hasStrings)
            addCharacterClassTerm();
        else {
            if (m_invertCharacterClass) {
                m_error = ErrorCode::NegatedClassSetMayContainStrings;
                return;
            }

            atomParenthesesSubpatternBegin(false);
            unsigned alternativeCount = 0;
            for (unsigned i = 0; i < newCharacterClass->m_strings.size(); ++i) {
                if (alternativeCount)
                    disjunction(CreateDisjunctionPurpose::ForNextAlternative);

                auto string = newCharacterClass->m_strings[i];

                for (auto ch : string)
                    atomPatternCharacter(ch, /* hyphenIsRange */ false);

                ++alternativeCount;
            }

            if (newCharacterClass->hasSingleCharacters()) {
                if (alternativeCount)
                    disjunction(CreateDisjunctionPurpose::ForNextAlternative);

                addCharacterClassTerm();
            }

            atomParenthesesEnd();
        }

        m_pattern.m_userCharacterClasses.append(WTF::move(newCharacterClass));
    }

    void atomParenthesesSubpatternBegin(bool capture, std::optional<String> optGroupName = std::nullopt)
    {
        unsigned subpatternId = m_pattern.m_numSubpatterns + 1;
        if (capture) {
            m_pattern.m_numSubpatterns++;
            if (optGroupName) {
                addCaptureGroupForName(optGroupName.value(), subpatternId);
            }
        } else
            ASSERT(!optGroupName);

        auto parenthesesDisjunction = makeUnique<PatternDisjunction>(m_alternative);
        m_alternative->m_terms.append(PatternTerm(PatternTerm::Type::ParenthesesSubpattern, subpatternId, parenthesesDisjunction.get(), m_flags, capture, false, parenthesisMatchDirection()));
        m_alternative = parenthesesDisjunction->addNewAlternative(m_pattern.m_numSubpatterns, parenthesisMatchDirection());
        pushParenthesisContext();
        m_pattern.m_disjunctions.append(WTF::move(parenthesesDisjunction));
    }

    void atomParentheticalAssertionBegin(bool invert, MatchDirection matchDirection)
    {
        auto parenthesesDisjunction = makeUnique<PatternDisjunction>(m_alternative);
        m_alternative->m_terms.append(PatternTerm(PatternTerm::Type::ParentheticalAssertion, m_pattern.m_numSubpatterns + 1, parenthesesDisjunction.get(), m_flags, false, invert, matchDirection));
        m_alternative = parenthesesDisjunction->addNewAlternative(m_pattern.m_numSubpatterns, matchDirection);
        pushParenthesisContext();
        setParenthesisInvert(invert);
        setParenthesisMatchDirection(matchDirection);
        if (matchDirection == Backward)
            m_pattern.m_containsLookbehinds = true;
        m_pattern.m_disjunctions.append(WTF::move(parenthesesDisjunction));
    }

    void atomParentheticalModifierBegin(OptionSet<Flags> set, OptionSet<Flags> unset)
    {
        auto parenthesesDisjunction = makeUnique<PatternDisjunction>(m_alternative);
        m_alternative->m_terms.append(PatternTerm(PatternTerm::Type::ParenthesesSubpattern, m_pattern.m_numSubpatterns + 1, parenthesesDisjunction.get(), m_flags, false, false, parenthesisMatchDirection()));
        m_alternative = parenthesesDisjunction->addNewAlternative(m_pattern.m_numSubpatterns, parenthesisMatchDirection());
        pushParenthesisContext();
        m_pattern.m_disjunctions.append(WTF::move(parenthesesDisjunction));

        // Mark this context as a modifier, so we restore the flags afterwards
        m_parenthesisContext.setModifier(true);
        // Keep the old flags here, so when we come back up we can get it
        m_parenthesisContext.setFlags(m_flags);
        m_flags.add(set);
        m_flags.remove(unset);
        m_pattern.m_containsModifiers = true;
    }

    void atomParenthesesEnd()
    {
        ASSERT(m_alternative->m_parent);
        ASSERT(m_alternative->m_parent->m_parent);

        PatternDisjunction* parenthesesDisjunction = m_alternative->m_parent;
        m_alternative = m_alternative->m_parent->m_parent;

        PatternTerm& lastTerm = m_alternative->lastTerm();

        unsigned numBOLAnchoredAlts = 0;
        unsigned numParenAlternatives = parenthesesDisjunction->m_alternatives.size();
        ASSERT(numParenAlternatives);

        for (unsigned i = 0; i < numParenAlternatives; i++) {
            // Bubble up BOL flags
            if (parenthesesDisjunction->m_alternatives[i]->m_startsWithBOL)
                numBOLAnchoredAlts++;
        }

        parenthesesDisjunction->m_alternatives.last()->m_isLastAlternative = true;

        if (numBOLAnchoredAlts) {
            m_alternative->m_containsBOL = true;
            // If all the alternatives in parens start with BOL, then so does this one
            if (numBOLAnchoredAlts == numParenAlternatives)
                m_alternative->m_startsWithBOL = true;
        }

        lastTerm.parentheses.lastSubpatternId = m_pattern.m_numSubpatterns;

        bool shouldTryConvertingForwardReferencesToBackreferences =
            lastTerm.type == PatternTerm::Type::ParentheticalAssertion
            && !m_forwardReferencesInLookbehind.isEmpty()
            && parenthesisMatchDirection() == Backward;

        if (m_parenthesisContext.isModifier())
            m_flags = m_parenthesisContext.flags();

        popParenthesisContext();

        if (shouldTryConvertingForwardReferencesToBackreferences && parenthesisMatchDirection() == Forward)
            tryConvertingForwardReferencesToBackreferences();
    }

    void atomBackReference(unsigned subpatternId)
    {
        ASSERT(subpatternId);
        if (subpatternId > m_pattern.m_numSubpatterns) {
            m_alternative->m_terms.append(PatternTerm::NumberedForwardReference(m_flags));
            if (parenthesisMatchDirection() == Backward) {
                // When matching backwards, this forward reference could actually be
                // a backreference for a captured paren in the lookbehind yet to be parsed.
                PatternTerm& term = m_alternative->lastTerm();
                term.backReferenceSubpatternId = subpatternId;
                term.m_matchDirection = parenthesisMatchDirection();
                m_forwardReferencesInLookbehind.append(UnresolvedForwardReference(m_alternative, m_alternative->lastTermIndex()));
            }
            return;
        }

        PatternAlternative* currentAlternative = m_alternative;
        ASSERT(currentAlternative);

        // Note to self: if we waited until the AST was baked, we could also remove forwards refs 
        while ((currentAlternative = currentAlternative->m_parent->m_parent)) {
            PatternTerm& term = currentAlternative->lastTerm();
            ASSERT((term.type == PatternTerm::Type::ParenthesesSubpattern) || (term.type == PatternTerm::Type::ParentheticalAssertion));

            if ((term.type == PatternTerm::Type::ParenthesesSubpattern) && term.capture() && (subpatternId == term.parentheses.subpatternId)) {
                m_alternative->m_terms.append(PatternTerm::NumberedForwardReference(m_flags));
                return;
            }
        }

        m_alternative->m_terms.append(PatternTerm(subpatternId, m_flags));
        m_pattern.m_containsBackreferences = true;
    }

    void atomNamedBackReference(const String& subpatternName)
    {
        ASSERT(m_pattern.m_namedGroupToParenIndices.find(subpatternName) != m_pattern.m_namedGroupToParenIndices.end());
        auto parenIndices = m_pattern.m_namedGroupToParenIndices.get(subpatternName);

        if (parenIndices.size() == 2) {
            // If this isn't a duplicate group, we need to go through the same analysis as a non-named backreferece to determine if
            // this backreference appears in the capture itself. A duplicate could be satisfied by a prior capture and therefore doesn't
            // need this analysis.
            unsigned subpatternId = parenIndices.last();

            PatternAlternative* currentAlternative = m_alternative;
            ASSERT(currentAlternative);

            while ((currentAlternative = currentAlternative->m_parent->m_parent)) {
                PatternTerm& term = currentAlternative->lastTerm();
                ASSERT((term.type == PatternTerm::Type::ParenthesesSubpattern) || (term.type == PatternTerm::Type::ParentheticalAssertion));

                if ((term.type == PatternTerm::Type::ParenthesesSubpattern) && term.capture() && (subpatternId == term.parentheses.subpatternId)) {
                    m_alternative->m_terms.append(PatternTerm::NamedForwardReference(m_flags));
                    return;
                }
            }
        }

        if (parenthesisMatchDirection() == Forward) {
            m_alternative->m_terms.append(PatternTerm::NamedBackReference(parenIndices.last(), m_flags));
            PatternTerm& lastTerm = m_alternative->lastTerm();
            lastTerm.m_matchDirection = parenthesisMatchDirection();
            m_pattern.m_containsBackreferences = true;
            return;
        }

        // When part of a lookbehind, it could be the case that a prior alternative has a duplicate
        // named capture. Therefore we create a ForwardReference that will be converted to a
        // Backreference when the lookbehind or alternative is closed.
        m_alternative->m_terms.append(PatternTerm::NamedForwardReference(m_flags));
        PatternTerm& term = m_alternative->lastTerm();
        term.m_matchDirection = parenthesisMatchDirection();
        // We record the current subpatternId, which we use when we try to convert to a back reference.
        // To convert this forward reference to a back reference, the patternId for the named groups must be greater than the
        // subpatternId we save here. We'll change it then.
        term.backReferenceSubpatternId = m_pattern.m_numSubpatterns;
        m_forwardReferencesInLookbehind.append(UnresolvedForwardReference(m_alternative, m_alternative->lastTermIndex(), subpatternName));
    }

    void atomNamedForwardReference(const String& subpatternName)
    {
        m_alternative->m_terms.append(PatternTerm::NamedForwardReference(m_flags));

        if (parenthesisMatchDirection() == Backward) {
            PatternTerm& term = m_alternative->lastTerm();
            term.m_matchDirection = parenthesisMatchDirection();
            m_forwardReferencesInLookbehind.append(UnresolvedForwardReference(m_alternative, m_alternative->lastTermIndex(), subpatternName));
        }
    }
    
    // deep copy the argument disjunction.  If filterStartsWithBOL is true,
    // skip alternatives with m_startsWithBOL set true.
    PatternDisjunction* copyDisjunction(PatternDisjunction* disjunction, bool filterStartsWithBOL)
    {
        if (!isSafeToRecurse()) [[unlikely]] {
            m_error = ErrorCode::PatternTooLarge;
            return nullptr;
        }

        std::unique_ptr<PatternDisjunction> newDisjunction;
        for (unsigned alt = 0; alt < disjunction->m_alternatives.size(); ++alt) {
            PatternAlternative* alternative = disjunction->m_alternatives[alt].get();
            if (!filterStartsWithBOL || !alternative->m_startsWithBOL || alternative->m_direction == Backward) {
                if (!newDisjunction) {
                    newDisjunction = makeUnique<PatternDisjunction>();
                    newDisjunction->m_parent = disjunction->m_parent;
                }
                PatternAlternative* newAlternative = newDisjunction->addNewAlternative(alternative->m_firstSubpatternId, alternative->matchDirection());
                newAlternative->m_lastSubpatternId = alternative->m_lastSubpatternId;
                newAlternative->m_terms.reserveCapacity(alternative->m_terms.size());
                for (auto& term : alternative->m_terms) {
                    if (auto copied = copyTerm(term, filterStartsWithBOL))
                        newAlternative->m_terms.append(WTF::move(*copied));
                }
            }
        }
        
        if (hasError(error())) {
            newDisjunction = nullptr;
            return nullptr;
        }

        if (!newDisjunction)
            return nullptr;

        PatternDisjunction* copiedDisjunction = newDisjunction.get();
        m_pattern.m_disjunctions.append(WTF::move(newDisjunction));
        return copiedDisjunction;
    }
    
    std::optional<PatternTerm> copyTerm(PatternTerm& term, bool filterStartsWithBOL)
    {
        if (!isSafeToRecurse()) [[unlikely]] {
            m_error = ErrorCode::PatternTooLarge;
            return PatternTerm(term);
        }

        if ((term.type != PatternTerm::Type::ParenthesesSubpattern) && (term.type != PatternTerm::Type::ParentheticalAssertion))
            return PatternTerm(term);
        
        if (auto* newDisjunction = copyDisjunction(term.parentheses.disjunction, filterStartsWithBOL)) {
            PatternTerm termCopy = term;
            termCopy.parentheses.disjunction = newDisjunction;
            m_pattern.m_hasCopiedParenSubexpressions = true;
            return termCopy;
        }
        return std::nullopt;
    }
    
    void quantifyAtom(unsigned min, unsigned max, bool greedy)
    {
        ASSERT(min <= max);
        ASSERT(m_alternative->m_terms.size());

        if (!max) {
            // In a case of backwards parentheses matching, we may have a forward reference that has
            // been quantified with {0}, meaning that we can elide it. We should check if we added an
            // UnresolvedForwardReference object for this term, and if so, pop it.
            if (parenthesisMatchDirection() == Backward && m_forwardReferencesInLookbehind.size()) {
                UnresolvedForwardReference& mostRecentForwardReference = m_forwardReferencesInLookbehind.last();
                if (mostRecentForwardReference.term() == &m_alternative->lastTerm())
                    m_forwardReferencesInLookbehind.removeLast();
            }
            m_alternative->removeLastTerm();
            return;
        }

        PatternTerm& term = m_alternative->lastTerm();
        ASSERT(term.type > PatternTerm::Type::AssertionWordBoundary);
        ASSERT(term.quantityMinCount == 1 && term.quantityMaxCount == 1 && term.quantityType == QuantifierType::FixedCount);

        if (term.type == PatternTerm::Type::ParentheticalAssertion) {
            // If an assertion is quantified with a minimum count of zero, it can simply be removed.
            // This arises from the RepeatMatcher behaviour in the spec. Matching an assertion never
            // results in any input being consumed, however the continuation passed to the assertion
            // (called in steps, 8c and 9 of the RepeatMatcher definition, ES5.1 15.10.2.5) will
            // reject all zero length matches (see step 2.1). A match from the continuation of the
            // expression will still be accepted regardless (via steps 8a and 11) - the upshot of all
            // this is that matches from the assertion are not required, and won't be accepted anyway,
            // so no need to ever run it.
            if (!min)
                m_alternative->removeLastTerm();
            // We never need to run an assertion more than once. Subsequent interations will be run
            // with the same start index (since assertions are non-capturing) and the same captures
            // (per step 4 of RepeatMatcher in ES5.1 15.10.2.5), and as such will always produce the
            // same result and captures. If the first match succeeds then the subsequent (min - 1)
            // matches will too. Any additional optional matches will fail (on the same basis as the
            // minimum zero quantified assertions, above), but this will still result in a match.
            return;
        }

        if (min == max)
            term.quantify(min, max, QuantifierType::FixedCount);
        else if (!min
            || (term.type == PatternTerm::Type::ParenthesesSubpattern
                && (m_pattern.m_hasCopiedParenSubexpressions || term.matchDirection() == Forward))) {
            // Forward-direction parenthesized subpatterns with non-zero minimum are kept
            // as a single PatternTerm with quantityMinCount > 0; YarrJIT compiles these
            // natively (see opCompileParenthesesSubpattern) without splitting into
            // FixedCount{min} + Greedy/NonGreedy{0,max-min}. The split would deep-copy
            // the disjunction subtree, which can hit OffsetTooLarge / pattern-size limits
            // for very large bounds (e.g. (?:x){2147483648,...}). Backward parens
            // (lookbehinds) still use the expansion path; the JIT's right-to-left
            // backtracking machinery hasn't been generalized for single-term VariableMin.
            term.quantify(min, max, greedy ? QuantifierType::Greedy : QuantifierType::NonGreedy);
        } else {
            if (term.matchDirection() == Forward) {
                term.quantify(min, min, QuantifierType::FixedCount);
                auto copied = copyTerm(term, /* filterStartsWithBOL */ false);
                if (!copied) [[unlikely]]
                    return;
                m_alternative->m_terms.append(WTF::move(*copied));
                // NOTE: this term is interesting from an analysis perspective, in that it can be ignored.....
                m_alternative->lastTerm().quantify((max == quantifyInfinite) ? max : max - min, greedy ? QuantifierType::Greedy : QuantifierType::NonGreedy);
                if (m_alternative->lastTerm().type == PatternTerm::Type::ParenthesesSubpattern)
                    m_alternative->lastTerm().parentheses.isCopy = true;
            } else {
                term.quantify((max == quantifyInfinite) ? max : max - min, greedy ? QuantifierType::Greedy : QuantifierType::NonGreedy);
                if (term.type == PatternTerm::Type::ParenthesesSubpattern)
                    term.parentheses.isCopy = true;
                auto copied = copyTerm(term, /* filterStartsWithBOL */ false);
                if (!copied) [[unlikely]]
                    return;
                m_alternative->m_terms.append(WTF::move(*copied));
                m_alternative->lastTerm().quantify(min, min, QuantifierType::FixedCount);
                if (m_alternative->lastTerm().type == PatternTerm::Type::ParenthesesSubpattern)
                    m_alternative->lastTerm().parentheses.isCopy = false;
            }
        }
    }

    void disjunction(CreateDisjunctionPurpose purpose = CreateDisjunctionPurpose::NotForNextAlternative)
    {
        if (purpose == CreateDisjunctionPurpose::ForNextAlternative && !m_alternative->m_parent->m_parent) {
            // Top level alternative, record captured ranges to clear out from prior alternatives.
            m_alternative->m_lastSubpatternId = m_pattern.m_numSubpatterns;
        }

        m_alternative = m_alternative->m_parent->addNewAlternative(m_pattern.m_numSubpatterns, parenthesisMatchDirection());
    }

    inline bool NODELETE abortedDueToError() const
    {
        return hasError(m_error);
    }

    inline ErrorCode NODELETE abortErrorCode() const
    {
        return m_error;
    }

    [[nodiscard]] ErrorCode setupAlternativeOffsets(PatternAlternative* alternative, CheckedUint32 currentCallFrameSize, unsigned initialInputPosition, CheckedUint32& newCallFrameSize)
    {
        if (!isSafeToRecurse()) [[unlikely]]
            return ErrorCode::TooManyDisjunctions;

        ErrorCode error = ErrorCode::NoError;
        alternative->m_hasFixedSize = true;
        CheckedUint32 currentInputPosition = initialInputPosition;

        for (unsigned i = 0; i < alternative->m_terms.size(); ++i) {
            PatternTerm& term = alternative->m_terms[i];

            switch (term.type) {
            case PatternTerm::Type::AssertionBOL:
            case PatternTerm::Type::AssertionEOL:
            case PatternTerm::Type::AssertionWordBoundary:
                term.inputPosition = currentInputPosition;
                break;

            case PatternTerm::Type::NumberedBackReference:
            case PatternTerm::Type::NamedBackReference:
                term.inputPosition = currentInputPosition;
                term.frameLocation = currentCallFrameSize;
                currentCallFrameSize += YarrStackSpaceForBackTrackInfoBackReference;
                if (currentCallFrameSize.hasOverflowed())
                    return ErrorCode::FrameTooLarge;
                alternative->m_hasFixedSize = false;
                break;

            case PatternTerm::Type::NumberedForwardReference:
            case PatternTerm::Type::NamedForwardReference:
                break;

            case PatternTerm::Type::PatternCharacter:
                term.inputPosition = currentInputPosition;
                if (term.quantityType != QuantifierType::FixedCount) {
                    term.frameLocation = currentCallFrameSize;
                    currentCallFrameSize += YarrStackSpaceForBackTrackInfoPatternCharacter;
                    if (currentCallFrameSize.hasOverflowed())
                        return ErrorCode::FrameTooLarge;
                    alternative->m_hasFixedSize = false;
                } else if (m_pattern.eitherUnicode()) {
                    CheckedUint32 tempCount = term.quantityMaxCount;
                    tempCount *= U16_LENGTH(term.patternCharacter);
                    if (tempCount.hasOverflowed())
                        return ErrorCode::OffsetTooLarge;
                    currentInputPosition += tempCount;
                } else
                    currentInputPosition += term.quantityMaxCount;
                break;

            case PatternTerm::Type::CharacterClass:
                term.inputPosition = currentInputPosition;
                if (term.quantityType != QuantifierType::FixedCount) {
                    term.frameLocation = currentCallFrameSize;
                    currentCallFrameSize += YarrStackSpaceForBackTrackInfoCharacterClass;
                    if (currentCallFrameSize.hasOverflowed())
                        return ErrorCode::FrameTooLarge;
                    alternative->m_hasFixedSize = false;
                } else if (m_pattern.eitherUnicode()) {
                    term.frameLocation = currentCallFrameSize;
                    currentCallFrameSize += YarrStackSpaceForBackTrackInfoCharacterClass;
                    if (currentCallFrameSize.hasOverflowed())
                        return ErrorCode::FrameTooLarge;
                    if (term.characterClass->hasOneCharacterSize() && !term.invert()) {
                        CheckedUint32 tempCount = term.quantityMaxCount;
                        tempCount *= term.characterClass->hasNonBMPCharacters() ? 2 : 1;
                        if (tempCount.hasOverflowed())
                            return ErrorCode::OffsetTooLarge;
                        currentInputPosition += tempCount;
                    } else {
                        currentInputPosition += term.quantityMaxCount;
                        alternative->m_hasFixedSize = false;
                    }
                } else
                    currentInputPosition += term.quantityMaxCount;
                break;

            case PatternTerm::Type::ParenthesesSubpattern:
                // Note: for fixed once parentheses we will ensure at least the minimum is available; others are on their own.
                term.frameLocation = currentCallFrameSize;
                if (term.quantityMaxCount == 1 && !term.parentheses.isCopy) {
                    currentCallFrameSize += YarrStackSpaceForBackTrackInfoParenthesesOnce;
                    if (currentCallFrameSize.hasOverflowed())
                        return ErrorCode::FrameTooLarge;
                    error = setupDisjunctionOffsets(term.parentheses.disjunction, currentCallFrameSize, currentInputPosition, currentCallFrameSize);
                    if (hasError(error))
                        return error;
                    // If quantity is fixed, then pre-check its minimum size.
                    if (term.quantityType == QuantifierType::FixedCount)
                        currentInputPosition += term.parentheses.disjunction->m_minimumSize;
                    term.inputPosition = currentInputPosition;
                } else if (term.parentheses.isTerminal) {
                    currentCallFrameSize += YarrStackSpaceForBackTrackInfoParenthesesTerminal;
                    if (currentCallFrameSize.hasOverflowed())
                        return ErrorCode::FrameTooLarge;
                    error = setupDisjunctionOffsets(term.parentheses.disjunction, currentCallFrameSize, currentInputPosition, currentCallFrameSize);
                    if (hasError(error))
                        return error;
                    term.inputPosition = currentInputPosition;
                } else {
                    term.inputPosition = currentInputPosition;
                    currentCallFrameSize += YarrStackSpaceForBackTrackInfoParentheses;
                    if (currentCallFrameSize.hasOverflowed())
                        return ErrorCode::FrameTooLarge;
                    error = setupDisjunctionOffsets(term.parentheses.disjunction, currentCallFrameSize, currentInputPosition, currentCallFrameSize);
                    if (hasError(error))
                        return error;
                }
                // Fixed count of 1 could be accepted, if they have a fixed size *AND* if all alternatives are of the same length.
                alternative->m_hasFixedSize = false;
                break;

            case PatternTerm::Type::ParentheticalAssertion: {
                unsigned disjunctionInitialInputPosition = (term.matchDirection() == Forward) ? currentInputPosition.value() : 0;
                term.inputPosition = currentInputPosition;
                term.frameLocation = currentCallFrameSize;
                currentCallFrameSize += YarrStackSpaceForBackTrackInfoParentheticalAssertion;
                if (currentCallFrameSize.hasOverflowed())
                    return ErrorCode::FrameTooLarge;
                error = setupDisjunctionOffsets(term.parentheses.disjunction, currentCallFrameSize, disjunctionInitialInputPosition, currentCallFrameSize);
                if (hasError(error))
                    return error;
                break;
            }

            case PatternTerm::Type::DotStarEnclosure:
                ASSERT(!m_pattern.m_saveInitialStartValue);
                alternative->m_hasFixedSize = false;
                term.inputPosition = initialInputPosition;
                m_pattern.m_initialStartValueFrameLocation = currentCallFrameSize;
                currentCallFrameSize += YarrStackSpaceForDotStarEnclosure;
                if (currentCallFrameSize.hasOverflowed())
                    return ErrorCode::FrameTooLarge;
                m_pattern.m_saveInitialStartValue = true;
                break;
            }
            if (currentInputPosition.hasOverflowed())
                return ErrorCode::OffsetTooLarge;
        }

        alternative->m_minimumSize = currentInputPosition - initialInputPosition;
        newCallFrameSize = currentCallFrameSize.value();
        return error;
    }

    ErrorCode setupDisjunctionOffsets(PatternDisjunction* disjunction, CheckedUint32 initialCallFrameSize, unsigned initialInputPosition, CheckedUint32& callFrameSize)
    {
        if (!isSafeToRecurse()) [[unlikely]]
            return ErrorCode::TooManyDisjunctions;

        if ((disjunction != m_pattern.m_body) && (disjunction->m_alternatives.size() > 1)) {
            initialCallFrameSize += YarrStackSpaceForBackTrackInfoAlternative;
            if (initialCallFrameSize.hasOverflowed())
                return ErrorCode::FrameTooLarge;
        }

        bool shareOffsets = (disjunction == m_pattern.m_body);

        unsigned minimumInputSize = UINT_MAX;
        unsigned maximumCallFrameSize = 0;
        bool hasFixedSize = true;
        ErrorCode error = ErrorCode::NoError;

        CheckedUint32 perAlternativeInitial = initialCallFrameSize;
        for (unsigned alt = 0; alt < disjunction->m_alternatives.size(); ++alt) {
            PatternAlternative* alternative = disjunction->m_alternatives[alt].get();
            CheckedUint32 currentAlternativeCallFrameSize;
            error = setupAlternativeOffsets(alternative, perAlternativeInitial, initialInputPosition, currentAlternativeCallFrameSize);
            if (hasError(error))
                return error;
            minimumInputSize = std::min(minimumInputSize, alternative->m_minimumSize);
            maximumCallFrameSize = std::max(maximumCallFrameSize, currentAlternativeCallFrameSize.value());
            hasFixedSize &= alternative->m_hasFixedSize;
            if (alternative->m_minimumSize > INT_MAX)
                m_pattern.m_containsUnsignedLengthPattern = true;
            if (!shareOffsets)
                perAlternativeInitial = currentAlternativeCallFrameSize;
        }

        ASSERT(maximumCallFrameSize >= initialCallFrameSize);

        disjunction->m_hasFixedSize = hasFixedSize;
        disjunction->m_minimumSize = minimumInputSize;
        disjunction->m_callFrameSize = maximumCallFrameSize;
        callFrameSize = maximumCallFrameSize;
        return error;
    }

    ErrorCode setupOffsets()
    {
        // FIXME: Yarr should not use the stack to handle subpatterns (rdar://problem/26436314).
        CheckedUint32 ignoredCallFrameSize;
        return setupDisjunctionOffsets(m_pattern.m_body, 0, 0, ignoredCallFrameSize);
    }

    // This optimization identifies sets of parentheses that we will never need to backtrack.
    // In these cases we do not need to store state from prior iterations.
    // We can presently avoid backtracking for:
    //   * where the parens are at the end of the regular expression (last term in any of the
    //     alternatives of the main body disjunction).
    //   * where the parens are non-capturing, and quantified unbounded greedy (*).
    //   * where the parens do not contain any capturing subpatterns.
    //   * Where the parens contains a BOL anchored non-captured subpattern with a single
    //     alternative of fixed strings, e.g. /^(?:foo|bar|baz).
    //     In such a case we can simplify matching a little more by stopping at the first
    //     matched string alternative, without jumping to backtracking doe to fixup offests.
    //     Instead we fixup the offsets, if needed, at the top of the next alternative's
    //     matching JIT code.
    void checkForTerminalParentheses()
    {
        // This check is much too crude; should be just checking whether the candidate
        // node contains nested capturing subpatterns, not the whole expression!
        if (m_pattern.m_numSubpatterns)
            return;

        Vector<std::unique_ptr<PatternAlternative>>& alternatives = m_pattern.m_body->m_alternatives;
        alternatives.last()->m_isLastAlternative = true;

        if (alternatives.size() == 1 && alternatives[0]->m_startsWithBOL) {
            Vector<PatternTerm>& terms = alternatives[0]->m_terms;

            bool isStringList = false;

            if (terms.size() >= 2
                && terms[0].type == PatternTerm::Type::AssertionBOL
                && terms[1].type == PatternTerm::Type::ParenthesesSubpattern
                && terms[1].quantityType == QuantifierType::FixedCount
                && terms[1].quantityMaxCount == 1
                && (terms.size() == 2
                    || (terms.size() == 3 && terms[2].type == PatternTerm::Type::AssertionEOL))) {
                // We start assuming this is a string list and then prove the negative.
                isStringList = true;

                PatternTerm& term = terms[1];

                PatternDisjunction* nestedDisjunction = term.parentheses.disjunction;
                constexpr unsigned emptyAlternativeNotFound = std::numeric_limits<unsigned>::max();
                unsigned firstEmptyAlternative = emptyAlternativeNotFound;
                for (unsigned alt = 0; isStringList && alt < nestedDisjunction->m_alternatives.size(); ++alt) {
                    Vector<PatternTerm>& innerTerms = nestedDisjunction->m_alternatives[alt]->m_terms;

                    if (innerTerms.isEmpty() && firstEmptyAlternative == emptyAlternativeNotFound)
                        firstEmptyAlternative = alt;

                    for (size_t termIndex = 0; termIndex < innerTerms.size(); ++termIndex) {
                        PatternTerm& innerTerm = innerTerms[termIndex];
                        if (innerTerm.type != PatternTerm::Type::PatternCharacter
                            || innerTerm.quantityType != QuantifierType::FixedCount
                            || innerTerm.quantityMaxCount != 1) {
                            isStringList = false;
                            break;
                        }
                    }
                }

                bool isEOLStringList = terms.size() == 3 && terms[2].type == PatternTerm::Type::AssertionEOL;
                term.parentheses.isStringList = isStringList;
                term.parentheses.isEOLStringList = isEOLStringList;

                // In a non-EOL string list the first empty alternative always matches and ends the match, so later
                // alternatives are unreachable. Drop them so the empty alternative is last and the JIT can fall through to success.
                if (isStringList && !isEOLStringList && firstEmptyAlternative != emptyAlternativeNotFound && firstEmptyAlternative + 1 < nestedDisjunction->m_alternatives.size()) {
                    nestedDisjunction->m_alternatives.shrink(firstEmptyAlternative + 1);
                    nestedDisjunction->m_alternatives.last()->m_isLastAlternative = true;
                }
            }

            if (isStringList)
                return;
        }

        for (auto& alternative : alternatives) {
            auto& terms = alternative->m_terms;
            if (terms.size()) {
                PatternTerm& term = terms.last();
                if (term.type == PatternTerm::Type::ParenthesesSubpattern
                    && term.quantityType == QuantifierType::Greedy
                    && term.quantityMinCount == 0
                    && term.quantityMaxCount == quantifyInfinite
                    && !term.capture())
                    term.parentheses.isTerminal = true;
            }
        }
    }

    // Auto-possessification optimization
    //
    // "Possessive Quantifier" is yet another quantifier type supported in non-JS RegExp engines (e.g. PCRE2).
    // https://www.pcre.org/current/doc/html/pcre2syntax.html
    // It is like /a++/, /a*+/. This is different from Greedy quantifier (/a+/, /a*/) in particular it never does backtracking.
    // Once it greedily matches, even if the subsequent pattern fails, we do not do backtracking. For example,
    //
    // /.*+b/ and "textb". In /.*b/ case, .* will do backtrack to spare "b" for the subsequent pattern. But possessive quantifier
    // drains all text input in this case and never doing backtracking, thus match fails.
    //
    // The benefit of possessive quantifier is it can eliminate the cost of backtracking, so failure becomes quick due to removal
    // of backtracking.
    //
    // While JS RegExp does not support possessive quantifiers, we can internally support and convert patterns to possessive quantifier
    // if we can find this term's backtracking never produces the potentially matching cases for the subsequent patterns.
    //
    // Let's show an example. A greedy single-character term `T` immediately followed by a mandatory term `U` whose first character
    // can never be a character that `T` matches is effectively possessive. Once `T` has matched greedily, giving characters back
    // can never let `U` match (a given-back position still holds a `T` character, which `U` rejects), so all of the backtracking the
    // engine would do into `T` is futile. We mark such a `T` so the JIT can skip generating (and running) that dead backtracking.

    void optimizePossessiveQuantifiers()
    {
        auto rawClassContains = [](const CharacterClass* characterClass, char32_t ch) -> TriState {
            if (characterClass->m_anyCharacter)
                return TriState::True;

            if (!characterClass->hasSingleCharacters())
                return characterClass->m_table ? TriState::Indeterminate : TriState::False;

            bool isLatin1Char = isLatin1(ch);
            const auto& matches = isLatin1Char ? characterClass->m_matches8 : characterClass->m_matches32;
            for (auto match : matches) {
                if (match == ch)
                    return TriState::True;
            }
            const auto& ranges = isLatin1Char ? characterClass->m_ranges8 : characterClass->m_ranges32;
            for (auto range : ranges) {
                if (ch >= range.begin && ch <= range.end)
                    return TriState::True;
            }
            return TriState::False;
        };

        auto termMatchesCharacter = [&](const PatternTerm& term, char32_t ch) -> TriState {
            if (term.type == PatternTerm::Type::PatternCharacter) {
                char32_t pc = term.patternCharacter;
                if (pc == ch)
                    return TriState::True;
                if (term.ignoreCase()) {
                    if (!isASCII(pc) || !isASCII(ch))
                        return TriState::Indeterminate;
                    if (toASCIIUpper(pc) == toASCIIUpper(ch))
                        return TriState::True;
                }
                return TriState::False;
            }

            ASSERT(term.type == PatternTerm::Type::CharacterClass);
            TriState raw = rawClassContains(term.characterClass, ch);
            if (raw == TriState::Indeterminate)
                return TriState::Indeterminate;
            if (term.invert())
                return raw == TriState::True ? TriState::False : TriState::True;
            return raw;
        };

        // Returns true if and only if `next` (the term right after the greedy term) is mandatory and its
        // first character is provably disjoint from `greedy`'s set.
        auto followerForcesPossessive = [&](const PatternTerm& greedy, const PatternTerm& next) -> bool {
            if (next.type != PatternTerm::Type::PatternCharacter)
                return false;

            if (next.quantityType != QuantifierType::FixedCount || next.quantityMinCount < 1)
                return false;

            // Every character `next` would accept must be rejected by `greedy`.
            char32_t fc = next.patternCharacter;
            if (termMatchesCharacter(greedy, fc) != TriState::False)
                return false;

            if (next.ignoreCase()) {
                if (!isASCII(fc))
                    return false; // Don't reason about non-ASCII case folds.

                if (termMatchesCharacter(greedy, toASCIIUpper(fc)) != TriState::False)
                    return false;

                if (termMatchesCharacter(greedy, toASCIILower(fc)) != TriState::False)
                    return false;
            }
            return true;
        };

        auto isPossessifiableGreedyTerm = [](const PatternTerm& term) -> bool {
            return term.quantityType == QuantifierType::Greedy && (term.type == PatternTerm::Type::PatternCharacter || term.type == PatternTerm::Type::CharacterClass);
        };

        for (auto& disjunction : m_pattern.m_disjunctions) {
            for (auto& alternative : disjunction->m_alternatives) {
                if (alternative->matchDirection() != Forward)
                    continue;

                auto& terms = alternative->m_terms;
                for (unsigned i = 1; i < terms.size(); ++i) {
                    PatternTerm& current = terms[i - 1];
                    PatternTerm& next = terms[i];
                    if (!isPossessifiableGreedyTerm(current))
                        continue;

                    if (!followerForcesPossessive(current, next))
                        continue;

                    current.m_possessive = true;
                }
            }
        }
    }

    void optimizeBOL()
    {
        // Look for expressions containing beginning of line (^) anchoring and unroll them.
        // e.g. /^a|^b|c/ becomes /^a|^b|c/ which is executed once followed by /c/ which loops
        // This code relies on the parsing code tagging alternatives with m_containsBOL and
        // m_startsWithBOL and rolling those up to containing alternatives.
        // At this point, this is only valid for non-multiline expressions.
        PatternDisjunction* disjunction = m_pattern.m_body;
        
        // We'll start by being safe, since `m` mode could change with modifiers
        if (m_pattern.m_containsModifiers || !m_pattern.m_containsBOL || m_pattern.multiline())
            return;
        
        PatternDisjunction* loopDisjunction = copyDisjunction(disjunction, /* filterStartsWithBOL */ true);

        // Set alternatives in disjunction to "onceThrough"
        for (unsigned alt = 0; alt < disjunction->m_alternatives.size(); ++alt)
            disjunction->m_alternatives[alt]->setOnceThrough();

        if (loopDisjunction) {
            // Move alternatives from loopDisjunction to disjunction
            for (unsigned alt = 0; alt < loopDisjunction->m_alternatives.size(); ++alt)
                disjunction->m_alternatives.append(loopDisjunction->m_alternatives[alt].release());
                
            loopDisjunction->m_alternatives.clear();
        }
    }

    bool containsCapturingTerms(PatternAlternative* alternative, size_t firstTermIndex, size_t endIndex)
    {
        if (!isSafeToRecurse()) [[unlikely]] {
            m_error = ErrorCode::PatternTooLarge;
            return true;
        }

        Vector<PatternTerm>& terms = alternative->m_terms;

        ASSERT(endIndex <= terms.size());
        for (size_t termIndex = firstTermIndex; termIndex < endIndex; ++termIndex) {
            PatternTerm& term = terms[termIndex];

            if (term.m_capture)
                return true;

            if (term.type == PatternTerm::Type::ParenthesesSubpattern) {
                PatternDisjunction* nestedDisjunction = term.parentheses.disjunction;
                for (unsigned alt = 0; alt < nestedDisjunction->m_alternatives.size(); ++alt) {
                    if (containsCapturingTerms(nestedDisjunction->m_alternatives[alt].get(), 0, nestedDisjunction->m_alternatives[alt]->m_terms.size()))
                        return true;
                }
            }
        }

        return false;
    }

    // This optimization identifies alternatives in the form of 
    // [^].*[?]<expression>.*[$] for expressions that don't have any 
    // capturing terms. The alternative is changed to <expression> 
    // followed by processing of the dot stars to find and adjust the 
    // beginning and the end of the match.
    void optimizeDotStarWrappedExpressions()
    {
        Vector<std::unique_ptr<PatternAlternative>>& alternatives = m_pattern.m_body->m_alternatives;
        if (alternatives.size() != 1)
            return;

        CharacterClass* dotCharacterClass = dotAll() ? m_pattern.anyCharacterClass() : m_pattern.newlineCharacterClass();
        PatternAlternative* alternative = alternatives[0].get();
        Vector<PatternTerm>& terms = alternative->m_terms;
        if (terms.size() >= 3) {
            bool startsWithBOL = false;
            bool endsWithEOL = false;
            size_t termIndex, firstExpressionTerm;

            termIndex = 0;
            if (terms[termIndex].type == PatternTerm::Type::AssertionBOL) {
                startsWithBOL = true;
                ++termIndex;
            }
            
            PatternTerm& firstNonAnchorTerm = terms[termIndex];
            if (firstNonAnchorTerm.type != PatternTerm::Type::CharacterClass
                || firstNonAnchorTerm.characterClass != dotCharacterClass
                || firstNonAnchorTerm.quantityMinCount
                || firstNonAnchorTerm.quantityMaxCount != quantifyInfinite)
                return;
            
            firstExpressionTerm = termIndex + 1;
            
            termIndex = terms.size() - 1;
            if (terms[termIndex].type == PatternTerm::Type::AssertionEOL) {
                endsWithEOL = true;
                --termIndex;
            }
            
            PatternTerm& lastNonAnchorTerm = terms[termIndex];
            if (lastNonAnchorTerm.type != PatternTerm::Type::CharacterClass
                || lastNonAnchorTerm.characterClass != dotCharacterClass
                || lastNonAnchorTerm.quantityType != QuantifierType::Greedy
                || lastNonAnchorTerm.quantityMinCount
                || lastNonAnchorTerm.quantityMaxCount != quantifyInfinite)
                return;

            size_t endIndex = termIndex;
            if (firstExpressionTerm >= endIndex)
                return;

            if (!containsCapturingTerms(alternative, firstExpressionTerm, endIndex)) {
                for (termIndex = terms.size() - 1; termIndex >= endIndex; --termIndex)
                    terms.removeAt(termIndex);

                for (termIndex = firstExpressionTerm; termIndex > 0; --termIndex)
                    terms.removeAt(termIndex - 1);

                terms.append(PatternTerm(startsWithBOL, endsWithEOL, m_flags));
                
                m_pattern.m_containsBOL = false;
            }
        }
    }

    void setupNamedCaptures()
    {
        if (!m_pattern.m_hasNamedCaptureGroups)
            return;

        // Finish padding out m_captureGroupNames vector.
        while (m_pattern.m_captureGroupNames.size() <= m_pattern.m_numSubpatterns)
            m_pattern.m_captureGroupNames.append(String());

        for (auto& namedGroupIndicies : m_pattern.m_namedGroupToParenIndices.values()) {
            if (namedGroupIndicies.size() == 2) {
                // Since this named group is only used in one place, i.e. not a duplicate name,
                // make that subpatternId as the only value in the vector.
                ASSERT(namedGroupIndicies[0] == namedGroupIndicies[1]);
                namedGroupIndicies.takeLast();
            }
        }

        if (m_pattern.m_numDuplicateNamedCaptureGroups) {
            m_pattern.m_duplicateNamedGroupForSubpatternId.fill(0, m_pattern.m_numSubpatterns + 1);
            for (auto& namedGroupIndicies : m_pattern.m_namedGroupToParenIndices.values()) {
                if (namedGroupIndicies.size() > 2) {
                    auto duplicateNamedGroupId = namedGroupIndicies[0];
                    for (unsigned i = 1; i < namedGroupIndicies.size(); ++i) {
                        auto subpatternId = namedGroupIndicies[i];
                        ASSERT(!m_pattern.m_duplicateNamedGroupForSubpatternId[subpatternId]);
                        m_pattern.m_duplicateNamedGroupForSubpatternId[subpatternId] = duplicateNamedGroupId;
                    }
                }
            }
        }
    }

    void extractSpecificPattern()
    {
        if (m_pattern.m_containsBackreferences)
            return;
        if (m_pattern.m_containsLookbehinds)
            return;
        if (m_pattern.m_containsUnsignedLengthPattern)
            return;
        if (m_pattern.m_containsModifiers)
            return;
        if (m_pattern.m_hasCopiedParenSubexpressions)
            return;
        if (m_pattern.m_hasNamedCaptureGroups)
            return;
        if (m_pattern.m_saveInitialStartValue)
            return;
        if (m_pattern.m_numSubpatterns)
            return;
        if (m_pattern.multiline())
            return;
        if (m_pattern.sticky())
            return;
        if (m_pattern.eitherUnicode())
            return;
        if (m_pattern.ignoreCase())
            return;

        auto tryExtractAtom = [&]() -> bool {
            if (m_pattern.m_containsBOL)
                return false;
            PatternDisjunction* disjunction = m_pattern.m_body;
            if (!disjunction->m_minimumSize)
                return false;
            auto& alternatives = disjunction->m_alternatives;
            if (alternatives.size() != 1)
                return false;
            StringBuilder builder;
            auto* alternative = alternatives[0].get();
            for (unsigned index = 0; index < alternative->m_terms.size(); ++index) {
                auto& term = alternative->m_terms[index];
                if (term.type != PatternTerm::Type::PatternCharacter)
                    return false;
                if (term.quantityType != QuantifierType::FixedCount)
                    return false;
                if (term.quantityMaxCount != 1)
                    return false;
                if (term.inputPosition != index)
                    return false;
                if (U16_LENGTH(term.patternCharacter) != 1)
                    return false;
                if (term.m_matchDirection != MatchDirection::Forward)
                    return false;
                builder.append(static_cast<char16_t>(term.patternCharacter));
            }
            String atom = builder.toString();
            if (atom.length() > 0) {
                m_pattern.m_atom = WTF::move(atom);
                m_pattern.m_specificPattern = SpecificPattern::Atom;
                return true;
            }
            return false;
        };

        auto tryExtractSpaces = [&]() -> bool {
            PatternDisjunction* disjunction = m_pattern.m_body;
            auto& alternatives = disjunction->m_alternatives;
            if (alternatives.size() != 1)
                return false;

            auto* alternative = alternatives[0].get();
            if (alternative->m_terms.isEmpty())
                return false;

            if (m_pattern.m_containsBOL) {
                auto& termFirst = alternative->m_terms.first();
                if (termFirst.invert() || termFirst.type != PatternTerm::Type::AssertionBOL)
                    return false;

                if (alternative->m_terms.size() == 2) {
                    // ^\s*
                    auto& term1 = alternative->m_terms[1];
                    if (term1.invert() || term1.type != PatternTerm::Type::CharacterClass || term1.characterClass != m_pattern.spacesCharacterClass())
                        return false;
                    if (term1.inputPosition)
                        return false;
                    if (term1.quantityType != QuantifierType::Greedy)
                        return false;
                    if (term1.quantityMinCount)
                        return false;
                    if (term1.quantityMaxCount != quantifyInfinite)
                        return false;

                    m_pattern.m_specificPattern = SpecificPattern::LeadingSpacesStar;
                    return true;
                }

                if (alternative->m_terms.size() == 3) {
                    // ^\s+
                    auto& term1 = alternative->m_terms[1];
                    if (term1.invert() || term1.type != PatternTerm::Type::CharacterClass || term1.characterClass != m_pattern.spacesCharacterClass())
                        return false;
                    if (term1.inputPosition)
                        return false;
                    if (term1.quantityType != QuantifierType::FixedCount)
                        return false;
                    if (term1.quantityMinCount != 1)
                        return false;
                    if (term1.quantityMaxCount != 1)
                        return false;

                    auto& term2 = alternative->m_terms[2];
                    if (term2.invert() || term2.type != PatternTerm::Type::CharacterClass || term2.characterClass != m_pattern.spacesCharacterClass())
                        return false;
                    if (term2.inputPosition != 1)
                        return false;
                    if (term2.quantityType != QuantifierType::Greedy)
                        return false;
                    if (term2.quantityMinCount)
                        return false;
                    if (term2.quantityMaxCount != quantifyInfinite)
                        return false;

                    m_pattern.m_specificPattern = SpecificPattern::LeadingSpacesPlus;
                    return true;
                }
                return false;
            }

            auto& termLast = alternative->m_terms.last();
            if (termLast.invert() || termLast.type != PatternTerm::Type::AssertionEOL)
                return false;

            if (alternative->m_terms.size() == 2) {
                // \s*$
                auto& term0 = alternative->m_terms[0];
                if (term0.invert() || term0.type != PatternTerm::Type::CharacterClass || term0.characterClass != m_pattern.spacesCharacterClass())
                    return false;
                if (term0.inputPosition)
                    return false;
                if (term0.quantityType != QuantifierType::Greedy)
                    return false;
                if (term0.quantityMinCount)
                    return false;
                if (term0.quantityMaxCount != quantifyInfinite)
                    return false;

                m_pattern.m_specificPattern = SpecificPattern::TrailingSpacesStar;
                return true;
            }

            if (alternative->m_terms.size() == 3) {
                // \s+$
                auto& term0 = alternative->m_terms[0];
                if (term0.invert() || term0.type != PatternTerm::Type::CharacterClass || term0.characterClass != m_pattern.spacesCharacterClass())
                    return false;
                if (term0.inputPosition)
                    return false;
                if (term0.quantityType != QuantifierType::FixedCount)
                    return false;
                if (term0.quantityMinCount != 1)
                    return false;
                if (term0.quantityMaxCount != 1)
                    return false;

                auto& term1 = alternative->m_terms[1];
                if (term1.invert() || term1.type != PatternTerm::Type::CharacterClass || term1.characterClass != m_pattern.spacesCharacterClass())
                    return false;
                if (term1.inputPosition != 1)
                    return false;
                if (term1.quantityType != QuantifierType::Greedy)
                    return false;
                if (term1.quantityMinCount)
                    return false;
                if (term1.quantityMaxCount != quantifyInfinite)
                    return false;

                m_pattern.m_specificPattern = SpecificPattern::TrailingSpacesPlus;
                return true;
            }

            return false;
        };

        auto tryExtractNewlines = [&]() -> bool {
            // Detect patterns: \r\n?|\n or \n|\r\n?
            // These patterns match LF (\n), CR (\r), and CRLF (\r\n)

            PatternDisjunction* disjunction = m_pattern.m_body;
            auto& alternatives = disjunction->m_alternatives;

            if (alternatives.size() != 2)
                return false;

            auto isCROptionalLF = [](PatternAlternative* alternative) -> bool {
                if (alternative->m_terms.size() != 2)
                    return false;

                auto& term0 = alternative->m_terms[0];
                if (term0.type != PatternTerm::Type::PatternCharacter)
                    return false;
                if (term0.patternCharacter != '\r')
                    return false;
                if (term0.quantityType != QuantifierType::FixedCount)
                    return false;
                if (term0.quantityMinCount != 1 || term0.quantityMaxCount != 1)
                    return false;

                auto& term1 = alternative->m_terms[1];
                if (term1.type != PatternTerm::Type::PatternCharacter)
                    return false;
                if (term1.patternCharacter != '\n')
                    return false;
                if (term1.quantityType != QuantifierType::Greedy)
                    return false;
                if (term1.quantityMinCount || term1.quantityMaxCount != 1)
                    return false;

                return true;
            };

            auto isLF = [](PatternAlternative* alternative) -> bool {
                if (alternative->m_terms.size() != 1)
                    return false;

                auto& term = alternative->m_terms[0];
                if (term.type != PatternTerm::Type::PatternCharacter)
                    return false;
                if (term.patternCharacter != '\n')
                    return false;
                if (term.quantityType != QuantifierType::FixedCount)
                    return false;
                if (term.quantityMinCount != 1 || term.quantityMaxCount != 1)
                    return false;

                return true;
            };

            auto* alternative1 = alternatives[0].get();
            auto* alternative2 = alternatives[1].get();

            bool matches = (isCROptionalLF(alternative1) && isLF(alternative2))
                || (isLF(alternative1) && isCROptionalLF(alternative2));

            if (matches) {
                m_pattern.m_specificPattern = SpecificPattern::Newlines;
                return true;
            }

            return false;
        };

        if (tryExtractAtom())
            return;

        if (tryExtractSpaces())
            return;

        if (tryExtractNewlines())
            return;
    }

    ErrorCode NODELETE error() { return m_error; }

private:
    class ParenthesisContext {
    private:
        class SavedContext {
        public:
            SavedContext(bool isModifier, bool invert, MatchDirection matchDirection, OptionSet<Flags> flags)
                : m_isModifier(isModifier)
                , m_invert(invert)
                , m_matchDirection(matchDirection)
                , m_flags(flags)
            {
            }

            void NODELETE restore(bool& isModifier, bool& invert, MatchDirection& matchDirection, OptionSet<Flags>& flags)
            {
                isModifier = m_isModifier;
                invert = m_invert;
                matchDirection = m_matchDirection;
                flags = m_flags;
            }

        private:
            bool m_isModifier { false };
            bool m_invert { false };
            MatchDirection m_matchDirection { Forward };
            OptionSet<Flags> m_flags;
        };

    public:
        ParenthesisContext()
        {
        }

        void push()
        {
            ASSERT(m_stackDepth < std::numeric_limits<unsigned>::max());

            if (m_stackDepth++ > 0)
                m_backingStack.append(SavedContext(m_isModifier, m_invert, m_matchDirection, m_flags));

            // isModifier should only apply to one frame at a time
            m_isModifier = false;
        }

        void NODELETE pop()
        {
            ASSERT(m_stackDepth > 0);

            if (--m_stackDepth > 0) {
                SavedContext context = m_backingStack.takeLast();
                context.restore(m_isModifier, m_invert, m_matchDirection, m_flags);
            } else {
                m_isModifier = false;
                m_invert = false;
                m_matchDirection = Forward;
                m_flags = { };
            }
        }

        void NODELETE setModifier(bool isMod)
        {
            m_isModifier = isMod;
        }

        bool NODELETE isModifier() const
        {
            return m_isModifier;
        }

        void NODELETE setInvert(bool invert)
        {
            m_invert = invert;
        }

        bool invert() const
        {
            return m_invert;
        }

        void setMatchDirection(MatchDirection matchDirection)
        {
            m_matchDirection = matchDirection;
        }

        MatchDirection matchDirection() const
        {
            return m_matchDirection;
        }

        void NODELETE setFlags(OptionSet<Flags> flags)
        {
            m_flags = flags;
        }

        OptionSet<Flags> NODELETE flags() const
        {
            return m_flags;
        }

        void reset()
        {
            m_backingStack.clear();
            m_stackDepth = 0;

            m_isModifier = false;
            m_invert = false;
            m_matchDirection = Forward;
            m_flags = { };
        }

    private:
        Vector<SavedContext, 0> m_backingStack;
        unsigned m_stackDepth { 0 };
        bool m_isModifier { false };
        bool m_invert { false };
        MatchDirection m_matchDirection { Forward };
        OptionSet<Flags> m_flags;
    };

    void pushParenthesisContext()
    {
        m_parenthesisContext.push();
    }

    void NODELETE popParenthesisContext()
    {
        m_parenthesisContext.pop();
    }

    void NODELETE setParenthesisInvert(bool invert)
    {
        m_parenthesisContext.setInvert(invert);
    }

    bool NODELETE parenthesisInvert() const
    {
        return m_parenthesisContext.invert();
    }

    void NODELETE setParenthesisMatchDirection(MatchDirection matchDirection)
    {
        m_parenthesisContext.setMatchDirection(matchDirection);
    }

    MatchDirection NODELETE parenthesisMatchDirection() const
    {
        return m_parenthesisContext.matchDirection();
    }

    bool ignoreCase() const
    {
        return m_flags.contains(Flags::IgnoreCase);
    }

    bool multiline() const
    {
        return m_flags.contains(Flags::Multiline);
    }

    bool dotAll() const
    {
        return m_flags.contains(Flags::DotAll);
    }

    inline bool isSafeToRecurse() { return m_stackCheck.isSafeToRecurse(); }

    YarrPattern& m_pattern;
    PatternAlternative* m_alternative;
    CharacterClassConstructor m_baseCharacterClassConstructor;
    CharacterClassConstructor* m_currentCharacterClassConstructor;
    Vector<CharacterClassConstructor> m_characterClassStack;
    Vector<UnresolvedForwardReference> m_forwardReferencesInLookbehind;
    StackCheck m_stackCheck;
    ErrorCode m_error { ErrorCode::NoError };
    bool m_invertCharacterClass;
    ParenthesisContext m_parenthesisContext;

    OptionSet<Flags> m_initialFlags;
    OptionSet<Flags> m_flags;
};
static_assert(YarrSyntaxCheckable<YarrPatternConstructor>);

ErrorCode YarrPattern::compile(StringView patternString)
{
    YarrPatternConstructor constructor(*this, m_flags);

    {
        ErrorCode error = parse(constructor, patternString, compileMode());
        if (hasError(constructor.error()))
            return constructor.error();

        if (hasError(error))
            return error;
    }

    constructor.checkForTerminalParentheses();
    constructor.optimizeDotStarWrappedExpressions();
    constructor.optimizeBOL();
    constructor.optimizePossessiveQuantifiers();

    if (hasError(constructor.error()))
        return constructor.error();

    {
        ErrorCode error = constructor.setupOffsets();
        if (hasError(error))
            return error;
    }

    constructor.setupNamedCaptures();

    constructor.extractSpecificPattern();

    if (Options::dumpCompiledRegExpPatterns()) [[unlikely]]
        dumpPattern(patternString);

    return ErrorCode::NoError;
}

YarrPattern::YarrPattern(StringView pattern, OptionSet<Flags> flags, ErrorCode& error)
    : m_containsBackreferences(false)
    , m_containsBOL(false)
    , m_containsLookbehinds(false)
    , m_containsUnsignedLengthPattern(false)
    , m_containsModifiers(false)
    , m_hasCopiedParenSubexpressions(false)
    , m_hasNamedCaptureGroups(false)
    , m_saveInitialStartValue(false)
    , m_flags(flags)
{
    ASSERT(m_flags != Flags::DeletedValue);
    error = compile(pattern);
}

void indentForNestingLevel(PrintStream& out, unsigned nestingDepth)
{
    out.print("    ");
    for (; nestingDepth; --nestingDepth)
        out.print("  ");
}

void dumpChar32(PrintStream& out, char32_t c)
{
    if (c >= ' ' && c <= 0xff)
        out.printf("'%c'", static_cast<char>(c));
    else
        out.printf("0x%04x", c);
}

void dumpCharacterClass(PrintStream& out, YarrPattern* pattern, CharacterClass* characterClass)
{
    if (pattern) {
        if (characterClass == pattern->anyCharacterClass()) {
            out.print("<any character>");
            return;
        }
        if (characterClass == pattern->newlineCharacterClass()) {
            out.print("<newline>");
            return;
        }
        if (characterClass == pattern->digitsCharacterClass()) {
            out.print("<digits>");
            return;
        }
        if (characterClass == pattern->spacesCharacterClass()) {
            out.print("<whitespace>");
            return;
        }
        if (characterClass == pattern->wordcharCharacterClass()) {
            out.print("<word>");
            return;
        }
        if (characterClass == pattern->wordUnicodeIgnoreCaseCharCharacterClass()) {
            out.print("<unicode word ignore case>");
            return;
        }
        if (characterClass == pattern->nondigitsCharacterClass()) {
            out.print("<non-digits>");
            return;
        }
        if (characterClass == pattern->nonspacesCharacterClass()) {
            out.print("<non-whitespace>");
            return;
        }
        if (characterClass == pattern->nonwordcharCharacterClass()) {
            out.print("<non-word>");
            return;
        }
        if (characterClass == pattern->nonwordUnicodeIgnoreCaseCharCharacterClass()) {
            out.print("<unicode non-word ignore case>");
            return;
        }
    }

    bool needMatchesRangesSeparator = false;

    auto dumpMatches = [&] (const char* prefix, Vector<char32_t> matches) {
        size_t matchesSize = matches.size();
        if (matchesSize) {
            if (needMatchesRangesSeparator)
                out.print(",");
            needMatchesRangesSeparator = true;

            out.print(prefix, ":(");
            for (size_t i = 0; i < matchesSize; ++i) {
                if (i)
                    out.print(",");
                dumpChar32(out, matches[i]);
            }
            out.print(")");
        }
    };

    auto dumpRanges = [&] (const char* prefix, Vector<CharacterRange> ranges) {
        size_t rangeSize = ranges.size();
        if (rangeSize) {
            if (needMatchesRangesSeparator)
                out.print(",");
            needMatchesRangesSeparator = true;

            out.print(prefix, " ranges:(");
            for (size_t i = 0; i < rangeSize; ++i) {
                if (i)
                    out.print(",");
                CharacterRange range = ranges[i];
                out.print("(");
                dumpChar32(out, range.begin);
                out.print("..");
                dumpChar32(out, range.end);
                out.print(")");
            }
            out.print(")");
        }
    };

    out.print("[");
    dumpMatches("Latin1", characterClass->m_matches8);
    dumpRanges("Latin1", characterClass->m_ranges8);
    dumpMatches("NonLatin1", characterClass->m_matches32);
    dumpRanges("NonLatin1", characterClass->m_ranges32);
    out.print("]");
}

void PatternAlternative::dump(PrintStream& out, YarrPattern* thisPattern, unsigned nestingDepth)
{
    out.print("minimum size: ", m_minimumSize);
    if (m_hasFixedSize)
        out.print(",fixed size");
    if (m_onceThrough)
        out.print(",once through");
    if (m_startsWithBOL)
        out.print(",starts with ^");
    if (m_containsBOL)
        out.print(",contains ^");
    if (m_isLastAlternative)
        out.print(", last alternative");
    out.print("\n");

    for (size_t i = 0; i < m_terms.size(); ++i)
        m_terms[i].dump(out, thisPattern, nestingDepth);
}

void PatternTerm::dumpQuantifier(PrintStream& out)
{
    if (quantityType == QuantifierType::FixedCount && quantityMinCount == 1 && quantityMaxCount == 1)
        return;
    out.print(" {", quantityMinCount.value());
    if (quantityMinCount != quantityMaxCount) {
        if (quantityMaxCount == UINT_MAX)
            out.print(",...");
        else
            out.print(",", quantityMaxCount.value());
    }
    out.print("}");
    if (quantityType == QuantifierType::Greedy)
        out.print(" greedy");
    else if (quantityType == QuantifierType::NonGreedy)
        out.print(" non-greedy");
    if (m_possessive)
        out.print(" possessive");
}

void PatternTerm::dump(PrintStream& out, YarrPattern* thisPattern, unsigned nestingDepth)
{
    indentForNestingLevel(out, nestingDepth);

    out.print("<");
    if (m_currentFlags.contains(Flags::IgnoreCase))
        out.print("i");
    else
        out.print(" ");
    if (m_currentFlags.contains(Flags::Multiline))
        out.print("m");
    else
        out.print(" ");
    if (m_currentFlags.contains(Flags::DotAll))
        out.print("s");
    else
        out.print(" ");
    out.print("> ");

    if (type != Type::ParenthesesSubpattern && type != Type::ParentheticalAssertion) {
        if (invert())
            out.print("not ");
    }

    switch (type) {
    case Type::AssertionBOL:
        out.println("BOL");
        break;
    case Type::AssertionEOL:
        out.println("EOL");
        break;
    case Type::AssertionWordBoundary:
        out.println("word boundary");
        break;
    case Type::PatternCharacter:
        out.printf("character ");
        out.printf("inputPosition %u ", inputPosition);
        if (thisPattern->ignoreCase() && isASCIIAlpha(patternCharacter)) {
            dumpChar32(out, toASCIIUpper(patternCharacter));
            out.print("/");
            dumpChar32(out, toASCIILower(patternCharacter));
        } else
            dumpChar32(out, patternCharacter);
        dumpQuantifier(out);
        if (quantityType != QuantifierType::FixedCount)
            out.print(",frame location ", frameLocation);
        out.println();
        break;
    case Type::CharacterClass:
        out.print("character class ");
        out.printf("inputPosition %u ", inputPosition);
        dumpCharacterClass(out, thisPattern, characterClass);
        dumpQuantifier(out);
        if (quantityType != QuantifierType::FixedCount || thisPattern->eitherUnicode())
            out.print(",frame location ", frameLocation);
        out.println();
        break;
    case Type::NumberedBackReference:
    case Type::NamedBackReference:
        out.print(type == Type::NumberedBackReference ? "numbered " : "named ");
        out.print("back reference of subpattern #", backReferenceSubpatternId);
        out.printf(" inputPosition %u", inputPosition);
        out.println();
        break;
    case Type::NumberedForwardReference:
        out.println("numbered forward reference");
        break;
    case Type::NamedForwardReference:
        out.println("named forward reference");
        break;
    case Type::ParenthesesSubpattern:
        if (m_capture)
            out.print("captured ");
        else
            out.print("non-captured ");

        [[fallthrough]];
    case Type::ParentheticalAssertion:
        if (m_matchDirection) {
            if (type == Type::ParenthesesSubpattern)
                out.print("backwards ");
            else
                out.print("lookbehind ");
        }
        out.printf("inputPosition %u ", inputPosition);
        if (m_invert)
            out.print("inverted ");

        if (type == Type::ParenthesesSubpattern)
            out.print("subpattern");
        else if (type == Type::ParentheticalAssertion)
            out.print("assertion");

        if (m_capture)
            out.print(" #", parentheses.subpatternId);

        dumpQuantifier(out);

        if (parentheses.isCopy)
            out.print(",copy");

        if (parentheses.isTerminal)
            out.print(",terminal");

        if (parentheses.isStringList)
            out.print(",string-list");

        out.println(",frame location ", frameLocation);

        if (parentheses.disjunction->m_alternatives.size() > 1) {
            indentForNestingLevel(out, nestingDepth + 1);
            unsigned alternativeFrameLocation = frameLocation;
            if (quantityMaxCount == 1 && !parentheses.isCopy)
                alternativeFrameLocation += YarrStackSpaceForBackTrackInfoParenthesesOnce;
            else if (parentheses.isTerminal)
                alternativeFrameLocation += YarrStackSpaceForBackTrackInfoParenthesesTerminal;
            else
                alternativeFrameLocation += YarrStackSpaceForBackTrackInfoParentheses;
            out.println("alternative list,frame location ", alternativeFrameLocation);
        }

        parentheses.disjunction->dump(out, thisPattern, nestingDepth + 1);
        break;
    case Type::DotStarEnclosure:
        out.println(".* enclosure,frame location ", thisPattern->m_initialStartValueFrameLocation);
        break;
    }
}

void PatternDisjunction::dump(PrintStream& out, YarrPattern* thisPattern, unsigned nestingDepth = 0)
{
    unsigned alternativeCount = m_alternatives.size();
    for (unsigned i = 0; i < alternativeCount; ++i) {
        indentForNestingLevel(out, nestingDepth);
        if (alternativeCount > 1)
            out.print("alternative #", i, ": ");
        m_alternatives[i].get()->dump(out, thisPattern, nestingDepth + (alternativeCount > 1));
    }
}

void YarrPattern::dumpPatternString(PrintStream& out, StringView patternString)
{
    out.print("/", patternString, "/");

    if (global())
        out.print("g");
    if (ignoreCase())
        out.print("i");
    if (multiline())
        out.print("m");
    if (unicode())
        out.print("u");
    if (unicodeSets())
        out.print("v");
    if (sticky())
        out.print("y");
}

void YarrPattern::dumpPattern(StringView patternString)
{
    dumpPattern(WTF::dataFile(), patternString);
}

void YarrPattern::dumpPattern(PrintStream& out, StringView patternString)
{
    out.print("RegExp pattern for ");
    dumpPatternString(out, patternString);

    if (m_flags) {
        bool printSeparator = false;
        out.print(" (");
        if (global()) {
            out.print("global");
            printSeparator = true;
        }
        if (ignoreCase()) {
            if (printSeparator)
                out.print("|");
            out.print("ignore case");
            printSeparator = true;
        }
        if (multiline()) {
            if (printSeparator)
                out.print("|");
            out.print("multiline");
            printSeparator = true;
        }
        if (unicode()) {
            if (printSeparator)
                out.print("|");
            out.print("unicode");
            printSeparator = true;
        }
        if (unicodeSets()) {
            if (printSeparator)
                out.print("|");
            out.print("unicodeSets");
            printSeparator = true;
        }
        if (sticky()) {
            if (printSeparator)
                out.print("|");
            out.print("sticky");
        }
        out.print(")");
    }
    out.print(":\n");
    if (m_specificPattern != SpecificPattern::None)
        out.print("    specific pattern: ", m_specificPattern, "\n");
    if (m_body->m_callFrameSize)
        out.print("    callframe size: ", m_body->m_callFrameSize, "\n");
    m_body->dump(out, this);
}

std::unique_ptr<CharacterClass> anycharCreate()
{
    auto characterClass = makeUnique<CharacterClass>();
    characterClass->m_ranges8.append(CharacterRange(0x00, 0xff));
    characterClass->m_ranges32.append(CharacterRange(0x0100, UCHAR_MAX_VALUE));
    characterClass->m_characterWidths = CharacterClassWidths::HasBothBMPAndNonBMP;
    characterClass->m_anyCharacter = true;
    return characterClass;
}

std::optional<char16_t> CharacterClass::hasSharedLeadSurrogate() const
{
    if (!hasOnlyNonBMPCharacters())
        return std::nullopt;
    if (!m_strings.isEmpty())
        return std::nullopt;

    ASSERT(m_matches8.isEmpty());
    ASSERT(m_ranges8.isEmpty());

    std::optional<char16_t> commonLeadSurrogate;
    for (auto cp : m_matches32) {
        ASSERT(!U_IS_BMP(cp));
        char16_t leadSurrogate = U16_LEAD(cp);
        if (!commonLeadSurrogate)
            commonLeadSurrogate = leadSurrogate;
        else if (leadSurrogate != commonLeadSurrogate.value())
            return std::nullopt;
    }

    for (auto& range : m_ranges32) {
        ASSERT(!U_IS_BMP(range.begin));
        ASSERT(!U_IS_BMP(range.end));
        char16_t leadSurrogateBegin = U16_LEAD(range.begin);
        char16_t leadSurrogateEnd = U16_LEAD(range.end);
        if (leadSurrogateBegin != leadSurrogateEnd)
            return std::nullopt;

        if (!commonLeadSurrogate)
            commonLeadSurrogate = leadSurrogateBegin;
        else if (leadSurrogateBegin != commonLeadSurrogate.value())
            return std::nullopt;
    }

    return commonLeadSurrogate;
}

} } // namespace JSC::Yarr

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
