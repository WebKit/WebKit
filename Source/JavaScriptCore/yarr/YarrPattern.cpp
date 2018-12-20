/*
 * Copyright (C) 2009, 2013-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Peter Varga (pvarga@inf.u-szeged.hu), University of Szeged
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
#include "YarrPattern.h"

#include "Options.h"
#include "Yarr.h"
#include "YarrCanonicalize.h"
#include "YarrParser.h"
#include <wtf/DataLog.h>
#include <wtf/Optional.h>
#include <wtf/StackPointer.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

using namespace WTF;

namespace JSC { namespace Yarr {

#include "RegExpJitTables.h"

class CharacterClassConstructor {
public:
    CharacterClassConstructor(bool isCaseInsensitive, CanonicalMode canonicalMode)
        : m_isCaseInsensitive(isCaseInsensitive)
        , m_hasNonBMPCharacters(false)
        , m_anyCharacter(false)
        , m_canonicalMode(canonicalMode)
    {
    }
    
    void reset()
    {
        m_matches.clear();
        m_ranges.clear();
        m_matchesUnicode.clear();
        m_rangesUnicode.clear();
        m_hasNonBMPCharacters = false;
        m_anyCharacter = false;
    }

    void append(const CharacterClass* other)
    {
        for (size_t i = 0; i < other->m_matches.size(); ++i)
            addSorted(m_matches, other->m_matches[i]);
        for (size_t i = 0; i < other->m_ranges.size(); ++i)
            addSortedRange(m_ranges, other->m_ranges[i].begin, other->m_ranges[i].end);
        for (size_t i = 0; i < other->m_matchesUnicode.size(); ++i)
            addSorted(m_matchesUnicode, other->m_matchesUnicode[i]);
        for (size_t i = 0; i < other->m_rangesUnicode.size(); ++i)
            addSortedRange(m_rangesUnicode, other->m_rangesUnicode[i].begin, other->m_rangesUnicode[i].end);
    }

    void appendInverted(const CharacterClass* other)
    {
        auto addSortedInverted = [&](UChar32 min, UChar32 max,
            const Vector<UChar32>& srcMatches, const Vector<CharacterRange>& srcRanges,
            Vector<UChar32>& destMatches, Vector<CharacterRange>& destRanges) {

            auto addSortedMatchOrRange = [&](UChar32 lo, UChar32 hiPlusOne) {
                if (lo < hiPlusOne) {
                    if (lo + 1 == hiPlusOne)
                        addSorted(destMatches, lo);
                    else
                        addSortedRange(destRanges, lo, hiPlusOne - 1);
                }
            };

            UChar32 lo = min;
            size_t matchesIndex = 0;
            size_t rangesIndex = 0;
            bool matchesRemaining = matchesIndex < srcMatches.size();
            bool rangesRemaining = rangesIndex < srcRanges.size();

            if (!matchesRemaining && !rangesRemaining) {
                addSortedMatchOrRange(min, max + 1);
                return;
            }

            while (matchesRemaining || rangesRemaining) {
                UChar32 hiPlusOne;
                UChar32 nextLo;

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

        addSortedInverted(0, 0x7f, other->m_matches, other->m_ranges, m_matches, m_ranges);
        addSortedInverted(0x80, 0x10ffff, other->m_matchesUnicode, other->m_rangesUnicode, m_matchesUnicode, m_rangesUnicode);
    }

    void putChar(UChar32 ch)
    {
        if (!m_isCaseInsensitive) {
            addSorted(ch);
            return;
        }

        if (m_canonicalMode == CanonicalMode::UCS2 && isASCII(ch)) {
            // Handle ASCII cases.
            if (isASCIIAlpha(ch)) {
                addSorted(m_matches, toASCIIUpper(ch));
                addSorted(m_matches, toASCIILower(ch));
            } else
                addSorted(m_matches, ch);
            return;
        }

        // Add multiple matches, if necessary.
        const CanonicalizationRange* info = canonicalRangeInfoFor(ch, m_canonicalMode);
        if (info->type == CanonicalizeUnique)
            addSorted(ch);
        else
            putUnicodeIgnoreCase(ch, info);
    }

    void putUnicodeIgnoreCase(UChar32 ch, const CanonicalizationRange* info)
    {
        ASSERT(m_isCaseInsensitive);
        ASSERT(ch >= info->begin && ch <= info->end);
        ASSERT(info->type != CanonicalizeUnique);
        if (info->type == CanonicalizeSet) {
            for (const UChar32* set = canonicalCharacterSetInfo(info->value, m_canonicalMode); (ch = *set); ++set)
                addSorted(ch);
        } else {
            addSorted(ch);
            addSorted(getCanonicalPair(info, ch));
        }
    }

    void putRange(UChar32 lo, UChar32 hi)
    {
        if (isASCII(lo)) {
            char asciiLo = lo;
            char asciiHi = std::min(hi, (UChar32)0x7f);
            addSortedRange(m_ranges, lo, asciiHi);
            
            if (m_isCaseInsensitive) {
                if ((asciiLo <= 'Z') && (asciiHi >= 'A'))
                    addSortedRange(m_ranges, std::max(asciiLo, 'A')+('a'-'A'), std::min(asciiHi, 'Z')+('a'-'A'));
                if ((asciiLo <= 'z') && (asciiHi >= 'a'))
                    addSortedRange(m_ranges, std::max(asciiLo, 'a')+('A'-'a'), std::min(asciiHi, 'z')+('A'-'a'));
            }
        }
        if (isASCII(hi))
            return;

        lo = std::max(lo, (UChar32)0x80);
        addSortedRange(m_rangesUnicode, lo, hi);
        
        if (!m_isCaseInsensitive)
            return;

        const CanonicalizationRange* info = canonicalRangeInfoFor(lo, m_canonicalMode);
        while (true) {
            // Handle the range [lo .. end]
            UChar32 end = std::min<UChar32>(info->end, hi);

            switch (info->type) {
            case CanonicalizeUnique:
                // Nothing to do - no canonical equivalents.
                break;
            case CanonicalizeSet: {
                UChar ch;
                for (const UChar32* set = canonicalCharacterSetInfo(info->value, m_canonicalMode); (ch = *set); ++set)
                    addSorted(m_matchesUnicode, ch);
                break;
            }
            case CanonicalizeRangeLo:
                addSortedRange(m_rangesUnicode, lo + info->value, end + info->value);
                break;
            case CanonicalizeRangeHi:
                addSortedRange(m_rangesUnicode, lo - info->value, end - info->value);
                break;
            case CanonicalizeAlternatingAligned:
                // Use addSortedRange since there is likely an abutting range to combine with.
                if (lo & 1)
                    addSortedRange(m_rangesUnicode, lo - 1, lo - 1);
                if (!(end & 1))
                    addSortedRange(m_rangesUnicode, end + 1, end + 1);
                break;
            case CanonicalizeAlternatingUnaligned:
                // Use addSortedRange since there is likely an abutting range to combine with.
                if (!(lo & 1))
                    addSortedRange(m_rangesUnicode, lo - 1, lo - 1);
                if (end & 1)
                    addSortedRange(m_rangesUnicode, end + 1, end + 1);
                break;
            }

            if (hi == end)
                return;

            ++info;
            lo = info->begin;
        };

    }

    std::unique_ptr<CharacterClass> charClass()
    {
        coalesceTables();

        auto characterClass = std::make_unique<CharacterClass>();

        characterClass->m_matches.swap(m_matches);
        characterClass->m_ranges.swap(m_ranges);
        characterClass->m_matchesUnicode.swap(m_matchesUnicode);
        characterClass->m_rangesUnicode.swap(m_rangesUnicode);
        characterClass->m_hasNonBMPCharacters = hasNonBMPCharacters();
        characterClass->m_anyCharacter = anyCharacter();

        m_hasNonBMPCharacters = false;
        m_anyCharacter = false;

        return characterClass;
    }

private:
    void addSorted(UChar32 ch)
    {
        addSorted(isASCII(ch) ? m_matches : m_matchesUnicode, ch);
    }

    void addSorted(Vector<UChar32>& matches, UChar32 ch)
    {
        unsigned pos = 0;
        unsigned range = matches.size();

        if (!U_IS_BMP(ch))
            m_hasNonBMPCharacters = true;

        // binary chop, find position to insert char.
        while (range) {
            unsigned index = range >> 1;

            int val = matches[pos+index] - ch;
            if (!val)
                return;
            else if (val > 0) {
                if (val == 1) {
                    UChar32 lo = ch;
                    UChar32 hi = ch + 1;
                    matches.remove(pos + index);
                    if (pos + index > 0 && matches[pos + index - 1] == ch - 1) {
                        lo = ch - 1;
                        matches.remove(pos + index - 1);
                    }
                    addSortedRange(isASCII(ch) ? m_ranges : m_rangesUnicode, lo, hi);
                    return;
                }
                range = index;
            } else {
                if (val == -1) {
                    UChar32 lo = ch - 1;
                    UChar32 hi = ch;
                    matches.remove(pos + index);
                    if (pos + index + 1 < matches.size() && matches[pos + index + 1] == ch + 1) {
                        hi = ch + 1;
                        matches.remove(pos + index + 1);
                    }
                    addSortedRange(isASCII(ch) ? m_ranges : m_rangesUnicode, lo, hi);
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

    void addSortedRange(Vector<CharacterRange>& ranges, UChar32 lo, UChar32 hi)
    {
        size_t end = ranges.size();

        if (!U_IS_BMP(hi))
            m_hasNonBMPCharacters = true;

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

    void mergeRangesFrom(Vector<CharacterRange>& ranges, size_t index)
    {
        unsigned next = index + 1;

        // each iteration of the loop we will either remove something from the list, or break out of the loop.
        while (next < ranges.size()) {
            if (ranges[next].begin <= (ranges[index].end + 1)) {
                // the next entry now overlaps / concatenates with this one.
                ranges[index].end = std::max(ranges[index].end, ranges[next].end);
                ranges.remove(next);
            } else
                break;
        }

    }

    void coalesceTables()
    {
        auto coalesceMatchesAndRanges = [&](Vector<UChar32>& matches, Vector<CharacterRange>& ranges) {

            size_t matchesIndex = 0;
            size_t rangesIndex = 0;

            while (matchesIndex < matches.size() && rangesIndex < ranges.size()) {
                while (matchesIndex < matches.size() && matches[matchesIndex] < ranges[rangesIndex].begin - 1)
                    matchesIndex++;

                if (matchesIndex < matches.size() && matches[matchesIndex] == ranges[rangesIndex].begin - 1) {
                    ranges[rangesIndex].begin = matches[matchesIndex];
                    matches.remove(matchesIndex);
                }

                while (matchesIndex < matches.size() && matches[matchesIndex] < ranges[rangesIndex].end + 1)
                    matchesIndex++;

                if (matchesIndex < matches.size()) {
                    if (matches[matchesIndex] == ranges[rangesIndex].end + 1) {
                        ranges[rangesIndex].end = matches[matchesIndex];
                        matches.remove(matchesIndex);

                        mergeRangesFrom(ranges, rangesIndex);
                    } else
                        matchesIndex++;
                }
            }
        };

        coalesceMatchesAndRanges(m_matches, m_ranges);
        coalesceMatchesAndRanges(m_matchesUnicode, m_rangesUnicode);

        if (!m_matches.size() && !m_matchesUnicode.size()
            && m_ranges.size() == 1 && m_rangesUnicode.size() == 1
            && m_ranges[0].begin == 0 && m_ranges[0].end == 0x7f
            && m_rangesUnicode[0].begin == 0x80 && m_rangesUnicode[0].end == 0x10ffff)
            m_anyCharacter = true;
    }

    bool hasNonBMPCharacters()
    {
        return m_hasNonBMPCharacters;
    }

    bool anyCharacter()
    {
        return m_anyCharacter;
    }

    bool m_isCaseInsensitive : 1;
    bool m_hasNonBMPCharacters : 1;
    bool m_anyCharacter : 1;
    CanonicalMode m_canonicalMode;

    Vector<UChar32> m_matches;
    Vector<CharacterRange> m_ranges;
    Vector<UChar32> m_matchesUnicode;
    Vector<CharacterRange> m_rangesUnicode;
};

class YarrPatternConstructor {
public:
    YarrPatternConstructor(YarrPattern& pattern, void* stackLimit)
        : m_pattern(pattern)
        , m_characterClassConstructor(pattern.ignoreCase(), pattern.unicode() ? CanonicalMode::Unicode : CanonicalMode::UCS2)
        , m_stackLimit(stackLimit)
    {
        auto body = std::make_unique<PatternDisjunction>();
        m_pattern.m_body = body.get();
        m_alternative = body->addNewAlternative();
        m_pattern.m_disjunctions.append(WTFMove(body));
    }

    ~YarrPatternConstructor()
    {
    }

    void resetForReparsing()
    {
        m_pattern.resetForReparsing();
        m_characterClassConstructor.reset();

        auto body = std::make_unique<PatternDisjunction>();
        m_pattern.m_body = body.get();
        m_alternative = body->addNewAlternative();
        m_pattern.m_disjunctions.append(WTFMove(body));
    }

    void saveUnmatchedNamedForwardReferences()
    {
        m_unmatchedNamedForwardReferences.shrink(0);
        
        for (auto& entry : m_pattern.m_namedForwardReferences) {
            if (!m_pattern.m_captureGroupNames.contains(entry))
                m_unmatchedNamedForwardReferences.append(entry);
        }
    }

    void assertionBOL()
    {
        if (!m_alternative->m_terms.size() && !m_invertParentheticalAssertion) {
            m_alternative->m_startsWithBOL = true;
            m_alternative->m_containsBOL = true;
            m_pattern.m_containsBOL = true;
        }
        m_alternative->m_terms.append(PatternTerm::BOL());
    }
    void assertionEOL()
    {
        m_alternative->m_terms.append(PatternTerm::EOL());
    }
    void assertionWordBoundary(bool invert)
    {
        m_alternative->m_terms.append(PatternTerm::WordBoundary(invert));
    }

    void atomPatternCharacter(UChar32 ch)
    {
        // We handle case-insensitive checking of unicode characters which do have both
        // cases by handling them as if they were defined using a CharacterClass.
        if (!m_pattern.ignoreCase() || (isASCII(ch) && !m_pattern.unicode())) {
            m_alternative->m_terms.append(PatternTerm(ch));
            return;
        }

        const CanonicalizationRange* info = canonicalRangeInfoFor(ch, m_pattern.unicode() ? CanonicalMode::Unicode : CanonicalMode::UCS2);
        if (info->type == CanonicalizeUnique) {
            m_alternative->m_terms.append(PatternTerm(ch));
            return;
        }

        m_characterClassConstructor.putUnicodeIgnoreCase(ch, info);
        auto newCharacterClass = m_characterClassConstructor.charClass();
        m_alternative->m_terms.append(PatternTerm(newCharacterClass.get(), false));
        m_pattern.m_userCharacterClasses.append(WTFMove(newCharacterClass));
    }

    void atomBuiltInCharacterClass(BuiltInCharacterClassID classID, bool invert)
    {
        switch (classID) {
        case BuiltInCharacterClassID::DigitClassID:
            m_alternative->m_terms.append(PatternTerm(m_pattern.digitsCharacterClass(), invert));
            break;
        case BuiltInCharacterClassID::SpaceClassID:
            m_alternative->m_terms.append(PatternTerm(m_pattern.spacesCharacterClass(), invert));
            break;
        case BuiltInCharacterClassID::WordClassID:
            if (m_pattern.unicode() && m_pattern.ignoreCase())
                m_alternative->m_terms.append(PatternTerm(m_pattern.wordUnicodeIgnoreCaseCharCharacterClass(), invert));
            else
                m_alternative->m_terms.append(PatternTerm(m_pattern.wordcharCharacterClass(), invert));
            break;
        case BuiltInCharacterClassID::DotClassID:
            ASSERT(!invert);
            if (m_pattern.dotAll())
                m_alternative->m_terms.append(PatternTerm(m_pattern.anyCharacterClass(), false));
            else
                m_alternative->m_terms.append(PatternTerm(m_pattern.newlineCharacterClass(), true));
            break;
        default:
            m_alternative->m_terms.append(PatternTerm(m_pattern.unicodeCharacterClassFor(classID), invert));
            break;
        }
    }

    void atomCharacterClassBegin(bool invert = false)
    {
        m_invertCharacterClass = invert;
    }

    void atomCharacterClassAtom(UChar32 ch)
    {
        m_characterClassConstructor.putChar(ch);
    }

    void atomCharacterClassRange(UChar32 begin, UChar32 end)
    {
        m_characterClassConstructor.putRange(begin, end);
    }

    void atomCharacterClassBuiltIn(BuiltInCharacterClassID classID, bool invert)
    {
        ASSERT(classID != BuiltInCharacterClassID::DotClassID);

        switch (classID) {
        case BuiltInCharacterClassID::DigitClassID:
            m_characterClassConstructor.append(invert ? m_pattern.nondigitsCharacterClass() : m_pattern.digitsCharacterClass());
            break;
        
        case BuiltInCharacterClassID::SpaceClassID:
            m_characterClassConstructor.append(invert ? m_pattern.nonspacesCharacterClass() : m_pattern.spacesCharacterClass());
            break;
        
        case BuiltInCharacterClassID::WordClassID:
            if (m_pattern.unicode() && m_pattern.ignoreCase())
                m_characterClassConstructor.append(invert ? m_pattern.nonwordUnicodeIgnoreCaseCharCharacterClass() : m_pattern.wordUnicodeIgnoreCaseCharCharacterClass());
            else
                m_characterClassConstructor.append(invert ? m_pattern.nonwordcharCharacterClass() : m_pattern.wordcharCharacterClass());
            break;
        
        default:
            if (!invert)
                m_characterClassConstructor.append(m_pattern.unicodeCharacterClassFor(classID));
            else
                m_characterClassConstructor.appendInverted(m_pattern.unicodeCharacterClassFor(classID));
        }
    }

    void atomCharacterClassEnd()
    {
        auto newCharacterClass = m_characterClassConstructor.charClass();

        if (!m_invertCharacterClass && newCharacterClass.get()->m_anyCharacter) {
            m_alternative->m_terms.append(PatternTerm(m_pattern.anyCharacterClass(), false));
            return;
        }
        m_alternative->m_terms.append(PatternTerm(newCharacterClass.get(), m_invertCharacterClass));
        m_pattern.m_userCharacterClasses.append(WTFMove(newCharacterClass));
    }

    void atomParenthesesSubpatternBegin(bool capture = true, Optional<String> optGroupName = WTF::nullopt)
    {
        unsigned subpatternId = m_pattern.m_numSubpatterns + 1;
        if (capture) {
            m_pattern.m_numSubpatterns++;
            if (optGroupName) {
                while (m_pattern.m_captureGroupNames.size() < subpatternId)
                    m_pattern.m_captureGroupNames.append(String());
                m_pattern.m_captureGroupNames.append(optGroupName.value());
                m_pattern.m_namedGroupToParenIndex.add(optGroupName.value(), subpatternId);
            }
        } else
            ASSERT(!optGroupName);

        auto parenthesesDisjunction = std::make_unique<PatternDisjunction>(m_alternative);
        m_alternative->m_terms.append(PatternTerm(PatternTerm::TypeParenthesesSubpattern, subpatternId, parenthesesDisjunction.get(), capture, false));
        m_alternative = parenthesesDisjunction->addNewAlternative();
        m_pattern.m_disjunctions.append(WTFMove(parenthesesDisjunction));
    }

    void atomParentheticalAssertionBegin(bool invert = false)
    {
        auto parenthesesDisjunction = std::make_unique<PatternDisjunction>(m_alternative);
        m_alternative->m_terms.append(PatternTerm(PatternTerm::TypeParentheticalAssertion, m_pattern.m_numSubpatterns + 1, parenthesesDisjunction.get(), false, invert));
        m_alternative = parenthesesDisjunction->addNewAlternative();
        m_invertParentheticalAssertion = invert;
        m_pattern.m_disjunctions.append(WTFMove(parenthesesDisjunction));
    }

    void atomParenthesesEnd()
    {
        ASSERT(m_alternative->m_parent);
        ASSERT(m_alternative->m_parent->m_parent);

        PatternDisjunction* parenthesesDisjunction = m_alternative->m_parent;
        m_alternative = m_alternative->m_parent->m_parent;

        PatternTerm& lastTerm = m_alternative->lastTerm();

        unsigned numParenAlternatives = parenthesesDisjunction->m_alternatives.size();
        unsigned numBOLAnchoredAlts = 0;

        for (unsigned i = 0; i < numParenAlternatives; i++) {
            // Bubble up BOL flags
            if (parenthesesDisjunction->m_alternatives[i]->m_startsWithBOL)
                numBOLAnchoredAlts++;
        }

        if (numBOLAnchoredAlts) {
            m_alternative->m_containsBOL = true;
            // If all the alternatives in parens start with BOL, then so does this one
            if (numBOLAnchoredAlts == numParenAlternatives)
                m_alternative->m_startsWithBOL = true;
        }

        lastTerm.parentheses.lastSubpatternId = m_pattern.m_numSubpatterns;
        m_invertParentheticalAssertion = false;
    }

    void atomBackReference(unsigned subpatternId)
    {
        ASSERT(subpatternId);
        m_pattern.m_containsBackreferences = true;
        m_pattern.m_maxBackReference = std::max(m_pattern.m_maxBackReference, subpatternId);

        if (subpatternId > m_pattern.m_numSubpatterns) {
            m_alternative->m_terms.append(PatternTerm::ForwardReference());
            return;
        }

        PatternAlternative* currentAlternative = m_alternative;
        ASSERT(currentAlternative);

        // Note to self: if we waited until the AST was baked, we could also remove forwards refs 
        while ((currentAlternative = currentAlternative->m_parent->m_parent)) {
            PatternTerm& term = currentAlternative->lastTerm();
            ASSERT((term.type == PatternTerm::TypeParenthesesSubpattern) || (term.type == PatternTerm::TypeParentheticalAssertion));

            if ((term.type == PatternTerm::TypeParenthesesSubpattern) && term.capture() && (subpatternId == term.parentheses.subpatternId)) {
                m_alternative->m_terms.append(PatternTerm::ForwardReference());
                return;
            }
        }

        m_alternative->m_terms.append(PatternTerm(subpatternId));
    }

    void atomNamedBackReference(const String& subpatternName)
    {
        ASSERT(m_pattern.m_namedGroupToParenIndex.find(subpatternName) != m_pattern.m_namedGroupToParenIndex.end());
        atomBackReference(m_pattern.m_namedGroupToParenIndex.get(subpatternName));
    }

    bool isValidNamedForwardReference(const String& subpatternName)
    {
        return !m_unmatchedNamedForwardReferences.contains(subpatternName);
    }

    void atomNamedForwardReference(const String& subpatternName)
    {
        m_pattern.m_namedForwardReferences.appendIfNotContains(subpatternName);
        m_alternative->m_terms.append(PatternTerm::ForwardReference());
    }
    
    // deep copy the argument disjunction.  If filterStartsWithBOL is true,
    // skip alternatives with m_startsWithBOL set true.
    PatternDisjunction* copyDisjunction(PatternDisjunction* disjunction, bool filterStartsWithBOL = false)
    {
        std::unique_ptr<PatternDisjunction> newDisjunction;
        for (unsigned alt = 0; alt < disjunction->m_alternatives.size(); ++alt) {
            PatternAlternative* alternative = disjunction->m_alternatives[alt].get();
            if (!filterStartsWithBOL || !alternative->m_startsWithBOL) {
                if (!newDisjunction) {
                    newDisjunction = std::make_unique<PatternDisjunction>();
                    newDisjunction->m_parent = disjunction->m_parent;
                }
                PatternAlternative* newAlternative = newDisjunction->addNewAlternative();
                newAlternative->m_terms.reserveInitialCapacity(alternative->m_terms.size());
                for (unsigned i = 0; i < alternative->m_terms.size(); ++i)
                    newAlternative->m_terms.append(copyTerm(alternative->m_terms[i], filterStartsWithBOL));
            }
        }
        
        if (!newDisjunction)
            return 0;

        PatternDisjunction* copiedDisjunction = newDisjunction.get();
        m_pattern.m_disjunctions.append(WTFMove(newDisjunction));
        return copiedDisjunction;
    }
    
    PatternTerm copyTerm(PatternTerm& term, bool filterStartsWithBOL = false)
    {
        if ((term.type != PatternTerm::TypeParenthesesSubpattern) && (term.type != PatternTerm::TypeParentheticalAssertion))
            return PatternTerm(term);
        
        PatternTerm termCopy = term;
        termCopy.parentheses.disjunction = copyDisjunction(termCopy.parentheses.disjunction, filterStartsWithBOL);
        m_pattern.m_hasCopiedParenSubexpressions = true;
        return termCopy;
    }
    
    void quantifyAtom(unsigned min, unsigned max, bool greedy)
    {
        ASSERT(min <= max);
        ASSERT(m_alternative->m_terms.size());

        if (!max) {
            m_alternative->removeLastTerm();
            return;
        }

        PatternTerm& term = m_alternative->lastTerm();
        ASSERT(term.type > PatternTerm::TypeAssertionWordBoundary);
        ASSERT(term.quantityMinCount == 1 && term.quantityMaxCount == 1 && term.quantityType == QuantifierFixedCount);

        if (term.type == PatternTerm::TypeParentheticalAssertion) {
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
            term.quantify(min, max, QuantifierFixedCount);
        else if (!min || (term.type == PatternTerm::TypeParenthesesSubpattern && m_pattern.m_hasCopiedParenSubexpressions))
            term.quantify(min, max, greedy ? QuantifierGreedy : QuantifierNonGreedy);
        else {
            term.quantify(min, min, QuantifierFixedCount);
            m_alternative->m_terms.append(copyTerm(term));
            // NOTE: this term is interesting from an analysis perspective, in that it can be ignored.....
            m_alternative->lastTerm().quantify((max == quantifyInfinite) ? max : max - min, greedy ? QuantifierGreedy : QuantifierNonGreedy);
            if (m_alternative->lastTerm().type == PatternTerm::TypeParenthesesSubpattern)
                m_alternative->lastTerm().parentheses.isCopy = true;
        }
    }

    void disjunction()
    {
        m_alternative = m_alternative->m_parent->addNewAlternative();
    }

    ErrorCode setupAlternativeOffsets(PatternAlternative* alternative, unsigned currentCallFrameSize, unsigned initialInputPosition, unsigned& newCallFrameSize) WARN_UNUSED_RETURN
    {
        if (UNLIKELY(!isSafeToRecurse()))
            return ErrorCode::TooManyDisjunctions;

        ErrorCode error = ErrorCode::NoError;
        alternative->m_hasFixedSize = true;
        Checked<unsigned, RecordOverflow> currentInputPosition = initialInputPosition;

        for (unsigned i = 0; i < alternative->m_terms.size(); ++i) {
            PatternTerm& term = alternative->m_terms[i];

            switch (term.type) {
            case PatternTerm::TypeAssertionBOL:
            case PatternTerm::TypeAssertionEOL:
            case PatternTerm::TypeAssertionWordBoundary:
                term.inputPosition = currentInputPosition.unsafeGet();
                break;

            case PatternTerm::TypeBackReference:
                term.inputPosition = currentInputPosition.unsafeGet();
                term.frameLocation = currentCallFrameSize;
                currentCallFrameSize += YarrStackSpaceForBackTrackInfoBackReference;
                alternative->m_hasFixedSize = false;
                break;

            case PatternTerm::TypeForwardReference:
                break;

            case PatternTerm::TypePatternCharacter:
                term.inputPosition = currentInputPosition.unsafeGet();
                if (term.quantityType != QuantifierFixedCount) {
                    term.frameLocation = currentCallFrameSize;
                    currentCallFrameSize += YarrStackSpaceForBackTrackInfoPatternCharacter;
                    alternative->m_hasFixedSize = false;
                } else if (m_pattern.unicode()) {
                    Checked<unsigned, RecordOverflow> tempCount = term.quantityMaxCount;
                    tempCount *= U16_LENGTH(term.patternCharacter);
                    if (tempCount.hasOverflowed())
                        return ErrorCode::OffsetTooLarge;
                    currentInputPosition += tempCount;
                } else
                    currentInputPosition += term.quantityMaxCount;
                break;

            case PatternTerm::TypeCharacterClass:
                term.inputPosition = currentInputPosition.unsafeGet();
                if (term.quantityType != QuantifierFixedCount) {
                    term.frameLocation = currentCallFrameSize;
                    currentCallFrameSize += YarrStackSpaceForBackTrackInfoCharacterClass;
                    alternative->m_hasFixedSize = false;
                } else if (m_pattern.unicode()) {
                    term.frameLocation = currentCallFrameSize;
                    currentCallFrameSize += YarrStackSpaceForBackTrackInfoCharacterClass;
                    currentInputPosition += term.quantityMaxCount;
                    alternative->m_hasFixedSize = false;
                } else
                    currentInputPosition += term.quantityMaxCount;
                break;

            case PatternTerm::TypeParenthesesSubpattern:
                // Note: for fixed once parentheses we will ensure at least the minimum is available; others are on their own.
                term.frameLocation = currentCallFrameSize;
                if (term.quantityMaxCount == 1 && !term.parentheses.isCopy) {
                    currentCallFrameSize += YarrStackSpaceForBackTrackInfoParenthesesOnce;
                    error = setupDisjunctionOffsets(term.parentheses.disjunction, currentCallFrameSize, currentInputPosition.unsafeGet(), currentCallFrameSize);
                    if (hasError(error))
                        return error;
                    // If quantity is fixed, then pre-check its minimum size.
                    if (term.quantityType == QuantifierFixedCount)
                        currentInputPosition += term.parentheses.disjunction->m_minimumSize;
                    term.inputPosition = currentInputPosition.unsafeGet();
                } else if (term.parentheses.isTerminal) {
                    currentCallFrameSize += YarrStackSpaceForBackTrackInfoParenthesesTerminal;
                    error = setupDisjunctionOffsets(term.parentheses.disjunction, currentCallFrameSize, currentInputPosition.unsafeGet(), currentCallFrameSize);
                    if (hasError(error))
                        return error;
                    term.inputPosition = currentInputPosition.unsafeGet();
                } else {
                    term.inputPosition = currentInputPosition.unsafeGet();
                    currentCallFrameSize += YarrStackSpaceForBackTrackInfoParentheses;
                    error = setupDisjunctionOffsets(term.parentheses.disjunction, currentCallFrameSize, currentInputPosition.unsafeGet(), currentCallFrameSize);
                    if (hasError(error))
                        return error;
                }
                // Fixed count of 1 could be accepted, if they have a fixed size *AND* if all alternatives are of the same length.
                alternative->m_hasFixedSize = false;
                break;

            case PatternTerm::TypeParentheticalAssertion:
                term.inputPosition = currentInputPosition.unsafeGet();
                term.frameLocation = currentCallFrameSize;
                error = setupDisjunctionOffsets(term.parentheses.disjunction, currentCallFrameSize + YarrStackSpaceForBackTrackInfoParentheticalAssertion, currentInputPosition.unsafeGet(), currentCallFrameSize);
                if (hasError(error))
                    return error;
                break;

            case PatternTerm::TypeDotStarEnclosure:
                ASSERT(!m_pattern.m_saveInitialStartValue);
                alternative->m_hasFixedSize = false;
                term.inputPosition = initialInputPosition;
                m_pattern.m_initialStartValueFrameLocation = currentCallFrameSize;
                currentCallFrameSize += YarrStackSpaceForDotStarEnclosure;
                m_pattern.m_saveInitialStartValue = true;
                break;
            }
            if (currentInputPosition.hasOverflowed())
                return ErrorCode::OffsetTooLarge;
        }

        alternative->m_minimumSize = (currentInputPosition - initialInputPosition).unsafeGet();
        newCallFrameSize = currentCallFrameSize;
        return error;
    }

    ErrorCode setupDisjunctionOffsets(PatternDisjunction* disjunction, unsigned initialCallFrameSize, unsigned initialInputPosition, unsigned& callFrameSize)
    {
        if (UNLIKELY(!isSafeToRecurse()))
            return ErrorCode::TooManyDisjunctions;

        if ((disjunction != m_pattern.m_body) && (disjunction->m_alternatives.size() > 1))
            initialCallFrameSize += YarrStackSpaceForBackTrackInfoAlternative;

        unsigned minimumInputSize = UINT_MAX;
        unsigned maximumCallFrameSize = 0;
        bool hasFixedSize = true;
        ErrorCode error = ErrorCode::NoError;

        for (unsigned alt = 0; alt < disjunction->m_alternatives.size(); ++alt) {
            PatternAlternative* alternative = disjunction->m_alternatives[alt].get();
            unsigned currentAlternativeCallFrameSize;
            error = setupAlternativeOffsets(alternative, initialCallFrameSize, initialInputPosition, currentAlternativeCallFrameSize);
            if (hasError(error))
                return error;
            minimumInputSize = std::min(minimumInputSize, alternative->m_minimumSize);
            maximumCallFrameSize = std::max(maximumCallFrameSize, currentAlternativeCallFrameSize);
            hasFixedSize &= alternative->m_hasFixedSize;
            if (alternative->m_minimumSize > INT_MAX)
                m_pattern.m_containsUnsignedLengthPattern = true;
        }
        
        ASSERT(minimumInputSize != UINT_MAX);
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
        unsigned ignoredCallFrameSize;
        return setupDisjunctionOffsets(m_pattern.m_body, 0, 0, ignoredCallFrameSize);
    }

    // This optimization identifies sets of parentheses that we will never need to backtrack.
    // In these cases we do not need to store state from prior iterations.
    // We can presently avoid backtracking for:
    //   * where the parens are at the end of the regular expression (last term in any of the
    //     alternatives of the main body disjunction).
    //   * where the parens are non-capturing, and quantified unbounded greedy (*).
    //   * where the parens do not contain any capturing subpatterns.
    void checkForTerminalParentheses()
    {
        // This check is much too crude; should be just checking whether the candidate
        // node contains nested capturing subpatterns, not the whole expression!
        if (m_pattern.m_numSubpatterns)
            return;

        Vector<std::unique_ptr<PatternAlternative>>& alternatives = m_pattern.m_body->m_alternatives;
        for (size_t i = 0; i < alternatives.size(); ++i) {
            Vector<PatternTerm>& terms = alternatives[i]->m_terms;
            if (terms.size()) {
                PatternTerm& term = terms.last();
                if (term.type == PatternTerm::TypeParenthesesSubpattern
                    && term.quantityType == QuantifierGreedy
                    && term.quantityMinCount == 0
                    && term.quantityMaxCount == quantifyInfinite
                    && !term.capture())
                    term.parentheses.isTerminal = true;
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
        
        if (!m_pattern.m_containsBOL || m_pattern.multiline())
            return;
        
        PatternDisjunction* loopDisjunction = copyDisjunction(disjunction, true);

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
        Vector<PatternTerm>& terms = alternative->m_terms;

        ASSERT(endIndex <= terms.size());
        for (size_t termIndex = firstTermIndex; termIndex < endIndex; ++termIndex) {
            PatternTerm& term = terms[termIndex];

            if (term.m_capture)
                return true;

            if (term.type == PatternTerm::TypeParenthesesSubpattern) {
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

        CharacterClass* dotCharacterClass = m_pattern.dotAll() ? m_pattern.anyCharacterClass() : m_pattern.newlineCharacterClass();
        PatternAlternative* alternative = alternatives[0].get();
        Vector<PatternTerm>& terms = alternative->m_terms;
        if (terms.size() >= 3) {
            bool startsWithBOL = false;
            bool endsWithEOL = false;
            size_t termIndex, firstExpressionTerm;

            termIndex = 0;
            if (terms[termIndex].type == PatternTerm::TypeAssertionBOL) {
                startsWithBOL = true;
                ++termIndex;
            }
            
            PatternTerm& firstNonAnchorTerm = terms[termIndex];
            if (firstNonAnchorTerm.type != PatternTerm::TypeCharacterClass
                || firstNonAnchorTerm.characterClass != dotCharacterClass
                || firstNonAnchorTerm.quantityMinCount
                || firstNonAnchorTerm.quantityMaxCount != quantifyInfinite)
                return;
            
            firstExpressionTerm = termIndex + 1;
            
            termIndex = terms.size() - 1;
            if (terms[termIndex].type == PatternTerm::TypeAssertionEOL) {
                endsWithEOL = true;
                --termIndex;
            }
            
            PatternTerm& lastNonAnchorTerm = terms[termIndex];
            if (lastNonAnchorTerm.type != PatternTerm::TypeCharacterClass
                || lastNonAnchorTerm.characterClass != dotCharacterClass
                || lastNonAnchorTerm.quantityType != QuantifierGreedy
                || lastNonAnchorTerm.quantityMinCount
                || lastNonAnchorTerm.quantityMaxCount != quantifyInfinite)
                return;

            size_t endIndex = termIndex;
            if (firstExpressionTerm >= endIndex)
                return;

            if (!containsCapturingTerms(alternative, firstExpressionTerm, endIndex)) {
                for (termIndex = terms.size() - 1; termIndex >= endIndex; --termIndex)
                    terms.remove(termIndex);

                for (termIndex = firstExpressionTerm; termIndex > 0; --termIndex)
                    terms.remove(termIndex - 1);

                terms.append(PatternTerm(startsWithBOL, endsWithEOL));
                
                m_pattern.m_containsBOL = false;
            }
        }
    }

private:
    bool isSafeToRecurse() const
    {
        if (!m_stackLimit)
            return true;
        ASSERT(Thread::current().stack().isGrowingDownward());
        int8_t* curr = reinterpret_cast<int8_t*>(currentStackPointer());
        int8_t* limit = reinterpret_cast<int8_t*>(m_stackLimit);
        return curr >= limit;
    }

    YarrPattern& m_pattern;
    PatternAlternative* m_alternative;
    CharacterClassConstructor m_characterClassConstructor;
    Vector<String> m_unmatchedNamedForwardReferences;
    void* m_stackLimit;
    bool m_invertCharacterClass;
    bool m_invertParentheticalAssertion { false };
};

ErrorCode YarrPattern::compile(const String& patternString, void* stackLimit)
{
    YarrPatternConstructor constructor(*this, stackLimit);

    if (m_flags == InvalidFlags)
        return ErrorCode::InvalidRegularExpressionFlags;

    {
        ErrorCode error = parse(constructor, patternString, unicode());
        if (hasError(error))
            return error;
    }
    
    // If the pattern contains illegal backreferences reset & reparse.
    // Quoting Netscape's "What's new in JavaScript 1.2",
    //      "Note: if the number of left parentheses is less than the number specified
    //       in \#, the \# is taken as an octal escape as described in the next row."
    if (containsIllegalBackReference() || containsIllegalNamedForwardReferences()) {
        if (unicode())
            return ErrorCode::InvalidBackreference;

        unsigned numSubpatterns = m_numSubpatterns;

        constructor.saveUnmatchedNamedForwardReferences();
        constructor.resetForReparsing();
        ErrorCode error = parse(constructor, patternString, unicode(), numSubpatterns);
        ASSERT_UNUSED(error, !hasError(error));
        ASSERT(numSubpatterns == m_numSubpatterns);
    }

    constructor.checkForTerminalParentheses();
    constructor.optimizeDotStarWrappedExpressions();
    constructor.optimizeBOL();

    {
        ErrorCode error = constructor.setupOffsets();
        if (hasError(error))
            return error;
    }

    if (Options::dumpCompiledRegExpPatterns())
        dumpPattern(patternString);

    return ErrorCode::NoError;
}

YarrPattern::YarrPattern(const String& pattern, RegExpFlags flags, ErrorCode& error, void* stackLimit)
    : m_containsBackreferences(false)
    , m_containsBOL(false)
    , m_containsUnsignedLengthPattern(false)
    , m_hasCopiedParenSubexpressions(false)
    , m_saveInitialStartValue(false)
    , m_flags(flags)
{
    error = compile(pattern, stackLimit);
}

void indentForNestingLevel(PrintStream& out, unsigned nestingDepth)
{
    out.print("    ");
    for (; nestingDepth; --nestingDepth)
        out.print("  ");
}

void dumpUChar32(PrintStream& out, UChar32 c)
{
    if (c >= ' '&& c <= 0xff)
        out.printf("'%c'", static_cast<char>(c));
    else
        out.printf("0x%04x", c);
}

void dumpCharacterClass(PrintStream& out, YarrPattern* pattern, CharacterClass* characterClass)
{
    if (characterClass == pattern->anyCharacterClass())
        out.print("<any character>");
    else if (characterClass == pattern->newlineCharacterClass())
        out.print("<newline>");
    else if (characterClass == pattern->digitsCharacterClass())
        out.print("<digits>");
    else if (characterClass == pattern->spacesCharacterClass())
        out.print("<whitespace>");
    else if (characterClass == pattern->wordcharCharacterClass())
        out.print("<word>");
    else if (characterClass == pattern->wordUnicodeIgnoreCaseCharCharacterClass())
        out.print("<unicode word ignore case>");
    else if (characterClass == pattern->nondigitsCharacterClass())
        out.print("<non-digits>");
    else if (characterClass == pattern->nonspacesCharacterClass())
        out.print("<non-whitespace>");
    else if (characterClass == pattern->nonwordcharCharacterClass())
        out.print("<non-word>");
    else if (characterClass == pattern->nonwordUnicodeIgnoreCaseCharCharacterClass())
        out.print("<unicode non-word ignore case>");
    else {
        bool needMatchesRangesSeperator = false;

        auto dumpMatches = [&] (const char* prefix, Vector<UChar32> matches) {
            size_t matchesSize = matches.size();
            if (matchesSize) {
                if (needMatchesRangesSeperator)
                    out.print(",");
                needMatchesRangesSeperator = true;

                out.print(prefix, ":(");
                for (size_t i = 0; i < matchesSize; ++i) {
                    if (i)
                        out.print(",");
                    dumpUChar32(out, matches[i]);
                }
                out.print(")");
            }
        };

        auto dumpRanges = [&] (const char* prefix, Vector<CharacterRange> ranges) {
            size_t rangeSize = ranges.size();
            if (rangeSize) {
                if (needMatchesRangesSeperator)
                    out.print(",");
                needMatchesRangesSeperator = true;

                out.print(prefix, " ranges:(");
                for (size_t i = 0; i < rangeSize; ++i) {
                    if (i)
                        out.print(",");
                    CharacterRange range = ranges[i];
                    out.print("(");
                    dumpUChar32(out, range.begin);
                    out.print("..");
                    dumpUChar32(out, range.end);
                    out.print(")");
                }
                out.print(")");
            }
        };

        out.print("[");
        dumpMatches("ASCII", characterClass->m_matches);
        dumpRanges("ASCII", characterClass->m_ranges);
        dumpMatches("Unicode", characterClass->m_matchesUnicode);
        dumpRanges("Unicode", characterClass->m_rangesUnicode);
        out.print("]");
    }
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
    out.print("\n");

    for (size_t i = 0; i < m_terms.size(); ++i)
        m_terms[i].dump(out, thisPattern, nestingDepth);
}

void PatternTerm::dumpQuantifier(PrintStream& out)
{
    if (quantityType == QuantifierFixedCount && quantityMinCount == 1 && quantityMaxCount == 1)
        return;
    out.print(" {", quantityMinCount.unsafeGet());
    if (quantityMinCount != quantityMaxCount) {
        if (quantityMaxCount == UINT_MAX)
            out.print(",...");
        else
            out.print(",", quantityMaxCount.unsafeGet());
    }
    out.print("}");
    if (quantityType == QuantifierGreedy)
        out.print(" greedy");
    else if (quantityType == QuantifierNonGreedy)
        out.print(" non-greedy");
}

void PatternTerm::dump(PrintStream& out, YarrPattern* thisPattern, unsigned nestingDepth)
{
    indentForNestingLevel(out, nestingDepth);

    if (type != TypeParenthesesSubpattern && type != TypeParentheticalAssertion) {
        if (invert())
            out.print("not ");
    }

    switch (type) {
    case TypeAssertionBOL:
        out.println("BOL");
        break;
    case TypeAssertionEOL:
        out.println("EOL");
        break;
    case TypeAssertionWordBoundary:
        out.println("word boundary");
        break;
    case TypePatternCharacter:
        out.printf("character ");
        out.printf("inputPosition %u ", inputPosition);
        if (thisPattern->ignoreCase() && isASCIIAlpha(patternCharacter)) {
            dumpUChar32(out, toASCIIUpper(patternCharacter));
            out.print("/");
            dumpUChar32(out, toASCIILower(patternCharacter));
        } else
            dumpUChar32(out, patternCharacter);
        dumpQuantifier(out);
        if (quantityType != QuantifierFixedCount)
            out.print(",frame location ", frameLocation);
        out.println();
        break;
    case TypeCharacterClass:
        out.print("character class ");
        dumpCharacterClass(out, thisPattern, characterClass);
        dumpQuantifier(out);
        if (quantityType != QuantifierFixedCount || thisPattern->unicode())
            out.print(",frame location ", frameLocation);
        out.println();
        break;
    case TypeBackReference:
        out.print("back reference to subpattern #", backReferenceSubpatternId);
        out.println(",frame location ", frameLocation);
        break;
    case TypeForwardReference:
        out.println("forward reference");
        break;
    case TypeParenthesesSubpattern:
        if (m_capture)
            out.print("captured ");
        else
            out.print("non-captured ");

        FALLTHROUGH;
    case TypeParentheticalAssertion:
        if (m_invert)
            out.print("inverted ");

        if (type == TypeParenthesesSubpattern)
            out.print("subpattern");
        else if (type == TypeParentheticalAssertion)
            out.print("assertion");

        if (m_capture)
            out.print(" #", parentheses.subpatternId);

        dumpQuantifier(out);

        if (parentheses.isCopy)
            out.print(",copy");

        if (parentheses.isTerminal)
            out.print(",terminal");

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
    case TypeDotStarEnclosure:
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

void YarrPattern::dumpPatternString(PrintStream& out, const String& patternString)
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
    if (sticky())
        out.print("y");
}

void YarrPattern::dumpPattern(const String& patternString)
{
    dumpPattern(WTF::dataFile(), patternString);
}

void YarrPattern::dumpPattern(PrintStream& out, const String& patternString)
{
    out.print("RegExp pattern for ");
    dumpPatternString(out, patternString);

    if (m_flags != NoFlags) {
        bool printSeperator = false;
        out.print(" (");
        if (global()) {
            out.print("global");
            printSeperator = true;
        }
        if (ignoreCase()) {
            if (printSeperator)
                out.print("|");
            out.print("ignore case");
            printSeperator = true;
        }
        if (multiline()) {
            if (printSeperator)
                out.print("|");
            out.print("multiline");
            printSeperator = true;
        }
        if (unicode()) {
            if (printSeperator)
                out.print("|");
            out.print("unicode");
            printSeperator = true;
        }
        if (sticky()) {
            if (printSeperator)
                out.print("|");
            out.print("sticky");
            printSeperator = true;
        }
        out.print(")");
    }
    out.print(":\n");
    if (m_body->m_callFrameSize)
        out.print("    callframe size: ", m_body->m_callFrameSize, "\n");
    m_body->dump(out, this);
}

std::unique_ptr<CharacterClass> anycharCreate()
{
    auto characterClass = std::make_unique<CharacterClass>();
    characterClass->m_ranges.append(CharacterRange(0x00, 0x7f));
    characterClass->m_rangesUnicode.append(CharacterRange(0x0080, 0x10ffff));
    characterClass->m_hasNonBMPCharacters = true;
    characterClass->m_anyCharacter = true;
    return characterClass;
}

} } // namespace JSC::Yarr
