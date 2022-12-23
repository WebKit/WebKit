/*
 * Copyright (C) 2009-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2019 the V8 project authors. All rights reserved.
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
#include "YarrJIT.h"

#include "CCallHelpers.h"
#include "LinkBuffer.h"
#include "Options.h"
#include "VM.h"
#include "Yarr.h"
#include "YarrCanonicalize.h"
#include "YarrDisassembler.h"
#include "YarrJITRegisters.h"
#include "YarrMatchingContextHolder.h"
#include <wtf/ASCIICType.h>
#include <wtf/HexNumber.h>
#include <wtf/ListDump.h>
#include <wtf/Threading.h>


#if ENABLE(YARR_JIT)

namespace JSC { namespace Yarr {
namespace YarrJITInternal {
static constexpr bool verbose = false;
}

#if CPU(ARM64E)
JSC_ANNOTATE_JIT_OPERATION_RETURN(vmEntryToYarrJITAfter);
#endif

void BoyerMooreFastCandidates::dump(PrintStream& out) const
{
    if (!isValid()) {
        out.print("isValid:(false)");
        return;
    }
    out.print("isValid:(true),characters:(", listDump(m_characters), ")");
}

class BoyerMooreInfo {
    WTF_MAKE_NONCOPYABLE(BoyerMooreInfo);
    WTF_MAKE_FAST_ALLOCATED(BoyerMooreInfo);
public:
    static constexpr unsigned maxLength = 32;

    explicit BoyerMooreInfo(CharSize charSize, unsigned length)
        : m_characters(length)
        , m_charSize(charSize)
    {
        ASSERT(this->length() <= maxLength);
    }

    unsigned length() const { return m_characters.size(); }
    void shortenLength(unsigned length)
    {
        ASSERT(length <= this->length());
        m_characters.shrink(length);
    }

    void set(unsigned index, UChar32 character)
    {
        m_characters[index].add(m_charSize, character);
    }

    void setAll(unsigned index)
    {
        m_characters[index].setAll();
    }

    void addCharacters(unsigned index, const Vector<UChar32>& characters)
    {
        m_characters[index].addCharacters(m_charSize, characters);
    }

    void addRanges(unsigned index, const Vector<CharacterRange>& range)
    {
        m_characters[index].addRanges(m_charSize, range);
    }

    static UniqueRef<BoyerMooreInfo> create(CharSize charSize, unsigned length)
    {
        return makeUniqueRef<BoyerMooreInfo>(charSize, length);
    }

    std::optional<std::tuple<unsigned, unsigned>> findWorthwhileCharacterSequenceForLookahead() const;
    std::tuple<BoyerMooreBitmap::Map, BoyerMooreFastCandidates> createCandidateBitmap(unsigned begin, unsigned end) const;

private:
    std::tuple<int, unsigned, unsigned> findBestCharacterSequence(unsigned numberOfCandidatesLimit) const;

    Vector<BoyerMooreBitmap> m_characters;
    CharSize m_charSize;
};

std::tuple<int, unsigned, unsigned> BoyerMooreInfo::findBestCharacterSequence(unsigned numberOfCandidatesLimit) const
{
    int biggestPoint = 0;
    unsigned beginResult = 0;
    unsigned endResult = 0;
    for (unsigned index = 0; index < length();) {
        while (index < length() && m_characters[index].count() > numberOfCandidatesLimit)
            ++index;
        if (index == length())
            break;
        unsigned begin = index;
        BoyerMooreBitmap::Map map { };
        for (; index < length() && m_characters[index].count() <= numberOfCandidatesLimit; ++index)
            map.merge(m_characters[index].map());

        // If map has many candidates, then point of this sequence is low since it will match too many things.
        // And if the sequence is longer, then the point of this sequence is higher since it can skip many characters.
        // FIXME: Currently we are handling all characters equally. But we should have weight per character since e.g. 'e' should appear more frequently than '\v'.
        // https://bugs.webkit.org/show_bug.cgi?id=228610
        int frequency = map.count();
        int matchingProbability = BoyerMooreBitmap::mapSize - frequency;
        int point = (index - begin) * matchingProbability;
        if (point > biggestPoint) {
            biggestPoint = point;
            beginResult = begin;
            endResult = index;
        }
    }
    return std::tuple { biggestPoint, beginResult, endResult };
}

std::optional<std::tuple<unsigned, unsigned>> BoyerMooreInfo::findWorthwhileCharacterSequenceForLookahead() const
{
    // If candiates-per-character becomes larger, then sequence is not profitable since this sequence will match against
    // too many characters. But if we limit candiates-per-character smaller, it is possible that we only find very short
    // character sequence. We start with low limit, then enlarging the limit to find more and more profitable
    // character sequence.
    int biggestPoint = 0;
    unsigned begin = 0;
    unsigned end = 0;
    constexpr unsigned maxCandidatesPerCharacter = 32;
    static_assert(maxCandidatesPerCharacter < BoyerMooreBitmap::mapSize);
    for (unsigned limit = 4; limit < maxCandidatesPerCharacter; limit *= 2) {
        auto [newPoint, newBegin, newEnd] = findBestCharacterSequence(limit);
        if (newPoint > biggestPoint) {
            biggestPoint = newPoint;
            begin = newBegin;
            end = newEnd;
        }
    }
    if (!biggestPoint)
        return std::nullopt;
    return std::tuple { begin, end };
}

std::tuple<BoyerMooreBitmap::Map, BoyerMooreFastCandidates> BoyerMooreInfo::createCandidateBitmap(unsigned begin, unsigned end) const
{
    BoyerMooreBitmap::Map map { };
    BoyerMooreFastCandidates charactersFastPath;
    for (unsigned index = begin; index < end; ++index) {
        auto& bmBitmap = m_characters[index];
        map.merge(bmBitmap.map());
        charactersFastPath.merge(bmBitmap.charactersFastPath());
    }
    return std::tuple { WTFMove(map), WTFMove(charactersFastPath) };
}

template<class YarrJITRegs = YarrJITDefaultRegisters>
class YarrGenerator final : public YarrJITInfo {

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    struct ParenContextSizes {
        size_t m_numSubpatterns;
        size_t m_frameSlots;

        ParenContextSizes(size_t numSubpatterns, size_t frameSlots)
            : m_numSubpatterns(numSubpatterns)
            , m_frameSlots(frameSlots)
        {
        }

        size_t numSubpatterns() { return m_numSubpatterns; }

        size_t frameSlots() { return m_frameSlots; }
    };

    struct ParenContext {
        struct ParenContext* next;
        struct BeginAndMatchAmount {
            uint32_t begin;
            uint32_t matchAmount;
        } beginAndMatchAmount;
        uintptr_t returnAddress;
        struct Subpatterns {
            unsigned start;
            unsigned end;
        } subpatterns[0];
        uintptr_t frameSlots[0];

        static size_t sizeFor(ParenContextSizes& parenContextSizes)
        {
            return sizeof(ParenContext) + sizeof(Subpatterns) * parenContextSizes.numSubpatterns() + sizeof(uintptr_t) * parenContextSizes.frameSlots();
        }

        static constexpr ptrdiff_t nextOffset()
        {
            return offsetof(ParenContext, next);
        }

        static constexpr ptrdiff_t beginOffset()
        {
            return offsetof(ParenContext, beginAndMatchAmount) + offsetof(BeginAndMatchAmount, begin);
        }

        static constexpr ptrdiff_t matchAmountOffset()
        {
            return offsetof(ParenContext, beginAndMatchAmount) + offsetof(BeginAndMatchAmount, matchAmount);
        }

        static constexpr ptrdiff_t returnAddressOffset()
        {
            return offsetof(ParenContext, returnAddress);
        }

        static constexpr ptrdiff_t subpatternOffset(size_t subpattern)
        {
            return offsetof(ParenContext, subpatterns) + (subpattern - 1) * sizeof(Subpatterns);
        }

        static ptrdiff_t savedFrameOffset(ParenContextSizes& parenContextSizes)
        {
            return offsetof(ParenContext, subpatterns) + (parenContextSizes.numSubpatterns()) * sizeof(Subpatterns);
        }
    };

    void initParenContextFreeList()
    {
        MacroAssembler::RegisterID parenContextPointer = m_regs.regT0;
        MacroAssembler::RegisterID nextParenContextPointer = m_regs.regT2;

        m_usesT2 = true;

        size_t parenContextSize = ParenContext::sizeFor(m_parenContextSizes);

        parenContextSize = WTF::roundUpToMultipleOf<sizeof(uintptr_t)>(parenContextSize);

        if (parenContextSize > VM::patternContextBufferSize) {
            m_failureReason = JITFailureReason::ParenthesisNestedTooDeep;
            return;
        }

        m_jit.load32(MacroAssembler::Address(m_regs.matchingContext, MatchingContextHolder::offsetOfPatternContextBufferSize()), m_regs.freelistSizeRegister);
        // Note that matchingContext and freelistRegister are likely the same register.
        m_jit.loadPtr(MacroAssembler::Address(m_regs.matchingContext, MatchingContextHolder::offsetOfPatternContextBuffer()), m_regs.freelistRegister);
        MacroAssembler::Jump emptyFreeList = m_jit.branchTestPtr(MacroAssembler::Zero, m_regs.freelistRegister);
        m_jit.move(m_regs.freelistRegister, parenContextPointer);
        m_jit.addPtr(MacroAssembler::TrustedImm32(parenContextSize), m_regs.freelistRegister, nextParenContextPointer);
        m_jit.addPtr(m_regs.freelistRegister, m_regs.freelistSizeRegister);
        m_jit.subPtr(MacroAssembler::TrustedImm32(parenContextSize), m_regs.freelistSizeRegister);

        MacroAssembler::Label loopTop(&m_jit);
        MacroAssembler::Jump initDone = m_jit.branchPtr(MacroAssembler::Above, nextParenContextPointer, m_regs.freelistSizeRegister);
        m_jit.storePtr(nextParenContextPointer, MacroAssembler::Address(parenContextPointer, ParenContext::nextOffset()));
        m_jit.move(nextParenContextPointer, parenContextPointer);
        m_jit.addPtr(MacroAssembler::TrustedImm32(parenContextSize), parenContextPointer, nextParenContextPointer);
        m_jit.jump(loopTop);

        initDone.link(&m_jit);
        m_jit.storePtr(MacroAssembler::TrustedImmPtr(nullptr), MacroAssembler::Address(parenContextPointer, ParenContext::nextOffset()));
        emptyFreeList.link(&m_jit);
    }

    void allocateParenContext(MacroAssembler::RegisterID result)
    {
        m_abortExecution.append(m_jit.branchTestPtr(MacroAssembler::Zero, m_regs.freelistRegister));
        m_jit.sub32(MacroAssembler::TrustedImm32(1), m_regs.remainingMatchCount);
        m_hitMatchLimit.append(m_jit.branchTestPtr(MacroAssembler::Zero, m_regs.remainingMatchCount));
        m_jit.move(m_regs.freelistRegister, result);
        m_jit.loadPtr(MacroAssembler::Address(m_regs.freelistRegister, ParenContext::nextOffset()), m_regs.freelistRegister);
    }

    void freeParenContext(MacroAssembler::RegisterID headPtrRegister, MacroAssembler::RegisterID newHeadPtrRegister)
    {
        m_jit.loadPtr(MacroAssembler::Address(headPtrRegister, ParenContext::nextOffset()), newHeadPtrRegister);
        m_jit.storePtr(m_regs.freelistRegister, MacroAssembler::Address(headPtrRegister, ParenContext::nextOffset()));
        m_jit.move(headPtrRegister, m_regs.freelistRegister);
    }

    void storeBeginAndMatchAmountToParenContext(MacroAssembler::RegisterID beginGPR, MacroAssembler::RegisterID matchAmountGPR, MacroAssembler::RegisterID parenContextGPR)
    {
        static_assert(ParenContext::beginOffset() + 4 == ParenContext::matchAmountOffset());
        m_jit.storePair32(beginGPR, matchAmountGPR, parenContextGPR, MacroAssembler::TrustedImm32(ParenContext::beginOffset()));
    }

    void loadBeginAndMatchAmountFromParenContext(MacroAssembler::RegisterID parenContextGPR, MacroAssembler::RegisterID beginGPR, MacroAssembler::RegisterID matchAmountGPR)
    {
        static_assert(ParenContext::beginOffset() + 4 == ParenContext::matchAmountOffset());
        m_jit.loadPair32(parenContextGPR, MacroAssembler::TrustedImm32(ParenContext::beginOffset()), beginGPR, matchAmountGPR);
    }

    void saveParenContext(MacroAssembler::RegisterID parenContextReg, MacroAssembler::RegisterID tempReg, unsigned firstSubpattern, unsigned lastSubpattern, unsigned subpatternBaseFrameLocation)
    {
        loadFromFrame(subpatternBaseFrameLocation + BackTrackInfoParentheses::matchAmountIndex(), tempReg);
        storeBeginAndMatchAmountToParenContext(m_regs.index, tempReg, parenContextReg);
        loadFromFrame(subpatternBaseFrameLocation + BackTrackInfoParentheses::returnAddressIndex(), tempReg);
        m_jit.storePtr(tempReg, MacroAssembler::Address(parenContextReg, ParenContext::returnAddressOffset()));
        if (m_compileMode == JITCompileMode::IncludeSubpatterns) {
            for (unsigned subpattern = firstSubpattern; subpattern <= lastSubpattern; subpattern++) {
                static_assert(is64Bit());
                m_jit.load64(MacroAssembler::Address(m_regs.output, (subpattern << 1) * sizeof(unsigned)), tempReg);
                m_jit.store64(tempReg, MacroAssembler::Address(parenContextReg, ParenContext::subpatternOffset(subpattern)));
                clearSubpatternStart(subpattern);
            }
        }
        subpatternBaseFrameLocation += YarrStackSpaceForBackTrackInfoParentheses;
        for (unsigned frameLocation = subpatternBaseFrameLocation; frameLocation < m_parenContextSizes.frameSlots(); frameLocation++) {
            loadFromFrame(frameLocation, tempReg);
            m_jit.storePtr(tempReg, MacroAssembler::Address(parenContextReg, ParenContext::savedFrameOffset(m_parenContextSizes) + frameLocation * sizeof(uintptr_t)));
        }
    }

    void restoreParenContext(MacroAssembler::RegisterID parenContextReg, MacroAssembler::RegisterID tempReg, unsigned firstSubpattern, unsigned lastSubpattern, unsigned subpatternBaseFrameLocation)
    {
        loadBeginAndMatchAmountFromParenContext(parenContextReg, m_regs.index, tempReg);
        storeToFrame(m_regs.index, subpatternBaseFrameLocation + BackTrackInfoParentheses::beginIndex());
        storeToFrame(tempReg, subpatternBaseFrameLocation + BackTrackInfoParentheses::matchAmountIndex());
        m_jit.loadPtr(MacroAssembler::Address(parenContextReg, ParenContext::returnAddressOffset()), tempReg);
        storeToFrame(tempReg, subpatternBaseFrameLocation + BackTrackInfoParentheses::returnAddressIndex());
        if (m_compileMode == JITCompileMode::IncludeSubpatterns) {
            for (unsigned subpattern = firstSubpattern; subpattern <= lastSubpattern; subpattern++) {
                static_assert(is64Bit());
                m_jit.load64(MacroAssembler::Address(parenContextReg, ParenContext::subpatternOffset(subpattern)), tempReg);
                m_jit.store64(tempReg, MacroAssembler::Address(m_regs.output, (subpattern << 1) * sizeof(unsigned)));
            }
        }
        subpatternBaseFrameLocation += YarrStackSpaceForBackTrackInfoParentheses;
        for (unsigned frameLocation = subpatternBaseFrameLocation; frameLocation < m_parenContextSizes.frameSlots(); frameLocation++) {
            m_jit.loadPtr(MacroAssembler::Address(parenContextReg, ParenContext::savedFrameOffset(m_parenContextSizes) + frameLocation * sizeof(uintptr_t)), tempReg);
            storeToFrame(tempReg, frameLocation);
        }
    }
#endif

    void optimizeAlternative(PatternAlternative* alternative)
    {
        if (!alternative->m_terms.size())
            return;

        for (unsigned i = 0; i < alternative->m_terms.size() - 1; ++i) {
            PatternTerm& term = alternative->m_terms[i];
            PatternTerm& nextTerm = alternative->m_terms[i + 1];

            // We can move BMP only character classes after fixed character terms.
            if ((term.type == PatternTerm::Type::CharacterClass)
                && (term.quantityType == QuantifierType::FixedCount)
                && (!m_decodeSurrogatePairs || (term.characterClass->hasOneCharacterSize() && !term.m_invert))
                && (nextTerm.type == PatternTerm::Type::PatternCharacter)
                && (nextTerm.quantityType == QuantifierType::FixedCount)) {
                PatternTerm termCopy = term;
                alternative->m_terms[i] = nextTerm;
                alternative->m_terms[i + 1] = termCopy;
            }
        }
    }

    void matchCharacterClassRange(MacroAssembler::RegisterID character, MacroAssembler::JumpList& failures, MacroAssembler::JumpList& matchDest, const CharacterRange* ranges, unsigned count, unsigned* matchIndex, const UChar32* matches, unsigned matchCount)
    {
        do {
            // pick which range we're going to generate
            int which = count >> 1;
            char lo = ranges[which].begin;
            char hi = ranges[which].end;

            // check if there are any ranges or matches below lo.  If not, just jl to failure -
            // if there is anything else to check, check that first, if it falls through jmp to failure.
            if ((*matchIndex < matchCount) && (matches[*matchIndex] < lo)) {
                MacroAssembler::Jump loOrAbove = m_jit.branch32(MacroAssembler::GreaterThanOrEqual, character, MacroAssembler::Imm32((unsigned short)lo));

                // generate code for all ranges before this one
                if (which)
                    matchCharacterClassRange(character, failures, matchDest, ranges, which, matchIndex, matches, matchCount);

                while ((*matchIndex < matchCount) && (matches[*matchIndex] < lo)) {
                    matchDest.append(m_jit.branch32(MacroAssembler::Equal, character, MacroAssembler::Imm32((unsigned short)matches[*matchIndex])));
                    ++*matchIndex;
                }
                failures.append(m_jit.jump());

                loOrAbove.link(&m_jit);
            } else if (which) {
                MacroAssembler::Jump loOrAbove = m_jit.branch32(MacroAssembler::GreaterThanOrEqual, character, MacroAssembler::Imm32((unsigned short)lo));

                matchCharacterClassRange(character, failures, matchDest, ranges, which, matchIndex, matches, matchCount);
                failures.append(m_jit.jump());

                loOrAbove.link(&m_jit);
            } else
                failures.append(m_jit.branch32(MacroAssembler::LessThan, character, MacroAssembler::Imm32((unsigned short)lo)));

            while ((*matchIndex < matchCount) && (matches[*matchIndex] <= hi))
                ++*matchIndex;

            matchDest.append(m_jit.branch32(MacroAssembler::LessThanOrEqual, character, MacroAssembler::Imm32((unsigned short)hi)));
            // fall through to here, the value is above hi.

            // shuffle along & loop around if there are any more matches to handle.
            unsigned next = which + 1;
            ranges += next;
            count -= next;
        } while (count);
    }

    void matchCharacterClass(MacroAssembler::RegisterID character, MacroAssembler::JumpList& matchDest, const CharacterClass* charClass)
    {
        if (charClass->m_table && !m_decodeSurrogatePairs) {
            MacroAssembler::ExtendedAddress tableEntry(character, reinterpret_cast<intptr_t>(charClass->m_table));
            matchDest.append(m_jit.branchTest8(charClass->m_tableInverted ? MacroAssembler::Zero : MacroAssembler::NonZero, tableEntry));
            return;
        }

        MacroAssembler::JumpList unicodeFail;
        if (charClass->m_matchesUnicode.size() || charClass->m_rangesUnicode.size()) {
            MacroAssembler::JumpList isAscii;
            if (charClass->m_matches.size() || charClass->m_ranges.size())
                isAscii.append(m_jit.branch32(MacroAssembler::LessThanOrEqual, character, MacroAssembler::TrustedImm32(0x7f)));

            if (charClass->m_matchesUnicode.size()) {
                for (unsigned i = 0; i < charClass->m_matchesUnicode.size(); ++i) {
                    UChar32 ch = charClass->m_matchesUnicode[i];
                    matchDest.append(m_jit.branch32(MacroAssembler::Equal, character, MacroAssembler::Imm32(ch)));
                }
            }

            if (charClass->m_rangesUnicode.size()) {
                for (unsigned i = 0; i < charClass->m_rangesUnicode.size(); ++i) {
                    UChar32 lo = charClass->m_rangesUnicode[i].begin;
                    UChar32 hi = charClass->m_rangesUnicode[i].end;

                    MacroAssembler::Jump below = m_jit.branch32(MacroAssembler::LessThan, character, MacroAssembler::Imm32(lo));
                    matchDest.append(m_jit.branch32(MacroAssembler::LessThanOrEqual, character, MacroAssembler::Imm32(hi)));
                    below.link(&m_jit);
                }
            }

            if (charClass->m_matches.size() || charClass->m_ranges.size())
                unicodeFail = m_jit.jump();
            isAscii.link(&m_jit);
        }

        if (charClass->m_ranges.size()) {
            unsigned matchIndex = 0;
            MacroAssembler::JumpList failures;
            matchCharacterClassRange(character, failures, matchDest, charClass->m_ranges.begin(), charClass->m_ranges.size(), &matchIndex, charClass->m_matches.begin(), charClass->m_matches.size());
            while (matchIndex < charClass->m_matches.size())
                matchDest.append(m_jit.branch32(MacroAssembler::Equal, character, MacroAssembler::Imm32((unsigned short)charClass->m_matches[matchIndex++])));

            failures.link(&m_jit);
        } else if (charClass->m_matches.size()) {
            // optimization: gather 'a','A' etc back together, can mask & test once.
            Vector<char> matchesAZaz;

            for (unsigned i = 0; i < charClass->m_matches.size(); ++i) {
                char ch = charClass->m_matches[i];
                if (m_pattern.ignoreCase()) {
                    if (isASCIILower(ch)) {
                        matchesAZaz.append(ch);
                        continue;
                    }
                    if (isASCIIUpper(ch))
                        continue;
                }
                matchDest.append(m_jit.branch32(MacroAssembler::Equal, character, MacroAssembler::Imm32((unsigned short)ch)));
            }

            if (unsigned countAZaz = matchesAZaz.size()) {
                m_jit.or32(MacroAssembler::TrustedImm32(32), character);
                for (unsigned i = 0; i < countAZaz; ++i)
                    matchDest.append(m_jit.branch32(MacroAssembler::Equal, character, MacroAssembler::TrustedImm32(matchesAZaz[i])));
            }
        }

        if (charClass->m_matchesUnicode.size() || charClass->m_rangesUnicode.size())
            unicodeFail.link(&m_jit);
    }

#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
    void advanceIndexAfterCharacterClassTermMatch(const PatternTerm* term, MacroAssembler::JumpList& failuresAfterIncrementingIndex, const MacroAssembler::RegisterID character)
    {
        ASSERT(term->type == PatternTerm::Type::CharacterClass);

        if (term->isFixedWidthCharacterClass())
            m_jit.add32(MacroAssembler::TrustedImm32(term->characterClass->hasNonBMPCharacters() ? 2 : 1), m_regs.index);
        else {
            m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
            MacroAssembler::Jump isBMPChar = m_jit.branch32(MacroAssembler::LessThan, character, m_regs.supplementaryPlanesBase);
            failuresAfterIncrementingIndex.append(atEndOfInput());
            m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
            isBMPChar.link(&m_jit);
        }
    }
#endif

    // Jumps if input not available; will have (incorrectly) incremented already!
    MacroAssembler::Jump jumpIfNoAvailableInput(unsigned countToCheck = 0)
    {
        if (countToCheck)
            m_jit.add32(MacroAssembler::Imm32(countToCheck), m_regs.index);
        return m_jit.branch32(MacroAssembler::Above, m_regs.index, m_regs.length);
    }

    MacroAssembler::Jump jumpIfAvailableInput(unsigned countToCheck)
    {
        m_jit.add32(MacroAssembler::Imm32(countToCheck), m_regs.index);
        return m_jit.branch32(MacroAssembler::BelowOrEqual, m_regs.index, m_regs.length);
    }

    MacroAssembler::Jump checkNotEnoughInput(MacroAssembler::RegisterID additionalAmount)
    {
        m_jit.add32(m_regs.index, additionalAmount);
        return m_jit.branch32(MacroAssembler::Above, additionalAmount, m_regs.length);
    }

    MacroAssembler::Jump checkInput()
    {
        return m_jit.branch32(MacroAssembler::BelowOrEqual, m_regs.index, m_regs.length);
    }

    MacroAssembler::Jump atEndOfInput()
    {
        return m_jit.branch32(MacroAssembler::Equal, m_regs.index, m_regs.length);
    }

    MacroAssembler::Jump notAtEndOfInput()
    {
        return m_jit.branch32(MacroAssembler::NotEqual, m_regs.index, m_regs.length);
    }

    MacroAssembler::BaseIndex negativeOffsetIndexedAddress(Checked<unsigned> negativeCharacterOffset, MacroAssembler::RegisterID tempReg)
    {
        return negativeOffsetIndexedAddress(negativeCharacterOffset, tempReg, m_regs.index);
    }

    MacroAssembler::BaseIndex negativeOffsetIndexedAddress(Checked<unsigned> negativeCharacterOffset, MacroAssembler::RegisterID tempReg, MacroAssembler::RegisterID indexReg)
    {
        MacroAssembler::RegisterID base = m_regs.input;

        // MacroAssembler::BaseIndex() addressing can take a int32_t offset. Given that we can have a regular
        // expression that has unsigned character offsets, MacroAssembler::BaseIndex's signed offset is insufficient
        // for addressing in extreme cases where we might underflow. Therefore we check to see if
        // negativeCharacterOffset will underflow directly or after converting for 16 bit characters.
        // If so, we do our own address calculating by adjusting the base, using the result register
        // as a temp address register.
        unsigned maximumNegativeOffsetForCharacterSize = m_charSize == CharSize::Char8 ? 0x7fffffff : 0x3fffffff;
        unsigned offsetAdjustAmount = 0x40000000;
        if (negativeCharacterOffset > maximumNegativeOffsetForCharacterSize) {
            base = tempReg;
            m_jit.move(m_regs.input, base);
            while (negativeCharacterOffset > maximumNegativeOffsetForCharacterSize) {
                m_jit.subPtr(MacroAssembler::TrustedImm32(offsetAdjustAmount), base);
                if (m_charSize != CharSize::Char8)
                    m_jit.subPtr(MacroAssembler::TrustedImm32(offsetAdjustAmount), base);
                negativeCharacterOffset -= offsetAdjustAmount;
            }
        }

        Checked<int32_t> characterOffset(-static_cast<int32_t>(negativeCharacterOffset));

        if (m_charSize == CharSize::Char8)
            return MacroAssembler::BaseIndex(m_regs.input, indexReg, MacroAssembler::TimesOne, characterOffset * static_cast<int32_t>(sizeof(char)));

        return MacroAssembler::BaseIndex(m_regs.input, indexReg, MacroAssembler::TimesTwo, characterOffset * static_cast<int32_t>(sizeof(UChar)));
    }

#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
    void tryReadUnicodeCharImpl(MacroAssembler::RegisterID resultReg)
    {
        ASSERT(m_charSize == CharSize::Char16);

        MacroAssembler::JumpList notUnicode;

        m_jit.load16Unaligned(MacroAssembler::Address(m_regs.regUnicodeInputAndTrail), resultReg);

        // Is the character a leading surrogate?
        m_jit.and32(m_regs.surrogateTagMask, resultReg, m_regs.unicodeTemp);
        notUnicode.append(m_jit.branch32(MacroAssembler::NotEqual, m_regs.unicodeTemp, m_regs.leadingSurrogateTag));

        // Is the input long enough to read a trailing surrogate?
        m_jit.addPtr(MacroAssembler::TrustedImm32(2), m_regs.regUnicodeInputAndTrail);
        notUnicode.append(m_jit.branchPtr(MacroAssembler::AboveOrEqual, m_regs.regUnicodeInputAndTrail, m_regs.endOfStringAddress));

        // Is the character a trailing surrogate?
        m_jit.load16Unaligned(MacroAssembler::Address(m_regs.regUnicodeInputAndTrail), m_regs.regUnicodeInputAndTrail);
        m_jit.and32(m_regs.surrogateTagMask, m_regs.regUnicodeInputAndTrail, m_regs.unicodeTemp);
        notUnicode.append(m_jit.branch32(MacroAssembler::NotEqual, m_regs.unicodeTemp, m_regs.trailingSurrogateTag));

        // Combine leading and trailing surrogates to produce a code point.
        m_jit.lshift32(MacroAssembler::TrustedImm32(10), resultReg);
        m_jit.getEffectiveAddress(MacroAssembler::BaseIndex(resultReg, m_regs.regUnicodeInputAndTrail, MacroAssembler::TimesOne, -U16_SURROGATE_OFFSET), resultReg);
        notUnicode.link(&m_jit);
    }

    void tryReadUnicodeChar(MacroAssembler::BaseIndex address, MacroAssembler::RegisterID resultReg)
    {
        ASSERT(m_charSize == CharSize::Char16);

        m_jit.getEffectiveAddress(address, m_regs.regUnicodeInputAndTrail);

        if (resultReg == m_regs.regT0)
            m_tryReadUnicodeCharacterCalls.append(m_jit.nearCall());
        else
            tryReadUnicodeCharImpl(resultReg);
    }
#endif
    
    void readCharacter(Checked<unsigned> negativeCharacterOffset, MacroAssembler::RegisterID resultReg)
    {
        readCharacter(negativeCharacterOffset, resultReg, m_regs.index);
    }

    void readCharacter(Checked<unsigned> negativeCharacterOffset, MacroAssembler::RegisterID resultReg, MacroAssembler::RegisterID indexReg)
    {
        MacroAssembler::BaseIndex address = negativeOffsetIndexedAddress(negativeCharacterOffset, resultReg, indexReg);

        if (m_charSize == CharSize::Char8)
            m_jit.load8(address, resultReg);
#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        else if (m_decodeSurrogatePairs)
            tryReadUnicodeChar(address, resultReg);
#endif
        else
            m_jit.load16Unaligned(address, resultReg);
    }

    MacroAssembler::Jump jumpIfCharNotEquals(UChar32 ch, Checked<unsigned> negativeCharacterOffset, MacroAssembler::RegisterID character)
    {
        readCharacter(negativeCharacterOffset, character);

        // For case-insesitive compares, non-ascii characters that have different
        // upper & lower case representations are converted to a character class.
        ASSERT(!m_pattern.ignoreCase() || isASCIIAlpha(ch) || isCanonicallyUnique(ch, m_canonicalMode));
        if (m_pattern.ignoreCase() && isASCIIAlpha(ch)) {
            m_jit.or32(MacroAssembler::TrustedImm32(0x20), character);
            ch |= 0x20;
        }

        return m_jit.branch32(MacroAssembler::NotEqual, character, MacroAssembler::Imm32(ch));
    }
    
    void storeToFrame(MacroAssembler::RegisterID reg, unsigned frameLocation)
    {
        m_jit.poke(reg, frameLocation);
    }

    void storeToFrame(MacroAssembler::TrustedImm32 imm, unsigned frameLocation)
    {
        m_jit.poke(imm, frameLocation);
    }

#if CPU(ARM64) || CPU(X86_64) || CPU(RISCV64)
    void storeToFrame(MacroAssembler::TrustedImmPtr imm, unsigned frameLocation)
    {
        m_jit.poke(imm, frameLocation);
    }
#endif

    MacroAssembler::DataLabelPtr storeToFrameWithPatch(unsigned frameLocation)
    {
        return m_jit.storePtrWithPatch(MacroAssembler::TrustedImmPtr(nullptr), MacroAssembler::Address(MacroAssembler::stackPointerRegister, frameLocation * sizeof(void*)));
    }

    void loadFromFrame(unsigned frameLocation, MacroAssembler::RegisterID reg)
    {
        m_jit.peek(reg, frameLocation);
    }

    void loadFromFrameAndJump(unsigned frameLocation)
    {
        m_jit.farJump(MacroAssembler::Address(MacroAssembler::stackPointerRegister, frameLocation * sizeof(void*)), YarrBacktrackPtrTag);
    }

    unsigned alignCallFrameSizeInBytes(unsigned callFrameSize)
    {
        if (!callFrameSize)
            return 0;

        callFrameSize *= sizeof(void*);
        if (callFrameSize / sizeof(void*) != m_pattern.m_body->m_callFrameSize)
            CRASH();
        callFrameSize = (callFrameSize + 0x3f) & ~0x3f;
        return callFrameSize;
    }
    void removeCallFrame()
    {
        unsigned callFrameSizeInBytes = alignCallFrameSizeInBytes(m_pattern.m_body->m_callFrameSize);
        if (callFrameSizeInBytes)
            m_jit.addPtr(MacroAssembler::Imm32(callFrameSizeInBytes), MacroAssembler::stackPointerRegister);
    }

    void generateFailReturn()
    {
        m_jit.move(MacroAssembler::TrustedImmPtr((void*)WTF::notFound), m_regs.returnRegister);
        m_jit.move(MacroAssembler::TrustedImm32(0), m_regs.returnRegister2);

#if ENABLE(YARR_JIT_REGEXP_TEST_INLINE)
        if (m_compileMode == JITCompileMode::InlineTest) {
            m_inlinedFailedMatch.append(m_jit.jump());
            return;
        }
#endif

        generateReturn();
    }

    void generateJITFailReturn()
    {
        if (m_abortExecution.empty() && m_hitMatchLimit.empty())
            return;

        MacroAssembler::JumpList finishExiting;
        if (!m_abortExecution.empty()) {
            m_abortExecution.link(&m_jit);
            m_jit.move(MacroAssembler::TrustedImmPtr((void*)static_cast<size_t>(-2)), m_regs.returnRegister);
            finishExiting.append(m_jit.jump());
        }

        if (!m_hitMatchLimit.empty()) {
            m_hitMatchLimit.link(&m_jit);
            m_jit.move(MacroAssembler::TrustedImmPtr((void*)static_cast<size_t>(-1)), m_regs.returnRegister);
        }

        finishExiting.link(&m_jit);
        removeCallFrame();
        m_jit.move(MacroAssembler::TrustedImm32(0), m_regs.returnRegister2);
        generateReturn();
    }

    // Used to record subpatterns, should only be called if m_compileMode is JITCompileMode::IncludeSubpatterns.
    void setSubpatternStart(MacroAssembler::RegisterID reg, unsigned subpattern)
    {
        ASSERT(subpattern);
        // FIXME: should be able to ASSERT(m_compileMode == JITCompileMode::IncludeSubpatterns), but then this function is conditionally NORETURN. :-(
        m_jit.store32(reg, MacroAssembler::Address(m_regs.output, (subpattern << 1) * sizeof(int)));
    }
    void setSubpatternEnd(MacroAssembler::RegisterID reg, unsigned subpattern)
    {
        ASSERT(subpattern);
        // FIXME: should be able to ASSERT(m_compileMode == JITCompileMode::IncludeSubpatterns), but then this function is conditionally NORETURN. :-(
        m_jit.store32(reg, MacroAssembler::Address(m_regs.output, ((subpattern << 1) + 1) * sizeof(int)));
    }
    void clearSubpatternStart(unsigned subpattern)
    {
        ASSERT(subpattern);
        // FIXME: should be able to ASSERT(m_compileMode == JITCompileMode::IncludeSubpatterns), but then this function is conditionally NORETURN. :-(
        m_jit.store32(MacroAssembler::TrustedImm32(-1), MacroAssembler::Address(m_regs.output, (subpattern << 1) * sizeof(int)));
    }

    // We use one of three different strategies to track the start of the current match,
    // while matching.
    // 1) If the pattern has a fixed size, do nothing! - we calculate the value lazily
    //    at the end of matching. This is irrespective of m_compileMode, and in this case
    //    these methods should never be called.
    // 2) If we're compiling JITCompileMode::IncludeSubpatterns, 'm_regs.output' contains a pointer to an output
    //    vector, store the match start in the output vector.
    // 3) If we're compiling MatchOnly or InlinedTest, 'm_regs.output' is unused, store the match start directly
    //    in this register.
    void setMatchStart(MacroAssembler::RegisterID reg)
    {
        ASSERT(!m_pattern.m_body->m_hasFixedSize);
        if (m_compileMode == JITCompileMode::IncludeSubpatterns)
            m_jit.store32(reg, MacroAssembler::Address(m_regs.output));
        else
            m_jit.move(reg, m_regs.output);
    }
    void getMatchStart(MacroAssembler::RegisterID reg)
    {
        ASSERT(!m_pattern.m_body->m_hasFixedSize);
        if (m_compileMode == JITCompileMode::IncludeSubpatterns)
            m_jit.load32(MacroAssembler::Address(m_regs.output), reg);
        else
            m_jit.move(m_regs.output, reg);
    }

    enum class YarrOpCode : uint8_t {
        // These nodes wrap body alternatives - those in the main disjunction,
        // rather than subpatterns or assertions. These are chained together in
        // a doubly linked list, with a 'begin' node for the first alternative,
        // a 'next' node for each subsequent alternative, and an 'end' node at
        // the end. In the case of repeating alternatives, the 'end' node also
        // has a reference back to 'begin'.
        BodyAlternativeBegin,
        BodyAlternativeNext,
        BodyAlternativeEnd,
        // Similar to the body alternatives, but used for subpatterns with two
        // or more alternatives.
        NestedAlternativeBegin,
        NestedAlternativeNext,
        NestedAlternativeEnd,
        // Used for alternatives in subpatterns where there is only a single
        // alternative (backtracking is easier in these cases), or for alternatives
        // which never need to be backtracked (those in parenthetical assertions,
        // terminal subpatterns).
        SimpleNestedAlternativeBegin,
        SimpleNestedAlternativeNext,
        SimpleNestedAlternativeEnd,
        // Used to wrap 'Once' subpattern matches (quantityMaxCount == 1).
        ParenthesesSubpatternOnceBegin,
        ParenthesesSubpatternOnceEnd,
        // Used to wrap 'Terminal' subpattern matches (at the end of the regexp).
        ParenthesesSubpatternTerminalBegin,
        ParenthesesSubpatternTerminalEnd,
        // Used to wrap generic captured matches
        ParenthesesSubpatternBegin,
        ParenthesesSubpatternEnd,
        // Used to wrap parenthetical assertions.
        ParentheticalAssertionBegin,
        ParentheticalAssertionEnd,
        // Wraps all simple terms (pattern characters, character classes).
        Term,
        // Where an expression contains only 'once through' body alternatives
        // and no repeating ones, this op is used to return match failure.
        MatchFailed
    };

    // This structure is used to hold the compiled opcode information,
    // including reference back to the original PatternTerm/PatternAlternatives,
    // and JIT compilation data structures.
    struct YarrOp {
        explicit YarrOp(PatternTerm* term)
            : m_term(term)
            , m_op(YarrOpCode::Term)
        {
        }

        explicit YarrOp(YarrOpCode op)
            : m_op(op)
        {
        }

        // For alternatives, this holds the PatternAlternative and doubly linked
        // references to this alternative's siblings. In the case of the
        // YarrOpCode::BodyAlternativeEnd node at the end of a section of repeating nodes,
        // m_nextOp will reference the YarrOpCode::BodyAlternativeBegin node of the first
        // repeating alternative.
        PatternAlternative* m_alternative;
        size_t m_previousOp;
        size_t m_nextOp;
        
        // The operation, as a YarrOpCode, and also a reference to the PatternTerm.
        PatternTerm* m_term;
        YarrOpCode m_op;

        // Used to record a set of Jumps out of the generated code, typically
        // used for jumps out to backtracking code, and a single reentry back
        // into the code for a node (likely where a backtrack will trigger
        // rematching).
        MacroAssembler::Label m_reentry;
        MacroAssembler::JumpList m_jumps;

        // Used for backtracking when the prior alternative did not consume any
        // characters but matched.
        MacroAssembler::Jump m_zeroLengthMatch;

        // This flag is used to null out the second pattern character, when
        // two are fused to match a pair together.
        bool m_isDeadCode { false };

        // Currently used in the case of some of the more complex management of
        // 'm_checkedOffset', to cache the offset used in this alternative, to avoid
        // recalculating it.
        Checked<unsigned> m_checkAdjust;

        // This records the current input offset being applied due to the current
        // set of alternatives we are nested within. E.g. when matching the
        // character 'b' within the regular expression /abc/, we will know that
        // the minimum size for the alternative is 3, checked upon entry to the
        // alternative, and that 'b' is at offset 1 from the start, and as such
        // when matching 'b' we need to apply an offset of -2 to the load.
        Checked<unsigned> m_checkedOffset { };

        // Used by YarrOpCode::NestedAlternativeNext/End to hold the pointer to the
        // value that will be pushed into the pattern's frame to return to,
        // upon backtracking back into the disjunction.
        MacroAssembler::DataLabelPtr m_returnAddress;

        BoyerMooreInfo* m_bmInfo { nullptr };
    };

    // BacktrackingState
    // This class encapsulates information about the state of code generation
    // whilst generating the code for backtracking, when a term fails to match.
    // Upon entry to code generation of the backtracking code for a given node,
    // the Backtracking state will hold references to all control flow sources
    // that are outputs in need of further backtracking from the prior node
    // generated (which is the subsequent operation in the regular expression,
    // and in the m_ops Vector, since we generated backtracking backwards).
    // These references to control flow take the form of:
    //  - A jump list of jumps, to be linked to code that will backtrack them
    //    further.
    //  - A set of DataLabelPtr values, to be populated with values to be
    //    treated effectively as return addresses backtracking into complex
    //    subpatterns.
    //  - A flag indicating that the current sequence of generated code up to
    //    this point requires backtracking.
    class BacktrackingState {
    private:
        struct ReturnAddressRecord {
            ReturnAddressRecord(MacroAssembler::DataLabelPtr dataLabel, MacroAssembler::Label backtrackLocation)
                : m_dataLabel(dataLabel)
                , m_backtrackLocation(backtrackLocation)
            {
            }

            MacroAssembler::DataLabelPtr m_dataLabel;
            MacroAssembler::Label m_backtrackLocation;
        };

    public:
        typedef Vector<ReturnAddressRecord, 4> BacktrackRecords;

        BacktrackingState()
            : m_pendingFallthrough(false)
        {
        }

        // Add a jump or jumps, a return address, or set the flag indicating
        // that the current 'fallthrough' control flow requires backtracking.
        void append(const MacroAssembler::Jump& jump)
        {
            m_laterFailures.append(jump);
        }
        void append(MacroAssembler::JumpList& jumpList)
        {
            m_laterFailures.append(jumpList);
        }
        void append(const MacroAssembler::DataLabelPtr& returnAddress)
        {
            m_pendingReturns.append(returnAddress);
        }
        void fallthrough()
        {
            ASSERT(!m_pendingFallthrough);
            m_pendingFallthrough = true;
        }

        // These methods clear the backtracking state, either linking to the
        // current location, a provided label, or copying the backtracking out
        // to a JumpList. All actions may require code generation to take place,
        // and as such are passed a pointer to the assembler.
        void link(MacroAssembler* assembler)
        {
            if (m_pendingReturns.size()) {
                MacroAssembler::Label here(assembler);
                for (unsigned i = 0; i < m_pendingReturns.size(); ++i)
                    m_backtrackRecords.append(ReturnAddressRecord(m_pendingReturns[i], here));
                m_pendingReturns.clear();
            }
            m_laterFailures.link(assembler);
            m_laterFailures.clear();
            m_pendingFallthrough = false;
        }
        void linkTo(MacroAssembler::Label label, MacroAssembler* assembler)
        {
            if (m_pendingReturns.size()) {
                for (unsigned i = 0; i < m_pendingReturns.size(); ++i)
                    m_backtrackRecords.append(ReturnAddressRecord(m_pendingReturns[i], label));
                m_pendingReturns.clear();
            }
            if (m_pendingFallthrough)
                assembler->jump(label);
            m_laterFailures.linkTo(label, assembler);
            m_laterFailures.clear();
            m_pendingFallthrough = false;
        }
        void takeBacktracksToJumpList(MacroAssembler::JumpList& jumpList, MacroAssembler* assembler)
        {
            if (m_pendingReturns.size()) {
                MacroAssembler::Label here(assembler);
                for (unsigned i = 0; i < m_pendingReturns.size(); ++i)
                    m_backtrackRecords.append(ReturnAddressRecord(m_pendingReturns[i], here));
                m_pendingReturns.clear();
                m_pendingFallthrough = true;
            }
            if (m_pendingFallthrough)
                jumpList.append(assembler->jump());
            jumpList.append(m_laterFailures);
            m_laterFailures.clear();
            m_pendingFallthrough = false;
        }

        bool isEmpty()
        {
            return m_laterFailures.empty() && m_pendingReturns.isEmpty() && !m_pendingFallthrough;
        }

        BacktrackRecords& backtrackRecords()
        {
            return m_backtrackRecords;
        }

        static void linkBacktrackRecords(LinkBuffer& linkBuffer, const BacktrackRecords& backtrackRecords)
        {
            for (unsigned i = 0; i < backtrackRecords.size(); ++i)
                linkBuffer.patch(backtrackRecords[i].m_dataLabel, linkBuffer.locationOf<YarrBacktrackPtrTag>(backtrackRecords[i].m_backtrackLocation));
        }

        // Called at the end of code generation to link all return addresses.
        void linkDataLabels(LinkBuffer& linkBuffer)
        {
            ASSERT(isEmpty());
            for (unsigned i = 0; i < m_backtrackRecords.size(); ++i)
                linkBuffer.patch(m_backtrackRecords[i].m_dataLabel, linkBuffer.locationOf<YarrBacktrackPtrTag>(m_backtrackRecords[i].m_backtrackLocation));
        }

    private:
        MacroAssembler::JumpList m_laterFailures;
        bool m_pendingFallthrough;
        Vector<MacroAssembler::DataLabelPtr, 4> m_pendingReturns;
        Vector<ReturnAddressRecord, 4> m_backtrackRecords;
    };

    // Generation methods:
    // ===================

    // This method provides a default implementation of backtracking common
    // to many terms; terms commonly jump out of the forwards  matching path
    // on any failed conditions, and add these jumps to the m_jumps list. If
    // no special handling is required we can often just backtrack to m_jumps.
    void backtrackTermDefault(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        m_backtrackingState.append(op.m_jumps);
    }

    void generateAssertionBOL(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        if (m_pattern.multiline()) {
            const MacroAssembler::RegisterID character = m_regs.regT0;

            MacroAssembler::JumpList matchDest;
            if (!term->inputPosition)
                matchDest.append(m_jit.branch32(MacroAssembler::Equal, m_regs.index, MacroAssembler::Imm32(op.m_checkedOffset)));

            readCharacter(op.m_checkedOffset - term->inputPosition + 1, character);
            matchCharacterClass(character, matchDest, m_pattern.newlineCharacterClass());
            op.m_jumps.append(m_jit.jump());

            matchDest.link(&m_jit);
        } else {
            // Erk, really should poison out these alternatives early. :-/
            if (term->inputPosition)
                op.m_jumps.append(m_jit.jump());
            else
                op.m_jumps.append(m_jit.branch32(MacroAssembler::NotEqual, m_regs.index, MacroAssembler::Imm32(op.m_checkedOffset)));
        }
    }
    void backtrackAssertionBOL(size_t opIndex)
    {
        backtrackTermDefault(opIndex);
    }

    void generateAssertionEOL(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        if (m_pattern.multiline()) {
            const MacroAssembler::RegisterID character = m_regs.regT0;

            MacroAssembler::JumpList matchDest;
            if (term->inputPosition == op.m_checkedOffset)
                matchDest.append(atEndOfInput());

            readCharacter(op.m_checkedOffset - term->inputPosition, character);
            matchCharacterClass(character, matchDest, m_pattern.newlineCharacterClass());
            op.m_jumps.append(m_jit.jump());

            matchDest.link(&m_jit);
        } else {
            if (term->inputPosition == op.m_checkedOffset)
                op.m_jumps.append(notAtEndOfInput());
            // Erk, really should poison out these alternatives early. :-/
            else
                op.m_jumps.append(m_jit.jump());
        }
    }
    void backtrackAssertionEOL(size_t opIndex)
    {
        backtrackTermDefault(opIndex);
    }

    // Also falls though on nextIsNotWordChar.
    void matchAssertionWordchar(size_t opIndex, MacroAssembler::JumpList& nextIsWordChar, MacroAssembler::JumpList& nextIsNotWordChar)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const MacroAssembler::RegisterID character = m_regs.regT0;

        if (term->inputPosition == op.m_checkedOffset)
            nextIsNotWordChar.append(atEndOfInput());

        readCharacter(op.m_checkedOffset - term->inputPosition, character);

        CharacterClass* wordcharCharacterClass;

        if (m_unicodeIgnoreCase)
            wordcharCharacterClass = m_pattern.wordUnicodeIgnoreCaseCharCharacterClass();
        else
            wordcharCharacterClass = m_pattern.wordcharCharacterClass();

        matchCharacterClass(character, nextIsWordChar, wordcharCharacterClass);
    }

    void generateAssertionWordBoundary(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const MacroAssembler::RegisterID character = m_regs.regT0;

        MacroAssembler::Jump atBegin;
        MacroAssembler::JumpList matchDest;
        if (!term->inputPosition)
            atBegin = m_jit.branch32(MacroAssembler::Equal, m_regs.index, MacroAssembler::Imm32(op.m_checkedOffset));
        readCharacter(op.m_checkedOffset - term->inputPosition + 1, character);

        CharacterClass* wordcharCharacterClass;

        if (m_unicodeIgnoreCase)
            wordcharCharacterClass = m_pattern.wordUnicodeIgnoreCaseCharCharacterClass();
        else
            wordcharCharacterClass = m_pattern.wordcharCharacterClass();

        matchCharacterClass(character, matchDest, wordcharCharacterClass);
        if (!term->inputPosition)
            atBegin.link(&m_jit);

        // We fall through to here if the last character was not a wordchar.
        MacroAssembler::JumpList nonWordCharThenWordChar;
        MacroAssembler::JumpList nonWordCharThenNonWordChar;
        if (term->invert()) {
            matchAssertionWordchar(opIndex, nonWordCharThenNonWordChar, nonWordCharThenWordChar);
            nonWordCharThenWordChar.append(m_jit.jump());
        } else {
            matchAssertionWordchar(opIndex, nonWordCharThenWordChar, nonWordCharThenNonWordChar);
            nonWordCharThenNonWordChar.append(m_jit.jump());
        }
        op.m_jumps.append(nonWordCharThenNonWordChar);

        // We jump here if the last character was a wordchar.
        matchDest.link(&m_jit);
        MacroAssembler::JumpList wordCharThenWordChar;
        MacroAssembler::JumpList wordCharThenNonWordChar;
        if (term->invert()) {
            matchAssertionWordchar(opIndex, wordCharThenNonWordChar, wordCharThenWordChar);
            wordCharThenWordChar.append(m_jit.jump());
        } else {
            matchAssertionWordchar(opIndex, wordCharThenWordChar, wordCharThenNonWordChar);
            // This can fall-though!
        }

        op.m_jumps.append(wordCharThenWordChar);

        nonWordCharThenWordChar.link(&m_jit);
        wordCharThenNonWordChar.link(&m_jit);
    }
    void backtrackAssertionWordBoundary(size_t opIndex)
    {
        backtrackTermDefault(opIndex);
    }

#if ENABLE(YARR_JIT_BACKREFERENCES)
    void matchBackreference(size_t opIndex, MacroAssembler::JumpList& characterMatchFails, MacroAssembler::RegisterID character, MacroAssembler::RegisterID patternIndex, MacroAssembler::RegisterID patternCharacter)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;
        unsigned subpatternId = term->backReferenceSubpatternId;

        MacroAssembler::Label loop(&m_jit);

        readCharacter(0, patternCharacter, patternIndex);
        readCharacter(op.m_checkedOffset - term->inputPosition, character);
    
        if (!m_pattern.ignoreCase())
            characterMatchFails.append(m_jit.branch32(MacroAssembler::NotEqual, character, patternCharacter));
        else {
            MacroAssembler::Jump charactersMatch = m_jit.branch32(MacroAssembler::Equal, character, patternCharacter);
            MacroAssembler::ExtendedAddress characterTableEntry(character, reinterpret_cast<intptr_t>(&canonicalTableLChar));
            m_jit.load16(characterTableEntry, character);
            MacroAssembler::ExtendedAddress patternTableEntry(patternCharacter, reinterpret_cast<intptr_t>(&canonicalTableLChar));
            m_jit.load16(patternTableEntry, patternCharacter);
            characterMatchFails.append(m_jit.branch32(MacroAssembler::NotEqual, character, patternCharacter));
            charactersMatch.link(&m_jit);
        }

        m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
        m_jit.add32(MacroAssembler::TrustedImm32(1), patternIndex);

        if (m_decodeSurrogatePairs) {
            MacroAssembler::Jump isBMPChar = m_jit.branch32(MacroAssembler::LessThan, character, m_regs.supplementaryPlanesBase);
            m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
            m_jit.add32(MacroAssembler::TrustedImm32(1), patternIndex);
            isBMPChar.link(&m_jit);
        }

        m_jit.branch32(MacroAssembler::NotEqual, patternIndex, MacroAssembler::Address(m_regs.output, ((subpatternId << 1) + 1) * sizeof(int))).linkTo(loop, &m_jit);
    }

    void generateBackReference(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        if (m_pattern.ignoreCase() && m_charSize != CharSize::Char8) {
            m_failureReason = JITFailureReason::BackReference;
            return;
        }

        unsigned subpatternId = term->backReferenceSubpatternId;
        unsigned parenthesesFrameLocation = term->frameLocation;

        const MacroAssembler::RegisterID characterOrTemp = m_regs.regT0;
        const MacroAssembler::RegisterID patternIndex = m_regs.regT1;
        const MacroAssembler::RegisterID patternTemp = m_regs.regT2;

        storeToFrame(m_regs.index, parenthesesFrameLocation + BackTrackInfoBackReference::beginIndex());
        if (term->quantityType != QuantifierType::FixedCount || term->quantityMaxCount != 1)
            storeToFrame(MacroAssembler::TrustedImm32(0), parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());

        MacroAssembler::JumpList matches;

        if (term->quantityType != QuantifierType::NonGreedy) {
            loadSubPattern(m_regs.output, subpatternId, patternIndex, patternTemp);

            // An empty match is successful without consuming characters
            if (term->quantityType != QuantifierType::FixedCount || term->quantityMaxCount != 1) {
                matches.append(m_jit.branch32(MacroAssembler::Equal, MacroAssembler::TrustedImm32(-1), patternIndex));
                matches.append(m_jit.branch32(MacroAssembler::Equal, patternIndex, patternTemp));
            } else {
                MacroAssembler::Jump zeroLengthMatch = m_jit.branch32(MacroAssembler::Equal, MacroAssembler::TrustedImm32(-1), patternIndex);
                MacroAssembler::Jump tryNonZeroMatch = m_jit.branch32(MacroAssembler::NotEqual, patternIndex, patternTemp);
                zeroLengthMatch.link(&m_jit);
                storeToFrame(MacroAssembler::TrustedImm32(1), parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());
                matches.append(m_jit.jump());
                tryNonZeroMatch.link(&m_jit);
            }
        }

        switch (term->quantityType) {
        case QuantifierType::FixedCount: {
            MacroAssembler::Label outerLoop(&m_jit);

            // PatternTemp should contain pattern end index at this point
            m_jit.sub32(patternIndex, patternTemp);
            op.m_jumps.append(checkNotEnoughInput(patternTemp));

            matchBackreference(opIndex, op.m_jumps, characterOrTemp, patternIndex, patternTemp);

            if (term->quantityMaxCount != 1) {
                loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex(), characterOrTemp);
                m_jit.add32(MacroAssembler::TrustedImm32(1), characterOrTemp);
                storeToFrame(characterOrTemp, parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());
                matches.append(m_jit.branch32(MacroAssembler::Equal, MacroAssembler::Imm32(term->quantityMaxCount), characterOrTemp));
                loadSubPattern(m_regs.output, subpatternId, patternIndex, patternTemp);
                m_jit.jump(outerLoop);
            }
            matches.link(&m_jit);
            break;
        }

        case QuantifierType::Greedy: {
            MacroAssembler::JumpList incompleteMatches;

            MacroAssembler::Label outerLoop(&m_jit);

            // PatternTemp should contain pattern end index at this point
            m_jit.sub32(patternIndex, patternTemp);
            matches.append(checkNotEnoughInput(patternTemp));

            matchBackreference(opIndex, incompleteMatches, characterOrTemp, patternIndex, patternTemp);

            loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex(), characterOrTemp);
            m_jit.add32(MacroAssembler::TrustedImm32(1), characterOrTemp);
            storeToFrame(characterOrTemp, parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());
            if (term->quantityMaxCount != quantifyInfinite)
                matches.append(m_jit.branch32(MacroAssembler::Equal, MacroAssembler::Imm32(term->quantityMaxCount), characterOrTemp));
            loadSubPattern(m_regs.output, subpatternId, patternIndex, patternTemp);

            // Store current index in frame for restoring after a partial match
            storeToFrame(m_regs.index, parenthesesFrameLocation + BackTrackInfoBackReference::beginIndex());
            m_jit.jump(outerLoop);

            incompleteMatches.link(&m_jit);
            loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::beginIndex(), m_regs.index);

            matches.link(&m_jit);
            op.m_reentry = m_jit.label();
            break;
        }

        case QuantifierType::NonGreedy: {
            MacroAssembler::JumpList incompleteMatches;

            matches.append(m_jit.jump());

            op.m_reentry = m_jit.label();

            loadSubPattern(m_regs.output, subpatternId, patternIndex, patternTemp);

            // An empty match is successful without consuming characters
            MacroAssembler::Jump zeroLengthMatch = m_jit.branch32(MacroAssembler::Equal, MacroAssembler::TrustedImm32(-1), patternIndex);
            MacroAssembler::Jump tryNonZeroMatch = m_jit.branch32(MacroAssembler::NotEqual, patternIndex, patternTemp);
            zeroLengthMatch.link(&m_jit);
            storeToFrame(MacroAssembler::TrustedImm32(1), parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());
            matches.append(m_jit.jump());
            tryNonZeroMatch.link(&m_jit);

            // Check if we have input remaining to match
            m_jit.sub32(patternIndex, patternTemp);
            matches.append(checkNotEnoughInput(patternTemp));

            storeToFrame(m_regs.index, parenthesesFrameLocation + BackTrackInfoBackReference::beginIndex());

            matchBackreference(opIndex, incompleteMatches, characterOrTemp, patternIndex, patternTemp);

            matches.append(m_jit.jump());

            incompleteMatches.link(&m_jit);
            loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::beginIndex(), m_regs.index);

            matches.link(&m_jit);
            break;
        }
        }
    }
    void backtrackBackReference(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        unsigned subpatternId = term->backReferenceSubpatternId;

        m_backtrackingState.link(&m_jit);
        op.m_jumps.link(&m_jit);

        MacroAssembler::JumpList failures;

        unsigned parenthesesFrameLocation = term->frameLocation;
        switch (term->quantityType) {
        case QuantifierType::FixedCount:
            loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::beginIndex(), m_regs.index);
            break;

        case QuantifierType::Greedy: {
            const MacroAssembler::RegisterID matchAmount = m_regs.regT0;
            const MacroAssembler::RegisterID patternStartIndex = m_regs.regT1;
            const MacroAssembler::RegisterID patternEndIndexOrLen = m_regs.regT2;

            loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex(), matchAmount);
            failures.append(m_jit.branchTest32(MacroAssembler::Zero, matchAmount));

            loadSubPattern(m_regs.output, subpatternId, patternStartIndex, patternEndIndexOrLen);
            m_jit.sub32(patternStartIndex, patternEndIndexOrLen);
            m_jit.sub32(patternEndIndexOrLen, m_regs.index);

            m_jit.sub32(MacroAssembler::TrustedImm32(1), matchAmount);
            storeToFrame(matchAmount, parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());
            m_jit.jump(op.m_reentry);
            break;
        }

        case QuantifierType::NonGreedy: {
            const MacroAssembler::RegisterID matchAmount = m_regs.regT0;

            failures.append(atEndOfInput());
            loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex(), matchAmount);
            if (term->quantityMaxCount != quantifyInfinite)
                failures.append(m_jit.branch32(MacroAssembler::AboveOrEqual, MacroAssembler::Imm32(term->quantityMaxCount), matchAmount));
            m_jit.add32(MacroAssembler::TrustedImm32(1), matchAmount);
            storeToFrame(matchAmount, parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());
            m_jit.jump(op.m_reentry);
            break;
        }
        }
        failures.link(&m_jit);
        m_backtrackingState.fallthrough();
    }
#endif

    void generatePatternCharacterOnce(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];

        if (op.m_isDeadCode)
            return;
        
        // m_ops always ends with a YarrOpCode::BodyAlternativeEnd or YarrOpCode::MatchFailed
        // node, so there must always be at least one more node.
        ASSERT(opIndex + 1 < m_ops.size());
        YarrOp* nextOp = &m_ops[opIndex + 1];

        PatternTerm* term = op.m_term;
        UChar32 ch = term->patternCharacter;

        if (!isLatin1(ch) && (m_charSize == CharSize::Char8)) {
            // Have a 16 bit pattern character and an 8 bit string - short circuit
            op.m_jumps.append(m_jit.jump());
            return;
        }

        const MacroAssembler::RegisterID character = m_regs.regT0;
#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
        unsigned maxCharactersAtOnce = m_charSize == CharSize::Char8 ? 8 : 4;
#else
        unsigned maxCharactersAtOnce = m_charSize == CharSize::Char8 ? 4 : 2;
#endif
        uint64_t ignoreCaseMask = 0;
#if CPU(BIG_ENDIAN)
        uint64_t allCharacters = ch << (m_charSize == CharSize::Char8 ? 24 : 16);
#else
        uint64_t allCharacters = ch;
#endif
        unsigned numberCharacters;
        unsigned startTermPosition = term->inputPosition;

        // For case-insesitive compares, non-ascii characters that have different
        // upper & lower case representations are converted to a character class.
        ASSERT(!m_pattern.ignoreCase() || isASCIIAlpha(ch) || isCanonicallyUnique(ch, m_canonicalMode));

        if (m_pattern.ignoreCase() && isASCIIAlpha(ch)) {
#if CPU(BIG_ENDIAN)
            ignoreCaseMask |= 32 << (m_charSize == CharSize::Char8 ? 24 : 16);
#else
            ignoreCaseMask |= 32;
#endif
        }

        for (numberCharacters = 1; numberCharacters < maxCharactersAtOnce && nextOp->m_op == YarrOpCode::Term; ++numberCharacters, nextOp = &m_ops[opIndex + numberCharacters]) {
            PatternTerm* nextTerm = nextOp->m_term;

            // YarrJIT handles decoded surrogate pair as one character if unicode flag is enabled.
            // Note that the numberCharacters become 1 while the width of the pattern character becomes 32bit in this case.
            if (nextTerm->type != PatternTerm::Type::PatternCharacter
                || nextTerm->quantityType != QuantifierType::FixedCount
                || nextTerm->quantityMaxCount != 1
                || nextTerm->inputPosition != (startTermPosition + numberCharacters)
                || (U16_LENGTH(nextTerm->patternCharacter) != 1 && m_decodeSurrogatePairs))
                break;

            nextOp->m_isDeadCode = true;

#if CPU(BIG_ENDIAN)
            int shiftAmount = (m_charSize == CharSize::Char8 ? 24 : 16) - ((m_charSize == CharSize::Char8 ? 8 : 16) * numberCharacters);
#else
            int shiftAmount = (m_charSize == CharSize::Char8 ? 8 : 16) * numberCharacters;
#endif

            UChar32 currentCharacter = nextTerm->patternCharacter;

            if (!isLatin1(currentCharacter) && (m_charSize == CharSize::Char8)) {
                // Have a 16 bit pattern character and an 8 bit string - short circuit
                op.m_jumps.append(m_jit.jump());
                return;
            }

            // For case-insesitive compares, non-ascii characters that have different
            // upper & lower case representations are converted to a character class.
            ASSERT(!m_pattern.ignoreCase() || isASCIIAlpha(currentCharacter) || isCanonicallyUnique(currentCharacter, m_canonicalMode));

            allCharacters |= (static_cast<uint64_t>(currentCharacter) << shiftAmount);

            if (m_pattern.ignoreCase() && isASCIIAlpha(currentCharacter))
                ignoreCaseMask |= 32ULL << shiftAmount;
        }

        if (m_decodeSurrogatePairs)
            op.m_jumps.append(jumpIfNoAvailableInput());

        if (m_charSize == CharSize::Char8) {
            auto check1 = [&] (Checked<unsigned> offset, UChar32 characters) {
                op.m_jumps.append(jumpIfCharNotEquals(characters, offset, character));
            };

            auto check2 = [&] (Checked<unsigned> offset, uint16_t characters, uint16_t mask) {
                m_jit.load16Unaligned(negativeOffsetIndexedAddress(offset, character), character);
                if (mask)
                    m_jit.or32(MacroAssembler::Imm32(mask), character);
                op.m_jumps.append(m_jit.branch32(MacroAssembler::NotEqual, character, MacroAssembler::Imm32(characters | mask)));
            };

            auto check4 = [&] (Checked<unsigned> offset, unsigned characters, unsigned mask) {
                if (mask) {
                    m_jit.load32WithUnalignedHalfWords(negativeOffsetIndexedAddress(offset, character), character);
                    if (mask)
                        m_jit.or32(MacroAssembler::Imm32(mask), character);
                    op.m_jumps.append(m_jit.branch32(MacroAssembler::NotEqual, character, MacroAssembler::Imm32(characters | mask)));
                    return;
                }
                op.m_jumps.append(m_jit.branch32WithUnalignedHalfWords(MacroAssembler::NotEqual, negativeOffsetIndexedAddress(offset, character), MacroAssembler::TrustedImm32(characters)));
            };

#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
            auto check8 = [&] (Checked<unsigned> offset, uint64_t characters, uint64_t mask) {
                m_jit.load64(negativeOffsetIndexedAddress(offset, character), character);
                if (mask)
                    m_jit.or64(MacroAssembler::TrustedImm64(mask), character);
                op.m_jumps.append(m_jit.branch64(MacroAssembler::NotEqual, character, MacroAssembler::TrustedImm64(characters | mask)));
            };
#endif

            switch (numberCharacters) {
            case 1:
                // Use 32bit width of allCharacters since Yarr counts surrogate pairs as one character with unicode flag.
                check1(op.m_checkedOffset - startTermPosition, allCharacters & 0xffffffff);
                return;
            case 2: {
                check2(op.m_checkedOffset - startTermPosition, allCharacters & 0xffff, ignoreCaseMask & 0xffff);
                return;
            }
            case 3: {
                check2(op.m_checkedOffset - startTermPosition, allCharacters & 0xffff, ignoreCaseMask & 0xffff);
                check1(op.m_checkedOffset - startTermPosition - 2, (allCharacters >> 16) & 0xff);
                return;
            }
            case 4: {
                check4(op.m_checkedOffset - startTermPosition, allCharacters & 0xffffffff, ignoreCaseMask & 0xffffffff);
                return;
            }
#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
            case 5: {
                check4(op.m_checkedOffset - startTermPosition, allCharacters & 0xffffffff, ignoreCaseMask & 0xffffffff);
                check1(op.m_checkedOffset - startTermPosition - 4, (allCharacters >> 32) & 0xff);
                return;
            }
            case 6: {
                check4(op.m_checkedOffset - startTermPosition, allCharacters & 0xffffffff, ignoreCaseMask & 0xffffffff);
                check2(op.m_checkedOffset - startTermPosition - 4, (allCharacters >> 32) & 0xffff, (ignoreCaseMask >> 32) & 0xffff);
                return;
            }
            case 7: {
                check4(op.m_checkedOffset - startTermPosition, allCharacters & 0xffffffff, ignoreCaseMask & 0xffffffff);
                check2(op.m_checkedOffset - startTermPosition - 4, (allCharacters >> 32) & 0xffff, (ignoreCaseMask >> 32) & 0xffff);
                check1(op.m_checkedOffset - startTermPosition - 6, (allCharacters >> 48) & 0xff);
                return;
            }
            case 8: {
                check8(op.m_checkedOffset - startTermPosition, allCharacters, ignoreCaseMask);
                return;
            }
#endif
            }
        } else {
            auto check1 = [&] (Checked<unsigned> offset, UChar32 characters) {
                op.m_jumps.append(jumpIfCharNotEquals(characters, offset, character));
            };

            auto check2 = [&] (Checked<unsigned> offset, unsigned characters, unsigned mask) {
                if (mask) {
                    m_jit.load32WithUnalignedHalfWords(negativeOffsetIndexedAddress(offset, character), character);
                    if (mask)
                        m_jit.or32(MacroAssembler::Imm32(mask), character);
                    op.m_jumps.append(m_jit.branch32(MacroAssembler::NotEqual, character, MacroAssembler::Imm32(characters | mask)));
                    return;
                }
                op.m_jumps.append(m_jit.branch32WithUnalignedHalfWords(MacroAssembler::NotEqual, negativeOffsetIndexedAddress(offset, character), MacroAssembler::TrustedImm32(characters)));
            };

#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
            auto check4 = [&] (Checked<unsigned> offset, uint64_t characters, uint64_t mask) {
                m_jit.load64(negativeOffsetIndexedAddress(offset, character), character);
                if (mask)
                    m_jit.or64(MacroAssembler::TrustedImm64(mask), character);
                op.m_jumps.append(m_jit.branch64(MacroAssembler::NotEqual, character, MacroAssembler::TrustedImm64(characters | mask)));
            };
#endif

            switch (numberCharacters) {
            case 1:
                // Use 32bit width of allCharacters since Yarr counts surrogate pairs as one character with unicode flag.
                check1(op.m_checkedOffset - startTermPosition, allCharacters & 0xffffffff);
                return;
            case 2:
                check2(op.m_checkedOffset - startTermPosition, allCharacters & 0xffffffff, ignoreCaseMask & 0xffffffff);
                return;
#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
            case 3:
                check2(op.m_checkedOffset - startTermPosition, allCharacters & 0xffffffff, ignoreCaseMask & 0xffffffff);
                check1(op.m_checkedOffset - startTermPosition - 2, (allCharacters >> 32) & 0xffff);
                return;
            case 4:
                check4(op.m_checkedOffset - startTermPosition, allCharacters, ignoreCaseMask);
                return;
#endif
            }
        }
    }
    void backtrackPatternCharacterOnce(size_t opIndex)
    {
        backtrackTermDefault(opIndex);
    }

    void generatePatternCharacterFixed(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;
        UChar32 ch = term->patternCharacter;

        const MacroAssembler::RegisterID character = m_regs.regT0;
        const MacroAssembler::RegisterID countRegister = m_regs.regT1;

        if (m_decodeSurrogatePairs)
            op.m_jumps.append(jumpIfNoAvailableInput());

        Checked<unsigned> scaledMaxCount = term->quantityMaxCount;
        scaledMaxCount *= U_IS_BMP(ch) ? 1 : 2;
        m_jit.sub32(m_regs.index, MacroAssembler::Imm32(scaledMaxCount), countRegister);

        MacroAssembler::Label loop(&m_jit);
        readCharacter(op.m_checkedOffset - term->inputPosition - scaledMaxCount, character, countRegister);
        // For case-insesitive compares, non-ascii characters that have different
        // upper & lower case representations are converted to a character class.
        ASSERT(!m_pattern.ignoreCase() || isASCIIAlpha(ch) || isCanonicallyUnique(ch, m_canonicalMode));
        if (m_pattern.ignoreCase() && isASCIIAlpha(ch)) {
            m_jit.or32(MacroAssembler::TrustedImm32(0x20), character);
            ch |= 0x20;
        }

        op.m_jumps.append(m_jit.branch32(MacroAssembler::NotEqual, character, MacroAssembler::Imm32(ch)));
#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (m_decodeSurrogatePairs && !U_IS_BMP(ch))
            m_jit.add32(MacroAssembler::TrustedImm32(2), countRegister);
        else
#endif
            m_jit.add32(MacroAssembler::TrustedImm32(1), countRegister);
        m_jit.branch32(MacroAssembler::NotEqual, countRegister, m_regs.index).linkTo(loop, &m_jit);
    }
    void backtrackPatternCharacterFixed(size_t opIndex)
    {
        backtrackTermDefault(opIndex);
    }

    void generatePatternCharacterGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;
        UChar32 ch = term->patternCharacter;

        const MacroAssembler::RegisterID character = m_regs.regT0;
        const MacroAssembler::RegisterID countRegister = m_regs.regT1;

        m_jit.move(MacroAssembler::TrustedImm32(0), countRegister);

        // Unless have a 16 bit pattern character and an 8 bit string - short circuit
        if (!(!isLatin1(ch) && (m_charSize == CharSize::Char8))) {
            MacroAssembler::JumpList failures;
            MacroAssembler::Label loop(&m_jit);
            failures.append(atEndOfInput());
            failures.append(jumpIfCharNotEquals(ch, op.m_checkedOffset - term->inputPosition, character));

            m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
            if (m_decodeSurrogatePairs && !U_IS_BMP(ch)) {
                MacroAssembler::Jump surrogatePairOk = notAtEndOfInput();
                m_jit.sub32(MacroAssembler::TrustedImm32(1), m_regs.index);
                failures.append(m_jit.jump());
                surrogatePairOk.link(&m_jit);
                m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
            }
#endif
            m_jit.add32(MacroAssembler::TrustedImm32(1), countRegister);

            if (term->quantityMaxCount == quantifyInfinite)
                m_jit.jump(loop);
            else
                m_jit.branch32(MacroAssembler::NotEqual, countRegister, MacroAssembler::Imm32(term->quantityMaxCount)).linkTo(loop, &m_jit);

            failures.link(&m_jit);
        }
        op.m_reentry = m_jit.label();

        storeToFrame(countRegister, term->frameLocation + BackTrackInfoPatternCharacter::matchAmountIndex());
    }
    void backtrackPatternCharacterGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const MacroAssembler::RegisterID countRegister = m_regs.regT1;

        m_backtrackingState.link(&m_jit);

        loadFromFrame(term->frameLocation + BackTrackInfoPatternCharacter::matchAmountIndex(), countRegister);
        m_backtrackingState.append(m_jit.branchTest32(MacroAssembler::Zero, countRegister));
        m_jit.sub32(MacroAssembler::TrustedImm32(1), countRegister);
        if (!m_decodeSurrogatePairs || U_IS_BMP(term->patternCharacter))
            m_jit.sub32(MacroAssembler::TrustedImm32(1), m_regs.index);
        else
            m_jit.sub32(MacroAssembler::TrustedImm32(2), m_regs.index);
        m_jit.jump(op.m_reentry);
    }

    void generatePatternCharacterNonGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const MacroAssembler::RegisterID countRegister = m_regs.regT1;

        m_jit.move(MacroAssembler::TrustedImm32(0), countRegister);
        op.m_reentry = m_jit.label();
        storeToFrame(countRegister, term->frameLocation + BackTrackInfoPatternCharacter::matchAmountIndex());
    }
    void backtrackPatternCharacterNonGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;
        UChar32 ch = term->patternCharacter;

        const MacroAssembler::RegisterID character = m_regs.regT0;
        const MacroAssembler::RegisterID countRegister = m_regs.regT1;

        m_backtrackingState.link(&m_jit);

        loadFromFrame(term->frameLocation + BackTrackInfoPatternCharacter::matchAmountIndex(), countRegister);

        // Unless have a 16 bit pattern character and an 8 bit string - short circuit
        if (!(!isLatin1(ch) && (m_charSize == CharSize::Char8))) {
            MacroAssembler::JumpList nonGreedyFailures;
            nonGreedyFailures.append(atEndOfInput());
            if (term->quantityMaxCount != quantifyInfinite)
                nonGreedyFailures.append(m_jit.branch32(MacroAssembler::Equal, countRegister, MacroAssembler::Imm32(term->quantityMaxCount)));
            nonGreedyFailures.append(jumpIfCharNotEquals(ch, op.m_checkedOffset - term->inputPosition, character));

            m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
            if (m_decodeSurrogatePairs && !U_IS_BMP(ch)) {
                MacroAssembler::Jump surrogatePairOk = notAtEndOfInput();
                m_jit.sub32(MacroAssembler::TrustedImm32(1), m_regs.index);
                nonGreedyFailures.append(m_jit.jump());
                surrogatePairOk.link(&m_jit);
                m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
            }
#endif
            m_jit.add32(MacroAssembler::TrustedImm32(1), countRegister);

            m_jit.jump(op.m_reentry);
            nonGreedyFailures.link(&m_jit);
        }

        if (m_decodeSurrogatePairs && !U_IS_BMP(ch)) {
            // subtract countRegister*2 for non-BMP characters
            m_jit.lshift32(MacroAssembler::TrustedImm32(1), countRegister);
        }

        m_jit.sub32(countRegister, m_regs.index);
        m_backtrackingState.fallthrough();
    }

    void generateCharacterClassOnce(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const MacroAssembler::RegisterID character = m_regs.regT0;

        if (m_decodeSurrogatePairs) {
            op.m_jumps.append(jumpIfNoAvailableInput());
            storeToFrame(m_regs.index, term->frameLocation + BackTrackInfoCharacterClass::beginIndex());
        }

        MacroAssembler::JumpList matchDest;
        readCharacter(op.m_checkedOffset - term->inputPosition, character);
        // If we are matching the "any character" builtin class we only need to read the
        // character and don't need to match as it will always succeed.
        if (term->invert() || !term->characterClass->m_anyCharacter) {
            matchCharacterClass(character, matchDest, term->characterClass);

            if (term->invert())
                op.m_jumps.append(matchDest);
            else {
                op.m_jumps.append(m_jit.jump());
                matchDest.link(&m_jit);
            }
        }
#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (m_decodeSurrogatePairs && (!term->characterClass->hasOneCharacterSize() || term->invert())) {
            MacroAssembler::Jump isBMPChar = m_jit.branch32(MacroAssembler::LessThan, character, m_regs.supplementaryPlanesBase);
            op.m_jumps.append(atEndOfInput());
            m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
            isBMPChar.link(&m_jit);
        }
#endif
    }
    void backtrackCharacterClassOnce(size_t opIndex)
    {
#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (m_decodeSurrogatePairs) {
            YarrOp& op = m_ops[opIndex];
            PatternTerm* term = op.m_term;

            m_backtrackingState.link(&m_jit);
            loadFromFrame(term->frameLocation + BackTrackInfoCharacterClass::beginIndex(), m_regs.index);
            m_backtrackingState.fallthrough();
        }
#endif
        backtrackTermDefault(opIndex);
    }

    void generateCharacterClassFixed(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const MacroAssembler::RegisterID character = m_regs.regT0;
        const MacroAssembler::RegisterID countRegister = m_regs.regT1;

        if (m_decodeSurrogatePairs)
            op.m_jumps.append(jumpIfNoAvailableInput());

        Checked<unsigned> scaledMaxCount = term->quantityMaxCount;
#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (m_decodeSurrogatePairs && term->characterClass->hasOnlyNonBMPCharacters() && !term->invert())
            scaledMaxCount *= 2;
#endif
        m_jit.sub32(m_regs.index, MacroAssembler::Imm32(scaledMaxCount), countRegister);

        MacroAssembler::Label loop(&m_jit);
        MacroAssembler::JumpList matchDest;
        readCharacter(op.m_checkedOffset - term->inputPosition - scaledMaxCount, character, countRegister);
        // If we are matching the "any character" builtin class we only need to read the
        // character and don't need to match as it will always succeed.
        if (term->invert() || !term->characterClass->m_anyCharacter) {
            matchCharacterClass(character, matchDest, term->characterClass);

            if (term->invert())
                op.m_jumps.append(matchDest);
            else {
                op.m_jumps.append(m_jit.jump());
                matchDest.link(&m_jit);
            }
        }

#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (m_decodeSurrogatePairs) {
            if (term->isFixedWidthCharacterClass())
                m_jit.add32(MacroAssembler::TrustedImm32(term->characterClass->hasNonBMPCharacters() ? 2 : 1), countRegister);
            else {
                m_jit.add32(MacroAssembler::TrustedImm32(1), countRegister);
                MacroAssembler::Jump isBMPChar = m_jit.branch32(MacroAssembler::LessThan, character, m_regs.supplementaryPlanesBase);
                op.m_jumps.append(atEndOfInput());
                m_jit.add32(MacroAssembler::TrustedImm32(1), countRegister);
                m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
                isBMPChar.link(&m_jit);
            }
        } else
#endif
            m_jit.add32(MacroAssembler::TrustedImm32(1), countRegister);
        m_jit.branch32(MacroAssembler::NotEqual, countRegister, m_regs.index).linkTo(loop, &m_jit);
    }
    void backtrackCharacterClassFixed(size_t opIndex)
    {
        backtrackTermDefault(opIndex);
    }

    void generateCharacterClassGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const MacroAssembler::RegisterID character = m_regs.regT0;
        const MacroAssembler::RegisterID countRegister = m_regs.regT1;

        if (m_decodeSurrogatePairs && (!term->characterClass->hasOneCharacterSize() || term->invert()))
            storeToFrame(m_regs.index, term->frameLocation + BackTrackInfoCharacterClass::beginIndex());
        m_jit.move(MacroAssembler::TrustedImm32(0), countRegister);

        MacroAssembler::JumpList failures;
        MacroAssembler::JumpList failuresDecrementIndex;
        MacroAssembler::Label loop(&m_jit);
#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (term->isFixedWidthCharacterClass() && term->characterClass->hasNonBMPCharacters()) {
            m_jit.move(MacroAssembler::TrustedImm32(1), character);
            failures.append(checkNotEnoughInput(character));
        } else
#endif
            failures.append(atEndOfInput());

        if (term->invert()) {
            readCharacter(op.m_checkedOffset - term->inputPosition, character);
            matchCharacterClass(character, failures, term->characterClass);
        } else {
            MacroAssembler::JumpList matchDest;
            readCharacter(op.m_checkedOffset - term->inputPosition, character);
            // If we are matching the "any character" builtin class for non-unicode patterns,
            // we only need to read the character and don't need to match as it will always succeed.
            if (!term->characterClass->m_anyCharacter) {
                matchCharacterClass(character, matchDest, term->characterClass);
                failures.append(m_jit.jump());
            }
            matchDest.link(&m_jit);
        }

#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (m_decodeSurrogatePairs)
            advanceIndexAfterCharacterClassTermMatch(term, failuresDecrementIndex, character);
        else
#endif
            m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
        m_jit.add32(MacroAssembler::TrustedImm32(1), countRegister);

        if (term->quantityMaxCount != quantifyInfinite) {
            m_jit.branch32(MacroAssembler::NotEqual, countRegister, MacroAssembler::Imm32(term->quantityMaxCount)).linkTo(loop, &m_jit);
            failures.append(m_jit.jump());
        } else
            m_jit.jump(loop);

        if (!failuresDecrementIndex.empty()) {
            failuresDecrementIndex.link(&m_jit);
            m_jit.sub32(MacroAssembler::TrustedImm32(1), m_regs.index);
        }

        failures.link(&m_jit);
        op.m_reentry = m_jit.label();

        storeToFrame(countRegister, term->frameLocation + BackTrackInfoCharacterClass::matchAmountIndex());
    }
    void backtrackCharacterClassGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const MacroAssembler::RegisterID countRegister = m_regs.regT1;

        m_backtrackingState.link(&m_jit);

        loadFromFrame(term->frameLocation + BackTrackInfoCharacterClass::matchAmountIndex(), countRegister);
        m_backtrackingState.append(m_jit.branchTest32(MacroAssembler::Zero, countRegister));
        m_jit.sub32(MacroAssembler::TrustedImm32(1), countRegister);
        storeToFrame(countRegister, term->frameLocation + BackTrackInfoCharacterClass::matchAmountIndex());

        if (!m_decodeSurrogatePairs)
            m_jit.sub32(MacroAssembler::TrustedImm32(1), m_regs.index);
        else if (term->isFixedWidthCharacterClass())
            m_jit.sub32(MacroAssembler::TrustedImm32(term->characterClass->hasNonBMPCharacters() ? 2 : 1), m_regs.index);
        else {
            // Rematch one less
            const MacroAssembler::RegisterID character = m_regs.regT0;

            loadFromFrame(term->frameLocation + BackTrackInfoCharacterClass::beginIndex(), m_regs.index);

            MacroAssembler::Label rematchLoop(&m_jit);
            MacroAssembler::Jump doneRematching = m_jit.branchTest32(MacroAssembler::Zero, countRegister);

            readCharacter(op.m_checkedOffset - term->inputPosition, character);

            m_jit.sub32(MacroAssembler::TrustedImm32(1), countRegister);
            m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);

#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
            MacroAssembler::Jump isBMPChar = m_jit.branch32(MacroAssembler::LessThan, character, m_regs.supplementaryPlanesBase);
            m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
            isBMPChar.link(&m_jit);
#endif

            m_jit.jump(rematchLoop);
            doneRematching.link(&m_jit);

            loadFromFrame(term->frameLocation + BackTrackInfoCharacterClass::matchAmountIndex(), countRegister);
        }
        m_jit.jump(op.m_reentry);
    }

    void generateCharacterClassNonGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const MacroAssembler::RegisterID countRegister = m_regs.regT1;

        m_jit.move(MacroAssembler::TrustedImm32(0), countRegister);
        op.m_reentry = m_jit.label();

#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (m_decodeSurrogatePairs) {
            if (!term->characterClass->hasOneCharacterSize() || term->invert())
                storeToFrame(m_regs.index, term->frameLocation + BackTrackInfoCharacterClass::beginIndex());
        }
#endif

        storeToFrame(countRegister, term->frameLocation + BackTrackInfoCharacterClass::matchAmountIndex());
    }

    void backtrackCharacterClassNonGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const MacroAssembler::RegisterID character = m_regs.regT0;
        const MacroAssembler::RegisterID countRegister = m_regs.regT1;

        MacroAssembler::JumpList nonGreedyFailures;
        MacroAssembler::JumpList nonGreedyFailuresDecrementIndex;

        m_backtrackingState.link(&m_jit);

#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (m_decodeSurrogatePairs) {
            if (!term->characterClass->hasOneCharacterSize() || term->invert())
                loadFromFrame(term->frameLocation + BackTrackInfoCharacterClass::beginIndex(), m_regs.index);
        }
#endif

        loadFromFrame(term->frameLocation + BackTrackInfoCharacterClass::matchAmountIndex(), countRegister);

        nonGreedyFailures.append(atEndOfInput());
        nonGreedyFailures.append(m_jit.branch32(MacroAssembler::Equal, countRegister, MacroAssembler::Imm32(term->quantityMaxCount)));

        MacroAssembler::JumpList matchDest;
        readCharacter(op.m_checkedOffset - term->inputPosition, character);
        // If we are matching the "any character" builtin class for non-unicode patterns,
        // we only need to read the character and don't need to match as it will always succeed.
        if (term->invert() || !term->characterClass->m_anyCharacter) {
            matchCharacterClass(character, matchDest, term->characterClass);

            if (term->invert())
                nonGreedyFailures.append(matchDest);
            else {
                nonGreedyFailures.append(m_jit.jump());
                matchDest.link(&m_jit);
            }
        }

#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (m_decodeSurrogatePairs)
            advanceIndexAfterCharacterClassTermMatch(term, nonGreedyFailuresDecrementIndex, character);
        else
#endif
            m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
        m_jit.add32(MacroAssembler::TrustedImm32(1), countRegister);

        m_jit.jump(op.m_reentry);

        if (!nonGreedyFailuresDecrementIndex.empty()) {
            nonGreedyFailuresDecrementIndex.link(&m_jit);
            m_jit.sub32(MacroAssembler::TrustedImm32(1), m_regs.index);
        }
        nonGreedyFailures.link(&m_jit);
        m_jit.sub32(countRegister, m_regs.index);
        m_backtrackingState.fallthrough();
    }

    void generateDotStarEnclosure(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const MacroAssembler::RegisterID character = m_regs.regT0;
        const MacroAssembler::RegisterID matchPos = m_regs.regT1;
        MacroAssembler::JumpList foundBeginningNewLine;
        MacroAssembler::JumpList saveStartIndex;
        MacroAssembler::JumpList foundEndingNewLine;

        if (m_pattern.dotAll()) {
            m_jit.move(MacroAssembler::TrustedImm32(0), matchPos);
            setMatchStart(matchPos);
            m_jit.move(m_regs.length, m_regs.index);
            return;
        }

        ASSERT(!m_pattern.m_body->m_hasFixedSize);
        getMatchStart(matchPos);

        saveStartIndex.append(m_jit.branch32(MacroAssembler::BelowOrEqual, matchPos, m_regs.initialStart));
        MacroAssembler::Label findBOLLoop(&m_jit);
        m_jit.sub32(MacroAssembler::TrustedImm32(1), matchPos);
        if (m_charSize == CharSize::Char8)
            m_jit.load8(MacroAssembler::BaseIndex(m_regs.input, matchPos, MacroAssembler::TimesOne, 0), character);
        else
            m_jit.load16(MacroAssembler::BaseIndex(m_regs.input, matchPos, MacroAssembler::TimesTwo, 0), character);
        matchCharacterClass(character, foundBeginningNewLine, m_pattern.newlineCharacterClass());

        m_jit.branch32(MacroAssembler::Above, matchPos, m_regs.initialStart).linkTo(findBOLLoop, &m_jit);
        saveStartIndex.append(m_jit.jump());

        foundBeginningNewLine.link(&m_jit);
        m_jit.add32(MacroAssembler::TrustedImm32(1), matchPos); // Advance past newline
        saveStartIndex.link(&m_jit);

        if (!m_pattern.multiline() && term->anchors.bolAnchor)
            op.m_jumps.append(m_jit.branchTest32(MacroAssembler::NonZero, matchPos));

        ASSERT(!m_pattern.m_body->m_hasFixedSize);
        setMatchStart(matchPos);

        m_jit.move(m_regs.index, matchPos);

        MacroAssembler::Label findEOLLoop(&m_jit);
        foundEndingNewLine.append(m_jit.branch32(MacroAssembler::Equal, matchPos, m_regs.length));
        if (m_charSize == CharSize::Char8)
            m_jit.load8(MacroAssembler::BaseIndex(m_regs.input, matchPos, MacroAssembler::TimesOne, 0), character);
        else
            m_jit.load16(MacroAssembler::BaseIndex(m_regs.input, matchPos, MacroAssembler::TimesTwo, 0), character);
        matchCharacterClass(character, foundEndingNewLine, m_pattern.newlineCharacterClass());
        m_jit.add32(MacroAssembler::TrustedImm32(1), matchPos);
        m_jit.jump(findEOLLoop);

        foundEndingNewLine.link(&m_jit);

        if (!m_pattern.multiline() && term->anchors.eolAnchor)
            op.m_jumps.append(m_jit.branch32(MacroAssembler::NotEqual, matchPos, m_regs.length));

        m_jit.move(matchPos, m_regs.index);
    }

    void backtrackDotStarEnclosure(size_t opIndex)
    {
        backtrackTermDefault(opIndex);
    }
    
    // Code generation/backtracking for simple terms
    // (pattern characters, character classes, and assertions).
    // These methods farm out work to the set of functions above.
    void generateTerm(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        switch (term->type) {
        case PatternTerm::Type::PatternCharacter:
            switch (term->quantityType) {
            case QuantifierType::FixedCount:
                if (term->quantityMaxCount == 1)
                    generatePatternCharacterOnce(opIndex);
                else
                    generatePatternCharacterFixed(opIndex);
                break;
            case QuantifierType::Greedy:
                generatePatternCharacterGreedy(opIndex);
                break;
            case QuantifierType::NonGreedy:
                generatePatternCharacterNonGreedy(opIndex);
                break;
            }
            break;

        case PatternTerm::Type::CharacterClass:
            switch (term->quantityType) {
            case QuantifierType::FixedCount:
                if (term->quantityMaxCount == 1)
                    generateCharacterClassOnce(opIndex);
                else
                    generateCharacterClassFixed(opIndex);
                break;
            case QuantifierType::Greedy:
                generateCharacterClassGreedy(opIndex);
                break;
            case QuantifierType::NonGreedy:
                generateCharacterClassNonGreedy(opIndex);
                break;
            }
            break;

        case PatternTerm::Type::AssertionBOL:
            generateAssertionBOL(opIndex);
            break;

        case PatternTerm::Type::AssertionEOL:
            generateAssertionEOL(opIndex);
            break;

        case PatternTerm::Type::AssertionWordBoundary:
            generateAssertionWordBoundary(opIndex);
            break;

        case PatternTerm::Type::ForwardReference:
            m_failureReason = JITFailureReason::ForwardReference;
            break;

        case PatternTerm::Type::ParenthesesSubpattern:
        case PatternTerm::Type::ParentheticalAssertion:
            RELEASE_ASSERT_NOT_REACHED();

        case PatternTerm::Type::BackReference:
#if ENABLE(YARR_JIT_BACKREFERENCES)
            generateBackReference(opIndex);
#else
            m_failureReason = JITFailureReason::BackReference;
#endif
            break;
        case PatternTerm::Type::DotStarEnclosure:
            generateDotStarEnclosure(opIndex);
            break;
        }
    }
    void backtrackTerm(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        switch (term->type) {
        case PatternTerm::Type::PatternCharacter:
            switch (term->quantityType) {
            case QuantifierType::FixedCount:
                if (term->quantityMaxCount == 1)
                    backtrackPatternCharacterOnce(opIndex);
                else
                    backtrackPatternCharacterFixed(opIndex);
                break;
            case QuantifierType::Greedy:
                backtrackPatternCharacterGreedy(opIndex);
                break;
            case QuantifierType::NonGreedy:
                backtrackPatternCharacterNonGreedy(opIndex);
                break;
            }
            break;

        case PatternTerm::Type::CharacterClass:
            switch (term->quantityType) {
            case QuantifierType::FixedCount:
                if (term->quantityMaxCount == 1)
                    backtrackCharacterClassOnce(opIndex);
                else
                    backtrackCharacterClassFixed(opIndex);
                break;
            case QuantifierType::Greedy:
                backtrackCharacterClassGreedy(opIndex);
                break;
            case QuantifierType::NonGreedy:
                backtrackCharacterClassNonGreedy(opIndex);
                break;
            }
            break;

        case PatternTerm::Type::AssertionBOL:
            backtrackAssertionBOL(opIndex);
            break;

        case PatternTerm::Type::AssertionEOL:
            backtrackAssertionEOL(opIndex);
            break;

        case PatternTerm::Type::AssertionWordBoundary:
            backtrackAssertionWordBoundary(opIndex);
            break;

        case PatternTerm::Type::ForwardReference:
            m_failureReason = JITFailureReason::ForwardReference;
            break;

        case PatternTerm::Type::ParenthesesSubpattern:
        case PatternTerm::Type::ParentheticalAssertion:
            RELEASE_ASSERT_NOT_REACHED();

        case PatternTerm::Type::BackReference:
#if ENABLE(YARR_JIT_BACKREFERENCES)
            backtrackBackReference(opIndex);
#else
            m_failureReason = JITFailureReason::BackReference;
#endif
            break;

        case PatternTerm::Type::DotStarEnclosure:
            backtrackDotStarEnclosure(opIndex);
            break;
        }
    }

    void generate()
    {
        // Forwards generate the matching code.
        ASSERT(m_ops.size());
        size_t opIndex = 0;

        do {
            if (m_disassembler)
                m_disassembler->setForGenerate(opIndex, m_jit.label());

            YarrOp& op = m_ops[opIndex];
            switch (op.m_op) {

            case YarrOpCode::Term:
                generateTerm(opIndex);
                break;

            // YarrOpCode::BodyAlternativeBegin/Next/End
            //
            // These nodes wrap the set of alternatives in the body of the regular expression.
            // There may be either one or two chains of OpBodyAlternative nodes, one representing
            // the 'once through' sequence of alternatives (if any exist), and one representing
            // the repeating alternatives (again, if any exist).
            //
            // Upon normal entry to the Begin alternative, we will check that input is available.
            // Reentry to the Begin alternative will take place after the check has taken place,
            // and will assume that the input position has already been progressed as appropriate.
            //
            // Entry to subsequent Next/End alternatives occurs when the prior alternative has
            // successfully completed a match - return a success state from JIT code.
            //
            // Next alternatives allow for reentry optimized to suit backtracking from its
            // preceding alternative. It expects the input position to still be set to a position
            // appropriate to its predecessor, and it will only perform an input check if the
            // predecessor had a minimum size less than its own.
            //
            // In the case 'once through' expressions, the End node will also have a reentry
            // point to jump to when the last alternative fails. Again, this expects the input
            // position to still reflect that expected by the prior alternative.
            case YarrOpCode::BodyAlternativeBegin: {
                PatternAlternative* alternative = op.m_alternative;

                // Upon entry at the head of the set of alternatives, check if input is available
                // to run the first alternative. (This progresses the input position).
                op.m_jumps.append(jumpIfNoAvailableInput(alternative->m_minimumSize));

                // We will reenter after the check, and assume the input position to have been
                // set as appropriate to this alternative.
                op.m_reentry = m_jit.label();

                // Emit fast skip path with stride if we have BoyerMooreInfo.
                if (op.m_bmInfo) {
                    auto range = op.m_bmInfo->findWorthwhileCharacterSequenceForLookahead();
                    if (range) {
                        auto [beginIndex, endIndex] = *range;
                        ASSERT(endIndex <= alternative->m_minimumSize);

                        auto [map, charactersFastPath] = op.m_bmInfo->createCandidateBitmap(beginIndex, endIndex);
                        unsigned mapCount = map.count();
                        // If candiate characters are <= 2, checking each is better than using vector.
                        MacroAssembler::JumpList matched;
                        dataLogLnIf(YarrJITInternal::verbose, "BM Bitmap is ", map);
                        // Patterns like /[]/ have zero candidates. Since it is rare, we do not do nothing for now.
                        if (!mapCount)
                            break;
                        if (charactersFastPath.isValid() && !charactersFastPath.isEmpty()) {
                            static_assert(BoyerMooreFastCandidates::maxSize == 2);
                            dataLogLnIf(Options::verboseRegExpCompilation(), "Found characters fastpath lookahead ", charactersFastPath, " range:[", beginIndex, ", ", endIndex, ")");

                            auto loopHead = m_jit.label();
                            readCharacter(op.m_checkedOffset - endIndex + 1, m_regs.regT0);
                            matched.append(m_jit.branch32(MacroAssembler::Equal, m_regs.regT0, MacroAssembler::TrustedImm32(charactersFastPath.at(0))));
                            if (charactersFastPath.size() > 1)
                                matched.append(m_jit.branch32(MacroAssembler::Equal, m_regs.regT0, MacroAssembler::TrustedImm32(charactersFastPath.at(1))));
                            jumpIfAvailableInput(endIndex - beginIndex).linkTo(loopHead, &m_jit);
                        } else {
                            const auto* pointer = getBoyerMooreBitmap(map);
                            dataLogLnIf(Options::verboseRegExpCompilation(), "Found bitmap lookahead count:(", mapCount, "),range:[", beginIndex, ", ", endIndex, ")");

                            m_jit.move(MacroAssembler::TrustedImmPtr(pointer), m_regs.regT1);
                            auto loopHead = m_jit.label();
                            readCharacter(op.m_checkedOffset - endIndex + 1, m_regs.regT0);
#if CPU(ARM64) || CPU(RISCV64)
                            static_assert(sizeof(BoyerMooreBitmap::Map::WordType) == sizeof(uint64_t));
                            static_assert(1 << 6 == 64);
                            static_assert(1 << (6 + 1) == BoyerMooreBitmap::Map::size());
                            m_jit.extractUnsignedBitfield32(m_regs.regT0, MacroAssembler::TrustedImm32(6), MacroAssembler::TrustedImm32(1), m_regs.regT2); // Extract 1 bit for index.
                            m_jit.load64(MacroAssembler::BaseIndex(m_regs.regT1, m_regs.regT2, MacroAssembler::TimesEight), m_regs.regT2);
                            m_jit.urshift64(m_regs.regT0, m_regs.regT2); // We can ignore upper bits and only lower 6bits are effective.
                            matched.append(m_jit.branchTest64(MacroAssembler::NonZero, m_regs.regT2, MacroAssembler::TrustedImm32(1)));
#elif CPU(X86_64)
                            static_assert(sizeof(BoyerMooreBitmap::Map::WordType) == sizeof(uint64_t));
                            static_assert(1 << 6 == 64);
                            static_assert(1 << (6 + 1) == BoyerMooreBitmap::Map::size());
                            m_jit.move(m_regs.regT0, m_regs.regT2);
                            m_jit.urshift32(MacroAssembler::TrustedImm32(6), m_regs.regT2);
                            m_jit.and32(MacroAssembler::TrustedImm32(1), m_regs.regT2);
                            m_jit.load64(MacroAssembler::BaseIndex(m_regs.regT1, m_regs.regT2, MacroAssembler::TimesEight), m_regs.regT2);
                            matched.append(m_jit.branchTestBit64(MacroAssembler::NonZero, m_regs.regT2, m_regs.regT0)); // We can ignore upper bits since modulo-64 is performed.
#else
                            static_assert(sizeof(BoyerMooreBitmap::Map::WordType) == sizeof(uint32_t));
                            static_assert(1 << 5 == 32);
                            static_assert(1 << (5 + 2) == BoyerMooreBitmap::Map::size());
                            m_jit.move(m_regs.regT0, m_regs.regT2);
                            m_jit.urshift32(MacroAssembler::TrustedImm32(5), m_regs.regT2);
                            m_jit.and32(MacroAssembler::TrustedImm32(0b11), m_regs.regT2);
                            m_jit.load32(MacroAssembler::BaseIndex(m_regs.regT1, m_regs.regT2, MacroAssembler::TimesFour), m_regs.regT2);
                            m_jit.urshift32(m_regs.regT0, m_regs.regT2); // We can ignore upper bits and only lower 5bits are effective.
                            matched.append(m_jit.branchTest32(MacroAssembler::NonZero, m_regs.regT2, MacroAssembler::TrustedImm32(1)));
#endif
                            jumpIfAvailableInput(endIndex - beginIndex).linkTo(loopHead, &m_jit);
                        }
                        // Fallthrough if out-of-length failure happens.

                        // If the pattern size is not fixed, then store the start index for use if we match.
                        // This is used for adjusting match-start when we failed to find the start with BoyerMoore search.
                        if (!m_pattern.m_body->m_hasFixedSize) {
                            if (alternative->m_minimumSize) {
                                m_jit.sub32(m_regs.index, MacroAssembler::Imm32(alternative->m_minimumSize), m_regs.regT0);
                                setMatchStart(m_regs.regT0);
                            } else
                                setMatchStart(m_regs.index);
                            op.m_jumps.append(m_jit.jump());
                        } else
                            op.m_jumps.append(m_jit.jump());

                        matched.link(&m_jit);
                        // If the pattern size is not fixed, then store the start index for use if we match.
                        // This is used for adjusting match-start when we start pattern matching with the updated index
                        // by BoyerMoore search.
                        if (!m_pattern.m_body->m_hasFixedSize) {
                            if (alternative->m_minimumSize) {
                                m_jit.sub32(m_regs.index, MacroAssembler::Imm32(alternative->m_minimumSize), m_regs.regT0);
                                setMatchStart(m_regs.regT0);
                            } else
                                setMatchStart(m_regs.index);
                        }
                    }
                }
                break;
            }
            case YarrOpCode::BodyAlternativeNext:
            case YarrOpCode::BodyAlternativeEnd: {
                PatternAlternative* priorAlternative = m_ops[op.m_previousOp].m_alternative;
                PatternAlternative* alternative = op.m_alternative;

                // If we get here, the prior alternative matched - return success.
                
                // Adjust the stack pointer to remove the pattern's frame.
                removeCallFrame();

                // Load appropriate values into the return register and the first output
                // slot, and return. In the case of pattern with a fixed size, we will
                // not have yet set the value in the first 
                ASSERT(m_regs.index != m_regs.returnRegister);
                ASSERT(m_regs.output != m_regs.returnRegister);
                if (m_pattern.m_body->m_hasFixedSize) {
                    if (priorAlternative->m_minimumSize)
                        m_jit.sub32(m_regs.index, MacroAssembler::Imm32(priorAlternative->m_minimumSize), m_regs.returnRegister);
                    else
                        m_jit.move(m_regs.index, m_regs.returnRegister);
                    if (m_compileMode == JITCompileMode::IncludeSubpatterns)
                        m_jit.storePair32(m_regs.returnRegister, m_regs.index, m_regs.output, MacroAssembler::TrustedImm32(0));
                } else {
                    getMatchStart(m_regs.returnRegister);
                    if (m_compileMode == JITCompileMode::IncludeSubpatterns)
                        m_jit.store32(m_regs.index, MacroAssembler::Address(m_regs.output, 4));
                }
                m_jit.move(m_regs.index, m_regs.returnRegister2);
                generateReturn();

                // This is the divide between the tail of the prior alternative, above, and
                // the head of the subsequent alternative, below.

                if (op.m_op == YarrOpCode::BodyAlternativeNext) {
                    // This is the reentry point for the Next alternative. We expect any code
                    // that jumps here to do so with the input position matching that of the
                    // PRIOR alteranative, and we will only check input availability if we
                    // need to progress it forwards.
                    op.m_reentry = m_jit.label();
                    if (alternative->m_minimumSize > priorAlternative->m_minimumSize) {
                        m_jit.add32(MacroAssembler::Imm32(alternative->m_minimumSize - priorAlternative->m_minimumSize), m_regs.index);
                        op.m_jumps.append(jumpIfNoAvailableInput());
                    } else if (priorAlternative->m_minimumSize > alternative->m_minimumSize)
                        m_jit.sub32(MacroAssembler::Imm32(priorAlternative->m_minimumSize - alternative->m_minimumSize), m_regs.index);
                } else if (op.m_nextOp == notFound) {
                    // This is the reentry point for the End of 'once through' alternatives,
                    // jumped to when the last alternative fails to match.
                    op.m_reentry = m_jit.label();
                    m_jit.sub32(MacroAssembler::Imm32(priorAlternative->m_minimumSize), m_regs.index);
                }
                break;
            }

            // YarrOpCode::SimpleNestedAlternativeBegin/Next/End
            // YarrOpCode::NestedAlternativeBegin/Next/End
            //
            // These nodes are used to handle sets of alternatives that are nested within
            // subpatterns and parenthetical assertions. The 'simple' forms are used where
            // we do not need to be able to backtrack back into any alternative other than
            // the last, the normal forms allow backtracking into any alternative.
            //
            // Each Begin/Next node is responsible for planting an input check to ensure
            // sufficient input is available on entry. Next nodes additionally need to
            // jump to the end - Next nodes use the End node's m_jumps list to hold this
            // set of jumps.
            //
            // In the non-simple forms, successful alternative matches must store a
            // 'return address' using a DataLabelPtr, used to store the address to jump
            // to when backtracking, to get to the code for the appropriate alternative.
            case YarrOpCode::SimpleNestedAlternativeBegin:
            case YarrOpCode::NestedAlternativeBegin: {
                PatternTerm* term = op.m_term;
                PatternAlternative* alternative = op.m_alternative;
                PatternDisjunction* disjunction = term->parentheses.disjunction;

                // Calculate how much input we need to check for, and if non-zero check.
                op.m_checkAdjust = Checked<unsigned>(alternative->m_minimumSize);
                if ((term->quantityType == QuantifierType::FixedCount) && (term->type != PatternTerm::Type::ParentheticalAssertion))
                    op.m_checkAdjust -= disjunction->m_minimumSize;
                if (op.m_checkAdjust)
                    op.m_jumps.append(jumpIfNoAvailableInput(op.m_checkAdjust));
                break;
            }
            case YarrOpCode::SimpleNestedAlternativeNext:
            case YarrOpCode::NestedAlternativeNext: {
                PatternTerm* term = op.m_term;
                PatternAlternative* alternative = op.m_alternative;
                PatternDisjunction* disjunction = term->parentheses.disjunction;

                // In the non-simple case, store a 'return address' so we can backtrack correctly.
                if (op.m_op == YarrOpCode::NestedAlternativeNext) {
                    unsigned parenthesesFrameLocation = term->frameLocation;
                    op.m_returnAddress = storeToFrameWithPatch(parenthesesFrameLocation + BackTrackInfoParentheses::returnAddressIndex());
                }

                if (term->quantityType != QuantifierType::FixedCount && !m_ops[op.m_previousOp].m_alternative->m_minimumSize) {
                    // If the previous alternative matched without consuming characters then
                    // backtrack to try to match while consumming some input.
                    op.m_zeroLengthMatch = m_jit.branch32(MacroAssembler::Equal, m_regs.index, MacroAssembler::Address(MacroAssembler::stackPointerRegister, term->frameLocation * sizeof(void*)));
                }

                // If we reach here then the last alternative has matched - jump to the
                // End node, to skip over any further alternatives.
                //
                // FIXME: this is logically O(N^2) (though N can be expected to be very
                // small). We could avoid this either by adding an extra jump to the JIT
                // data structures, or by making backtracking code that jumps to Next
                // alternatives are responsible for checking that input is available (if
                // we didn't need to plant the input checks, then m_jumps would be free).
                YarrOp* endOp = &m_ops[op.m_nextOp];
                while (endOp->m_nextOp != notFound) {
                    ASSERT(endOp->m_op == YarrOpCode::SimpleNestedAlternativeNext || endOp->m_op == YarrOpCode::NestedAlternativeNext);
                    endOp = &m_ops[endOp->m_nextOp];
                }
                ASSERT(endOp->m_op == YarrOpCode::SimpleNestedAlternativeEnd || endOp->m_op == YarrOpCode::NestedAlternativeEnd);
                endOp->m_jumps.append(m_jit.jump());

                // This is the entry point for the next alternative.
                op.m_reentry = m_jit.label();

                // Calculate how much input we need to check for, and if non-zero check.
                op.m_checkAdjust = alternative->m_minimumSize;
                if ((term->quantityType == QuantifierType::FixedCount) && (term->type != PatternTerm::Type::ParentheticalAssertion))
                    op.m_checkAdjust -= disjunction->m_minimumSize;
                if (op.m_checkAdjust)
                    op.m_jumps.append(jumpIfNoAvailableInput(op.m_checkAdjust));
                break;
            }
            case YarrOpCode::SimpleNestedAlternativeEnd:
            case YarrOpCode::NestedAlternativeEnd: {
                PatternTerm* term = op.m_term;

                // In the non-simple case, store a 'return address' so we can backtrack correctly.
                if (op.m_op == YarrOpCode::NestedAlternativeEnd) {
                    unsigned parenthesesFrameLocation = term->frameLocation;
                    op.m_returnAddress = storeToFrameWithPatch(parenthesesFrameLocation + BackTrackInfoParentheses::returnAddressIndex());
                }

                if (term->quantityType != QuantifierType::FixedCount && !m_ops[op.m_previousOp].m_alternative->m_minimumSize) {
                    // If the previous alternative matched without consuming characters then
                    // backtrack to try to match while consumming some input.
                    op.m_zeroLengthMatch = m_jit.branch32(MacroAssembler::Equal, m_regs.index, MacroAssembler::Address(MacroAssembler::stackPointerRegister, term->frameLocation * sizeof(void*)));
                }

                // If this set of alternatives contains more than one alternative,
                // then the Next nodes will have planted jumps to the End, and added
                // them to this node's m_jumps list.
                op.m_jumps.link(&m_jit);
                op.m_jumps.clear();
                break;
            }

            // YarrOpCode::ParenthesesSubpatternOnceBegin/End
            //
            // These nodes support (optionally) capturing subpatterns, that have a
            // quantity count of 1 (this covers fixed once, and ?/?? quantifiers). 
            case YarrOpCode::ParenthesesSubpatternOnceBegin: {
                PatternTerm* term = op.m_term;
                unsigned parenthesesFrameLocation = term->frameLocation;
                const MacroAssembler::RegisterID indexTemporary = m_regs.regT0;
                ASSERT(term->quantityMaxCount == 1);

                // Upon entry to a Greedy quantified set of parenthese store the index.
                // We'll use this for two purposes:
                //  - To indicate which iteration we are on of mathing the remainder of
                //    the expression after the parentheses - the first, including the
                //    match within the parentheses, or the second having skipped over them.
                //  - To check for empty matches, which must be rejected.
                //
                // At the head of a NonGreedy set of parentheses we'll immediately set the
                // value on the stack to -1 (indicating a match skipping the subpattern),
                // and plant a jump to the end. We'll also plant a label to backtrack to
                // to reenter the subpattern later, with a store to set up index on the
                // second iteration.
                //
                // FIXME: for capturing parens, could use the index in the capture array?
                if (term->quantityType == QuantifierType::Greedy)
                    storeToFrame(m_regs.index, parenthesesFrameLocation + BackTrackInfoParenthesesOnce::beginIndex());
                else if (term->quantityType == QuantifierType::NonGreedy) {
                    storeToFrame(MacroAssembler::TrustedImm32(-1), parenthesesFrameLocation + BackTrackInfoParenthesesOnce::beginIndex());
                    op.m_jumps.append(m_jit.jump());
                    op.m_reentry = m_jit.label();
                    storeToFrame(m_regs.index, parenthesesFrameLocation + BackTrackInfoParenthesesOnce::beginIndex());
                }

                // If the parenthese are capturing, store the starting index value to the
                // captures array, offsetting as necessary.
                //
                // FIXME: could avoid offsetting this value in JIT code, apply
                // offsets only afterwards, at the point the results array is
                // being accessed.
                if (term->capture() && m_compileMode == JITCompileMode::IncludeSubpatterns) {
                    unsigned inputOffset = op.m_checkedOffset - term->inputPosition;
                    if (term->quantityType == QuantifierType::FixedCount)
                        inputOffset += term->parentheses.disjunction->m_minimumSize;
                    if (inputOffset) {
                        m_jit.sub32(m_regs.index, MacroAssembler::Imm32(inputOffset), indexTemporary);
                        setSubpatternStart(indexTemporary, term->parentheses.subpatternId);
                    } else
                        setSubpatternStart(m_regs.index, term->parentheses.subpatternId);
                }
                break;
            }
            case YarrOpCode::ParenthesesSubpatternOnceEnd: {
                PatternTerm* term = op.m_term;
                const MacroAssembler::RegisterID indexTemporary = m_regs.regT0;
                ASSERT(term->quantityMaxCount == 1);

                // If the nested alternative matched without consuming any characters, punt this back to the interpreter.
                // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=200786> Add ability for the YARR JIT to properly
                // handle nested expressions that can match without consuming characters
                if (term->quantityType != QuantifierType::FixedCount && !term->parentheses.disjunction->m_minimumSize)
                    m_abortExecution.append(m_jit.branch32(MacroAssembler::Equal, m_regs.index, MacroAssembler::Address(MacroAssembler::stackPointerRegister, term->frameLocation * sizeof(void*))));

                // If the parenthese are capturing, store the ending index value to the
                // captures array, offsetting as necessary.
                //
                // FIXME: could avoid offsetting this value in JIT code, apply
                // offsets only afterwards, at the point the results array is
                // being accessed.
                if (term->capture() && m_compileMode == JITCompileMode::IncludeSubpatterns) {
                    unsigned inputOffset = op.m_checkedOffset - term->inputPosition;
                    if (inputOffset) {
                        m_jit.sub32(m_regs.index, MacroAssembler::Imm32(inputOffset), indexTemporary);
                        setSubpatternEnd(indexTemporary, term->parentheses.subpatternId);
                    } else
                        setSubpatternEnd(m_regs.index, term->parentheses.subpatternId);
                }

                // If the parentheses are quantified Greedy then add a label to jump back
                // to if we get a failed match from after the parentheses. For NonGreedy
                // parentheses, link the jump from before the subpattern to here.
                if (term->quantityType == QuantifierType::Greedy)
                    op.m_reentry = m_jit.label();
                else if (term->quantityType == QuantifierType::NonGreedy) {
                    YarrOp& beginOp = m_ops[op.m_previousOp];
                    beginOp.m_jumps.link(&m_jit);
                }
                break;
            }

            // YarrOpCode::ParenthesesSubpatternTerminalBegin/End
            case YarrOpCode::ParenthesesSubpatternTerminalBegin: {
                PatternTerm* term = op.m_term;
                ASSERT(term->quantityType == QuantifierType::Greedy);
                ASSERT(term->quantityMaxCount == quantifyInfinite);
                ASSERT(!term->capture());

                // Upon entry set a label to loop back to.
                op.m_reentry = m_jit.label();

                // Store the start index of the current match; we need to reject zero
                // length matches.
                storeToFrame(m_regs.index, term->frameLocation + BackTrackInfoParenthesesTerminal::beginIndex());
                break;
            }
            case YarrOpCode::ParenthesesSubpatternTerminalEnd: {
                YarrOp& beginOp = m_ops[op.m_previousOp];
                PatternTerm* term = op.m_term;

                // If the nested alternative matched without consuming any characters, punt this back to the interpreter.
                // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=200786> Add ability for the YARR JIT to properly
                // handle nested expressions that can match without consuming characters
                if (term->quantityType != QuantifierType::FixedCount && !term->parentheses.disjunction->m_minimumSize)
                    m_abortExecution.append(m_jit.branch32(MacroAssembler::Equal, m_regs.index, MacroAssembler::Address(MacroAssembler::stackPointerRegister, term->frameLocation * sizeof(void*))));

                // We know that the match is non-zero, we can accept it and
                // loop back up to the head of the subpattern.
                m_jit.jump(beginOp.m_reentry);

                // This is the entry point to jump to when we stop matching - we will
                // do so once the subpattern cannot match any more.
                op.m_reentry = m_jit.label();
                break;
            }

            // YarrOpCode::ParenthesesSubpatternBegin/End
            //
            // These nodes support generic subpatterns.
            case YarrOpCode::ParenthesesSubpatternBegin: {
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
                PatternTerm* term = op.m_term;
                unsigned parenthesesFrameLocation = term->frameLocation;

                // Upon entry to a Greedy quantified set of parenthese store the index.
                // We'll use this for two purposes:
                //  - To indicate which iteration we are on of mathing the remainder of
                //    the expression after the parentheses - the first, including the
                //    match within the parentheses, or the second having skipped over them.
                //  - To check for empty matches, which must be rejected.
                //
                // At the head of a NonGreedy set of parentheses we'll immediately set 'begin'
                // in the backtrack info to -1 (indicating a match skipping the subpattern),
                // and plant a jump to the end. We'll also plant a label to backtrack to
                // to reenter the subpattern later, with a store to set 'begin' to current index
                // on the second iteration.
                //
                // FIXME: for capturing parens, could use the index in the capture array?
                if (term->quantityType == QuantifierType::Greedy || term->quantityType == QuantifierType::NonGreedy) {
                    storeToFrame(MacroAssembler::TrustedImm32(0), parenthesesFrameLocation + BackTrackInfoParentheses::matchAmountIndex());
                    storeToFrame(MacroAssembler::TrustedImmPtr(nullptr), parenthesesFrameLocation + BackTrackInfoParentheses::parenContextHeadIndex());

                    if (term->quantityType == QuantifierType::NonGreedy) {
                        storeToFrame(MacroAssembler::TrustedImm32(-1), parenthesesFrameLocation + BackTrackInfoParentheses::beginIndex());
                        op.m_jumps.append(m_jit.jump());
                    }
                    
                    op.m_reentry = m_jit.label();
                    MacroAssembler::RegisterID currParenContextReg = m_regs.regT0;
                    MacroAssembler::RegisterID newParenContextReg = m_regs.regT1;

                    loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheses::parenContextHeadIndex(), currParenContextReg);
                    allocateParenContext(newParenContextReg);
                    m_jit.storePtr(currParenContextReg, MacroAssembler::Address(newParenContextReg));
                    storeToFrame(newParenContextReg, parenthesesFrameLocation + BackTrackInfoParentheses::parenContextHeadIndex());
                    saveParenContext(newParenContextReg, m_regs.regT2, term->parentheses.subpatternId, term->parentheses.lastSubpatternId, parenthesesFrameLocation);
                    storeToFrame(m_regs.index, parenthesesFrameLocation + BackTrackInfoParentheses::beginIndex());
                }

                // If the parenthese are capturing, store the starting index value to the
                // captures array, offsetting as necessary.
                //
                // FIXME: could avoid offsetting this value in JIT code, apply
                // offsets only afterwards, at the point the results array is
                // being accessed.
                if (term->capture() && m_compileMode == JITCompileMode::IncludeSubpatterns) {
                    const MacroAssembler::RegisterID indexTemporary = m_regs.regT0;
                    unsigned inputOffset = op.m_checkedOffset - term->inputPosition;
                    if (term->quantityType == QuantifierType::FixedCount)
                        inputOffset += term->parentheses.disjunction->m_minimumSize;
                    if (inputOffset) {
                        m_jit.sub32(m_regs.index, MacroAssembler::Imm32(inputOffset), indexTemporary);
                        setSubpatternStart(indexTemporary, term->parentheses.subpatternId);
                    } else
                        setSubpatternStart(m_regs.index, term->parentheses.subpatternId);
                }
#else // !YARR_JIT_ALL_PARENS_EXPRESSIONS
                RELEASE_ASSERT_NOT_REACHED();
#endif
                break;
            }
            case YarrOpCode::ParenthesesSubpatternEnd: {
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
                PatternTerm* term = op.m_term;
                unsigned parenthesesFrameLocation = term->frameLocation;

                // If the nested alternative matched without consuming any characters, punt this back to the interpreter.
                // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=200786> Add ability for the YARR JIT to properly
                // handle nested expressions that can match without consuming characters
                if (term->quantityType != QuantifierType::FixedCount && !term->parentheses.disjunction->m_minimumSize)
                    m_abortExecution.append(m_jit.branch32(MacroAssembler::Equal, m_regs.index, MacroAssembler::Address(MacroAssembler::stackPointerRegister, parenthesesFrameLocation * sizeof(void*))));

                const MacroAssembler::RegisterID countTemporary = m_regs.regT1;

                YarrOp& beginOp = m_ops[op.m_previousOp];
                loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheses::matchAmountIndex(), countTemporary);
                m_jit.add32(MacroAssembler::TrustedImm32(1), countTemporary);
                storeToFrame(countTemporary, parenthesesFrameLocation + BackTrackInfoParentheses::matchAmountIndex());

                // If the parenthese are capturing, store the ending index value to the
                // captures array, offsetting as necessary.
                //
                // FIXME: could avoid offsetting this value in JIT code, apply
                // offsets only afterwards, at the point the results array is
                // being accessed.
                if (term->capture() && m_compileMode == JITCompileMode::IncludeSubpatterns) {
                    const MacroAssembler::RegisterID indexTemporary = m_regs.regT0;
                    
                    unsigned inputOffset = op.m_checkedOffset - term->inputPosition;
                    if (inputOffset) {
                        m_jit.sub32(m_regs.index, MacroAssembler::Imm32(inputOffset), indexTemporary);
                        setSubpatternEnd(indexTemporary, term->parentheses.subpatternId);
                    } else
                        setSubpatternEnd(m_regs.index, term->parentheses.subpatternId);
                }

                // If the parentheses are quantified Greedy then add a label to jump back
                // to if we get a failed match from after the parentheses. For NonGreedy
                // parentheses, link the jump from before the subpattern to here.
                if (term->quantityType == QuantifierType::Greedy) {
                    if (term->quantityMaxCount != quantifyInfinite)
                        m_jit.branch32(MacroAssembler::Below, countTemporary, MacroAssembler::Imm32(term->quantityMaxCount)).linkTo(beginOp.m_reentry, &m_jit);
                    else
                        m_jit.jump(beginOp.m_reentry);
                    
                    op.m_reentry = m_jit.label();
                } else if (term->quantityType == QuantifierType::NonGreedy) {
                    YarrOp& beginOp = m_ops[op.m_previousOp];
                    beginOp.m_jumps.link(&m_jit);
                    op.m_reentry = m_jit.label();
                }
#else // !YARR_JIT_ALL_PARENS_EXPRESSIONS
                RELEASE_ASSERT_NOT_REACHED();
#endif
                break;
            }

            // YarrOpCode::ParentheticalAssertionBegin/End
            case YarrOpCode::ParentheticalAssertionBegin: {
                PatternTerm* term = op.m_term;

                // Store the current index - assertions should not update index, so
                // we will need to restore it upon a successful match.
                unsigned parenthesesFrameLocation = term->frameLocation;
                storeToFrame(m_regs.index, parenthesesFrameLocation + BackTrackInfoParentheticalAssertion::beginIndex());

                if (op.m_checkAdjust)
                    m_jit.sub32(MacroAssembler::Imm32(op.m_checkAdjust), m_regs.index);
                break;
            }
            case YarrOpCode::ParentheticalAssertionEnd: {
                PatternTerm* term = op.m_term;

                // Restore the input index value.
                unsigned parenthesesFrameLocation = term->frameLocation;
                loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheticalAssertion::beginIndex(), m_regs.index);

                // If inverted, a successful match of the assertion must be treated
                // as a failure, so jump to backtracking.
                if (term->invert()) {
                    op.m_jumps.append(m_jit.jump());
                    op.m_reentry = m_jit.label();
                }
                break;
            }

            case YarrOpCode::MatchFailed:
                removeCallFrame();
                generateFailReturn();
                break;
            }

            ++opIndex;
        } while (opIndex < m_ops.size());
    }

    void backtrack()
    {
        // Backwards generate the backtracking code.
        size_t opIndex = m_ops.size();
        ASSERT(opIndex);

        do {
            --opIndex;

            if (m_disassembler)
                m_disassembler->setForBacktrack(opIndex, m_jit.label());

            YarrOp& op = m_ops[opIndex];
            switch (op.m_op) {

            case YarrOpCode::Term:
                backtrackTerm(opIndex);
                break;

            // YarrOpCode::BodyAlternativeBegin/Next/End
            //
            // For each Begin/Next node representing an alternative, we need to decide what to do
            // in two circumstances:
            //  - If we backtrack back into this node, from within the alternative.
            //  - If the input check at the head of the alternative fails (if this exists).
            //
            // We treat these two cases differently since in the former case we have slightly
            // more information - since we are backtracking out of a prior alternative we know
            // that at least enough input was available to run it. For example, given the regular
            // expression /a|b/, if we backtrack out of the first alternative (a failed pattern
            // character match of 'a'), then we need not perform an additional input availability
            // check before running the second alternative.
            //
            // Backtracking required differs for the last alternative, which in the case of the
            // repeating set of alternatives must loop. The code generated for the last alternative
            // will also be used to handle all input check failures from any prior alternatives -
            // these require similar functionality, in seeking the next available alternative for
            // which there is sufficient input.
            //
            // Since backtracking of all other alternatives simply requires us to link backtracks
            // to the reentry point for the subsequent alternative, we will only be generating any
            // code when backtracking the last alternative.
            case YarrOpCode::BodyAlternativeBegin:
            case YarrOpCode::BodyAlternativeNext: {
                PatternAlternative* alternative = op.m_alternative;

                // Is this the last alternative? If not, then if we backtrack to this point we just
                // need to jump to try to match the next alternative.
                if (m_ops[op.m_nextOp].m_op != YarrOpCode::BodyAlternativeEnd) {
                    m_backtrackingState.linkTo(m_ops[op.m_nextOp].m_reentry, &m_jit);
                    break;
                }
                YarrOp& endOp = m_ops[op.m_nextOp];
                ASSERT(endOp.m_op == YarrOpCode::BodyAlternativeEnd);

                YarrOp* beginOp = &op;
                while (beginOp->m_op != YarrOpCode::BodyAlternativeBegin) {
                    ASSERT(beginOp->m_op == YarrOpCode::BodyAlternativeNext);
                    beginOp = &m_ops[beginOp->m_previousOp];
                }

                bool onceThrough = endOp.m_nextOp == notFound;
                
                MacroAssembler::JumpList lastStickyAlternativeFailures;

                // First, generate code to handle cases where we backtrack out of an attempted match
                // of the last alternative. If this is a 'once through' set of alternatives then we
                // have nothing to do - link this straight through to the End.
                if (onceThrough)
                    m_backtrackingState.linkTo(endOp.m_reentry, &m_jit);
                else {
                    if (m_pattern.sticky()) {
                        // It is a sticky pattern and the last alternative failed, jump to the end.
                        m_backtrackingState.takeBacktracksToJumpList(lastStickyAlternativeFailures, &m_jit);
                    } else if (m_pattern.m_body->m_hasFixedSize
                        && (alternative->m_minimumSize > beginOp->m_alternative->m_minimumSize)
                        && (alternative->m_minimumSize - beginOp->m_alternative->m_minimumSize == 1)) {
                        // If we don't need to move the input position, and the pattern has a fixed size
                        // (in which case we omit the store of the start index until the pattern has matched)
                        // then we can just link the backtrack out of the last alternative straight to the
                        // head of the first alternative.
                        m_backtrackingState.linkTo(beginOp->m_reentry, &m_jit);
                    } else {
                        // We need to generate a trampoline of code to execute before looping back
                        // around to the first alternative.
                        m_backtrackingState.link(&m_jit);

                        // No need to advance and retry for a sticky pattern. And it is already handled before this branch.
                        ASSERT(!m_pattern.sticky());

                        // If the pattern size is not fixed, then store the start index for use if we match.
                        if (!m_pattern.m_body->m_hasFixedSize) {
                            if (alternative->m_minimumSize == 1)
                                setMatchStart(m_regs.index);
                            else {
                                if (alternative->m_minimumSize)
                                    m_jit.sub32(m_regs.index, MacroAssembler::Imm32(alternative->m_minimumSize - 1), m_regs.regT0);
                                else
                                    m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index, m_regs.regT0);
                                setMatchStart(m_regs.regT0);
                            }
                        }

                        // Generate code to loop. Check whether the last alternative is longer than the
                        // first (e.g. /a|xy/ or /a|xyz/).
                        if (alternative->m_minimumSize > beginOp->m_alternative->m_minimumSize) {
                            // We want to loop, and increment input position. If the delta is 1, it is
                            // already correctly incremented, if more than one then decrement as appropriate.
                            unsigned delta = alternative->m_minimumSize - beginOp->m_alternative->m_minimumSize;
                            ASSERT(delta);
                            if (delta != 1)
                                m_jit.sub32(MacroAssembler::Imm32(delta - 1), m_regs.index);
                            m_jit.jump(beginOp->m_reentry);
                        } else {
                            // If the first alternative has minimum size 0xFFFFFFFFu, then there cannot
                            // be sufficent input available to handle this, so just fall through.
                            unsigned delta = beginOp->m_alternative->m_minimumSize - alternative->m_minimumSize;
                            if (delta != 0xFFFFFFFFu) {
                                // We need to check input because we are incrementing the input.
                                m_jit.add32(MacroAssembler::Imm32(delta + 1), m_regs.index);
                                checkInput().linkTo(beginOp->m_reentry, &m_jit);
                            }
                        }
                    }
                }

                // We can reach this point in the code in two ways:
                //  - Fallthrough from the code above (a repeating alternative backtracked out of its
                //    last alternative, and did not have sufficent input to run the first).
                //  - We will loop back up to the following label when a repeating alternative loops,
                //    following a failed input check.
                //
                // Either way, we have just failed the input check for the first alternative.
                MacroAssembler::Label firstInputCheckFailed(&m_jit);

                // Generate code to handle input check failures from alternatives except the last.
                // prevOp is the alternative we're handling a bail out from (initially Begin), and
                // nextOp is the alternative we will be attempting to reenter into.
                // 
                // We will link input check failures from the forwards matching path back to the code
                // that can handle them.
                YarrOp* prevOp = beginOp;
                YarrOp* nextOp = &m_ops[beginOp->m_nextOp];
                while (nextOp->m_op != YarrOpCode::BodyAlternativeEnd) {
                    prevOp->m_jumps.link(&m_jit);

                    // We only get here if an input check fails, it is only worth checking again
                    // if the next alternative has a minimum size less than the last.
                    if (prevOp->m_alternative->m_minimumSize > nextOp->m_alternative->m_minimumSize) {
                        // FIXME: if we added an extra label to YarrOp, we could avoid needing to
                        // subtract delta back out, and reduce this code. Should performance test
                        // the benefit of this.
                        unsigned delta = prevOp->m_alternative->m_minimumSize - nextOp->m_alternative->m_minimumSize;
                        m_jit.sub32(MacroAssembler::Imm32(delta), m_regs.index);
                        MacroAssembler::Jump fail = jumpIfNoAvailableInput();
                        m_jit.add32(MacroAssembler::Imm32(delta), m_regs.index);
                        m_jit.jump(nextOp->m_reentry);
                        fail.link(&m_jit);
                    } else if (prevOp->m_alternative->m_minimumSize < nextOp->m_alternative->m_minimumSize)
                        m_jit.add32(MacroAssembler::Imm32(nextOp->m_alternative->m_minimumSize - prevOp->m_alternative->m_minimumSize), m_regs.index);
                    prevOp = nextOp;
                    nextOp = &m_ops[nextOp->m_nextOp];
                }

                // We fall through to here if there is insufficient input to run the last alternative.

                // If there is insufficient input to run the last alternative, then for 'once through'
                // alternatives we are done - just jump back up into the forwards matching path at the End.
                if (onceThrough) {
                    op.m_jumps.linkTo(endOp.m_reentry, &m_jit);
                    m_jit.jump(endOp.m_reentry);
                    break;
                }

                // For repeating alternatives, link any input check failure from the last alternative to
                // this point.
                op.m_jumps.link(&m_jit);

                bool needsToUpdateMatchStart = !m_pattern.m_body->m_hasFixedSize;

                // Check for cases where input position is already incremented by 1 for the last
                // alternative (this is particularly useful where the minimum size of the body
                // disjunction is 0, e.g. /a*|b/).
                if (needsToUpdateMatchStart && alternative->m_minimumSize == 1) {
                    // index is already incremented by 1, so just store it now!
                    setMatchStart(m_regs.index);
                    needsToUpdateMatchStart = false;
                }

                if (!m_pattern.sticky()) {
                    // Check whether there is sufficient input to loop. Increment the input position by
                    // one, and check. Also add in the minimum disjunction size before checking - there
                    // is no point in looping if we're just going to fail all the input checks around
                    // the next iteration.
                    ASSERT(alternative->m_minimumSize >= m_pattern.m_body->m_minimumSize);
                    if (alternative->m_minimumSize == m_pattern.m_body->m_minimumSize) {
                        // If the last alternative had the same minimum size as the disjunction,
                        // just simply increment input pos by 1, no adjustment based on minimum size.
                        m_jit.add32(MacroAssembler::TrustedImm32(1), m_regs.index);
                    } else {
                        // If the minumum for the last alternative was one greater than than that
                        // for the disjunction, we're already progressed by 1, nothing to do!
                        unsigned delta = (alternative->m_minimumSize - m_pattern.m_body->m_minimumSize) - 1;
                        if (delta)
                            m_jit.sub32(MacroAssembler::Imm32(delta), m_regs.index);
                    }
                    MacroAssembler::Jump matchFailed = jumpIfNoAvailableInput();

                    if (needsToUpdateMatchStart) {
                        if (!m_pattern.m_body->m_minimumSize)
                            setMatchStart(m_regs.index);
                        else {
                            m_jit.sub32(m_regs.index, MacroAssembler::Imm32(m_pattern.m_body->m_minimumSize), m_regs.regT0);
                            setMatchStart(m_regs.regT0);
                        }
                    }

                    // Calculate how much more input the first alternative requires than the minimum
                    // for the body as a whole. If no more is needed then we dont need an additional
                    // input check here - jump straight back up to the start of the first alternative.
                    if (beginOp->m_alternative->m_minimumSize == m_pattern.m_body->m_minimumSize)
                        m_jit.jump(beginOp->m_reentry);
                    else {
                        if (beginOp->m_alternative->m_minimumSize > m_pattern.m_body->m_minimumSize)
                            m_jit.add32(MacroAssembler::Imm32(beginOp->m_alternative->m_minimumSize - m_pattern.m_body->m_minimumSize), m_regs.index);
                        else
                            m_jit.sub32(MacroAssembler::Imm32(m_pattern.m_body->m_minimumSize - beginOp->m_alternative->m_minimumSize), m_regs.index);
                        checkInput().linkTo(beginOp->m_reentry, &m_jit);
                        m_jit.jump(firstInputCheckFailed);
                    }

                    // We jump to here if we iterate to the point that there is insufficient input to
                    // run any matches, and need to return a failure state from JIT code.
                    matchFailed.link(&m_jit);
                }

                lastStickyAlternativeFailures.link(&m_jit);
                removeCallFrame();
                generateFailReturn();
                break;
            }
            case YarrOpCode::BodyAlternativeEnd: {
                // We should never backtrack back into a body disjunction.
                ASSERT(m_backtrackingState.isEmpty());
                break;
            }

            // YarrOpCode::SimpleNestedAlternativeBegin/Next/End
            // YarrOpCode::NestedAlternativeBegin/Next/End
            //
            // Generate code for when we backtrack back out of an alternative into
            // a Begin or Next node, or when the entry input count check fails. If
            // there are more alternatives we need to jump to the next alternative,
            // if not we backtrack back out of the current set of parentheses.
            //
            // In the case of non-simple nested assertions we need to also link the
            // 'return address' appropriately to backtrack back out into the correct
            // alternative.
            case YarrOpCode::SimpleNestedAlternativeBegin:
            case YarrOpCode::SimpleNestedAlternativeNext:
            case YarrOpCode::NestedAlternativeBegin:
            case YarrOpCode::NestedAlternativeNext: {
                YarrOp& nextOp = m_ops[op.m_nextOp];
                bool isBegin = op.m_previousOp == notFound;
                bool isLastAlternative = nextOp.m_nextOp == notFound;
                ASSERT(isBegin == (op.m_op == YarrOpCode::SimpleNestedAlternativeBegin || op.m_op == YarrOpCode::NestedAlternativeBegin));
                ASSERT(isLastAlternative == (nextOp.m_op == YarrOpCode::SimpleNestedAlternativeEnd || nextOp.m_op == YarrOpCode::NestedAlternativeEnd));

                // Treat an input check failure the same as a failed match.
                m_backtrackingState.append(op.m_jumps);

                // Set the backtracks to jump to the appropriate place. We may need
                // to link the backtracks in one of three different way depending on
                // the type of alternative we are dealing with:
                //  - A single alternative, with no siblings.
                //  - The last alternative of a set of two or more.
                //  - An alternative other than the last of a set of two or more.
                //
                // In the case of a single alternative on its own, we don't need to
                // jump anywhere - if the alternative fails to match we can just
                // continue to backtrack out of the parentheses without jumping.
                //
                // In the case of the last alternative in a set of more than one, we
                // need to jump to return back out to the beginning. We'll do so by
                // adding a jump to the End node's m_jumps list, and linking this
                // when we come to generate the Begin node. For alternatives other
                // than the last, we need to jump to the next alternative.
                //
                // If the alternative had adjusted the input position we must link
                // backtracking to here, correct, and then jump on. If not we can
                // link the backtracks directly to their destination.
                if (op.m_checkAdjust) {
                    // Handle the cases where we need to link the backtracks here.
                    m_backtrackingState.link(&m_jit);
                    m_jit.sub32(MacroAssembler::Imm32(op.m_checkAdjust), m_regs.index);
                    if (!isLastAlternative) {
                        // An alternative that is not the last should jump to its successor.
                        m_jit.jump(nextOp.m_reentry);
                    } else if (!isBegin) {
                        // The last of more than one alternatives must jump back to the beginning.
                        nextOp.m_jumps.append(m_jit.jump());
                    } else {
                        // A single alternative on its own can fall through.
                        m_backtrackingState.fallthrough();
                    }
                } else {
                    // Handle the cases where we can link the backtracks directly to their destinations.
                    if (!isLastAlternative) {
                        // An alternative that is not the last should jump to its successor.
                        m_backtrackingState.linkTo(nextOp.m_reentry, &m_jit);
                    } else if (!isBegin) {
                        // The last of more than one alternatives must jump back to the beginning.
                        m_backtrackingState.takeBacktracksToJumpList(nextOp.m_jumps, &m_jit);
                    }
                    // In the case of a single alternative on its own do nothing - it can fall through.
                }

                // If there is a backtrack jump from a zero length match link it here.
                if (op.m_zeroLengthMatch.isSet())
                    m_backtrackingState.append(op.m_zeroLengthMatch);

                // At this point we've handled the backtracking back into this node.
                // Now link any backtracks that need to jump to here.

                // For non-simple alternatives, link the alternative's 'return address'
                // so that we backtrack back out into the previous alternative.
                if (op.m_op == YarrOpCode::NestedAlternativeNext)
                    m_backtrackingState.append(op.m_returnAddress);

                // If there is more than one alternative, then the last alternative will
                // have planted a jump to be linked to the end. This jump was added to the
                // End node's m_jumps list. If we are back at the beginning, link it here.
                if (isBegin) {
                    YarrOp* endOp = &m_ops[op.m_nextOp];
                    while (endOp->m_nextOp != notFound) {
                        ASSERT(endOp->m_op == YarrOpCode::SimpleNestedAlternativeNext || endOp->m_op == YarrOpCode::NestedAlternativeNext);
                        endOp = &m_ops[endOp->m_nextOp];
                    }
                    ASSERT(endOp->m_op == YarrOpCode::SimpleNestedAlternativeEnd || endOp->m_op == YarrOpCode::NestedAlternativeEnd);
                    m_backtrackingState.append(endOp->m_jumps);
                }
                break;
            }
            case YarrOpCode::SimpleNestedAlternativeEnd:
            case YarrOpCode::NestedAlternativeEnd: {
                PatternTerm* term = op.m_term;

                // If there is a backtrack jump from a zero length match link it here.
                if (op.m_zeroLengthMatch.isSet())
                    m_backtrackingState.append(op.m_zeroLengthMatch);

                // If we backtrack into the end of a simple subpattern do nothing;
                // just continue through into the last alternative. If we backtrack
                // into the end of a non-simple set of alterntives we need to jump
                // to the backtracking return address set up during generation.
                if (op.m_op == YarrOpCode::NestedAlternativeEnd) {
                    m_backtrackingState.link(&m_jit);

                    // Plant a jump to the return address.
                    unsigned parenthesesFrameLocation = term->frameLocation;
                    loadFromFrameAndJump(parenthesesFrameLocation + BackTrackInfoParentheses::returnAddressIndex());

                    // Link the DataLabelPtr associated with the end of the last
                    // alternative to this point.
                    m_backtrackingState.append(op.m_returnAddress);
                }
                break;
            }

            // YarrOpCode::ParenthesesSubpatternOnceBegin/End
            //
            // When we are backtracking back out of a capturing subpattern we need
            // to clear the start index in the matches output array, to record that
            // this subpattern has not been captured.
            //
            // When backtracking back out of a Greedy quantified subpattern we need
            // to catch this, and try running the remainder of the alternative after
            // the subpattern again, skipping the parentheses.
            //
            // Upon backtracking back into a quantified set of parentheses we need to
            // check whether we were currently skipping the subpattern. If not, we
            // can backtrack into them, if we were we need to either backtrack back
            // out of the start of the parentheses, or jump back to the forwards
            // matching start, depending of whether the match is Greedy or NonGreedy.
            case YarrOpCode::ParenthesesSubpatternOnceBegin: {
                PatternTerm* term = op.m_term;
                ASSERT(term->quantityMaxCount == 1);

                // We only need to backtrack to this point if capturing or greedy.
                if ((term->capture() && m_compileMode == JITCompileMode::IncludeSubpatterns) || term->quantityType == QuantifierType::Greedy) {
                    m_backtrackingState.link(&m_jit);

                    // If capturing, clear the capture (we only need to reset start).
                    if (term->capture() && m_compileMode == JITCompileMode::IncludeSubpatterns)
                        clearSubpatternStart(term->parentheses.subpatternId);

                    // If Greedy, jump to the end.
                    if (term->quantityType == QuantifierType::Greedy) {
                        // Clear the flag in the stackframe indicating we ran through the subpattern.
                        unsigned parenthesesFrameLocation = term->frameLocation;
                        storeToFrame(MacroAssembler::TrustedImm32(-1), parenthesesFrameLocation + BackTrackInfoParenthesesOnce::beginIndex());
                        // Jump to after the parentheses, skipping the subpattern.
                        m_jit.jump(m_ops[op.m_nextOp].m_reentry);
                        // A backtrack from after the parentheses, when skipping the subpattern,
                        // will jump back to here.
                        op.m_jumps.link(&m_jit);
                    }

                    m_backtrackingState.fallthrough();
                }
                break;
            }
            case YarrOpCode::ParenthesesSubpatternOnceEnd: {
                PatternTerm* term = op.m_term;

                if (term->quantityType != QuantifierType::FixedCount) {
                    m_backtrackingState.link(&m_jit);

                    // Check whether we should backtrack back into the parentheses, or if we
                    // are currently in a state where we had skipped over the subpattern
                    // (in which case the flag value on the stack will be -1).
                    unsigned parenthesesFrameLocation = term->frameLocation;
                    MacroAssembler::Jump hadSkipped = m_jit.branch32(MacroAssembler::Equal, MacroAssembler::Address(MacroAssembler::stackPointerRegister, (parenthesesFrameLocation + BackTrackInfoParenthesesOnce::beginIndex()) * sizeof(void*)), MacroAssembler::TrustedImm32(-1));

                    if (term->quantityType == QuantifierType::Greedy) {
                        // For Greedy parentheses, we skip after having already tried going
                        // through the subpattern, so if we get here we're done.
                        YarrOp& beginOp = m_ops[op.m_previousOp];
                        beginOp.m_jumps.append(hadSkipped);
                    } else {
                        // For NonGreedy parentheses, we try skipping the subpattern first,
                        // so if we get here we need to try running through the subpattern
                        // next. Jump back to the start of the parentheses in the forwards
                        // matching path.
                        ASSERT(term->quantityType == QuantifierType::NonGreedy);
                        YarrOp& beginOp = m_ops[op.m_previousOp];
                        hadSkipped.linkTo(beginOp.m_reentry, &m_jit);
                    }

                    m_backtrackingState.fallthrough();
                }

                m_backtrackingState.append(op.m_jumps);
                break;
            }

            // YarrOpCode::ParenthesesSubpatternTerminalBegin/End
            //
            // Terminal subpatterns will always match - there is nothing after them to
            // force a backtrack, and they have a minimum count of 0, and as such will
            // always produce an acceptable result.
            case YarrOpCode::ParenthesesSubpatternTerminalBegin: {
                // We will backtrack to this point once the subpattern cannot match any
                // more. Since no match is accepted as a successful match (we are Greedy
                // quantified with a minimum of zero) jump back to the forwards matching
                // path at the end.
                YarrOp& endOp = m_ops[op.m_nextOp];
                m_backtrackingState.linkTo(endOp.m_reentry, &m_jit);
                break;
            }
            case YarrOpCode::ParenthesesSubpatternTerminalEnd:
                // We should never be backtracking to here (hence the 'terminal' in the name).
                ASSERT(m_backtrackingState.isEmpty());
                m_backtrackingState.append(op.m_jumps);
                break;

            // YarrOpCode::ParenthesesSubpatternBegin/End
            //
            // When we are backtracking back out of a capturing subpattern we need
            // to clear the start index in the matches output array, to record that
            // this subpattern has not been captured.
            //
            // When backtracking back out of a Greedy quantified subpattern we need
            // to catch this, and try running the remainder of the alternative after
            // the subpattern again, skipping the parentheses.
            //
            // Upon backtracking back into a quantified set of parentheses we need to
            // check whether we were currently skipping the subpattern. If not, we
            // can backtrack into them, if we were we need to either backtrack back
            // out of the start of the parentheses, or jump back to the forwards
            // matching start, depending of whether the match is Greedy or NonGreedy.
            case YarrOpCode::ParenthesesSubpatternBegin: {
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
                PatternTerm* term = op.m_term;
                unsigned parenthesesFrameLocation = term->frameLocation;

                if (term->quantityType != QuantifierType::FixedCount) {
                    m_backtrackingState.link(&m_jit);

                    MacroAssembler::RegisterID currParenContextReg = m_regs.regT0;
                    MacroAssembler::RegisterID newParenContextReg = m_regs.regT1;

                    loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheses::parenContextHeadIndex(), currParenContextReg);
                    
                    restoreParenContext(currParenContextReg, m_regs.regT2, term->parentheses.subpatternId, term->parentheses.lastSubpatternId, parenthesesFrameLocation);
                    
                    freeParenContext(currParenContextReg, newParenContextReg);
                    storeToFrame(newParenContextReg, parenthesesFrameLocation + BackTrackInfoParentheses::parenContextHeadIndex());

                    const MacroAssembler::RegisterID countTemporary = m_regs.regT0;
                    loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheses::matchAmountIndex(), countTemporary);
                    MacroAssembler::Jump zeroLengthMatch = m_jit.branchTest32(MacroAssembler::Zero, countTemporary);

                    m_jit.sub32(MacroAssembler::TrustedImm32(1), countTemporary);
                    storeToFrame(countTemporary, parenthesesFrameLocation + BackTrackInfoParentheses::matchAmountIndex());

                    m_jit.jump(m_ops[op.m_nextOp].m_reentry);

                    zeroLengthMatch.link(&m_jit);

                    // Clear the flag in the stackframe indicating we didn't run through the subpattern.
                    storeToFrame(MacroAssembler::TrustedImm32(-1), parenthesesFrameLocation + BackTrackInfoParentheses::beginIndex());

                    if (term->quantityType == QuantifierType::Greedy)
                        m_jit.jump(m_ops[op.m_nextOp].m_reentry);

                    // If Greedy, jump to the end.
                    if (term->quantityType == QuantifierType::Greedy) {
                        // A backtrack from after the parentheses, when skipping the subpattern,
                        // will jump back to here.
                        op.m_jumps.link(&m_jit);
                    }

                    m_backtrackingState.fallthrough();
                }
#else // !YARR_JIT_ALL_PARENS_EXPRESSIONS
                RELEASE_ASSERT_NOT_REACHED();
#endif
                break;
            }
            case YarrOpCode::ParenthesesSubpatternEnd: {
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
                PatternTerm* term = op.m_term;

                if (term->quantityType != QuantifierType::FixedCount) {
                    m_backtrackingState.link(&m_jit);

                    unsigned parenthesesFrameLocation = term->frameLocation;

                    if (term->quantityType == QuantifierType::Greedy) {
                        // Check whether we should backtrack back into the parentheses, or if we
                        // are currently in a state where we had skipped over the subpattern
                        // (in which case the flag value on the stack will be -1).
                        MacroAssembler::Jump hadSkipped = m_jit.branch32(MacroAssembler::Equal, MacroAssembler::Address(MacroAssembler::stackPointerRegister, (parenthesesFrameLocation  + BackTrackInfoParentheses::beginIndex()) * sizeof(void*)), MacroAssembler::TrustedImm32(-1));

                        // For Greedy parentheses, we skip after having already tried going
                        // through the subpattern, so if we get here we're done.
                        YarrOp& beginOp = m_ops[op.m_previousOp];
                        beginOp.m_jumps.append(hadSkipped);
                    } else {
                        // For NonGreedy parentheses, we try skipping the subpattern first,
                        // so if we get here we need to try running through the subpattern
                        // next. Jump back to the start of the parentheses in the forwards
                        // matching path.
                        ASSERT(term->quantityType == QuantifierType::NonGreedy);

                        const MacroAssembler::RegisterID beginTemporary = m_regs.regT0;
                        const MacroAssembler::RegisterID countTemporary = m_regs.regT1;

                        YarrOp& beginOp = m_ops[op.m_previousOp];

                        loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheses::beginIndex(), beginTemporary);
                        m_jit.branch32(MacroAssembler::Equal, beginTemporary, MacroAssembler::TrustedImm32(-1)).linkTo(beginOp.m_reentry, &m_jit);

                        MacroAssembler::JumpList exceededMatchLimit;

                        if (term->quantityMaxCount != quantifyInfinite) {
                            loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheses::matchAmountIndex(), countTemporary);
                            exceededMatchLimit.append(m_jit.branch32(MacroAssembler::AboveOrEqual, countTemporary, MacroAssembler::Imm32(term->quantityMaxCount)));
                        }

                        m_jit.branch32(MacroAssembler::Above, m_regs.index, beginTemporary).linkTo(beginOp.m_reentry, &m_jit);

                        exceededMatchLimit.link(&m_jit);
                    }

                    m_backtrackingState.fallthrough();
                }

                m_backtrackingState.append(op.m_jumps);
#else // !YARR_JIT_ALL_PARENS_EXPRESSIONS
                RELEASE_ASSERT_NOT_REACHED();
#endif
                break;
            }

            // YarrOpCode::ParentheticalAssertionBegin/End
            case YarrOpCode::ParentheticalAssertionBegin: {
                PatternTerm* term = op.m_term;
                YarrOp& endOp = m_ops[op.m_nextOp];

                // We need to handle the backtracks upon backtracking back out
                // of a parenthetical assertion if either we need to correct
                // the input index, or the assertion was inverted.
                if (op.m_checkAdjust || term->invert()) {
                    m_backtrackingState.link(&m_jit);

                    if (op.m_checkAdjust)
                        m_jit.add32(MacroAssembler::Imm32(op.m_checkAdjust), m_regs.index);

                    // In an inverted assertion failure to match the subpattern
                    // is treated as a successful match - jump to the end of the
                    // subpattern. We already have adjusted the input position
                    // back to that before the assertion, which is correct.
                    if (term->invert())
                        m_jit.jump(endOp.m_reentry);

                    m_backtrackingState.fallthrough();
                }

                // The End node's jump list will contain any backtracks into
                // the end of the assertion. Also, if inverted, we will have
                // added the failure caused by a successful match to this.
                m_backtrackingState.append(endOp.m_jumps);
                break;
            }
            case YarrOpCode::ParentheticalAssertionEnd: {
                // FIXME: We should really be clearing any nested subpattern
                // matches on bailing out from after the pattern. Firefox has
                // this bug too (presumably because they use YARR!)

                // Never backtrack into an assertion; later failures bail to before the begin.
                m_backtrackingState.takeBacktracksToJumpList(op.m_jumps, &m_jit);
                break;
            }

            case YarrOpCode::MatchFailed:
                break;
            }
        } while (opIndex);
    }

    // Compilation methods:
    // ====================

    // opCompileParenthesesSubpattern
    // Emits ops for a subpattern (set of parentheses). These consist
    // of a set of alternatives wrapped in an outer set of nodes for
    // the parentheses.
    // Supported types of parentheses are 'Once' (quantityMaxCount == 1),
    // 'Terminal' (non-capturing parentheses quantified as greedy
    // and infinite), and 0 based greedy / non-greedy quantified parentheses.
    // Alternatives will use the 'Simple' set of ops if either the
    // subpattern is terminal (in which case we will never need to
    // backtrack), or if the subpattern only contains one alternative.
    void opCompileParenthesesSubpattern(Checked<unsigned> checkedOffset, PatternTerm* term)
    {
        YarrOpCode parenthesesBeginOpCode;
        YarrOpCode parenthesesEndOpCode;
        YarrOpCode alternativeBeginOpCode = YarrOpCode::SimpleNestedAlternativeBegin;
        YarrOpCode alternativeNextOpCode = YarrOpCode::SimpleNestedAlternativeNext;
        YarrOpCode alternativeEndOpCode = YarrOpCode::SimpleNestedAlternativeEnd;

        if (UNLIKELY(!isSafeToRecurse())) {
            m_failureReason = JITFailureReason::ParenthesisNestedTooDeep;
            return;
        }

        // We can currently only compile quantity 1 subpatterns that are
        // not copies. We generate a copy in the case of a range quantifier,
        // e.g. /(?:x){3,9}/, or /(?:x)+/ (These are effectively expanded to
        // /(?:x){3,3}(?:x){0,6}/ and /(?:x)(?:x)*/ repectively). The problem
        // comes where the subpattern is capturing, in which case we would
        // need to restore the capture from the first subpattern upon a
        // failure in the second.
        if (term->quantityMinCount && term->quantityMinCount != term->quantityMaxCount) {
            m_failureReason = JITFailureReason::VariableCountedParenthesisWithNonZeroMinimum;
            return;
        }
        
        if (term->quantityMaxCount == 1 && !term->parentheses.isCopy) {
            // Select the 'Once' nodes.
            parenthesesBeginOpCode = YarrOpCode::ParenthesesSubpatternOnceBegin;
            parenthesesEndOpCode = YarrOpCode::ParenthesesSubpatternOnceEnd;

            // If there is more than one alternative we cannot use the 'simple' nodes.
            if (term->parentheses.disjunction->m_alternatives.size() != 1) {
                alternativeBeginOpCode = YarrOpCode::NestedAlternativeBegin;
                alternativeNextOpCode = YarrOpCode::NestedAlternativeNext;
                alternativeEndOpCode = YarrOpCode::NestedAlternativeEnd;
            }
        } else if (term->parentheses.isTerminal) {
            // Select the 'Terminal' nodes.
            parenthesesBeginOpCode = YarrOpCode::ParenthesesSubpatternTerminalBegin;
            parenthesesEndOpCode = YarrOpCode::ParenthesesSubpatternTerminalEnd;
        } else {
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
            // We only handle generic parenthesis with non-fixed counts.
            if (term->quantityType == QuantifierType::FixedCount) {
                // This subpattern is not supported by the JIT.
                m_failureReason = JITFailureReason::FixedCountParenthesizedSubpattern;
                return;
            }

            m_containsNestedSubpatterns = true;

            // Select the 'Generic' nodes.
            parenthesesBeginOpCode = YarrOpCode::ParenthesesSubpatternBegin;
            parenthesesEndOpCode = YarrOpCode::ParenthesesSubpatternEnd;

            // If there is more than one alternative we cannot use the 'simple' nodes.
            if (term->parentheses.disjunction->m_alternatives.size() != 1) {
                alternativeBeginOpCode = YarrOpCode::NestedAlternativeBegin;
                alternativeNextOpCode = YarrOpCode::NestedAlternativeNext;
                alternativeEndOpCode = YarrOpCode::NestedAlternativeEnd;
            }
#else
            // This subpattern is not supported by the JIT.
            m_failureReason = JITFailureReason::ParenthesizedSubpattern;
            return;
#endif
        }

        size_t parenBegin = m_ops.size();
        m_ops.append(parenthesesBeginOpCode);

        m_ops.append(alternativeBeginOpCode);
        m_ops.last().m_previousOp = notFound;
        m_ops.last().m_term = term;
        PatternDisjunction* disjunction = term->parentheses.disjunction;
        auto& alternatives = disjunction->m_alternatives;
        for (unsigned i = 0; i < alternatives.size(); ++i) {
            size_t lastOpIndex = m_ops.size() - 1;

            PatternAlternative* nestedAlternative = alternatives[i].get();
            {
                // Calculate how much input we need to check for, and if non-zero check.
                YarrOp& lastOp = m_ops[lastOpIndex];
                lastOp.m_checkAdjust = nestedAlternative->m_minimumSize;
                if ((term->quantityType == QuantifierType::FixedCount) && (term->type != PatternTerm::Type::ParentheticalAssertion))
                    lastOp.m_checkAdjust -= disjunction->m_minimumSize;

                Checked<unsigned, RecordOverflow> checkedOffsetResult(checkedOffset);
                checkedOffsetResult += lastOp.m_checkAdjust;

                if (UNLIKELY(checkedOffsetResult.hasOverflowed())) {
                    m_failureReason = JITFailureReason::OffsetTooLarge;
                    return;
                }

                lastOp.m_checkedOffset = checkedOffsetResult;
            }
            opCompileAlternative(m_ops[lastOpIndex].m_checkedOffset, nestedAlternative);

            size_t thisOpIndex = m_ops.size();
            m_ops.append(YarrOp(alternativeNextOpCode));

            YarrOp& lastOp = m_ops[lastOpIndex];
            YarrOp& thisOp = m_ops[thisOpIndex];

            lastOp.m_alternative = nestedAlternative;
            lastOp.m_nextOp = thisOpIndex;
            thisOp.m_previousOp = lastOpIndex;
            thisOp.m_term = term;
        }
        YarrOp& lastOp = m_ops.last();
        ASSERT(lastOp.m_op == alternativeNextOpCode);
        lastOp.m_op = alternativeEndOpCode;
        lastOp.m_alternative = nullptr;
        lastOp.m_nextOp = notFound;
        lastOp.m_checkedOffset = checkedOffset;

        size_t parenEnd = m_ops.size();
        m_ops.append(parenthesesEndOpCode);

        m_ops[parenBegin].m_term = term;
        m_ops[parenBegin].m_previousOp = notFound;
        m_ops[parenBegin].m_nextOp = parenEnd;
        m_ops[parenBegin].m_checkedOffset = checkedOffset;
        m_ops[parenEnd].m_term = term;
        m_ops[parenEnd].m_previousOp = parenBegin;
        m_ops[parenEnd].m_nextOp = notFound;
        m_ops[parenEnd].m_checkedOffset = checkedOffset;
    }

    // opCompileParentheticalAssertion
    // Emits ops for a parenthetical assertion. These consist of an
    // YarrOpCode::SimpleNestedAlternativeBegin/Next/End set of nodes wrapping
    // the alternatives, with these wrapped by an outer pair of
    // YarrOpCode::ParentheticalAssertionBegin/End nodes.
    // We can always use the OpSimpleNestedAlternative nodes in the
    // case of parenthetical assertions since these only ever match
    // once, and will never backtrack back into the assertion.
    void opCompileParentheticalAssertion(Checked<unsigned> checkedOffset, PatternTerm* term)
    {
        if (UNLIKELY(!isSafeToRecurse())) {
            m_failureReason = JITFailureReason::ParenthesisNestedTooDeep;
            return;
        }

        auto originalCheckedOffset = checkedOffset;
        size_t parenBegin = m_ops.size();
        m_ops.append(YarrOpCode::ParentheticalAssertionBegin);
        m_ops.last().m_checkAdjust = checkedOffset - term->inputPosition;
        checkedOffset -= m_ops.last().m_checkAdjust;
        m_ops.last().m_checkedOffset = checkedOffset;

        m_ops.append(YarrOpCode::SimpleNestedAlternativeBegin);
        m_ops.last().m_previousOp = notFound;
        m_ops.last().m_term = term;
        PatternDisjunction* disjunction = term->parentheses.disjunction;
        auto& alternatives = disjunction->m_alternatives;
        for (unsigned i = 0; i < alternatives.size(); ++i) {
            size_t lastOpIndex = m_ops.size() - 1;

            PatternAlternative* nestedAlternative = alternatives[i].get();
            {
                // Calculate how much input we need to check for, and if non-zero check.
                YarrOp& lastOp = m_ops[lastOpIndex];
                lastOp.m_checkAdjust = nestedAlternative->m_minimumSize;
                if ((term->quantityType == QuantifierType::FixedCount) && (term->type != PatternTerm::Type::ParentheticalAssertion))
                    lastOp.m_checkAdjust -= disjunction->m_minimumSize;
                lastOp.m_checkedOffset = checkedOffset + lastOp.m_checkAdjust;
            }
            opCompileAlternative(m_ops[lastOpIndex].m_checkedOffset, nestedAlternative);

            size_t thisOpIndex = m_ops.size();
            m_ops.append(YarrOp(YarrOpCode::SimpleNestedAlternativeNext));

            YarrOp& lastOp = m_ops[lastOpIndex];
            YarrOp& thisOp = m_ops[thisOpIndex];

            lastOp.m_alternative = nestedAlternative;
            lastOp.m_nextOp = thisOpIndex;
            thisOp.m_previousOp = lastOpIndex;
            thisOp.m_term = term;
        }
        YarrOp& lastOp = m_ops.last();
        ASSERT(lastOp.m_op == YarrOpCode::SimpleNestedAlternativeNext);
        lastOp.m_op = YarrOpCode::SimpleNestedAlternativeEnd;
        lastOp.m_alternative = nullptr;
        lastOp.m_nextOp = notFound;
        lastOp.m_checkedOffset = checkedOffset;

        size_t parenEnd = m_ops.size();
        m_ops.append(YarrOpCode::ParentheticalAssertionEnd);

        m_ops[parenBegin].m_term = term;
        m_ops[parenBegin].m_previousOp = notFound;
        m_ops[parenBegin].m_nextOp = parenEnd;
        m_ops[parenEnd].m_term = term;
        m_ops[parenEnd].m_previousOp = parenBegin;
        m_ops[parenEnd].m_nextOp = notFound;
        m_ops[parenEnd].m_checkedOffset = originalCheckedOffset;
    }

    // opCompileAlternative
    // Called to emit nodes for all terms in an alternative.
    void opCompileAlternative(Checked<unsigned> checkedOffset, PatternAlternative* alternative)
    {
        optimizeAlternative(alternative);

        for (unsigned i = 0; i < alternative->m_terms.size(); ++i) {
            PatternTerm* term = &alternative->m_terms[i];

            switch (term->type) {
            case PatternTerm::Type::ParenthesesSubpattern:
                opCompileParenthesesSubpattern(checkedOffset, term);
                break;

            case PatternTerm::Type::ParentheticalAssertion:
                opCompileParentheticalAssertion(checkedOffset, term);
                break;

            default:
                m_ops.append(term);
                m_ops.last().m_checkedOffset = checkedOffset;
            }
        }
    }

    // opCompileBody
    // This method compiles the body disjunction of the regular expression.
    // The body consists of two sets of alternatives - zero or more 'once
    // through' (BOL anchored) alternatives, followed by zero or more
    // repeated alternatives.
    // For each of these two sets of alteratives, if not empty they will be
    // wrapped in a set of YarrOpCode::BodyAlternativeBegin/Next/End nodes (with the
    // 'begin' node referencing the first alternative, and 'next' nodes
    // referencing any further alternatives. The begin/next/end nodes are
    // linked together in a doubly linked list. In the case of repeating
    // alternatives, the end node is also linked back to the beginning.
    // If no repeating alternatives exist, then a YarrOpCode::MatchFailed node exists
    // to return the failing result.
    void opCompileBody(PatternDisjunction* disjunction)
    {
        if (UNLIKELY(!isSafeToRecurse())) {
            m_failureReason = JITFailureReason::ParenthesisNestedTooDeep;
            return;
        }
        
        auto& alternatives = disjunction->m_alternatives;
        size_t currentAlternativeIndex = 0;

        // Emit the 'once through' alternatives.
        if (alternatives.size() && alternatives[0]->onceThrough()) {
            m_ops.append(YarrOp(YarrOpCode::BodyAlternativeBegin));
            m_ops.last().m_previousOp = notFound;

            do {
                size_t lastOpIndex = m_ops.size() - 1;

                PatternAlternative* alternative = alternatives[currentAlternativeIndex].get();
                m_ops[lastOpIndex].m_checkedOffset = alternative->m_minimumSize;
                opCompileAlternative(alternative->m_minimumSize, alternative);

                size_t thisOpIndex = m_ops.size();
                m_ops.append(YarrOp(YarrOpCode::BodyAlternativeNext));

                YarrOp& lastOp = m_ops[lastOpIndex];
                YarrOp& thisOp = m_ops[thisOpIndex];

                lastOp.m_alternative = alternative;
                lastOp.m_nextOp = thisOpIndex;
                thisOp.m_previousOp = lastOpIndex;
                
                ++currentAlternativeIndex;
            } while (currentAlternativeIndex < alternatives.size() && alternatives[currentAlternativeIndex]->onceThrough());

            YarrOp& lastOp = m_ops.last();

            ASSERT(lastOp.m_op == YarrOpCode::BodyAlternativeNext);
            lastOp.m_op = YarrOpCode::BodyAlternativeEnd;
            lastOp.m_alternative = nullptr;
            lastOp.m_nextOp = notFound;
            lastOp.m_checkedOffset = 0;
        }

        if (currentAlternativeIndex == alternatives.size()) {
            m_ops.append(YarrOp(YarrOpCode::MatchFailed));
            m_ops.last().m_checkedOffset = 0;
            return;
        }

        // Emit the repeated alternatives.
        size_t repeatLoop = m_ops.size();
        m_ops.append(YarrOp(YarrOpCode::BodyAlternativeBegin));
        m_ops.last().m_previousOp = notFound;
        // Collect BoyerMooreInfo if it is possible and profitable. BoyerMooreInfo will be used to emit fast skip path with large stride
        // at the beginning of the body alternatives.
        // We do not emit these fast path when RegExp has sticky or unicode flag. Sticky case does not need this since
        // it fails when the body alternatives fail to match with the current offset.
        // FIXME: Support unicode flag.
        // https://bugs.webkit.org/show_bug.cgi?id=228611
        if (disjunction->m_minimumSize && !m_pattern.sticky() && !m_pattern.unicode()) {
            auto bmInfo = BoyerMooreInfo::create(m_charSize, std::min<unsigned>(disjunction->m_minimumSize, BoyerMooreInfo::maxLength));
            if (collectBoyerMooreInfo(disjunction, currentAlternativeIndex, bmInfo.get())) {
                m_ops.last().m_bmInfo = bmInfo.ptr();
                m_bmInfos.append(WTFMove(bmInfo));
                m_usesT2 = true;
            }
        }

        do {
            size_t lastOpIndex = m_ops.size() - 1;

            PatternAlternative* alternative = alternatives[currentAlternativeIndex].get();
            ASSERT(!alternative->onceThrough());
            m_ops[lastOpIndex].m_checkedOffset = alternative->m_minimumSize;
            opCompileAlternative(alternative->m_minimumSize, alternative);

            size_t thisOpIndex = m_ops.size();
            m_ops.append(YarrOp(YarrOpCode::BodyAlternativeNext));

            YarrOp& lastOp = m_ops[lastOpIndex];
            YarrOp& thisOp = m_ops[thisOpIndex];

            lastOp.m_alternative = alternative;
            lastOp.m_nextOp = thisOpIndex;
            thisOp.m_previousOp = lastOpIndex;
            
            ++currentAlternativeIndex;
        } while (currentAlternativeIndex < alternatives.size());
        YarrOp& lastOp = m_ops.last();
        ASSERT(lastOp.m_op == YarrOpCode::BodyAlternativeNext);
        lastOp.m_op = YarrOpCode::BodyAlternativeEnd;
        lastOp.m_alternative = nullptr;
        lastOp.m_nextOp = repeatLoop;
        lastOp.m_checkedOffset = 0;
    }

    bool collectBoyerMooreInfo(PatternDisjunction* disjunction, size_t currentAlternativeIndex, BoyerMooreInfo& bmInfo)
    {
        // If we have a searching pattern /abcdef/, then we can check the 6th character against a set of {a, b, c, d, e, f}.
        // If it does not match, we can shift 6 characters. We use this strategy since this way can be extended easily to support
        // disjunction, character-class, and ignore-cases. For example, in the case of /(?:abc|def)/, we can check 3rd character
        // against {a, b, c, d, e, f} and shift 3 characters if it does not match.
        //
        // Then, the best way to perform the above shifting is that finding the longest character sequence which does not have
        // many candidates. In the case of /[a-z]aaaaaaa[a-z]/, we can extract "aaaaaaa" sequence and check 8th character against {a}.
        // If it does not match, then we can shift 7 characters (length of "aaaaaaa"). This shifting is better than using "[a-z]aaaaaaa[a-z]"
        // sequence and {a-z} set since {a-z} set will almost always match.
        //
        // We first collect possible characters for each character position. Then, apply heuristics to extract good character sequence from
        // that and construct fast searching with long stride.

        ASSERT(disjunction->m_minimumSize);

        // FIXME: Support nested disjunctions (e.g. /(?:abc|def|g(?:hi|jk))/).
        // https://bugs.webkit.org/show_bug.cgi?id=228614
        // FIXME: Support non-fixed-sized lookahead (e.g. /.*abc/ and extract "abc" sequence).
        // https://bugs.webkit.org/show_bug.cgi?id=228612
        auto& alternatives = disjunction->m_alternatives;
        for (; currentAlternativeIndex < alternatives.size(); ++currentAlternativeIndex) {
            unsigned cursor = 0;
            PatternAlternative* alternative = alternatives[currentAlternativeIndex].get();
            for (unsigned index = 0; index < alternative->m_terms.size() && cursor < bmInfo.length(); ++index) {
                PatternTerm& term = alternative->m_terms[index];
                switch (term.type) {
                case PatternTerm::Type::AssertionBOL:
                case PatternTerm::Type::AssertionEOL:
                case PatternTerm::Type::AssertionWordBoundary:
                case PatternTerm::Type::BackReference:
                case PatternTerm::Type::ForwardReference:
                case PatternTerm::Type::ParenthesesSubpattern:
                case PatternTerm::Type::ParentheticalAssertion:
                case PatternTerm::Type::DotStarEnclosure:
                    break;
                case PatternTerm::Type::CharacterClass: {
                    if (term.quantityType != QuantifierType::FixedCount || term.quantityMaxCount != 1)
                        break;
                    if (term.inputPosition != index)
                        break;
                    auto& characterClass = *term.characterClass;
                    if (term.invert() || characterClass.m_anyCharacter) {
                        bmInfo.setAll(cursor);
                        ++cursor;
                        continue;
                    }
                    if (!characterClass.m_rangesUnicode.isEmpty())
                        bmInfo.addRanges(cursor, characterClass.m_rangesUnicode);
                    if (!characterClass.m_matchesUnicode.isEmpty())
                        bmInfo.addCharacters(cursor, characterClass.m_matchesUnicode);
                    if (!characterClass.m_ranges.isEmpty())
                        bmInfo.addRanges(cursor, characterClass.m_ranges);
                    if (!characterClass.m_matches.isEmpty())
                        bmInfo.addCharacters(cursor, characterClass.m_matches);
                    ++cursor;
                    continue;
                }
                case PatternTerm::Type::PatternCharacter: {
                    if (term.quantityType != QuantifierType::FixedCount || term.quantityMaxCount != 1)
                        break;
                    if (term.inputPosition != index)
                        break;
                    if (U16_LENGTH(term.patternCharacter) != 1 && m_decodeSurrogatePairs)
                        break;
                    // For case-insesitive compares, non-ascii characters that have different
                    // upper & lower case representations are already converted to a character class.
                    ASSERT(!m_pattern.ignoreCase() || isASCIIAlpha(term.patternCharacter) || isCanonicallyUnique(term.patternCharacter, m_canonicalMode));
                    if (m_pattern.ignoreCase() && isASCIIAlpha(term.patternCharacter)) {
                        bmInfo.set(cursor, toASCIIUpper(term.patternCharacter));
                        bmInfo.set(cursor, toASCIILower(term.patternCharacter));
                    } else
                        bmInfo.set(cursor, term.patternCharacter);
                    ++cursor;
                    continue;
                }
                }
                dataLogLnIf(YarrJITInternal::verbose, "Shortening to ", cursor);
                bmInfo.shortenLength(cursor);
                break;
            }
        }
        return bmInfo.length();
    }

    const BoyerMooreBitmap::Map::WordType* getBoyerMooreBitmap(const BoyerMooreBitmap::Map& map)
    {
        if (const auto* existingPointer = m_boyerMooreData->tryReuseBoyerMooreBitmap(map))
            return existingPointer;

        auto heapMap = makeUniqueRef<BoyerMooreBitmap::Map>(map);
        const auto* pointer = heapMap->storage();
        m_bmMaps.append(WTFMove(heapMap));
        return pointer;
    }

    void generateTryReadUnicodeCharacterHelper()
    {
#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (m_tryReadUnicodeCharacterCalls.isEmpty())
            return;

        ASSERT(m_decodeSurrogatePairs);

        m_tryReadUnicodeCharacterEntry = m_jit.label();

        m_jit.tagReturnAddress();

        tryReadUnicodeCharImpl(m_regs.regT0);

        m_jit.ret();
#endif
    }

    void generateEnter()
    {
        auto pushInEnter = [&](GPRReg gpr) {
            m_jit.push(gpr);
            m_pushCountInEnter += 1;
        };

        auto pushPairInEnter = [&](GPRReg gpr1, GPRReg gpr2) {
            m_jit.pushPair(gpr1, gpr2);
            m_pushCountInEnter += 2;
        };

#if CPU(X86_64)
        UNUSED_VARIABLE(pushPairInEnter);
        m_jit.emitFunctionPrologue();

        if (m_pattern.m_saveInitialStartValue)
            pushInEnter(X86Registers::ebx);

#if OS(WINDOWS)
        pushInEnter(X86Registers::edi);
#endif
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        if (m_containsNestedSubpatterns) {
#if OS(WINDOWS)
            pushInEnter(X86Registers::esi);
#endif
            pushInEnter(X86Registers::r12);
        }
#endif

        if (m_decodeSurrogatePairs) {
            pushInEnter(X86Registers::r13);
            pushInEnter(X86Registers::r14);
            pushInEnter(X86Registers::r15);
        }
#if OS(WINDOWS)
        if (m_compileMode == JITCompileMode::IncludeSubpatterns)
            m_jit.loadPtr(MacroAssembler::Address(MacroAssembler::framePointerRegister, 6 * sizeof(void*)), m_regs.output);
        // rcx is the pointer to the allocated space for result in x64 Windows.
        pushInEnter(X86Registers::ecx);
#endif
#elif CPU(ARM64)
        UNUSED_VARIABLE(pushInEnter);
        if (!Options::useJITCage())
            m_jit.tagReturnAddress();
        if (m_decodeSurrogatePairs) {
            if (!Options::useJITCage())
                pushPairInEnter(MacroAssembler::framePointerRegister, MacroAssembler::linkRegister);
            m_jit.move(MacroAssembler::TrustedImm32(0x10000), m_regs.supplementaryPlanesBase);
            m_jit.move(MacroAssembler::TrustedImm32(0xd800), m_regs.leadingSurrogateTag);
            m_jit.move(MacroAssembler::TrustedImm32(0xdc00), m_regs.trailingSurrogateTag);
        }
#elif CPU(ARM_THUMB2)
        UNUSED_VARIABLE(pushPairInEnter);
        pushInEnter(ARMRegisters::r4);
        pushInEnter(ARMRegisters::r5);
        pushInEnter(ARMRegisters::r6);
        pushInEnter(ARMRegisters::r8);
        pushInEnter(ARMRegisters::r10);
#elif CPU(RISCV64)
        UNUSED_VARIABLE(pushInEnter);
        if (m_decodeSurrogatePairs)
            pushPairInEnter(MacroAssembler::framePointerRegister, MacroAssembler::linkRegister);
#else
        UNUSED_VARIABLE(pushInEnter);
        UNUSED_VARIABLE(pushPairInEnter);
#endif
    }

    void generateReturn()
    {
#if ENABLE(YARR_JIT_REGEXP_TEST_INLINE)
        if (m_compileMode == JITCompileMode::InlineTest) {
            m_inlinedMatched.append(m_jit.jump());
            return;
        }
#endif

#if CPU(X86_64)
#if OS(WINDOWS)
        // Store the return value in the allocated space pointed by rcx.
        ASSERT(noOverlap(X86Registers::ecx, m_regs.returnRegister, m_regs.returnRegister2));
        m_jit.pop(X86Registers::ecx);
        m_jit.store64(m_regs.returnRegister, MacroAssembler::Address(X86Registers::ecx));
        m_jit.store64(m_regs.returnRegister2, MacroAssembler::Address(X86Registers::ecx, sizeof(void*)));
        m_jit.move(X86Registers::ecx, m_regs.returnRegister);
#endif
        if (m_decodeSurrogatePairs) {
            m_jit.pop(X86Registers::r15);
            m_jit.pop(X86Registers::r14);
            m_jit.pop(X86Registers::r13);
        }

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        if (m_containsNestedSubpatterns) {
            m_jit.pop(X86Registers::r12);
#if OS(WINDOWS)
            m_jit.pop(X86Registers::esi);
#endif
        }
#endif
#if OS(WINDOWS)
        m_jit.pop(X86Registers::edi);
#endif

        if (m_pattern.m_saveInitialStartValue)
            m_jit.pop(X86Registers::ebx);
        m_jit.emitFunctionEpilogue();
#elif CPU(ARM64)
        if (m_decodeSurrogatePairs) {
            if (!Options::useJITCage())
                m_jit.popPair(MacroAssembler::framePointerRegister, MacroAssembler::linkRegister);
        }
#elif CPU(ARM_THUMB2)
        m_jit.pop(ARMRegisters::r10);
        m_jit.pop(ARMRegisters::r8);
        m_jit.pop(ARMRegisters::r6);
        m_jit.pop(ARMRegisters::r5);
        m_jit.pop(ARMRegisters::r4);
#elif CPU(RISCV64)
        if (m_decodeSurrogatePairs)
            m_jit.popPair(MacroAssembler::framePointerRegister, MacroAssembler::linkRegister);
#endif

#if CPU(ARM64E)
        if (Options::useJITCage())
            m_jit.farJump(MacroAssembler::TrustedImmPtr(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(&vmEntryToYarrJITAfter)), OperationPtrTag);
        else
            m_jit.ret();
#else
        m_jit.ret();
#endif
    }

    void loadSubPattern(MacroAssembler::RegisterID outputGPR, unsigned subpatternId, MacroAssembler::RegisterID startIndexGPR, MacroAssembler::RegisterID endIndexOrLenGPR)
    {
        m_jit.loadPair32(outputGPR, MacroAssembler::TrustedImm32((subpatternId << 1) * sizeof(int)), startIndexGPR, endIndexOrLenGPR);
    }

public:
    YarrGenerator(CCallHelpers& jit, const VM* vm, YarrCodeBlock* codeBlock, const YarrJITRegs& regs, YarrPattern& pattern, StringView patternString, CharSize charSize, JITCompileMode compileMode)
        : m_jit(jit)
        , m_vm(vm)
        , m_codeBlock(codeBlock)
        , m_boyerMooreData(static_cast<YarrBoyerMooreData*>(codeBlock))
        , m_regs(regs)
        , m_pattern(pattern)
        , m_patternString(patternString)
        , m_charSize(charSize)
        , m_compileMode(compileMode)
        , m_decodeSurrogatePairs(m_charSize == CharSize::Char16 && m_pattern.unicode())
        , m_unicodeIgnoreCase(m_pattern.unicode() && m_pattern.ignoreCase())
        , m_canonicalMode(m_pattern.unicode() ? CanonicalMode::Unicode : CanonicalMode::UCS2)
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        , m_parenContextSizes(compileMode == JITCompileMode::IncludeSubpatterns ? m_pattern.m_numSubpatterns : 0, m_pattern.m_body->m_callFrameSize)
#endif
    {
    }

    YarrGenerator(CCallHelpers& jit, const VM* vm, YarrBoyerMooreData* yarrBMData, const YarrJITRegs& regs, YarrPattern& pattern, StringView patternString, CharSize charSize, JITCompileMode compileMode)
        : m_jit(jit)
        , m_vm(vm)
        , m_codeBlock(nullptr)
        , m_boyerMooreData(yarrBMData)
        , m_regs(regs)
        , m_pattern(pattern)
        , m_patternString(patternString)
        , m_charSize(charSize)
        , m_compileMode(compileMode)
        , m_decodeSurrogatePairs(m_charSize == CharSize::Char16 && m_pattern.unicode())
        , m_unicodeIgnoreCase(m_pattern.unicode() && m_pattern.ignoreCase())
        , m_canonicalMode(m_pattern.unicode() ? CanonicalMode::Unicode : CanonicalMode::UCS2)
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        , m_parenContextSizes(compileMode == JITCompileMode::IncludeSubpatterns ? m_pattern.m_numSubpatterns : 0, m_pattern.m_body->m_callFrameSize)
#endif
    {
        if (m_pattern.m_containsBackreferences)
            m_usesT2 = true;
    }

    bool isSafeToRecurse() const
    {
        if (m_compilationThreadStackChecker)
            return m_compilationThreadStackChecker->isSafeToRecurse();

        return m_vm->isSafeToRecurse();
    }

    void setStackChecker(StackCheck* stackChecker)
    {
        m_compilationThreadStackChecker = stackChecker;
    }

    template<typename OperationType>
    static constexpr void functionChecks()
    {
        static_assert(FunctionTraits<OperationType>::cCallArity() == 5, "YarrJITCode takes 5 arguments");
        static_assert(std::is_same<MatchingContextHolder*, typename FunctionTraits<OperationType>::template ArgumentType<4>>::value, "MatchingContextHolder* is expected as the function 5th argument");
    }

    void compile(YarrCodeBlock& codeBlock)
    {
        MacroAssembler::Label startOfMainCode;

#if !ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (m_decodeSurrogatePairs) {
            codeBlock.setFallBackWithFailureReason(JITFailureReason::DecodeSurrogatePair);
            return;
        }
#endif

        if (m_pattern.m_containsBackreferences
#if ENABLE(YARR_JIT_BACKREFERENCES)
            && (m_compileMode == JITCompileMode::MatchOnly || (m_pattern.ignoreCase() && m_charSize != CharSize::Char8))
#endif
            ) {
                codeBlock.setFallBackWithFailureReason(JITFailureReason::BackReference);
                return;
        }

        if (m_pattern.m_containsLookbehinds) {
            codeBlock.setFallBackWithFailureReason(JITFailureReason::Lookbehind);
            return;
        }

        // We need to compile before generating code since we set flags based on compilation that
        // are used during generation.
        opCompileBody(m_pattern.m_body);
        
        if (m_failureReason) {
            codeBlock.setFallBackWithFailureReason(*m_failureReason);
            return;
        }

        if (UNLIKELY(Options::dumpDisassembly() || Options::dumpRegExpDisassembly()))
            m_disassembler = makeUnique<YarrDisassembler>(this);

        if (m_disassembler)
            m_disassembler->setStartOfCode(m_jit.label());

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        if (m_containsNestedSubpatterns)
            codeBlock.setUsesPatternContextBuffer();
#endif

        generateEnter();

        startOfMainCode = m_jit.label();

        MacroAssembler::Jump hasInput = checkInput();
        generateFailReturn();
        hasInput.link(&m_jit);

        unsigned callFrameSizeInBytes = alignCallFrameSizeInBytes(m_pattern.m_body->m_callFrameSize);
        if (callFrameSizeInBytes) {
            // Check stack size
            m_jit.addPtr(MacroAssembler::TrustedImm32(-callFrameSizeInBytes), MacroAssembler::stackPointerRegister, m_regs.regT0);

            // Make sure that the JITed functions have 5 parameters and that the 5th argument is a MatchingContextHolder*
            functionChecks<YarrCodeBlock::YarrJITCode8>();
            functionChecks<YarrCodeBlock::YarrJITCode16>();
            functionChecks<YarrCodeBlock::YarrJITCodeMatchOnly8>();
            functionChecks<YarrCodeBlock::YarrJITCodeMatchOnly16>();
#if CPU(X86_64) && OS(WINDOWS)
            // matchingContext is the 5th argument, it is found on the stack.
            MacroAssembler::RegisterID matchingContext = m_regs.regT1;
            m_jit.loadPtr(MacroAssembler::Address(MacroAssembler::framePointerRegister, 7 * sizeof(void*)), matchingContext);
#elif CPU(ARM_THUMB2) || CPU(MIPS)
            // Not enough argument registers: try to load the 5th argument from the stack
            MacroAssembler::RegisterID matchingContext = m_regs.regT1;

            // The argument will be in an offset that depends on the arch and the number of registers we pushed into the stack
            // POKE_ARGUMENT_OFFSET: MIPS reserves space in the stack for all arguments, so we add +4 offset
            // m_pushCountInEnter: number of registers pushed into the stack (see generateEnter())
            unsigned offset = POKE_ARGUMENT_OFFSET + m_pushCountInEnter;
            m_jit.loadPtr(MacroAssembler::Address(MacroAssembler::stackPointerRegister, offset * sizeof(void*)), matchingContext);
#else
            MacroAssembler::RegisterID matchingContext = m_regs.matchingContext;
#endif
            MacroAssembler::Jump stackOk = m_jit.branchPtr(MacroAssembler::BelowOrEqual, MacroAssembler::Address(matchingContext, MatchingContextHolder::offsetOfStackLimit()), m_regs.regT0);

            // Exceeded stack limit, punt to the interpreter.
            m_jit.move(MacroAssembler::TrustedImmPtr((void*)static_cast<size_t>(JSRegExpResult::JITCodeFailure)), m_regs.returnRegister);
            m_jit.move(MacroAssembler::TrustedImm32(0), m_regs.returnRegister2);
            generateReturn();

            stackOk.link(&m_jit);
            m_jit.move(m_regs.regT0, MacroAssembler::stackPointerRegister);
        }

#if ENABLE(YARR_JIT_UNICODE_EXPRESSIONS)
        if (m_decodeSurrogatePairs)
            m_jit.getEffectiveAddress(MacroAssembler::BaseIndex(m_regs.input, m_regs.length, MacroAssembler::TimesTwo), m_regs.endOfStringAddress);
#endif

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        if (m_containsNestedSubpatterns)
            m_jit.move(MacroAssembler::TrustedImm32(matchLimit), m_regs.remainingMatchCount);
#endif

        // Initialize subpatterns' starts. And initialize matchStart if `!m_pattern.m_body->m_hasFixedSize`.
        // If the mode is JITCompileMode::IncludeSubpatterns, then matchStart is subpatterns[0]'s start.
        if (m_compileMode == JITCompileMode::IncludeSubpatterns) {
            unsigned subpatternId = 0;
            // First subpatternId's start is configured to `index` if !m_pattern.m_body->m_hasFixedSize.
            if (!m_pattern.m_body->m_hasFixedSize) {
                setMatchStart(m_regs.index);
                ++subpatternId;
            }
            for (; subpatternId < m_pattern.m_numSubpatterns + 1; ++subpatternId)
                m_jit.store32(MacroAssembler::TrustedImm32(-1), MacroAssembler::Address(m_regs.output, (subpatternId << 1) * sizeof(int)));
        } else {
            if (!m_pattern.m_body->m_hasFixedSize)
                setMatchStart(m_regs.index);
        }

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        if (m_containsNestedSubpatterns) {
            initParenContextFreeList();
            if (m_failureReason) {
                codeBlock.setFallBackWithFailureReason(*m_failureReason);
                return;
            }
        }
#endif
        
        if (m_pattern.m_saveInitialStartValue)
            m_jit.move(m_regs.index, m_regs.initialStart);

        generate();
        if (m_disassembler)
            m_disassembler->setEndOfGenerate(m_jit.label());
        backtrack();
        if (m_disassembler)
            m_disassembler->setEndOfBacktrack(m_jit.label());

        ptrdiff_t codeSize = MacroAssembler::differenceBetween(startOfMainCode, m_jit.label());
        bool canInline = m_compileMode != JITCompileMode::IncludeSubpatterns
            && !m_pattern.global() && !m_pattern.sticky() && !m_pattern.unicode()
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
            && !m_containsNestedSubpatterns
#endif
            && !m_pattern.m_containsBackreferences
            && !m_pattern.m_saveInitialStartValue;

        generateTryReadUnicodeCharacterHelper();

        generateJITFailReturn();

        if (m_disassembler)
            m_disassembler->setEndOfCode(m_jit.label());

        auto backtrackRecords = m_backtrackingState.backtrackRecords();
        if (!backtrackRecords.isEmpty()) {
            m_jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                BacktrackingState::linkBacktrackRecords(linkBuffer, backtrackRecords);
            });
        }

        if (!m_tryReadUnicodeCharacterCalls.isEmpty()) {
            m_jit.addLinkTask([=, this] (LinkBuffer& linkBuffer) {
                CodeLocationLabel<NoPtrTag> tryReadUnicodeCharacterHelper = linkBuffer.locationOf<NoPtrTag>(m_tryReadUnicodeCharacterEntry);

                for (auto call : m_tryReadUnicodeCharacterCalls)
                    linkBuffer.link(call, tryReadUnicodeCharacterHelper);
            });
        }

        LinkBuffer linkBuffer(m_jit, REGEXP_CODE_ID, LinkBuffer::Profile::YarrJIT, JITCompilationCanFail);
        if (linkBuffer.didFailToAllocate()) {
            codeBlock.setFallBackWithFailureReason(JITFailureReason::ExecutableMemoryAllocationFailure);
            return;
        }

        if (m_disassembler)
            m_disassembler->dump(linkBuffer);

        if (m_compileMode == JITCompileMode::MatchOnly) {
            if (m_charSize == CharSize::Char8) {
                codeBlock.set8BitCodeMatchOnly(FINALIZE_REGEXP_CODE(linkBuffer, YarrMatchOnly8BitPtrTag, "Match-only 8-bit regular expression"), WTFMove(m_bmMaps));
                codeBlock.set8BitInlineStats(codeSize, callFrameSizeInBytes, canInline, m_usesT2);
            } else {
                codeBlock.set16BitCodeMatchOnly(FINALIZE_REGEXP_CODE(linkBuffer, YarrMatchOnly16BitPtrTag, "Match-only 16-bit regular expression"), WTFMove(m_bmMaps));
                codeBlock.set16BitInlineStats(codeSize, callFrameSizeInBytes, canInline, m_usesT2);
            }
        } else {
            if (m_charSize == CharSize::Char8)
                codeBlock.set8BitCode(FINALIZE_REGEXP_CODE(linkBuffer, Yarr8BitPtrTag, "8-bit regular expression"), WTFMove(m_bmMaps));
            else
                codeBlock.set16BitCode(FINALIZE_REGEXP_CODE(linkBuffer, Yarr16BitPtrTag, "16-bit regular expression"), WTFMove(m_bmMaps));
        }
        if (m_failureReason)
            codeBlock.setFallBackWithFailureReason(*m_failureReason);
    }

#if ENABLE(YARR_JIT_REGEXP_TEST_INLINE)
    void compileInline(YarrBoyerMooreData& boyerMooreData)
    {
        RELEASE_ASSERT(!m_pattern.m_containsBackreferences);

        // We need to compile before generating code since we set flags based on compilation that
        // are used during generation.
        opCompileBody(m_pattern.m_body);

#ifndef JIT_UNICODE_EXPRESSIONS
        RELEASE_ASSERT(!m_decodeSurrogatePairs);
#endif

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        RELEASE_ASSERT(!m_containsNestedSubpatterns);
#endif

        ASSERT(!m_failureReason);

        if (m_usesT2)
            ASSERT(m_regs.regT2 != MacroAssembler::InvalidGPRReg);

        if (UNLIKELY(Options::dumpDisassembly() || Options::dumpRegExpDisassembly()))
            m_disassembler = makeUnique<YarrDisassembler>(this);

        if (m_disassembler)
            m_disassembler->setStartOfCode(m_jit.label());

        MacroAssembler::Jump hasInput = checkInput();
        generateFailReturn();
        hasInput.link(&m_jit);

        unsigned callFrameSizeInBytes = alignCallFrameSizeInBytes(m_pattern.m_body->m_callFrameSize);
        if (callFrameSizeInBytes) {
            // Create space on stack for matching context data.
            m_jit.addPtr(MacroAssembler::TrustedImm32(-callFrameSizeInBytes), MacroAssembler::stackPointerRegister, MacroAssembler::stackPointerRegister);
        }

#ifdef JIT_UNICODE_EXPRESSIONS
        if (m_decodeSurrogatePairs)
            m_jit.getEffectiveAddress(MacroAssembler::BaseIndex(m_regs.input, m_regs.length, MacroAssembler::TimesTwo), m_regs.endOfStringAddress);
#endif

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        if (m_containsNestedSubpatterns)
            m_jit.move(MacroAssembler::TrustedImm32(matchLimit), m_regs.remainingMatchCount);
#endif

        if (m_compileMode == JITCompileMode::IncludeSubpatterns) {
            for (unsigned i = 0; i < m_pattern.m_numSubpatterns + 1; ++i)
                m_jit.store32(MacroAssembler::TrustedImm32(-1), MacroAssembler::Address(m_regs.output, (i << 1) * sizeof(int)));
        }

        if (!m_pattern.m_body->m_hasFixedSize)
            setMatchStart(m_regs.index);

        if (m_pattern.m_saveInitialStartValue)
            m_jit.move(m_regs.index, m_regs.initialStart);

        generate();
        if (m_disassembler)
            m_disassembler->setEndOfGenerate(m_jit.label());
        backtrack();
        if (m_disassembler)
            m_disassembler->setEndOfBacktrack(m_jit.label());

        generateTryReadUnicodeCharacterHelper();

        generateJITFailReturn();

        if (m_disassembler)
            m_disassembler->setEndOfCode(m_jit.label());

        m_inlinedFailedMatch.link(&m_jit);
        m_inlinedMatched.link(&m_jit);

        auto backtrackRecords = m_backtrackingState.backtrackRecords();
        if (!backtrackRecords.isEmpty()) {
            m_jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                BacktrackingState::linkBacktrackRecords(linkBuffer, backtrackRecords);
            });
        }

        if (!m_tryReadUnicodeCharacterCalls.isEmpty()) {
            m_jit.addLinkTask([=, this] (LinkBuffer& linkBuffer) {
                CodeLocationLabel<NoPtrTag> tryReadUnicodeCharacterHelper = linkBuffer.locationOf<NoPtrTag>(m_tryReadUnicodeCharacterEntry);

                for (auto call : m_tryReadUnicodeCharacterCalls)
                    linkBuffer.link(call, tryReadUnicodeCharacterHelper);
            });
        }

        boyerMooreData.saveMaps(WTFMove(m_bmMaps));
    }
#endif

    const char* variant() final
    {
        if (m_compileMode == JITCompileMode::MatchOnly) {
            if (m_charSize == CharSize::Char8)
                return "Match-only 8-bit regular expression";

            return "Match-only 16-bit regular expression";
        }

        if (m_charSize == CharSize::Char8)
            return "8-bit regular expression";

        return "16-bit regular expression";
    }

    unsigned opCount() final
    {
        return m_ops.size();
    }

    void dumpPatternString(PrintStream& out) final
    {
        m_pattern.dumpPatternString(out, m_patternString);
    }

    int dumpFor(PrintStream& out, unsigned opIndex) final
    {
        if (opIndex >= opCount())
            return 0;

        out.printf("%4d:", opIndex);

        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;
        switch (op.m_op) {
        case YarrOpCode::Term: {
            out.print("Term ");
            switch (term->type) {
            case PatternTerm::Type::AssertionBOL:
                out.printf("Assert BOL checked-offset:(%u)", op.m_checkedOffset.value());
                break;

            case PatternTerm::Type::AssertionEOL:
                out.printf("Assert EOL checked-offset:(%u)", op.m_checkedOffset.value());
                break;

            case PatternTerm::Type::BackReference:
                out.printf("BackReference pattern #%u checked-offset:(%u)", term->backReferenceSubpatternId, op.m_checkedOffset.value());
                term->dumpQuantifier(out);
                break;

            case PatternTerm::Type::PatternCharacter:
                out.printf("PatternCharacter checked-offset:(%u) ", op.m_checkedOffset.value());
                dumpUChar32(out, term->patternCharacter);
                if (m_pattern.ignoreCase())
                    out.print("ignore case ");

                term->dumpQuantifier(out);
                break;

            case PatternTerm::Type::CharacterClass:
                out.printf("PatternCharacterClass checked-offset:(%u) ", op.m_checkedOffset.value());
                if (term->invert())
                    out.print("not ");
                dumpCharacterClass(out, &m_pattern, term->characterClass);
                term->dumpQuantifier(out);
                break;

            case PatternTerm::Type::AssertionWordBoundary:
                out.printf("%sword boundary checked-offset:(%u)", term->invert() ? "non-" : "", op.m_checkedOffset.value());
                break;

            case PatternTerm::Type::DotStarEnclosure:
                out.printf(".* enclosure checked-offset:(%u)", op.m_checkedOffset.value());
                break;

            case PatternTerm::Type::ForwardReference:
                out.printf("ForwardReference <not handled> checked-offset:(%u)", op.m_checkedOffset.value());
                break;

            case PatternTerm::Type::ParenthesesSubpattern:
            case PatternTerm::Type::ParentheticalAssertion:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            if (op.m_isDeadCode)
                out.print(" already handled");
            out.print("\n");
            return 0;
        }

        case YarrOpCode::BodyAlternativeBegin:
            out.printf("BodyAlternativeBegin minimum-size:(%u),checked-offset:(%u)\n", op.m_alternative->m_minimumSize, op.m_checkedOffset.value());
            return 0;

        case YarrOpCode::BodyAlternativeNext:
            out.printf("BodyAlternativeNext minimum-size:(%u),checked-offset:(%u)\n", op.m_alternative->m_minimumSize, op.m_checkedOffset.value());
            return 0;

        case YarrOpCode::BodyAlternativeEnd:
            out.printf("BodyAlternativeEnd checked-offset:(%u)\n", op.m_checkedOffset.value());
            return 0;

        case YarrOpCode::SimpleNestedAlternativeBegin:
            out.printf("SimpleNestedAlternativeBegin minimum-size:(%u),checked-offset:(%u)\n", op.m_alternative->m_minimumSize, op.m_checkedOffset.value());
            return 1;

        case YarrOpCode::NestedAlternativeBegin:
            out.printf("NestedAlternativeBegin minimum-size:(%u),checked-offset:(%u)\n", op.m_alternative->m_minimumSize, op.m_checkedOffset.value());
            return 1;

        case YarrOpCode::SimpleNestedAlternativeNext:
            out.printf("SimpleNestedAlternativeNext minimum-size:(%u),checked-offset:(%u)\n", op.m_alternative->m_minimumSize, op.m_checkedOffset.value());
            return 0;

        case YarrOpCode::NestedAlternativeNext:
            out.printf("NestedAlternativeNext minimum-size:(%u),checked-offset:(%u)\n", op.m_alternative->m_minimumSize, op.m_checkedOffset.value());
            return 0;

        case YarrOpCode::SimpleNestedAlternativeEnd:
            out.printf("SimpleNestedAlternativeEnd checked-offset:(%u) ", op.m_checkedOffset.value());
            term->dumpQuantifier(out);
            out.print("\n");
            return -1;

        case YarrOpCode::NestedAlternativeEnd:
            out.printf("NestedAlternativeEnd checked-offset:(%u) ", op.m_checkedOffset.value());
            term->dumpQuantifier(out);
            out.print("\n");
            return -1;

        case YarrOpCode::ParenthesesSubpatternOnceBegin:
            out.printf("ParenthesesSubpatternOnceBegin checked-offset:(%u) ", op.m_checkedOffset.value());
            if (term->capture())
                out.printf("capturing pattern #%u ", term->parentheses.subpatternId);
            else
                out.print("non-capturing ");
            term->dumpQuantifier(out);
            out.print("\n");
            return 0;

        case YarrOpCode::ParenthesesSubpatternOnceEnd:
            out.printf("ParenthesesSubpatternOnceEnd checked-offset:(%u) ", op.m_checkedOffset.value());
            if (term->capture())
                out.printf("capturing pattern #%u ", term->parentheses.subpatternId);
            else
                out.print("non-capturing ");
            term->dumpQuantifier(out);
            out.print("\n");
            return 0;

        case YarrOpCode::ParenthesesSubpatternTerminalBegin:
            out.printf("ParenthesesSubpatternTerminalBegin checked-offset:(%u) ", op.m_checkedOffset.value());
            if (term->capture())
                out.printf("capturing pattern #%u\n", term->parentheses.subpatternId);
            else
                out.print("non-capturing\n");
            return 0;

        case YarrOpCode::ParenthesesSubpatternTerminalEnd:
            out.printf("ParenthesesSubpatternTerminalEnd checked-offset:(%u) ", op.m_checkedOffset.value());
            if (term->capture())
                out.printf("capturing pattern #%u\n", term->parentheses.subpatternId);
            else
                out.print("non-capturing\n");
            return 0;

        case YarrOpCode::ParenthesesSubpatternBegin:
            out.printf("ParenthesesSubpatternBegin checked-offset:(%u) ", op.m_checkedOffset.value());
            if (term->capture())
                out.printf("capturing pattern #%u", term->parentheses.subpatternId);
            else
                out.print("non-capturing");
            term->dumpQuantifier(out);
            out.print("\n");
            return 0;

        case YarrOpCode::ParenthesesSubpatternEnd:
            out.printf("ParenthesesSubpatternEnd checked-offset:(%u) ", op.m_checkedOffset.value());
            if (term->capture())
                out.printf("capturing pattern #%u", term->parentheses.subpatternId);
            else
                out.print("non-capturing");
            term->dumpQuantifier(out);
            out.print("\n");
            return 0;

        case YarrOpCode::ParentheticalAssertionBegin:
            out.printf("ParentheticalAssertionBegin%s checked-offset:(%u)\n", term->invert() ? " inverted" : "", op.m_checkedOffset.value());
            return 0;

        case YarrOpCode::ParentheticalAssertionEnd:
            out.printf("ParentheticalAssertionEnd%s checked-offset:(%u)\n", term->invert() ? " inverted" : "", op.m_checkedOffset.value());
            return 0;

        case YarrOpCode::MatchFailed:
            out.printf("MatchFailed checked-offset:(%u)\n", op.m_checkedOffset.value());
            return 0;
        }

        return 0;
    }

private:
    CCallHelpers& m_jit;
    const VM* const m_vm;
    YarrCodeBlock* const m_codeBlock;
    YarrBoyerMooreData* const m_boyerMooreData;
    const YarrJITRegs& m_regs;

    StackCheck* m_compilationThreadStackChecker { nullptr };
    YarrPattern& m_pattern;
    const StringView m_patternString;

    const CharSize m_charSize;
    const JITCompileMode m_compileMode;

    // Used to detect regular expression constructs that are not currently
    // supported in the JIT; fall back to the interpreter when this is detected.
    std::optional<JITFailureReason> m_failureReason;

    const bool m_decodeSurrogatePairs;
    const bool m_unicodeIgnoreCase;
    bool m_usesT2 { false };
    const CanonicalMode m_canonicalMode;
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    bool m_containsNestedSubpatterns { false };
    ParenContextSizes m_parenContextSizes;
#endif
    MacroAssembler::JumpList m_abortExecution;
    MacroAssembler::JumpList m_hitMatchLimit;
    Vector<MacroAssembler::Call> m_tryReadUnicodeCharacterCalls;
    MacroAssembler::Label m_tryReadUnicodeCharacterEntry;

    MacroAssembler::JumpList m_inlinedMatched;
    MacroAssembler::JumpList m_inlinedFailedMatch;

    // The regular expression expressed as a linear sequence of operations.
    Vector<YarrOp, 128> m_ops;
    Vector<UniqueRef<BoyerMooreInfo>, 4> m_bmInfos;
    Vector<UniqueRef<BoyerMooreBitmap::Map>> m_bmMaps;

    // This class records state whilst generating the backtracking path of code.
    BacktrackingState m_backtrackingState;
    
    std::unique_ptr<YarrDisassembler> m_disassembler;

    // Member is used to count the number of GPR pushed into the stack when
    // entering JITed code. It is used to figure out if an function argument
    // offset in the stack if there wasn't enough registers to pass it, e.g.,
    // ARMv7 and MIPS only use 4 registers to pass function arguments.
    unsigned m_pushCountInEnter { 0 };
};

static void dumpCompileFailure(JITFailureReason failure)
{
    switch (failure) {
    case JITFailureReason::DecodeSurrogatePair:
        dataLog("Can't JIT a pattern decoding surrogate pairs\n");
        break;
    case JITFailureReason::BackReference:
        dataLog("Can't JIT some patterns containing back references\n");
        break;
    case JITFailureReason::ForwardReference:
        dataLog("Can't JIT a pattern containing forward references\n");
        break;
    case JITFailureReason::Lookbehind:
        dataLog("Can't JIT a pattern containing lookbehinds\n");
        break;
    case JITFailureReason::VariableCountedParenthesisWithNonZeroMinimum:
        dataLog("Can't JIT a pattern containing a variable counted parenthesis with a non-zero minimum\n");
        break;
    case JITFailureReason::ParenthesizedSubpattern:
        dataLog("Can't JIT a pattern containing parenthesized subpatterns\n");
        break;
    case JITFailureReason::FixedCountParenthesizedSubpattern:
        dataLog("Can't JIT a pattern containing fixed count parenthesized subpatterns\n");
        break;
    case JITFailureReason::ParenthesisNestedTooDeep:
        dataLog("Can't JIT pattern due to parentheses nested too deeply\n");
        break;
    case JITFailureReason::ExecutableMemoryAllocationFailure:
        dataLog("Can't JIT because of failure of allocation of executable memory\n");
        break;
    case JITFailureReason::OffsetTooLarge:
        dataLog("Can't JIT because pattern exceeds string length limits\n");
        break;
    }
}

void jitCompile(YarrPattern& pattern, StringView patternString, CharSize charSize, VM* vm, YarrCodeBlock& codeBlock, JITCompileMode mode)
{
    CCallHelpers masm;

    ASSERT(mode == JITCompileMode::MatchOnly || mode == JITCompileMode::IncludeSubpatterns);

    YarrJITDefaultRegisters jitRegisters;
    YarrGenerator<YarrJITDefaultRegisters>(masm, vm, &codeBlock, jitRegisters, pattern, patternString, charSize, mode).compile(codeBlock);

    if (auto failureReason = codeBlock.failureReason()) {
        if (UNLIKELY(Options::dumpCompiledRegExpPatterns())) {
            pattern.dumpPatternString(WTF::dataFile(), patternString);
            dataLog(" : ");
            dumpCompileFailure(*failureReason);
        }
    }
}

#if ENABLE(YARR_JIT_REGEXP_TEST_INLINE)
#if !(CPU(ARM64) || (CPU(X86_64) && !OS(WINDOWS)) || CPU(RISCV64))
#error "No support for inlined JIT'ing of RegExp.test for this CPU / OS combination."
#endif

void jitCompileInlinedTest(StackCheck* m_compilationThreadStackChecker, StringView patternString, OptionSet<Yarr::Flags> flags, CharSize charSize, const VM* vm, YarrBoyerMooreData& boyerMooreData, CCallHelpers& jit, YarrJITRegisters& jitRegisters)
{
    Yarr::ErrorCode errorCode;
    Yarr::YarrPattern pattern(patternString, flags, errorCode);

    jitRegisters.validate();

    YarrGenerator<YarrJITRegisters> yarrGenerator(jit, vm, &boyerMooreData, jitRegisters, pattern, patternString, charSize, JITCompileMode::InlineTest);
    yarrGenerator.setStackChecker(m_compilationThreadStackChecker);
    yarrGenerator.compileInline(boyerMooreData);
}
#endif

}}

#endif
