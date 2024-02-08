/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2011 the V8 project authors. All rights reserved.
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

#pragma once

#include <limits>
#include <wtf/text/StringCommon.h>
#include <wtf/text/StringView.h>

namespace WTF {

//---------------------------------------------------------------------
// String Search object.
//---------------------------------------------------------------------

// Class holding constants and methods that apply to all string search variants,
// independently of subject and pattern char size.
class AdaptiveStringSearcherBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // Cap on the maximal shift in the Boyer-Moore implementation. By setting a
    // limit, we can fix the size of tables. For a needle longer than this limit,
    // search will not be optimal, since we only build tables for a suffix
    // of the string, but it is a safe approximation.
    static constexpr int bmMaxShift = 250;

    // Reduce alphabet to this size.
    // One of the tables used by Boyer-Moore and Boyer-Moore-Horspool has size
    // proportional to the input alphabet. We reduce the alphabet size by
    // equating input characters modulo a smaller alphabet size. This gives
    // a potentially less efficient searching, but is a safe approximation.
    // For needles using only characters in the same Unicode 256-code point page,
    // there is no search speed degradation.
    static constexpr int latin1AlphabetSize = 256;
    static constexpr int ucharAlphabetSize = 256;

    // Bad-char shift table stored in the state. It's length is the alphabet size.
    // For patterns below this length, the skip length of Boyer-Moore is too short
    // to compensate for the algorithmic overhead compared to simple brute force.
    static constexpr int bmMinPatternLength = 7;

    static constexpr bool exceedsOneByte(LChar) { return false; }
    static constexpr bool exceedsOneByte(UChar c) { return c > 0xff; }

    template <typename T, typename U>
    static inline T alignDown(T value, U alignment)
    {
        return reinterpret_cast<T>((reinterpret_cast<uintptr_t>(value) & ~(alignment - 1)));
    }

    static constexpr uint8_t getHighestValueByte(LChar character) { return character; }

    static constexpr uint8_t getHighestValueByte(UChar character)
    {
        return std::max<unsigned>(static_cast<uint8_t>(character & 0xFF), static_cast<uint8_t>(character >> 8));
    }

    template <typename PatternChar, typename SubjectChar>
    static inline int findFirstCharacter(std::span<const PatternChar> pattern, std::span<const SubjectChar> subject, int index)
    {
        const auto* subjectPtr = subject.data();
        const PatternChar patternFirstChar = pattern[0];
        const int maxN = (subject.size() - pattern.size() + 1);

        if (sizeof(SubjectChar) == 2 && !patternFirstChar) {
            // Special-case looking for the 0 char in other than one-byte strings.
            // memchr mostly fails in this case due to every other byte being 0 in text
            // that is mostly ascii characters.
            for (int i = index; i < maxN; ++i) {
                if (!subjectPtr[i])
                    return i;
            }
            return -1;
        }
        const uint8_t searchByte = getHighestValueByte(patternFirstChar);
        const SubjectChar searchChar = static_cast<SubjectChar>(patternFirstChar);
        int pos = index;
        do {
            ASSERT(maxN - pos >= 0);
            const SubjectChar* charPos = reinterpret_cast<const SubjectChar*>(memchr(subjectPtr + pos, searchByte, (maxN - pos) * sizeof(SubjectChar)));
            if (charPos == nullptr)
                return -1;
            charPos = alignDown(charPos, sizeof(SubjectChar));
            pos = static_cast<int>(charPos - subjectPtr);
            if (subjectPtr[pos] == searchChar)
                return pos;
        } while (++pos < maxN);

        return -1;
    }
};

class AdaptiveStringSearcherTables {
    WTF_MAKE_FAST_ALLOCATED;
public:
    int* badCharShiftTable() { return m_badCharShiftTable.data(); }
    int* goodSuffixShiftTable() { return m_goodSuffixShiftTable.data(); }
    int* suffixTable() { return m_suffixTable.data(); }

private:
    std::array<int, AdaptiveStringSearcherBase::ucharAlphabetSize> m_badCharShiftTable { };
    std::array<int, AdaptiveStringSearcherBase::bmMaxShift> m_goodSuffixShiftTable { };
    std::array<int, AdaptiveStringSearcherBase::bmMaxShift + 1> m_suffixTable { };
};

template <typename PatternChar, typename SubjectChar>
class AdaptiveStringSearcher : private AdaptiveStringSearcherBase {
public:
    AdaptiveStringSearcher(AdaptiveStringSearcherTables& tables, std::span<const PatternChar> pattern)
        : m_tables(tables)
        , m_pattern(pattern)
        , m_start(std::max<int>(0, pattern.size() - bmMaxShift))
    {
        if (sizeof(PatternChar) > sizeof(SubjectChar)) {
            if (!charactersAreAllLatin1(m_pattern.data(), m_pattern.size())) {
                m_strategy = &failSearch;
                return;
            }
        }
        int patternLength = m_pattern.size();
        if (patternLength < bmMinPatternLength) {
            if (patternLength == 1) {
                m_strategy = &singleCharSearch;
                return;
            }
            m_strategy = &linearSearch;
            return;
        }
        m_strategy = &initialSearch;
    }

    int search(std::span<const SubjectChar> subject, int index)
    {
        return m_strategy(*this, subject, index);
    }

    static constexpr int alphabetSize()
    {
        if constexpr (sizeof(PatternChar) == 1) {
            // Latin1 needle.
            return latin1AlphabetSize;
        } else {
            ASSERT_UNDER_CONSTEXPR_CONTEXT(sizeof(PatternChar) == 2);
            // UC16 needle.
            return ucharAlphabetSize;
        }
    }

private:
    using SearchFunction = int (*)(AdaptiveStringSearcher<PatternChar, SubjectChar>&, std::span<const SubjectChar>, int);

    static int failSearch(AdaptiveStringSearcher<PatternChar, SubjectChar>&, std::span<const SubjectChar>, int)
    {
        return -1;
    }

    static int singleCharSearch(AdaptiveStringSearcher<PatternChar, SubjectChar>&, std::span<const SubjectChar>, int startIndex);

    static int linearSearch(AdaptiveStringSearcher<PatternChar, SubjectChar>&, std::span<const SubjectChar>, int startIndex);

    static int initialSearch(AdaptiveStringSearcher<PatternChar, SubjectChar>&, std::span<const SubjectChar>, int startIndex);

    static int boyerMooreHorspoolSearch(AdaptiveStringSearcher<PatternChar, SubjectChar>&, std::span<const SubjectChar>, int startIndex);

    static int boyerMooreSearch(AdaptiveStringSearcher<PatternChar, SubjectChar>&, std::span<const SubjectChar>, int startIndex);

    void populateBoyerMooreHorspoolTable();

    void populateBoyerMooreTable();

    static inline int charOccurrence(int* badCharOccurrence, SubjectChar charCode)
    {
        if constexpr (sizeof(SubjectChar) == 1)
            return badCharOccurrence[static_cast<int>(charCode)];

        if constexpr (sizeof(PatternChar) == 1) {
            if (exceedsOneByte(charCode))
                return -1;
            return badCharOccurrence[static_cast<unsigned>(charCode)];
        }
        // Both pattern and subject are UC16. Reduce character to equivalence
        // class.
        int equivClass = charCode % ucharAlphabetSize;
        return badCharOccurrence[equivClass];
    }

    // The following tables are shared by all searches.
    // TODO(lrn): Introduce a way for a pattern to keep its tables
    // between searches (e.g., for an Atom RegExp).

    // Store for the BoyerMoore(Horspool) bad char shift table.
    // Return a table covering the last bmMaxShift+1 positions of
    // pattern.
    int* badCharTable() { return m_tables.badCharShiftTable(); }

    // Store for the BoyerMoore good suffix shift table.
    int* goodSuffixShiftTable()
    {
        // Return biased pointer that maps the range  [m_start..m_pattern.size()
        // to the kGoodSuffixShiftTable array.
        return m_tables.goodSuffixShiftTable() - m_start;
    }

    // Table used temporarily while building the BoyerMoore good suffix
    // shift table.
    int* suffixTable()
    {
        // Return biased pointer that maps the range  [m_start..m_pattern.size()
        // to the kSuffixTable array.
        return m_tables.suffixTable() - m_start;
    }

    AdaptiveStringSearcherTables& m_tables;
    // The pattern to search for.
    std::span<const PatternChar> m_pattern;
    // Pointer to implementation of the search.
    SearchFunction m_strategy;
    // Cache value of max(0, pattern_size() - bmMaxShift)
    int m_start;
};

//---------------------------------------------------------------------
// Single Character Pattern Search Strategy
//---------------------------------------------------------------------

template <typename PatternChar, typename SubjectChar>
int AdaptiveStringSearcher<PatternChar, SubjectChar>::singleCharSearch(AdaptiveStringSearcher<PatternChar, SubjectChar>& search, std::span<const SubjectChar> subject, int index)
{
    ASSERT(search.m_pattern.size() == 1);
    PatternChar patternFirstChar = search.m_pattern[0];
    if constexpr (sizeof(PatternChar) > sizeof(SubjectChar)) {
        if (exceedsOneByte(patternFirstChar))
            return -1;
    }
    return findFirstCharacter(search.m_pattern, subject, index);
}

//---------------------------------------------------------------------
// Linear Search Strategy
//---------------------------------------------------------------------

// Simple linear search for short patterns. Never bails out.
template <typename PatternChar, typename SubjectChar>
int AdaptiveStringSearcher<PatternChar, SubjectChar>::linearSearch(AdaptiveStringSearcher<PatternChar, SubjectChar>& search, std::span<const SubjectChar> subject, int index)
{
    auto charCompare = [](const PatternChar* pattern, const SubjectChar* subject, int length) ALWAYS_INLINE_LAMBDA {
        ASSERT(length > 0);
        int pos = 0;
        do {
            if (pattern[pos] != subject[pos])
                return false;
            pos++;
        } while (pos < length);
        return true;
    };

    std::span<const PatternChar> pattern = search.m_pattern;
    ASSERT(pattern.size() > 1);
    int patternLength = pattern.size();
    int i = index;
    int n = subject.size() - patternLength;
    while (i <= n) {
        i = findFirstCharacter(pattern, subject, i);
        if (i == -1)
            return -1;
        ASSERT(i <= n);
        i++;
        // Loop extracted to separate function to allow using return to do
        // a deeper break.
        if (charCompare(pattern.data() + 1, subject.data() + i, patternLength - 1))
            return i - 1;
    }
    return -1;
}

//---------------------------------------------------------------------
// Boyer-Moore string search
//---------------------------------------------------------------------

template <typename PatternChar, typename SubjectChar>
int AdaptiveStringSearcher<PatternChar, SubjectChar>::boyerMooreSearch(AdaptiveStringSearcher<PatternChar, SubjectChar>& search, std::span<const SubjectChar> subject, int startIndex)
{
    std::span<const PatternChar> pattern = search.m_pattern;
    const auto* subjectPtr = subject.data();
    const auto* patternPtr = pattern.data();
    int subjectLength = subject.size();
    int patternLength = pattern.size();
    // Only preprocess at most bmMaxShift last characters of pattern.
    int start = search.m_start;

    int* badCharOccurrence = search.badCharTable();
    int* goodSuffixShift = search.goodSuffixShiftTable();

    PatternChar lastChar = patternPtr[patternLength - 1];
    int index = startIndex;
    // Continue search from i.
    while (index <= subjectLength - patternLength) {
        int j = patternLength - 1;
        int c;
        while (lastChar != (c = subjectPtr[index + j])) {
            int shift = j - charOccurrence(badCharOccurrence, c);
            index += shift;
            if (index > subjectLength - patternLength)
                return -1;
        }
        while (j >= 0 && patternPtr[j] == (c = subjectPtr[index + j]))
            j--;
        if (j < 0)
            return index;
        if (j < start) {
            // we have matched more than our tables allow us to be smart about.
            // Fall back on BMH shift.
            index += patternLength - 1 - charOccurrence(badCharOccurrence, static_cast<SubjectChar>(lastChar));
        } else {
            int gsShift = goodSuffixShift[j + 1];
            int bcOcc = charOccurrence(badCharOccurrence, c);
            int shift = j - bcOcc;
            if (gsShift > shift)
                shift = gsShift;
            index += shift;
        }
    }

    return -1;
}

template <typename PatternChar, typename SubjectChar>
void AdaptiveStringSearcher<PatternChar, SubjectChar>::populateBoyerMooreTable()
{
    const auto* patternPtr = m_pattern.data();
    int patternLength = m_pattern.size();
    // Only look at the last bmMaxShift characters of pattern (from m_start
    // to patternLength).
    int start = m_start;
    int length = patternLength - start;

    // Biased tables so that we can use pattern indices as table indices,
    // even if we only cover the part of the pattern from offset start.
    int* shiftTable = goodSuffixShiftTable();
    int* suffixTable = this->suffixTable();

    // Initialize table.
    for (int i = start; i < patternLength; i++)
        shiftTable[i] = length;
    shiftTable[patternLength] = 1;
    suffixTable[patternLength] = patternLength + 1;

    if (patternLength <= start)
        return;

    // Find suffixes.
    PatternChar lastChar = patternPtr[patternLength - 1];
    int suffix = patternLength + 1;
    {
        int i = patternLength;
        while (i > start) {
            PatternChar c = patternPtr[i - 1];
            while (suffix <= patternLength && c != patternPtr[suffix - 1]) {
                if (shiftTable[suffix] == length)
                    shiftTable[suffix] = suffix - i;
                suffix = suffixTable[suffix];
            }
            suffixTable[--i] = --suffix;
            if (suffix == patternLength) {
                // No suffix to extend, so we check against lastChar only.
                while ((i > start) && (patternPtr[i - 1] != lastChar)) {
                    if (shiftTable[patternLength] == length)
                        shiftTable[patternLength] = patternLength - i;
                    suffixTable[--i] = patternLength;
                }
                if (i > start)
                    suffixTable[--i] = --suffix;
            }
        }
    }
    // Build shift table using suffixes.
    if (suffix < patternLength) {
        for (int i = start; i <= patternLength; i++) {
            if (shiftTable[i] == length)
                shiftTable[i] = suffix - start;
            if (i == suffix)
                suffix = suffixTable[suffix];
        }
    }
}

//---------------------------------------------------------------------
// Boyer-Moore-Horspool string search.
//---------------------------------------------------------------------

template <typename PatternChar, typename SubjectChar>
int AdaptiveStringSearcher<PatternChar, SubjectChar>::boyerMooreHorspoolSearch(AdaptiveStringSearcher<PatternChar, SubjectChar>& search, std::span<const SubjectChar> subject, int startIndex)
{
    std::span<const PatternChar> pattern = search.m_pattern;
    const auto* subjectPtr = subject.data();
    const auto* patternPtr = pattern.data();
    int subjectLength = subject.size();
    int patternLength = pattern.size();
    int* charOccurrences = search.badCharTable();
    int badness = -patternLength;

    // How bad we are doing without a good-suffix table.
    PatternChar lastChar = patternPtr[patternLength - 1];
    int lastCharShift = patternLength - 1 - charOccurrence(charOccurrences, static_cast<SubjectChar>(lastChar));
    // Perform search
    int index = startIndex; // No matches found prior to this index.
    while (index <= subjectLength - patternLength) {
        int j = patternLength - 1;
        int subjectChar;
        while (lastChar != (subjectChar = subjectPtr[index + j])) {
            int bcOcc = charOccurrence(charOccurrences, subjectChar);
            int shift = j - bcOcc;
            index += shift;
            badness += 1 - shift; // at most zero, so badness cannot increase.
            if (index > subjectLength - patternLength)
                return -1;
        }
        j--;
        while (j >= 0 && patternPtr[j] == (subjectPtr[index + j]))
            j--;
        if (j < 0)
            return index;

        index += lastCharShift;
        // Badness increases by the number of characters we have
        // checked, and decreases by the number of characters we
        // can skip by shifting. It's a measure of how we are doing
        // compared to reading each character exactly once.
        badness += (patternLength - j) - lastCharShift;
        if (badness > 0) {
            search.populateBoyerMooreTable();
            search.m_strategy = &boyerMooreSearch;
            return boyerMooreSearch(search, subject, index);
        }
    }
    return -1;
}

template <typename PatternChar, typename SubjectChar>
void AdaptiveStringSearcher<PatternChar, SubjectChar>::populateBoyerMooreHorspoolTable()
{
    int patternLength = m_pattern.size();

    int* badCharOccurrence = badCharTable();

    // Only preprocess at most bmMaxShift last characters of pattern.
    int start = m_start;
    // Run forwards to populate badCharTable, so that *last* instance
    // of character equivalence class is the one registered.
    // Notice: Doesn't include the last character.
    int tableSize = alphabetSize();
    if (!start) // All patterns less than bmMaxShift in length.
        memset(badCharOccurrence, -1, tableSize * sizeof(*badCharOccurrence));
    else {
        for (int i = 0; i < tableSize; i++)
            badCharOccurrence[i] = start - 1;
    }
    const auto* patternPtr = m_pattern.data();
    for (int i = start; i < patternLength - 1; i++) {
        PatternChar c = patternPtr[i];
        int bucket = (sizeof(PatternChar) == 1) ? c : c % alphabetSize();
        badCharOccurrence[bucket] = i;
    }
}

//---------------------------------------------------------------------
// Linear string search with bailout to BMH.
//---------------------------------------------------------------------

// Simple linear search for short patterns, which bails out if the string
// isn't found very early in the subject. Upgrades to BoyerMooreHorspool.
template <typename PatternChar, typename SubjectChar>
int AdaptiveStringSearcher<PatternChar, SubjectChar>::initialSearch(AdaptiveStringSearcher<PatternChar, SubjectChar>& search, std::span<const SubjectChar> subject, int index)
{
    std::span<const PatternChar> pattern = search.m_pattern;
    const auto* subjectPtr = subject.data();
    const auto* patternPtr = pattern.data();
    int patternLength = pattern.size();
    // Badness is a count of how much work we have done. When we have
    // done enough work we decide it's probably worth switching to a better
    // algorithm.
    int badness = -10 - (patternLength << 2);

    // We know our pattern is at least 2 characters, we cache the first so
    // the common case of the first character not matching is faster.
    for (int i = index, n = subject.size() - patternLength; i <= n; i++) {
        badness++;
        if (badness <= 0) {
            i = findFirstCharacter(pattern, subject, i);
            if (i == -1)
                return -1;
            ASSERT(i <= n);
            int j = 1;
            do {
                if (patternPtr[j] != subjectPtr[i + j])
                    break;
                j++;
            } while (j < patternLength);
            if (j == patternLength)
                return i;
            badness += j;
        } else {
            search.populateBoyerMooreHorspoolTable();
            search.m_strategy = &boyerMooreHorspoolSearch;
            return boyerMooreHorspoolSearch(search, subject, i);
        }
    }
    return -1;
}

// Perform a a single stand-alone search.
// If searching multiple times for the same pattern, a search
// object should be constructed once and the Search function then called
// for each search.
template <typename SubjectChar, typename PatternChar>
int searchString(AdaptiveStringSearcherTables& tables, std::span<const SubjectChar> subject, std::span<const PatternChar> pattern, int startIndex)
{
    AdaptiveStringSearcher<PatternChar, SubjectChar> search(tables, pattern);
    return search.search(subject, startIndex);
}

// A wrapper function around SearchString that wraps raw pointers to the subject
// and pattern as vectors before calling SearchString. Used from the
// StringIndexOf builtin.
template <typename SubjectChar, typename PatternChar>
intptr_t searchStringRaw(AdaptiveStringSearcherTables& tables, const SubjectChar* subjectPtr, int subjectLength, const PatternChar* patternPtr, int patternLength, int startIndex)
{
    std::span<const SubjectChar> subject(subjectPtr, subjectLength);
    std::span<const PatternChar> pattern(patternPtr, patternLength);
    return searchString(tables, subject, pattern, startIndex);
}

} // namespace WTF

using WTF::AdaptiveStringSearcher;
using WTF::AdaptiveStringSearcherTables;
using WTF::searchString;
using WTF::searchStringRaw;
