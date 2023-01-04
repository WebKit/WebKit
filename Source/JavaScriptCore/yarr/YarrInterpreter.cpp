/*
 * Copyright (C) 2009, 2013-2017 Apple Inc. All rights reserved.
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
#include "YarrInterpreter.h"

#include "Options.h"
#include "SuperSampler.h"
#include "Yarr.h"
#include "YarrCanonicalize.h"
#include <wtf/BumpPointerAllocator.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/DataLog.h>
#include <wtf/StackCheck.h>
#include <wtf/text/WTFString.h>

namespace JSC { namespace Yarr {

class ByteTermDumper {
public:
    ByteTermDumper(YarrPattern* pattern = nullptr)
        : m_pattern(pattern)
    {
        if (pattern)
            m_unicode = pattern->unicode();
    }

    ByteTermDumper(bool unicode)
        : m_unicode(unicode)
    {
    }

    void dumpTerm(size_t idx, ByteTerm);
    void dumpDisjunction(ByteDisjunction*, unsigned nesting = 0);
    bool unicode() { return m_unicode; }

private:
    YarrPattern* m_pattern { nullptr };
    unsigned m_nesting { 0 };
    unsigned m_lineIndent { 0 };
    bool m_unicode { false };
    bool m_recursiveDump { false };
};

template<typename CharType>
class Interpreter {
public:
    static constexpr bool verbose = false;

    struct ParenthesesDisjunctionContext;

    struct BackTrackInfoParentheses {
        uintptr_t begin;
        uintptr_t matchAmount;
        ParenthesesDisjunctionContext* lastContext;
    };

    static inline void appendParenthesesDisjunctionContext(BackTrackInfoParentheses* backTrack, ParenthesesDisjunctionContext* context)
    {
        context->next = backTrack->lastContext;
        backTrack->lastContext = context;
        ++backTrack->matchAmount;
    }

    static inline void popParenthesesDisjunctionContext(BackTrackInfoParentheses* backTrack)
    {
        RELEASE_ASSERT(backTrack->matchAmount);
        RELEASE_ASSERT(backTrack->lastContext);
        backTrack->lastContext = backTrack->lastContext->next;
        --backTrack->matchAmount;
    }

    struct DisjunctionContext
    {
        DisjunctionContext() = default;

        void* operator new(size_t, void* where)
        {
            return where;
        }

        static size_t allocationSize(unsigned numberOfFrames)
        {
            static_assert(alignof(DisjunctionContext) <= sizeof(void*));
            size_t rawSize = sizeof(DisjunctionContext) - sizeof(uintptr_t) + Checked<size_t>(numberOfFrames) * sizeof(uintptr_t);
            size_t roundedSize = roundUpToMultipleOf<sizeof(void*)>(rawSize);
            RELEASE_ASSERT(roundedSize >= rawSize);
            return roundedSize;
        }

        int term { 0 };
        unsigned matchBegin;
        unsigned matchEnd;
        uintptr_t frame[1];
    };

    DisjunctionContext* allocDisjunctionContext(ByteDisjunction* disjunction)
    {
        size_t size = DisjunctionContext::allocationSize(disjunction->m_frameSize);
        allocatorPool = allocatorPool->ensureCapacity(size);
        RELEASE_ASSERT(allocatorPool);
        return new (allocatorPool->alloc(size)) DisjunctionContext();
    }

    void freeDisjunctionContext(DisjunctionContext* context)
    {
        allocatorPool = allocatorPool->dealloc(context);
    }

    struct ParenthesesDisjunctionContext
    {
        ParenthesesDisjunctionContext(unsigned* output, ByteTerm& term)
        {
            unsigned firstSubpatternId = term.subpatternId();
            unsigned numNestedSubpatterns = term.atom.parenthesesDisjunction->m_numSubpatterns;

            for (unsigned i = 0; i < (numNestedSubpatterns << 1); ++i) {
                subpatternBackup[i] = output[(firstSubpatternId << 1) + i];
                output[(firstSubpatternId << 1) + i] = offsetNoMatch;
            }

            new (getDisjunctionContext(term)) DisjunctionContext();
        }

        void* operator new(size_t, void* where)
        {
            return where;
        }

        void restoreOutput(unsigned* output, unsigned firstSubpatternId, unsigned numNestedSubpatterns)
        {
            for (unsigned i = 0; i < (numNestedSubpatterns << 1); ++i)
                output[(firstSubpatternId << 1) + i] = subpatternBackup[i];
        }

        DisjunctionContext* getDisjunctionContext(ByteTerm& term)
        {
            return bitwise_cast<DisjunctionContext*>(bitwise_cast<uintptr_t>(this) + allocationSize(term.atom.parenthesesDisjunction->m_numSubpatterns));
        }

        static size_t allocationSize(unsigned numberOfSubpatterns)
        {
            static_assert(alignof(ParenthesesDisjunctionContext) <= sizeof(void*));
            size_t rawSize = sizeof(ParenthesesDisjunctionContext) - sizeof(unsigned) + (Checked<size_t>(numberOfSubpatterns) * 2U) * sizeof(unsigned);
            size_t roundedSize = roundUpToMultipleOf<sizeof(void*)>(rawSize);
            RELEASE_ASSERT(roundedSize >= rawSize);
            return roundedSize;
        }

        ParenthesesDisjunctionContext* next { nullptr };
        unsigned subpatternBackup[1];
    };

    ParenthesesDisjunctionContext* allocParenthesesDisjunctionContext(ByteDisjunction* disjunction, unsigned* output, ByteTerm& term)
    {
        size_t size = Checked<size_t>(ParenthesesDisjunctionContext::allocationSize(term.atom.parenthesesDisjunction->m_numSubpatterns)) + DisjunctionContext::allocationSize(disjunction->m_frameSize);
        allocatorPool = allocatorPool->ensureCapacity(size);
        RELEASE_ASSERT(allocatorPool);
        return new (allocatorPool->alloc(size)) ParenthesesDisjunctionContext(output, term);
    }

    void freeParenthesesDisjunctionContext(ParenthesesDisjunctionContext* context)
    {
        allocatorPool = allocatorPool->dealloc(context);
    }

    class InputStream {
    public:
        InputStream(const CharType* input, unsigned start, unsigned length, bool decodeSurrogatePairs)
            : input(input)
            , pos(start)
            , length(length)
            , decodeSurrogatePairs(decodeSurrogatePairs)
        {
        }

        void next()
        {
            ++pos;
        }

        void rewind(unsigned amount)
        {
            ASSERT(pos >= amount);
            pos -= amount;
        }

        int read()
        {
            ASSERT(pos < length);
            if (pos < length)
                return input[pos];
            return -1;
        }

        int readChecked(unsigned negativePositionOffest)
        {
            RELEASE_ASSERT(pos >= negativePositionOffest);
            unsigned p = pos - negativePositionOffest;
            ASSERT(p < length);
            int result = input[p];
            if (U16_IS_LEAD(result) && decodeSurrogatePairs && p + 1 < length && U16_IS_TRAIL(input[p + 1])) {
                if (atEnd())
                    return -1;
                
                result = U16_GET_SUPPLEMENTARY(result, input[p + 1]);
                next();
            }
            return result;
        }

        // readForCharacterDump() is only for use by the DUMP_CURR_CHAR macro.
        // We don't want any side effects like the next() in readChecked() above.
        int readForCharacterDump(unsigned negativePositionOffest)
        {
            RELEASE_ASSERT(pos >= negativePositionOffest);
            unsigned p = pos - negativePositionOffest;
            ASSERT(p < length);
            int result = input[p];
            if (U16_IS_LEAD(result) && decodeSurrogatePairs && p + 1 < length && U16_IS_TRAIL(input[p + 1])) {
                if (atEnd())
                    return -1;

                result = U16_GET_SUPPLEMENTARY(result, input[p + 1]);
            }
            return result;
        }
        
        int tryReadBackward(unsigned negativePositionOffest)
        {
            if (pos < negativePositionOffest)
                return -1;

            unsigned p = pos - negativePositionOffest;
            ASSERT(p < length);
            int result = input[p];
            if (U16_IS_TRAIL(result) && decodeSurrogatePairs && p > 0 && U16_IS_LEAD(input[p - 1])) {
                result = U16_GET_SUPPLEMENTARY(input[p - 1], result);
                rewind(1);
            }
            return result;
        }

        int readSurrogatePairChecked(unsigned negativePositionOffset)
        {
            RELEASE_ASSERT(pos >= negativePositionOffset);
            unsigned p = pos - negativePositionOffset;
            ASSERT(p < length);
            if (p + 1 >= length)
                return -1;

            int first = input[p];
            int second = input[p + 1];
            if (U16_IS_LEAD(first) && U16_IS_TRAIL(second))
                return U16_GET_SUPPLEMENTARY(first, second);

            return -1;
        }

        int reread(unsigned from)
        {
            ASSERT(from < length);
            int result = input[from];
            if (U16_IS_LEAD(result) && decodeSurrogatePairs && from + 1 < length && U16_IS_TRAIL(input[from + 1]))
                result = U16_GET_SUPPLEMENTARY(result, input[from + 1]);
            return result;
        }

        int prev()
        {
            ASSERT(!(pos > length));
            if (pos && length)
                return input[pos - 1];
            return -1;
        }

        unsigned getPos()
        {
            return pos;
        }

        void setPos(unsigned p)
        {
            pos = p;
        }

        bool atStart()
        {
            return pos == 0;
        }

        bool atEnd()
        {
            return pos == length;
        }

        unsigned end()
        {
            return length;
        }

        bool checkInput(unsigned count)
        {
            if (((pos + count) <= length) && ((pos + count) >= pos)) {
                pos += count;
                return true;
            }
            return false;
        }

        void uncheckInput(unsigned count)
        {
            RELEASE_ASSERT(pos >= count);
            pos -= count;
        }

        bool tryUncheckInput(unsigned count)
        {
            if (count > pos)
                return false;
            pos -= count;
            return true;
        }

        bool atStart(unsigned negativePositionOffset)
        {
            return pos == negativePositionOffset;
        }

        bool atEnd(unsigned negativePositionOffest)
        {
            RELEASE_ASSERT(pos >= negativePositionOffest);
            return (pos - negativePositionOffest) == length;
        }

        bool isAvailableInput(unsigned offset)
        {
            return (((pos + offset) <= length) && ((pos + offset) >= pos));
        }

        bool isValidNegativeInputOffset(unsigned offset)
        {
            return (pos >= offset) && ((pos - offset) < length);
        }

        void dump(PrintStream& out) const
        {
            const unsigned maxPrintLen = 50;

            out.print(sizeof(CharType) == 1 ? "8bit" : "16bit", " len: ", length, " \"");
            unsigned printLen = std::min(length, maxPrintLen);
            for (unsigned i = 0; i < printLen; ++i)
                out.print(CharacterDump(input[i]));
            if (length > maxPrintLen)
                out.print("...");
            out.print("\"");
        }

    private:
        const CharType* input;
        unsigned pos;
        unsigned length;
        bool decodeSurrogatePairs;
    };

    bool testCharacterClass(CharacterClass* characterClass, int ch)
    {
        auto linearSearchMatches = [&ch](const Vector<UChar32>& matches) {
            for (unsigned i = 0; i < matches.size(); ++i) {
                if (ch == matches[i])
                    return true;
            }

            return false;
        };

        auto binarySearchMatches = [&ch](const Vector<UChar32>& matches) {
            size_t low = 0;
            size_t high = matches.size() - 1;

            while (low <= high) {
                size_t mid = low + (high - low) / 2;
                int diff = ch - matches[mid];
                if (!diff)
                    return true;

                if (diff < 0) {
                    if (mid == low)
                        return false;
                    high = mid - 1;
                } else
                    low = mid + 1;
            }
            return false;
        };

        auto linearSearchRanges = [&ch](const Vector<CharacterRange>& ranges) {
            for (unsigned i = 0; i < ranges.size(); ++i) {
                if ((ch >= ranges[i].begin) && (ch <= ranges[i].end))
                    return true;
            }

            return false;
        };

        auto binarySearchRanges = [&ch](const Vector<CharacterRange>& ranges) {
            size_t low = 0;
            size_t high = ranges.size() - 1;

            while (low <= high) {
                size_t mid = low + (high - low) / 2;
                int rangeBeginDiff = ch - ranges[mid].begin;
                if (rangeBeginDiff >= 0 && ch <= ranges[mid].end)
                    return true;

                if (rangeBeginDiff < 0) {
                    if (mid == low)
                        return false;
                    high = mid - 1;
                } else
                    low = mid + 1;
            }
            return false;
        };

        if (characterClass->m_anyCharacter)
            return true;

        const size_t thresholdForBinarySearch = 6;

        if (!isASCII(ch)) {
            if (characterClass->m_matchesUnicode.size()) {
                if (characterClass->m_matchesUnicode.size() > thresholdForBinarySearch) {
                    if (binarySearchMatches(characterClass->m_matchesUnicode))
                        return true;
                } else if (linearSearchMatches(characterClass->m_matchesUnicode))
                    return true;
            }

            if (characterClass->m_rangesUnicode.size()) {
                if (characterClass->m_rangesUnicode.size() > thresholdForBinarySearch) {
                    if (binarySearchRanges(characterClass->m_rangesUnicode))
                        return true;
                } else if (linearSearchRanges(characterClass->m_rangesUnicode))
                    return true;
            }
        } else {
            if (characterClass->m_matches.size()) {
                if (characterClass->m_matches.size() > thresholdForBinarySearch) {
                    if (binarySearchMatches(characterClass->m_matches))
                        return true;
                } else if (linearSearchMatches(characterClass->m_matches))
                    return true;
            }

            if (characterClass->m_ranges.size()) {
                if (characterClass->m_ranges.size() > thresholdForBinarySearch) {
                    if (binarySearchRanges(characterClass->m_ranges))
                        return true;
                } else if (linearSearchRanges(characterClass->m_ranges))
                    return true;
            }
        }

        return false;
    }

    bool checkCharacter(ByteTerm& term, unsigned negativeInputOffset)
    {
        ASSERT(term.isCharacterType());
        if (term.matchDirection() == Forward)
            return term.atom.patternCharacter == input.readChecked(negativeInputOffset);

        return term.atom.patternCharacter == input.tryReadBackward(negativeInputOffset);
    }

    bool checkSurrogatePair(ByteTerm& term, unsigned negativeInputOffset)
    {
        ASSERT(term.isCharacterType());
        return term.atom.patternCharacter == input.readSurrogatePairChecked(negativeInputOffset);
    }

    bool checkCasedCharacter(ByteTerm& term, unsigned negativeInputOffset)
    {
        ASSERT(term.isCasedCharacterType());
        int ch = term.matchDirection() == Forward ? input.readChecked(negativeInputOffset) : input.tryReadBackward(negativeInputOffset);
        return (term.atom.casedCharacter.lo == ch) || (term.atom.casedCharacter.hi == ch);
    }

    bool checkCharacterClass(ByteTerm& term, unsigned negativeInputOffset)
    {
        ASSERT(term.isCharacterClass());

        int inputChar = term.matchDirection() == Forward ? input.readChecked(negativeInputOffset) : input.tryReadBackward(negativeInputOffset);
        if (inputChar < 0)
            return false;

        bool match = testCharacterClass(term.atom.characterClass, inputChar);
        return term.invert() ? !match : match;
    }
    
    bool checkCharacterClassDontAdvanceInputForNonBMP(ByteTerm& term, unsigned negativeInputOffset)
    {
        ASSERT(term.isCharacterClass());
        CharacterClass* characterClass = term.atom.characterClass;

        if (term.matchDirection() == Backward && negativeInputOffset > input.getPos())
            return false;

        int readCharacter = characterClass->hasOnlyNonBMPCharacters() ? input.readSurrogatePairChecked(negativeInputOffset) :  input.readChecked(negativeInputOffset);

        if (readCharacter < 0)
            return false;

        return testCharacterClass(characterClass, readCharacter);
    }

    bool tryConsumeBackReference(int matchBegin, int matchEnd, ByteTerm& term)
    {
        unsigned matchSize = (unsigned)(matchEnd - matchBegin);

        if (term.matchDirection() == Forward) {
            if (!input.checkInput(matchSize))
                return false;
        }

        for (unsigned i = 0; i < matchSize; ++i) {
            unsigned negativeInputOffset = term.inputPosition + matchSize - i;
            if (term.matchDirection() == Backward && negativeInputOffset > input.getPos())
                return false;

            int oldCh = input.reread(matchBegin + i);
            int ch;
            if (!U_IS_BMP(oldCh)) {
                ch = input.readSurrogatePairChecked(negativeInputOffset);
                ++i;
            } else
                ch = term.matchDirection() == Forward ? input.readChecked(negativeInputOffset) : input.tryReadBackward(negativeInputOffset);

            if (oldCh == ch)
                continue;

            if (pattern->ignoreCase()) {
                // See ES 6.0, 21.2.2.8.2 for the definition of Canonicalize(). For non-Unicode
                // patterns, Unicode values are never allowed to match against ASCII ones.
                // For Unicode, we need to check all canonical equivalents of a character.
                if (!unicode && (isASCII(oldCh) || isASCII(ch))) {
                    if (toASCIIUpper(oldCh) == toASCIIUpper(ch))
                        continue;
                } else if (areCanonicallyEquivalent(oldCh, ch, unicode ? CanonicalMode::Unicode : CanonicalMode::UCS2))
                    continue;
            }

            if (term.matchDirection() == Forward)
                input.uncheckInput(matchSize);

            return false;
        }

        if (term.matchDirection() == Backward)
            input.uncheckInput(matchSize);

        return true;
    }

    bool matchAssertionBOL(ByteTerm& term)
    {
        return (input.atStart(term.inputPosition)) || (pattern->multiline() && testCharacterClass(pattern->newlineCharacterClass, input.readChecked(term.inputPosition + 1)));
    }

    bool matchAssertionEOL(ByteTerm& term)
    {
        if (term.inputPosition)
            return (input.atEnd(term.inputPosition)) || (pattern->multiline() && testCharacterClass(pattern->newlineCharacterClass, input.readChecked(term.inputPosition)));

        return (input.atEnd()) || (pattern->multiline() && testCharacterClass(pattern->newlineCharacterClass, input.read()));
    }

    bool matchAssertionWordBoundary(ByteTerm& term)
    {
        unsigned inputOffset = term.inputPosition;

        bool prevIsWordchar = !input.atStart(inputOffset) && testCharacterClass(pattern->wordcharCharacterClass, input.readChecked(inputOffset + 1));
        bool readIsWordchar;
        if (inputOffset)
            readIsWordchar = !input.atEnd(inputOffset) && testCharacterClass(pattern->wordcharCharacterClass, input.readChecked(inputOffset));
        else
            readIsWordchar = !input.atEnd() && testCharacterClass(pattern->wordcharCharacterClass, input.read());

        bool wordBoundary = prevIsWordchar != readIsWordchar;
        return term.invert() ? !wordBoundary : wordBoundary;
    }

    bool backtrackPatternCharacter(ByteTerm& term, DisjunctionContext* context)
    {
        BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount:
            break;

        case QuantifierType::Greedy:
            if (backTrack->matchAmount) {
                --backTrack->matchAmount;
                if (term.matchDirection() == Forward)
                    input.uncheckInput(U16_LENGTH(term.atom.patternCharacter));
                else {
                    if (!input.checkInput(U16_LENGTH(term.atom.patternCharacter)))
                        break;
                }
                return true;
            }
            break;

        case QuantifierType::NonGreedy:
            if (term.matchDirection() == Forward) {
                if ((backTrack->matchAmount < term.atom.quantityMaxCount) && input.checkInput(1)) {
                    ++backTrack->matchAmount;
                    if (checkCharacter(term, term.inputPosition + 1))
                        return true;
                }
                input.setPos(backTrack->begin);
                break;
            }
            // matchDirection Backward
            unsigned position = input.getPos();

            if (position < term.inputPosition)
                break;

            if ((backTrack->matchAmount < term.atom.quantityMaxCount) && input.tryUncheckInput(1)) {
                ++backTrack->matchAmount;
                if (checkCharacter(term, term.inputPosition))
                    return true;
            }
            input.setPos(backTrack->begin);
            break;

        }

        return false;
    }

    bool backtrackPatternCasedCharacter(ByteTerm& term, DisjunctionContext* context)
    {
        BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount:
            break;

        case QuantifierType::Greedy:
            if (backTrack->matchAmount) {
                --backTrack->matchAmount;
                if (term.matchDirection() == Forward)
                    input.uncheckInput(1);
                else {
                    if (!input.checkInput(1))
                        break;
                }
                return true;
            }
            break;

        case QuantifierType::NonGreedy:
            if (term.matchDirection() == Forward) {
                if ((backTrack->matchAmount < term.atom.quantityMaxCount) && input.checkInput(1)) {
                    ++backTrack->matchAmount;
                    if (checkCasedCharacter(term, term.inputPosition + 1))
                        return true;
                }
                input.uncheckInput(backTrack->matchAmount);
                break;
            }
            // matchDirection Backward
            if ((backTrack->matchAmount < term.atom.quantityMaxCount) && input.tryUncheckInput(1)) {
                ++backTrack->matchAmount;
                if (checkCasedCharacter(term, term.inputPosition))
                    return true;
            }
            input.uncheckInput(backTrack->matchAmount);
            break;
        }

        return false;
    }

    bool matchCharacterClass(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::CharacterClass);
        BackTrackInfoCharacterClass* backTrack = reinterpret_cast<BackTrackInfoCharacterClass*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount: {
            if (term.matchDirection() == Forward) {
                if (unicode) {
                    backTrack->begin = input.getPos();
                    unsigned matchAmount = 0;
                    for (matchAmount = 0; matchAmount < term.atom.quantityMaxCount; ++matchAmount) {
                        if (term.invert()) {
                            if (!checkCharacterClass(term, term.inputPosition - matchAmount)) {
                                input.setPos(backTrack->begin);
                                return false;
                            }
                        } else {
                            unsigned matchOffset = matchAmount * (term.atom.characterClass->hasOnlyNonBMPCharacters() ? 2 : 1);
                            if (!checkCharacterClassDontAdvanceInputForNonBMP(term, term.inputPosition - matchOffset)) {
                                input.setPos(backTrack->begin);
                                return false;
                            }
                        }
                    }

                    return true;
                }

                for (unsigned matchAmount = 0; matchAmount < term.atom.quantityMaxCount; ++matchAmount) {
                    if (!checkCharacterClass(term, term.inputPosition - matchAmount))
                        return false;
                }
                return true;
            }

            // matchDirection is Backward
            if (unicode) {
                backTrack->begin = input.getPos();
                for (unsigned matchAmount = 0; matchAmount < term.atom.quantityMaxCount; ++matchAmount) {
                    unsigned matchOffset = term.atom.quantityMaxCount - 1 - matchAmount;
                    if (term.invert()) {
                        if (!checkCharacterClass(term, term.inputPosition - matchOffset)) {
                            input.setPos(backTrack->begin);
                            return false;
                        }
                    } else {
                        matchOffset = matchOffset * (term.atom.characterClass->hasOnlyNonBMPCharacters() ? 2 : 1);
                        if (!checkCharacterClassDontAdvanceInputForNonBMP(term, term.inputPosition - matchOffset)) {
                            input.setPos(backTrack->begin);
                            return false;
                        }
                    }
                }

                return true;
            }

            if (input.getPos() < term.inputPosition)
                return false;

            for (unsigned matchAmount = 0; matchAmount < term.atom.quantityMaxCount; ++matchAmount) {
                if (!checkCharacterClass(term, term.inputPosition - term.atom.quantityMaxCount + matchAmount + 1))
                    return false;
            }
            return true;
        }

        case QuantifierType::Greedy: {
            unsigned position = input.getPos();
            backTrack->begin = position;
            unsigned matchAmount = 0;
            if (term.matchDirection() == Forward) {
                while ((matchAmount < term.atom.quantityMaxCount) && input.checkInput(1)) {
                    if (!checkCharacterClass(term, term.inputPosition + 1)) {
                        input.setPos(position);
                        break;
                    }
                    ++matchAmount;
                    position = input.getPos();
                }
                backTrack->matchAmount = matchAmount;
                return true;
            }

            // matchDirection = Backward
            if (input.getPos() < term.inputPosition)
                return false;

            while ((matchAmount < term.atom.quantityMaxCount) && input.tryUncheckInput(1)) {
                if (!checkCharacterClass(term, term.inputPosition)) {
                    input.setPos(position);
                    break;
                }
                ++matchAmount;
                position = input.getPos();
            }
            backTrack->matchAmount = matchAmount;
            return true;
        }

        case QuantifierType::NonGreedy:
            backTrack->begin = input.getPos();
            backTrack->matchAmount = 0;
            return true;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }

    bool backtrackCharacterClass(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::CharacterClass);
        BackTrackInfoCharacterClass* backTrack = reinterpret_cast<BackTrackInfoCharacterClass*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount:
            if (unicode)
                input.setPos(backTrack->begin);
            break;

        case QuantifierType::Greedy:
            if (backTrack->matchAmount) {
                if (unicode) {
                    // Rematch one less match
                    if (term.matchDirection() == Forward) {
                        input.setPos(backTrack->begin);
                        --backTrack->matchAmount;
                        for (unsigned matchAmount = 0; (matchAmount < backTrack->matchAmount) && input.checkInput(1); ++matchAmount) {
                            if (!checkCharacterClass(term, term.inputPosition + 1)) {
                                input.uncheckInput(1);
                                break;
                            }
                        }
                        return true;
                    }
                    // matchDirection Backwards
                    input.setPos(backTrack->begin);
                    --backTrack->matchAmount;
                    for (unsigned matchAmount = 0; (matchAmount < backTrack->matchAmount) && input.tryUncheckInput(1); ++matchAmount) {
                        if (!checkCharacterClass(term, term.inputPosition)) {
                            input.checkInput(1);
                            break;
                        }
                    }
                    return true;
                }
                --backTrack->matchAmount;
                if (term.matchDirection() == Forward)
                    input.uncheckInput(1);
                else
                    input.checkInput(1);
                return true;
            }
            break;

        case QuantifierType::NonGreedy:
            if (term.matchDirection() == Forward) {
                if ((backTrack->matchAmount < term.atom.quantityMaxCount) && input.checkInput(1)) {
                    ++backTrack->matchAmount;
                    if (checkCharacterClass(term, term.inputPosition + 1))
                        return true;
                }
                input.setPos(backTrack->begin);
                break;
            }
            // matchDirection Backward
            if ((backTrack->matchAmount < term.atom.quantityMaxCount) && input.tryUncheckInput(1)) {
                ++backTrack->matchAmount;
                if (checkCharacterClass(term, term.inputPosition))
                    return true;
            }
            input.setPos(backTrack->begin);
            break;
        }

        return false;
    }

    bool matchBackReference(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::BackReference);
        BackTrackInfoBackReference* backTrack = reinterpret_cast<BackTrackInfoBackReference*>(context->frame + term.frameLocation);

        unsigned matchBegin = output[(term.subpatternId() << 1)];
        unsigned matchEnd = output[(term.subpatternId() << 1) + 1];

        // If the end position of the referenced match hasn't set yet then the backreference in the same parentheses where it references to that.
        // In this case the result of match is empty string like when it references to a parentheses with zero-width match.
        // Eg.: /(a\1)/
        if (matchEnd == offsetNoMatch)
            return true;

        if (matchBegin == offsetNoMatch)
            return true;

        ASSERT(matchBegin <= matchEnd);

        if (matchBegin == matchEnd)
            return true;

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount: {
            backTrack->begin = input.getPos();
            for (unsigned matchAmount = 0; matchAmount < term.atom.quantityMaxCount; ++matchAmount) {
                if (!tryConsumeBackReference(matchBegin, matchEnd, term)) {
                    input.setPos(backTrack->begin);
                    return false;
                }
            }
            return true;
        }

        case QuantifierType::Greedy: {
            unsigned matchAmount = 0;
            while ((matchAmount < term.atom.quantityMaxCount) && tryConsumeBackReference(matchBegin, matchEnd, term))
                ++matchAmount;
            backTrack->matchAmount = matchAmount;
            return true;
        }

        case QuantifierType::NonGreedy:
            backTrack->begin = input.getPos();
            backTrack->matchAmount = 0;
            return true;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }

    bool backtrackBackReference(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::BackReference);
        BackTrackInfoBackReference* backTrack = reinterpret_cast<BackTrackInfoBackReference*>(context->frame + term.frameLocation);

        unsigned matchBegin = output[(term.subpatternId() << 1)];
        unsigned matchEnd = output[(term.subpatternId() << 1) + 1];

        if (matchBegin == offsetNoMatch)
            return false;

        ASSERT(matchBegin <= matchEnd);

        if (matchBegin == matchEnd)
            return false;

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount:
            // for quantityMaxCount == 1, could rewind.
            input.setPos(backTrack->begin);
            break;

        case QuantifierType::Greedy:
            if (backTrack->matchAmount) {
                --backTrack->matchAmount;
                input.rewind(matchEnd - matchBegin);
                return true;
            }
            break;

        case QuantifierType::NonGreedy:
            if ((backTrack->matchAmount < term.atom.quantityMaxCount) && tryConsumeBackReference(matchBegin, matchEnd, term)) {
                ++backTrack->matchAmount;
                return true;
            }
            input.setPos(backTrack->begin);
            break;
        }

        return false;
    }

    void recordParenthesesMatch(ByteTerm& term, ParenthesesDisjunctionContext* context)
    {
        if (term.capture()) {
            unsigned subpatternId = term.subpatternId();
            // For Backward matches, the captured indexes where recorded end then start.
            output[(subpatternId << 1) + term.matchDirection()] = context->getDisjunctionContext(term)->matchBegin - term.inputPosition;
            output[(subpatternId << 1) + 1 - term.matchDirection()] = context->getDisjunctionContext(term)->matchEnd - term.inputPosition;
        }
    }
    void resetMatches(ByteTerm& term, ParenthesesDisjunctionContext* context)
    {
        unsigned firstSubpatternId = term.subpatternId();
        unsigned count = term.atom.parenthesesDisjunction->m_numSubpatterns;
        context->restoreOutput(output, firstSubpatternId, count);
    }
    JSRegExpResult parenthesesDoBacktrack(ByteTerm& term, BackTrackInfoParentheses* backTrack)
    {
        while (backTrack->matchAmount) {
            ParenthesesDisjunctionContext* context = backTrack->lastContext;

            JSRegExpResult result = matchDisjunction(term.atom.parenthesesDisjunction, context->getDisjunctionContext(term), true);
            if (result == JSRegExpResult::Match)
                return JSRegExpResult::Match;

            resetMatches(term, context);
            popParenthesesDisjunctionContext(backTrack);
            freeParenthesesDisjunctionContext(context);

            if (result != JSRegExpResult::NoMatch)
                return result;
        }

        return JSRegExpResult::NoMatch;
    }

    bool matchParenthesesOnceBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternOnceBegin);
        ASSERT(term.atom.quantityMaxCount == 1);

        BackTrackInfoParenthesesOnce* backTrack = reinterpret_cast<BackTrackInfoParenthesesOnce*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierType::Greedy: {
            // set this speculatively; if we get to the parens end this will be true.
            backTrack->begin = input.getPos();
            break;
        }
        case QuantifierType::NonGreedy: {
            backTrack->begin = notFound;
            context->term += term.atom.parenthesesWidth;
            return true;
        }
        case QuantifierType::FixedCount:
            break;
        }

        if (term.capture()) {
            unsigned subpatternId = term.subpatternId();
            // For Backward matches, the captured indexes where recorded end then start.
            output[(subpatternId << 1) + term.matchDirection()] = input.getPos() - term.inputPosition;
        }

        return true;
    }

    bool matchParenthesesOnceEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternOnceEnd);
        ASSERT(term.atom.quantityMaxCount == 1);

        if (term.capture()) {
            unsigned subpatternId = term.subpatternId();
            // For Backward matches, the captured indexes where recorded end then start.
            output[(subpatternId << 1) + 1 - term.matchDirection()] = input.getPos() - term.inputPosition;
        }

        if (term.atom.quantityType == QuantifierType::FixedCount)
            return true;

        BackTrackInfoParenthesesOnce* backTrack = reinterpret_cast<BackTrackInfoParenthesesOnce*>(context->frame + term.frameLocation);
        return backTrack->begin != input.getPos();
    }

    bool backtrackParenthesesOnceBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternOnceBegin);
        ASSERT(term.atom.quantityMaxCount == 1);

        BackTrackInfoParenthesesOnce* backTrack = reinterpret_cast<BackTrackInfoParenthesesOnce*>(context->frame + term.frameLocation);

        if (term.capture()) {
            unsigned subpatternId = term.subpatternId();
            output[(subpatternId << 1)] = offsetNoMatch;
            output[(subpatternId << 1) + 1] = offsetNoMatch;
        }

        switch (term.atom.quantityType) {
        case QuantifierType::Greedy:
            // if we backtrack to this point, there is another chance - try matching nothing.
            ASSERT(backTrack->begin != notFound);
            backTrack->begin = notFound;
            context->term += term.atom.parenthesesWidth;
            return true;
        case QuantifierType::NonGreedy:
            ASSERT(backTrack->begin != notFound);
            FALLTHROUGH;
        case QuantifierType::FixedCount:
            break;
        }

        return false;
    }

    bool backtrackParenthesesOnceEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternOnceEnd);
        ASSERT(term.atom.quantityMaxCount == 1);

        BackTrackInfoParenthesesOnce* backTrack = reinterpret_cast<BackTrackInfoParenthesesOnce*>(context->frame + term.frameLocation);

        switch (term.atom.quantityType) {
        case QuantifierType::Greedy:
            if (backTrack->begin == notFound) {
                context->term -= term.atom.parenthesesWidth;
                return false;
            }
            FALLTHROUGH;
        case QuantifierType::NonGreedy:
            if (backTrack->begin == notFound) {
                backTrack->begin = input.getPos();
                if (term.capture()) {
                    // Technically this access to inputPosition should be accessing the begin term's
                    // inputPosition, but for repeats other than fixed these values should be
                    // the same anyway! (We don't pre-check for greedy or non-greedy matches.)
                    ASSERT((&term - term.atom.parenthesesWidth)->type == ByteTerm::Type::ParenthesesSubpatternOnceBegin);
                    ASSERT((&term - term.atom.parenthesesWidth)->inputPosition == term.inputPosition);
                    unsigned subpatternId = term.subpatternId();
                    // For Backward matches, the captured indexes where recorded end then start.
                    output[(subpatternId << 1) + term.matchDirection()] = input.getPos() - term.inputPosition;
                }
                context->term -= term.atom.parenthesesWidth;
                return true;
            }
            FALLTHROUGH;
        case QuantifierType::FixedCount:
            break;
        }

        return false;
    }

    bool matchParenthesesTerminalBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternTerminalBegin);
        ASSERT(term.atom.quantityType == QuantifierType::Greedy);
        ASSERT(term.atom.quantityMaxCount == quantifyInfinite);
        ASSERT(!term.capture());

        BackTrackInfoParenthesesTerminal* backTrack = reinterpret_cast<BackTrackInfoParenthesesTerminal*>(context->frame + term.frameLocation);
        backTrack->begin = input.getPos();
        return true;
    }

    bool matchParenthesesTerminalEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternTerminalEnd);

        BackTrackInfoParenthesesTerminal* backTrack = reinterpret_cast<BackTrackInfoParenthesesTerminal*>(context->frame + term.frameLocation);
        // Empty match is a failed match.
        if (backTrack->begin == input.getPos())
            return false;

        // Successful match! Okay, what's next? - loop around and try to match more!
        context->term -= (term.atom.parenthesesWidth + 1);
        return true;
    }

    bool backtrackParenthesesTerminalBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpatternTerminalBegin);
        ASSERT(term.atom.quantityType == QuantifierType::Greedy);
        ASSERT(term.atom.quantityMaxCount == quantifyInfinite);
        ASSERT(!term.capture());

        // If we backtrack to this point, we have failed to match this iteration of the parens.
        // Since this is greedy / zero minimum a failed is also accepted as a match!
        context->term += term.atom.parenthesesWidth;
        return true;
    }

    bool backtrackParenthesesTerminalEnd(ByteTerm&, DisjunctionContext*)
    {
        // 'Terminal' parentheses are at the end of the regex, and as such a match past end
        // should always be returned as a successful match - we should never backtrack to here.
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }

    bool matchParentheticalAssertionBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParentheticalAssertionBegin);
        ASSERT(term.atom.quantityMaxCount == 1);

        BackTrackInfoParentheticalAssertion* backTrack = reinterpret_cast<BackTrackInfoParentheticalAssertion*>(context->frame + term.frameLocation);

        backTrack->begin = input.getPos();

        return true;
    }

    bool matchParentheticalAssertionEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParentheticalAssertionEnd);
        ASSERT(term.atom.quantityMaxCount == 1);

        BackTrackInfoParentheticalAssertion* backTrack = reinterpret_cast<BackTrackInfoParentheticalAssertion*>(context->frame + term.frameLocation);

        input.setPos(backTrack->begin);

        // We've reached the end of the parens; if they are inverted, this is failure.
        if (term.invert()) {
            if (term.containsAnyCaptures()) {
                for (unsigned subpattern = term.subpatternId(); subpattern <= term.lastSubpatternId(); subpattern++)
                    output[subpattern << 1] = offsetNoMatch;
            }
            context->term -= term.atom.parenthesesWidth;
            return false;
        }

        return true;
    }

    bool backtrackParentheticalAssertionBegin(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParentheticalAssertionBegin);
        ASSERT(term.atom.quantityMaxCount == 1);

        // We've failed to match parens; if they are inverted, this is win!
        if (term.invert()) {
            context->term += term.atom.parenthesesWidth;
            return true;
        }

        if (term.matchDirection() == Backward) {
            BackTrackInfoParentheticalAssertion* backTrack = reinterpret_cast<BackTrackInfoParentheticalAssertion*>(context->frame + term.frameLocation);
            input.setPos(backTrack->begin);
        }

        return false;
    }

    bool backtrackParentheticalAssertionEnd(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParentheticalAssertionEnd);
        ASSERT(term.atom.quantityMaxCount == 1);

        BackTrackInfoParentheticalAssertion* backTrack = reinterpret_cast<BackTrackInfoParentheticalAssertion*>(context->frame + term.frameLocation);

        input.setPos(backTrack->begin);

        if (term.containsAnyCaptures()) {
            for (unsigned subpattern = term.subpatternId(); subpattern <= term.lastSubpatternId(); subpattern++)
                output[subpattern << 1] = offsetNoMatch;
        }

        context->term -= term.atom.parenthesesWidth;
        return false;
    }

    JSRegExpResult matchParentheses(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpattern);

        BackTrackInfoParentheses* backTrack = reinterpret_cast<BackTrackInfoParentheses*>(context->frame + term.frameLocation);
        ByteDisjunction* disjunctionBody = term.atom.parenthesesDisjunction;

        backTrack->begin = input.getPos();
        backTrack->matchAmount = 0;
        backTrack->lastContext = nullptr;

        ASSERT(term.atom.quantityType != QuantifierType::FixedCount || term.atom.quantityMinCount == term.atom.quantityMaxCount);

        unsigned minimumMatchCount = term.atom.quantityMinCount;
        JSRegExpResult fixedMatchResult;

        // Handle fixed matches and the minimum part of a variable length match.
        if (minimumMatchCount) {
            // While we haven't yet reached our fixed limit,
            while (backTrack->matchAmount < minimumMatchCount) {
                // Try to do a match, and it it succeeds, add it to the list.
                ParenthesesDisjunctionContext* context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                fixedMatchResult = matchDisjunction(disjunctionBody, context->getDisjunctionContext(term));
                if (fixedMatchResult == JSRegExpResult::Match)
                    appendParenthesesDisjunctionContext(backTrack, context);
                else {
                    // The match failed; try to find an alternate point to carry on from.
                    resetMatches(term, context);
                    freeParenthesesDisjunctionContext(context);
                    
                    if (fixedMatchResult != JSRegExpResult::NoMatch)
                        return fixedMatchResult;
                    JSRegExpResult backtrackResult = parenthesesDoBacktrack(term, backTrack);
                    if (backtrackResult != JSRegExpResult::Match)
                        return backtrackResult;
                }
            }

            ParenthesesDisjunctionContext* context = backTrack->lastContext;
            recordParenthesesMatch(term, context);
        }

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount: {
            ASSERT(backTrack->matchAmount == term.atom.quantityMaxCount);
            return JSRegExpResult::Match;
        }

        case QuantifierType::Greedy: {
            while (backTrack->matchAmount < term.atom.quantityMaxCount) {
                ParenthesesDisjunctionContext* context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                JSRegExpResult result = matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext(term));
                if (result == JSRegExpResult::Match)
                    appendParenthesesDisjunctionContext(backTrack, context);
                else {
                    resetMatches(term, context);
                    freeParenthesesDisjunctionContext(context);

                    if (result != JSRegExpResult::NoMatch)
                        return result;

                    break;
                }
            }

            if (backTrack->matchAmount) {
                ParenthesesDisjunctionContext* context = backTrack->lastContext;
                recordParenthesesMatch(term, context);
            }
            return JSRegExpResult::Match;
        }

        case QuantifierType::NonGreedy:
            return JSRegExpResult::Match;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return JSRegExpResult::ErrorNoMatch;
    }

    // Rules for backtracking differ depending on whether this is greedy or non-greedy.
    //
    // Greedy matches never should try just adding more - you should already have done
    // the 'more' cases.  Always backtrack, at least a leetle bit.  However cases where
    // you backtrack an item off the list needs checking, since we'll never have matched
    // the one less case.  Tracking forwards, still add as much as possible.
    //
    // Non-greedy, we've already done the one less case, so don't match on popping.
    // We haven't done the one more case, so always try to add that.
    //
    JSRegExpResult backtrackParentheses(ByteTerm& term, DisjunctionContext* context)
    {
        ASSERT(term.type == ByteTerm::Type::ParenthesesSubpattern);

        BackTrackInfoParentheses* backTrack = reinterpret_cast<BackTrackInfoParentheses*>(context->frame + term.frameLocation);
        ByteDisjunction* disjunctionBody = term.atom.parenthesesDisjunction;

        switch (term.atom.quantityType) {
        case QuantifierType::FixedCount: {
            ASSERT(backTrack->matchAmount == term.atom.quantityMaxCount);

            ParenthesesDisjunctionContext* context = nullptr;
            JSRegExpResult result = parenthesesDoBacktrack(term, backTrack);

            if (result != JSRegExpResult::Match)
                return result;

            // While we haven't yet reached our fixed limit,
            while (backTrack->matchAmount < term.atom.quantityMaxCount) {
                // Try to do a match, and it it succeeds, add it to the list.
                context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                result = matchDisjunction(disjunctionBody, context->getDisjunctionContext(term));

                if (result == JSRegExpResult::Match)
                    appendParenthesesDisjunctionContext(backTrack, context);
                else {
                    // The match failed; try to find an alternate point to carry on from.
                    resetMatches(term, context);
                    freeParenthesesDisjunctionContext(context);

                    if (result != JSRegExpResult::NoMatch)
                        return result;
                    JSRegExpResult backtrackResult = parenthesesDoBacktrack(term, backTrack);
                    if (backtrackResult != JSRegExpResult::Match)
                        return backtrackResult;
                }
            }

            ASSERT(backTrack->matchAmount == term.atom.quantityMaxCount);
            context = backTrack->lastContext;
            recordParenthesesMatch(term, context);
            return JSRegExpResult::Match;
        }

        case QuantifierType::Greedy: {
            if (!backTrack->matchAmount)
                return JSRegExpResult::NoMatch;

            ParenthesesDisjunctionContext* context = backTrack->lastContext;
            JSRegExpResult result = matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext(term), true);
            if (result == JSRegExpResult::Match) {
                while (backTrack->matchAmount < term.atom.quantityMaxCount) {
                    ParenthesesDisjunctionContext* context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                    JSRegExpResult parenthesesResult = matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext(term));
                    if (parenthesesResult == JSRegExpResult::Match)
                        appendParenthesesDisjunctionContext(backTrack, context);
                    else {
                        resetMatches(term, context);
                        freeParenthesesDisjunctionContext(context);

                        if (parenthesesResult != JSRegExpResult::NoMatch)
                            return parenthesesResult;

                        break;
                    }
                }
            } else {
                resetMatches(term, context);
                popParenthesesDisjunctionContext(backTrack);
                freeParenthesesDisjunctionContext(context);

                if (backTrack->matchAmount < term.atom.quantityMinCount) {
                    while (backTrack->matchAmount) {
                        context = backTrack->lastContext;
                        resetMatches(term, context);
                        popParenthesesDisjunctionContext(backTrack);
                        freeParenthesesDisjunctionContext(context);
                    }

                    input.setPos(backTrack->begin);
                    return result;
                }

                if (result != JSRegExpResult::NoMatch)
                    return result;
            }

            if (backTrack->matchAmount) {
                ParenthesesDisjunctionContext* context = backTrack->lastContext;
                recordParenthesesMatch(term, context);
            }
            return JSRegExpResult::Match;
        }

        case QuantifierType::NonGreedy: {
            // If we've not reached the limit, try to add one more match.
            if (backTrack->matchAmount < term.atom.quantityMaxCount) {
                ParenthesesDisjunctionContext* context = allocParenthesesDisjunctionContext(disjunctionBody, output, term);
                JSRegExpResult result = matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext(term));
                if (result == JSRegExpResult::Match) {
                    appendParenthesesDisjunctionContext(backTrack, context);
                    recordParenthesesMatch(term, context);
                    return JSRegExpResult::Match;
                }

                resetMatches(term, context);
                freeParenthesesDisjunctionContext(context);

                if (result != JSRegExpResult::NoMatch)
                    return result;
            }

            // Nope - okay backtrack looking for an alternative.
            while (backTrack->matchAmount) {
                ParenthesesDisjunctionContext* context = backTrack->lastContext;
                JSRegExpResult result = matchNonZeroDisjunction(disjunctionBody, context->getDisjunctionContext(term), true);
                if (result == JSRegExpResult::Match) {
                    // successful backtrack! we're back in the game!
                    if (backTrack->matchAmount) {
                        context = backTrack->lastContext;
                        recordParenthesesMatch(term, context);
                    }
                    return JSRegExpResult::Match;
                }

                // pop a match off the stack
                resetMatches(term, context);
                popParenthesesDisjunctionContext(backTrack);
                freeParenthesesDisjunctionContext(context);

                if (result != JSRegExpResult::NoMatch)
                    return result;
            }

            return JSRegExpResult::NoMatch;
        }
        }

        RELEASE_ASSERT_NOT_REACHED();
        return JSRegExpResult::ErrorNoMatch;
    }

    bool matchDotStarEnclosure(ByteTerm& term, DisjunctionContext* context)
    {
        UNUSED_PARAM(term);

        if (pattern->dotAll()) {
            context->matchBegin = startOffset;
            context->matchEnd = input.end();
            return true;
        }

        unsigned matchBegin = context->matchBegin;

        if (matchBegin > startOffset) {
            for (matchBegin--; true; matchBegin--) {
                if (testCharacterClass(pattern->newlineCharacterClass, input.reread(matchBegin))) {
                    ++matchBegin;
                    break;
                }

                if (matchBegin == startOffset)
                    break;
            }
        }

        unsigned matchEnd = input.getPos();

        for (; (matchEnd != input.end())
             && (!testCharacterClass(pattern->newlineCharacterClass, input.reread(matchEnd))); matchEnd++) { }

        if (((matchBegin && term.anchors.m_bol)
             || ((matchEnd != input.end()) && term.anchors.m_eol))
            && !pattern->multiline())
            return false;

        context->matchBegin = matchBegin;
        context->matchEnd = matchEnd;
        return true;
    }

#define MATCH_NEXT() { if (verbose) { dataLog(" - Match\n"); if (btrack) { dataLog("  Matching\n"); btrack = false; } }  ++context->term; goto matchAgain; }
#define BACKTRACK() { if (verbose) { dataLog(" - Backtrack\n"); if (!btrack) { dataLog("  Backtracking\n"); btrack = true; } } --context->term; goto backtrack; }
#define currentTerm() (disjunction->terms[context->term])

#define DUMP_TERM() \
{ \
    if (verbose) { \
        termDumper.dumpTerm(context->term, currentTerm()); \
        dataLog(" pos:", input.getPos()); \
    } \
}

#define DUMP_EXTRA(...) \
{ \
    dataLogIf(verbose, " ", __VA_ARGS__); \
}

#define DUMP_EXTRA_IF(predicate, ...) \
{ \
    dataLogIf(verbose && predicate, " ", __VA_ARGS__); \
}

#define DUMP_CURR_CHAR() \
{ \
    if (verbose) { \
        unsigned offset = currentTerm().inputPosition; \
        if (currentTerm().matchDirection() == Backward && currentTerm().atom.quantityType == QuantifierType::FixedCount) \
            offset -= currentTerm().atom.quantityMaxCount - 1; \
        dataLog(" off:", offset, " got:'"); \
        int ch = -1; \
        if (input.isValidNegativeInputOffset(offset)) \
            ch = input.readForCharacterDump(offset); \
        if (ch == -1) \
            dataLog("<illegal>"); \
        else if (ch < 0x10000 && (ch < 0xd800 || ch > 0xdfff))\
            dataLog(static_cast<char16_t>(ch)); \
        else \
            dataLogF("\\u{%x}", ch); \
        dataLog("'"); \
    } \
}

    JSRegExpResult matchDisjunction(ByteDisjunction* disjunction, DisjunctionContext* context, bool btrack = false)
    {
        if (UNLIKELY(!isSafeToRecurse()))
            return JSRegExpResult::ErrorNoMemory;

        if (!--remainingMatchCount) {
            dataLogLnIf(verbose, "      Reached match limit - Returning ErrorHitLimit");
            return JSRegExpResult::ErrorHitLimit;
        }

        ByteTermDumper termDumper(unicode);

        if (btrack)
            BACKTRACK();

        context->matchBegin = input.getPos();
        context->term = 0;

    matchAgain:
        ASSERT(context->term < static_cast<int>(disjunction->terms.size()));

        DUMP_TERM();

        switch (currentTerm().type) {
        case ByteTerm::Type::SubpatternBegin:
            DUMP_EXTRA_IF(currentTerm().capture(), "id:", currentTerm().subpatternId());
            MATCH_NEXT();
        case ByteTerm::Type::SubpatternEnd:
            DUMP_EXTRA_IF(currentTerm().capture(), "id:", currentTerm().subpatternId(), " - Return Match\n");
            context->matchEnd = input.getPos();
            return JSRegExpResult::Match;

        case ByteTerm::Type::BodyAlternativeBegin:
            MATCH_NEXT();
        case ByteTerm::Type::BodyAlternativeDisjunction:
        case ByteTerm::Type::BodyAlternativeEnd:
            context->matchEnd = input.getPos();
            DUMP_EXTRA("- Return Match\n");
            return JSRegExpResult::Match;

        case ByteTerm::Type::AlternativeBegin:
            MATCH_NEXT();
        case ByteTerm::Type::AlternativeDisjunction:
        case ByteTerm::Type::AlternativeEnd: {
            int offset = currentTerm().alternative.end;
            BackTrackInfoAlternative* backTrack = reinterpret_cast<BackTrackInfoAlternative*>(context->frame + currentTerm().frameLocation);
            backTrack->offset = offset;
            context->term += offset;
            MATCH_NEXT();
        }

        case ByteTerm::Type::AssertionBOL:
            if (matchAssertionBOL(currentTerm()))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::AssertionEOL:
            if (matchAssertionEOL(currentTerm()))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::AssertionWordBoundary:
            if (matchAssertionWordBoundary(currentTerm()))
                MATCH_NEXT();
            BACKTRACK();

        case ByteTerm::Type::PatternCharacterOnce:
        case ByteTerm::Type::PatternCharacterFixed: {
            DUMP_CURR_CHAR();
            if (currentTerm().matchDirection() == Forward) {
                if (unicode) {
                    if (!U_IS_BMP(currentTerm().atom.patternCharacter)) {
                        for (unsigned matchAmount = 0; matchAmount < currentTerm().atom.quantityMaxCount; ++matchAmount) {
                            if (!checkSurrogatePair(currentTerm(), currentTerm().inputPosition - 2 * matchAmount))
                                BACKTRACK();
                        }
                        MATCH_NEXT();
                    }
                }

                unsigned position = input.getPos(); // May need to back out reading a surrogate pair.

                for (unsigned matchAmount = 0; matchAmount < currentTerm().atom.quantityMaxCount; ++matchAmount) {
                    if (!checkCharacter(currentTerm(), currentTerm().inputPosition - matchAmount)) {
                        input.setPos(position);
                        BACKTRACK();
                    }
                }
            } else {
                auto& term = currentTerm();

                if (unicode) {
                    if (!U_IS_BMP(term.atom.patternCharacter)) {
                        for (unsigned matchAmount = 0; matchAmount < currentTerm().atom.quantityMaxCount; ++matchAmount) {
                            auto inputPosition = term.inputPosition + 2 * matchAmount;
                            if (input.getPos() < inputPosition)
                                BACKTRACK();
                            if (!checkSurrogatePair(term, inputPosition))
                                BACKTRACK();
                        }
                        MATCH_NEXT();
                    }
                }

                if (input.getPos() < term.inputPosition)
                    BACKTRACK();

                unsigned position = input.getPos(); // May need to back out reading a surrogate pair.

                for (unsigned matchAmount = 0; matchAmount < term.atom.quantityMaxCount; ++matchAmount) {
                    if (!checkCharacter(term, term.inputPosition + matchAmount + 1 - term.atom.quantityMaxCount)) {
                        input.setPos(position);
                        BACKTRACK();
                    }
                }
            }
            MATCH_NEXT();
        }
        case ByteTerm::Type::PatternCharacterGreedy: {
            DUMP_CURR_CHAR();
            BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + currentTerm().frameLocation);
            unsigned matchAmount = 0;
            unsigned position = input.getPos(); // May need to back out reading a surrogate pair.
            if (currentTerm().matchDirection() == Forward) {
                while ((matchAmount < currentTerm().atom.quantityMaxCount) && input.checkInput(1)) {
                    if (!checkCharacter(currentTerm(), currentTerm().inputPosition + 1)) {
                        input.setPos(position);
                        break;
                    }
                    ++matchAmount;
                    position = input.getPos();
                }
            } else {
                auto& term = currentTerm();
                if (input.getPos() < term.inputPosition)
                    BACKTRACK();

                while ((matchAmount < term.atom.quantityMaxCount) && input.tryUncheckInput(1)) {
                    if (!checkCharacter(currentTerm(), term.inputPosition)) {
                        input.setPos(position);
                        break;
                    }
                    ++matchAmount;
                    position = input.getPos();
                }
            }
            backTrack->matchAmount = matchAmount;

            MATCH_NEXT();
        }
        case ByteTerm::Type::PatternCharacterNonGreedy: {
            DUMP_CURR_CHAR();
            BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + currentTerm().frameLocation);
            backTrack->begin = input.getPos();
            backTrack->matchAmount = 0;
            MATCH_NEXT();
        }

        case ByteTerm::Type::PatternCasedCharacterOnce:
        case ByteTerm::Type::PatternCasedCharacterFixed: {
            DUMP_CURR_CHAR();
            if (unicode) {
                // Case insensitive matching of unicode characters is handled as Type::CharacterClass.
                ASSERT(U_IS_BMP(currentTerm().atom.patternCharacter));

                unsigned position = input.getPos(); // May need to back out reading a surrogate pair.

                if (currentTerm().matchDirection() == Forward) {
                    for (unsigned matchAmount = 0; matchAmount < currentTerm().atom.quantityMaxCount; ++matchAmount) {
                        if (!checkCasedCharacter(currentTerm(), currentTerm().inputPosition - matchAmount)) {
                            input.setPos(position);
                            BACKTRACK();
                        }
                    }
                } else {
                    auto& term = currentTerm();

                    if (input.getPos() < term.inputPosition)
                        BACKTRACK();

                    for (unsigned matchAmount = 0; matchAmount < currentTerm().atom.quantityMaxCount; ++matchAmount) {
                        if (!checkCasedCharacter(term, term.inputPosition - term.atom.quantityMaxCount + matchAmount + 1)) {
                            input.setPos(position);
                            BACKTRACK();
                        }
                    }
                }
                MATCH_NEXT();
            }

            for (unsigned matchAmount = 0; matchAmount < currentTerm().atom.quantityMaxCount; ++matchAmount) {
                if (!checkCasedCharacter(currentTerm(), currentTerm().inputPosition - matchAmount))
                    BACKTRACK();
            }
            MATCH_NEXT();
        }
        case ByteTerm::Type::PatternCasedCharacterGreedy: {
            DUMP_CURR_CHAR();
            BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + currentTerm().frameLocation);

            // Case insensitive matching of unicode characters is handled as Type::CharacterClass.
            ASSERT(!unicode || U_IS_BMP(currentTerm().atom.patternCharacter));

            if (currentTerm().matchDirection() == Forward) {
                unsigned matchAmount = 0;
                while ((matchAmount < currentTerm().atom.quantityMaxCount) && input.checkInput(1)) {
                    if (!checkCasedCharacter(currentTerm(), currentTerm().inputPosition + 1)) {
                        input.uncheckInput(1);
                        break;
                    }
                    ++matchAmount;
                }
                backTrack->matchAmount = matchAmount;

                MATCH_NEXT();
            } else {
                auto& term = currentTerm();

                if (input.getPos() < term.inputPosition)
                    BACKTRACK();

                unsigned position = input.getPos();
                unsigned matchAmount = 0;
                while ((matchAmount < term.atom.quantityMaxCount) && input.tryUncheckInput(1)) {
                    if (!checkCasedCharacter(term, term.inputPosition)) {
                        input.setPos(position);
                        break;
                    }

                    ++matchAmount;
                    position = input.getPos();
                }
                backTrack->matchAmount = matchAmount;

                MATCH_NEXT();
            }
        }
        case ByteTerm::Type::PatternCasedCharacterNonGreedy: {
            DUMP_CURR_CHAR();
            BackTrackInfoPatternCharacter* backTrack = reinterpret_cast<BackTrackInfoPatternCharacter*>(context->frame + currentTerm().frameLocation);

            // Case insensitive matching of unicode characters is handled as Type::CharacterClass.
            ASSERT(!unicode || U_IS_BMP(currentTerm().atom.patternCharacter));
            
            backTrack->matchAmount = 0;
            MATCH_NEXT();
        }

        case ByteTerm::Type::CharacterClass:
            DUMP_CURR_CHAR();
            if (matchCharacterClass(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::BackReference:
            if (matchBackReference(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpattern: {
            DUMP_EXTRA("\n");
            JSRegExpResult result = matchParentheses(currentTerm(), context);

            if (result == JSRegExpResult::Match) {
                DUMP_EXTRA("     ParenthesesSubpattern");
                MATCH_NEXT();
            }  else if (result != JSRegExpResult::NoMatch)
                return result;

            DUMP_EXTRA("     ParenthesesSubpattern");
            BACKTRACK();
        }
        case ByteTerm::Type::ParenthesesSubpatternOnceBegin:
            if (matchParenthesesOnceBegin(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpatternOnceEnd:
            if (matchParenthesesOnceEnd(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpatternTerminalBegin:
            if (matchParenthesesTerminalBegin(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpatternTerminalEnd:
            if (matchParenthesesTerminalEnd(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParentheticalAssertionBegin:
            if (matchParentheticalAssertionBegin(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParentheticalAssertionEnd:
            if (matchParentheticalAssertionEnd(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();

        case ByteTerm::Type::CheckInput:
            DUMP_EXTRA("count:", currentTerm().checkInputCount);
            if (input.checkInput(currentTerm().checkInputCount))
                MATCH_NEXT();
            BACKTRACK();

        case ByteTerm::Type::UncheckInput:
            DUMP_EXTRA("count:", currentTerm().checkInputCount);
            input.uncheckInput(currentTerm().checkInputCount);
            MATCH_NEXT();

        case ByteTerm::Type::HaveCheckedInput:
            DUMP_EXTRA("count:", currentTerm().checkInputCount);
            if (input.isValidNegativeInputOffset(currentTerm().checkInputCount))
                MATCH_NEXT();
            BACKTRACK();

        case ByteTerm::Type::DotStarEnclosure:
            if (matchDotStarEnclosure(currentTerm(), context)) {
                DUMP_EXTRA("- Return Match\n");
                return JSRegExpResult::Match;
            }
            BACKTRACK();
        }

        // We should never fall-through to here.
        RELEASE_ASSERT_NOT_REACHED();

    backtrack:
        ASSERT(context->term < static_cast<int>(disjunction->terms.size()));

        DUMP_TERM();

        switch (currentTerm().type) {
        case ByteTerm::Type::SubpatternBegin:
            DUMP_EXTRA("id:", currentTerm().subpatternId(), " - Return NoMatch\n");
            return JSRegExpResult::NoMatch;
        case ByteTerm::Type::SubpatternEnd:
            RELEASE_ASSERT_NOT_REACHED();

        case ByteTerm::Type::BodyAlternativeBegin:
        case ByteTerm::Type::BodyAlternativeDisjunction: {
            int offset = currentTerm().alternative.next;
            context->term += offset;
            if (offset > 0)
                MATCH_NEXT();

            if (input.atEnd() || pattern->sticky()) {
                DUMP_EXTRA("- Return NoMatch\n");
                return JSRegExpResult::NoMatch;
            }

            input.next();

            context->matchBegin = input.getPos();

            if (currentTerm().alternative.onceThrough)
                context->term += currentTerm().alternative.next;

            MATCH_NEXT();
        }
        case ByteTerm::Type::BodyAlternativeEnd:
            RELEASE_ASSERT_NOT_REACHED();

        case ByteTerm::Type::AlternativeBegin:
        case ByteTerm::Type::AlternativeDisjunction: {
            int offset = currentTerm().alternative.next;
            context->term += offset;
            if (offset > 0)
                MATCH_NEXT();
            BACKTRACK();
        }
        case ByteTerm::Type::AlternativeEnd: {
            // We should never backtrack back into an alternative of the main body of the regex.
            BackTrackInfoAlternative* backTrack = reinterpret_cast<BackTrackInfoAlternative*>(context->frame + currentTerm().frameLocation);
            unsigned offset = backTrack->offset;
            context->term -= offset;
            BACKTRACK();
        }

        case ByteTerm::Type::AssertionBOL:
        case ByteTerm::Type::AssertionEOL:
        case ByteTerm::Type::AssertionWordBoundary:
            BACKTRACK();

        case ByteTerm::Type::PatternCharacterOnce:
        case ByteTerm::Type::PatternCharacterFixed:
        case ByteTerm::Type::PatternCharacterGreedy:
        case ByteTerm::Type::PatternCharacterNonGreedy:
            if (backtrackPatternCharacter(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::PatternCasedCharacterOnce:
        case ByteTerm::Type::PatternCasedCharacterFixed:
        case ByteTerm::Type::PatternCasedCharacterGreedy:
        case ByteTerm::Type::PatternCasedCharacterNonGreedy:
            if (backtrackPatternCasedCharacter(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::CharacterClass:
            if (backtrackCharacterClass(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::BackReference:
            if (backtrackBackReference(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpattern: {
            JSRegExpResult result = backtrackParentheses(currentTerm(), context);

            if (result == JSRegExpResult::Match) {
                MATCH_NEXT();
            } else if (result != JSRegExpResult::NoMatch)
                return result;

            BACKTRACK();
        }
        case ByteTerm::Type::ParenthesesSubpatternOnceBegin:
            if (backtrackParenthesesOnceBegin(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpatternOnceEnd:
            if (backtrackParenthesesOnceEnd(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpatternTerminalBegin:
            if (backtrackParenthesesTerminalBegin(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParenthesesSubpatternTerminalEnd:
            if (backtrackParenthesesTerminalEnd(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParentheticalAssertionBegin:
            if (backtrackParentheticalAssertionBegin(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();
        case ByteTerm::Type::ParentheticalAssertionEnd:
            if (backtrackParentheticalAssertionEnd(currentTerm(), context))
                MATCH_NEXT();
            BACKTRACK();

        case ByteTerm::Type::CheckInput:
            DUMP_EXTRA("count:", currentTerm().checkInputCount);
            input.uncheckInput(currentTerm().checkInputCount);
            BACKTRACK();

        case ByteTerm::Type::UncheckInput:
            DUMP_EXTRA("count:", currentTerm().checkInputCount);
            input.checkInput(currentTerm().checkInputCount);
            BACKTRACK();

        case ByteTerm::Type::HaveCheckedInput:
            DUMP_EXTRA("count:", currentTerm().checkInputCount);
            BACKTRACK();

        case ByteTerm::Type::DotStarEnclosure:
            RELEASE_ASSERT_NOT_REACHED();
        }

        RELEASE_ASSERT_NOT_REACHED();
        return JSRegExpResult::ErrorNoMatch;
    }

    JSRegExpResult matchNonZeroDisjunction(ByteDisjunction* disjunction, DisjunctionContext* context, bool btrack = false)
    {
        JSRegExpResult result = matchDisjunction(disjunction, context, btrack);

        if (result == JSRegExpResult::Match) {
            while (context->matchBegin == context->matchEnd) {
                result = matchDisjunction(disjunction, context, true);
                if (result != JSRegExpResult::Match)
                    return result;
            }
            return JSRegExpResult::Match;
        }

        return result;
    }

    // WTF_IGNORES_THREAD_SAFETY_ANALYSIS because this function does conditional locking.
    unsigned interpret() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
    {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195970
        // [Yarr Interpreter] The interpreter doesn't have checks for stack overflow due to deep recursion
        if (!input.isAvailableInput(0))
            return offsetNoMatch;

        if (pattern->m_lock)
            pattern->m_lock->lock();
        
        for (unsigned i = 0; i < pattern->m_body->m_numSubpatterns + 1; ++i)
            output[i << 1] = offsetNoMatch;

        allocatorPool = pattern->m_allocator->startAllocator();
        RELEASE_ASSERT(allocatorPool);

        DisjunctionContext* context = allocDisjunctionContext(pattern->m_body.get());

        dataLogLnIf(verbose, "  Interpret input: ", input, "\n  Matching");

        JSRegExpResult result = matchDisjunction(pattern->m_body.get(), context, false);
        if (result == JSRegExpResult::Match) {
            output[0] = context->matchBegin;
            output[1] = context->matchEnd;
        }

        freeDisjunctionContext(context);

        pattern->m_allocator->stopAllocator();

        ASSERT((result == JSRegExpResult::Match) == (output[0] != offsetNoMatch));

        if (pattern->m_lock)
            pattern->m_lock->unlock();
        
        return output[0];
    }

    Interpreter(BytecodePattern* pattern, unsigned* output, const CharType* input, unsigned length, unsigned start)
        : pattern(pattern)
        , unicode(pattern->unicode())
        , output(output)
        , input(input, start, length, pattern->unicode())
        , startOffset(start)
        , remainingMatchCount(matchLimit)
    {
    }

private:
    inline bool isSafeToRecurse() { return m_stackCheck.isSafeToRecurse(); }

    BytecodePattern* pattern;
    bool unicode;
    unsigned* output;
    InputStream input;
    StackCheck m_stackCheck;
    WTF::BumpPointerPool* allocatorPool { nullptr };
    unsigned startOffset;
    unsigned remainingMatchCount;
};

class ByteCompiler {
    struct ParenthesesStackEntry {
        unsigned beginTerm;
        unsigned savedAlternativeIndex;
        ParenthesesStackEntry(unsigned beginTerm, unsigned savedAlternativeIndex/*, unsigned subpatternId, bool capture = false*/)
            : beginTerm(beginTerm)
            , savedAlternativeIndex(savedAlternativeIndex)
        {
        }
    };

public:
    ByteCompiler(YarrPattern& pattern)
        : m_pattern(pattern)
    {
    }

    std::unique_ptr<BytecodePattern> compile(BumpPointerAllocator* allocator, ConcurrentJSLock* lock, ErrorCode& errorCode)
    {
        if (UNLIKELY(!isSafeToRecurse())) {
            errorCode = ErrorCode::TooManyDisjunctions;
            return nullptr;
        }

        regexBegin(m_pattern.m_numSubpatterns, m_pattern.m_body->m_callFrameSize, m_pattern.m_body->m_alternatives[0]->onceThrough());
        if (auto error = emitDisjunction(m_pattern.m_body, 0, 0)) {
            errorCode = error.value();
            return nullptr;
        }
        regexEnd();

#ifdef ASSERT_ENABLED
        if (Options::dumpCompiledRegExpPatterns())
            ByteTermDumper(&m_pattern).dumpDisjunction(m_bodyDisjunction.get());
#endif

        return makeUnique<BytecodePattern>(WTFMove(m_bodyDisjunction), m_allParenthesesInfo, m_pattern, allocator, lock);
    }

    void checkInput(unsigned count)
    {
        m_bodyDisjunction->terms.append(ByteTerm::CheckInput(count));
    }

    void uncheckInput(unsigned count)
    {
        m_bodyDisjunction->terms.append(ByteTerm::UncheckInput(count));
    }

    void haveCheckedInput(unsigned count)
    {
        m_bodyDisjunction->terms.append(ByteTerm::HaveCheckedInput(count));
    }

    void assertionBOL(unsigned inputPosition)
    {
        m_bodyDisjunction->terms.append(ByteTerm::BOL(inputPosition));
    }

    void assertionEOL(unsigned inputPosition)
    {
        m_bodyDisjunction->terms.append(ByteTerm::EOL(inputPosition));
    }

    void assertionWordBoundary(bool invert, MatchDirection matchDirection, unsigned inputPosition)
    {
        m_bodyDisjunction->terms.append(ByteTerm::WordBoundary(invert, matchDirection, inputPosition));
    }

    void atomPatternCharacter(UChar32 ch, MatchDirection matchDirection, unsigned inputPosition, unsigned frameLocation, Checked<unsigned> quantityMaxCount, QuantifierType quantityType)
    {
        if (m_pattern.ignoreCase()) {
            UChar32 lo = u_tolower(ch);
            UChar32 hi = u_toupper(ch);

            if (lo != hi) {
                m_bodyDisjunction->terms.append(ByteTerm(lo, hi, inputPosition, frameLocation, quantityMaxCount, quantityType));
                m_bodyDisjunction->terms.last().m_matchDirection = matchDirection;
                return;
            }
        }

        m_bodyDisjunction->terms.append(ByteTerm(ch, inputPosition, frameLocation, quantityMaxCount, quantityType));
        m_bodyDisjunction->terms.last().m_matchDirection = matchDirection;
    }

    void atomCharacterClass(CharacterClass* characterClass, bool invert, MatchDirection matchDirection, unsigned inputPosition, unsigned frameLocation, Checked<unsigned> quantityMaxCount, QuantifierType quantityType)
    {
        m_bodyDisjunction->terms.append(ByteTerm(characterClass, invert, inputPosition));

        if (quantityType != QuantifierType::FixedCount)
            m_bodyDisjunction->terms.last().atom.quantityMinCount = 0;
        m_bodyDisjunction->terms.last().atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms.last().atom.quantityType = quantityType;
        m_bodyDisjunction->terms.last().frameLocation = frameLocation;
        m_bodyDisjunction->terms.last().m_matchDirection = matchDirection;
    }

    void atomBackReference(unsigned subpatternId, MatchDirection matchDirection, unsigned inputPosition, unsigned frameLocation, Checked<unsigned> quantityMaxCount, QuantifierType quantityType)
    {
        ASSERT(subpatternId);

        m_bodyDisjunction->terms.append(ByteTerm::BackReference(subpatternId, matchDirection, inputPosition));

        m_bodyDisjunction->terms.last().atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms.last().atom.quantityType = quantityType;
        m_bodyDisjunction->terms.last().frameLocation = frameLocation;
    }

    void atomParenthesesOnceBegin(unsigned subpatternId, MatchDirection matchDirection, bool capture, unsigned inputPosition, unsigned frameLocation, unsigned alternativeFrameLocation)
    {
        unsigned beginTerm = m_bodyDisjunction->terms.size();

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParenthesesSubpatternOnceBegin, subpatternId, capture, false, matchDirection, inputPosition));
        m_bodyDisjunction->terms.last().frameLocation = frameLocation;
        m_bodyDisjunction->terms.append(ByteTerm::AlternativeBegin());
        m_bodyDisjunction->terms.last().frameLocation = alternativeFrameLocation;

        m_parenthesesStack.append(ParenthesesStackEntry(beginTerm, m_currentAlternativeIndex));
        m_currentAlternativeIndex = beginTerm + 1;
    }

    void atomParenthesesTerminalBegin(unsigned subpatternId, MatchDirection matchDirection, bool capture, unsigned inputPosition, unsigned frameLocation, unsigned alternativeFrameLocation)
    {
        unsigned beginTerm = m_bodyDisjunction->terms.size();

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParenthesesSubpatternTerminalBegin, subpatternId, capture, false, matchDirection, inputPosition));
        m_bodyDisjunction->terms.last().frameLocation = frameLocation;
        m_bodyDisjunction->terms.append(ByteTerm::AlternativeBegin());
        m_bodyDisjunction->terms.last().frameLocation = alternativeFrameLocation;

        m_parenthesesStack.append(ParenthesesStackEntry(beginTerm, m_currentAlternativeIndex));
        m_currentAlternativeIndex = beginTerm + 1;
    }

    void atomParenthesesSubpatternBegin(unsigned subpatternId, MatchDirection matchDirection, bool capture, unsigned inputPosition, unsigned frameLocation, unsigned alternativeFrameLocation)
    {
        // Errrk! - this is a little crazy, we initially generate as a Type::ParenthesesSubpatternOnceBegin,
        // then fix this up at the end! - simplifying this should make it much clearer.
        // https://bugs.webkit.org/show_bug.cgi?id=50136

        unsigned beginTerm = m_bodyDisjunction->terms.size();

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParenthesesSubpatternOnceBegin, subpatternId, capture, false, matchDirection, inputPosition));
        m_bodyDisjunction->terms.last().frameLocation = frameLocation;
        m_bodyDisjunction->terms.append(ByteTerm::AlternativeBegin());
        m_bodyDisjunction->terms.last().frameLocation = alternativeFrameLocation;

        m_parenthesesStack.append(ParenthesesStackEntry(beginTerm, m_currentAlternativeIndex));
        m_currentAlternativeIndex = beginTerm + 1;
    }

    void atomParentheticalAssertionBegin(unsigned subpatternId, unsigned minimumSize, bool invert, MatchDirection matchDirection, unsigned frameLocation, unsigned alternativeFrameLocation)
    {
        unsigned beginTerm = m_bodyDisjunction->terms.size();

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParentheticalAssertionBegin, subpatternId, false, invert, matchDirection, minimumSize));
        m_bodyDisjunction->terms.last().frameLocation = frameLocation;
        m_bodyDisjunction->terms.append(ByteTerm::AlternativeBegin());
        m_bodyDisjunction->terms.last().frameLocation = alternativeFrameLocation;

        m_parenthesesStack.append(ParenthesesStackEntry(beginTerm, m_currentAlternativeIndex));
        m_currentAlternativeIndex = beginTerm + 1;
    }

    void atomParentheticalAssertionEnd(unsigned inputPosition, unsigned lastSubpatternId, unsigned frameLocation, Checked<unsigned> quantityMaxCount, QuantifierType quantityType)
    {
        unsigned beginTerm = popParenthesesStack();
        closeAlternative(beginTerm + 1);
        unsigned endTerm = m_bodyDisjunction->terms.size();

        ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::ParentheticalAssertionBegin);

        bool invert = m_bodyDisjunction->terms[beginTerm].invert();
        MatchDirection matchDirection = m_bodyDisjunction->terms[beginTerm].matchDirection();
        unsigned subpatternId = m_bodyDisjunction->terms[beginTerm].subpatternId();

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParentheticalAssertionEnd, subpatternId, false, invert, matchDirection, inputPosition));
        m_bodyDisjunction->terms[beginTerm].atom.parenthesesWidth = endTerm - beginTerm;
        m_bodyDisjunction->terms[endTerm].atom.parenthesesWidth = endTerm - beginTerm;
        m_bodyDisjunction->terms[beginTerm].atom.ids.lastSubpatternId = lastSubpatternId;
        m_bodyDisjunction->terms[endTerm].atom.ids.lastSubpatternId = lastSubpatternId;
        m_bodyDisjunction->terms[endTerm].frameLocation = frameLocation;

        m_bodyDisjunction->terms[beginTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityType = quantityType;
        m_bodyDisjunction->terms[endTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[endTerm].atom.quantityType = quantityType;
    }

    void assertionDotStarEnclosure(bool bolAnchored, bool eolAnchored)
    {
        m_bodyDisjunction->terms.append(ByteTerm::DotStarEnclosure(bolAnchored, eolAnchored));
    }

    unsigned popParenthesesStack()
    {
        ASSERT(m_parenthesesStack.size());
        unsigned beginTerm = m_parenthesesStack.last().beginTerm;
        m_currentAlternativeIndex = m_parenthesesStack.last().savedAlternativeIndex;
        m_parenthesesStack.removeLast();

        ASSERT(beginTerm < m_bodyDisjunction->terms.size());
        ASSERT(m_currentAlternativeIndex < m_bodyDisjunction->terms.size());

        return beginTerm;
    }

    void closeAlternative(unsigned beginTerm)
    {
        unsigned origBeginTerm = beginTerm;
        ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::AlternativeBegin);
        unsigned endIndex = m_bodyDisjunction->terms.size();

        unsigned frameLocation = m_bodyDisjunction->terms[beginTerm].frameLocation;

        if (!m_bodyDisjunction->terms[beginTerm].alternative.next)
            m_bodyDisjunction->terms.remove(beginTerm);
        else {
            while (m_bodyDisjunction->terms[beginTerm].alternative.next) {
                beginTerm += m_bodyDisjunction->terms[beginTerm].alternative.next;
                ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::AlternativeDisjunction);
                m_bodyDisjunction->terms[beginTerm].alternative.end = endIndex - beginTerm;
                m_bodyDisjunction->terms[beginTerm].frameLocation = frameLocation;
            }

            m_bodyDisjunction->terms[beginTerm].alternative.next = origBeginTerm - beginTerm;

            m_bodyDisjunction->terms.append(ByteTerm::AlternativeEnd());
            m_bodyDisjunction->terms[endIndex].frameLocation = frameLocation;
        }
    }

    void closeBodyAlternative()
    {
        unsigned beginTerm = 0;
        unsigned origBeginTerm = 0;
        ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::BodyAlternativeBegin);
        unsigned endIndex = m_bodyDisjunction->terms.size();

        unsigned frameLocation = m_bodyDisjunction->terms[beginTerm].frameLocation;

        while (m_bodyDisjunction->terms[beginTerm].alternative.next) {
            beginTerm += m_bodyDisjunction->terms[beginTerm].alternative.next;
            ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::BodyAlternativeDisjunction);
            m_bodyDisjunction->terms[beginTerm].alternative.end = endIndex - beginTerm;
            m_bodyDisjunction->terms[beginTerm].frameLocation = frameLocation;
        }

        m_bodyDisjunction->terms[beginTerm].alternative.next = origBeginTerm - beginTerm;

        m_bodyDisjunction->terms.append(ByteTerm::BodyAlternativeEnd());
        m_bodyDisjunction->terms[endIndex].frameLocation = frameLocation;
    }

    void atomParenthesesSubpatternEnd(unsigned lastSubpatternId, unsigned inputPosition, unsigned frameLocation, Checked<unsigned> quantityMinCount, Checked<unsigned> quantityMaxCount, QuantifierType quantityType, unsigned callFrameSize = 0)
    {
        unsigned beginTerm = popParenthesesStack();
        closeAlternative(beginTerm + 1);
        unsigned endTerm = m_bodyDisjunction->terms.size();

        ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::ParenthesesSubpatternOnceBegin);

        ByteTerm& parenthesesBegin = m_bodyDisjunction->terms[beginTerm];

        auto parenthesesMatchDirection = parenthesesBegin.matchDirection();
        bool capture = parenthesesBegin.capture();
        unsigned subpatternId = parenthesesBegin.subpatternId();

        unsigned numSubpatterns = lastSubpatternId - subpatternId + 1;
        auto parenthesesDisjunction = makeUnique<ByteDisjunction>(numSubpatterns, callFrameSize);

        unsigned firstTermInParentheses = beginTerm + 1;
        parenthesesDisjunction->terms.reserveInitialCapacity(endTerm - firstTermInParentheses + 2);

        parenthesesDisjunction->terms.append(ByteTerm::SubpatternBegin());
        for (unsigned termInParentheses = firstTermInParentheses; termInParentheses < endTerm; ++termInParentheses)
            parenthesesDisjunction->terms.append(m_bodyDisjunction->terms[termInParentheses]);
        parenthesesDisjunction->terms.append(ByteTerm::SubpatternEnd());

        m_bodyDisjunction->terms.shrink(beginTerm);

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParenthesesSubpattern, subpatternId, parenthesesDisjunction.get(), capture, inputPosition));
        m_bodyDisjunction->terms.last().m_matchDirection = parenthesesMatchDirection;
        m_allParenthesesInfo.append(WTFMove(parenthesesDisjunction));

        m_bodyDisjunction->terms[beginTerm].atom.quantityMinCount = quantityMinCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityType = quantityType;
        m_bodyDisjunction->terms[beginTerm].frameLocation = frameLocation;
    }

    void atomParenthesesOnceEnd(unsigned inputPosition, unsigned frameLocation, Checked<unsigned> quantityMinCount, Checked<unsigned> quantityMaxCount, QuantifierType quantityType)
    {
        unsigned beginTerm = popParenthesesStack();
        closeAlternative(beginTerm + 1);
        unsigned endTerm = m_bodyDisjunction->terms.size();

        ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::ParenthesesSubpatternOnceBegin);

        bool capture = m_bodyDisjunction->terms[beginTerm].capture();
        unsigned subpatternId = m_bodyDisjunction->terms[beginTerm].subpatternId();

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParenthesesSubpatternOnceEnd, subpatternId, capture, false, inputPosition));
        if (m_bodyDisjunction->terms[beginTerm].matchDirection() == Backward) {
            // Swap input positions for backward captures.
            m_bodyDisjunction->terms[endTerm].inputPosition = m_bodyDisjunction->terms[beginTerm].inputPosition;
            m_bodyDisjunction->terms[beginTerm].inputPosition = inputPosition;
        }
        m_bodyDisjunction->terms[beginTerm].atom.parenthesesWidth = endTerm - beginTerm;
        m_bodyDisjunction->terms[endTerm].atom.parenthesesWidth = endTerm - beginTerm;
        m_bodyDisjunction->terms[endTerm].frameLocation = frameLocation;
        m_bodyDisjunction->terms[endTerm].m_matchDirection = m_bodyDisjunction->terms[beginTerm].matchDirection();

        m_bodyDisjunction->terms[beginTerm].atom.quantityMinCount = quantityMinCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityType = quantityType;
        m_bodyDisjunction->terms[endTerm].atom.quantityMinCount = quantityMinCount;
        m_bodyDisjunction->terms[endTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[endTerm].atom.quantityType = quantityType;
    }

    void atomParenthesesTerminalEnd(unsigned inputPosition, unsigned frameLocation, Checked<unsigned> quantityMinCount, Checked<unsigned> quantityMaxCount, QuantifierType quantityType)
    {
        unsigned beginTerm = popParenthesesStack();
        closeAlternative(beginTerm + 1);
        unsigned endTerm = m_bodyDisjunction->terms.size();

        ASSERT(m_bodyDisjunction->terms[beginTerm].type == ByteTerm::Type::ParenthesesSubpatternTerminalBegin);

        if (m_bodyDisjunction->terms[beginTerm].matchDirection() == Backward)
            inputPosition = 0;
        bool capture = m_bodyDisjunction->terms[beginTerm].capture();
        unsigned subpatternId = m_bodyDisjunction->terms[beginTerm].subpatternId();

        m_bodyDisjunction->terms.append(ByteTerm(ByteTerm::Type::ParenthesesSubpatternTerminalEnd, subpatternId, capture, false, inputPosition));
        m_bodyDisjunction->terms[beginTerm].atom.parenthesesWidth = endTerm - beginTerm;
        m_bodyDisjunction->terms[endTerm].atom.parenthesesWidth = endTerm - beginTerm;
        m_bodyDisjunction->terms[endTerm].frameLocation = frameLocation;

        m_bodyDisjunction->terms[beginTerm].atom.quantityMinCount = quantityMinCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[beginTerm].atom.quantityType = quantityType;
        m_bodyDisjunction->terms[endTerm].atom.quantityMinCount = quantityMinCount;
        m_bodyDisjunction->terms[endTerm].atom.quantityMaxCount = quantityMaxCount;
        m_bodyDisjunction->terms[endTerm].atom.quantityType = quantityType;
    }

    void regexBegin(unsigned numSubpatterns, unsigned callFrameSize, bool onceThrough)
    {
        m_bodyDisjunction = makeUnique<ByteDisjunction>(numSubpatterns, callFrameSize);
        m_bodyDisjunction->terms.append(ByteTerm::BodyAlternativeBegin(onceThrough));
        m_bodyDisjunction->terms[0].frameLocation = 0;
        m_currentAlternativeIndex = 0;
    }

    void regexEnd()
    {
        closeBodyAlternative();
    }

    void alternativeBodyDisjunction(bool onceThrough)
    {
        unsigned newAlternativeIndex = m_bodyDisjunction->terms.size();
        m_bodyDisjunction->terms[m_currentAlternativeIndex].alternative.next = newAlternativeIndex - m_currentAlternativeIndex;
        m_bodyDisjunction->terms.append(ByteTerm::BodyAlternativeDisjunction(onceThrough));

        m_currentAlternativeIndex = newAlternativeIndex;
    }

    void alternativeDisjunction()
    {
        unsigned newAlternativeIndex = m_bodyDisjunction->terms.size();
        m_bodyDisjunction->terms[m_currentAlternativeIndex].alternative.next = newAlternativeIndex - m_currentAlternativeIndex;
        m_bodyDisjunction->terms.append(ByteTerm::AlternativeDisjunction());

        m_currentAlternativeIndex = newAlternativeIndex;
    }

    std::optional<ErrorCode> WARN_UNUSED_RETURN emitDisjunction(PatternDisjunction* disjunction, CheckedUint32 inputCountAlreadyChecked, unsigned parenthesesInputCountAlreadyChecked, MatchDirection matchDirection = Forward)
    {
        if (UNLIKELY(!isSafeToRecurse()))
            return ErrorCode::TooManyDisjunctions;

        for (unsigned alt = 0; alt < disjunction->m_alternatives.size(); ++alt) {
            auto currentCountAlreadyChecked = inputCountAlreadyChecked;

            PatternAlternative* alternative = disjunction->m_alternatives[alt].get();

            if (alt) {
                if (disjunction == m_pattern.m_body)
                    alternativeBodyDisjunction(alternative->onceThrough());
                else
                    alternativeDisjunction();
            }

            unsigned minimumSize = alternative->m_minimumSize;
            ASSERT(matchDirection == Backward || minimumSize >= parenthesesInputCountAlreadyChecked);

            unsigned countToCheck = 0;

            if (matchDirection == Forward)
                countToCheck = minimumSize - parenthesesInputCountAlreadyChecked;
            else if (minimumSize > parenthesesInputCountAlreadyChecked)
                countToCheck = minimumSize - parenthesesInputCountAlreadyChecked;

            if (countToCheck) {
                if (matchDirection == Forward)
                    checkInput(countToCheck);
                else {
                    // Check that we have enough input for this alternative.
                    haveCheckedInput(minimumSize);
                }

                currentCountAlreadyChecked += countToCheck;
                if (currentCountAlreadyChecked.hasOverflowed())
                    return ErrorCode::OffsetTooLarge;
            }

            auto termCount = alternative->m_terms.size();
            for (unsigned i = 0; i < termCount; ++i) {
                unsigned termIndex = matchDirection == Forward ? i : termCount - 1 - i;
                auto& term = alternative->m_terms[termIndex];

                auto currentInputPosition = [&]() {
                    return currentCountAlreadyChecked - term.inputPosition;
                };

                switch (term.type) {
                case PatternTerm::Type::AssertionBOL:
                    assertionBOL(currentInputPosition());
                    break;

                case PatternTerm::Type::AssertionEOL:
                    assertionEOL(currentInputPosition());
                    break;

                case PatternTerm::Type::AssertionWordBoundary:
                    assertionWordBoundary(term.invert(), matchDirection, currentInputPosition());
                    break;

                case PatternTerm::Type::PatternCharacter:
                    atomPatternCharacter(term.patternCharacter, matchDirection, currentInputPosition(), term.frameLocation, term.quantityMaxCount, term.quantityType);
                    break;

                case PatternTerm::Type::CharacterClass:
                    atomCharacterClass(term.characterClass, term.invert(), matchDirection, currentInputPosition(), term.frameLocation, term.quantityMaxCount, term.quantityType);
                    break;

                case PatternTerm::Type::BackReference:
                    atomBackReference(term.backReferenceSubpatternId, matchDirection, currentInputPosition(), term.frameLocation, term.quantityMaxCount, term.quantityType);
                    break;

                case PatternTerm::Type::ForwardReference:
                    break;

                case PatternTerm::Type::ParenthesesSubpattern: {
                    unsigned disjunctionAlreadyCheckedCount = 0;
                    if (term.quantityMaxCount == 1 && !term.parentheses.isCopy) {
                        unsigned alternativeFrameLocation = term.frameLocation;
                        // For QuantifierType::FixedCount we pre-check the minimum size; for greedy/non-greedy we reserve a slot in the frame.
                        if (term.quantityType == QuantifierType::FixedCount)
                            disjunctionAlreadyCheckedCount = term.parentheses.disjunction->m_minimumSize;
                        else
                            alternativeFrameLocation += YarrStackSpaceForBackTrackInfoParenthesesOnce;
                        unsigned delegateEndInputOffset = currentInputPosition();
                        atomParenthesesOnceBegin(term.parentheses.subpatternId, matchDirection, term.capture(), disjunctionAlreadyCheckedCount + delegateEndInputOffset, term.frameLocation, alternativeFrameLocation);
                        if (auto error = emitDisjunction(term.parentheses.disjunction, currentCountAlreadyChecked, disjunctionAlreadyCheckedCount, matchDirection))
                            return error;
                        atomParenthesesOnceEnd(delegateEndInputOffset, term.frameLocation, term.quantityMinCount, term.quantityMaxCount, term.quantityType);
                    } else if (term.parentheses.isTerminal) {
                        unsigned delegateEndInputOffset = currentInputPosition();
                        atomParenthesesTerminalBegin(term.parentheses.subpatternId, matchDirection, term.capture(), disjunctionAlreadyCheckedCount + delegateEndInputOffset, term.frameLocation, term.frameLocation + YarrStackSpaceForBackTrackInfoParenthesesTerminal);
                        if (auto error = emitDisjunction(term.parentheses.disjunction, currentCountAlreadyChecked, disjunctionAlreadyCheckedCount, matchDirection))
                            return error;
                        atomParenthesesTerminalEnd(delegateEndInputOffset, term.frameLocation, term.quantityMinCount, term.quantityMaxCount, term.quantityType);
                    } else {
                        unsigned delegateEndInputOffset = currentInputPosition();
                        atomParenthesesSubpatternBegin(term.parentheses.subpatternId, matchDirection, term.capture(), disjunctionAlreadyCheckedCount + delegateEndInputOffset, term.frameLocation, 0);
                        unsigned inputOffset = 0;
                        if (auto error = emitDisjunction(term.parentheses.disjunction, currentCountAlreadyChecked, inputOffset, matchDirection))
                            return error;
                        atomParenthesesSubpatternEnd(term.parentheses.lastSubpatternId, delegateEndInputOffset, term.frameLocation, term.quantityMinCount, term.quantityMaxCount, term.quantityType, term.parentheses.disjunction->m_callFrameSize);
                    }
                    break;
                }

                case PatternTerm::Type::ParentheticalAssertion: {
                    unsigned alternativeFrameLocation = term.frameLocation + YarrStackSpaceForBackTrackInfoParentheticalAssertion;
                    unsigned positiveInputOffset = currentInputPosition();
                    if (term.matchDirection() == Forward) {
                        unsigned uncheckAmount = 0;
                        if (positiveInputOffset > term.parentheses.disjunction->m_minimumSize) {
                            uncheckAmount = positiveInputOffset - term.parentheses.disjunction->m_minimumSize;
                            uncheckInput(uncheckAmount);
                            currentCountAlreadyChecked -= uncheckAmount;
                            if (currentCountAlreadyChecked.hasOverflowed())
                                return ErrorCode::OffsetTooLarge;
                        }

                        atomParentheticalAssertionBegin(term.parentheses.subpatternId, 0, term.invert(), term.matchDirection(), term.frameLocation, alternativeFrameLocation);
                        if (auto error = emitDisjunction(term.parentheses.disjunction, currentCountAlreadyChecked, positiveInputOffset - uncheckAmount, term.matchDirection()))
                            return error;
                        atomParentheticalAssertionEnd(0, term.parentheses.lastSubpatternId, term.frameLocation, term.quantityMaxCount, term.quantityType);
                        if (uncheckAmount) {
                            checkInput(uncheckAmount);
                            currentCountAlreadyChecked += uncheckAmount;
                            if (currentCountAlreadyChecked.hasOverflowed())
                                return ErrorCode::OffsetTooLarge;
                        }
                    } else { // Backward
                        unsigned uncheckAmount = 0;
                        CheckedUint32 checkedCountForLookbehind = currentCountAlreadyChecked;
                        ASSERT(checkedCountForLookbehind >= term.inputPosition);
                        checkedCountForLookbehind -= term.inputPosition;
                        auto minimumSize = term.parentheses.disjunction->m_minimumSize;
                        if (minimumSize) {
                            checkedCountForLookbehind += minimumSize;
                            if (checkedCountForLookbehind.hasOverflowed())
                                return ErrorCode::OffsetTooLarge;
                            if (checkedCountForLookbehind > currentCountAlreadyChecked
                                && !term.invert()) {
                                // Do a quick check for what is required for the lookbehind.
                                // An inverted lookbehind can "match" without processing any input.
                                haveCheckedInput(checkedCountForLookbehind);
                            }
                        }
                        atomParentheticalAssertionBegin(term.parentheses.subpatternId, term.parentheses.disjunction->m_minimumSize, term.invert(), term.matchDirection(), term.frameLocation, alternativeFrameLocation);

                        if (auto error = emitDisjunction(term.parentheses.disjunction, checkedCountForLookbehind, positiveInputOffset + minimumSize, term.matchDirection()))
                            return error;
                        atomParentheticalAssertionEnd(0, term.parentheses.lastSubpatternId, term.frameLocation, term.quantityMaxCount, term.quantityType);

                        if (uncheckAmount) {
                            checkInput(uncheckAmount);
                            currentCountAlreadyChecked += uncheckAmount;
                            if (currentCountAlreadyChecked.hasOverflowed())
                                return ErrorCode::OffsetTooLarge;
                        }
                    }
                    break;
                }

                case PatternTerm::Type::DotStarEnclosure:
                    assertionDotStarEnclosure(term.anchors.bolAnchor, term.anchors.eolAnchor);
                    break;
                }
            }

            if (matchDirection == Backward && countToCheck)
                uncheckInput(countToCheck);
        }
        return std::nullopt;
    }

private:
    inline bool isSafeToRecurse() { return m_stackCheck.isSafeToRecurse(); }

    YarrPattern& m_pattern;
    std::unique_ptr<ByteDisjunction> m_bodyDisjunction;
    StackCheck m_stackCheck;
    unsigned m_currentAlternativeIndex { 0 };
    Vector<ParenthesesStackEntry> m_parenthesesStack;
    Vector<std::unique_ptr<ByteDisjunction>> m_allParenthesesInfo;
};

void ByteTermDumper::dumpTerm(size_t idx, ByteTerm term)
{
    PrintStream& out = WTF::dataFile();

    auto outputTermIndexAndNest = [&](size_t index, unsigned termNesting) {
        if (!m_recursiveDump)
            termNesting = 1;

        for (unsigned lineIndent = 0; lineIndent < m_lineIndent; lineIndent++)
            out.print(" ");
        out.printf("%4zu", index);
        for (unsigned nestingDepth = 0; nestingDepth < termNesting; nestingDepth++)
            out.print("  ");
    };

    auto dumpQuantity = [&](ByteTerm& term) {
        if (term.atom.quantityType == QuantifierType::FixedCount) {
            if (term.atom.quantityMaxCount > 1)
                out.print(" {", term.atom.quantityMaxCount, "}");
            return;
        }

        out.print(" {", term.atom.quantityMinCount);
        if (term.atom.quantityMaxCount == UINT_MAX)
            out.print(",inf");
        else
            out.print(",", term.atom.quantityMaxCount);
        out.print("}");

        if (term.atom.quantityType == QuantifierType::Greedy)
            out.print(" greedy");
        else if (term.atom.quantityType == QuantifierType::NonGreedy)
            out.print(" non-greedy");
    };

    auto dumpCaptured = [&](ByteTerm& term) {
        if (term.capture())
            out.print(" captured (#", term.subpatternId(), ")");
    };

    auto dumpInverted = [&](ByteTerm& term) {
        if (term.invert())
            out.print(" inverted");
    };

    auto dumpMatchDirection = [&](ByteTerm& term) {
        if (term.matchDirection() == Backward) {
            if (term.type == ByteTerm::Type::ParentheticalAssertionBegin
                || term.type == ByteTerm::Type::ParentheticalAssertionEnd) {
                out.print(" lookbehind");
            } else
                out.print(" backward");
        }
    };

    auto dumpInputPosition = [&](ByteTerm& term) {
        out.printf(" inputPosition %u", term.inputPosition);
    };

    auto dumpFrameLocation = [&](ByteTerm& term) {
        out.printf(" frameLocation %u", term.frameLocation);
    };

    auto dumpCharacter = [&](ByteTerm& term) {
        out.print(" ");
        dumpUChar32(out, term.atom.patternCharacter);
    };

    auto dumpCharClass = [&](ByteTerm& term) {
        out.print(" ");
        dumpCharacterClass(out, m_pattern, term.atom.characterClass);
    };

    switch (term.type) {
    case ByteTerm::Type::BodyAlternativeBegin:
        outputTermIndexAndNest(idx, m_nesting++);
        out.print("BodyAlternativeBegin");
        if (term.alternative.onceThrough)
            out.print(" onceThrough");
        break;
    case ByteTerm::Type::BodyAlternativeDisjunction:
        outputTermIndexAndNest(idx, m_nesting - 1);
        out.print("BodyAlternativeDisjunction");
        break;
    case ByteTerm::Type::BodyAlternativeEnd:
        outputTermIndexAndNest(idx, --m_nesting);
        out.print("BodyAlternativeEnd");
        break;
    case ByteTerm::Type::AlternativeBegin:
        outputTermIndexAndNest(idx, m_nesting++);
        out.print("AlternativeBegin");
        dumpFrameLocation(term);
        break;
    case ByteTerm::Type::AlternativeDisjunction:
        outputTermIndexAndNest(idx, m_nesting - 1);
        out.print("AlternativeDisjunction");
        dumpFrameLocation(term);
        break;
    case ByteTerm::Type::AlternativeEnd:
        outputTermIndexAndNest(idx, --m_nesting);
        out.print("AlternativeEnd");
        dumpFrameLocation(term);
        break;
    case ByteTerm::Type::SubpatternBegin:
        outputTermIndexAndNest(idx, m_nesting++);
        out.print("SubpatternBegin");
        dumpMatchDirection(term);
        break;
    case ByteTerm::Type::SubpatternEnd:
        outputTermIndexAndNest(idx, --m_nesting);
        out.print("SubpatternEnd");
        dumpMatchDirection(term);
        break;
    case ByteTerm::Type::AssertionBOL:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("AssertionBOL");
        break;
    case ByteTerm::Type::AssertionEOL:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("AssertionEOL");
        break;
    case ByteTerm::Type::AssertionWordBoundary:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("AssertionWordBoundary");
        dumpInverted(term);
        dumpMatchDirection(term);
        break;
    case ByteTerm::Type::PatternCharacterOnce:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("PatternCharacterOnce");
        dumpInverted(term);
        dumpInputPosition(term);
        dumpCharacter(term);
        dumpQuantity(term);
        dumpMatchDirection(term);
        break;
    case ByteTerm::Type::PatternCharacterFixed:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("PatternCharacterFixed");
        dumpInverted(term);
        dumpInputPosition(term);
        dumpFrameLocation(term);
        dumpCharacter(term);
        out.print(" {", term.atom.quantityMinCount, "}");
        dumpMatchDirection(term);
        break;
    case ByteTerm::Type::PatternCharacterGreedy:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("PatternCharacterGreedy");
        dumpInverted(term);
        dumpInputPosition(term);
        dumpFrameLocation(term);
        dumpCharacter(term);
        dumpQuantity(term);
        dumpMatchDirection(term);
        break;
    case ByteTerm::Type::PatternCharacterNonGreedy:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("PatternCharacterNonGreedy");
        dumpInverted(term);
        dumpInputPosition(term);
        dumpFrameLocation(term);
        dumpCharacter(term);
        dumpQuantity(term);
        dumpMatchDirection(term);
        break;
    case ByteTerm::Type::PatternCasedCharacterOnce:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("PatternCasedCharacterOnce");
        dumpMatchDirection(term);
        break;
    case ByteTerm::Type::PatternCasedCharacterFixed:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("PatternCasedCharacterFixed");
        dumpMatchDirection(term);
        break;
    case ByteTerm::Type::PatternCasedCharacterGreedy:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("PatternCasedCharacterGreedy");
        dumpMatchDirection(term);
        break;
    case ByteTerm::Type::PatternCasedCharacterNonGreedy:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("PatternCasedCharacterNonGreedy");
        dumpMatchDirection(term);
        break;
    case ByteTerm::Type::CharacterClass:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("CharacterClass");
        dumpInverted(term);
        dumpInputPosition(term);
        if (term.atom.quantityType != QuantifierType::FixedCount || unicode())
            dumpFrameLocation(term);
        dumpCharClass(term);
        dumpQuantity(term);
        dumpMatchDirection(term);
        break;
    case ByteTerm::Type::BackReference:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("BackReference #", term.subpatternId());
        dumpInputPosition(term);
        dumpQuantity(term);
        break;
    case ByteTerm::Type::ParenthesesSubpattern:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("ParenthesesSubpattern");
        dumpCaptured(term);
        dumpInverted(term);
        dumpMatchDirection(term);
        dumpInputPosition(term);
        dumpFrameLocation(term);
        dumpQuantity(term);
        if (m_recursiveDump) {
            out.print("\n");
            dumpDisjunction(term.atom.parenthesesDisjunction, m_nesting);
        }
        break;
    case ByteTerm::Type::ParenthesesSubpatternOnceBegin:
        outputTermIndexAndNest(idx, m_nesting++);
        out.print("ParenthesesSubpatternOnceBegin");
        dumpCaptured(term);
        dumpInverted(term);
        dumpMatchDirection(term);
        dumpInputPosition(term);
        dumpFrameLocation(term);
        break;
    case ByteTerm::Type::ParenthesesSubpatternOnceEnd:
        outputTermIndexAndNest(idx, --m_nesting);
        out.print("ParenthesesSubpatternOnceEnd");
        dumpCaptured(term);
        dumpInverted(term);
        dumpMatchDirection(term);
        dumpInputPosition(term);
        dumpFrameLocation(term);
        break;
    case ByteTerm::Type::ParenthesesSubpatternTerminalBegin:
        outputTermIndexAndNest(idx, m_nesting++);
        out.print("ParenthesesSubpatternTerminalBegin");
        dumpInverted(term);
        dumpMatchDirection(term);
        dumpInputPosition(term);
        dumpFrameLocation(term);
        break;
    case ByteTerm::Type::ParenthesesSubpatternTerminalEnd:
        outputTermIndexAndNest(idx, --m_nesting);
        out.print("ParenthesesSubpatternTerminalEnd");
        dumpInverted(term);
        dumpMatchDirection(term);
        dumpInputPosition(term);
        dumpFrameLocation(term);
        break;
    case ByteTerm::Type::ParentheticalAssertionBegin:
        outputTermIndexAndNest(idx, m_nesting++);
        out.print("ParentheticalAssertionBegin");
        dumpInverted(term);
        dumpMatchDirection(term);
        dumpInputPosition(term);
        dumpFrameLocation(term);
        break;
    case ByteTerm::Type::ParentheticalAssertionEnd:
        outputTermIndexAndNest(idx, --m_nesting);
        out.print("ParentheticalAssertionEnd");
        dumpInverted(term);
        dumpMatchDirection(term);
        dumpInputPosition(term);
        dumpFrameLocation(term);
        break;
    case ByteTerm::Type::CheckInput:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("CheckInput ", term.checkInputCount);
        break;
    case ByteTerm::Type::UncheckInput:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("UncheckInput ", term.checkInputCount);
        break;
    case ByteTerm::Type::HaveCheckedInput:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("HaveCheckedInput ", term.checkInputCount);
        break;
    case ByteTerm::Type::DotStarEnclosure:
        outputTermIndexAndNest(idx, m_nesting);
        out.print("DotStarEnclosure");
        break;
    }
}

void ByteTermDumper::dumpDisjunction(ByteDisjunction* disjunction, unsigned nesting)
{
    PrintStream& out = WTF::dataFile();

    unsigned savedLineIndent = m_lineIndent;
    if (!nesting) {
        out.printf("ByteDisjunction(%p):\n", disjunction);
        m_recursiveDump = true;
        m_nesting = 1;
    } else
        m_lineIndent = nesting - 1;

    for (size_t idx = 0; idx < disjunction->terms.size(); ++idx) {
        ByteTerm term = disjunction->terms[idx];

        dumpTerm(idx, term);

        if (term.type != ByteTerm::Type::ParenthesesSubpattern)
            out.print("\n");
    }

    m_lineIndent = savedLineIndent;
}

std::unique_ptr<BytecodePattern> byteCompile(YarrPattern& pattern, BumpPointerAllocator* allocator, ErrorCode& errorCode, ConcurrentJSLock* lock)
{
    return ByteCompiler(pattern).compile(allocator, lock, errorCode);
}

unsigned interpret(BytecodePattern* bytecode, StringView input, unsigned start, unsigned* output)
{
    SuperSamplerScope superSamplerScope(false);
    if (input.is8Bit())
        return Interpreter<LChar>(bytecode, output, input.characters8(), input.length(), start).interpret();
    return Interpreter<UChar>(bytecode, output, input.characters16(), input.length(), start).interpret();
}

unsigned interpret(BytecodePattern* bytecode, const LChar* input, unsigned length, unsigned start, unsigned* output)
{
    SuperSamplerScope superSamplerScope(false);
    return Interpreter<LChar>(bytecode, output, input, length, start).interpret();
}

unsigned interpret(BytecodePattern* bytecode, const UChar* input, unsigned length, unsigned start, unsigned* output)
{
    SuperSamplerScope superSamplerScope(false);
    return Interpreter<UChar>(bytecode, output, input, length, start).interpret();
}

// These should be the same for both UChar & LChar.
static_assert(sizeof(BackTrackInfoPatternCharacter) == (YarrStackSpaceForBackTrackInfoPatternCharacter * sizeof(uintptr_t)));
static_assert(sizeof(BackTrackInfoCharacterClass) == (YarrStackSpaceForBackTrackInfoCharacterClass * sizeof(uintptr_t)));
static_assert(sizeof(BackTrackInfoBackReference) == (YarrStackSpaceForBackTrackInfoBackReference * sizeof(uintptr_t)));
static_assert(sizeof(BackTrackInfoAlternative) == (YarrStackSpaceForBackTrackInfoAlternative * sizeof(uintptr_t)));
static_assert(sizeof(BackTrackInfoParentheticalAssertion) == (YarrStackSpaceForBackTrackInfoParentheticalAssertion * sizeof(uintptr_t)));
static_assert(sizeof(BackTrackInfoParenthesesOnce) == (YarrStackSpaceForBackTrackInfoParenthesesOnce * sizeof(uintptr_t)));
static_assert(sizeof(Interpreter<UChar>::BackTrackInfoParentheses) <= (YarrStackSpaceForBackTrackInfoParentheses * sizeof(uintptr_t)));


} }
